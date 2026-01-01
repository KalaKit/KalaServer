//Copyright(C) 2025 Lost Empire Entertainment
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

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/string_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"

namespace KalaServer::Server
{
	class Inbound;
}

namespace KalaServer::Server
{
	using std::unique_ptr;
	using std::vector;
	using std::string;
	using std::unordered_set;
	using std::thread;
	using std::mutex;
	using std::function;

	using u8 = uint8_t;
	using u16 = uint16_t;
	using u32 = uint32_t;
	using f32 = float;

	using KalaHeaders::KalaLog::Log;
	using KalaHeaders::KalaLog::LogType;
	using KalaHeaders::KalaString::HasAnyWhiteSpace;
	using KalaHeaders::KalaString::ContainsString;
	using KalaHeaders::KalaString::SplitString;
	using KalaHeaders::KalaThread::abool;
	using KalaHeaders::KalaThread::lockwait_m;
	using KalaHeaders::KalaThread::unlock_m;

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
		friend class KalaServer::Server::Inbound;
	public:
		//Initialize a new server on this process.
		//Port is where your server connects to Cloudflare.
		//Server name helps distinguish this server from other servers.
		//Domain name is name inserted to url path in browser.
		//Server root is the true origin where the server will expose routes from relative to where the process was ran from,
		//this means yourserver.exe is default root, but that is not recommended <--- always adjust that.
		//Users container lists all user ips and their roles.
		//Routes container lists all routes and their required role to access them
		static void Initialize(
			u16 port,
			const string& serverName,
			const string& domainName,
			const string& serverRoot = "/",
			const vector<User>& users = {},
			const vector<Route>& routes = {});

		//Returns true if this server instance has been initialized successfully
		static inline bool IsInitialized() { return isInitialized; }

		//Returns true if the server Cloudflare backend has been initialized successfully,
		//the server cannot be started if its not ready yet, even if its instance is already initialized
		static inline bool IsReady() { return isReady; }

		//Returns false if the server fails to connect to google.com
		static bool HasInternet();

		static inline u32 GetID() { return ID; }

		static inline u16 GetPort() { return port; }

