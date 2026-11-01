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
#include "connection/ks_client.hpp"
#include "connection/ks_link.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaString::DecToHex;

using KalaServer::Core::KalaServerCore;
using KalaServer::Connection::Client;
using KalaServer::Connection::SenderType;
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

static vector<u8> GeneratePayloadFromString(string_view input)
{
    vector<u8> payload(input.begin(), input.end());
    return payload;
}

static void PrintResponse(
    string_view targetIP,
    u16 targetPort,
    bool decToHex,
    const vector<u8>& payload)
{
    string payloadString{};
    if (!decToHex) payloadString = string(payload.begin(), payload.end());
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

static void SendMessage(
    const string& targetIP,
    const u16 targetPort,
    const SenderType senderType,
    const TransportProtocol transportProtocol,
    const ResponseType responseType,
    const string& message_string = {},
    const vector<u8>& message_payload = {})
{
    vector<u8> targetPayload = !message_string.empty()
        ? GeneratePayloadFromString(message_string)
        : message_payload;

    Link::SendPacket(
        0,
        senderType,
        targetIP,
        targetPort,
        5,
        transportProtocol,
        responseType,
        targetPayload,
            [
            targetIP,
            targetPort,
            message_string](
            vector<u8> payload) 
            { 
                PrintResponse(
                    targetIP, 
                    targetPort, 
                    message_string.empty(),
                    payload); 
            });
}

static void Command_Help()
{
    string help = 
        "help - prints all commands\n"
        "exit - exits the client test program\n"
        "clear - clear console messages\n"

        "tcpmode - switches to tcp mode where you can provide ip address, port and payload\n"
        "return - goes back from tcpmode to regular mode\n"

        "test_python_server - attempts to connect to the local python server, run 'python test_server.py' before running this command\n"
        "test_wikipedia - sends a simple tcp homepage request to wikipedia\n"
        "test_cloudflare - sends a simple udp request for the wikipedia ipv4 address to cloudflare";

    Log::Print(help);
}

static void Test_Python_Server()
{
    //local python server test, run 'python test_server.py'
    //before running this program to verify this works,
    //simple hello round-trip test with tcp
    SendMessage(
        "127.0.0.1",
        8080,
        SenderType::S_CLIENT,
        TransportProtocol::TP_TCP,
        ResponseType::RT_AcceptResponse,
        "hello");
}

static void Test_Wikipedia()
{
    //attempt to reach wikipedia homepage with tcp,
    //returns response from wikipedia
    SendMessage(
        "185.15.59.226",
        80,
        SenderType::S_CLIENT,
        TransportProtocol::TP_TCP,
        ResponseType::RT_AcceptResponse,
        "GET / HTTP/1.1\r\n"
        "Host: wikipedia.com\r\n"
        "Connection: close\r\n"
        "\r\n");
}

static void Test_Cloudflare()
{
    //ask for wikipedias ipv4 address from cloudflare
    SendMessage(
        "1.1.1.1",
        53,
        SenderType::S_CLIENT,
        TransportProtocol::TP_UDP,
        ResponseType::RT_AcceptResponse,
        "",
        {
            0x12, 0x34, 0x01, 0x00,
            0x00, 0x01, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x09, 'w', 'i', 'k', 'i', 'p', 'e', 'd', 'i', 'a', 
            0x03, 'c', 'o', 'm', 
            0x00,
            0x00, 0x01,
            0x00, 0x01
        });
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
    Client::Initialize("user");
    Client::SetVisibility(true);

    Log::Print("Type a command, like 'help' to list all commands.");

    bool inTCPMode{};
    string ip{};
    u64 port{};
    string payload{};

    while (true)
    {
        string input{};
        if (!getline(cin, input)) break;

        if (input.empty()) continue;

        if (input == "help")
        {
            Command_Help();
            continue;
        }
        else if (input == "exit") break;
        else if (input == "clear")
        {
            Clear();
            continue;
        }

        if (inTCPMode)
        {
            if (input == "return")
            {
                ip.clear();
                port = 0;
                payload.clear();

                inTCPMode = false;

                Clear();
                Log::Print("Switched back to normal mode.");

                continue;
            }

            if (ip.empty())
            {
                ip = input;
                continue;
            }
            if (port == 0)
            {
                try
                {
                    port = stoul(input);
                }
                catch (...)
                {
                    Log::Print(
                        "Failed to cast port to u16 because it was not a valid number! Try again.",
                        "CLIENT_TEST",
                        LogType::LOG_ERROR,
                        2);

                    port = 0;

                    continue;
                }

                if (port < 1
                    || port > UINT16_MAX)
                {
                    Log::Print(
                        "Failed to cast port to u16 because it was out of range! Try again.",
                        "CLIENT_TEST",
                        LogType::LOG_WARNING);

                    port = 0;

                    continue;
                }

                continue;
            }
            if (payload.empty())
            {
                payload = input;

                SendMessage(
                    ip,
                    port,
                    SenderType::S_CLIENT,
                    TransportProtocol::TP_TCP,
                    ResponseType::RT_AcceptResponse,
                    payload);

                ip.clear();
                port = 0;
                payload.clear();

                continue;
            }
        }
        else
        {
            if (input == "test_python_server")
            {
                Test_Python_Server();
                continue;
            }
            else if (input == "test_wikipedia")
            {
                Test_Wikipedia();
                continue;
            }
            else if (input == "test_cloudflare")
            {
                Test_Cloudflare();
                continue;
            }
            else if (input == "tcpmode")
            {
                inTCPMode = true;

                Clear();
                Log::Print("Switched to TCP mode. Type IP first, then port and then payload to reach a server in tcp mode.");

                continue;
            }
        }

        Log::Print("Command '" + input + "' not found! Type 'help' to list all commands...");
    }

    KalaServerCore::Shutdown();

    return 0;
}