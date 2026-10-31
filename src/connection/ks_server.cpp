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

using std::to_string;
using std::vector;
using std::filesystem::exists;
using std::filesystem::is_directory;
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
    u32 serverID{};
    thread thisThread{};
};

static KalaServerRegistry<Server> registry{};

static atomic<bool> isShuttingDown{};

static mutex threadDataMutex{};
static vector<ThreadData> threadData{};

static void _TCPThread(Server* server);
static void _UDPThread(Server* server);

namespace KalaServer::Connection
{
    KalaServerRegistry<Server>& Server::GetRegistry() { return registry; }

    Server* Server::Initialize(
        string_view serverName, 
        const path& serverRoot, 
        u16 serverPort)
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to initialize server because KalaServer has not been initialized!",
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

        for (const Server* s : registry.GetAllContent())
        {
            if (s->serverName == serverName)
            {
                Log::Print(
                    "Failed to initialize server because the server name '" + string(serverName) + "' is already in use!",
                    "KS_SERVER",
                    LogType::LOG_WARNING);

                return nullptr;
            }
        }

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
        serverPtr->serverPort = serverPort;
        serverPtr->serverRoot = serverRoot;

		string err = registry.AddContent(
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

		return serverPtr;
    }

    u32 Server::GetID() const { return ID; }

    vector<u32> Server::GetLinkIDs()
    {
        scoped_lock lock(linkIDsMutex);
        return linkIDs;
    }

    bool Server::IsVisible() const { return isVisible.load(); }
    void Server::SetVisibility(bool newValue)
    {
        if (newValue
            && isVisible.load())
        {
            Log::Print(
                "Failed to set server '" + to_string(ID) + "' visible because it already was visible!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return;
        }
        if (!newValue
            && !isVisible.load())
        {
            Log::Print(
                "Failed to set server '" + to_string(ID) + "' invisible because it already was invisible!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return;
        }

        isVisible.store(newValue);

        Log::Print(
            "Changed server '" + to_string(ID) + "' visibility to '" + (isVisible.load() ? "on" : "off") + "'.",
            "KS_SERVER",
            LogType::LOG_SUCCESS);
    }

    const string& Server::GetServerName() const { return serverName; }

    u16 Server::GetServerPort() const { return serverPort; }

    const path& Server::GetServerRoot() const { return serverRoot; }

    vector<path> Server::GetWhitelistedRoutes()
    { 
        scoped_lock lock(whitelistedRoutesMutex);
        return whitelistedRoutes;
    }
    void Server::AddWhitelistedRoute(const path& targetPath)
    {

    }
    void Server::RemoveWhitelistedRoute(const path& targetPath)
    {

    }

    void Server::ImportWhitelistedRoutes(const path& targetPath)
    {

    }
    void Server::ExportWhitelistedRoutes(const path& targetPath)
    {

    }

    vector<string> Server::GetBannedIPs()
    {
        scoped_lock lock(bannedIPsMutex);
        return bannedIPs;
    }
    void Server::BanIP(string_view IP)
    {

    }
    void Server::UnbanIP(string_view IP)
    {

    }

    void Server::ImportBannedIPs(const path& targetPath)
    {

    }
    void Server::ExportBannedIPs(const path& targetPath)
    {

    }

    vector<string> Server::GetBlacklistedKeywords()
    { 
        scoped_lock lock(blacklistedKeywordsMutex);
        return blacklistedKeywords;
    }
    void Server::AddBlacklistedKeyword(string_view newValue)
    {

    }
    void Server::RemoveBlacklistedKeyword(string_view newValue)
    {

    }

    void Server::ImportBlacklistedKeywords(const path& targetPath)
    {

    }
    void Server::ExportBlacklistedKeywords(const path& targetPath)
    {

    }

    void Server::CloseRemainingThreads()
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

    void Server::Destroy()
    {
        isVisible = false;

        vector<thread> targetThreads{};

        {
            scoped_lock lock(threadDataMutex);
            
            for (size_t i = threadData.size(); i > 0; --i)
            {
                if (threadData[i - 1].serverID != ID) continue;

                targetThreads.push_back(std::move(threadData[i - 1].thisThread));
                threadData.erase(threadData.begin() + (i - 1));
            }
        }

        for (thread& t : targetThreads)
        {
            if (t.joinable()) t.join();
        }

        for (const u32 lid : GetLinkIDs())
        {
            Link* l{};

            string err = Link::GetRegistry().GetContent(lid, l);
            if (!err.empty())
            {
                KalaServerCore::ForceClose(
                    "KalaServer Server error",
                    "Failed to destroy server '" + to_string(ID) 
                    + "' because its link '" + to_string(lid) + "' was invalid!");
            }

            l->Destroy();
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

void _TCPThread(Server* server)
{

}
void _UDPThread(Server* server)
{

}