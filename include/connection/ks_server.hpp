//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

#include "core_utils.hpp"

#include "core/ks_registry.hpp"
#include "connection/ks_link.hpp"

namespace KalaServer::Core
{
    class KalaServerCore;
}

namespace KalaServer::Connection
{
    using KalaServer::Core::KalaServerRegistry;
    using KalaServer::Connection::TransportProtocol;
    using KalaServer::Connection::PacketConnectionMode;
    using KalaServer::Connection::ResponseType;

    using std::string;
    using std::string_view;
    using std::vector;
    using std::filesystem::path;
    using std::shared_mutex;
    using std::default_delete;

    static constexpr u8 MIN_SERVER_NAME_LENGTH = 3;
    static constexpr u8 MAX_SERVER_NAME_LENGTH = 20;

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
    friend class KalaServer::Core::KalaServerCore;
    friend class Link;
    friend struct default_delete<Server>;
    public:
        KNODISCARD
        static KalaServerRegistry<Server>& GetRegistry();

        //Create a new server for accepting incoming UDP and TCP connections,
        //set messageCallback to decide how to parse a client-sent message,
        //set messageCallback return value to decide how to respond to a client-sent message
        static Server* Initialize(
            string_view serverName,
            const path& serverRoot,
            u16 serverPort,
            const function<vector<u8>(string, u16, vector<u8>)>& messageCallback = {});

        u32 GetID() const;

        bool IsVisible() const;
        void SetVisibility(bool newValue);

        string GetServerName() const;
        u16 GetServerPort() const;

        path GetServerRoot() const;

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

        mutable shared_mutex serverMutex{};

        u32 ID{};

        bool isVisible{};

        string serverName{};
        u16 serverPort{};

        path serverRoot{};
        vector<path> whitelistedRoutes{};

        vector<string> bannedIPs{};
        vector<string> blacklistedKeywords{};

        function<void(string, u16, vector<u8>)> messageCallback{};
    };
}