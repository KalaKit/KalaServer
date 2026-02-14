//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <curl/curl.h>
#include <curl/easy.h>
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#endif

#include <thread>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/string_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"

#include "server/ks_server.hpp"
#include "server/ks_cloudflare.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaString::HasAnyWhiteSpace;
using KalaHeaders::KalaString::SplitString;
using KalaHeaders::KalaThread::lockwait_m;
using KalaHeaders::KalaThread::unlock_m;

using KalaServer::Core::KalaServerCore;

using std::make_unique;
using std::unique_ptr;
using std::to_string;
using std::string;
using std::wstring;
using std::thread;
using std::this_thread::sleep_for;
using std::memory_order_release;
using std::memory_order_acquire;
using std::chrono::seconds;
using std::chrono::milliseconds;

#ifdef _WIN32
constexpr SOCKET invalid_socket = INVALID_SOCKET;
#else
constexpr int invalid_socket = -1;
#endif

#ifdef _WIN32
static wstring ToWide(const string& input);
#endif

static void InitializeCurl()
{
	static bool curlInitialized{};

	if (curlInitialized) return;

	bool success = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;

	if (success) curlInitialized = true;
	else
	{
		KalaServerCore::ForceClose(
			"Curl error",
			"Failed to initialize Curl!");
	}
}

namespace KalaServer::Server
{
	static bool isInitialized{};
	static bool isReady{};

	static u32 ID{};

	static u16 port{};

	static string serverName{};
	static string domainName{};
	static string serverRoot{};

	static vector<User> users{};
	static mutex m_users{};

	static vector<Route> routes{};
	static mutex m_routes{};

	static abool isListenerRunning{ false };

	static uintptr_t listenerSocket{};
	static thread listenerThread{};
	static mutex m_listenerSocket{};

	static vector<unique_ptr<Connection>> listenerSockets{};
	static mutex m_listenerSockets{};

	static vector<unique_ptr<Connection>> connectSockets{};
	static mutex m_connectSockets{};

