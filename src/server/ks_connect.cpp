//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <string>
#include <thread>
#include <chrono>
#include <memory>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"
#include "KalaHeaders/string_utils.hpp"

#include "server/ks_connect.hpp"
#include "server/ks_server.hpp"
#include "server/ks_response.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaCore::FromVar;
using KalaHeaders::KalaCore::ToVar;

using KalaHeaders::KalaThread::lockwait_m;
using KalaHeaders::KalaThread::unlock_m;
using KalaHeaders::KalaThread::joinable_thread;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaString::HasAnyWhiteSpace;
using KalaHeaders::KalaString::SplitString;

using KalaServer::Server::Connection;
using KalaServer::Core::KalaServerCore;

using std::memory_order_acquire;
using std::memory_order_release;
using std::string;
using std::to_string;
using std::this_thread::sleep_for;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::make_unique;

#ifdef _WIN32
using ksocket = SOCKET;
#else
using ksocket = int;
#endif

#ifdef _WIN32
constexpr ksocket invalid_socket = INVALID_SOCKET;
#else
constexpr ksocket invalid_socket = -1;
#endif

#ifdef _WIN32
using std::wstring;
static wstring ToWide(const string& input);
#endif

static void ConnectStart(Connection* c);

namespace KalaServer::Server
{
	static vector<User> users{};
	static mutex m_users{};

	static vector<Route> routes{};
	static mutex m_routes{};

	static vector<string> whitelistedIPs{};
	static vector<string> blacklistedIPs{};
	static vector<string> whitelistedExtensions{};
	static vector<string> blacklistedKeywords{};

	static unique_ptr<Connection> listenerSocket{};
	static mutex m_listenerSocket{};

	static vector<unique_ptr<Connection>> connectSockets{};
	static mutex m_connectSockets{};

	Connection::~Connection()
	{
		isRunning.store(false, memory_order_release);

		ksocket cs = 
#ifdef _WIN32
		ToVar<SOCKET>(connectionSocket.load(memory_order_acquire));
#else
		ToVar<int>(connectionSocket.load(memory_order_acquire));
#endif

		if (cs != UNASSIGNED_SOCKET_VALUE)
		{
#ifdef _WIN32
			shutdown(cs, SD_BOTH);
			closesocket(cs);
#else
			shutdown(cs, SHUT_RDWR);
			close(cs);
#endif	
		}

		if (connectionThread.joinable()) listenerSocket->connectionThread.join();
	}

	void Connect::HandleListenerCallback(Connection& c)
	{

	}

	void Connect::CreateListenerSocket(function<void(Connection&)> onConnect)
	{
		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the server has not been initialized!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the server is not ready!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Log::Print(
			"Creating a new listener socket for server '" + ServerCore::GetServerName() + "'!",
			"LISTENER_SOCKET",
			LogType::LOG_INFO);

		lockwait_m(m_listenerSocket);
		if (listenerSocket)
		{
			ksocket readls =
#ifdef _WIN32
			ToVar<SOCKET>(listenerSocket->connectionSocket.load(memory_order_acquire));
#else
			ToVar<int>(listenerSocket->connectionSocket.load(memory_order_acquire));
#endif
				
			if (readls != UNASSIGNED_SOCKET_VALUE)
			{
				Log::Print(
					"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the server already has a listener socket!",
					"LISTENER_SOCKET",
					LogType::LOG_ERROR,
					2);

				unlock_m(m_listenerSocket);

				return;
			}
		}
		unlock_m(m_listenerSocket);

#ifdef _WIN32
		WSADATA wsaData{};
		int iResult{};

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because WSAStartup failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			WSACleanup();

			return;
		}

		SOCKET newSocket = socket(
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP);

		if (newSocket == INVALID_SOCKET)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket creation failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			WSACleanup();

			return;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		serverAddress.sin_port = htons(ServerCore::GetPort());

