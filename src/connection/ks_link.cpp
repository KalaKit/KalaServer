//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "connection/ks_link.hpp"

#if defined(KWIN_ANY)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <arpa/inet.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

#include <thread>

#include "log_utils.hpp"

#include "core/ks_core.hpp"
#include "connection/ks_server.hpp"
#include "connection/ks_client.hpp"

using KalaServer::Core::KalaServerCore;
using KalaServer::Core::KalaServerRegistry;
using KalaServer::Connection::Link;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaServer::Connection::SenderType;
using KalaServer::Connection::TransportProtocol;
using KalaServer::Connection::PacketConnectionMode;
using KalaServer::Connection::ResponseType;
using KalaServer::Connection::Server;
using KalaServer::Connection::Client;

using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::function;
using std::mutex;
using std::atomic;
using std::thread;
using std::scoped_lock;
using std::unique_ptr;
using std::make_unique;

#if defined(KWIN_ANY)
using ksocket = SOCKET;
constexpr ksocket invalid_socket = INVALID_SOCKET;
#define kclose(targetSocket) closesocket(targetSocket)
#else
using ksocket = int;
constexpr ksocket invalid_socket = -1;
#define kclose(targetSocket) close(targetSocket)
#endif

struct ThreadData
{
    u32 linkID{};
    thread thisThread{};
};

static constexpr u32 MAX_TCP_PAYLOAD_SIZE = UINT16_MAX;

//max ipv4 packet 65535 -20 for ipv4 header, -8 for udp header
static constexpr u32 MAX_UDP_PAYLOAD_SIZE_IPV4 = 65507;
//max ipv6 packet 65535 -8 for udp header
static constexpr u32 MAX_UDP_PAYLOAD_SIZE_IPV6 = 65527;

static KalaServerRegistry<Link> registry{};

static atomic<bool> isShuttingDown{};

static mutex threadDataMutex{};
static vector<ThreadData> threadData{};

static atomic<u32> activeHeadlessConnections{0};

static void _SendPacket(
    const bool isSingleShot,
    const u32 linkID,
    const u32 senderID,
    const SenderType senderType,
    const string& receiverIP,
    const u16 receiverPort,
    const u8 timeout,
    const TransportProtocol transportProtocol,
    const PacketConnectionMode packetConnectionMode,
    const ResponseType responseType,
    const vector<u8>& payload,
    const function<void(vector<u8>)>& initialResponseCallback,
    const function<void(vector<u8>)>& listenerResponseCallback);

namespace KalaServer::Connection
{
    KalaServerRegistry<Link>& Link::GetRegistry() { return registry; }

    u32 Link::GetActiveHeadlessConnectionCount() { return activeHeadlessConnections.load(); }

    void Link::SendPacket(
        u32 senderID,
        SenderType senderType,
        string_view receiverIP,
        u16 receiverPort,
        u8 timeout,
        TransportProtocol transportProtocol,
        ResponseType responseType,
        const vector<u8>& payload,
        const function<void(vector<u8>)>& responseCallback)
    {
        _SendPacket(
            true,
            0,
            senderID,
            senderType,
            string(receiverIP),
            receiverPort,
            timeout,
            transportProtocol,
            PacketConnectionMode::PCM_DisconnectAfterSend,
            responseType,
            payload,
            responseCallback,
            {});
    }