	bool ServerCore::Initialize(
		u16 newPort,
		string_view newServerName,
		string_view newDomainName,
		string_view newServerRoot,
		const vector<User>& newUsers,
		const vector<Route>& newRoutes)
	{
		Log::Print(
			"Starting to initialize server '" + string(newServerName) 
			+ "' at port '" + to_string(newPort) 
			+ "' with domain '" + string(newDomainName)
			+ "', server root '" + string(newServerRoot) 
			+ "', '" + to_string(newUsers.size()) 
			+ "' role-based users and '" + to_string(newRoutes.size()) + "' routes.",
			"CLOUDFLARE",
			LogType::LOG_INFO);

		if (newPort < MIN_PORT_RANGE
			|| newPort > MAX_PORT_RANGE)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its port '" + to_string(newPort) + "' is out of range!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (newServerName.empty()
			|| newServerName.length() > 50)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its name is empty or too long!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (newDomainName.empty()
			|| newDomainName.length() > 50)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its domain name '" + string(newDomainName) + "' is empty or too long!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		serverName = newServerName;
		domainName = newDomainName;
		port = newPort;

		isInitialized = true;

		Log::Print(
			"Created new server '" + serverName + "'!",
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool ServerCore::IsInitialized() { return isInitialized; }

	bool ServerCore::IsListenerRunning() { return isListenerRunning.load(memory_order_acquire); }

	bool ServerCore::IsReady() { return isReady; }

	bool ServerCore::HasInternet()
	{
		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot check for internet access because the server has not been initialized!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Cannot check for internet access because the server is not ready!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		const string testPage = "https://www.google.com";

		InitializeCurl();

		CURL* curl = curl_easy_init();
		if (!curl)
		{
			KalaServerCore::ForceClose(
				"Curl error",
				"curl_easy failed!");
		}

		curl_easy_setopt(curl, CURLOPT_URL, testPage.c_str());

		//ignore response body
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

		//reasonable timeouts
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		//follow redirects (google will redirect)
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		//ignore errors
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return res == CURLE_OK;
	}

	u32 ServerCore::GetID() { return ID; }

	u16 ServerCore::GetPort() { return port; }

	const string& ServerCore::GetServerName() { return serverName; }
	const string& ServerCore::GetDomainName() { return domainName; }
	const string& ServerCore::GetServerRoot() { return serverRoot; }

	void ServerCore::CreateListenerSocket(
		bool isLocal,
		function<void(unique_ptr<Connection> c, bool isLocal)> connectionCallback)
	{
		if (listenerSocket != 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + serverName + "' because the server already has a listener socket!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to create new listener socket for server '" + serverName + "' because the server has not been initialized!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to create new listener socket for server '" + serverName + "' because the server is not ready!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Log::Print(
			"Creating a new listener socket for server '" + serverName + "'!",
			"LISTENER_SOCKET",
			LogType::LOG_DEBUG);

#ifdef _WIN32
		WSADATA wsaData{};
		int iResult{};

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + serverName + "' because WSAStartup failed!",
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
				"Failed to create new listener socket for server '" + serverName + "' because socket creation failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			WSACleanup();

			return;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		serverAddress.sin_port = htons(port);

		if (bind(
			newSocket,
			(sockaddr*)&serverAddress,
			sizeof(serverAddress)) == SOCKET_ERROR)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + serverName + "' because socket bind failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			WSACleanup();

			return;
		}

		if (listen(newSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + serverName + "' because socket listen failed!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			WSACleanup();

			return;
		}

		listenerSocket = newSocket;

		Log::Print(
			"Created a new listener socket for server '" + serverName + "'! Starting the accept loop now...",
			"LISTENER_SOCKET",
			LogType::LOG_SUCCESS);

		isListenerRunning.store(true, memory_order_release);

		listenerThread = thread([isLocal, connectionCallback]
			{
				while (isListenerRunning.load(memory_order_acquire))
				{
					bool isHealthy = 
						Cloudflare::IsHealthy(0)
						&& Cloudflare::IsHealthy(1)
						&& Cloudflare::IsHealthy(2)
						&& Cloudflare::IsHealthy(3);

					if (!isHealthy)
					{
						bool isOnline =
							Cloudflare::IsTunnelAlive()
							&& ServerCore::HasInternet();

						//wait a second instead of spamming full check every frame
						if (!isOnline) sleep_for(seconds(SERVER_HEALTH_SLEEP_SECONDS));

						continue;
					}

					SOCKET client = accept(
						listenerSocket,
						nullptr,
						nullptr);

					if (client == INVALID_SOCKET)
					{
						Log::Print(
							"Accept failed! Reason: " + to_string(WSAGetLastError()),
							"ACCEPT_SOCKET",
							LogType::LOG_ERROR,
							2);

						continue;
					}

					unique_ptr<Connection> c = make_unique<Connection>();
					Connection* raw = c.get();

					c->connectionSocket = client;
					if (connectionCallback) connectionCallback(move(c), isLocal);

					sleep_for(milliseconds(SERVER_ACCEPT_SLEEP_MILLISECONDS));
				}
			});

#else
		//TODO: add linux equivalent
#endif
	}

	void ServerCore::CreateConnectSocket(
		const string& targetIP,
		function<void()> onConnectFail)
	{
		//TODO: use callback

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to create connect socket with target IP '" + targetIP + "' for server '" + serverName + "' because the server has not been initialized!",
				"CREATE_CONNECT_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to create connect socket with target IP '" + targetIP + "' for server '" + serverName + "' because the server is not ready!",
				"CREATE_CONNECT_SOCKET",
				LogType::LOG_ERROR,
				2);

			return;
		}
	}

	vector<unique_ptr<Connection>>& ServerCore::GetListenerSockets() { return listenerSockets; }
	mutex& ServerCore::GetListenerMutex() { return m_listenerSockets; }

