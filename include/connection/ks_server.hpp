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

    class LIB_API Server
    {
    friend struct default_delete<Server>;
    public:
        KNODISCARD
        static KalaServerRegistry<Server>& GetRegistry();

        static Server* Initialize(
            bool isWebServer,
            string_view serverName,
            const path& serverRoot,
            u16 serverPort);

        u32 GetID() const;

        const vector<u32>& GetClientIDs() const;
        const vector<u32>& GetPacketIDs() const;

        bool IsWebServer() const;

        bool CanReceiveClients() const;
        void SetServerVisibility(bool newValue);

        const string& GetServerName() const;
        void SetServerName(string_view newValue);

        u16 GetServerPort() const;

        const path& GetServerRoot() const;

        const vector<path>& GetWhitelistedRoutes() const;
        void WhitelistRoute(const path& targetPath);
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

        u32 ID{};
        
        vector<u32> clientIDs{};
        vector<u32> packetIDs{};

        bool isWebServer{};
        bool isVisible{};

        string serverName{};
        u16 serverPort{};

        path serverRoot{};
        vector<path> whitelistedRoutes{};

        vector<string> bannedIPs{};
        vector<string> blacklistedKeywords{};
    };
}