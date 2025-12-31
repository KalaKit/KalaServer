//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <wininet.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Wininet.lib")
#else
//TODO: add linux equivalent
#endif

#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include "server/ks_server.hpp"
#include "server/ks_cloudflare.hpp"
#include "core/ks_core.hpp"

using KalaServer::Core::KalaServerCore;
using KalaServer::Server::CloudFlare;

using std::make_unique;
using std::unique_ptr;
using std::to_string;
using std::string;
using std::wstring;
using std::thread;
using std::memory_order_release;
using std::memory_order_acquire;
using std::this_thread::sleep_for;
using std::chrono::seconds;
using std::chrono::milliseconds;

static uintptr_t listenerSocket{};
static thread listenerThread{};

static wstring ToWide(const string& input);

namespace KalaServer::Server
{
	void ServerCore::Initialize(
		u16 port,
		const string& serverName,
		const string& domainName,
		const string& serverRoot,
		const vector<User>& users,
		const vector<Route>& routes)
	{
		if (port < MIN_PORT_RANGE
			|| port > MAX_PORT_RANGE)
		{
			Log::Print(
				"Failed to initialize server '" + serverName + "' because its port '" + to_string(port) + "' is out of range!",
				"SERVER_INIT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (serverName.empty()
			|| serverName.length() > 50)
		{
			Log::Print(
				"Failed to initialize server '" + serverName + "' because its name is empty or too long!",
				"SERVER_INIT",
				LogType::LOG_ERROR,
				2);

			return;
		}
		if (domainName.empty()
			|| domainName.length() > 50)
		{
			Log::Print(
				"Failed to initialize server '" + serverName + "' because its domain name '" + domainName + "' is empty or too long!",
				"SERVER_INIT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		ServerCore::serverName = serverName;
		ServerCore::domainName = domainName;
		ServerCore::port = port;

		ServerCore::isInitialized = true;

		Log::Print(
			"Created new server '" + serverName + "'!",
			"SERVER_INIT",
			LogType::LOG_SUCCESS);
	}

	bool ServerCore::HasInternet()
	{
		if (ServerCore::IsInitialized())
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
		const wstring testPageWide = ToWide(testPage);
		HINTERNET hInternet = InternetOpenW(
			ccast<LPWSTR>(testPageWide.c_str()),
			INTERNET_OPEN_TYPE_DIRECT,
			NULL,
			NULL,
			0);

		if (!hInternet)
		{
			Log::Print(
				"Failed to check for internet access because InternetOpenW returned false!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		HINTERNET hUrl = InternetOpenUrlW(
			hInternet,
			wstring(testPage.begin(), testPage.end()).c_str(),
			NULL,
			0,
			INTERNET_FLAG_NO_UI,
			0);

		bool result = (hUrl != nullptr);

		if (hUrl) InternetCloseHandle(hUrl);
		if (hInternet) InternetCloseHandle(hInternet);

		return result;
	}

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
						CloudFlare::IsHealthy(0)
						&& CloudFlare::IsHealthy(1)
						&& CloudFlare::IsHealthy(2)
						&& CloudFlare::IsHealthy(3);

					if (!isHealthy)
					{
						bool isOnline =
							CloudFlare::IsTunnelAlive()
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
		const vector<u8>& reason)
	{
		//TODO: use callback

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
			|| scast<SOCKET>(targetSocket) == INVALID_SOCKET)
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
					|| scast<SOCKET>(c->connectionSocket) == INVALID_SOCKET)
				{
					Log::Print(
						"Failed to disconnect target via socket for server '" + serverName + "' because the stored socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_ERROR,
						2);
				}
				else
				{
					SOCKET socket = scast<SOCKET>(c->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
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
					|| scast<SOCKET>(c->connectionSocket) == INVALID_SOCKET)
				{
					Log::Print(
						"Failed to disconnect target via socket for server '" + serverName + "' because the stored socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_ERROR,
						2);
				}
				else
				{
					SOCKET socket = scast<SOCKET>(c->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
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
		const vector<u8>& reason)
	{
		//TODO: use callback

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
					|| scast<SOCKET>(c->connectionSocket) == INVALID_SOCKET)
				{
					Log::Print(
						"Couldn't close socket for target via IP '" + targetIP + "' for server '" + serverName + "' because the socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_WARNING);
				}
				else
				{
					SOCKET socket = scast<SOCKET>(c->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
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
					|| scast<SOCKET>(c->connectionSocket) == INVALID_SOCKET)
				{
					Log::Print(
						"Couldn't close socket for target via IP '" + targetIP + "' for server '" + serverName + "' because the socket is unassigned or invalid!",
						"DISCONNECT_TARGET",
						LogType::LOG_WARNING);
				}
				else
				{
					SOCKET socket = scast<SOCKET>(c->connectionSocket);
					shutdown(socket, SD_BOTH);
					closesocket(socket);
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

	void ServerCore::DisconnectListener(const vector<u8>& reason)
	{
		//TODO: use callback

		if (listenerSocket == 0 
			|| scast<SOCKET>(listenerSocket) == INVALID_SOCKET)
		{
			Log::Print(
				"Failed to disconnect listener for server '" + serverName + "' because the server has no listener socket or it is invalid!",
				"LISTENER_DISCONNECT",
				LogType::LOG_ERROR,
				2);

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
				&& scast<SOCKET>(s->connectionSocket) != INVALID_SOCKET)
			{
				SOCKET socket = scast<SOCKET>(s->connectionSocket);
				shutdown(socket, SD_BOTH);
				closesocket(socket);
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

		SOCKET socket = scast<SOCKET>(listenerSocket);
		shutdown(socket, SD_BOTH);
		closesocket(socket);
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

	void ServerCore::CancelAllPackets(const vector<u8>& reason)
	{
		//TODO: use callback

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
				LogType::LOG_INFO);

			unlock_m(m_connectSockets);

			return;
		}

		connectSockets.swap(sockets);
		unlock_m(m_connectSockets);

		for (auto& s : sockets)
		{
			lockwait_m(s->m_connection);

			if (s->connectionSocket != 0
				&& scast<SOCKET>(s->connectionSocket) != INVALID_SOCKET)
			{
				SOCKET socket = scast<SOCKET>(s->connectionSocket);
				shutdown(socket, SD_BOTH);
				closesocket(socket);
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
}

wstring ToWide(const string& input)
{
	if (input.empty()) return wstring();

	int size_needed = MultiByteToWideChar(
		CP_UTF8,
		0,
		input.c_str(),
		-1,
		nullptr,
		0);

	wstring wstr(size_needed - 1, 0);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		input.c_str(),
		-1,
		wstr.data(),
		size_needed);

	return wstr;
}