		if (bind(
			newSocket,
			(sockaddr*)&serverAddress,
			sizeof(serverAddress)) == SOCKET_ERROR)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket bind failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			closesocket(newSocket);
			WSACleanup();

			return;
		}

		if (listen(newSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket listen failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			closesocket(newSocket);
			WSACleanup();

			return;
		}
#else
		int newSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (newSocket < 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket creation failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		serverAddress.sin_port = htons(ServerCore::GetPort());

		if (bind(
			newSocket,
			(sockaddr*)&serverAddress,
			sizeof(serverAddress)) < 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket bind failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			close(newSocket);
			return;
		}

		if (listen(newSocket, SOMAXCONN) < 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket listen failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			close(newSocket);
			return;
		}
#endif
		unique_ptr<Connection> newListenerSocket = make_unique<Connection>();
		Connection* localListener = newListenerSocket.get();

		lockwait_m(m_listenerSocket);
		listenerSocket = std::move(newListenerSocket);

		listenerSocket->connectionSocket.store(FromVar(newSocket), memory_order_release);
		listenerSocket->isRunning.store(true, memory_order_release);

		unlock_m(m_listenerSocket);

		Log::Print(
			"Created a new listener socket for server '" + ServerCore::GetServerName() + "', starting the accept loop!",
			"LISTENER_SOCKET",
			LogType::LOG_SUCCESS);

		localListener->connectionThread = joinable_thread([localListener, onConnect]
			{
				while (true)
				{
					if (!localListener->isRunning.load(memory_order_acquire))
					{
						Log::Print(
							"Listener socket for server '" + ServerCore::GetServerName() + "' has been shut down.",
							"ACCEPT_LOOP",
							LogType::LOG_INFO);

						return;
					}

					if (!Cloudflare::IsTunnelHealthy())
					{
						bool isOnline =
							Cloudflare::IsTunnelAlive()
							&& ServerCore::HasInternet();

						//wait a moment instead of spamming full check every frame
						if (!isOnline) sleep_for(seconds(SERVER_HEALTH_SLEEP_SECONDS));

						continue;
					}
						
#ifdef _WIN32
					ksocket ls = ToVar<SOCKET>(localListener->connectionSocket.load(memory_order_acquire));

					SOCKET client = accept(
						ls,
						nullptr,
						nullptr);

					if (client == invalid_socket)
					{
						int err = WSAGetLastError();

						Log::Print(
							"Connection failed! Reason: " + to_string(err),
							"ACCEPT_LOOP",
							LogType::LOG_ERROR,
							2);

						continue;
					}
#else
					ksocket ls = ToVar<int>(localListener->connectionSocket.load(memory_order_acquire));

					int client = accept(
						ls,
						nullptr,
						nullptr);

					if (client == invalid_socket)
					{
						int err = errno;

						Log::Print(
							"Connection failed! Reason: " + to_string(err),
							"ACCEPT_LOOP",
							LogType::LOG_ERROR,
							2);

						continue;
					}
#endif

					if (!onConnect)
					{
						Log::Print(
							"Connection received but no connection callback was assigned!",
							"ACCEPT_LOOP",
							LogType::LOG_ERROR,
							2);

						continue;
					}

					Log::Print(
						"Connection received.",
						"ACCEPT_LOOP",
						LogType::LOG_INFO);

					unique_ptr<Connection> c = make_unique<Connection>();
					Connection* raw = c.get();

					raw->connectionSocket.store(FromVar(client), memory_order_release);

					lockwait_m(m_connectSockets);
					if (connectSockets.size() >= MAX_ACTIVE_CONNECTIONS)
					{
						unlock_m(m_connectSockets);

						Log::Print(
							"Max user count '" + to_string(MAX_ACTIVE_CONNECTIONS) + "' reached, kicking new connection.",
							"ACCEPT_LOOP",
							LogType::LOG_WARNING);
						
						Response::SendResponse({
							.responseType = ResponseType::R_503,
							.contentType = ContentType::CT_HTML,
							.responseBody = 
								"<html><body><h1>Service unavailable</h1>\n"
								"<p>Server limit reached! Try again later.</p></body></html>",
							.connection = raw
						});

						continue;
					}
					connectSockets.push_back(std::move(c));
					unlock_m(m_connectSockets);

					raw->connectionThread = joinable_thread([onConnect, raw]
						{
							onConnect(*raw);
						});
				}
			});
	}

	bool Connect::IsListenerRunning()
	{ 
		bool isRunning{};
		lockwait_m(m_listenerSocket);
		isRunning = listenerSocket->isRunning.load(memory_order_acquire);
		unlock_m(m_listenerSocket);

		return isRunning;
	}

	void Connect::CreateConnectSocket(
		const string& targetIP,
		function<void()> onConnectFail)
	{
		//TODO: use callback

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to create connect socket with target IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the server has not been initialized!",
				"CREATE_CONNECT_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to create connect socket with target IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the server is not ready!",
				"CREATE_CONNECT_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}
	}

	Connection* Connect::GetListenerSocket() { return listenerSocket.get(); }
	mutex& Connect::GetListenerMutex() { return m_listenerSocket; }

	vector<Connection*> Connect::GetConnectSockets()
	{ 
		vector<Connection*> connections{};
		for (const auto& c : connectSockets)
		{
			connections.push_back(c.get());
		}

		return connections;
	}
	mutex& Connect::GetConnectMutex() { return m_connectSockets; }

	void Connect::SendPacket(
		uintptr_t targetSocket,
		bool getResponse,
		function<void(vector<u8>)> onSucceed,
		function<void()> onFail)
	{
		//TODO: use callbacks

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to send packet from server '" + ServerCore::GetServerName() + "' because the server has not been initialized!",
				"SEND_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to send packet from server '" + ServerCore::GetServerName() + "' because the server is not ready!",
				"SEND_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}
	}

	void Connect::SendPacketLocal(
		const string& targetIP,
		bool getResponse,
		function<void(vector<u8>)> onSucceed,
		function<void()> onFail)
	{
		//TODO: use callbacks

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to send local packet from server '" + ServerCore::GetServerName() + "' because the server has not been initialized!",
				"SEND_LOCAL_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to send local packet from server '" + ServerCore::GetServerName() + "' because the server is not ready!",
				"SEND_LOCAL_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}
	}

	void Connect::DisconnectConnectedUser(
		uintptr_t targetSocket,
		string_view reason)
	{
		//TODO: send reason

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + ServerCore::GetServerName() + "' because the server has not been initialized!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + ServerCore::GetServerName() + "' because the server is not ready!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!targetSocket
#ifdef _WIN32
			|| scast<SOCKET>(targetSocket) == invalid_socket)
#else
			|| targetSocket == invalid_socket)
