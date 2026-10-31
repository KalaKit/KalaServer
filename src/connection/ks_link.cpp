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
using KalaServer::Connection::ResponseType;
using KalaServer::Connection::Server;
using KalaServer::Connection::Client;

using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::function;
using std::unique_ptr;
using std::make_unique;

#if defined(KWIN_ANY)
using ksocket = SOCKET;
constexpr ksocket invalid_socket = INVALID_SOCKET;
#define kclose(sock) closesocket(sock)
#else
using ksocket = int;
constexpr ksocket invalid_socket = -1;
#define kclose(sock) close(sock)
#endif

static constexpr u32 MAX_TCP_PAYLOAD_SIZE = UINT16_MAX;

//max ipv4 packet 65535 -20 for ipv4 header, -8 for udp header
static constexpr u32 MAX_UDP_PAYLOAD_SIZE_IPV4 = 65507;
//max ipv6 packet 65535 -8 for udp header
static constexpr u32 MAX_UDP_PAYLOAD_SIZE_IPV6 = 65527;

static KalaServerRegistry<Link> registry{};

static void _SendPacket(
    u32 senderID,
    SenderType senderType,
    string_view receiverIP,
    u16 receiverPort,
    u8 timeout,
    TransportProtocol transportProtocol,
    ResponseType responseType,
    const vector<u8>& payload,
    function<void(vector<u8>)> responseCallback);

namespace KalaServer::Connection
{
    KalaServerRegistry<Link>& Link::GetRegistry() { return registry; }

