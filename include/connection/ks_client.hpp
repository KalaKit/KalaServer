//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
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
    using std::default_delete;

    static constexpr u8 MIN_CLIENT_NAME_LENGTH = 3;
    static constexpr u8 MAX_CLIENT_NAME_LENGTH = 10;

    class LIB_API Client
    {
    friend class KalaServer::Core::KalaServerCore;
    friend class Server;
    friend class Link;
    friend struct default_delete<Client>;
    public:
        KNODISCARD
        static KalaServerRegistry<Client>& GetRegistry();

        //Create a new client with a username for identification
        static Client* Initialize(string_view username);

        u32 GetID() const;
        u32 GetServerID() const;
        const vector<u32>& GetLinkIDs() const;

        bool IsVisible() const;
        void SetVisibility(bool newValue);

        //Connect to local server by server ID
        void ConnectToServer(u32 serverID);
        //Connect to remote or local server by port
        void ConnectToServer(u16 serverPort);
        //Disconnect from currently connected server
        void DisconnectFromServer();

        const string& GetUsername() const;
        void SetUsername(string_view newValue);

        //Disconnects the client from its target server if connected to one
        void Destroy();
    private:
        ~Client();

        u32 ID{};
        u32 serverID{};
        vector<u32> linkIDs{};

        bool isVisible{};

        u16 serverPort{};

        string username{};
    };
}