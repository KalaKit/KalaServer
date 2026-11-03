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
    thread thisThread{};

    vector<vector<u8>> responses{};
};

static KalaServerRegistry<Server> registry{};

static atomic<u32> serverDataID{};

static shared_mutex serverDataMutex{};
static vector<pair<u32, ServerData>> serverData{};

static void _StartConnection(
    const u32 serverDataID,
    const function<void(vector<u8>)>& messageCallback);

namespace KalaServer::Connection
{
    KalaServerRegistry<Server>& Server::GetRegistry() { return registry; }

    Server* Server::Initialize(
        string_view serverName, 
        const path& serverRoot, 
        u16 serverPort,
        const function<void(vector<u8>)>& messageCallback)
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
        if (!IsStringInRange(serverName, MIN_SERVER_NAME_LENGTH, MAX_SERVER_NAME_LENGTH))
        {
            Log::Print(
                "Failed to initialize server for IP '" 
                + string(KalaServerCore::GetIP()) + "' because the server name length was out of range!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }

        bool continueInit = true;
        string err = registry.LockRegistry([serverName, &continueInit]()
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

        path cleanPath = GetCleanPath(serverRoot);
        if (!exists(cleanPath))
        {
            Log::Print(
                "Failed to initialize server for IP '" 
                + string(KalaServerCore::GetIP()) + "' because the server root '" + serverRoot.string() + "' does not exist!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }
        if (!is_directory(cleanPath))
        {
            Log::Print(
                "Failed to initialize server for IP '" 
                + string(KalaServerCore::GetIP()) + "' because the server root '" + serverRoot.string() + "' is not a directory!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return nullptr;
        }

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
        serverPtr->serverRoot = serverRoot;
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

    path Server::GetServerRoot() const
    {
        shared_lock lock(serverMutex); 
        return serverRoot;
    }

    vector<path> Server::GetWhitelistedRoutes()
    { 
        scoped_lock lock(serverMutex);
        return whitelistedRoutes;
    }
    void Server::AddWhitelistedRoute(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }
    void Server::RemoveWhitelistedRoute(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }

    void Server::ImportWhitelistedRoutes(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }
    void Server::ExportWhitelistedRoutes(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }

    vector<string> Server::GetBannedIPs()
    {
        shared_lock lock(serverMutex);
        return bannedIPs;
    }
    void Server::BanIP(string_view IP)
    {
        scoped_lock lock(serverMutex);
    }
    void Server::UnbanIP(string_view IP)
    {
        scoped_lock lock(serverMutex);
    }

    void Server::ImportBannedIPs(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }
    void Server::ExportBannedIPs(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }

    vector<string> Server::GetBlacklistedKeywords()
    { 
        shared_lock lock(serverMutex);
        return blacklistedKeywords;
    }
    void Server::AddBlacklistedKeyword(string_view newValue)
    {
        scoped_lock lock(serverMutex);
    }
    void Server::RemoveBlacklistedKeyword(string_view newValue)
    {
        scoped_lock lock(serverMutex);
    }

    void Server::ImportBlacklistedKeywords(const path& targetPath)
    {
        scoped_lock lock(serverMutex);
    }
    void Server::ExportBlacklistedKeywords(const path& targetPath)
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
    const u32 serverDataID,
    const function<void(vector<u8>)>& messageCallback)
{
    auto worker_thread = [
        serverDataID,
        messageCallback](
        const string serverStr,
        const u32 serverID,
        const u8 timeout,
        const TransportProtocol transportProtocol) -> void
        {
            ksocket targetSocket = invalid_socket;
            u32 serverPort{};
            
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

                targetSocket = socket(
                    AF_INET,
                    SOCK_STREAM,
                    IPPROTO_TCP);

                if (targetSocket == invalid_socket)
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
                    targetSocket,
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

                if (listen(targetSocket, SOMAXCONN) == -1)
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

            bool isAlive = true;

            while (isAlive)
            {
                fd_set readSet{};
                FD_ZERO(&readSet);
                FD_SET(targetSocket, &readSet);

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
                    targetSocket + 1,
                    &readSet,
                    nullptr,
                    nullptr,
                    &tv);
#endif

                if (ready == 0) continue;
                if (ready < 0) break;

                sockaddr_in clientAddr{};
                socklen_t clientAddrLen = sizeof(clientAddr);

                ksocket clientSocket = accept(
                    targetSocket,
                    rcast<sockaddr*>(&clientAddr),
                    &clientAddrLen);

                if (clientSocket == invalid_socket) continue;

                char buffer[4096]{};

                int received = recv(
                    clientSocket,
                    buffer,
                    sizeof(buffer),
                    0);

                if (received > 0)
                {
                    Log::Print(
                        "Received payload: " +
                        string(buffer, scast<size_t>(received)),
                        "KS_SERVER",
                        LogType::LOG_INFO);
                }

                kclose(clientSocket);
            }

            kclose(targetSocket);
        };

    {
        scoped_lock lock(serverDataMutex);

        for (pair<u32, ServerData>& sd : serverData)
        {
            if (sd.first != serverDataID) continue;

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