//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "log_utils.hpp"
#include "string_utils.hpp"

#include "core/ks_core.hpp"
#include "connection/ks_server.hpp"
#include "connection/ks_client.hpp"
#include "connection/ks_link.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaString::DecToHex;

using KalaServer::Core::KalaServerCore;
using KalaServer::Connection::Server;
using KalaServer::Connection::Client;
using KalaServer::Connection::TransportProtocol;
using KalaServer::Connection::ResponseType;
using KalaServer::Connection::Link;

using std::cin;
using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::this_thread::sleep_for;
using std::chrono::seconds;

static void PrintResponse(
    string_view targetIP,
    u16 targetPort,
    const vector<u8>& payload)
{
    string payloadString{};
    if (KalaServerCore::IsTextPayload(payload))
    {
        payloadString = string(payload.begin(), payload.end());
    }
    else
    {
        for (const u8 d : payload)
        {
            string hex{};
            DecToHex(d, hex, true);
            payloadString += hex;
        }
    }

    Log::Print(
        "\ntarget IP: " + string(targetIP) + "\n"
        "target port: " + to_string(targetPort) + "\n"
        "target payload:\n" + payloadString,
        "CLIENT_TEST",
        LogType::LOG_INFO);
}

static void Clear()
{
#if defined(KWIN_ANY)
    system("cls");
#else
    system("clear");
#endif
}

int main()
{
    KalaServerCore::Initialize("192.168.1.102");

    Server* server = Server::Initialize(
        "server_test",
        ".",
        80,
        {});

    server->SetVisibility(true);

    sleep_for(seconds(5));

    KalaServerCore::Shutdown();

    return 0;
}