#endif
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + ServerCore::GetServerName() + "' because the socket is unassigned or invalid!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		lockwait_m(m_connectSockets);

		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			Connection* raw = it->get();

			if (raw
				&& raw->connectionSocket.load(memory_order_acquire) == targetSocket)
			{
				connectSockets.erase(it);

				Log::Print(
					"Disconnected target via socket for server '" + ServerCore::GetServerName() + "'!",
					"DISCONNECT_TARGET",
					LogType::LOG_SUCCESS);

				break;
			}
		}

		unlock_m(m_connectSockets);

		Log::Print(
			"Failed to disconnect target via socket for server '" + ServerCore::GetServerName() + "' because the target socket was not found!",
			"DISCONNECT_TARGET",
			LogType::LOG_ERROR,
			2);
	}

	void Connect::DisconnectConnectedUser(
		const string& targetIP,
		string_view reason)
	{
		//TODO: send reason

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the server has not been initialized!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the server is not ready!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		IPResult result = IsValidIP(targetIP);
		if (result != IPResult::IP_IS_VALID)
		{
			Log::Print(
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "'! Reason: " + IPResultToString(result),
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		lockwait_m(m_connectSockets);
		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			Connection* c = it->get();

			string connectionIP{};
			lockwait_m(c->m_connectionIP);
			connectionIP = c->connectionIP;
			unlock_m(c->m_connectionIP);

			if (connectionIP == targetIP)
			{
				c->isRunning.store(false, memory_order_release);

				ksocket cs =
#ifdef _WIN32
				ToVar<SOCKET>(c->connectionSocket.load(memory_order_acquire));
#else
				ToVar<int>(c->connectionSocket.load(memory_order_acquire));
#endif
				
				//we don't return error here if there is no socket because the target may only have a local socket via SendPacketLocal
				if (cs == UNASSIGNED_SOCKET_VALUE)
				{
					Log::Print(
						"Couldn't close socket for target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_WARNING);
				}
				else
				{
#ifdef _WIN32
					shutdown(cs, SD_BOTH);
					closesocket(cs);
#else
					shutdown(cs, SHUT_RDWR);
					close(cs);
#endif	
					c->connectionSocket.store(UNASSIGNED_SOCKET_VALUE, memory_order_release);
				}

				if (c->connectionThread.joinable()) c->connectionThread.join();
				else
				{
					KalaServerCore::ForceClose(
						"Disconnect target error",
						"Failed to disconnect target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because its thread failed to join!");
				}

				connectSockets.erase(it);

				unlock_m(m_connectSockets);

				Log::Print(
					"Disconnected target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "'!",
					"DISCONNECT_TARGET",
					LogType::LOG_SUCCESS);

				return;
			}
		}
		unlock_m(m_connectSockets);

		Log::Print(
			"Failed to disconnect target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the target IP was not found!",
			"DISCONNECT_TARGET",
			LogType::LOG_ERROR,
			2);
	}

	void Connect::DisconnectListener(string_view reason)
	{
		//TODO: send reason
		
		if (listenerSocket == nullptr)
		{
			Log::Print(
				"Failed to disconnect listener for server '" + ServerCore::GetServerName() + "' because the server has no listener socket!",
				"LISTENER_DISCONNECT",
				LogType::LOG_WARNING);

			return;
		}

		ksocket ls = 
#ifdef _WIN32
		ToVar<SOCKET>(listenerSocket->connectionSocket.load(memory_order_acquire));
#else
		ToVar<int>(listenerSocket->connectionSocket.load(memory_order_acquire));
#endif

		if (ls == UNASSIGNED_SOCKET_VALUE)
		{
			Log::Print(
				"Failed to disconnect listener for server '" + ServerCore::GetServerName() + "' because the server has not assigned a listener socket!",
				"LISTENER_DISCONNECT",
				LogType::LOG_WARNING);

			return;
		}

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to disconnect listener for server '" + ServerCore::GetServerName() + "' because the server has not been initialized!",
				"LISTENER_DISCONNECT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect listener for server '" + ServerCore::GetServerName() + "' because the server is not ready!",
				"LISTENER_DISCONNECT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		lockwait_m(m_listenerSocket);
		if (listenerSocket) listenerSocket = nullptr;
		unlock_m(m_listenerSocket);

		lockwait_m(m_connectSockets);
		for (const auto& c : connectSockets)
		{
			DisconnectConnectedUser(c->connectionSocket.load(memory_order_acquire), reason);
		}
		unlock_m(m_connectSockets);

		CancelAllPackets(reason);

		Log::Print(
			"Disconnected listener socket for server '" + ServerCore::GetServerName() + "'!",
			"LISTENER_DISCONNECT",
			LogType::LOG_SUCCESS);
	}

	void Connect::CancelAllPackets(string_view reason)
	{
		//TODO: send reason

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to cancel all packets for server '" + ServerCore::GetServerName() + "' because the server has not been initialized!",
				"SERVER_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to cancel all packets for server '" + ServerCore::GetServerName() + "' because the server is not ready!",
				"SERVER_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		lockwait_m(m_connectSockets);

		if (connectSockets.empty())
		{
			Log::Print(
				"Couldn't cancel all packets for server '" + ServerCore::GetServerName() + "' because there are no active packets.",
				"SERVER_PACKET",
				LogType::LOG_WARNING);

			unlock_m(m_connectSockets);

			return;
		}
		
		for (const auto& s : connectSockets)
		{
			ksocket cs = 
#ifdef _WIN32
			ToVar<SOCKET>(s->connectionSocket.load(memory_order_acquire));
#else
			ToVar<int>(s->connectionSocket.load(memory_order_acquire));
#endif

			if (cs != UNASSIGNED_SOCKET_VALUE)
			{
#ifdef _WIN32
				shutdown(cs, SD_BOTH);
				closesocket(cs);
#else
				shutdown(cs, SHUT_RDWR);
				close(cs);
#endif	
				s->connectionSocket.store(UNASSIGNED_SOCKET_VALUE, memory_order_release);
			}

			if (s->connectionThread.joinable()) s->connectionThread.join();
			else
			{
				KalaServerCore::ForceClose(
					"Disconnect listener error",
					"Failed to disconnect listener for server '" + ServerCore::GetServerName() + "' because a connection thread failed to join!");
			}
		}

		unlock_m(m_connectSockets);

		Log::Print(
			"Closed all packets for server '" + ServerCore::GetServerName() + "'!",
			"SERVER_PACKET",
			LogType::LOG_SUCCESS);
	}

	IPResult Connect::IsValidIP(const string& targetIP)
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

	string Connect::IPResultToString(IPResult result)
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

	string Connect::RoleToString(Role role)
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
	Role Connect::StringToRole(const string& role)
	{
		if (role == "BANNED")           return Role::ROLE_BANNED;
		else if (role == "GUEST")       return Role::ROLE_GUEST;
		else if (role == "WHITELISTED") return Role::ROLE_WHITELISTED;
		else if (role == "BLACKLISTED") return Role::ROLE_BLACKLISTED;
		else if (role == "USER")        return Role::ROLE_USER;
		else if (role == "ADMIN")       return Role::ROLE_ADMIN;

		else return Role::ROLE_NONE; //assume all unknown inputs route to NONE by default
	}

	Role Connect::GetUserRole(const string& userIP)
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
	void Connect::SetUserRole(const string& userIP, Role newRole)
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

	void Connect::AddUser(const User& newUser)
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
	void Connect::RemoveUser(const string& userIP)
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

	Role Connect::GetRouteRole(const string& route)
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
	void Connect::SetRouteRole(const string& route, Role newRole)
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

	void Connect::AddRoute(const Route& newRoute)
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
	void Connect::RemoveRoute(const string& route)
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

	vector<string> Connect::GetAllUsersByRole(Role targetRole)
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
	vector<string> Connect::GetAllRoutesByRole(Role targetRole)
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

	vector<User> Connect::GetAllUsers()
	{
		lockwait_m(m_users);
		vector<User> copy = users;
		unlock_m(m_users);
		return copy;
	}
	vector<Route> Connect::GetAllRoutes()
	{
		lockwait_m(m_routes);
		vector<Route> copy = routes;
		unlock_m(m_routes);
		return copy;
	}

	void Connect::ClearAllUsers()
	{
		lockwait_m(m_users);
		users.clear();
		unlock_m(m_users);
	}
	void Connect::ClearAllRoutes()
	{
		lockwait_m(m_routes);
		routes.clear();
		unlock_m(m_routes);
	}
}

void ConnectStart(Connection* c)
{

}

#ifdef _WIN32
wstring ToWide(const string& input)
{
	if (input.empty()) return wstring();

	int size_needed = MultiByteToWideChar(
		CP_UTF8,
		0,
		input.data(),
		scast<int>(input.size()),
		nullptr,
		0);

	wstring wstr(size_needed - 1, 0);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		input.data(),
		scast<int>(input.size()),
		wstr.data(),
		size_needed);

	return wstr;
}
#endif