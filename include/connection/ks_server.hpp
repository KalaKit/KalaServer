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

namespace KalaServer::Connection
{
    using KalaServer::Core::KalaServerRegistry;

    using std::string;
    using std::string_view;
    using std::vector;
    using std::filesystem::path;
    using std::default_delete;

    static constexpr u8 MIN_SERVER_NAME_LENGTH = 3;
    static constexpr u8 MAX_SERVER_NAME_LENGTH = 10;

	//Minimum port, 1-1023 requires admin/root
	static constexpr u16 MIN_PORT_RANGE = 1;
	//Maximum port, cannot go past 16-bit unsigned integer TCP and UDP port fields
	static constexpr u16 MAX_PORT_RANGE = 65535;

    class LIB_API Server
    {
    friend class Client;
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

        const vector<u32>& GetClientIDs() const;
        const vector<u32>& GetPacketIDs() const;

        bool IsServerVisible() const;
        void SetServerVisibility(bool newValue);

        const string& GetServerName() const;
        void SetServerName(string_view newValue);

        u16 GetServerPort() const;

        const path& GetServerRoot() const;

        const vector<path>& GetWhitelistedRoutes() const;
        void AddWhitelistedRoute(const path& targetPath);
        void RemoveWhitelistedRoute(const path& targetPath);

        void ImportWhitelistedRoutes(const path& targetPath);
        void ExportWhitelistedRoutes(const path& targetPath);

        const vector<string>& GetBannedIPs() const;
        void BanIP(string_view IP);
        void UnbanIP(string_view IP);

        void ImportBannedIPs(const path& targetPath);
        void ExportBannedIPs(const path& targetPath);

        const vector<string>& GetBlacklistedKeywords() const;
        void AddBlacklistedKeyword(string_view newValue);
        void RemoveBlacklistedKeyword(string_view newValue);

        void ImportBlacklistedKeywords(const path& targetPath);
        void ExportBlacklistedKeywords(const path& targetPath);

        //Disconnects clients from this server before destroying this server
        void Destroy();
    private:
        ~Server();

        void RecreateWriterSocket(bool tcpSocket);

        u32 ID{};
        
        vector<u32> clientIDs{};
        vector<u32> packetIDs{};

        bool isVisible{};

        string serverName{};
        u16 serverPort{};

        path serverRoot{};
        vector<path> whitelistedRoutes{};

        vector<string> bannedIPs{};
        vector<string> blacklistedKeywords{};

        uintptr_t tcpSocket{};
        uintptr_t udpSocket{};
    };
}