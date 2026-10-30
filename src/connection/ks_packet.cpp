//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "connection/ks_packet.hpp"

#if defined(KWIN_ANY)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

using KalaServer::Connection::Packet;
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

static KalaServerRegistry<Packet> registry{};

namespace KalaServer::Connection
{
    KalaServerRegistry<Packet>& Packet::GetRegistry() { return registry; }

    Packet* Packet::Initialize(
        u32 senderID,
        SenderType senderType,
        string receiverIP,
        u16 receiverPort,
        TransportProtocol transportProtocol,
        PacketConnectionMode pcMode,
        vector<u8> payload,
        ResponseType responseType,
        vector<u8>& response)
    {
        return nullptr;
    }

    u32 Packet::GetID() const { return ID; }
    u32 Packet::GetSenderID() const { return senderID; }

    SenderType Packet::GetSenderType() const { return senderType; }

    const string& Packet::GetReceiverIP() const { return receiverIP; }
    u16 Packet::GetReceiverPort() const { return receiverPort; }

    TransportProtocol Packet::GetTransportProtocol() const { return transportProtocol; }
    PacketConnectionMode Packet::GetPacketConnectionMode() const { return pcMode; }

    const vector<u8>& Packet::GetPayload() const { return payload; }

    ResponseType Packet::GetResponseType() const { return responseType; }

    void Packet::Destroy()
    {

    }

    Packet::~Packet()
    {
        
    }
}