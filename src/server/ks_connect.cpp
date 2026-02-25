//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <atomic>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cerrno>
#endif

#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <sstream>
#include <array>
#include <chrono>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"
#include "KalaHeaders/string_utils.hpp"

#include "server/ks_connect.hpp"
#include "server/ks_server.hpp"
#include "server/ks_cloudflare.hpp"
#include "server/ks_response.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaCore::FromVar;
using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaThread::lockwait_m;
using KalaHeaders::KalaThread::unlock_m;
using KalaHeaders::KalaThread::joinable_thread;
using KalaHeaders::KalaThread::abool;

using KalaHeaders::KalaString::TrimString;
using KalaHeaders::KalaString::ToLowerString;
using KalaHeaders::KalaString::ToUpperString;

using KalaServer::Server::Response;
using KalaServer::Server::Connection;
using KalaServer::Server::ResponseType;
using KalaServer::Server::ContentType;
using KalaServer::Server::ResponseData;
using KalaServer::Core::KalaServerCore;

using std::memory_order_acquire;
using std::memory_order_release;
using std::string;
using std::string_view;
using std::to_string;
using std::this_thread::sleep_for;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::unique_ptr;
using std::make_unique;
using std::unordered_map;
using std::istringstream;
using std::array;
using std::chrono::steady_clock;
using std::vector;

using u16 = uint16_t;

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

static string serverIPDomain{};
static string serverIPPortDomain{};

constexpr array<string_view, 8> allowedDuplicateHeaders
{
	"accept",
	"accept-encoding",
	"accept-language",
	"cache-control",
	"pragma",
	"warning",
	"via",
	"x-forwarded-for"
};

constexpr string_view response_success = 
	"<html><body>\n"
	"    <h1>linux webserver lul</h1>\n"
	"        <p><a href=\"https://github.com/Lost-Empire-Entertainment/Websites\">\n"
	"            Check out the Website source code</a></p>\n"
	"        <p><a href=\"https://github.com/KalaKit/KalaServer\">\n"
	"            Check out the KalaKit server source code</a></p>\n"
	"</body></html>";

static string ReturnErrorBody(string_view error, ResponseType type)
{
	return 
		"<html><body>\n"
		"    <h1>" + string(Response::ResponseTypeToString(type)) + "</h1>\n"
		"        <p>" + string(error) + "</p>\n"
		"</body></html>";
}

namespace KalaServer::Server
{
	static unique_ptr<Connection> listenerSocket{};
	static mutex m_listenerSocket{};

	static vector<unique_ptr<Connection>> connectSockets{};
	static mutex m_connectSockets{};

