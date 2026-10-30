//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "connection/ks_server.hpp"

#if defined(KWIN_ANY)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

using KalaServer::Connection::Server;
using KalaServer::Core::KalaServerRegistry;

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
        bool isWebServer, 
        string_view serverName, 
        const path &serverRoot, 
        u16 serverPort)
    {
        return nullptr;
    }

    u32 Server::GetID() const { return ID; }

    const vector<u32>& Server::GetClientIDs() const { return clientIDs; }
    const vector<u32>& Server::GetPacketIDs() const { return packetIDs; }

    bool Server::IsWebServer() const { return isWebServer; }

    bool Server::CanReceiveClients() const { return isVisible; }
    void Server::SetServerVisibility(bool newValue)
    {

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

    }

    Server::~Server()
    {

    }
}