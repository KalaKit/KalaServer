//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"

namespace KalaServer::Server
{
    using std::string;
    using std::string_view;
    using std::unique_ptr;
    using std::vector;
	using std::unordered_map;

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

	enum class IPResult : u8
	{
		IP_TOO_SHORT = 0,            //must be 9 characters or longer
		IP_TOO_LONG = 1,             //must be 15 characters or less

		IP_OUT_OF_RANGE = 2,         //ip adresses have a very limited allowed range
		IP_STRUCTURE_IS_INVALID = 3, //you managed to mess up the ip structure somehow

		IP_IS_VALID = 4
	};

	//Roles are assigned to users and routes,
	//users with higher role can always access same and lower routes unless demoted
	enum class Role : u8
	{
		//Default empty-state and return type for invalid getters,
		//users and routes cannot be given this role
		ROLE_NONE        = 0u,

		//Users with this role have been manually banned or autobanned by the server,
		//routes cannot be given this role
		ROLE_BANNED      = 1u,

		//Users with this role have default server access,
		//guests, whitelisted, users and admins can access routes with this role
		ROLE_GUEST       = 2u,

		//Role dedicated to honeypot routes to catch annoying bots, users cannot be given this role,
		//guests will get autobanned if they access this route
		ROLE_BLACKLISTED = 3u,

		//Users with this role can access routes with user privileges,
		//users and admins can access routes with this role
		ROLE_USER        = 4u,

		//Users with this role bypass all privileges,
		//only admins can access routes with this role
		ROLE_ADMIN       = 5u
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

	struct LIB_API User
	{
		string userIP{};
		Role role{};
	};

	struct LIB_API Route
	{
		string route{}; //path relative to true server route root
		Role role{};
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
		static void DisconnectConnectedUser(const string& targetIP);

		//Closes the server listener socket and all inbound sockets and all outbound packets
		static void DisconnectListener();

		static bool IsValidIP(const string& targetIP);

		static string RoleToString(Role role);
		static Role StringToRole(const string& role);

		static Role GetUserRole(const string& userIP);
		static void SetUserRole(const string& userIP, Role newRole);

		static void AddUser(const User& newUser);
		static void RemoveUser(const string& userIP);

		static Role GetRouteRole(const string& route);
		static void SetRouteRole(const string& route, Role newRole);

		static void AddRoute(const Route& newRoute);
		static void RemoveRoute(const string& route);

		static vector<string> GetAllUsersByRole(Role targetRole);
		static vector<string> GetAllRoutesByRole(Role targetRole);

		static vector<User> GetAllUsers();
		static vector<Route> GetAllRoutes();

		static void ClearAllUsers();
		static void ClearAllRoutes();
    };
}