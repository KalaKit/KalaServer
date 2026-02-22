//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"

namespace KalaServer::Server
{
    using std::string;
    using std::string_view;
    using std::unique_ptr;
    using std::vector;
	using std::unordered_map;
	using std::chrono::steady_clock;

    using u8 = uint8_t;
	using u16 = uint16_t;
    using u32 = uint32_t;

    using KalaHeaders::KalaThread::thread;
    using KalaHeaders::KalaThread::mutex;
	using KalaHeaders::KalaThread::abool;
	using KalaHeaders::KalaThread::auptr;

	//How long is a user by IP timed out for in minutes
	//when violating max payload size or min packet spacing
	constexpr u8 TIME_OUT_PERIOD_M = 10u;

	//How long in seconds do we store all connections and check if the connected IP
	//has connected faster than MIN_PACKET_SPACING within this time period
	constexpr u8 ROLLING_WINDOW_TIMER_S = 5u;
	//Whats the shortest allowed time in milliseconds that a client
	//is allowed to have between each connection attempt
	constexpr u8 MIN_PACKET_SPACING_MS = 200u;

	//Wait for this amount of seconds before deeming the connection as inactive.
	constexpr u16 ACCEPT_WAIT_TIME_S = 60u;

	//Client must not exceed this max payload capacity in bytes at accept loop
	constexpr u16 MAX_TOTAL_PAYLOAD_SIZE_BYTES = 8192u;

	//Unreachable socket value for unassigned socket
	constexpr u32 UNASSIGNED_SOCKET_VALUE = 1000000u;

	//All further connect sockets are closed if this amount of total connections is reached
	constexpr u16 MAX_ACTIVE_CONNECTIONS = 1000u;

	//Sleep this many seconds on the listener thread before retrying from start
	//if internet checks failed at the top of the listener thread
	constexpr u8 SERVER_HEALTH_SLEEP_S = 1;

	struct BannedIP
	{
		string targetIP{};

		//Leave unassigned to mark as permanent ban
		steady_clock::time_point expiresAt{};
	};

	//The data received from an accepted socket ready to be parsed
	struct LIB_API RequestData
	{
		string method{};
		string route{};
		string httpVersion{};
		string host{};
		unordered_map<string, string> headers{};
		string body{};
	};

	//Any inbound or outbound socket and its data regardless of origin or destination,
	//connectionSocket will not be filled if it belongs to an
	//outgoing packet created by the server via SendPacketLocal
	struct LIB_API Connection
	{
		abool isRunning{};

		string connectionIP{};
		string connectionRoute{};

		auptr connectionSocket = UNASSIGNED_SOCKET_VALUE;

		thread connectionThread{};

		RequestData requestData{};
	};

    class LIB_API Connect 
    {
    public:
        //Create a new listener socket, the sole purpose of this socket is to be able to receive
		//incoming traffic so others with internet access can communicate with this server.
		//Only one listener socket is allowed, it is created on a separate thread.
		static bool CreateListenerSocket();

        static bool IsListenerRunning();

		static const Connection& GetListenerSocket();
		static mutex& GetListenerMutex();

		static const vector<const Connection*>& GetConnectSockets();
		static mutex& GetConnectMutex();

		//Disconnect the target user via connect socket
		static void DisconnectConnectedUser(uintptr_t targetSocket);

		//Disconnect the target user via IP
		static void DisconnectConnectedUser(string_view targetIP);

		//Closes the server listener socket and all inbound sockets and all outbound packets
		static void DisconnectListener();

		static bool IsValidIP(string_view targetIP);

		static bool IsBannedIP(string_view targetIP);
		static void BanIP(string_view targetIP);
		static void UnbanIP(string_view targetIP);

		static void AddRoute(string_view newValue);
		static void RemoveRoute(string_view newValue);

		static void AddBlacklistedKeyword(string_view newValue);
		static void RemoveBlacklistedKeyword(string_view newValue);

		static void ClearAllUsers();
		static void ClearAllRoutes();
    };
}