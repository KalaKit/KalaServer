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

static vector<u8> HandleResponse(
    string targetIP,
    u16 targetPort,
    vector<u8> payload)
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

    if (!KalaServerCore::IsLikeHTTP(payload))
    {
        Log::Print(
            "Target '" + string(targetIP) + "' via port '" + to_string(targetPort) + "'"
            " did not send a valid payload because it did not resemble real HTTP!",
            "SERVER_TEST",
            LogType::LOG_WARNING);
    }

    Log::Print(
        "\nIP: " + string(targetIP) + "\n"
        "port: " + to_string(targetPort) + "\n"
        "payload:\n" + payloadString,
        "SERVER_TEST",
        LogType::LOG_INFO);

    const string responseBody = "this is a test, you connected successfully";

    const string response = 
        "HTTP/1.1 418 I'm a teapot\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: " + to_string(responseBody.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + responseBody;

    return vector<u8>(response.begin(), response.end());
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
        HandleResponse);

    server->SetVisibility(true);

    //press Enter to exit
    cin.get();
    
    KalaServerCore::Shutdown();

    return 0;
}