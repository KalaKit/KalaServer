//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <ws2tcpip.h>
#include <ws2ipdef.h>
#include <winsock.h>
#include <winsock2.h>
#include <minwindef.h>
#include <winerror.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/capability.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#endif

#include <string>
#include <chrono>
#include <sstream>
#include <array>
#include <vector>

#include "core_utils.hpp"
#include "log_utils.hpp"
#include "string_utils.hpp"
#include "file_utils.hpp"

#include "core/ks_core.hpp"
#include "core/ks_response.hpp"

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::FromVar;
using KalaHeaders::KalaCore::ContainsValue;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

using KalaHeaders::KalaString::SplitString;
using KalaHeaders::KalaString::TrimString;

using KalaHeaders::KalaFile::WriteLinesToFile;
using KalaHeaders::KalaFile::ReadLinesFromFile;

using KalaServer::Core::KalaServerCore;
using KalaServer::Core::DomainRoute;
using KalaServer::Core::BannedIP;
using KalaServer::Core::Connection;
using KalaServer::Core::Response;
using KalaServer::Core::ResponseType;
using KalaServer::Core::ContentType;
using KalaServer::Core::OptionalSendType;
using KalaServer::Core::MAX_TOTAL_PAYLOAD_SIZE_BYTES;

using std::to_string;
using std::string;
using std::string_view;
using std::istringstream;
using std::vector;
using std::array;
using std::chrono::duration_cast;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::filesystem::path;

#ifdef _WIN32
using std::wstring;
#endif

using u16 = uint16_t;
using u64 = uint64_t;

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
#define kclose(sock) closesocket((SOCKET)(sock))
#else
#define kclose(sock) close((int)(sock))
#endif

static bool isInitialized{};

#ifdef _WIN32
	static bool startedWSA{};
	WSADATA wsaData{};
#endif

static string serverName{};
static path serverRoot{};
static string serverIP{};
static u16 serverPort{};

static Connection listenerSocket{};
static vector<Connection> connectSockets{};

static vector<string> domains{};
static vector<BannedIP> bannedIPs{};
static vector<DomainRoute> routes{};
static vector<string> blacklistedKeywords{};

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
	"    <h1>custom webserver lul</h1>\n"
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

static void HandleClient(Connection& c);

namespace KalaServer::Core
{
	string KalaServerCore::ErrorToString(int error)
	{
		static string empty{};
#ifdef _WIN32
		auto to_short = [](const wstring& str)
			{
				if (str.empty()) return empty;

				int size_needed = WideCharToMultiByte(
					CP_UTF8,
					0,
					str.data(),
					scast<int>(str.size()),
					nullptr,
					0,
					nullptr,
					nullptr);

				if (size_needed <= 0) return empty;

				string result(size_needed, 0);

				if (WideCharToMultiByte(
					CP_UTF8,
					0,
					str.data(),
					scast<int>(str.size()),
					result.data(),
					size_needed,
					nullptr,
					nullptr) <= 0)
				{
					return empty;
				}

				return result;
			};

		LPWSTR buffer{};

		FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER
			| FORMAT_MESSAGE_FROM_SYSTEM
			| FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&buffer,
			0,
			nullptr);

		wstring wmsg = buffer ? buffer : L"Unknown error";

		if (buffer) LocalFree(buffer);