    Link* Link::Initialize(
        u32 senderID, 
        SenderType senderType, 
        string_view receiverIP, 
        u16 receiverPort,
        TransportProtocol transportProtocol)
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to initialize link because KalaServer has not been initialized!",
                "KS_LINK",
                LogType::LOG_WARNING);

            return nullptr;
        }

		u32 newID = KalaServerCore::GetGlobalID() + 1;
		KalaServerCore::SetGlobalID(newID);

		unique_ptr<Link> newLink = make_unique<Link>();
		Link* linkPtr = newLink.get();

        linkPtr->ID = newID;
        linkPtr->senderID = senderID;
        linkPtr->senderType = senderType;
        linkPtr->receiverIP = receiverIP;
        linkPtr->receiverPort = receiverPort;
        linkPtr->transportProtocol = transportProtocol;

		string err = registry.AddContent(
			newID, 
			std::move(newLink));
		if (!err.empty())
		{
			KalaServerCore::ForceClose(
				"KalaServer Link error",
				"Failed to initialize link! Reason: " + err);
		}

		Log::Print(
			"Created new link '" + to_string(newID) + "' between '" + to_string(senderID) 
            + "' and '" + string(receiverIP) + "' via port '" + to_string(receiverPort) + "'!",
			"KS_LINK",
			LogType::LOG_SUCCESS);

		return linkPtr;
    }

    u32 Link::GetID() const { return ID; }

    u32 Link::GetSenderID() const { return senderID; }
    SenderType Link::GetSenderType() const { return senderType; }

    u32 Link::GetActiveConnectionCount() const { return activeConnections.load(); }

    const string& Link::GetReceiverIP() const { return receiverIP; }
    u16 Link::GetReceiverPort() const { return receiverPort; }

    u8 Link::GetTimeout() const { return timeout; }
    void Link::SetTimeout(u8 newValue) { timeout = newValue; }

    TransportProtocol Link::GetTransportProtocol() const { return transportProtocol; }

    void Link::SendPacket(
        const PacketConnectionMode packetConnectionMode, 
        const ResponseType responseType, 
        const vector<u8>& payload,
        const function<void(vector<u8>)>& initialResponseCallback,
        const function<void(vector<u8>)>& listenerResponseCallback)
    {
        _SendPacket(
            false,
            ID,
            senderID,
            senderType,
            receiverIP,
            receiverPort,
            timeout,
            transportProtocol,
            packetConnectionMode,
            responseType,
            payload,
            initialResponseCallback,
            listenerResponseCallback);
    }

    void Link::Update()
    {

    }

    void Link::CloseRemainingThreads()
    {
        isShuttingDown.store(true);

        vector<ThreadData> remainingThreads{};

        {
            scoped_lock lock(threadDataMutex);
            remainingThreads = std::move(threadData);
        }

        for (ThreadData& t : remainingThreads)
        {
            if (t.thisThread.joinable()) t.thisThread.join();
        }
    }

    void Link::Destroy()
    {
        vector<thread> targetThreads{};

        {
            scoped_lock lock(threadDataMutex);
            
            for (size_t i = threadData.size(); i > 0; --i)
            {
                if (threadData[i - 1].linkID != ID) continue;

                targetThreads.push_back(std::move(threadData[i - 1].thisThread));
                threadData.erase(threadData.begin() + (i - 1));
            }
        }

        for (thread& t : targetThreads)
        {
            if (t.joinable()) t.join();
        }

		string err = registry.DestroyContent(ID);
		if (!err.empty())
		{
			KalaServerCore::ForceClose(
				"KalaServer Link error",
				"Failed to destroy link! Reason: " + err);
		}
    }

    Link::~Link()
    {
		Log::Print(
			"Destroying link '" + to_string(ID) + "'.",
			"KS_LINK",
			LogType::LOG_INFO);
    }
}