    void Link::SendPacket(
        u32 senderID,
        SenderType senderType,
        string_view receiverIP,
        u16 receiverPort,
        u8 timeout,
        TransportProtocol transportProtocol,
        ResponseType responseType,
        const vector<u8>& payload,
        function<void(vector<u8>)> responseCallback)
    {
        Server* server{};
        Client* client{};

        if (senderType == SenderType::S_SERVER)
        {
            string err = Server::GetRegistry().GetContent(senderID, server);
            if (!err.empty())
            {
                Log::Print(
                    "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                    + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                    "because the sender ID for server type was invalid! Reason: " + err,
                    "KS_LINK",
                    LogType::LOG_WARNING);

                return;
            }
        }
        else
        {
            string err = Client::GetRegistry().GetContent(senderID, client);
            if (!err.empty())
            {
                Log::Print(
                    "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                    + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                    "because the sender ID for client type was invalid! Reason: " + err,
                    "KS_LINK",
                    LogType::LOG_WARNING);

                return;
            }
        }

        const bool isIPv4 = KalaServerCore::IsIPv4(receiverIP); 

        //
        // PREPARE TARGET SOCKET
        //

        const int addressFamily = isIPv4 ? AF_INET : AF_INET6;
        ksocket targetSocket = transportProtocol == TransportProtocol::TP_TCP
            ? socket(addressFamily, SOCK_STREAM, IPPROTO_TCP)
            : socket(addressFamily, SOCK_DGRAM, IPPROTO_UDP);
        
        if (targetSocket == invalid_socket)
        {
			Log::Print(
				"Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                "because socket creation failed",
				"KS_LINK",
				LogType::LOG_ERROR,
				2);

            return;
        }

#if defined (KWIN_ANY)
		u_long mode = 1;
		if (ioctlsocket(targetSocket, FIONBIO, &mode) == SOCKET_ERROR)
		{
			string err = KalaServerCore::ErrorToString(WSAGetLastError());

			Log::Print(
				"Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                "because nonblocking socket mode couldn't be enabled! Reason: " + err,
				"KS_LINK",
				LogType::LOG_ERROR,
				2);

            kclose(targetSocket);
			return;
		}
#else
		int flags = fcntl(targetSocket, F_GETFL, 0);

		if (flags == -1
			|| fcntl(targetSocket, F_SETFL, flags | O_NONBLOCK) == -1)
		{
			Log::Print(
				"Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                "because nonblocking socket mode couldn't be enabled!",
				"KS_LINK",
				LogType::LOG_ERROR,
				2);

            kclose(targetSocket);
			return;
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
                    "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                    + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                    "because ipv4 inet_ptons failed!",
                    "KS_LINK",
                    LogType::LOG_ERROR,
                    2);

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
                    "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                    + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                    "because ipv6 inet_ptons failed!",
                    "KS_LINK",
                    LogType::LOG_ERROR,
                    2);

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
                    "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                    + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                    "because the connection could not be established!",
                    "KS_LINK",
                    LogType::LOG_ERROR,
                    2);

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
                        "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                        + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                        "because TCP socket transmission returned zero bytes!",
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
                            "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                            + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                            "because TCP socket transmission timed out or select failed!",
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
                        "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                        + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                        "because TCP socket transmission failed!",
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
                    "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                    + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                    "because UDP payload exceeds maximum UDP payload send size!",
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
                            "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                            + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                            "because UDP socket transmission timed out or select failed!",
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
                        "Failed to send packet via sender '" + to_string(senderID) + "' to receiver '" 
                        + string(receiverIP) + "' at port '" + to_string(receiverPort) + "' "
                        "because UDP socket transmission failed!",
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
            "Packet has been sent from sender '" + to_string(senderID) + "' to receiver '" 
            + string(receiverIP) + "' via port '" + to_string(receiverPort) + "'!",
            "KS_LINK",
            LogType::LOG_SUCCESS);

        if (responseType == ResponseType::RT_IgnoreResponse
            || ((server
            && !server->isVisible)
            || (client
            && !client->isVisible)))
        {
            if ((server
                && !server->isVisible)
                || (client
                && !client->isVisible))
            {
                Log::Print(
                    "Response for sender '" + to_string(senderID) + "' was discarded from receiver '" 
                    + string(receiverIP) + "' via port '" + to_string(receiverPort) + "' because the sender was set as invisible.",
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
                "Failed receive response from receiver '" + string(receiverIP) 
                + "' at port '" + to_string(receiverPort) + "' "
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
                "Failed receive response from receiver '" + string(receiverIP) 
                + "' at port '" + to_string(receiverPort) + "' "
                "because receiving data failed!",
                "KS_LINK",
                LogType::LOG_ERROR,
                2);

            kclose(targetSocket);
            return;
        }

        kclose(targetSocket);

        Log::Print(
            "Response for sender '" + to_string(senderID) + "' has been received from receiver '" 
            + string(receiverIP) + "' via port '" + to_string(receiverPort) + "'!",
            "KS_LINK",
            LogType::LOG_SUCCESS);

        response.resize(received);

        if (responseCallback) responseCallback(std::move(response));
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
                "Failed to initialize link because KalaServer was not initialized!",
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
			"Created new link '" + to_string(newID) + "' between sender '" + to_string(senderID) 
            + "' and receiver '" + string(receiverIP) + "' via port '" + to_string(receiverPort) + "'!",
			"KS_LINK",
			LogType::LOG_SUCCESS);

		return linkPtr;
    }

    u32 Link::GetID() const { return ID; }

    u32 Link::GetSenderID() const { return senderID; }
    SenderType Link::GetSenderType() const { return senderType; }

    const string& Link::GetReceiverIP() const { return receiverIP; }
    u16 Link::GetReceiverPort() const { return receiverPort; }

    u8 Link::GetTimeout() const { return timeout; }
    void Link::SetTimeout(u8 newValue) { timeout = newValue; }

    TransportProtocol Link::GetTransportProtocol() const { return transportProtocol; }

    void Link::SendPacket(
        PacketConnectionMode packetConnectionMode, 
        ResponseType responseType, 
        const vector<u8>& payload,
        function<void(vector<u8>)> responseCallback)
    {

    }

    void Link::Update()
    {
        
    }

    void Link::Destroy()
    {
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