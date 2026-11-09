//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "connection/ks_server.hpp"

#if defined(KWIN_ANY)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

#include <thread>

#include "log_utils.hpp"
#include "file_utils.hpp"
#include "string_utils.hpp"

#include "core/ks_core.hpp"
#include "connection/ks_client.hpp"
#include "connection/ks_link.hpp"

using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::GetCleanPath;

using KalaHeaders::KalaString::IsStringInRange;

using KalaServer::Core::KalaServerCore;
using KalaServer::Core::KalaServerRegistry;
using KalaServer::Connection::Server;
using KalaServer::Connection::TransportProtocol;
using KalaServer::Connection::PacketConnectionMode;
using KalaServer::Connection::ResponseType;

using std::string;
using std::to_string;
using std::vector;
using std::pair;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::function;
using std::thread;
using std::atomic;
using std::shared_mutex;
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

struct ServerData
{
    u32 serverID{};

    string clientIP{};
    u16 clientPort{};

    u8 timeout{};

    TransportProtocol transportProtocol{};
    PacketConnectionMode packetConnectionMode{};
    ResponseType responseType{};

    ksocket socket = invalid_socket;
    sockaddr_storage addr{};
    i32 addrLen{};

    thread thisThread{};
};

static KalaServerRegistry<Server> registry{};

static atomic<u32> serverDataID{};

static shared_mutex serverDataMutex{};
static vector<pair<u32, ServerData>> serverData{};

static void _StartConnection(
    const u32 serverDataID,
    const function<vector<u8>(string, u16, vector<u8>)>& messageCallback);

static void _StartClient(
    const u32 serverDataID,
    const u32 clientDataID,
    const function<vector<u8>(string, u16, vector<u8>)>& messageCallback);

namespace KalaServer::Connection
{
    KalaServerRegistry<Server>& Server::GetRegistry() { return registry; }

