//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <vector>

#include "log_utils.hpp"

#include "core/ks_core.hpp"
#include "connection/ks_client.hpp"
#include "connection/ks_link.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

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

static vector<u8> GeneratePayloadFromString(string_view input)
{
    vector<u8> payload(input.begin(), input.end());
    return payload;
}

static void PrintResponse(
    string_view targetIP,
    u16 targetPort,
    const vector<u8>& payload)
{
    string targetPayload(payload.begin(), payload.end());

    Log::Print(
        "\ntarget IP: " + string(targetIP) + "\n"
        "target port: " + to_string(targetPort) + "\n"
        "target payload:" + targetPayload,
        "server_test",
        LogType::LOG_INFO);
}

int main()
{
    KalaServerCore::Initialize("192.168.1.1");
    Client* client = Client::Initialize("user");

    string targetIP = "45.79.112.203";
    u16 targetPort = 4242;

    Link::SendPacket(
        client->GetID(),
        SenderType::S_CLIENT,
        targetIP,
        targetPort,
        10,
        TransportProtocol::TP_TCP,
        ResponseType::RT_AcceptResponse,
        GeneratePayloadFromString("test\n"),
        [&targetIP, &targetPort](vector<u8> payload) { PrintResponse(targetIP, targetPort, payload); });

    KalaServerCore::Shutdown();

    return 0;
}