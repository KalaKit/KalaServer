//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#endif

#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <cerrno>
#include <unordered_map>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"

#include "server/ks_connect.hpp"
#include "server/ks_server.hpp"
#include "server/ks_cloudflare.hpp"
#include "server/ks_response.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaCore::FromVar;
using KalaHeaders::KalaCore::ToVar;

using KalaHeaders::KalaThread::lockwait_m;
using KalaHeaders::KalaThread::unlock_m;
using KalaHeaders::KalaThread::joinable_thread;
using KalaHeaders::KalaThread::abool;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

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

constexpr string_view response_ban
	= "<html><body>Get banned nerd</body></html>";
constexpr string_view response_not_found 
	= "<html><body>The page you're trying to access does not exist.</body></html>";
constexpr string_view response_success 
	= "<html><body>\n"
		"<h1>linux webserver lul</h1>\n"
			"<p><a href=\"https://github.com/Lost-Empire-Entertainment/KalaKit-website\">"
				"Check out the KalaKit website source code</a></p>\n"
			"<p><a href=\"https://github.com/KalaKit/KalaServer\">"
				"Check out the KalaKit server source code</a></p>\n"
	"</body></html>";

static void HandleConnectionCallback(ResponseData& data);

namespace KalaServer::Server
{
	static vector<User> users{};
	static mutex m_users{};

	static vector<Route> routes{};
	static mutex m_routes{};

	static vector<string> blacklistedIPs{};
	static vector<string> blacklistedKeywords{};

	static unique_ptr<Connection> listenerSocket{};
	static mutex m_listenerSocket{};

	static vector<unique_ptr<Connection>> connectSockets{};
	static mutex m_connectSockets{};

