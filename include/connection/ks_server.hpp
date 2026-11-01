//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <memory>

#include "core_utils.hpp"

#include "core/ks_registry.hpp"

namespace KalaServer::Core
{
    class KalaServerCore;
}

namespace KalaServer::Connection
{
    using KalaServer::Core::KalaServerRegistry;

    using std::string;
    using std::string_view;
    using std::vector;
    using std::filesystem::path;
    using std::atomic;
    using std::mutex;
    using std::shared_mutex;
    using std::default_delete;

    static constexpr u8 MIN_SERVER_NAME_LENGTH = 3;
    static constexpr u8 MAX_SERVER_NAME_LENGTH = 10;

	//Minimum port, 1-1023 requires admin/root
	static constexpr u16 MIN_PORT_RANGE = 1;
	//Maximum port, cannot go past 16-bit unsigned integer TCP and UDP port fields
	static constexpr u16 MAX_PORT_RANGE = 65535;

    //Max amount of clients across all servers,
    //adjust server-local maxClients parameter to change how many clients
    //can connect to an individual server
    static constexpr u32 MAX_CLIENTS = 10000;

    //Max amount of packets for each client per second across all servers,
    //adjust server-local maxClientPackets parameter to change how many packets each client
    //can send to this server across both the udp and tcp listener ports
    static constexpr u32 MAX_PACKETS_PER_CLIENT_S = 1000;

    class LIB_API Server
    {
    friend void _TCPThread(Server* server);
    friend void _UDPThread(Server* server);
    friend class KalaServer::Core::KalaServerCore;
    friend class Link;
    friend struct default_delete<Server>;
    public:
        KNODISCARD
        static KalaServerRegistry<Server>& GetRegistry();

        //Create a new server for accepting incoming UDP and TCP connections,
        //isWebServer allows extra features
        static Server* Initialize(
            string_view serverName,
            const path& serverRoot,
            u16 serverPort);

        u32 GetID() const;

        vector<u32> GetLinkIDs();

        bool IsVisible() const;
        void SetVisibility(bool newValue);

        const string& GetServerName() const;
        u16 GetServerPort() const;

        const path& GetServerRoot() const;

        vector<path> GetWhitelistedRoutes();
        void AddWhitelistedRoute(const path& targetPath);
        void RemoveWhitelistedRoute(const path& targetPath);

        void ImportWhitelistedRoutes(const path& targetPath);
        void ExportWhitelistedRoutes(const path& targetPath);

        vector<string> GetBannedIPs();
        void BanIP(string_view IP);
        void UnbanIP(string_view IP);

        void ImportBannedIPs(const path& targetPath);
        void ExportBannedIPs(const path& targetPath);

        vector<string> GetBlacklistedKeywords();
        void AddBlacklistedKeyword(string_view newValue);
        void RemoveBlacklistedKeyword(string_view newValue);

        void ImportBlacklistedKeywords(const path& targetPath);
        void ExportBlacklistedKeywords(const path& targetPath);

        //Disconnects clients from this server before destroying this server
        void Destroy();
    private:
        ~Server();

        static void Shutdown();

        u32 ID{};
        
        mutex linkIDsMutex{};
        vector<u32> linkIDs{};

        shared_mutex activeSocketsMutex{};
        vector<uintptr_t> activeSockets{};

        atomic<bool> isVisible{};

        string serverName{};
        u16 serverPort{};

        path serverRoot{};
        mutex whitelistedRoutesMutex{};
        vector<path> whitelistedRoutes{};

        mutex bannedIPsMutex{};
        vector<string> bannedIPs{};
        mutex blacklistedKeywordsMutex{};
        vector<string> blacklistedKeywords{};
    };
}