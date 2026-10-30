//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "connection/ks_client.hpp"

#if defined(KWIN_ANY)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

using KalaServer::Connection::Client;
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

static KalaServerRegistry<Client> registry{};

namespace KalaServer::Connection
{
    KalaServerRegistry<Client>& Client::GetRegistry() { return registry; }

    Client* Client::Initialize(string_view username)
    {
        return nullptr;
    }

    u32 Client::GetID() const { return ID; }
    u32 Client::GetServerID() const { return serverID; }

    void Client::ConnectToServer(u32 serverID)
    {

    }
    void Client::ConnectToServer(u16 serverPort)
    {

    }
    void Client::DisconnectFromServer()
    {

    }

    const string& Client::GetUsername() const { return username; }
    void Client::SetUsername(string_view newValue)
    {

    }

    void Client::Destroy()
    {

    }
    
    Client::~Client()
    {

    }
}