		static inline const string& GetServerName() { return serverName; }
		static inline const string& GetDomainName() { return domainName; }
		static inline const string& GetServerRoot() { return serverRoot; }

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
			const vector<u8>& reason = {});

		//Disconnect the target via IP with an optional reason sent as payload
		static void DisconnectTarget(
			const string& targetIP,
			const vector<u8>& reason = {});

		//Closes the server listener socket and all inbound sockets,
		//with optional reason sent as payload to all inbound sockets
		static void DisconnectListener(const vector<u8>& reason = {});

		//Closes all outgoing packet sockets,
		//with optional reason sent as payload
		static void CancelAllPackets(const vector<u8>& reason = {});

		static inline IPResult IsValidIP(const string& targetIP)
		{
			if (HasAnyWhiteSpace(targetIP)) return IPResult::IP_STRUCTURE_IS_INVALID;

			if (targetIP.length() < 9) return IPResult::IP_TOO_SHORT;
			if (targetIP.length() > 15) return IPResult::IP_TOO_LONG;

			u8 dotCount{};
			for (const auto& c : targetIP)
			{
				if (c == '.') dotCount++;
				if (dotCount > 3) return IPResult::IP_STRUCTURE_IS_INVALID;
			}

			if (dotCount < 3) return IPResult::IP_STRUCTURE_IS_INVALID;

			vector<string> split = SplitString(targetIP, ".");

			try
			{
				int v1 = stoi(split[0]);
				if (v1 != 10
					&& v1 != 172
					&& v1 != 192)
				{
					return IPResult::IP_OUT_OF_RANGE;
				}

				int v2 = stoi(split[1]);

				if (v1 == 10
					&& (v2 < 0
					|| v2 > 255))
				{
					return IPResult::IP_OUT_OF_RANGE;
				}
				if (v1 == 172
					&& (v2 < 16
					|| v2 > 31))
				{
					return IPResult::IP_OUT_OF_RANGE;
				}
				if (v1 == 192
					&& v2 != 168)
				{
					return IPResult::IP_OUT_OF_RANGE;
				}

				if (v2 < 0
					|| v2 > 255)
				{
					return IPResult::IP_OUT_OF_RANGE;
				}

				int v3 = stoi(split[2]);
				if (v3 < 0
					|| v3 > 255)
				{
					return IPResult::IP_OUT_OF_RANGE;
				}

				int v4 = stoi(split[3]);
				if (v4 < 0
					|| v4 > 255)
				{
					return IPResult::IP_OUT_OF_RANGE;
				}
			}
			catch (...)
			{
				return IPResult::IP_STRUCTURE_IS_INVALID;
			}

			return IPResult::IP_IS_VALID;
		}

		static inline string IPResultToString(IPResult result)
		{
			switch (result)
			{
			default:                                return "Unknown error!";
			case IPResult::IP_TOO_SHORT:            return "IP address was too short!";
			case IPResult::IP_TOO_LONG:             return "IP address was too long!";
			case IPResult::IP_OUT_OF_RANGE:         return "IP address was out of range!";
			case IPResult::IP_STRUCTURE_IS_INVALID: return "IP address structure was invalid!";
			}
		}

		static inline string RoleToString(Role role)
		{
			switch (role)
			{
			default:
			case Role::ROLE_NONE:         return "NONE";

			case Role::ROLE_BANNED:       return "BANNED";
			case Role::ROLE_GUEST:        return "GUEST";
			case Role::ROLE_WHITELISTED:  return "WHITELISTED";
			case Role::ROLE_BLACKLISTED:  return "BLACKLISTED";
			case Role::ROLE_USER:         return "USER";
			case Role::ROLE_ADMIN:        return "ADMIN";
			}
		}
		static inline Role StringToRole(const string& role)
		{
			if (role == "BANNED")           return Role::ROLE_BANNED;
			else if (role == "GUEST")       return Role::ROLE_GUEST;
			else if (role == "WHITELISTED") return Role::ROLE_WHITELISTED;
			else if (role == "BLACKLISTED") return Role::ROLE_BLACKLISTED;
			else if (role == "USER")        return Role::ROLE_USER;
			else if (role == "ADMIN")       return Role::ROLE_ADMIN;

			else return Role::ROLE_NONE; //assume all unknown inputs route to NONE by default
		}

		static inline Role GetUserRole(const string& userIP)
		{
			IPResult result = IsValidIP(userIP);
			if (result != IPResult::IP_IS_VALID)
			{
				Log::Print(
					"Failed to get role for user with IP '" + userIP + "'! Reason: " + IPResultToString(result),
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return Role::ROLE_BANNED;
			}

			lockwait_m(m_users);

			for (const auto& u : users)
			{
				if (u.userIP == userIP)
				{
					unlock_m(m_users); //early unlock

					return u.role;
				}
			}

			unlock_m(m_users);

			Log::Print(
				"Failed to get role for user with IP '" + userIP + "' because that user does not exist!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return Role::ROLE_NONE;
		}
		static inline void SetUserRole(const string& userIP, Role newRole)
		{
			IPResult result = IsValidIP(userIP);
			if (result != IPResult::IP_IS_VALID)
			{
				Log::Print(
					"Failed to set role for user with IP '" + userIP + "'! Reason: " + IPResultToString(result),
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return;
			}

			if (newRole == Role::ROLE_NONE
				|| newRole == Role::ROLE_BLACKLISTED)
			{
				Log::Print(
					"Role '" + RoleToString(newRole) + "' cannot be given to users!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return;
			}

			lockwait_m(m_users);

			for (auto& u : users)
			{
				if (u.userIP == userIP)
				{
					if (u.role == newRole)
					{
						Log::Print(
							"Failed to set role for user with IP '" + userIP + "' because that user already has that role!",
							"SERVER",
							LogType::LOG_ERROR,
							2);

						unlock_m(m_users); //early unlock

						return;
					}

					u.role = newRole;

					Log::Print(
						"Set user '" + userIP + "' role to '" + RoleToString(newRole) + "'.",
						"SERVER",
						LogType::LOG_SUCCESS);

					unlock_m(m_users); //early unlock

					return;
				}
			}

			unlock_m(m_users);

			Log::Print(
				"Failed to set role for user with IP '" + userIP + "' because that user does not exist!",
				"SERVER",
				LogType::LOG_ERROR,
				2);
		}

		static inline void AddUser(const User& newUser)
		{
			IPResult result = IsValidIP(newUser.userIP);
			if (result != IPResult::IP_IS_VALID)
			{
				Log::Print(
					"Failed to add new user with IP '" + newUser.userIP + "'! Reason: " + IPResultToString(result),
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return;
			}

			if (newUser.role == Role::ROLE_NONE
				|| newUser.role == Role::ROLE_BLACKLISTED)
			{
				Log::Print(
					"Role '" + RoleToString(newUser.role) + "' cannot be given to new user with IP '" + newUser.userIP + "'!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return;
			}

			lockwait_m(m_users);

			for (const auto& u : users)
			{
				if (u.userIP == newUser.userIP)
				{
					Log::Print(
						"Failed to add new user with IP '" + newUser.userIP + "' because that user has already been added!",
						"SERVER",
						LogType::LOG_ERROR,
						2);

					unlock_m(m_users); //early unlock

					return;
				}
			}

			users.push_back(newUser);

			unlock_m(m_users);

			Log::Print(
				"Added new user '" + newUser.userIP + "' with role '" + RoleToString(newUser.role) + "'!",
				"SERVER",
				LogType::LOG_SUCCESS);
		}
		static inline void RemoveUser(const string& userIP)
		{
			IPResult result = IsValidIP(userIP);
			if (result != IPResult::IP_IS_VALID)
			{
				Log::Print(
					"Failed to remove existing user with IP '" + userIP + "'! Reason: " + IPResultToString(result),
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return;
			}

			lockwait_m(m_users);

			auto it = remove_if(
				users.begin(),
				users.end(),
				[&](const User& u) { return u.userIP == userIP; });

			if (it == users.end())
			{
				Log::Print(
					"Failed to remove existing user with IP '" + userIP + "' because that user does not exist!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				unlock_m(m_users); //early unlock

				return;
			}

			users.erase((it), users.end());

			unlock_m(m_users);

			Log::Print(
				"Removed existing user '" + userIP + "'!",
				"SERVER",
				LogType::LOG_SUCCESS);
		}

		static inline Role GetRouteRole(const string& route)
		{
			lockwait_m(m_routes);

			for (const auto& r : routes)
			{
				if (r.route == route)
				{
					unlock_m(m_routes);

					return r.role;
				}
			}

			unlock_m(m_routes);

			Log::Print(
				"Failed to get role for route '" + route + "' because that route does not exist!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return Role::ROLE_NONE;
		}
		static inline void SetRouteRole(const string& route, Role newRole)
		{
			if (newRole == Role::ROLE_NONE
				|| newRole == Role::ROLE_BANNED
				|| newRole == Role::ROLE_WHITELISTED)
			{
				Log::Print(
					"Role '" + RoleToString(newRole) + "' cannot be given to routes!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return;
			}

			lockwait_m(m_routes);

			for (auto& r : routes)
			{
				if (r.route == route)
				{
					if (r.role == newRole)
					{
						Log::Print(
							"Failed to set role for route '" + route + "' because that route already has that role!",
							"SERVER",
							LogType::LOG_ERROR,
							2);

						unlock_m(m_routes); //early unlock

						return;
					}

					r.role = newRole;

					Log::Print(
						"Set route '" + route + "' role to '" + RoleToString(newRole) + "'.",
						"SERVER",
						LogType::LOG_SUCCESS);

					unlock_m(m_routes); //early unlock

					return;
				}
			}

			unlock_m(m_routes);

			Log::Print(
				"Failed to set role for route '" + route + "' because that route does not exist!",
				"SERVER",
				LogType::LOG_ERROR,
				2);
		}

		static inline void AddRoute(const Route& newRoute)
		{
			if (newRoute.role == Role::ROLE_NONE
				|| newRoute.role == Role::ROLE_BANNED
				|| newRoute.role == Role::ROLE_WHITELISTED)
			{
				Log::Print(
					"Role '" + RoleToString(newRoute.role) + "' cannot be given to new route '" + newRoute.route + "'!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return;
			}

			lockwait_m(m_routes);

			for (const auto& r : routes)
			{
				if (r.route == newRoute.route)
				{
					Log::Print(
						"Failed to add new route '" + newRoute.route + "' because that route has already been added!",
						"SERVER",
						LogType::LOG_ERROR,
						2);

					unlock_m(m_routes); //early unlock

					return;
				}
			}

			routes.push_back(newRoute);

			unlock_m(m_routes);

			Log::Print(
				"Added new route '" + newRoute.route + "' with role '" + RoleToString(newRoute.role) + "'!",
				"SERVER",
				LogType::LOG_SUCCESS);
		}
		static inline void RemoveRoute(const string& route)
		{
			lockwait_m(m_routes);

			auto it = remove_if(
				routes.begin(),
				routes.end(),
				[&](const Route& u) { return u.route == route; });

			if (it == routes.end())
			{
				Log::Print(
					"Failed to remove existing route '" + route + "' because that route does not exist!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				unlock_m(m_routes);

				return;
			}

			routes.erase((it), routes.end());

			unlock_m(m_routes);

			Log::Print(
				"Removed existing route '" + route + "'!",
				"SERVER",
				LogType::LOG_SUCCESS);
		}

		static inline vector<string> GetAllUsersByRole(Role targetRole)
		{
			lockwait_m(m_users);

			vector<string> foundUsers{};
			for (const auto& u : users)
			{
				if (u.role == targetRole) foundUsers.push_back(u.userIP);
			}

			unlock_m(m_users);

			return foundUsers;
		}
		static inline vector<string> GetAllRoutesByRole(Role targetRole)
		{
			lockwait_m(m_routes);

			vector<string> foundRoutes{};
			for (const auto& r : routes)
			{
				if (r.role == targetRole) foundRoutes.push_back(r.route);
			}

			unlock_m(m_routes);

			return foundRoutes;
		}

		static inline vector<User> GetAllUsers()
		{ 
			lockwait_m(m_users);
			vector<User> copy = users;
			unlock_m(m_users);
			return copy;
		}
		static inline vector<Route> GetAllRoutes()
		{ 
			lockwait_m(m_routes);
			vector<Route> copy = routes;
			unlock_m(m_routes);
			return copy;
		}

		static inline void ClearAllUsers()
		{ 
			lockwait_m(m_users);
			users.clear();
			unlock_m(m_users);
		}
		static inline void ClearAllRoutes()
		{
			lockwait_m(m_routes);
			routes.clear();
			unlock_m(m_routes);
		}

		//Close all sockets and clear all server resources
		static void Shutdown();
	protected:
		static inline bool isInitialized{};
		static inline bool isReady{};

		static inline u32 ID{};

		static inline u16 port{};

		static inline string serverName{};
		static inline string domainName{};
		static inline string serverRoot{};

		static inline vector<User> users{};
		static inline mutex m_users{};

		static inline vector<Route> routes{};
		static inline mutex m_routes{};

		static inline abool isListenerRunning{ false };

		static inline vector<unique_ptr<Connection>> listenerSockets{};
		static inline mutex m_listenerSockets{};

		static inline vector<unique_ptr<Connection>> connectSockets{};
		static inline mutex m_connectSockets{};
	};
}