		return TrimString(to_short(wmsg)) + " [" + to_string(error) + "]";
#else
		return string(strerror(error)) + " [" + to_string(error) + "]";
#endif
	}

	void KalaServerCore::ForceClose(
		string_view target,
		string_view reason)
	{
		Log::Print(
			"\n================"
			"\nFORCE CLOSE"
			"\n================\n",
			true);

		Log::Print(
			reason,
			target,
			LogType::LOG_ERROR,
			2,
			true,
			TimeFormat::TIME_NONE,
			DateFormat::DATE_NONE);

#ifdef _WIN32
		__debugbreak();
#else
		raise(SIGTRAP);
#endif
	}

	void KalaServerCore::Initialize(
		string_view newServerName,
		const path& newServerRoot,
		string_view newServerIP,
		u16 newServerPort)
	{
		if (newServerName.empty()
			|| newServerName.length() > 50)
		{
			ForceClose(
				"Server init error",
				"Failed to initialize server because its name is empty or too long!");
		}

		if (!exists(newServerRoot))
		{
			ForceClose(
				"Server init error",
				"Failed to initialize server '" + string(newServerName) + "' because its server root '" + newServerRoot.string() + "' is empty or does not exist!");
		}

		if (MIN_PORT_RANGE == 0)
		{
			ForceClose(
				"Server init error",
				"Failed to initialize server '" + string(newServerName) + "' because the MIN_PORT_RANGE value was set to 0!");
		}
		if (MIN_PORT_RANGE > MAX_PORT_RANGE)
		{
			ForceClose(
				"Server init error",
				"Failed to initialize server '" + string(newServerName) + "' because the MIN_PORT_RANGE value was set higher than the MAX_PORT_RANGE value!");
		}

		if (newServerPort < MIN_PORT_RANGE
			|| newServerPort > MAX_PORT_RANGE)
		{
			ForceClose(
				"Server init error",
				"Failed to initialize server '" + string(newServerName) + "' because its port '" + to_string(newServerPort) + "' is out of range!");
		}

		if (!IsValidIP(newServerIP))
		{
			ForceClose(
				"Server init error",
				"Failed to initialize server '" + string(newServerName) + "' because its IP '" + string(newServerIP) + "' is not a valid IP address!");
		}

#ifdef _WIN32
		if (!startedWSA)
		{
			u32 errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (errorCode != 0)
			{
				ForceClose(
					"Server init error",
					"Failed to call WSASTARTUP! Error code: " + KalaServerCore::ErrorToString(errorCode));
			}
			startedWSA = true;
		}
#endif

		serverName = newServerName;
		serverRoot = newServerRoot;
		serverIP = newServerIP;
		serverPort = newServerPort;

		CreateListenerSocket();

		isInitialized = true;

		string output = 
			"Created new server '" + serverName + "' "
			"with root '" + serverRoot.string() + "', "
			"IP '" + serverIP + "' "
			"and port '" + to_string(serverPort) + "'!";

		Log::Print(
			output,
			"KS_CORE",
			LogType::LOG_SUCCESS);
	}

	bool KalaServerCore::IsInitialized() { return isInitialized; }

	void KalaServerCore::CreateListenerSocket()
	{
		ksocket listener = 
#ifdef _WIN32
		socket(
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP);

		if (listener == INVALID_SOCKET)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName 
				+ "'! Reason: " + KalaServerCore::ErrorToString(WSAGetLastError()));
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(serverPort);

		if (inet_pton(
			AF_INET, 
			serverIP.c_str(),
			&serverAddress.sin_addr) != 1)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName 
				+ "' because IP '" + serverIP + "' is invalid!");
		}

		int opt = 1;

		int result_reuse_addr = setsockopt(
			listener,
			SOL_SOCKET,
			SO_REUSEADDR,
			(const char*)&opt,
			sizeof(opt));

		if (result_reuse_addr == SOCKET_ERROR)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName + " because SO_REUSEADDR couldn't be set!");
		}

		if (bind(
			listener,
			(sockaddr*)&serverAddress,
			sizeof(serverAddress)) == SOCKET_ERROR)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName + "' because socket bind failed! Error code: " + to_string(WSAGetLastError()));
		}

		if (listen(listener, SOMAXCONN) == SOCKET_ERROR)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName + " because socket listen failed!");
		}

		//make listener non-blocking

		u_long mode = 1;
		ioctlsocket(
			listener,
			FIONBIO,
			&mode);
#else
		socket(
			AF_INET,
			SOCK_STREAM,
			0);

		if (listener < 0)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName + " because socket creation failed!");
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(serverPort);

		if (inet_pton(
			AF_INET, 
			serverIP.c_str(),
			&serverAddress.sin_addr) != 1)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName 
				+ "' because IP '" + serverIP + "' is invalid!");
		}

		int opt = 1;

		int result_reuse_addr = setsockopt(
			listener,
			SOL_SOCKET,
			SO_REUSEADDR,
			(const char*)&opt,
			sizeof(opt));

		if (result_reuse_addr < 0)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName + " because SO_REUSEADDR couldn't be set!");
		}

		if (bind(
			listener,
			(sockaddr*)&serverAddress,
			sizeof(serverAddress)) == -1)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName + "' because socket bind failed! Error code: " + to_string(errno));
		}

		if (listen(listener, SOMAXCONN) < 0)
		{
			ForceClose(
				"Server init error",
				"Failed to create listener socket for server '" + serverName + " because socket listen failed!");
		}

		//make listener non-blocking

		fcntl(
			listener,
			F_SETFL,
			O_NONBLOCK);