	bool Connect::CreateListenerSocket()
	{
		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the server is not running or not ready!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (TIME_OUT_PERIOD_M == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the TIME_OUT_PERIOD_M value was set to 0!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (ROLLING_WINDOW_TIMER_S == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the ROLLING_WINDOW_TIMER_S value was set to 0!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (MIN_PACKET_SPACING_MS == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the MIN_PACKET_SPACING_MS value was set to 0!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (ACCEPT_WAIT_TIME_S == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the ACCEPT_WAIT_TIME_S value was set to 0!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (MAX_TOTAL_PAYLOAD_SIZE_BYTES == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the MAX_TOTAL_PAYLOAD_SIZE_BYTES value was set to 0!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (UNASSIGNED_SOCKET_VALUE < 8192)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the UNASSIGNED_SOCKET_VALUE value was set below 8192!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (MAX_ACTIVE_CONNECTIONS == 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the MAX_ACTIVE_CONNECTIONS value was set to 0!",
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		Log::Print(
			"Creating a new listener socket for server '" + ServerCore::GetServerName() + "'!",
			"LISTENER_SOCKET",
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
					"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because the server already has a listener socket!",
					"LISTENER_SOCKET",
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
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket creation failed! Reason: " + KalaServerCore::ErrorToString(WSAGetLastError()),
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		serverAddress.sin_port = htons(ServerCore::GetPort());

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
				"Failed to create new listener socket because SO_REUSEADDR could not be set!",
				"ACCEPT_LOOP",
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
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket bind failed! Reason: " + KalaServerCore::ErrorToString(WSAGetLastError()),
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			closesocket(listener);

			return false;
		}

		if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket listen failed! Reason: " + KalaServerCore::ErrorToString(WSAGetLastError()),
				"LISTENER_SOCKET",
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
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket creation failed! Reason: " + KalaServerCore::ErrorToString(errno),
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		serverAddress.sin_port = htons(ServerCore::GetPort());

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
				"Failed to create new listener socket because SO_REUSEADDR could not be set!",
				"ACCEPT_LOOP",
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
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket bind failed! Reason: " + KalaServerCore::ErrorToString(errno),
				"LISTENER_SOCKET",
				LogType::LOG_ERROR,
				2);

			close(listener);

			return false;
		}

		if (listen(listener, SOMAXCONN) < 0)
		{
			Log::Print(
				"Failed to create new listener socket for server '" + ServerCore::GetServerName() + "' because socket listen failed! Reason: " + KalaServerCore::ErrorToString(errno),
				"LISTENER_SOCKET",
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

		//
		// START CONNECT SOCKET ACCEPT PROCESS
		//

		Log::Print(
			"Created a new listener socket for server '" + ServerCore::GetServerName() + "', starting the accept loop!",
			"LISTENER_SOCKET",
			LogType::LOG_SUCCESS);

		localListener->connectionThread = joinable_thread([localListener]
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

					if (!ServerCore::IsHealthy())
					{
						Log::Print(
							"Server is not healthy, waiting until trying again.",
							"ACCEPT_LOOP",
							LogType::LOG_INFO);

						sleep_for(seconds(SERVER_HEALTH_SLEEP_S));

						continue;
					}

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
#ifdef _WIN32
						SOCKET cs = ToVar<SOCKET>(conn->connectionSocket.load(memory_order_acquire));
						if (cs != UNASSIGNED_SOCKET_VALUE)
						{
							shutdown(cs, SD_BOTH);
							closesocket(cs);
						}
#else
						int cs = ToVar<int>(conn->connectionSocket.load(memory_order_acquire));
						if (cs != UNASSIGNED_SOCKET_VALUE)
						{
							shutdown(cs, SHUT_RDWR);
							close(cs);
						}
#endif

						if (conn->connectionThread.joinable()) conn->connectionThread.join();
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

					Log::Print(
						"Connection received, verifying socket.",
						"ACCEPT_LOOP",
						LogType::LOG_INFO);

					if (client == invalid_socket)
					{
						Log::Print(
							"Failed to accept new connection! Reason: " + KalaServerCore::ErrorToString(WSAGetLastError()),
							"ACCEPT_LOOP",
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
							"Failed to accept new connection because SO_RCVTIMEO could not be set!",
							"ACCEPT_LOOP",
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
							"Failed to accept new connection because SO_SNDTIMEO could not be set!",
							"ACCEPT_LOOP",
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
							"Failed to accept new connection because TCP_NODELAY could not be set!",
							"ACCEPT_LOOP",
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

					Log::Print(
						"Connection received, verifying socket.",
						"ACCEPT_LOOP",
						LogType::LOG_INFO);

					if (client == invalid_socket)
					{
						Log::Print(
							"Failed to accept new connection! Reason: " + KalaServerCore::ErrorToString(errno),
							"ACCEPT_LOOP",
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
							"Failed to accept new connection because SO_RCVTIMEO could not be set!",
							"ACCEPT_LOOP",
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
							"Failed to accept new connection because SO_SNDTIMEO could not be set!",
							"ACCEPT_LOOP",
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
							"Failed to accept new connection because TCP_NODELAY could not be set!",
							"ACCEPT_LOOP",
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
								"Failed to get ipv4 from client socket!",
								"ACCEPT_LOOP",
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
								"Failed to get ipv6 from client socket!",
								"ACCEPT_LOOP",
								LogType::LOG_ERROR,
								2);
						}
					}
					else snprintf(ipStr, sizeof(ipStr), "UNKNOWN");

					//
					// CHECK IF IP IS NOT BANNED
					//

					lockwait_m(m_users);

					bool foundBannedUser{};
					for (const auto& u : users)
					{
						if (ipStr == u.userIP)
						{
							Log::Print(
								"Found banned user '" + string(ipStr) + "' and closing socket.",
								"ACCEPT_LOOP",
								LogType::LOG_INFO);

#ifdef _WIN32
							shutdown(client, SD_BOTH);
							closesocket(client);
#else
							shutdown(client, SHUT_RDWR);
							close(client);
#endif

							foundBannedUser = true;

							break;
						}
					}

					unlock_m(m_users);

					if (foundBannedUser) continue;

					//
					// CHECK USER COUNT, REJECT IF MAX
					//

					lockwait_m(m_connectSockets);

					if (connectSockets.size() >= MAX_ACTIVE_CONNECTIONS)
					{
						string reason = "Max user count '" + to_string(MAX_ACTIVE_CONNECTIONS) + "' was reached, cannot accept new connections!";

						Log::Print(
							"[ " + string(ipStr) + " ] " + reason,
							"ACCEPT_LOOP",
							LogType::LOG_ERROR,
							2);

						unlock_m(m_connectSockets);

						string_view responseBodyTitle = Response::ResponseTypeToString(ResponseType::R_503);

						Response::SendResponse({
							.responseType = ResponseType::R_503,
							.contentType = ContentType::CT_HTML,
							.responseBody = 
								"<html><body><h1>" + string(responseBodyTitle) + "</h1>\n"
								"<p>" + reason + "</p></body></html>",
							.connectionSocket = FromVar(client)
						});

#ifdef _WIN32
						shutdown(client, SD_BOTH);
						closesocket(client);
#else
						shutdown(client, SHUT_RDWR);
						close(client);
#endif

						continue;
					}
					unlock_m(m_connectSockets);

					//
					// STORE AND PARSE SOCKET DATA
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
					// RECEIVE INCOMING DATA
					//

					raw->connectionThread = joinable_thread([raw]
						{
							auto close_socket_on_error = [raw](
								string_view error_reason, 
								ResponseType type,
								Connection* conn) -> void
								{
									string reason = "Connection failed! Reason: " + string(error_reason) + "!";

									Log::Print(
										"[ " + raw->connectionIP + " ] " + reason,
										"ACCEPT_LOOP",
										LogType::LOG_ERROR,
										2);

									string_view responseBodyTitle = Response::ResponseTypeToString(type);

									Response::SendResponse({
										.responseType = type,
										.contentType = ContentType::CT_HTML,
										.responseBody = 
											"<html><body><h1>" + string(responseBodyTitle) + "</h1>\n"
											"<p>" + reason + "</p></body></html>",
										.connection = conn
									});

									raw->isRunning.store(false, memory_order_release);
								};

							string readBuffer{};

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

									else if (err == WSAETIMEDOUT
											 || err == WSAEWOULDBLOCK)
									{
										Log::Print(
											"[ " + raw->connectionIP + " ] BytesReceived recv read timed out.",
											"ACCEPT_LOOP",
											LogType::LOG_INFO);

										raw->isRunning.store(false, memory_order_release);

										continue;
									}
									else if (err == WSAECONNRESET
											 || err == WSAECONNABORTED)
									{
										Log::Print(
											"[ " + raw->connectionIP + " ] Connection was closed abruptly by client during bytesReceived recv read.",
											"ACCEPT_LOOP",
											LogType::LOG_INFO);

										raw->isRunning.store(false, memory_order_release);

										continue;
									}
									
									Log::Print(
										"[ " + raw->connectionIP + " ] BytesReceived recv read failed! Reason: " + KalaServerCore::ErrorToString(err),
										"ACCEPT_LOOP",
										LogType::LOG_ERROR,
										2);

									raw->isRunning.store(false, memory_order_release);

									continue;
								}

								if (bytesReceived == 0)
								{
									Log::Print(
										"[ " + raw->connectionIP + " ] Connection was closed during bytesReceived recv read.",
										"ACCEPT_LOOP",
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

									else if (errno == EAGAIN
											 || errno == EWOULDBLOCK)
									{
										Log::Print(
											"[ " + raw->connectionIP + " ] BytesReceived recv read timed out.",
											"ACCEPT_LOOP",
											LogType::LOG_INFO);

										raw->isRunning.store(false, memory_order_release);

										continue;
									}
									else if (errno == ECONNRESET
											 || errno == ECONNABORTED)
									{
										Log::Print(
											"[ " + raw->connectionIP + " ] Connection was closed abruptly by client during bytesReceived recv read.",
											"ACCEPT_LOOP",
											LogType::LOG_INFO);

										raw->isRunning.store(false, memory_order_release);

										continue;
									}
									
									Log::Print(
										"[ " + raw->connectionIP + " ] BytesReceived recv read failed! Reason: " + KalaServerCore::ErrorToString(errno),
										"ACCEPT_LOOP",
										LogType::LOG_ERROR,
										2);

									raw->isRunning.store(false, memory_order_release);

									continue;
								}

								if (bytesReceived == 0)
								{
									Log::Print(
										"[ " + raw->connectionIP + " ] Connection was closed by client during bytesReceived recv read.",
										"ACCEPT_LOOP",
										LogType::LOG_INFO);

									raw->isRunning.store(false, memory_order_release);

									continue;
								}
#endif

								readBuffer.append(buffer, bytesReceived);

								if (readBuffer.size() > MAX_TOTAL_PAYLOAD_SIZE_BYTES)
								{
									close_socket_on_error(
										"Max payload size '" + to_string(MAX_TOTAL_PAYLOAD_SIZE_BYTES) + "' was reached, cannot accept bigger payload",
										ResponseType::R_413,
										raw);

									break;
								}

								/*
								* for extra debugging if needed
								Log::Print(
										"\n---- EARLY BUFFER START ----\n\n"
										+ readBuffer +
										"---- EARLY BUFFER END ----\n");
								*/

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
										close_socket_on_error(
											"Max payload size '" + to_string(MAX_TOTAL_PAYLOAD_SIZE_BYTES) + "' was reached, cannot accept bigger payload",
											ResponseType::R_413,
											raw);

										break;
									}

									//remove processed request from buffer
									readBuffer.erase(0, totalRequired);

									Log::Print(
										"\n---- BUFFER START [ " + raw->connectionIP + " ] ----\n\n"
										+ fullRequest + newLine +
										"---- BUFFER END ----\n");

									//
									// STORE HEADER AND BODY CONTENT
									//

									//
									// VERIFY HOST
									//
										
									//
									// PARSE ROUTE BY IP AND ROLE
									//

									lockwait_m(m_routes);
									lockwait_m(m_users);

									

									unlock_m(m_users);
									unlock_m(m_routes);

									//
									// ALLOW CONNECTION
									//

									Log::Print(
										"[ " + raw->connectionIP + " ] Connection verified, sending response.",
										"ACCEPT_LOOP",
										LogType::LOG_INFO);

									Response::SendResponse({
										.responseType = ResponseType::R_200,
										.contentType = ContentType::CT_HTML,
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
				"Failed to disconnect target via socket for server '" + ServerCore::GetServerName() + "' because the server is not running or not ready!",
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
				"Failed to disconnect target via socket for server '" + ServerCore::GetServerName() + "' because the socket is unassigned or invalid!",
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
				"Failed to disconnect target via socket for server '" + ServerCore::GetServerName() + "' because the target socket was not found!",
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
			"Disconnected target via socket for server '" + ServerCore::GetServerName() + "'!",
			"DISCONNECT_TARGET",
			LogType::LOG_SUCCESS);
	}

	void Connect::DisconnectConnectedUser(const string& targetIP)
	{
		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the server is not running or not ready!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!IsValidIP(targetIP))
		{
			Log::Print(
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the IP structure is invalid!",
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
				"Failed to disconnect target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "' because the target IP was not found!",
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
			"Disconnected target via IP '" + targetIP + "' for server '" + ServerCore::GetServerName() + "'!",
			"DISCONNECT_TARGET",
			LogType::LOG_SUCCESS);
	}

	void Connect::DisconnectListener()
	{
		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect listener for server '" + ServerCore::GetServerName() + "' because the server is not running or not ready!",
				"LISTENER_DISCONNECT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!listenerSocket)
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
			"Disconnected listener socket for server '" + ServerCore::GetServerName() + "'!",
			"LISTENER_DISCONNECT",
			LogType::LOG_SUCCESS);
	}

	bool Connect::IsValidIP(const string& targetIP)
	{
		struct in_addr addr4{};
		if (inet_pton(AF_INET, targetIP.c_str(), &addr4) == 1) return true;

		struct in6_addr addr6{};
		if (inet_pton(AF_INET6, targetIP.c_str(), &addr6) == 1) return true;

		return false;
	}

	string Connect::RoleToString(Role role)
	{
		switch (role)
		{
		default:
		case Role::ROLE_NONE:         return "NONE";

		case Role::ROLE_BANNED:       return "BANNED";
		case Role::ROLE_GUEST:        return "GUEST";
		case Role::ROLE_BLACKLISTED:  return "BLACKLISTED";
		case Role::ROLE_USER:         return "USER";
		case Role::ROLE_ADMIN:        return "ADMIN";
		}
	}
	Role Connect::StringToRole(const string& role)
	{
		if (role == "BANNED")           return Role::ROLE_BANNED;
		else if (role == "GUEST")       return Role::ROLE_GUEST;
		else if (role == "BLACKLISTED") return Role::ROLE_BLACKLISTED;
		else if (role == "USER")        return Role::ROLE_USER;
		else if (role == "ADMIN")       return Role::ROLE_ADMIN;

		else return Role::ROLE_NONE; //assume all unknown inputs route to NONE by default
	}

	Role Connect::GetUserRole(const string& userIP)
	{
		if (!IsValidIP(userIP))
		{
			Log::Print(
				"Failed to get role for user with IP '" + userIP + "' because its structure is invalid!",
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
		if (!IsValidIP(userIP))
		{
			Log::Print(
				"Failed to set role for user with IP '" + userIP + "' because its structure is invalid!",
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
		if (!IsValidIP(newUser.userIP))
		{
			Log::Print(
				"Failed to add new user with IP '" + newUser.userIP + "' because its structure is invalid!",
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
		if (!IsValidIP(userIP))
		{
			Log::Print(
				"Failed to remove existing user with IP '" + userIP + "' because its structure is invalid!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return;
		}

		lockwait_m(m_users);

		auto it = remove_if(
			users.begin(),
			users.end(),
			[&userIP](const User& u) { return u.userIP == userIP; });

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
			|| newRole == Role::ROLE_BANNED)
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
			|| newRoute.role == Role::ROLE_BANNED)
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
			[&route](const Route& u) { return u.route == route; });

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

void HandleConnectionCallback(ResponseData& data)
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