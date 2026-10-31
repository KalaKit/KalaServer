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

#include "log_utils.hpp"
#include "file_utils.hpp"
#include "string_utils.hpp"

#include "core/ks_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaFile::GetCleanPath;

using KalaHeaders::KalaString::IsStringInRange;

using KalaServer::Core::KalaServerCore;
using KalaServer::Core::KalaServerRegistry;
using KalaServer::Connection::Server;

using std::to_string;
using std::filesystem::exists;
using std::filesystem::is_directory;
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

static KalaServerRegistry<Server> registry{};

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
                "Failed to initialize server because KalaServer was not initialized!",
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

    const vector<u32>& Server::GetClientIDs() const { return clientIDs; }
    const vector<u32>& Server::GetLinkIDs() const { return linkIDs; }

    bool Server::IsVisible() const { return isVisible; }
    void Server::SetVisibility(bool newValue)
    {
        if (newValue
            && isVisible)
        {
            Log::Print(
                "Failed to set server '" + serverName + "' visible because it already was visible!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return;
        }
        if (!newValue
            && !isVisible)
        {
            Log::Print(
                "Failed to set server '" + serverName + "' invisible because it already was invisible!",
                "KS_SERVER",
                LogType::LOG_WARNING);

            return;
        }

        isVisible = newValue;
    }

    const string& Server::GetServerName() const { return serverName; }
    void Server::SetServerName(string_view newValue)
    {

    }

    u16 Server::GetServerPort() const { return serverPort; }

    const path& Server::GetServerRoot() const { return serverRoot; }

    const vector<path>& Server::GetWhitelistedRoutes() const { return whitelistedRoutes; }
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

    const vector<string>& Server::GetBannedIPs() const { return bannedIPs; }
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

    const vector<string>& Server::GetBlacklistedKeywords() const { return blacklistedKeywords; }
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

    void Server::Destroy()
    {
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