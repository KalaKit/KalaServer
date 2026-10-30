//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "core_utils.hpp"
#include "core/ks_registry.hpp"

namespace KalaServer::Connection
{
    using KalaServer::Core::KalaServerRegistry;

    using std::string;
    using std::vector;
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
        PCM_KeepClientAlive = 0,
        PCM_DisconnectClientAfterSend = 1
    };

    enum class ResponseType : u8
    {
        RT_None = 0,
        RT_WaitForResponse = 1
    };
    
    class LIB_API Packet
    {
    friend struct default_delete<Packet>;
    public:
        KNODISCARD
        static KalaServerRegistry<Packet>& GetRegistry();

        //A packet dies after it was sent or after its response was received,
        //it cannot be destroyed manually,
        //the client always gets destroyed if a packet is sent
        //and if packet connection mode is set to disconnect after client send
        //even if response type is wait for response
        static Packet* Initialize(
            u32 senderID,
            SenderType senderType,
            string receiverIP,
            u16 receiverPort,
            TransportProtocol transportProtocol,
            PacketConnectionMode pcMode,
            vector<u8> payload,
            ResponseType responseType,
            vector<u8>& response);

        u32 GetID() const;
        u32 GetSenderID() const;
        SenderType GetSenderType() const;

        const string& GetReceiverIP() const;
        u16 GetReceiverPort() const;

        TransportProtocol GetTransportProtocol() const;
        PacketConnectionMode GetPacketConnectionMode() const;

        const vector<u8>& GetPayload() const;

        ResponseType GetResponseType() const;
    private:
        ~Packet();

        void Destroy();

        u32 ID{};
        u32 senderID{};
        SenderType senderType{};

        string receiverIP{};
        u16 receiverPort{};

        TransportProtocol transportProtocol{};
        PacketConnectionMode pcMode{};

        vector<u8> payload{};

        ResponseType responseType{};
    };
}