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
        //set messageCallback return value to decide how to respond to a client-sent message,
        //set bannedIPResponse to decide what to respond when a client gets banned or when a banned client tries to reconnect
        static Server* Initialize(
            string_view serverName,
            u16 serverPort,
            const function<vector<u8>(string, u16, vector<u8>)>& messageCallback = {},
            const vector<u8>& bannedIPResponse = {});

        u32 GetID() const;

        bool IsVisible() const;
        void SetVisibility(bool newValue);

        string GetServerName() const;
        u16 GetServerPort() const;

        //All IPs that are banned, they will never be able to
        //connect to the server unless removed from the banned IPs list
        vector<string> GetBannedIPs();
        void BanIP(string_view IP);
        void UnbanIP(string_view IP);

        //Expects a text file which lists each IP in a new line with no commas, spaces, tabs or other symbols
        void ImportBannedIPs(const path& targetPath);
        void ExportBannedIPs(const path& targetPath);

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

        vector<string> bannedIPs{};

        function<void(string, u16, vector<u8>)> messageCallback{};
    };
}