	bool Connect::CreateListenerSocket()
	{
		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to create new listener socket for server because the server is not running or not ready!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (TIME_OUT_PERIOD_M == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because the TIME_OUT_PERIOD_M value was set to 0!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (ROLLING_WINDOW_TIMER_S == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because the ROLLING_WINDOW_TIMER_S value was set to 0!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (MIN_PACKET_SPACING_MS == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because the MIN_PACKET_SPACING_MS value was set to 0!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (ACCEPT_WAIT_TIME_S == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because the ACCEPT_WAIT_TIME_S value was set to 0!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (MAX_TOTAL_PAYLOAD_SIZE_BYTES == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because the MAX_TOTAL_PAYLOAD_SIZE_BYTES value was set to 0!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (UNASSIGNED_SOCKET_VALUE < 8192)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because the UNASSIGNED_SOCKET_VALUE value was set below 8192!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (MAX_ACTIVE_CONNECTIONS == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because the MAX_ACTIVE_CONNECTIONS value was set to 0!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		Log::Print(
			"Creating a new listener socket for server '" + string(ServerCore::GetServerName()) + "'!",
			"LISTENER_INIT",
			LogType::LOG_INFO);

		//
		// CHECK FOR EXISTING SOCKET
		//

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
					"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because the server already has a listener socket!",
					"LISTENER_INIT",
					LogType::LOG_ERROR,
					2);

				unlock_m(m_listenerSocket);

				return false;
			}
		}
		unlock_m(m_listenerSocket);

		//
		// CREATE, BIND AND LISTEN
		//

#ifdef _WIN32
		int iResult{};

		SOCKET listener = socket(
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP);

		if (listener == INVALID_SOCKET)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because socket creation failed! Reason: " + KalaServerCore::ErrorToString(WSAGetLastError()),
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		serverAddress.sin_port = htons(ServerCore::GetServerPort());

		int opt = 1;

		int result_reuse_addr = setsockopt(
			listener,
			SOL_SOCKET,
			SO_REUSEADDR,
			(const char*)&opt,
			sizeof(opt));

		if (result_reuse_addr == SOCKET_ERROR)
		{
			Log::Print(
				"Failed to create new listener socket because SO_REUSEADDR couldn't be set!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			closesocket(listener);

			return false;
		}

		if (bind(
			listener,
			(sockaddr*)&serverAddress,
			sizeof(serverAddress)) == SOCKET_ERROR)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because socket bind failed! Reason: " + KalaServerCore::ErrorToString(WSAGetLastError()),
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			closesocket(listener);

			return false;
		}

		if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because socket listen failed! Reason: " + KalaServerCore::ErrorToString(WSAGetLastError()),
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			closesocket(listener);

			return false;
		}
#else
		int listener = socket(AF_INET, SOCK_STREAM, 0);
		if (listener < 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because socket creation failed! Reason: " + KalaServerCore::ErrorToString(errno),
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		serverAddress.sin_port = htons(ServerCore::GetServerPort());

		int opt = 1;

		int result_reuse_addr = setsockopt(
			listener,
			SOL_SOCKET,
			SO_REUSEADDR,
			&opt,
			sizeof(opt));

		if (result_reuse_addr < 0)
		{
			Log::Print(
				"Failed to create new listener socket because SO_REUSEADDR couldn't be set!",
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			close(listener);

			return false;
		}

		if (bind(
			listener,
			(sockaddr*)&serverAddress,
			sizeof(serverAddress)) < 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because socket bind failed! Reason: " + KalaServerCore::ErrorToString(errno),
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			close(listener);

			return false;
		}

		if (listen(listener, SOMAXCONN) < 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + string(ServerCore::GetServerName()) + "' because socket listen failed! Reason: " + KalaServerCore::ErrorToString(errno),
				"LISTENER_INIT",
				LogType::LOG_ERROR,
				2);

			close(listener);

			return false;
		}
#endif

		//
		// STORE LISTENER SOCKET
		//

		unique_ptr<Connection> newListenerSocket = make_unique<Connection>();
		Connection* localListener = newListenerSocket.get();

		lockwait_m(m_listenerSocket);
		listenerSocket = std::move(newListenerSocket);

		listenerSocket->connectionSocket.store(FromVar(listener), memory_order_release);
		listenerSocket->isRunning.store(true, memory_order_release);

		unlock_m(m_listenerSocket);

		if (serverIPDomain.empty()) serverIPDomain = string(ServerCore::GetServerIP());
		if (serverIPPortDomain.empty())
		{
			serverIPPortDomain = 
				string(ServerCore::GetServerIP())
				+ ":"
				+ to_string(ServerCore::GetServerPort());
		}

		//
		// START LISTENER SOCKET THREAD
		//

		Log::Print(
			"Created a new listener socket for server '" + string(ServerCore::GetServerName()) + "', starting the listener loop!",
			"LISTENER_INIT",
			LogType::LOG_SUCCESS);

		localListener->connectionThread = joinable_thread([localListener]
			{
				while (true)
				{
					if (!localListener->isRunning.load(memory_order_acquire))
					{
						Log::Print(
							"Listener socket for server '" + string(ServerCore::GetServerName()) + "' has been shut down.",
							"LISTENER_LOOP",
							LogType::LOG_INFO);

						return;
					}

					if (!ServerCore::IsHealthy())
					{
						Log::Print(
							"Server is not healthy, waiting until trying again.",
							"LISTENER_LOOP",
							LogType::LOG_INFO);

						sleep_for(seconds(SERVER_HEALTH_SLEEP_S));

						continue;
					}

					//
					// REMOVE DISABLED SOCKETS
					//

					vector<unique_ptr<Connection>> finishedConnections{};

					lockwait_m(m_connectSockets);
					for (auto it = connectSockets.begin(); it != connectSockets.end();)
					{
						if (!(*it)->isRunning.load(memory_order_acquire))
						{
							finishedConnections.push_back(std::move(*it));
							it = connectSockets.erase(it);
						}
						else ++it;
					}
					unlock_m(m_connectSockets);

					for (auto& conn : finishedConnections)
					{
						if (conn->connectionThread.joinable()) conn->connectionThread.join();

#ifdef _WIN32
						SOCKET cs = ToVar<SOCKET>(conn->connectionSocket.exchange(UNASSIGNED_SOCKET_VALUE));
						if (cs != UNASSIGNED_SOCKET_VALUE)
						{
							shutdown(cs, SD_BOTH);
							closesocket(cs);
						}
#else
						int cs = ToVar<int>(conn->connectionSocket.exchange(UNASSIGNED_SOCKET_VALUE));
						if (cs != UNASSIGNED_SOCKET_VALUE)
						{
							shutdown(cs, SHUT_RDWR);
							close(cs);
						}
#endif
					}

					//
					// VERIFY CONNECTING SOCKET
					//

#ifdef _WIN32
					ksocket lsock = ToVar<SOCKET>(localListener->connectionSocket.load(memory_order_acquire));

					sockaddr_storage clientAddress{};
					int addressLength = sizeof(clientAddress);

					ksocket client = accept(
						lsock,
						rcast<sockaddr*>(&clientAddress),
						&addressLength);

					if (client == invalid_socket)
					{	
						DWORD err = WSAGetLastError();

						//interrupted, try again
						if (err == WSAEINTR) continue;
						if (err == WSAETIMEDOUT)
						{
							Log::Print(
								"Accept timed out.",
								"LISTENER_LOOP",
								LogType::LOG_INFO);

							continue;
						}
						if (err == WSAECONNRESET
							|| err == WSAECONNABORTED)
						{
							Log::Print(
								"Connection was closed abruptly by client during accept.",
								"LISTENER_LOOP",
								LogType::LOG_INFO);

							continue;
						}

						Log::Print(
							"Failed to accept new connection! Reason: " + KalaServerCore::ErrorToString(err),
							"LISTENER_LOOP",
							LogType::LOG_ERROR,
							2);

						continue;
					}

					DWORD timeout = ACCEPT_WAIT_TIME_S * 1000;
					int result_rcv_time = setsockopt(
						client,
						SOL_SOCKET,
						SO_RCVTIMEO,
						(char*)&timeout,
						sizeof(timeout));

					if (result_rcv_time == SOCKET_ERROR)
					{
						Log::Print(
							"Failed to accept new connection because SO_RCVTIMEO couldn't be set!",
							"LISTENER_LOOP",
							LogType::LOG_ERROR,
							2);

						shutdown(client, SD_BOTH);
						closesocket(client);

						continue;
					}

					int result_snd_time = setsockopt(
						client,
						SOL_SOCKET,
						SO_SNDTIMEO,
						(char*)&timeout,
						sizeof(timeout));

					if (result_snd_time == SOCKET_ERROR)
					{
						Log::Print(
							"Failed to accept new connection because SO_SNDTIMEO couldn't be set!",
							"LISTENER_LOOP",
							LogType::LOG_ERROR,
							2);

						shutdown(client, SD_BOTH);
						closesocket(client);

						continue;
					}

					BOOL no_delay = TRUE;

					int result_no_delay = setsockopt(
						client,
						IPPROTO_TCP,
						TCP_NODELAY,
						(char*)&no_delay,
						sizeof(no_delay));

					if (result_no_delay == SOCKET_ERROR)
					{
						Log::Print(
							"Failed to accept new connection because TCP_NODELAY couldn't be set!",
							"LISTENER_LOOP",
							LogType::LOG_ERROR,
							2);

						shutdown(client, SD_BOTH);
						closesocket(client);

						continue;
					}
#else 
					ksocket lsock = ToVar<int>(localListener->connectionSocket.load(memory_order_acquire));

					sockaddr_storage clientAddress{};
					socklen_t addressLength = sizeof(clientAddress);

					ksocket client = accept(
						lsock,
						rcast<sockaddr*>(&clientAddress),
						&addressLength);

					if (client < 0)
					{
						//interrupted, try again
						if (errno == EINTR) continue;
						if (errno == EAGAIN
							|| errno == EWOULDBLOCK)
						{
							Log::Print(
								"Accept timed out.",
								"LISTENER_LOOP",
								LogType::LOG_INFO);

							continue;
						}
						if (errno == ECONNRESET
							|| errno == ECONNABORTED)
						{
							Log::Print(
								"Connection was closed abruptly by client during accept.",
								"LISTENER_LOOP",
								LogType::LOG_INFO);

							continue;
						}

						Log::Print(
							"Failed to accept new connection! Reason: " + KalaServerCore::ErrorToString(errno),
							"LISTENER_LOOP",
							LogType::LOG_ERROR,
							2);

						continue;
					}

					struct timeval timeout{};
					timeout.tv_sec = ACCEPT_WAIT_TIME_S;
					timeout.tv_usec = 0;

					int result_rcv_time = setsockopt(
						client,
						SOL_SOCKET,
						SO_RCVTIMEO,
						&timeout,
						sizeof(timeout));

					if (result_rcv_time < 0)
					{
						Log::Print(
							"Failed to accept new connection because SO_RCVTIMEO couldn't be set!",
							"LISTENER_LOOP",
							LogType::LOG_ERROR,
							2);

						shutdown(client, SHUT_RDWR);
						close(client);

						continue;
					}

					int result_snd_time = setsockopt(
						client,
						SOL_SOCKET,
						SO_SNDTIMEO,
						&timeout,
						sizeof(timeout));

					if (result_snd_time < 0)
					{
						Log::Print(
							"Failed to accept new connection because SO_SNDTIMEO couldn't be set!",
							"LISTENER_LOOP",
							LogType::LOG_ERROR,
							2);

						shutdown(client, SHUT_RDWR);
						close(client);

						continue;
					}

					int no_delay = 1;

					int result_no_delay = setsockopt(
						client,
						IPPROTO_TCP,
						TCP_NODELAY,
						&no_delay,
						sizeof(no_delay));

					if (result_no_delay < 0)
					{
						Log::Print(
							"Failed to accept new connection because TCP_NODELAY couldn't be set!",
							"LISTENER_LOOP",
							LogType::LOG_ERROR,
							2);

						shutdown(client, SHUT_RDWR);
						close(client);

						continue;
					}
#endif

					//
					// GET USER IP (get ip via http headers if using proxy or tunnel)
					//

					char ipStr[INET6_ADDRSTRLEN]{};

					if (clientAddress.ss_family == AF_INET)
					{
						auto* addr = rcast<sockaddr_in*>(&clientAddress);
						if (!inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr)))
						{
							ipStr[0] = '\0';

							Log::Print(
								"Failed accept new connection because ipv4 couldn't be found from it!",
								"LISTENER_LOOP",
								LogType::LOG_ERROR,
								2);
						}
					}
					else if (clientAddress.ss_family == AF_INET6)
					{
						auto* addr = rcast<sockaddr_in6*>(&clientAddress);
						if (!inet_ntop(AF_INET6, &addr->sin6_addr, ipStr, sizeof(ipStr)))
						{
							ipStr[0] = '\0';

							Log::Print(
								"Failed accept new connection because ipv6 couldn't be found from it!",
								"LISTENER_LOOP",
								LogType::LOG_ERROR,
								2);
						}
					}
					else snprintf(ipStr, sizeof(ipStr), "UNKNOWN");

					Log::Print(
						"[ " + string(ipStr) + " ] Connection received, verifying socket.",
						"LISTENER_LOOP",
						LogType::LOG_INFO);

					string connectionIP = "[ " + string(ipStr) + " ] ";
					string sendMsg{};

					//
					// CHECK USER COUNT, REJECT IF MAX
					//

					lockwait_m(m_connectSockets);

					if (connectSockets.size() >= MAX_ACTIVE_CONNECTIONS)
					{
						sendMsg = "Max user count '" + to_string(MAX_ACTIVE_CONNECTIONS) + "' was reached, cannot accept new connections!";

						Log::Print(
							connectionIP + sendMsg,
							"LISTENER_LOOP",
							LogType::LOG_WARNING);

						unlock_m(m_connectSockets);

						Response::SendResponse({
							.responseType = ResponseType::R_503,
							.contentType = ContentType::CT_HTML,
							.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
							.responseBody = 
								ReturnErrorBody(sendMsg,
								ResponseType::R_503),
							.connectionSocket = FromVar(client)
						});

						continue;
					}

					unlock_m(m_connectSockets);

					//
					// CHECK IF IP IS NOT BANNED
					//

					mutex& m_bannedIPs = ServerCore::GetBannedIPsMutex();
					lockwait_m(m_bannedIPs);

					bool foundBannedUser{};
					for (const auto& u : ServerCore::GetBannedIPs())
					{
						if (ipStr == u.targetIP)
						{
							Log::Print(
								connectionIP + "Banned user tried to reconnect to server.",
								"LISTENER_LOOP",
								LogType::LOG_INFO);

							unlock_m(m_bannedIPs);

							Response::SendResponse({
								.responseType = ResponseType::R_418,
								.contentType = ContentType::CT_HTML,
								.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
								.responseBody = 
								ReturnErrorBody("Get banned nerd",
								ResponseType::R_418),
								.connectionSocket = FromVar(client)
							});

							foundBannedUser = true;

							break;
						}
					}
					
					if (foundBannedUser) continue;

					unlock_m(m_bannedIPs);

					//
					// STORE CLIENT SOCKET
					//

					lockwait_m(m_connectSockets);

					unique_ptr<Connection> c = make_unique<Connection>();
					c->isRunning.store(true, memory_order_release);
					c->connectionSocket.store(FromVar(client), memory_order_release);

					c->connectionIP = ipStr;
				
					Connection* raw = c.get();
					connectSockets.push_back(std::move(c));

					unlock_m(m_connectSockets);

					//
					// START CLIENT SOCKET
					//

					raw->connectionThread = joinable_thread([&raw]
						{
							string readBuffer{};

							string connectionIP = "[ " + raw->connectionIP + " ] ";
							string sendMsg{};

							while (raw->isRunning.load(memory_order_acquire))
							{
#ifdef _WIN32
								ksocket csock = ToVar<SOCKET>(raw->connectionSocket.load(memory_order_acquire));

								char buffer[2048]{};
								int bytesReceived = recv(
									csock, 
									buffer, 
									sizeof(buffer), 
									0);

								if (bytesReceived == SOCKET_ERROR)
								{
									DWORD err = WSAGetLastError();

									//interrupted, try again
									if (err == WSAEINTR) continue;
									if (err == WSAETIMEDOUT)
									{
										Log::Print(
											connectionIP + "BytesReceived recv read timed out.",
											"CONNECTION_LOOP",
											LogType::LOG_INFO);

										raw->isRunning.store(false, memory_order_release);

										continue;
									}
									if (err == WSAECONNRESET
										|| err == WSAECONNABORTED)
									{
										Log::Print(
											connectionIP + "Connection was closed abruptly by client during bytesReceived recv read.",
											"CONNECTION_LOOP",
											LogType::LOG_INFO);

										raw->isRunning.store(false, memory_order_release);

										continue;
									}
									
									Log::Print(
										connectionIP + "BytesReceived recv read failed! Reason: " + KalaServerCore::ErrorToString(err),
										"CONNECTION_LOOP",
										LogType::LOG_WARNING);

									raw->isRunning.store(false, memory_order_release);

									continue;
								}

								if (bytesReceived == 0)
								{
									Log::Print(
										connectionIP + "Connection was closed during bytesReceived recv read.",
										"CONNECTION_LOOP",
										LogType::LOG_INFO);

									raw->isRunning.store(false, memory_order_release);

									continue;
								}
#else
								ksocket csock = ToVar<int>(raw->connectionSocket.load(memory_order_acquire));

								char buffer[2048]{};
								int bytesReceived = recv(
									csock, 
									buffer, 
									sizeof(buffer), 
									0);

								if (bytesReceived < 0)
								{
									//interrupted, try again
									if (errno == EINTR) continue;
									if (errno == EAGAIN
										|| errno == EWOULDBLOCK)
									{
										Log::Print(
											connectionIP + "BytesReceived recv read timed out.",
											"CONNECTION_LOOP",
											LogType::LOG_INFO);

										raw->isRunning.store(false, memory_order_release);

										continue;
									}
									if (errno == ECONNRESET
										|| errno == ECONNABORTED)
									{
										Log::Print(
											connectionIP + "Connection was closed abruptly by client during bytesReceived recv read.",
											"CONNECTION_LOOP",
											LogType::LOG_INFO);

										raw->isRunning.store(false, memory_order_release);

										continue;
									}
									
									Log::Print(
										connectionIP + "BytesReceived recv read failed! Reason: " + KalaServerCore::ErrorToString(errno),
										"CONNECTION_LOOP",
										LogType::LOG_WARNING);

									raw->isRunning.store(false, memory_order_release);

									continue;
								}

								if (bytesReceived == 0)
								{
									Log::Print(
										connectionIP + "Connection was closed by client during bytesReceived recv read.",
										"CONNECTION_LOOP",
										LogType::LOG_INFO);

									raw->isRunning.store(false, memory_order_release);

									continue;
								}
#endif
								
								readBuffer.append(buffer, bytesReceived);

								if (readBuffer.size() > MAX_TOTAL_PAYLOAD_SIZE_BYTES)
								{
									sendMsg = "Max payload size '" + to_string(MAX_TOTAL_PAYLOAD_SIZE_BYTES) + "' was reached, cannot accept bigger payload!";

									Log::Print(
										connectionIP + sendMsg,
										"CONNECTION_SOCKET",
										LogType::LOG_WARNING);

									Response::SendResponse({
										.responseType = ResponseType::R_413,
										.contentType = ContentType::CT_HTML,
										.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
										.responseBody = 
											ReturnErrorBody(sendMsg,
											ResponseType::R_413),
										.connection = raw
									});

									break;
								}

								while (true)
								{
									auto headerEnd = readBuffer.find("\r\n\r\n");

									//incomplete headers
									if (headerEnd == string::npos) break;

									size_t headerSize = headerEnd + 4;

									//extract header block
									string headerblock = readBuffer.substr(0, headerSize);

									//parse content length

									size_t contentLength{};
									auto clPos = headerblock.find("Content-Length:");
									if (clPos != string::npos)
									{
										size_t valueStart = clPos + 15;
										size_t valueEnd = headerblock.find("\r\n", valueStart);
										string value = headerblock.substr(valueStart, valueEnd - valueStart);

										contentLength = scast<size_t>(stoul(value));
									}

									size_t totalRequired = headerSize + contentLength;

									//wait until full body is present (if any)
									if (readBuffer.size() < totalRequired) break;

									string fullRequest = readBuffer.substr(0, totalRequired);

									string newLine = 
										!fullRequest.empty() && fullRequest.back() != '\n'
										? "\n"
										: "";

									if (fullRequest.size() > MAX_TOTAL_PAYLOAD_SIZE_BYTES)
									{
										sendMsg = "Max payload size '" + to_string(MAX_TOTAL_PAYLOAD_SIZE_BYTES) + "' was reached, cannot accept bigger payload!";

										Log::Print(
											connectionIP + sendMsg,
											"CONNECTION_SOCKET",
											LogType::LOG_WARNING);
										
										Response::SendResponse({
											.responseType = ResponseType::R_413,
											.contentType = ContentType::CT_HTML,
											.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
											.responseBody = 
												ReturnErrorBody(sendMsg,
												ResponseType::R_413),
											.connection = raw
										});

										break;
									}

									//remove processed request from buffer
									readBuffer.erase(0, totalRequired);

									if (readBuffer.size() > 0)
									{
										Log::Print(
											"There is '" + to_string(readBuffer.size()) + "' bytes of data remaining after removing the total required bytes from the readbuffer.",
											"CONNECTION_LOOP",
											LogType::LOG_INFO);
									}

									string fullRequestToLog = fullRequest;
									if (fullRequestToLog.ends_with("\r\n\r\n")) fullRequestToLog.erase(fullRequestToLog.size() - 4);
									if (fullRequestToLog.ends_with("\r\n")) fullRequestToLog.erase(fullRequestToLog.size() - 2);

									Log::Print(
											"------------------------------\n"
											+ connectionIP + "Parsing client request (" + to_string(bytesReceived) + " bytes):\n"
											+ fullRequestToLog
											+ "\n------------------------------");

									//
									// PARSE HEADER AND BODY CONTENT
									//

									RequestData req{};

									bool foundGetLineError{};
									{
										size_t headerEnd = fullRequest.find("\r\n\r\n");
										string headerBlock = fullRequest.substr(0, headerEnd);

										req.body = (headerEnd != string::npos)
											? fullRequest.substr(headerEnd + 4)
											: "";

										istringstream stream(headerBlock);
										string line{};

										if (getline(stream, line))
										{
											if (!line.empty()
												&& line.back() == '\r')
											{
												line.pop_back();
											}

											istringstream firstLine(line);
											firstLine >> req.method >> req.domainRoute.route >> req.httpVersion;

											req.method = ToUpperString(req.method);
											req.domainRoute.route = ToLowerString(req.domainRoute.route);
											req.httpVersion = ToUpperString(req.httpVersion);

											if (req.method.empty())
											{
												sendMsg = "Payload did not contain any method!";

												Log::Print(
													connectionIP + sendMsg,
													"CONNECTION_SOCKET",
													LogType::LOG_WARNING);

												Response::SendResponse({
													.responseType = ResponseType::R_400,
													.contentType = ContentType::CT_HTML,
													.responseBody = 
														ReturnErrorBody(sendMsg,
														ResponseType::R_400),
													.connection = raw
												});

												break;
											}
											if (req.method != "GET")
											{
												sendMsg = "Method '" + req.method + "' is not supported!";

												Log::Print(
													connectionIP + sendMsg,
													"CONNECTION_SOCKET",
													LogType::LOG_WARNING);

												Response::SendResponse({
													.responseType = ResponseType::R_405,
													.contentType = ContentType::CT_HTML,
													.responseBody = 
														ReturnErrorBody(sendMsg,
														ResponseType::R_405),
													.connection = raw
												});

												break;
											}

											if (req.domainRoute.route.empty())
											{
												sendMsg = "Payload did not contain a route!";

												Log::Print(
													connectionIP + sendMsg,
													"CONNECTION_SOCKET",
													LogType::LOG_WARNING);

												Response::SendResponse({
													.responseType = ResponseType::R_400,
													.contentType = ContentType::CT_HTML,
													.responseBody = 
														ReturnErrorBody(sendMsg,
														ResponseType::R_400),
													.connection = raw
												});

												break;
											}

											if (req.httpVersion.empty())
											{
												sendMsg = "Payload did not contain any http version!";

												Log::Print(
													connectionIP + sendMsg,
													"CONNECTION_SOCKET",
													LogType::LOG_WARNING);

												Response::SendResponse({
													.responseType = ResponseType::R_400,
													.contentType = ContentType::CT_HTML,
													.responseBody = 
														ReturnErrorBody(sendMsg,
														ResponseType::R_400),
													.connection = raw
												});

												break;
											}
											if (req.httpVersion != "HTTP/1.1")
											{
												sendMsg = "HTTP version '" + req.httpVersion + "' is not supported!";

												Log::Print(
													connectionIP + sendMsg,
													"CONNECTION_SOCKET",
													LogType::LOG_WARNING);

												Response::SendResponse({
													.responseType = ResponseType::R_400,
													.contentType = ContentType::CT_HTML,
													.responseBody = 
														ReturnErrorBody(sendMsg,
														ResponseType::R_400),
													.connection = raw
												});

												break;
											}
										}

										while (getline(stream, line))
										{
											if (!line.empty()
												&& line.back() == '\r')
											{
												line.pop_back();
											}

											if (line.empty()) continue;

											size_t colon = line.find(':');
											if (colon == string::npos)
											{
												sendMsg = "Payload headers are malformed!";

												Log::Print(
													connectionIP + sendMsg,
													"CONNECTION_SOCKET",
													LogType::LOG_WARNING);

												Response::SendResponse({
													.responseType = ResponseType::R_400,
													.contentType = ContentType::CT_HTML,
													.responseBody = 
														ReturnErrorBody(sendMsg,
														ResponseType::R_400),
													.connection = raw
												});

												foundGetLineError = true;
												break;
											}

											string key = line.substr(0, colon);
											string value = line.substr(colon + 1);

											key = ToLowerString(TrimString(key));
											value = TrimString(value);

											if (key == "host")
											{
												if (!req.domainRoute.domain.empty())
												{
													sendMsg = "Payload contained more than one 'host' field!";

													Log::Print(
														connectionIP + sendMsg,
														"CONNECTION_SOCKET",
														LogType::LOG_WARNING);

													Response::SendResponse({
														.responseType = ResponseType::R_400,
														.contentType = ContentType::CT_HTML,
														.responseBody = 
														ReturnErrorBody(sendMsg,
														ResponseType::R_400),
														.connection = raw
													});

													foundGetLineError = true;
													break;
												}

												req.domainRoute.domain = ToLowerString(value);
											}
											else
											{
												auto it = req.headers.find(key);
												if (it != req.headers.end())
												{
													if (ContainsValue(allowedDuplicateHeaders, key))
													{
														it->second += ", " + value;
													}
													else
													{
														sendMsg = "Payload contained more than one '" + key + "' field!";

														Log::Print(
															connectionIP + sendMsg,
															"CONNECTION_LOOP",
															LogType::LOG_ERROR,
															2);

														Response::SendResponse({
															.responseType = ResponseType::R_400,
															.contentType = ContentType::CT_HTML,
															.responseBody = 
																ReturnErrorBody(sendMsg,
																ResponseType::R_400),
															.connection = raw
														});

														foundGetLineError = true;
														break;
													}
												}
												else req.headers.emplace(std::move(key), std::move(value));
											}
										}
									}

									if (foundGetLineError) break;

									//
									// VERIFY HOST
									//

									if (req.domainRoute.domain.empty())
									{
										sendMsg = "Payload did not contain host!";

										Log::Print(
											connectionIP + sendMsg,
											"CONNECTION_SOCKET",
											LogType::LOG_WARNING);

										Response::SendResponse({
											.responseType = ResponseType::R_400,
											.contentType = ContentType::CT_HTML,
											.responseBody = 
												ReturnErrorBody(sendMsg,
												ResponseType::R_400),
											.connection = raw
										});

										break;
									}

									if (req.domainRoute.domain.starts_with("http://")) req.domainRoute.domain.erase(0, 7);
									if (req.domainRoute.domain.starts_with("https://")) req.domainRoute.domain.erase(0, 8);
									if (req.domainRoute.domain.starts_with("www.")) req.domainRoute.domain.erase(0, 4);

									bool foundDomain{};
									if (req.domainRoute.domain == serverIPDomain 
										|| req.domainRoute.domain == serverIPPortDomain)
									{
										foundDomain = true;
									}
									else
									{
										size_t dcolon = req.domainRoute.domain.find(':');
										if (dcolon != string::npos) req.domainRoute.domain.erase(dcolon);

										for (const auto& d : ServerCore::GetServerDomains())
										{
											if (req.domainRoute.domain == d)
											{
												foundDomain = true;
												break;
											}
										}
									}
									
									if (!foundDomain)
									{
										sendMsg = "Host '" + req.domainRoute.domain + "' was not found!";

										Log::Print(
											connectionIP + sendMsg,
											"CONNECTION_SOCKET",
											LogType::LOG_WARNING);

										Response::SendResponse({
											.responseType = ResponseType::R_400,
											.contentType = ContentType::CT_HTML,
											.responseBody = 
												ReturnErrorBody(sendMsg,
												ResponseType::R_400),
											.connection = raw
										});

										break;
									}
										
									//
									// PARSE ROUTE
									//

									if (req.domainRoute.route.starts_with("http://")) req.domainRoute.route.erase(0, 7);
									if (req.domainRoute.route.starts_with("https://")) req.domainRoute.route.erase(0, 8);
									if (req.domainRoute.route.starts_with("www.")) req.domainRoute.route.erase(0, 4);
									if (req.domainRoute.route.starts_with(serverIPPortDomain)) req.domainRoute.route.erase(0, serverIPPortDomain.size());
									if (req.domainRoute.route.starts_with(serverIPDomain)) req.domainRoute.route.erase(0, serverIPDomain.size());

									if (!req.domainRoute.route.starts_with('/')) req.domainRoute.route.insert(req.domainRoute.route.begin(), '/');

									mutex& m_routes = ServerCore::GetRoutesMutex();
									mutex& m_blacklistedKeywords = ServerCore::GetBlacklistedKeywordsMutex();

									lockwait_m(m_routes);
									lockwait_m(m_blacklistedKeywords);

									const vector<string>& blacklistedKeywords = ServerCore::GetBlacklistedKeywords();
									const vector<BannedIP>& bannedIPs = ServerCore::GetBannedIPs();
									const vector<DomainRoute>& routes = ServerCore::GetRoutes();

									string blacklistedKeyword{};
									for (const auto& b : ServerCore::GetBlacklistedKeywords())
									{
										if (req.domainRoute.route.find(b) != string::npos)
										{
											blacklistedKeyword = b;
											break;
										}
									}
									if (!blacklistedKeyword.empty())
									{
										ServerCore::BanIP(raw->connectionIP);

										Log::Print(
											"[ " + raw->connectionIP + " ] User was banned for trying to access route via blacklisted keyword '" + blacklistedKeyword + "'",
											"CONNECTION_LOOP",
											LogType::LOG_INFO);

										unlock_m(m_blacklistedKeywords);
										unlock_m(m_routes);

										Response::SendResponse({
											.responseType = ResponseType::R_418,
											.contentType = ContentType::CT_HTML,
											.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
											.responseBody = 
												ReturnErrorBody("Get banned nerd",
												ResponseType::R_418),
											.connection = raw
										});

										break;
									}

									bool foundValidRoute{};
									for (const auto& r : routes)
									{
										if (foundValidRoute) break;

										if (r.route == req.domainRoute.route)
										{
											foundValidRoute = true;
											break;
										}
									}

									if (!foundValidRoute)
									{
										unlock_m(m_blacklistedKeywords);
										unlock_m(m_routes);

										sendMsg = "Route '" + req.domainRoute.route + "' was not found!";

										Log::Print(
											connectionIP + sendMsg,
											"CONNECTION_SOCKET",
											LogType::LOG_WARNING);

										Response::SendResponse({
											.responseType = ResponseType::R_404,
											.contentType = ContentType::CT_HTML,
											.responseBody = 
												ReturnErrorBody(sendMsg,
												ResponseType::R_404),
											.connection = raw
										});

										break;
									}

									unlock_m(m_blacklistedKeywords);
									unlock_m(m_routes);

									//
									// ALLOW CONNECTION
									//

									vector<OptionalSendType> optSendTypes{};
									for (const auto& [k, v] : req.headers)
									{
										if (k == "connection")
										{
											string lowerValue = ToLowerString(v);
											if (lowerValue.find("close") != string::npos)
											{
												optSendTypes.push_back(OptionalSendType::S_FORCE_CLOSE);
												break;
											}
										}
									}

									raw->requestData = std::move(req);

									Log::Print(
										"[ " + raw->connectionIP + " ] Connection verified, sending response.",
										"CONNECTION_LOOP",
										LogType::LOG_INFO);

									Response::SendResponse({
										.responseType = ResponseType::R_200,
										.contentType = ContentType::CT_HTML,
										.optionalSendTypes = optSendTypes,
										.responseBody = string(response_success),
										.connection = raw
									});
								}
							}
						});
				}
			});

		return true;
	}

	bool Connect::IsListenerRunning()
	{ 
		bool isRunning{};
		lockwait_m(m_listenerSocket);
		isRunning = listenerSocket->isRunning.load(memory_order_acquire);
		unlock_m(m_listenerSocket);

		return isRunning;
	}

	const Connection& Connect::GetListenerSocket() { return *listenerSocket.get(); }
	mutex& Connect::GetListenerMutex() { return m_listenerSocket; }

	const vector<const Connection*>& Connect::GetConnectSockets()
	{ 
		static vector<const Connection*> connectSocketView{};

		connectSocketView.clear();
		connectSocketView.reserve(connectSockets.size());

		for (const auto& c : connectSockets)
		{
			connectSocketView.push_back(c.get());
		}

		return connectSocketView;
	}
	mutex& Connect::GetConnectMutex() { return m_connectSockets; }

	void Connect::DisconnectConnectedUser(uintptr_t targetSocket)
	{
		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + string(ServerCore::GetServerName()) + "' because the server is not running or not ready!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		ksocket target = 
#ifdef _WIN32
			ToVar<SOCKET>(targetSocket);
#else
			ToVar<int>(targetSocket);
#endif

		if (target == invalid_socket)
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + string(ServerCore::GetServerName()) + "' because the socket is unassigned or invalid!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		unique_ptr<Connection> targetUser{};

		lockwait_m(m_connectSockets);
		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			ksocket sock =
#ifdef _WIN32
				ToVar<SOCKET>((*it)->connectionSocket.load(memory_order_acquire));
#else
				ToVar<int>((*it)->connectionSocket.load(memory_order_acquire));
#endif

			if (sock == target)
			{
				targetUser = std::move(*it);
				connectSockets.erase(it);

				break;
			}
		}
		unlock_m(m_connectSockets);

		if (targetUser == nullptr)
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + string(ServerCore::GetServerName()) + "' because the target socket was not found!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		targetUser->isRunning.store(false, memory_order_release);

#ifdef _WIN32
		ksocket cs = ToVar<SOCKET>(targetUser->connectionSocket.load(memory_order_acquire));
		if (cs != invalid_socket)
		{
			shutdown(cs, SD_BOTH);
			closesocket(cs);
		}
#else
		ksocket cs = ToVar<int>(targetUser->connectionSocket.load(memory_order_acquire));
		if (cs != invalid_socket)
		{
			shutdown(cs, SHUT_RDWR);
			close(cs);
		}
#endif

		if (targetUser->connectionThread.joinable()) targetUser->connectionThread.join();

		Log::Print(
			"Disconnected target via socket for server '" + string(ServerCore::GetServerName()) + "'!",
			"DISCONNECT_TARGET",
			LogType::LOG_SUCCESS);
	}

