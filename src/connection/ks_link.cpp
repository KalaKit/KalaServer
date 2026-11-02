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

using KalaServer::Connection::TransportProtocol;
using KalaServer::Connection::PacketConnectionMode;
using KalaServer::Connection::ResponseType;
using KalaServer::Connection::Server;
using KalaServer::Connection::Client;

using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::pair;
using std::function;
using std::atomic;
using std::shared_mutex;
using std::thread;
using std::shared_lock;
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

struct LinkData
{
    u32 linkID{};
    u32 senderID{};

    string receiverIP{};
    u16 receiverPort{};

    u8 timeout{};

    TransportProtocol transportProtocol{};
    PacketConnectionMode packetConnectionMode{};
    ResponseType responseType{};

    ksocket socket = invalid_socket;
    thread thisThread{};

    vector<vector<u8>> responses{};
};

static constexpr u32 MAX_TCP_PAYLOAD_SIZE = UINT16_MAX;

//max ipv4 packet 65535 -20 for ipv4 header, -8 for udp header
static constexpr u32 MAX_UDP_PAYLOAD_SIZE_IPV4 = 65507;
//max ipv6 packet 65535 -8 for udp header
static constexpr u32 MAX_UDP_PAYLOAD_SIZE_IPV6 = 65527;

static KalaServerRegistry<Link> registry{};

static atomic<u32> linkDataID{};

static shared_mutex linkDataMutex{};
static vector<pair<u32, LinkData>> linkData{};

static void _SendPacket(
    const u32 linkDataID,
    const vector<u8>& payload,
    const function<void(vector<u8>)>& responseCallback);

namespace KalaServer::Connection
{
    KalaServerRegistry<Link>& Link::GetRegistry() { return registry; }

    u32 Link::GetActiveHeadlessConnectionCount()
    { 
        shared_lock lock(linkDataMutex);
        u32 headlessDataCount{};

        for (const pair<u32, LinkData>& ld : linkData)
        {
            if (ld.second.linkID == 0) headlessDataCount++;
        }

        return headlessDataCount;
    }

    void Link::SendPacket(
        u32 senderID,
        string_view receiverIP,
        u16 receiverPort,
        u8 timeout,
        TransportProtocol transportProtocol,
        ResponseType responseType,
        const vector<u8>& payload,
        const function<void(vector<u8>)>& responseCallback)
    {
        u32 nextLinkDataID = linkDataID.fetch_add(1) + 1;

        {
            scoped_lock lock(linkDataMutex);
            linkData.push_back(
                {
                    nextLinkDataID,
                    LinkData{
                        .linkID = 0,
                        .senderID = senderID,
                        .receiverIP = string(receiverIP),
                        .receiverPort = receiverPort,
                        .timeout = timeout,
                        .transportProtocol = transportProtocol,
                        .packetConnectionMode = PacketConnectionMode::PCM_DisconnectAfterSend,
                        .responseType = responseType
                    }
                });
        }

        _SendPacket(
            nextLinkDataID,
            payload,
            responseCallback);
    }

