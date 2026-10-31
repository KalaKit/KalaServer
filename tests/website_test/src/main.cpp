//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <vector>

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

using std::string;
using std::string_view;
using std::to_string;
using std::vector;

struct TestData
{
    string targetIP{};
    u16 targetPort{};
    string request_string{};
    vector<u8> request_payload{};
};

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
            DecToHex(d, hex);
            payloadString += hex;
        }
    }

    Log::Print(
        "\ntarget IP: " + string(targetIP) + "\n"
        "target port: " + to_string(targetPort) + "\n"
        "target payload:\n" + payloadString,
        "server_test",
        LogType::LOG_INFO);
}

static void TCPTest(Client* client)
{
    //ask for wikipedias homepage
    TestData tcpData =
    {
        .targetIP = "208.80.154.224",
        .targetPort = 80,
        .request_string = 
            "GET / HTTP/1.1\r\n"
            "Host: wikipedia.com\r\n"
            "Connection: close\r\n"
            "\r\n"
    };

    Link::SendPacket(
        client->GetID(),
        SenderType::S_CLIENT,
        tcpData.targetIP,
        tcpData.targetPort,
        5,
        TransportProtocol::TP_TCP,
        ResponseType::RT_AcceptResponse,
        GeneratePayloadFromString(tcpData.request_string),
        [&tcpData](vector<u8> payload) 
        { 
            PrintResponse(
                tcpData.targetIP, 
                tcpData.targetPort, 
                false,
                payload); 
        });
}

static void UDPTest(Client* client)
{
    //ask for wikipedias ipv4 address from cloudflare
    TestData udpData =
    {
        .targetIP = "1.1.1.1",
        .targetPort = 53,
        .request_payload = 
            {
                0x12, 0x34, 0x01, 0x00,
                0x00, 0x01, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x09, 'w', 'i', 'k', 'i', 'p', 'e', 'd', 'i', 'a', 
                0x03, 'o', 'r', 'g', 
                0x00,
                0x00, 0x01,
                0x00, 0x01
            }
    };

    Link::SendPacket(
        client->GetID(),
        SenderType::S_CLIENT,
        udpData.targetIP,
        udpData.targetPort,
        5,
        TransportProtocol::TP_UDP,
        ResponseType::RT_AcceptResponse,
        udpData.request_payload,
        [&udpData](vector<u8> payload) 
        { 
            PrintResponse(
                udpData.targetIP, 
                udpData.targetPort, 
                true,
                payload); 
        });
}

int main()
{
    KalaServerCore::Initialize("192.168.1.1");
    Client* client = Client::Initialize("user");

    Log::Print("---------- start tcp ipv4 test ----------");
    TCPTest(client);
    Log::Print("---------- start udp ipv4 test ----------");
    UDPTest(client);
    Log::Print("---------- all tests complete ----------");

    KalaServerCore::Shutdown();

    return 0;
}