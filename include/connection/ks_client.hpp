//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <memory>

#include "core_utils.hpp"

namespace KalaServer::Connection
{
    using std::string;
    using std::string_view;
    using std::default_delete;

    class LIB_API Client
    {
    friend struct default_delete<Client>;
    public:
        //Create a new client with a username for identification
        static Client* Initialize(string_view username);

        u32 GetID() const;

        u32 GetServerID() const;

        //Connect to local server by server ID
        void ConnectToServer(u32 serverID);
        //Connect to remote or local server by IP and port
        void ConnectToServer(
            string_view serverIP,
            u16 serverPort);
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

        string serverIP{};
        u16 serverPort{};

        string username{};
    };
}