//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <iostream>
#include <string>
#include <vector>

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

using std::cin;
using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::array;

Server* server{};



static constexpr array<string_view, 13> blacklistedKeywords =
{
    "/.env",                   //exposed env vulnerability scanners
    "/env",                    //exposed env vulnerability scanners
    "/.aws",                   //exposed aws server scanners
    "php",                     //php vulnerability scanners
    "secret",                  //exposed secret scanners
    "/cgi",                    //old server scanners (like apache)
    "/.git",                   //exposed git scanners
    "admin",                   //scanners trying to find admin endpoints
    "/SDK/webLanguage",        //hikvision camera scanners
    "__proto__",               //nodejs vulnerability scanners
    "constructor:constructor", //nodejs vulnerability scanners
    "process.mainModule",      //nodejs vulnerability scanners
    "child_process"            //nodejs vulnerability scanners
};

static vector<u8> GetNormalPayload()
{
    static constexpr string_view responseBody = "this is a test, you connected successfully";

    static const string response = 
        "HTTP/1.1 418 I'm a teapot\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: " + to_string(responseBody.size()) + "\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        + string(responseBody);

    static const vector<u8> responsePayload(response.begin(), response.end());

    return responsePayload;
}

static vector<u8> GetBannedPayload()
{
    static constexpr string_view responseBody = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Server Login</title>
    <style>
        body {
            margin: 0;
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            background: #101014;
            font-family: Arial, sans-serif;
        }

        .login {
            display: inline-block;
            padding: 32px 90px;
            background: #ff2020;
            color: white;
            font-size: 48px;
            font-weight: bold;
            text-decoration: none;
            border: 4px solid #ff7777;
            border-radius: 18px;
            box-shadow: 0 0 30px red;
            animation: pulse 1s infinite alternate;
        }

        .login:hover {
            background: #ff5555;
            box-shadow: 0 0 65px red;
            transform: scale(1.08);
        }

        @keyframes pulse {
            from { box-shadow: 0 0 15px red; }
            to   { box-shadow: 0 0 65px red; }
        }
    </style>
</head>
<body>
    <a class="login"
    href="https://www.youtube.com/watch?v=dQw4w9WgXcQ">
        LOGIN
    </a>
</body>
</html>
)HTML";

    static const string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + to_string(responseBody.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n"
        + string(responseBody);

    static const vector<u8> responsePayload(response.begin(), response.end());

    return responsePayload;
}

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

    for (const string_view kw : blacklistedKeywords)
    {
        if (payloadString.find(kw) == string::npos) continue;

        Log::Print(
            "Target '" + string(targetIP) + "' via port '" + to_string(targetPort) + "'"
            " was banned because it used blacklisted keyword '" + string(kw) + "'!",
            "SERVER_TEST",
            LogType::LOG_WARNING);

        server->BanIP(targetIP);
        return {};
    }

    Log::Print(
        "\nIP: " + string(targetIP) + "\n"
        "port: " + to_string(targetPort) + "\n"
        "payload:\n" + payloadString,
        "SERVER_TEST",
        LogType::LOG_INFO);

    return GetNormalPayload();
}

int main()
{
    KalaServerCore::Initialize("192.168.1.102");

    server = Server::Initialize(
        "server_test",
        80,
        HandleResponse,
        GetBannedPayload());

    server->SetVisibility(true);

    //press Enter to exit
    cin.get();

    KalaServerCore::Shutdown();

    return 0;
}