    Server* Server::Initialize(
        string_view serverName,
        u16 serverPort,
        const function<vector<u8>(string, u16, vector<u8>)>& messageCallback)
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to initialize server because KalaServer has not been initialized!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }

        if (KalaServerCore::IsShuttingDown())
        {
            Log::Print(
                "Failed to initialize server because KalaServer is shutting down!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }

        if (serverName.empty())
        {
            Log::Print(
                "Failed to initialize server for IP '" 
                + string(KalaServerCore::GetIP()) + "' because the server name was empty!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }

        bool isStringInRange{};
        string err = IsStringInRange(
            serverName,
            MIN_SERVER_NAME_LENGTH,
            MAX_SERVER_NAME_LENGTH,
            isStringInRange);

        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize server for IP '" 
                + string(KalaServerCore::GetIP()) + "'! Reason: " + err,
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }
        if (!isStringInRange)
        {
            Log::Print(
                "Failed to initialize server for IP '" 
                + string(KalaServerCore::GetIP()) + "' because the server name length was out of range!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }

        bool continueInit = true;
        err = registry.LockRegistry([serverName, &continueInit]()
            {
                for (const Server* s : registry.GetAllContent())
                {
                    if (s->serverName == serverName)
                    {
                        Log::Print(
                            "Failed to initialize server because the server name '" + string(serverName) + "' is already in use!",
                            "KS_SERVER",
                            LogType::LOG_WARNING);

                        continueInit = false;
                        return;
                    }
                } 
            });
		if (!err.empty())
		{
			KalaServerCore::ForceClose(
				"KalaServer Server error",
				"Failed to initialize server! Reason: " + err);
		}

        if (!continueInit) return nullptr;

        if (serverPort < MIN_PORT_RANGE
            || serverPort > MAX_PORT_RANGE)
        {
            Log::Print(
                "Failed to initialize server for IP '" 
                + string(KalaServerCore::GetIP()) + "' because the server port '" + to_string(serverPort) + "' was out of range!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }

		u32 newID = KalaServerCore::GetGlobalID() + 1;
		KalaServerCore::SetGlobalID(newID);

		unique_ptr<Server> newServer = make_unique<Server>();
		Server* serverPtr = newServer.get();

        serverPtr->ID = newID;
        serverPtr->serverName = serverName;
        serverPtr->serverPort = serverPort;
        serverPtr->messageCallback = messageCallback;

		err = registry.AddContent(
			newID, 
			std::move(newServer));
		if (!err.empty())
		{
			KalaServerCore::ForceClose(
				"KalaServer Server error",
				"Failed to initialize server! Reason: " + err);
		}

		Log::Print(
			"Created new server '" + to_string(newID) + "'!",
			"KS_SERVER",
			LogType::LOG_SUCCESS);

        u32 nextServerDataID = serverDataID.fetch_add(1) + 1;
        {
            scoped_lock lock(serverDataMutex);
            serverData.push_back(
                {
                    nextServerDataID,
                    ServerData{
                        .serverID = newID,
                        .timeout = 5,
                        .transportProtocol = TransportProtocol::TP_TCP
                    }
                });
        }

        _StartConnection(
            nextServerDataID,
            messageCallback);

		return serverPtr;
    }

    u32 Server::GetID() const
    {
        shared_lock lock(serverMutex);
        return ID;
    }

    bool Server::IsVisible() const
    {
        shared_lock lock(serverMutex); 
        return isVisible;
    }
    void Server::SetVisibility(bool newValue)
    {
        scoped_lock lock(serverMutex);

        if (newValue
            && isVisible)
        {
            Log::Print(
                "Failed to set server '" + to_string(ID) + "' visible because it already was visible!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return;
        }
        if (!newValue
            && !isVisible)
        {
            Log::Print(
                "Failed to set server '" + to_string(ID) + "' invisible because it already was invisible!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return;
        }

        isVisible = newValue;

        Log::Print(
            "Changed server '" + to_string(ID) + "' visibility to '" + (isVisible ? "on" : "off") + "'.",
            "KS_SERVER",
            LogType::LOG_SUCCESS);
    }

    string Server::GetServerName() const
    { 
        return serverName;
    }

    u16 Server::GetServerPort() const
    {
        shared_lock lock(serverMutex); 
        return serverPort;
    }

    vector<string> Server::GetBannedIPs()
    {
        shared_lock lock(serverMutex);
        return bannedIPs;
    }
    void Server::BanIP(string_view IP)
    {
        scoped_lock lock(serverMutex);
        if (ContainsValue(bannedIPs, IP))
        {
            Log::Print(
                "Failed to ban IP '" + string(IP) + "' because it is already banned!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return;
        }

        bannedIPs.push_back(string(IP));

        Log::Print(
            "IP '" + string(IP) + "' has been banned!",
            "KS_SERVER",
            LogType::LOG_SUCCESS);
    }
    void Server::UnbanIP(string_view IP)
    {
        scoped_lock lock(serverMutex);

        auto it = find(bannedIPs.begin(), bannedIPs.end(), IP);
        if (it == bannedIPs.end())
        {
            Log::Print(
                "Failed to unban IP '" + string(IP) + "' because it has not been banned!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return;
        }
        bannedIPs.erase(it);

        Log::Print(
            "IP '" + string(IP) + "' has been unbanned!",
            "KS_SERVER",
            LogType::LOG_SUCCESS);
    }

    void Server::ImportBannedIPs(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }
    void Server::ExportBannedIPs(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }

    void Server::Shutdown()
    {
        vector<thread> threads{};

        string err = registry.LockRegistry([]()
            {
                for (Server* s : registry.GetAllContent())
                {
                    scoped_lock lock(s->serverMutex);
                    s->isVisible = false;
                }
            });

        if (!err.empty())
        {
            KalaServerCore::ForceClose(
                "KalaServer Server error",
                "Failed to shut down! Reason: " + err);
        }

        {
            scoped_lock lock(serverDataMutex);

            threads.reserve(serverData.size());

            for (pair<u32, ServerData>& sd : serverData)
            {
                if (sd.second.socket != invalid_socket)
                {
#if defined(KWIN_ANY)
                    shutdown(sd.second.socket, SD_BOTH);
#else
                    shutdown(sd.second.socket, SHUT_RDWR);
#endif
                }

                threads.push_back(std::move(sd.second.thisThread));
            }
        }

        for (thread& t : threads)
        {
            if (t.joinable()) t.join();
        }

        {
            scoped_lock lock(serverDataMutex);
            serverData.clear();
        }

        err = registry.DestroyAllContent();

        if (!err.empty())
        {
            KalaServerCore::ForceClose(
                "KalaServer Server error",
                "Failed to shut down! Reason: " + err);
        }
    }

    void Server::Destroy()
    {
        vector<thread> threads{};

        {
            scoped_lock lock(serverMutex);
            isVisible = false;
        }

        {
            scoped_lock lock(serverDataMutex);

            for (pair<u32, ServerData>& entry : serverData)
            {
                ServerData& sd = entry.second;

                if (sd.serverID != ID) continue;

                if (sd.socket != invalid_socket)
                {
#if defined(KWIN_ANY)
                    shutdown(sd.socket, SD_BOTH);
#else
                    shutdown(sd.socket, SHUT_RDWR);
#endif
                }

                threads.push_back(std::move(sd.thisThread));
            }
        }

        for (thread& t : threads)
        {
            if (t.joinable()) t.join();
        }

        {
            scoped_lock lock(serverDataMutex);

            for (size_t i = serverData.size(); i > 0; i--)
            {
                if (serverData[i - 1].second.serverID == ID)
                {
                    serverData.erase(serverData.begin() + (i - 1));
                }
            }
        }

		string err = registry.DestroyContent(ID);
		if (!err.empty())
		{
			KalaServerCore::ForceClose(
				"KalaServer Server error",
				"Failed to destroy server! Reason: " + err);
		}
    }

    Server::~Server()
    {
		Log::Print(
			"Destroying server '" + to_string(ID) + "'.",
			"KS_SERVER",
			LogType::LOG_INFO);
    }
}

void _StartConnection(
    const u32 thisServerDataID,
    const function<vector<u8>(string, u16, vector<u8>)>& messageCallback)
{
    auto worker_thread = [
        thisServerDataID,
        messageCallback](
        const string serverStr,
        const u32 serverID,
        const u8 timeout,
        const TransportProtocol transportProtocol) -> void
        {
            ksocket serverSocket = invalid_socket;
            u32 serverPort{};

            const bool isIPv4 = KalaServerCore::IsIPv4(KalaServerCore::GetIP());

            auto create_socket = [](
                const string targetIP,
                const TransportProtocol transportProtocol) -> ksocket
                {
                    int domain = KalaServerCore::IsIPv4(targetIP)
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

            auto close_socket = [thisServerDataID](ksocket& targetSocket) -> void
                {
                    scoped_lock lock(serverDataMutex);
                    for (pair<u32, ServerData>& ld : serverData)
                    {
                        if (ld.first == thisServerDataID)
                        {
                            ld.second.socket = invalid_socket;
                            break;
                        }
                    }

                    kclose(targetSocket);
                };

            string err = registry.LockRegistry([serverID, &serverPort]()
                {
                    for (const Server* s : registry.GetAllContent())
                    {
                        if (s->GetID() != serverID) continue;

                        serverPort = s->GetServerPort();
                        break;
                    } 
                });

            if (transportProtocol == TransportProtocol::TP_TCP)
            {
                sockaddr_in addr{};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(serverPort);
                addr.sin_addr.s_addr = htonl(INADDR_ANY);

                serverSocket = create_socket(KalaServerCore::GetIP(), transportProtocol);

                if (serverSocket == invalid_socket)
                {
#if defined(KWIN_ANY)
                    string err = KalaServerCore::ErrorToString(WSAGetLastError());
#else
                    string err = KalaServerCore::ErrorToString(errno);
#endif

                    //always force close
                    KalaServerCore::ForceClose(
                        "KalaServer Server error",
                        "Failed to start connection for " + string(serverStr) 
                        + " because socket creation failed! Reason: " + err);
                }

                if (bind(
                    serverSocket,
                    rcast<sockaddr*>(&addr),
                    sizeof(addr)) == -1)
                {
#if defined(KWIN_ANY)
                    string err = KalaServerCore::ErrorToString(WSAGetLastError());
#else
                    string err = KalaServerCore::ErrorToString(errno);
#endif

                    KalaServerCore::ForceClose(
                        "KalaServer Server error",
                        "Failed to bind socket for " + serverStr + "! Reason: " + err);
                }

                if (listen(serverSocket, SOMAXCONN) == -1)
                {
#if defined(KWIN_ANY)
                    string err = KalaServerCore::ErrorToString(WSAGetLastError());
#else
                    string err = KalaServerCore::ErrorToString(errno);
#endif

                    KalaServerCore::ForceClose(
                        "KalaServer Server error",
                        "Failed to start listening with socket for " + serverStr + "! Reason: " + err);
                }
            }

            {
                scoped_lock lock(serverDataMutex);
                for (pair<u32, ServerData>& sd : serverData)
                {
                    if (sd.first != thisServerDataID) continue;

                    sd.second.socket = serverSocket;

                    break;
                }
            }

            bool isAlive = true;

            while (isAlive)
            {
                fd_set readSet{};
                FD_ZERO(&readSet);
                FD_SET(serverSocket, &readSet);

                timeval tv{};
                tv.tv_sec = 0;
                tv.tv_usec = 500000;

#if defined(KWIN_ANY)
                int ready = select(
                    0,
                    &readSet,
                    nullptr,
                    nullptr,
                    &tv);
#else
                int ready = select(
                    serverSocket + 1,
                    &readSet,
                    nullptr,
                    nullptr,
                    &tv);
#endif

                if (ready == 0) continue;
                if (ready < 0)
                {
#if defined(KWIN_ANY)
                    string err = KalaServerCore::ErrorToString(WSAGetLastError());
#else
                    string err = KalaServerCore::ErrorToString(errno);
#endif

                    Log::Print(
                        "Failed to select socket for " + serverStr + "! Reason: " + err,
                        "KS_SERVER",
                        LogType::LOG_ERROR,
                        2);

                    break;
                }

                sockaddr_in clientAddr{};
                socklen_t clientAddrLen = sizeof(clientAddr);

                if (KalaServerCore::IsShuttingDown())
                {
                    Log::Print(
                        "Closing connection for " + serverStr 
                        + " because server has closed the connection.",
                        "KS_SERVER",
                        LogType::LOG_INFO);
                    
                    break;
                }

                ksocket clientSocket = accept(
                    serverSocket,
                    rcast<sockaddr*>(&clientAddr),
                    &clientAddrLen);

                if (clientSocket == invalid_socket)
                {
#if defined(KWIN_ANY)
                    int errc = WSAGetLastError();
                    string err = KalaServerCore::ErrorToString(errc);
#else
                    int errc = errno;
                    string err = KalaServerCore::ErrorToString(errc);
#endif

                    Log::Print(
                        "Failed to accept client socket! Reason: " + err,
                        "KS_SERVER",
                        LogType::LOG_ERROR,
                        2);

#if defined (KWIN_ANY)
                    if (errc == WSAEINVAL) break;
#else
                    if (errc == EINVAL) break;
#endif

                    continue;
                }

                char ipBuffer[INET_ADDRSTRLEN]{};

                if (!inet_ntop(
                    AF_INET,
                    &clientAddr.sin_addr,
                    ipBuffer,
                    sizeof(ipBuffer)))
                {
#if defined(KWIN_ANY)
                    string err = KalaServerCore::ErrorToString(WSAGetLastError());
#else
                    string err = KalaServerCore::ErrorToString(errno);
#endif

                    Log::Print(
                        "Failed to convert incoming client IP address! Reason: " + err,
                        "KS_SERVER",
                        LogType::LOG_ERROR,
                        2);

                    kclose(clientSocket);
                    continue;
                }

                u32 serverID{};
                {
                    shared_lock lock(serverDataMutex);
                    for (const pair<u32, ServerData>& sd : serverData)
                    {
                        if (sd.first != thisServerDataID) continue;
                        serverID = sd.second.serverID;
                        break;
                    }
                }

                if (serverID == 0)
                {
                    Log::Print(
                        "Failed to check IP '" + string(ipBuffer) + "' status because " + serverStr + " is invalid!",
                        "KS_SERVER",
                        LogType::LOG_ERROR,
                        2);

                    kclose(clientSocket);
                    break;
                }

                bool isBanned{};
                string err = registry.LockRegistry([
                    serverID,
                    ipBuffer,
                    &isBanned]()
                    {
                        Server* server{};
                        for (Server* s : registry.GetAllContent())
                        {
                            if (s->GetID() == serverID)
                            {
                                server = s;
                                break;
                            }
                        }

                        if (!server)
                        {
                            KalaServerCore::ForceClose(
                                "KalaServer Server error",
                                "Failed to check IP '" + string(ipBuffer) + "' ban status because server '" 
                                + to_string(serverID) + "' was not found!");
                        }

                        isBanned = ContainsValue(server->GetBannedIPs(), ipBuffer);
                    });

                if (!err.empty())
                {
                    KalaServerCore::ForceClose(
                        "KalaServer Server error",
                        "Failed to check IP '" + string(ipBuffer) + "' ban status! Reason: " + err);
                }

                if (isBanned)
                {
                    Log::Print(
                        "Banned IP '" + string(ipBuffer) + "' tried to reconnect to " + serverStr + ".",
                        "KS_SERVER",
                        LogType::LOG_WARNING);

                    kclose(clientSocket);
                    continue;
                }

                u32 nextServerDataID = serverDataID.fetch_add(1) + 1;
                {
                    sockaddr_storage storage{};
                    memcpy(&storage, &clientAddr, sizeof(clientAddr));

                    scoped_lock lock(serverDataMutex);
                    serverData.push_back(
                        {
                            nextServerDataID,
                            ServerData{
                                .serverID = serverID,
                                .clientIP = ipBuffer,
                                .clientPort = ntohs(clientAddr.sin_port),
                                .timeout = 5,
                                .transportProtocol = TransportProtocol::TP_TCP,
                                .packetConnectionMode = PacketConnectionMode::PCM_KeepAlive,
                                .responseType = ResponseType::RT_AcceptResponse,
                                .socket = clientSocket,
                                .addr = storage,
                                .addrLen = sizeof(clientAddr)
                            }
                        });
                }

                _StartClient(
                    thisServerDataID,
                    nextServerDataID,
                    messageCallback);
            }

            close_socket(serverSocket);
        };

    {
        scoped_lock lock(serverDataMutex);

        for (pair<u32, ServerData>& sd : serverData)
        {
            if (sd.first != thisServerDataID) continue;

            ServerData& data = sd.second;

            Server* server{};
            string err = Server::GetRegistry().GetContent(data.serverID, server);
            if (!err.empty())
            {
                KalaServerCore::ForceClose(
                    "KalaServer Server error",
                    "Failed to start listener because the server was invalid! Reason: " + err);
            }

            const string serverStr = 
                "server '" + to_string(data.serverID) 
                + "' at port '" + to_string(server->GetServerPort()) + "'";

            data.thisThread = thread(
                std::move(worker_thread),
                serverStr,
                data.serverID,
                data.timeout,
                data.transportProtocol);

            break;
        }
    }
}

void _StartClient(
    const u32 originServerDataID,
    const u32 clientDataID,
    const function<vector<u8>(string, u16, vector<u8>)>& messageCallback)
{
    auto worker_thread = [
        originServerDataID,
        clientDataID,
        messageCallback](
        const string serverStr,
        const string clientStr,
        const u32 serverID,
        const string clientIP,
        const u16 clientPort,
        const u8 timeout,
        const TransportProtocol transportProtocol,
        ksocket clientSocket,
        sockaddr_storage addr,
        i32 addrLen) -> void
        {
            const bool isIPv4 = KalaServerCore::IsIPv4(clientIP);

            auto close_socket = [clientDataID](ksocket& targetSocket) -> void
                {
                    scoped_lock lock(serverDataMutex);
                    for (pair<u32, ServerData>& ld : serverData)
                    {
                        if (ld.first == clientDataID)
                        {
                            ld.second.socket = invalid_socket;
                            break;
                        }
                    }

                    kclose(targetSocket);
                };

            auto send_response = [
                isIPv4,
                serverStr,
                clientStr,
                addr,
                addrLen,
                transportProtocol](
                ksocket& targetSocket,
                const vector<u8>& payload) -> bool
                {
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
                                    "Cannot send response to client! "
                                    "Closing connection between " + serverStr + " and " + clientStr 
                                    + " because TCP socket transmission returned zero bytes!",
                                    "KS_SERVER",
                                    LogType::LOG_ERROR,
                                    2);

                                return false;
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
                                timeoutVal.tv_usec = 500000;

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
                                        "Cannot send response to client! "
                                        "Closing connection between " + serverStr + " and " + clientStr 
                                        + " because TCP socket transmission timed out or select failed!",
                                        "KS_SERVER",
                                        LogType::LOG_ERROR,
                                        2);

                                    return false;
                                }
                            }
                            else
                            {
                                Log::Print(
                                    "Cannot send response to client! "
                                    "Closing connection between " + serverStr + " and " + clientStr 
                                    + " because TCP socket transmission failed!",
                                    "KS_SERVER",
                                    LogType::LOG_ERROR,
                                    2);

                                return false;
                            }
                        }
                    }
                    else
                    {

                    }

                    return true;
                };

            bool isAlive = true;

            while (isAlive)
            {
                auto is_server_alive = [originServerDataID]() -> bool
                    {
                        shared_lock lock(serverDataMutex);
                        for (const pair<u32, ServerData>& sd : serverData)
                        {
                            if (sd.first != originServerDataID) continue;

                            return sd.second.socket != invalid_socket;
                        }

                        return false;
                    };

                auto is_client_banned = [
                    serverStr,
                    originServerDataID,
                    clientIP](bool& serverClosed) -> bool
                    {
                        bool isBanned{};

                        u32 serverID{};
                        {
                            shared_lock lock(serverDataMutex);
                            for (const pair<u32, ServerData>& sd : serverData)
                            {
                                if (sd.first != originServerDataID) continue;
                                serverID = sd.second.serverID;
                                break;
                            }
                        }

                        if (serverID == 0)
                        {
                            Log::Print(
                                "Failed to check IP '" + clientIP + "' status because " + serverStr + " is invalid!",
                                "KS_SERVER",
                                LogType::LOG_ERROR,
                                2);

                            serverClosed = true;
                            return false;
                        }

                        string err = registry.LockRegistry([
                            serverID,
                            clientIP,
                            &isBanned]()
                            {
                                Server* server{};
                                for (Server* s : registry.GetAllContent())
                                {
                                    if (s->GetID() == serverID)
                                    {
                                        server = s;
                                        break;
                                    }
                                }

                                if (!server)
                                {
                                    KalaServerCore::ForceClose(
                                        "KalaServer Server error",
                                        "Failed to check IP '" + clientIP + "' ban status because server '" 
                                        + to_string(serverID) + "' was not found!");
                                }

                                isBanned = ContainsValue(server->GetBannedIPs(), clientIP);
                            });

                        if (!err.empty())
                        {
                            KalaServerCore::ForceClose(
                                "KalaServer Server error",
                                "Failed to check IP '" + clientIP + "' ban status! Reason: " + err);
                        }

                        return isBanned;
                    };

                if (!is_server_alive())
                {
                    Log::Print(
                        "Closing connection between " + serverStr + " and " + clientStr 
                        + " because server has closed the connection.",
                        "KS_SERVER",
                        LogType::LOG_INFO);

                    close_socket(clientSocket);
                    return;
                }

                bool serverClosed{};
                if (is_client_banned(serverClosed))
                {
                    Log::Print(
                        "Closing connection between " + serverStr + " and " + clientStr 
                        + " because client was banned.",
                        "KS_SERVER",
                        LogType::LOG_WARNING);

                    close_socket(clientSocket);
                    return;
                }

                if (serverClosed)
                {
                    close_socket(clientSocket);
                    return;
                }

                char buffer[4096]{};

                int received = recv(
                    clientSocket,
                    buffer,
                    sizeof(buffer),
                    0);

                if (received > 0)
                {
                    vector<u8> response{};
                    if (messageCallback) response = messageCallback(
                        clientIP,
                        clientPort,
                        vector<u8>(buffer, buffer + received));

                    bool serverClosed = true;

                    if (!response.empty())
                    {
                        {
                            shared_lock lock(serverDataMutex);
                            for (pair<u32, ServerData>& sd : serverData)
                            {
                                if (sd.first != originServerDataID) continue;

                                serverClosed = (sd.second.socket == invalid_socket);

                                break;
                            }
                        }

                        if (!serverClosed
                            && !send_response(
                                clientSocket,
                                response))
                        {
                            Log::Print(
                                "Cannot send response to client! "
                                "Closing connection between " + serverStr + " and " + clientStr 
                                + " because response send failed.",
                                "KS_SERVER",
                                LogType::LOG_INFO);

                            close_socket(clientSocket);
                            return;
                        }
                        
                        if (serverClosed)
                        {
                            Log::Print(
                                "Cannot send response to client! "
                                "Closing connection between " + serverStr + " and " + clientStr 
                                + " because server has closed the connection.",
                                "KS_SERVER",
                                LogType::LOG_INFO);

                            close_socket(clientSocket);
                            return;
                        }
                    }
                }
                else if (received == 0)
                {
                    Log::Print(
                        "Closing connection between " + serverStr + " and " + clientStr 
                        + " because client has closed the connection.",
                        "KS_SERVER",
                        LogType::LOG_INFO);

                    close_socket(clientSocket);
                    return;
                }
                else
                {
                    Log::Print(
                        "Closing connection between " + serverStr + " and " + clientStr 
                        + " because receiving data failed!",
                        "KS_SERVER",
                        LogType::LOG_ERROR,
                        2);

                    close_socket(clientSocket);
                    return;
                }
            }
        };

    {
        scoped_lock lock(serverDataMutex);

        for (pair<u32, ServerData>& sd : serverData)
        {
            if (sd.first != clientDataID) continue;

            ServerData& data = sd.second;

            Server* server{};
            string err = Server::GetRegistry().GetContent(data.serverID, server);
            if (!err.empty())
            {
                KalaServerCore::ForceClose(
                    "KalaServer Server error",
                    "Failed to start listener because the server was invalid! Reason: " + err);
            }

            const string serverStr = 
                "server '" + to_string(data.serverID) 
                + "' at port '" + to_string(server->GetServerPort()) + "'";

            const string clientStr = 
                "client '" + data.clientIP + "' at port '" 
                + to_string(data.clientPort) + "'";

            data.thisThread = thread(
                std::move(worker_thread),
                serverStr,
                clientStr,
                data.serverID,
                data.clientIP,
                data.clientPort,
                data.timeout,
                data.transportProtocol,
                data.socket,
                data.addr,
                data.addrLen);

            break;
        }
    }
}