	void Connect::DisconnectConnectedUser(string_view targetIP)
	{
		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) + "' for server '" + string(ServerCore::GetServerName()) + "' because the server is not running or not ready!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsValidIP(targetIP))
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) + "' for server '" + string(ServerCore::GetServerName()) + "' because the IP structure is invalid!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		unique_ptr<Connection> targetUser{};

		lockwait_m(m_connectSockets);
		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			string connectionIP = (*it)->connectionIP;

			if (connectionIP == targetIP)
			{
				targetUser = std::move(*it);
				connectSockets.erase(it);

				break;
			}
		}
		unlock_m(m_connectSockets);

		if (targetUser == nullptr)
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) + "' for server '" + string(ServerCore::GetServerName()) + "' because the target IP was not found!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		targetUser->isRunning.store(false, memory_order_release);

#ifdef _WIN32
		ksocket cs = ToVar<SOCKET>(targetUser->connectionSocket.load(memory_order_acquire));
		if (cs != invalid_socket)
		{
			shutdown(cs, SD_BOTH);
			closesocket(cs);
		}
#else
		ksocket cs = ToVar<int>(targetUser->connectionSocket.load(memory_order_acquire));
		if (cs != invalid_socket)
		{
			shutdown(cs, SHUT_RDWR);
			close(cs);
		}