	vector<unique_ptr<Connection>>& ServerCore::GetConnectSockets() { return connectSockets; }
	mutex& ServerCore::GetConnectMutex() { return m_connectSockets; }

	void ServerCore::SendPacket(
		uintptr_t targetSocket,
		bool getResponse,
		function<void(vector<u8>)> onSucceed,
		function<void()> onFail)
	{
		//TODO: use callbacks

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to send packet from server '" + serverName + "' because the server has not been initialized!",
				"SEND_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to send packet from server '" + serverName + "' because the server is not ready!",
				"SEND_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}
	}

	void ServerCore::SendPacketLocal(
		const string& targetIP,
		bool getResponse,
		function<void(vector<u8>)> onSucceed,
		function<void()> onFail)
	{
		//TODO: use callbacks

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to send local packet from server '" + serverName + "' because the server has not been initialized!",
				"SEND_LOCAL_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to send local packet from server '" + serverName + "' because the server is not ready!",
				"SEND_LOCAL_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}
	}

	void ServerCore::DisconnectTarget(
		uintptr_t targetSocket,
		string_view reason)
	{
		//TODO: send reason

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + serverName + "' because the server has not been initialized!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + serverName + "' because the server is not ready!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}


		if (targetSocket == 0
#ifdef _WIN32
			|| scast<SOCKET>(targetSocket) == invalid_socket)
#else
			|| targetSocket == invalid_socket)
#endif
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + serverName + "' because the socket is unassigned or invalid!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		lockwait_m(m_connectSockets);
		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			Connection* c = it->get();