    Link* Link::Initialize(
        u32 senderID,
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

        if (KalaServerCore::IsShuttingDown())
        {
            Log::Print(
                "Failed to initialize link because KalaServer is shutting down!",
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

    const string& Link::GetReceiverIP() const { return receiverIP; }
    u16 Link::GetReceiverPort() const { return receiverPort; }

    TransportProtocol Link::GetTransportProtocol() const { return transportProtocol; }

    u32 Link::GetActiveConnectionCount() const
    { 
        shared_lock lock(linkDataMutex);
        u32 linkDataCount{};

        for (const pair<u32, LinkData>& ld : linkData)
        {
            if (ld.second.linkID == ID) linkDataCount++;
        }

        return linkDataCount;
    }

    void Link::SendPacket(
        const u8 timeout,
        const PacketConnectionMode packetConnectionMode, 
        const ResponseType responseType, 
        const vector<u8>& payload,
        const function<void(vector<u8>)>& responseCallback)
    {
        u32 nextLinkDataID = linkDataID.fetch_add(1) + 1;

        {
            scoped_lock lock(linkDataMutex);
            linkData.push_back(
                {
                    nextLinkDataID,
                    LinkData{
                        .linkID = ID,
                        .senderID = senderID,
                        .receiverIP = string(receiverIP),
                        .receiverPort = receiverPort,
                        .timeout = timeout,
                        .transportProtocol = transportProtocol,
                        .packetConnectionMode = packetConnectionMode,
                        .responseType = responseType
                    }
                });
        }

        _SendPacket(
            nextLinkDataID,
            payload,
            responseCallback);
    }

    void Link::Shutdown()
    {
        vector<thread> threads{};

        {
            scoped_lock lock(linkDataMutex);

            threads.reserve(linkData.size());

            for (pair<u32, LinkData>& ld : linkData)
            {
                if (ld.second.socket != invalid_socket)
                {
#if defined(KWIN_ANY)
                    shutdown(ld.second.socket, SD_BOTH);
#else
                    shutdown(ld.second.socket, SHUT_RDWR);
#endif
                }

                threads.push_back(std::move(ld.second.thisThread));
            }
        }

        for (thread& t : threads)
        {
            if (t.joinable()) t.join();
        }

        {
            scoped_lock lock(linkDataMutex);
            linkData.clear();
        }

        string err = registry.DestroyAllContent();

        if (!err.empty())
        {
            KalaServerCore::ForceClose(
                "KalaServer Link error",
                "Failed to shut down! Reason: " + err);
        }
    }

    void Link::Destroy()
    {
        vector<thread> threadsCopy{};

        {
            scoped_lock lock(linkDataMutex);

            for (pair<u32, LinkData>& entry : linkData)
            {
                LinkData& ld = entry.second;

                if (ld.linkID != ID) continue;

                if (ld.socket != invalid_socket)
                {
#if defined(KWIN_ANY)
                    shutdown(ld.socket, SD_BOTH);
#else
                    shutdown(ld.socket, SHUT_RDWR);
#endif
                }

                threadsCopy.push_back(std::move(ld.thisThread));
            }
        }

        for (thread& t : threadsCopy)
        {
            if (t.joinable()) t.join();
        }

        {
            scoped_lock lock(linkDataMutex);

            for (size_t i = linkData.size(); i > 0; i--)
            {
                if (linkData[i - 1].second.linkID == ID)
                {
                    linkData.erase(linkData.begin() + (i - 1));
                }
            }
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
    const u32 linkDataID,
    const vector<u8>& payload,
    const function<void(vector<u8>)>& responseCallback)
{
    auto worker_thread = [
        linkDataID,
        payload,
        responseCallback](
        const string senderStr,
        const string receiverStr,
        const string receiverIP,
        const u16 receiverPort,
        const u8 timeout,
        const TransportProtocol transportProtocol,
        const PacketConnectionMode packetConnectionMode,
        const ResponseType responseType) -> void
        {
            const bool isIPv4 = KalaServerCore::IsIPv4(receiverIP);

            auto create_socket = [](
                const string receiverIP,
                const TransportProtocol transportProtocol) -> ksocket
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

            auto close_socket = [linkDataID](ksocket& targetSocket) -> void
                {
                    scoped_lock lock(linkDataMutex);
                    for (pair<u32, LinkData>& ld : linkData)
                    {
                        if (ld.first == linkDataID)
                        {
                            if (ld.second.socket != invalid_socket) ld.second.socket = invalid_socket;
                            break;
                        }
                    }

                    kclose(targetSocket);
                };

            ksocket targetSocket = create_socket(receiverIP, transportProtocol);

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

            {
                scoped_lock lock(linkDataMutex);
                for (pair<u32, LinkData>& ld : linkData)
                {
                    if (ld.first == linkDataID)
                    {
                        ld.second.socket = targetSocket;
                        break;
                    }
                }
            }

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

                    close_socket(targetSocket);
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

                    close_socket(targetSocket);
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

                    close_socket(targetSocket);
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

                        close_socket(targetSocket);
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

                            close_socket(targetSocket);
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

                        close_socket(targetSocket);
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

                    close_socket(targetSocket);
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

                            close_socket(targetSocket);
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

                        close_socket(targetSocket);
                        return;
                    }
                }
            }

            if (KalaServerCore::IsShuttingDown())
            {
                close_socket(targetSocket);
                return;
            }

            //
            // GET BACK RESPONSE
            //

            Log::Print(
                "Packet has been sent from " + string(senderStr) + " to " + string(receiverStr) + "!",
                "KS_LINK",
                LogType::LOG_SUCCESS);

            bool shouldExit{};

            u32 senderID{};
            {
                shared_lock lock(linkDataMutex);
                for (const pair<u32, LinkData>& ld : linkData)
                {
                    if (ld.first == linkDataID)
                    {
                        senderID = ld.second.senderID;
                        break;
                    }
                }
            }

            string err = Server::GetRegistry().LockRegistry([
                &shouldExit,
                responseType,
                senderID,
                senderStr,
                receiverStr]()
                {
                    Server* server{};
                    if (senderID != 0)
                    {
                        string _ = Server::GetRegistry().GetContent(senderID, server);
                        if (!server)
                        {
                            Log::Print(
                                "Response for " + string(senderStr) + " was discarded from " 
                                + string(receiverStr) + " because the server was shut down.",
                                "KS_LINK",
                                LogType::LOG_WARNING);

                            shouldExit = true;
                            return;
                        }
                    }
                    else
                    {
                        if (!Client::IsInitialized())
                        {
                            Log::Print(
                                "Response for " + string(senderStr) + " was discarded from " 
                                + string(receiverStr) + " because the client was shut down.",
                                "KS_LINK",
                                LogType::LOG_WARNING);

                            shouldExit = true;
                            return;
                        }
                    }

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

                        shouldExit = true;
                    }
                });

            if (!err.empty())
            {
                Log::Print(
                    "Response for " + string(senderStr) + " couldn't be received! Reason: " + err,
                    "KS_LINK",
                    LogType::LOG_ERROR,
                    2);

                shouldExit = true;
            }

            if (shouldExit)
            {
                close_socket(targetSocket);
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

                close_socket(targetSocket);
                return;
            }

            auto has_become_invsibile = [](
                const u32 linkDataID,
                const string senderStr,
                const string receiverStr) -> bool
                {
                    bool invisible = false;
                    u32 senderID{};

                    {
                        scoped_lock lock(linkDataMutex);
                        for (const pair<u32, LinkData>& ld : linkData)
                        {
                            if (ld.first == linkDataID)
                            {
                                senderID = ld.second.senderID;
                                break;
                            }
                        }
                    }

                    if (senderID != 0)
                    {
                        string err = Server::GetRegistry().LockRegistry([
                            &invisible,
                            &senderID,
                            &senderStr,
                            &receiverStr]()
                            {
                                for (const Server* s : Server::GetRegistry().GetAllContent())
                                {
                                    if (s->GetID() == senderID)
                                    {
                                        if (!s->IsVisible())
                                        {
                                            Log::Print(
                                                "Dropping connection for " + string(senderStr) 
                                                + " and " + string(receiverStr) + " because server has become invisible.",
                                                "KS_LINK",
                                                LogType::LOG_INFO);

                                            invisible = true;
                                        }

                                        break;
                                    }
                                }
                            });

                        if (!err.empty())
                        {
                            KalaServerCore::ForceClose(
                                "KalaServer Link error",
                                "Failed to check server status! Reason: " + err);
                        }
                    }
                    else if (senderID == 0
                        && !Client::IsVisible())
                    {
                        Log::Print(
                            "Dropping connection for " + string(senderStr) 
                            + " and " + string(receiverStr) + " because client has become invisible.",
                            "KS_LINK",
                            LogType::LOG_INFO);

                        invisible = true;
                    }

                    return invisible;
                };

            bool isAlive = true;

            while (isAlive)
            {
                if (KalaServerCore::IsShuttingDown())
                {
                    Log::Print(
                        "Closing connection between " + string(senderStr) 
                        + " and " + string(receiverStr) + " because KalaServer is shutting down.",
                        "KS_LINK",
                        LogType::LOG_INFO);

                    isAlive = false;
                    break;
                }

                if (has_become_invsibile(
                    linkDataID,
                    senderStr,
                    receiverStr))
                {
                    isAlive = false;
                    break;
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
                    response.resize(isIPv4 
                        ? MAX_UDP_PAYLOAD_SIZE_IPV4 
                        : MAX_UDP_PAYLOAD_SIZE_IPV6);

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

                if (received <= 0)
                {
                    if (received < 0)
                    {
                        Log::Print(
                            "Failed receive response for " + string(senderStr) 
                            + " from " + string(receiverStr) + " because receiving data failed!",
                            "KS_LINK",
                            LogType::LOG_ERROR,
                            2);

                        break;
                    }
                    else if (received == 0
                        && transportProtocol == TransportProtocol::TP_TCP)
                    {
                        Log::Print(
                            "Closing connection between " + string(senderStr) 
                            + " and " + string(receiverStr) + " because receiver has closed the connection.",
                            "KS_LINK",
                            LogType::LOG_INFO);

                        break;
                    }
                }

                if (has_become_invsibile(
                    linkDataID,
                    senderStr,
                    receiverStr))
                {
                    isAlive = false;
                    break;
                }

                if (KalaServerCore::IsShuttingDown()
                    || packetConnectionMode != PacketConnectionMode::PCM_KeepAlive)
                {
                    if (KalaServerCore::IsShuttingDown())
                    {
                        Log::Print(
                            "Closing connection between " + string(senderStr) 
                            + " and " + string(receiverStr) + " because KalaServer is shutting down.",
                            "KS_LINK",
                            LogType::LOG_INFO);

                        isAlive = false;
                        break;
                    }
                    if (packetConnectionMode != PacketConnectionMode::PCM_KeepAlive)
                    {
                        Log::Print(
                            "Closing connection between " + string(senderStr) 
                            + " and " + string(receiverStr) + " because the connection is not long lasting.",
                            "KS_LINK",
                            LogType::LOG_INFO);
                    }

                    response.resize(received);

                    if (responseCallback) responseCallback(std::move(response));

                    isAlive = false;
                    break;
                }

                response.resize(received);

                if (responseCallback) responseCallback(std::move(response));
            }

            close_socket(targetSocket);
        };

    {
        scoped_lock lock(linkDataMutex);

        for (pair<u32, LinkData>& ld : linkData)
        {
            if (ld.first == linkDataID)
            {
                LinkData& data = ld.second;

                const string receiverStr = 
                    "reciever '" + string(data.receiverIP) 
                    + "' at port '" + to_string(data.receiverPort) + "'";

                Link* link{};

                if (data.linkID != 0)
                {
                    string err = Link::GetRegistry().GetContent(data.linkID, link);
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

                if (data.senderID != 0)
                {
                    string err = Server::GetRegistry().GetContent(data.senderID, server);
                    if (!err.empty())
                    {
                        Log::Print(
                            "Failed to send packet via server '" + to_string(data.senderID) + "' to " + receiverStr +
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

                const string senderStr = server 
                    ? "server '" + server->GetServerName() + " (" + to_string(server->GetID()) + ")'"
                    : "client '" + Client::GetUsername() + "'";

                if (data.linkID == 0
                    && payload.empty())
                {
                    Log::Print(
                        "Failed to send packet via " + senderStr + " to " + receiverStr 
                        + " because the payload was empty!",
                        "KS_LINK",
                        LogType::LOG_WARNING);

                    return;
                }

                ld.second.thisThread = thread(
                    std::move(worker_thread),
                    senderStr,
                    receiverStr,
                    data.receiverIP,
                    data.receiverPort,
                    data.timeout,
                    data.transportProtocol,
                    data.packetConnectionMode,
                    data.responseType);
            }
        }
    }
}