#endif

		if (targetUser->connectionThread.joinable()) targetUser->connectionThread.join();

		Log::Print(
			"Disconnected target via IP '" + string(targetIP) + "' for server '" + string(ServerCore::GetServerName()) + "'!",
			"DISCONNECT_TARGET",
			LogType::LOG_SUCCESS);
	}

	void Connect::DisconnectListener()
	{
		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect listener for server '" + string(ServerCore::GetServerName()) + "' because the server is not running or not ready!",
				"LISTENER_DISCONNECT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!listenerSocket)
		{
			Log::Print(
				"Failed to disconnect listener for server '" + string(ServerCore::GetServerName()) + "' because the server has no listener socket!",
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
				"Failed to disconnect listener for server '" + string(ServerCore::GetServerName()) + "' because the server has not assigned a listener socket!",
				"LISTENER_DISCONNECT",
				LogType::LOG_WARNING);

			return;
		}

		unique_ptr<Connection> lconnect{};

		lockwait_m(m_listenerSocket);
		if (listenerSocket) lconnect = std::move(listenerSocket);
		unlock_m(m_listenerSocket);

		lconnect->isRunning.store(false, memory_order_release);

#ifdef _WIN32
		ksocket thisls = ToVar<SOCKET>(lconnect->connectionSocket.load(memory_order_acquire));
		if (thisls != invalid_socket)
		{
			shutdown(thisls, SD_BOTH);
			closesocket(thisls);
		}
#else
		ksocket thisls = ToVar<int>(lconnect->connectionSocket.load(memory_order_acquire));
		if (thisls != invalid_socket)
		{
			shutdown(thisls, SHUT_RDWR);
			close(thisls);
		}
#endif

		if (lconnect->connectionThread.joinable()) lconnect->connectionThread.join();

		vector<unique_ptr<Connection>> cconnects{};

		lockwait_m(m_connectSockets);
		cconnects = std::move(connectSockets);
		unlock_m(m_connectSockets);

		for (auto& conn : cconnects)
		{
			conn->isRunning.store(false, memory_order_release);

#ifdef _WIN32
			ksocket cs = ToVar<SOCKET>(conn->connectionSocket.load(memory_order_acquire));
			if (cs != invalid_socket)
			{
				shutdown(cs, SD_BOTH);
				closesocket(cs);
			}
#else
			ksocket cs = ToVar<int>(conn->connectionSocket.load(memory_order_acquire));
			if (cs != invalid_socket)
			{
				shutdown(cs, SHUT_RDWR);
				close(cs);
			}
#endif

			if (conn->connectionThread.joinable()) conn->connectionThread.join();
		}

		Log::Print(
			"Disconnected listener socket for server '" + string(ServerCore::GetServerName()) + "'!",
			"LISTENER_DISCONNECT",
			LogType::LOG_SUCCESS);
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