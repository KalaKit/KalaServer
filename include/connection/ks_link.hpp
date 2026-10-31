//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
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
    using std::atomic;
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
        //The socket between sender and receiver stays alive after sending this packet
        //and can receive additional messages without needing to send a new packet
        PCM_KeepAlive = 0,
        //The socket for this packet gets closed and you have to send another packet to start receiving responses
        PCM_DisconnectAfterSend = 1
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
    friend void _SendPacket(
        const bool isSingleShot,
        const u32 linkID,
        const u32 senderID,
        const SenderType senderType,
        const string& receiverIP,
        const u16 receiverPort,
        const u8 timeout,
        const TransportProtocol transportProtocol,
        const PacketConnectionMode packetConnectionMode,
        const ResponseType responseType,
        const vector<u8>& payload,
        const function<void(vector<u8>)>& initialResponseCallback,
        const function<void(vector<u8>)>& listenerResponseCallback);
    friend class KalaServer::Core::KalaServerCore;
    friend class Server;
    friend struct default_delete<Link>;
    public:
        KNODISCARD
        static KalaServerRegistry<Link>& GetRegistry();

        //Returns count of active connections not tied to any link
        static u32 GetActiveHeadlessConnectionCount();

        //Create a socket and send a single packet via the
        //transport protocol type that then gets closed once it is finished,
        //timeout determines how long the packet waits in seconds before closing the socket.
        //Assign a response callback to decide what happens when
        //a direct response is received to the sent packet
        static void SendPacket(
            u32 senderID,
            SenderType senderType,
            string_view receiverIP,
            u16 receiverPort,
            u8 timeout,
            TransportProtocol transportProtocol,
            ResponseType responseType,
            const vector<u8>& payload,
            const function<void(vector<u8>)>& responseCallback);

        //Create a new link that connects this server or client and the target ip.
        //Pick the transport protocol to determine which kind of packets are sent.
        //Keep sender ID as 0 and set sender type to client if client is the sender,
        //otherwise set sender ID to server ID via server->GetID
        static Link* Initialize(
            u32 senderID,
            SenderType senderType,
            string_view receiverIP,
            u16 receiverPort,
            TransportProtocol transportProtocol);

        u32 GetID() const;

        u32 GetSenderID() const;
        SenderType GetSenderType() const;

        //Returns count of active connections tied to this link
        u32 GetActiveConnectionCount() const;

        const string& GetReceiverIP() const;
        u16 GetReceiverPort() const;

        u8 GetTimeout() const;
        void SetTimeout(u8 newValue);

        TransportProtocol GetTransportProtocol() const;

        //Send a packet to the target of this link.
        //Creates a new sender socket if one does not yet exist
        //for this packet connection mode,
        //creates a new listener socket for this packet connection mode
        //if the packet connection mode is set to PCM_KeepAlive.
        //Assign an initial response callback to decide what happens when
        //a direct response is received to the first sent packet
        //that created the sender socket for this packet connection mode,
        //Assign a listener response callback to decide what happens when
        //the listener socket for this packet connection mode receives a message.
        void SendPacket(
            const PacketConnectionMode packetConnectionMode,
            const ResponseType responseType,
            const vector<u8>& payload = {},
            const function<void(vector<u8>)>& initialResponseCallback = {},
            const function<void(vector<u8>)>& listenerResponseCallback = {});

        //Close the link
        void Destroy();
    private:
        ~Link();

        void Update();

        static void CloseRemainingThreads();

        u32 ID{};
        u32 senderID{};
        SenderType senderType{};

        atomic<u32> activeConnections{};

        string receiverIP{};
        u16 receiverPort{};

        u8 timeout = 5;

        TransportProtocol transportProtocol{};
    };
}