void _SendPacket(
    const bool isSingleShot,
    const u32 linkID,
    const u32 senderID,
    const SenderType senderType,
    const string& receiverIP,
    const u16 receiverPort,
    const u8 timeout,
    const TransportProtocol transportProtocol,
    const PacketConnectionMode packetConnectionMode,
    const ResponseType responseType,
    const vector<u8>& payload,
    const function<void(vector<u8>)>& initialResponseCallback,
    const function<void(vector<u8>)>& listenerResponseCallback)
{
    //
    // SETUP
    //

    string receiverStr = 
        "reciever '" + string(receiverIP) 
        + "' at port '" + to_string(receiverPort) + "'";

    Link* link{};

    if (linkID != 0)
    {
        string err = Link::GetRegistry().GetContent(linkID, link);
        if (!err.empty())
        {
            Log::Print(
                "Failed to send packet to " + receiverStr +
                " because the link was invalid! Reason: " + err,
                "KS_LINK",
                LogType::LOG_WARNING);

            return;
        }
    }

    Server* server{};

    if (senderType == SenderType::S_SERVER)
    {
        string err = Server::GetRegistry().GetContent(senderID, server);
        if (!err.empty())
        {
            Log::Print(
                "Failed to send packet via server '" + to_string(senderID) + "' to " + receiverStr +
                " because the sender ID for server type was invalid! Reason: " + err,
                "KS_LINK",
                LogType::LOG_WARNING);

            return;
        }
    }
    else
    {
        if (!Client::IsInitialized())
        {
            Log::Print(
                "Failed to send packet via client to " + receiverStr +
                " because the client has not been initialized",
                "KS_LINK",
                LogType::LOG_WARNING);

            return;
        }
    }

    string senderStr = server 
        ? "server '" + server->GetServerName() + " (" + to_string(server->GetID()) + ")'"
        : "client '" + Client::GetUsername() + "'";

    if (isSingleShot
        && payload.empty())
    {
        Log::Print(
            "Failed to send packet via " + senderStr + " to " + receiverStr 
            + " because the payload was empty!",
            "KS_LINK",
            LogType::LOG_WARNING);

        return;
    }

    const bool isIPv4 = KalaServerCore::IsIPv4(receiverIP);

    auto worker_thread = [
        isIPv4,
        linkID,
        senderID,
        senderStr,
        receiverStr,
        timeout,
        receiverIP,
        receiverPort,
        transportProtocol,
        packetConnectionMode,
        responseType,
        payload,
        initialResponseCallback,
        listenerResponseCallback]() -> void
        {
            auto get_server = [senderID]() -> Server*
                {
                    Server* s{};
                    string _ = Server::GetRegistry().GetContent(senderID, s);
                    return s;
                };

            auto get_link = [linkID]() -> Link*
                {
                    Link* l{};
                    string _ = Link::GetRegistry().GetContent(linkID, l);
                    return l;
                };

            Server* server = get_server();
            Link* link = get_link();

            //for verifying if server is still valid during the lifetime of this thread
            const bool hadServer = server;
            //for verifying if the link is still valid during the lifetime of this thread
            const bool hadLink = link;
            //for verifying if the client is still valid during the lifetime of this thread
            const bool hadClient = Client::IsInitialized();

            //
            // PREPARE TARGET SOCKET
            //

            auto create_socket = [
                receiverIP, 
                transportProtocol]() -> ksocket
                {
                    int domain = KalaServerCore::IsIPv4(receiverIP)
                        ? AF_INET
                        : AF_INET6;

                    int type = transportProtocol == TransportProtocol::TP_TCP
                        ? SOCK_STREAM
                        : SOCK_DGRAM;

                    int protocol = transportProtocol == TransportProtocol::TP_TCP
                        ? IPPROTO_TCP
                        : IPPROTO_UDP;
                    
                    return socket(domain, type, protocol);
                };

            ksocket targetSocket = create_socket();
            if (targetSocket == invalid_socket)
            {
#if defined(KWIN_ANY)
                string err = KalaServerCore::ErrorToString(WSAGetLastError());
#else
                string err = KalaServerCore::ErrorToString(errno);
#endif

                //always force close
                KalaServerCore::ForceClose(
                    "KalaServer Link error",
                    "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                    + " because socket creation failed! Reason: " + err);
            }

#if defined (KWIN_ANY)
            u_long mode = 1;
            if (ioctlsocket(targetSocket, FIONBIO, &mode) == SOCKET_ERROR)
            {
                string err = KalaServerCore::ErrorToString(WSAGetLastError());

                //always force close
                KalaServerCore::ForceClose(
                    "KalaServer Link error",
                    "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                    + " because nonblocking socket mode couldn't be enabled! Reason: " + err);
            }
#else
            int flags = fcntl(targetSocket, F_GETFL, 0);

            if (flags == -1
                || fcntl(targetSocket, F_SETFL, flags | O_NONBLOCK) == -1)
            {
                string err = KalaServerCore::ErrorToString(errno);

                //always force close
                KalaServerCore::ForceClose(
                    "KalaServer Link error",
                    "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                    + " because nonblocking socket mode couldn't be enabled! Reason: " + err);
            }
#endif

            //
            // CONNECT TO RECEIVER
            //

            sockaddr_storage addr{};
#if defined(KWIN_ANY)
            int addrLen{};
#else
            socklen_t addrLen{};
#endif

            if (isIPv4)
            {
                sockaddr_in ipv4{};
                ipv4.sin_family = AF_INET;
                ipv4.sin_port = htons(receiverPort);
                if (inet_pton(AF_INET, string(receiverIP).c_str(), &ipv4.sin_addr) != 1)
                {
                    Log::Print(
                        "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                        + " because ipv4 inet_ptons failed!",
                        "KS_LINK",
                        LogType::LOG_ERROR,
                        2);

                    //always exit thread
                    kclose(targetSocket);
                    return;
                }

                memcpy(&addr, &ipv4, sizeof(ipv4));
                addrLen = sizeof(ipv4);
            }
            else
            {
                sockaddr_in6 ipv6{};
                ipv6.sin6_family = AF_INET6;
                ipv6.sin6_port = htons(receiverPort);
                if (inet_pton(AF_INET6, string(receiverIP).c_str(), &ipv6.sin6_addr) != 1)
                {
                    Log::Print(
                        "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                        + " because ipv6 inet_ptons failed!",
                        "KS_LINK",
                        LogType::LOG_ERROR,
                        2);

                    //always exit thread
                    kclose(targetSocket);
                    return;
                }

                memcpy(&addr, &ipv6, sizeof(ipv6));
                addrLen = sizeof(ipv6);
            }

            if (transportProtocol == TransportProtocol::TP_TCP)
            {
                int result = connect(
                    targetSocket, 
                    rcast<const sockaddr*>(&addr), 
                    addrLen);

#if defined(KWIN_ANY)
                bool inProgress = 
                    result == SOCKET_ERROR
                    && WSAGetLastError() == WSAEWOULDBLOCK;
#else
                bool inProgress = 
                    result == -1
                    && errno == EINPROGRESS;
#endif

                bool connected = result == 0;

                if (!connected
                    && inProgress)
                {
                    fd_set writeSet{};
                    FD_ZERO(&writeSet);
                    FD_SET(targetSocket, &writeSet);

                    timeval timeoutVal{};
                    timeoutVal.tv_sec = timeout;

#if defined(KWIN_ANY)
                    int ready = select(
                        0,
                        nullptr,
                        &writeSet,
                        nullptr,
                        &timeoutVal);
#else
                    int ready = select(
                        targetSocket + 1,
                        nullptr,
                        &writeSet,
                        nullptr,
                        &timeoutVal);
#endif
                    
                    if (ready > 0)
                    {
                        int socketError{};

#if defined(KWIN_ANY)
                        int errorSize = sizeof(socketError);
                        if (getsockopt(
                            targetSocket,
                            SOL_SOCKET,
                            SO_ERROR,
                            (char*)&socketError,
                            &errorSize) == 0)
#else
                        socklen_t errorSize = sizeof(socketError);
                        if (getsockopt(
                            targetSocket,
                            SOL_SOCKET,
                            SO_ERROR,
                            &socketError,
                            &errorSize) == 0)
#endif
                        {
                            connected = socketError == 0;
                        }
                    }
                }

                if (!connected)
                {
                    Log::Print(
                        "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                        + " because the connection could not be established!",
                        "KS_LINK",
                        LogType::LOG_ERROR,
                        2);

                    //always exit thread
                    kclose(targetSocket);
                    return;
                }
            }

            //
            // SEND MESSAGE
            //

            if (transportProtocol == TransportProtocol::TP_TCP)
            {
                size_t totalSent{};

                while (totalSent < payload.size())
                {
                    int sent = send(
                        targetSocket,
                        rcast<const char*>(payload.data() + totalSent),
                        scast<int>(payload.size() - totalSent),
                        0);

                    if (sent > 0)
                    {
                        totalSent += sent;
                        continue;
                    }

                    if (sent == 0)
                    {
                        Log::Print(
                            "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                            + " because TCP socket transmission returned zero bytes!",
                            "KS_LINK",
                            LogType::LOG_ERROR,
                            2);

                        kclose(targetSocket);
                        return;
                    }

#if defined(KWIN_ANY)
                    int error = WSAGetLastError();
                    bool wouldBlock = error == WSAEWOULDBLOCK;
#else
                    int error = errno;
                    bool wouldBlock = error == EWOULDBLOCK || error == EAGAIN;
#endif

                    if (wouldBlock)
                    {
                        fd_set writeSet{};
                        FD_ZERO(&writeSet);
                        FD_SET(targetSocket, &writeSet);

                        timeval timeoutVal{};
                        timeoutVal.tv_sec = timeout;

#if defined(KWIN_ANY)
                        int ready = select(
                            0,
                            nullptr,
                            &writeSet,
                            nullptr,
                            &timeoutVal);
#else
                        int ready = select(
                            targetSocket + 1,
                            nullptr,
                            &writeSet,
                            nullptr,
                            &timeoutVal);
#endif
                        
                        if (ready <= 0)
                        {
                            Log::Print(
                                "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                                + " because TCP socket transmission timed out or select failed!",
                                "KS_LINK",
                                LogType::LOG_ERROR,
                                2);

                            kclose(targetSocket);
                            return;
                        }
                    }
                    else
                    {
                        Log::Print(
                            "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                            + " because TCP socket transmission failed!",
                            "KS_LINK",
                            LogType::LOG_ERROR,
                            2);

                        kclose(targetSocket);
                        return;
                    }
                }
            }
            else
            {
                if (payload.size() > (isIPv4 ? MAX_UDP_PAYLOAD_SIZE_IPV4 : MAX_UDP_PAYLOAD_SIZE_IPV6))
                {
                    Log::Print(
                        "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                        + " because UDP payload exceeds maximum UDP payload send size!",
                        "KS_LINK",
                        LogType::LOG_ERROR,
                        2);

                    kclose(targetSocket);
                    return;
                }

                while (true)
                {
                    int sent = sendto(
                        targetSocket,
                        rcast<const char*>(payload.data()),
                        scast<int>(payload.size()),
                        0,
                        rcast<const sockaddr*>(&addr),
                        addrLen);

                    if (sent >= 0) break;

#if defined(KWIN_ANY)
                    int error = WSAGetLastError();
                    bool wouldBlock = error == WSAEWOULDBLOCK;
#else
                    int error = errno;
                    bool wouldBlock = error == EWOULDBLOCK || error == EAGAIN;
#endif

                    if (wouldBlock)
                    {
                        fd_set writeSet{};
                        FD_ZERO(&writeSet);
                        FD_SET(targetSocket, &writeSet);

                        timeval timeoutVal{};
                        timeoutVal.tv_sec = timeout;

#if defined(KWIN_ANY)
                        int ready = select(
                            0,
                            nullptr,
                            &writeSet,
                            nullptr,
                            &timeoutVal);
#else
                        int ready = select(
                            targetSocket + 1,
                            nullptr,
                            &writeSet,
                            nullptr,
                            &timeoutVal);
#endif
                        
                        if (ready <= 0)
                        {
                            Log::Print(
                                "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                                + " because UDP socket transmission timed out or select failed!",
                                "KS_LINK",
                                LogType::LOG_ERROR,
                                2);

                            kclose(targetSocket);
                            return;
                        }
                    }
                    else
                    {
                        Log::Print(
                            "Failed to send packet via " + string(senderStr) + " to " + string(receiverStr) 
                            + " because UDP socket transmission failed!",
                            "KS_LINK",
                            LogType::LOG_ERROR,
                            2);

                        kclose(targetSocket);
                        return;
                    }
                }
            }

            //
            // GET BACK RESPONSE
            //

            Log::Print(
                "Packet has been sent from " + string(senderStr) + " to " + string(receiverStr) + "!",
                "KS_LINK",
                LogType::LOG_SUCCESS);

            if (responseType == ResponseType::RT_IgnoreResponse
                || ((server
                && !server->IsVisible())
                || (!server
                && !Client::IsVisible())))
            {
                if ((server
                    && !server->IsVisible())
                    || (!server
                    && !Client::IsVisible()))
                {
                    Log::Print(
                        "Response for " + string(senderStr) + " was discarded from " 
                        + string(receiverStr) + " because the sender was set as invisible.",
                        "KS_LINK",
                        LogType::LOG_INFO);
                }

                kclose(targetSocket);
                return;
            }

            fd_set readSet{};
            FD_ZERO(&readSet);
            FD_SET(targetSocket, &readSet);

            timeval responseTimeout{};
            responseTimeout.tv_sec = timeout;

#if defined(KWIN_ANY)
            int ready = select(
                0,
                &readSet,
                nullptr,
                nullptr,
                &responseTimeout);
#else
            int ready = select(
                targetSocket + 1,
                &readSet,
                nullptr,
                nullptr,
                &responseTimeout);
#endif

            if (ready <= 0)
            {
                Log::Print(
                    "Failed receive response for " + string(senderStr) 
                    + " from " + string(receiverStr) + " "
                    "because the response timed out or select failed!",
                    "KS_LINK",
                    LogType::LOG_ERROR,
                    2);

                kclose(targetSocket);
                return;
            }

            vector<u8> response{};
            int received{};

            if (transportProtocol == TransportProtocol::TP_TCP)
            {
                response.resize(MAX_TCP_PAYLOAD_SIZE);

                received = recv(
                    targetSocket,
                    rcast<char*>(response.data()),
                    scast<int>(response.size()),
                    0);
            }
            else
            {
                response.resize(isIPv4 ? MAX_UDP_PAYLOAD_SIZE_IPV4 : MAX_UDP_PAYLOAD_SIZE_IPV6);

                sockaddr_storage senderAddr{};

#if defined(KWIN_ANY)
                int senderLen = sizeof(senderAddr);
#else
                socklen_t senderLen = sizeof(senderAddr);
#endif

                received = recvfrom(
                    targetSocket,
                    rcast<char*>(response.data()),
                    scast<int>(response.size()),
                    0,
                    rcast<sockaddr*>(&senderAddr),
                    &senderLen);
            }

            if (received < 0
                || (received == 0
                && transportProtocol == TransportProtocol::TP_TCP))
            {
                Log::Print(
                    "Failed receive response for " + string(senderStr) 
                    + " from " + string(receiverStr) + " because receiving data failed!",
                    "KS_LINK",
                    LogType::LOG_ERROR,
                    2);

                kclose(targetSocket);
                return;
            }

            kclose(targetSocket);

            Log::Print(
                "Response for " + string(senderStr) + " has been received from " + string(receiverStr) + "!",
                "KS_LINK",
                LogType::LOG_SUCCESS);

            response.resize(received);

            if (initialResponseCallback) initialResponseCallback(std::move(response));
        };

    {
        scoped_lock lock(threadDataMutex);

        threadData.push_back(
            {
                .linkID = 0,
                .thisThread = thread(worker_thread)
            });
    }
}