//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <functional>
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
    using std::function;
    using std::default_delete;

    enum class SenderType : u8
    {
        S_SERVER = 0,
        S_CLIENT = 1
    };

    enum class TransportProtocol : u8
    {
        TP_TCP = 0,
        TP_UDP = 1
    };

    enum class PacketConnectionMode : u8
    {
        //The link between sender and receiver stays alive after sending this packet
        PCM_KeepConnectionAlive = 0,
        //The link between sender and receiver gets destroyed
        //after sending this packet, cannot be used by server
        PCM_DisconnectConnectionAfterSend = 1
    };

    enum class ResponseType : u8
    {
        //The packet ignores whatever response it gets
        RT_IgnoreResponse = 0,
        //The packet waits for a response and calls the response callback,
        //response callback must be assigned to use this option
        RT_AcceptResponse = 1
    };

    class LIB_API Link
    {
    friend class KalaServer::Core::KalaServerCore;
    friend class Server;
    friend class Client;
    friend struct default_delete<Link>;
    public:
        KNODISCARD
        static KalaServerRegistry<Link>& GetRegistry();

        //Create a single packet that dies off once it is finished,
        //timeout determines how long the packet waits in seconds before bailing out on bad connection,
        //response also waits for identical period before cancelling
        static void SendPacket(
            u32 senderID,
            SenderType senderType,
            string_view receiverIP,
            u16 receiverPort,
            u8 timeout,
            TransportProtocol transportProtocol,
            ResponseType responseType,
            const vector<u8>& payload,
            function<void(vector<u8>)> responseCallback);

        static Link* Initialize(
            u32 senderID,
            SenderType senderType,
            string_view receiverIP,
            u16 receiverPort,
            TransportProtocol transportProtocol);

        u32 GetID() const;

        u32 GetSenderID() const;
        SenderType GetSenderType() const;

        const string& GetReceiverIP() const;
        u16 GetReceiverPort() const;

        u8 GetTimeout() const;
        void SetTimeout(u8 newValue);

        TransportProtocol GetTransportProtocol() const;

        //Create a new packet on an existing link
        void SendPacket(
            PacketConnectionMode packetConnectionMode,
            ResponseType responseType,
            const vector<u8>& payload,
            function<void(vector<u8>)> responseCallback);

        //Close the link
        void Destroy();
    private:
        ~Link();

        void Update();

        u32 ID{};
        u32 senderID{};
        SenderType senderType{};

        string receiverIP{};
        u16 receiverPort{};

        u8 timeout = 2;

        TransportProtocol transportProtocol{};

        uintptr_t tcpSocket{};
        uintptr_t udpSocket{};
    };
}