#endif

		listenerSocket.connectionSocket = listener;
		listenerSocket.isRunning = true;

		Log::Print(
			"Created new listener socket for server '" + serverName + "'!",
			"KS_CORE",
			LogType::LOG_SUCCESS);
	}

	void KalaServerCore::Update()
	{
		if (!isInitialized)
		{
			Log::Print(
				"Cannot update server '" + serverName + "' because it is not initialized!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!HasInternet())
		{
			Log::Print(
				"Cannot update server '" + serverName + "' because it has no internet!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!listenerSocket.isRunning
			|| listenerSocket.connectionSocket == invalid_socket)
		{
			Log::Print(
				"There is no active listener socket for clients to connect to!",
				"KS_CORE",
				LogType::LOG_WARNING);

			return;
		}

		//
		// REMOVE DISABLED SOCKETS
		//

		for (auto it = connectSockets.begin(); it != connectSockets.end();)
		{
			ksocket sock = 
#ifdef _WIN32			
			ToVar<SOCKET>(it->connectionSocket);
#else
			ToVar<int>(it->connectionSocket);
#endif

			if (!it->isRunning)
			{
				if (sock != invalid_socket) kclose(sock);
				it = connectSockets.erase(it);
			}
			else ++it;
		}

		string sendMsg{};

		//
		// VERIFY CONNECTING SOCKETS
		//

		for (int i = 0; i < MAX_CONNECTIONS_PER_FRAME; ++i)
		{
			ksocket lsock = 
#ifdef _WIN32
			ToVar<SOCKET>(listenerSocket.connectionSocket);

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
				//queue empty, stop accepting this frame
				else if (err == WSAEWOULDBLOCK) break;
				else if (err == WSAECONNRESET
						 || err == WSAECONNABORTED)
				{
					Log::Print(
						"Socket was closed abruptly by client during accept.",
						"KS_CORE",
						LogType::LOG_INFO);

					continue;
				}

				Log::Print(
					"Failed to accept new connection! Reason: " + ErrorToString(err),
					"KS_CORE",
					LogType::LOG_ERROR,
					2);

				continue;
			}

			//make socket non-blocking
			u_long mode = 1;
			ioctlsocket(
				client,
				FIONBIO,
				&mode);

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
					"KS_CORE",
					LogType::LOG_ERROR,
					2);

				kclose(client);
				continue;
			}
#else
			ToVar<int>(listenerSocket.connectionSocket);

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
				else if (errno == EAGAIN
						 || errno == EWOULDBLOCK)
				{
					//queue empty, stop accepting this frame
					break;
				}
				else if (errno == ECONNRESET
						 || errno == ECONNABORTED)
				{
					Log::Print(
						"Socket was closed abruptly by client during accept.",
						"KS_CORE",
						LogType::LOG_INFO);

					continue;
				}

				Log::Print(
					"Failed to accept new connection! Reason: " + ErrorToString(errno),
					"KS_CORE",
					LogType::LOG_ERROR,
					2);

				continue;
			}

			//make socket non-blocking
			fcntl(
				client,
				F_SETFL,
				O_NONBLOCK);

			int no_delay = 1;

			int result_no_delay = setsockopt(
				client,
				IPPROTO_TCP,
				TCP_NODELAY,
				(char*)&no_delay,
				sizeof(no_delay));

			if (result_no_delay < 0)
			{
				Log::Print(
					"Failed to accept new connection because TCP_NODELAY couldn't be set!",
					"KS_CORE",
					LogType::LOG_ERROR,
					2);

				kclose(client);
				continue;
			}
#endif

			//
			// DROP USER IF SERVER IS FULL
			//

			if (connectSockets.size() >= MAX_ACTIVE_CONNECTIONS)
			{
				sendMsg = "Max user count '" + to_string(MAX_ACTIVE_CONNECTIONS) + "' was reached, cannot accept new connections!";

				Log::Print(
					sendMsg,
					"KS_CORE",
					LogType::LOG_WARNING);

				Response::SendResponse({
					.responseType = ResponseType::R_503,
					.contentType = ContentType::CT_HTML,
					.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
					.responseBody =
						ReturnErrorBody(
							sendMsg,
							ResponseType::R_503),
					.connectionSocket = FromVar(client)
				});

				continue;
			}

			//
			// GET USER IP
			//

			char ipStr[INET6_ADDRSTRLEN]{};

			if (clientAddress.ss_family == AF_INET)
			{
				auto* addr = rcast<sockaddr_in*>(&clientAddress);
				if (inet_ntop(
					AF_INET,
					&addr->sin_addr,
					ipStr,
					sizeof(ipStr)) == nullptr)
				{
					ipStr[0] = '\0';

					Log::Print(
						"Failed to accept new connection because ipv4 couldn't be found from it!",
						"KS_CORE",
						LogType::LOG_ERROR,
						2);

					kclose(client);
					continue;
				}
			}
			else if (clientAddress.ss_family == AF_INET6)
			{
				auto* addr = rcast<sockaddr_in6*>(&clientAddress);
				if (inet_ntop(
					AF_INET6,
					&addr->sin6_addr,
					ipStr,
					sizeof(ipStr)) == nullptr)
				{
					ipStr[0] = '\0';

					Log::Print(
						"Failed to accept new connection because ipv6 couldn't be found from it!",
						"KS_CORE",
						LogType::LOG_ERROR,
						2);

					kclose(client);
					continue;
				}
			}
			else snprintf(
				ipStr, 
				sizeof(ipStr), 
				"UNKNOWN");

			string connectionIP = "[ " + string(ipStr) + " ] ";
			
			Log::Print(
				connectionIP + "New client connected.",
				"KS_CORE",
				LogType::LOG_INFO);

			//
			// CHECK IF IP IS NOT BANNED
			//

			bool foundBannedUser{};

			for (const auto& u : bannedIPs)
			{
				if (string(ipStr) == u.targetIP)
				{
					Log::Print(
						connectionIP + "Banned user tried to reconnect to server.",
						"KS_CORE",
						LogType::LOG_INFO);

					Response::SendResponse({
						.responseType = ResponseType::R_418,
						.contentType = ContentType::CT_HTML,
						.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
						.responseBody =
							ReturnErrorBody(
								"Get kicked nerd",
								ResponseType::R_418),
						.connectionSocket = FromVar(client)
					});

					foundBannedUser = true;

					break;
				}
			}

			if (foundBannedUser) continue;

			connectSockets.push_back({
				.isRunning = true,
				.connectionIP = string(ipStr),
				.connectionSocket = FromVar(client)
			});
		}

		for (auto& c : connectSockets) HandleClient(c);
	}

	bool KalaServerCore::HasInternet()
	{
		static bool cachedResult{};
		static u64 lastCheckedTime{};

		u64 now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
		if (now - lastCheckedTime < SERVER_HEALTH_WAIT_S * 1000ULL) return cachedResult;

#ifdef _WIN32
		if (!startedWSA)
		{
			u32 errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (errorCode != 0)
			{
				ForceClose(
					"Internet health check error",
					"Failed to call WSASTARTUP! Reason: " + KalaServerCore::ErrorToString(errorCode));

				return false;
			}
			startedWSA = true;
		}

		ksocket sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
		{
			string failReason = KalaServerCore::ErrorToString(WSAGetLastError());

			Log::Print(
				"Failed to check internet state for server '" + serverName + "' because socket creation failed! Reason: " + failReason,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		DWORD timeout = 1000;
		setsockopt(
			sock, 
			SOL_SOCKET, 
			SO_SNDTIMEO, 
			(const char*)&timeout, 
			sizeof(timeout));
		setsockopt(
			sock, 
			SOL_SOCKET, 
			SO_RCVTIMEO, 
			(const char*)&timeout, 
			sizeof(timeout));
#else
		ksocket sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
		{
			Log::Print(
				"Failed to check internet state for server '" + serverName + "' because socket creation failed!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		struct timeval timeout{};
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		setsockopt(
			sock, 
			SOL_SOCKET, 
			SO_SNDTIMEO, 
			&timeout, 
			sizeof(timeout));
		setsockopt(
			sock, 
			SOL_SOCKET, 
			SO_RCVTIMEO, 
			&timeout, 
			sizeof(timeout));
#endif

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(53);
		if (inet_pton(AF_INET, "1.1.1.1", &addr.sin_addr) != 1)
		{
			kclose(sock);

			return false;
		}

		cachedResult = 
#ifdef _WIN32
		(connect(sock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR);
#else
		(connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0);
#endif

		kclose(sock);

		lastCheckedTime = now;

		return cachedResult;
	}

	string_view KalaServerCore::GetServerName() { return serverName; }
	const path& KalaServerCore::GetServerRoot() { return serverRoot; }
	string_view KalaServerCore::GetServerIP() { return serverIP; }
	u16 KalaServerCore::GetServerPort() { return serverPort; }

	const Connection& KalaServerCore::GetListenerSocket() { return listenerSocket; }

	const vector<Connection>& KalaServerCore::GetConnectSockets() { return connectSockets; }

	void KalaServerCore::DisconnectConnectedUser(uintptr_t targetSocket)
	{
		if (!KalaServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" 
				+ string(KalaServerCore::GetServerName()) + "' because the server is not initialized!",
				"KS_CORE",
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
				"Failed to disconnect target via socket for server '" 
				+ string(KalaServerCore::GetServerName()) + "' because the socket is unassigned or invalid!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		bool foundTarget{};

		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			if (it->connectionSocket == target)
			{
				kclose(target);

				it->isRunning = false;
				foundTarget = true;

				break;
			}
		}

		if (!foundTarget)
		{
			Log::Print(
				"Failed to disconnect target for server '" 
				+ string(KalaServerCore::GetServerName()) + "' because the requested socket was not found!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Log::Print(
			"Disconnected target via socket for server '" + string(KalaServerCore::GetServerName()) + "'!",
			"KS_CORE",
			LogType::LOG_SUCCESS);
	}
	void KalaServerCore::DisconnectConnectedUser(string_view targetIP)
	{
		if (!KalaServerCore::IsInitialized())
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) 
				+ "' for server '" + string(KalaServerCore::GetServerName()) + "' because the server is not initialized!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!KalaServerCore::IsValidIP(targetIP))
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) 
				+ "' for server '" + string(KalaServerCore::GetServerName()) + "' because the IP structure is invalid!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		bool foundTarget{};

		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			string connectionIP = it->connectionIP;

			if (connectionIP == targetIP)
			{
				if (it->connectionSocket == invalid_socket)
				{
					Log::Print(
						"Failed to disconnect target via IP '" + string(targetIP) 
						+ "' for server '" + string(KalaServerCore::GetServerName()) + "' because the socket was invalid!",
						"KS_CORE",
						LogType::LOG_ERROR,
						2);
				}
				else 
				{
					ksocket sock = 
#ifdef _WIN32			
					ToVar<SOCKET>(it->connectionSocket);
#else
					ToVar<int>(it->connectionSocket);
#endif

					kclose(sock);
				}
	
				it->isRunning = false;
				foundTarget = true;

				break;
			}
		}

		if (!foundTarget)
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) 
				+ "' for server '" + string(KalaServerCore::GetServerName()) + "' because no socket with the IP is connected!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Log::Print(
			"Disconnected target via IP '" + string(targetIP) + "' for server '" + string(KalaServerCore::GetServerName()) + "'!",
			"KS_CORE",
			LogType::LOG_SUCCESS);
	}

	bool KalaServerCore::IsValidIP(string_view targetIP)
	{
		struct in_addr addr4{};
		if (inet_pton(AF_INET, string(targetIP).c_str(), &addr4) == 1) return true;

		struct in6_addr addr6{};
		if (inet_pton(AF_INET6, string(targetIP).c_str(), &addr6) == 1) return true;

		return false;
	}

	const vector<BannedIP>& KalaServerCore::GetBannedIPs() { return bannedIPs; }

	bool KalaServerCore::BanIP(string_view targetIP)
	{
		for (const auto& b : bannedIPs)
		{
			if (b.targetIP == targetIP)
			{
				Log::Print(
					"Failed to ban IP '" + string(targetIP) + "' because it is already banned!",
					"KS_CORE",
					LogType::LOG_ERROR,
					2);

				return false;
			}
		}

		bannedIPs.push_back({.targetIP = string(targetIP)});

		return true;
	}
	bool KalaServerCore::UnbanIP(string_view targetIP)
	{
		for (auto it = bannedIPs.begin(); it != bannedIPs.end(); ++it)
		{
			if (it->targetIP == targetIP)
			{
				bannedIPs.erase(it);

				return false;
			}
		}

		Log::Print(
			"Failed to unban IP '" + string(targetIP) + "' because it has not been banned!",
			"KS_CORE",
			LogType::LOG_ERROR,
			2);

		return true;
	}

	bool KalaServerCore::SaveBannedIPsToDisk(const path& targetPath)
	{
		if (bannedIPs.empty())
		{
			Log::Print(
				"There are no banned IPs to save to disk.",
				"KS_CORE",
				LogType::LOG_INFO);

			return false;
		}

		vector<string> lines{};

		//TODO: add setup

		string errorMsg = WriteLinesToFile(targetPath, lines);

		if (!errorMsg.empty())
		{
			Log::Print(
				"Failed to save banned IPs to path '" + targetPath.string() + "'! Reason: " + errorMsg,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return true;
	}
	bool KalaServerCore::LoadBannedIPsFromDisk(const path& targetPath)
	{
		vector<string> lines{};

		string errorMsg = ReadLinesFromFile(targetPath, lines);

		if (!errorMsg.empty())
		{
			Log::Print(
				"Failed to load banned IPs from path '" + targetPath.string() + "'! Reason: " + errorMsg,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		//TODO: add setup

		return true;
	}

	const vector<string>& KalaServerCore::GetDomains() { return domains; }
	const vector<DomainRoute>& KalaServerCore::GetRoutes() { return routes; }

	bool KalaServerCore::AddRoute(const DomainRoute& newRoute)
	{
		if (!isInitialized)
		{
			Log::Print(
				"Cannot add route the server is not initialized!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		
		if (newRoute.domain.empty())
		{
			Log::Print(
				"Cannot add domain '" + newRoute.domain + "' to server '" + serverName + "' because the domain name is empty!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (newRoute.domain.find('.') == string::npos)
		{
			Log::Print(
				"Cannot add domain '" + newRoute.domain + "' to server '" + serverName + "' because the domain has no extension splitters!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!ContainsValue(domains, newRoute.domain))
		{
			domains.push_back(newRoute.domain);
		}

		vector<string> split = SplitString(newRoute.domain, ".");
		if (split.size() != 2)
		{
			Log::Print(
				"Cannot add domain '" + newRoute.domain + "' to server '" + serverName + "' because the domain has a malformed structure!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		for (const auto& r : routes)
		{
			if (r.domain == newRoute.domain
				&& r.route == newRoute.route)
			{
				Log::Print(
					"Failed to add new route with domain '" + newRoute.domain + "' and route '" + newRoute.route + "' because it has already been added!",
					"KS_CORE",
					LogType::LOG_ERROR,
					2);

				return false;
			}
		}

		path cleanedPath = weakly_canonical(newRoute.routePath);

		if (!exists(cleanedPath))
		{
			Log::Print(
				"Failed to add new route with domain '" + newRoute.domain + "' and route '" + newRoute.route + "' because its path '" + cleanedPath.string() + "' does not exist!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		for (const auto& r : routes)
		{
			if (r.routePath == cleanedPath)
			{
				Log::Print(
					"Failed to add new route with domain '" + newRoute.domain + "' and route '" + newRoute.route + "' because it has already been added!",
					"KS_CORE",
					LogType::LOG_ERROR,
					2);

				return false;
			}
		}

		routes.push_back(
			{ 
				.domain = newRoute.domain, 
				.route = newRoute.route, 
				.routePath = cleanedPath 
			});

		Log::Print(
			"Added new domain '" + newRoute.domain + "' with route '" + newRoute.route + "' and path '" + cleanedPath.string() + "'!",
			"KS_CORE",
			LogType::LOG_SUCCESS);

		return true;
	}
	bool KalaServerCore::RemoveRoute(const DomainRoute& existingRoute)
	{
		auto it = remove_if(
			routes.begin(),
			routes.end(),
			[&existingRoute](const DomainRoute& u) { return u == existingRoute; });

		if (it == routes.end())
		{
			Log::Print(
				"Failed to remove existing domain '" + existingRoute.domain + "' with route '" + existingRoute.route + "' because it has not been added!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		routes.erase((it), routes.end());

		for (auto it = domains.begin(); it != domains.end();)
		{
			bool foundIt{};
			for (const auto& r : routes)
			{
				if (r.domain == *it)
				{
					foundIt = true;
					break;
				}
			}

			if (!foundIt) it = domains.erase(it);
			else ++it;
		}

		Log::Print(
			"Removed existing domain '" + existingRoute.domain + "' with route '" + existingRoute.route + "'.",
			"KS_CORE",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool KalaServerCore::SaveRoutesToDisk(const path& targetPath)
	{
		if (routes.empty())
		{
			Log::Print(
				"There are no routes to save to disk.",
				"KS_CORE",
				LogType::LOG_INFO);

			return false;
		}

		vector<string> lines{};

		//TODO: add setup

		string errorMsg = WriteLinesToFile(targetPath, lines);

		if (!errorMsg.empty())
		{
			Log::Print(
				"Failed to save routes to path '" + targetPath.string() + "'! Reason: " + errorMsg,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return true;
	}
	bool KalaServerCore::LoadRoutesFromDisk(const path& targetPath)
	{
		vector<string> lines{};

		string errorMsg = ReadLinesFromFile(targetPath, lines);

		if (!errorMsg.empty())
		{
			Log::Print(
				"Failed to load routes from path '" + targetPath.string() + "'! Reason: " + errorMsg,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		//TODO: add setup

		return true;
	}

	bool KalaServerCore::AddBlacklistedKeyword(string_view newKeyword)
	{
		for (const auto& r : blacklistedKeywords)
		{
			if (r == newKeyword)
			{
				Log::Print(
					"Failed to add new blacklisted keyword '" + string(newKeyword) + "' because it has already been added!",
					"KS_CORE",
					LogType::LOG_ERROR,
					2);

				return false;
			}
		}

		blacklistedKeywords.push_back(string(newKeyword));

		Log::Print(
			"Added new blacklisted keyword '" + string(newKeyword) + "'!",
			"KS_CORE",
			LogType::LOG_SUCCESS);

		return true;
	}
	bool KalaServerCore::RemoveBlacklistedKeyword(string_view existingKeyword)
	{
		auto it = remove_if(
			blacklistedKeywords.begin(),
			blacklistedKeywords.end(),
			[&existingKeyword](const string& u) { return u == existingKeyword; });

		if (it == blacklistedKeywords.end())
		{
			Log::Print(
				"Failed to remove existing blacklisted keyword '" + string(existingKeyword) + "' because it has not been added!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		blacklistedKeywords.erase((it), blacklistedKeywords.end());

		Log::Print(
			"Removed existing blacklisted keyword '" + string(existingKeyword) + "'.",
			"KS_CORE",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool KalaServerCore::SaveBlacklistedKeywordsToDisk(const path& targetPath)
	{
		if (routes.empty())
		{
			Log::Print(
				"There are no blacklisted keywords to save to disk.",
				"KS_CORE",
				LogType::LOG_INFO);

			return false;
		}

		vector<string> lines{};

		//TODO: add setup

		string errorMsg = WriteLinesToFile(targetPath, lines);

		if (!errorMsg.empty())
		{
			Log::Print(
				"Failed to save blacklisted keywords to path '" + targetPath.string() + "'! Reason: " + errorMsg,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return true;
	}
	bool KalaServerCore::LoadBlacklistedKeywordsFromDisk(const path& targetPath)
	{
		vector<string> lines{};

		string errorMsg = ReadLinesFromFile(targetPath, lines);

		if (!errorMsg.empty())
		{
			Log::Print(
				"Failed to load blacklisted keywords from path '" + targetPath.string() + "'! Reason: " + errorMsg,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		//TODO: add setup

		return true;
	}

	const vector<string>& KalaServerCore::GetBlacklistedKeywords() { return blacklistedKeywords; }

	void KalaServerCore::Shutdown()
	{
		if (!KalaServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot shut down the server because it is not initialized!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		listenerSocket.isRunning = false;
		if (listenerSocket.connectionSocket != invalid_socket)
		{
			kclose(listenerSocket.connectionSocket);
		}

		for (auto& conn : connectSockets)
		{
			conn.isRunning = false;

#ifdef _WIN32
			ksocket cs = ToVar<SOCKET>(conn.connectionSocket);
#else
			ksocket cs = ToVar<int>(conn.connectionSocket);
#endif

			if (cs != invalid_socket) kclose(cs);
		}
		connectSockets.clear();
		
#ifdef _WIN32
		if (startedWSA)
		{
			WSACleanup();
			startedWSA = false;
		}
#endif

		isInitialized = false;

		Log::Print(
			"Server '" + serverName + "' has been fully shut down!",
			"KS_CORE",
			LogType::LOG_SUCCESS);
	}
}

void HandleClient(Connection& c)
{
	string sendMsg{};

	//
	// CHECK CLIENT PAYLOAD REQUEST
	//

	string connectionIP = "[ " + c.connectionIP + " ] ";

	ksocket client = 
#ifdef _WIN32
	ToVar<SOCKET>(c.connectionSocket);

	char buffer[MAX_TOTAL_PAYLOAD_SIZE_BYTES]{};
	int bytesReceived = recv(
		client,
		buffer,
		sizeof(buffer),
		0);

	if (bytesReceived == SOCKET_ERROR)
	{
		DWORD err = WSAGetLastError();

		//interrupted or no data, try again
		if (err == WSAEINTR
			|| err == WSAEWOULDBLOCK)
		{
			return;
		}
		else if (err == WSAECONNRESET
					|| err == WSAECONNABORTED)
		{
			Log::Print(
				connectionIP + "Socket was closed abruptly by client during bytesReceived recv read.",
				"KS_CORE",
				LogType::LOG_INFO);

			c.isRunning = false;

			return;
		}

		Log::Print(
			"BytesReceived recv read failed! Reason: " + KalaServerCore::ErrorToString(err),
			"KS_CORE",
			LogType::LOG_ERROR,
			2);

		c.isRunning = false;

		return;
	}

	if (bytesReceived == 0)
	{
		Log::Print(
			connectionIP + "Socket was closed gracefully by client during bytesReceived recv read.",
			"KS_CORE",
			LogType::LOG_INFO);

		c.isRunning = false;

		return;
	}
#else
	ToVar<int>(c.connectionSocket);

	char buffer[MAX_TOTAL_PAYLOAD_SIZE_BYTES]{};
	int bytesReceived = recv(
		client,
		buffer,
		sizeof(buffer),
		0);

	if (bytesReceived < 0)
	{
		//interrupted or no data, try again
		if (errno == EINTR
			|| errno == EWOULDBLOCK
			|| errno == EAGAIN)
		{
			return;
		}
		else if (errno == ECONNRESET
					|| errno == ECONNABORTED)
		{
			Log::Print(
				connectionIP + "Socket was closed abruptly by client during bytesReceived recv read.",
				"KS_CORE",
				LogType::LOG_INFO);

			c.isRunning = false;

			return;
		}

		Log::Print(
			"BytesReceived recv read failed! Reason: " + KalaServerCore::ErrorToString(errno),
			"KS_CORE",
			LogType::LOG_ERROR,
			2);

		c.isRunning = false;

		return;
	}

	if (bytesReceived == 0)
	{
		Log::Print(
			connectionIP + "Socket was closed gracefully by client during bytesReceived recv read.",
			"KS_CORE",
			LogType::LOG_INFO);

		c.isRunning = false;

		return;
	}
#endif

	c.partialBuffer.append(buffer, bytesReceived);

	if (c.partialBuffer.size() > MAX_TOTAL_PAYLOAD_SIZE_BYTES)
	{
		sendMsg = "Max payload size '" + to_string(MAX_TOTAL_PAYLOAD_SIZE_BYTES) + "' was reached, cannot accept bigger payload!";

		Response::SendResponse({
			.responseType = ResponseType::R_413,
			.contentType = ContentType::CT_HTML,
			.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
			.responseBody =
				ReturnErrorBody(
					sendMsg,
					ResponseType::R_413),
			.connectionSocket = c.connectionSocket
		});

		return;
	}

	//
	// PARSE CLIENT PAYLOAD
	//

	Log::Print(
		connectionIP + "Found partial data:\n" + buffer,
		"KS_CORE",
		LogType::LOG_INFO);

	Response::SendResponse({
		.responseType = ResponseType::R_200,
		.contentType = ContentType::CT_HTML,
		.responseBody = string(response_success),
		.connectionSocket = c.connectionSocket
	});
}