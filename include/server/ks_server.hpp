//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <functional>
#include <filesystem>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"
#include "server/ks_cloudflare.hpp"

namespace KalaServer::Server
{
	using std::unique_ptr;
	using std::vector;
	using std::string;
	using std::string_view;
	using std::unordered_set;
	using std::thread;
	using std::mutex;
	using std::function;
	using std::filesystem::path;

	using KalaHeaders::KalaThread::abool;

	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using f32 = float;

	constexpr u16 MIN_PORT_RANGE = 1024;
	constexpr u16 MAX_PORT_RANGE = 65535;

	//Sleep this many seconds on the listener thread before retrying from start
	//if internet checks failed at the top of the listener thread
	constexpr u8 SERVER_HEALTH_SLEEP_SECONDS = 1;

	//Sleep this many milliseconds after each successful accept loop
	constexpr u8 SERVER_ACCEPT_SLEEP_MILLISECONDS = 5;

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
		ROLE_NONE        = 0,

		//Users with this role have been manually banned or autobanned by the server,
		//routes cannot be given this role
		ROLE_BANNED      = 1,

		//Users with this role have default server access,
		//guests, whitelisted, users and admins can access routes with this role
		ROLE_GUEST       = 2,

		//Users with this role are same as guests but will never get autobanned,
		//routes cannot be given this role
		ROLE_WHITELISTED = 3,

		//Role dedicated to honeypot routes to catch annoying bots, users cannot be given this role,
		//guests will get autobanned if they access this route
		ROLE_BLACKLISTED = 4,

		//Users with this role can access routes with user privileges,
		//users and admins can access routes with this role
		ROLE_USER        = 5,

		//Users with this role bypass all privileges,
		//only admins can access routes with this role
		ROLE_ADMIN       = 6
	};

	//Active user whose data reached into the server
	//or server-created packet which is going out of the server.
	//Socket will not be filled if it belongs to an outgoing packet created by the server via SendPacketLocal
	struct LIB_API Connection
	{
		abool isRunning{};

		string connectionIP{};
		uintptr_t connectionSocket{};
		mutex m_connection{};

		thread connectionThread{};
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

	class LIB_API ServerCore
	{
	friend class Cloudflare;
	public:
		//Initialize a new server on this process.
		//Port is where your server connects to Cloudflare.
		//Server name helps distinguish this server from other servers.
		//Domain name is name inserted to url path in browser.
		//Server root is the true origin where the server will expose routes from relative to where the process was ran from,
		//this means yourserver.exe is default root, but that is not recommended <--- always adjust that.
		//Users container lists all user ips and their roles.
		//Routes container lists all routes and their required role to access them
		static bool Initialize(
			u16 port,
			string_view serverName,
			string_view domainName,
			const path& serverRoot = "/",
			const vector<User>& users = {},
			const vector<Route>& routes = {});

		//Returns true if this server instance has been initialized successfully
		static bool IsInitialized();

		static bool IsListenerRunning();

		//Returns true if the server Cloudflare backend has been initialized successfully,
		//the server cannot be started if its not ready yet, even if its instance is already initialized
		static bool IsReady();

		//Returns false if the server fails to connect to google.com
		static bool HasInternet();

		static u32 GetID();

		static u16 GetPort();
		static const string& GetServerName();
		static const string& GetDomainName();
		static const path& GetServerRoot();

		//Create a new listener socket, the sole purpose of this socket is to be able to receive
		//incoming traffic so others with internet access can communicate with this server.
		//Only one listener socket is allowed, it is created on a separate thread.
		//Setting isLocal to false will keep each connected socket alive after it has completed its first loop,
		//otherwise that socket dies once its done which is ideal for website inbound sockets.
		//Pass a callback for what to do when a connection succeeds and is ready to be processed,
		//it is recommended to use Inbound::HandleWebRequest for website requests.
		static void CreateListenerSocket(
			bool isLocal,
			function<void(unique_ptr<Connection> c, bool isLocal)> connectionCallback);

		//Create a new socket for sending packets to a specific target IP,
		//required for sending non-local packets.
		//Can pass an optional callback that gets fired if this connect socket fails to be created.
		static void CreateConnectSocket(
			const string& targetIP,
			function<void()> onConnectFail = {});

		static vector<unique_ptr<Connection>>& GetListenerSockets();
		static mutex& GetListenerMutex();

		static vector<unique_ptr<Connection>>& GetConnectSockets();
		static mutex& GetConnectMutex();

		//Send a packet from this server to a known target,
		//requires a socket that has been already created with CreateConnectSocket.
		//If getResponse is true then onSucceed does your desired callback
		//with the returned payload and onFail calls your response failure callback
		static void SendPacket(
			uintptr_t targetSocket,
			bool getResponse = false,
			function<void(vector<u8>)> onSucceed = {},
			function<void()> onFail = {});

		//Send a local packet from this server, does not keep the socket alive after use.
		//If getResponse is true then onSucceed does your desired callback
		//with the returned payload and onFail calls your response failure callback
		static void SendPacketLocal(
			const string& targetIP,
			bool getResponse = false,
			function<void(vector<u8>)> onSucceed = {},
			function<void()> onFail = {});

		//Disconnect the target via socket with an optional reason sent as payload
		static void DisconnectTarget(
			uintptr_t targetSocket,
			string_view reason = {});

		//Disconnect the target via IP with an optional reason sent as payload
		static void DisconnectTarget(
			const string& targetIP,
			string_view reason = {});

		//Closes the server listener socket and all inbound sockets,
		//with optional reason sent as payload to all inbound sockets
		static void DisconnectListener(string_view reason = {});

		//Closes all outgoing packet sockets,
		//with optional reason sent as payload
		static void CancelAllPackets(string_view reason = {});

		static IPResult IsValidIP(const string& targetIP);

		static string IPResultToString(IPResult result);

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

		//Close all sockets and clear all server resources
		static void Shutdown();
	private:
		static void SetServerReadyState(bool state);
	};
}