			if (c->connectionSocket == targetSocket)
			{
				lockwait_m(c->m_connection);
				
				c->isRunning.store(false, memory_order_release);

				if (c->connectionSocket == 0
#ifdef _WIN32
					|| scast<SOCKET>(c->connectionSocket) == invalid_socket)
#else
					|| c->connectionSocket == invalid_socket)
#endif
				{
					Log::Print(
						"Failed to disconnect target via socket for server '" + serverName + "' because the stored socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_ERROR,
						2);
				}
				else
				{
#ifdef _WIN32
					SOCKET socket = scast<SOCKET>(c->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
#else
					int socket = c->connectionSocket;
					shutdown(socket, SHUT_RDWR);
					close(socket);
#endif					
					c->connectionSocket = 0;
				}

				unlock_m(c->m_connection);

				if (c->connectionThread.joinable()) c->connectionThread.join();
				else
				{
					KalaServerCore::ForceClose(
						"Disconnect target error",
						"Failed to disconnect target via socket for server '" + serverName + "' because its thread failed to join!");
				}

				connectSockets.erase(it);

				unlock_m(m_connectSockets);

				Log::Print(
					"Disconnected target via socket for server '" + serverName + "'!",
					"DISCONNECT_TARGET",
					LogType::LOG_SUCCESS);

				return;
			}
		}
		unlock_m(m_connectSockets);

		lockwait_m(m_listenerSockets);
		for (auto it = listenerSockets.begin(); it != listenerSockets.end(); ++it)
		{
			Connection* c = it->get();

			if (c->connectionSocket == targetSocket)
			{
				lockwait_m(c->m_connection);

				c->isRunning.store(false, memory_order_release);

				if (c->connectionSocket == 0
#ifdef _WIN32
					|| scast<SOCKET>(c->connectionSocket) == invalid_socket)
#else
					|| c->connectionSocket == invalid_socket)
#endif
				{
					Log::Print(
						"Failed to disconnect target via socket for server '" + serverName + "' because the stored socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_ERROR,
						2);
				}
				else
				{
#ifdef _WIN32
					SOCKET socket = scast<SOCKET>(c->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
#else
					int socket = c->connectionSocket;
					shutdown(socket, SHUT_RDWR);
					close(socket);
#endif	
					c->connectionSocket = 0;
				}

				unlock_m(c->m_connection);

				if (c->connectionThread.joinable()) c->connectionThread.join();
				else
				{
					KalaServerCore::ForceClose(
						"Disconnect target error",
						"Failed to disconnect target via socket for server '" + serverName + "' because its thread failed to join!");
				}

				listenerSockets.erase(it);

				unlock_m(m_listenerSockets);

				Log::Print(
					"Disconnected target via socket for server '" + serverName + "'!",
					"DISCONNECT_TARGET",
					LogType::LOG_SUCCESS);

				return;
			}
		}
		unlock_m(m_listenerSockets);

		Log::Print(
			"Failed to disconnect target via socket for server '" + serverName + "' because the target socket was not found!",
			"DISCONNECT_TARGET",
			LogType::LOG_ERROR,
			2);
	}

	void ServerCore::DisconnectTarget(
		const string& targetIP,
		string_view reason)
	{
		//TODO: send reason

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + serverName + "' because the server has not been initialized!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + serverName + "' because the server is not ready!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		IPResult result = ServerCore::IsValidIP(targetIP);
		if (result != IPResult::IP_IS_VALID)
		{
			Log::Print(
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + serverName + "'! Reason: " + ServerCore::IPResultToString(result),
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		lockwait_m(m_connectSockets);
		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			Connection* c = it->get();

			if (c->connectionIP == targetIP)
			{
				lockwait_m(c->m_connection);

				c->isRunning.store(false, memory_order_release);
				
				//we don't return error here if there is no socket because the target may only have a local socket via SendPacketLocal
				if (c->connectionSocket == 0
#ifdef _WIN32
					|| scast<SOCKET>(c->connectionSocket) == invalid_socket)
#else
					|| c->connectionSocket == invalid_socket)
#endif
				{
					Log::Print(
						"Couldn't close socket for target via IP '" + targetIP + "' for server '" + serverName + "' because the socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_WARNING);
				}
				else
				{
#ifdef _WIN32
					SOCKET socket = scast<SOCKET>(c->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
#else
					int socket = c->connectionSocket;
					shutdown(socket, SHUT_RDWR);
					close(socket);
#endif	
					c->connectionSocket = 0;
				}

				unlock_m(c->m_connection);

				if (c->connectionThread.joinable()) c->connectionThread.join();
				else
				{
					KalaServerCore::ForceClose(
						"Disconnect target error",
						"Failed to disconnect target via IP '" + targetIP + "' for server '" + serverName + "' because its thread failed to join!");
				}

				connectSockets.erase(it);

				unlock_m(m_connectSockets);

				Log::Print(
					"Disconnected target via IP '" + targetIP + "' for server '" + serverName + "'!",
					"DISCONNECT_TARGET",
					LogType::LOG_SUCCESS);

				return;
			}
		}
		unlock_m(m_connectSockets);

		lockwait_m(m_listenerSockets);
		for (auto it = listenerSockets.begin(); it != listenerSockets.end(); ++it)
		{
			Connection* c = it->get();

			if (c->connectionIP == targetIP)
			{
				lockwait_m(c->m_connection);

				c->isRunning.store(false, memory_order_release);

				//we don't return error here if there is no socket because the target may only have a local socket via SendPacketLocal
				if (c->connectionSocket == 0
#ifdef _WIN32
					|| scast<SOCKET>(c->connectionSocket) == invalid_socket)
#else
					|| c->connectionSocket == invalid_socket)
#endif
				{
					Log::Print(
						"Couldn't close socket for target via IP '" + targetIP + "' for server '" + serverName + "' because the socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_WARNING);
				}
				else
				{
#ifdef _WIN32
					SOCKET socket = scast<SOCKET>(c->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
#else
					int socket = c->connectionSocket;
					shutdown(socket, SHUT_RDWR);
					close(socket);
#endif	
					c->connectionSocket = 0;
				}

				unlock_m(c->m_connection);

				if (c->connectionThread.joinable()) c->connectionThread.join();
				else
				{
					KalaServerCore::ForceClose(
						"Disconnect target error",
						"Failed to disconnect target via IP '" + targetIP + "' for server '" + serverName + "' because its thread failed to join!");
				}

				listenerSockets.erase(it);

				unlock_m(m_listenerSockets);

				Log::Print(
					"Disconnected target via IP '" + targetIP + "' for server '" + serverName + "'!",
					"DISCONNECT_TARGET",
					LogType::LOG_SUCCESS);

				return;
			}
		}
		unlock_m(m_listenerSockets);

		Log::Print(
			"Failed to disconnect target via IP '" + targetIP + "' for server '" + serverName + "' because the target IP was not found!",
			"DISCONNECT_TARGET",
			LogType::LOG_ERROR,
			2);
	}

	void ServerCore::DisconnectListener(string_view reason)
	{
		//TODO: send reason

		if (listenerSocket == 0 
#ifdef _WIN32
			|| scast<SOCKET>(listenerSocket) == invalid_socket)
#else
			|| listenerSocket == invalid_socket)
#endif
		{
			Log::Print(
				"Failed to disconnect listener for server '" + serverName + "' because the server has no listener socket or it is invalid!",
				"LISTENER_DISCONNECT",
				LogType::LOG_WARNING);

			return;
		}

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to disconnect listener for server '" + serverName + "' because the server has not been initialized!",
				"LISTENER_DISCONNECT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect listener for server '" + serverName + "' because the server is not ready!",
				"LISTENER_DISCONNECT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		isListenerRunning.store(false, memory_order_release);

		vector<unique_ptr<Connection>> sockets{};

		lockwait_m(m_listenerSockets);
		listenerSockets.swap(sockets);
		unlock_m(m_listenerSockets);

		for (auto& s : sockets)
		{
			lockwait_m(s->m_connection);

			s->isRunning.store(false, memory_order_release);

			if (s->connectionSocket != 0
#ifdef _WIN32
				&& scast<SOCKET>(s->connectionSocket) != invalid_socket)
#else
				&& s->connectionSocket != invalid_socket)
#endif
			{
#ifdef _WIN32
					SOCKET socket = scast<SOCKET>(s->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
#else
					int socket = s->connectionSocket;
					shutdown(socket, SHUT_RDWR);
					close(socket);
#endif	
				s->connectionSocket = 0;
			}

			unlock_m(s->m_connection);

			if (s->connectionThread.joinable()) s->connectionThread.join();
			else
			{
				KalaServerCore::ForceClose(
					"Disconnect listener error",
					"Failed to disconnect listener for server '" + serverName + "' because a connection thread failed to join!");
			}
		}

#ifdef _WIN32
		SOCKET socket = scast<SOCKET>(listenerSocket);
		shutdown(socket, SD_BOTH);
		closesocket(socket);
#else
		int socket = listenerSocket;
		shutdown(socket, SHUT_RDWR);
		close(socket);
#endif	
		listenerSocket = 0;

		if (listenerThread.joinable()) listenerThread.join();
		else
		{
			KalaServerCore::ForceClose(
				"Listener thread error",
				"Failed to disconnect listener for server '" + serverName + "' because its thread failed to join!");
		}

		Log::Print(
			"Disconnected listener socket for server '" + serverName + "'!",
			"LISTENER_DISCONNECT",
			LogType::LOG_SUCCESS);
	}

	void ServerCore::CancelAllPackets(string_view reason)
	{
		//TODO: send reason

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to cancel all packets for server '" + serverName + "' because the server has not been initialized!",
				"SERVER_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to cancel all packets for server '" + serverName + "' because the server is not ready!",
				"SERVER_PACKET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		vector<unique_ptr<Connection>> sockets{};

		lockwait_m(m_connectSockets);

		if (connectSockets.empty())
		{
			Log::Print(
				"Couldn't cancel all packets for server '" + serverName + "' because there are no active packets.",
				"SERVER_PACKET",
				LogType::LOG_WARNING);

			unlock_m(m_connectSockets);

			return;
		}

		connectSockets.swap(sockets);
		unlock_m(m_connectSockets);

		for (auto& s : sockets)
		{
			lockwait_m(s->m_connection);

			if (s->connectionSocket != 0
#ifdef _WIN32
				&& scast<SOCKET>(s->connectionSocket) != invalid_socket)
#else
				&& s->connectionSocket != invalid_socket)
#endif
			{
#ifdef _WIN32
				SOCKET socket = scast<SOCKET>(s->connectionSocket);
				shutdown(socket, SD_BOTH);
				closesocket(socket);
#else
				int socket = s->connectionSocket;
				shutdown(socket, SHUT_RDWR);
				close(socket);
#endif	
				s->connectionSocket = 0;
			}

			unlock_m(s->m_connection);

			if (s->connectionThread.joinable()) s->connectionThread.join();
			else
			{
				KalaServerCore::ForceClose(
					"Disconnect listener error",
					"Failed to disconnect listener for server '" + serverName + "' because a connection thread failed to join!");
			}
		}

		Log::Print(
			"Closed all packets for server '" + serverName + "'!",
			"SERVER_PACKET",
			LogType::LOG_SUCCESS);
	}

	IPResult ServerCore::IsValidIP(const string& targetIP)
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

	string ServerCore::IPResultToString(IPResult result)
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

	string ServerCore::RoleToString(Role role)
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
	Role ServerCore::StringToRole(const string& role)
	{
		if (role == "BANNED")           return Role::ROLE_BANNED;
		else if (role == "GUEST")       return Role::ROLE_GUEST;
		else if (role == "WHITELISTED") return Role::ROLE_WHITELISTED;
		else if (role == "BLACKLISTED") return Role::ROLE_BLACKLISTED;
		else if (role == "USER")        return Role::ROLE_USER;
		else if (role == "ADMIN")       return Role::ROLE_ADMIN;

		else return Role::ROLE_NONE; //assume all unknown inputs route to NONE by default
	}

	Role ServerCore::GetUserRole(const string& userIP)
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
	void ServerCore::SetUserRole(const string& userIP, Role newRole)
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

	void ServerCore::AddUser(const User& newUser)
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
	void ServerCore::RemoveUser(const string& userIP)
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

	Role ServerCore::GetRouteRole(const string& route)
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
	void ServerCore::SetRouteRole(const string& route, Role newRole)
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

	void ServerCore::AddRoute(const Route& newRoute)
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
	void ServerCore::RemoveRoute(const string& route)
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

	vector<string> ServerCore::GetAllUsersByRole(Role targetRole)
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
	vector<string> ServerCore::GetAllRoutesByRole(Role targetRole)
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

	vector<User> ServerCore::GetAllUsers()
	{
		lockwait_m(m_users);
		vector<User> copy = users;
		unlock_m(m_users);
		return copy;
	}
	vector<Route> ServerCore::GetAllRoutes()
	{
		lockwait_m(m_routes);
		vector<Route> copy = routes;
		unlock_m(m_routes);
		return copy;
	}

	void ServerCore::ClearAllUsers()
	{
		lockwait_m(m_users);
		users.clear();
		unlock_m(m_users);
	}
	void ServerCore::ClearAllRoutes()
	{
		lockwait_m(m_routes);
		routes.clear();
		unlock_m(m_routes);
	}

	void ServerCore::Shutdown()
	{
		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot shut down the server because it has not been initialized!",
				"SERVER_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		DisconnectListener();
		CancelAllPackets();
	}

	void ServerCore::SetServerReadyState(bool state)
	{
		isReady = state;
	}
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