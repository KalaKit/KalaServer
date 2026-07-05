//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/capability.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cerrno>
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
#include "core/ks_cloudflare.hpp"
#include "core/ks_response.hpp"

using KalaHeaders::KalaCore::ToVar;

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

static bool isInitialized{};
static bool isReady{};

static bool cloudflareRequired{};

#ifdef _WIN32
	static bool startedWSA{};
	WSADATA wsaData{};
#endif

static string serverName{};
static path serverRoot{};
static vector<string> serverDomains{};
static string serverIP{};
static u16 serverPort{};

static string serverIPDomain{};
static string serverIPPortDomain{};

static Connection listenerSocket{};
static vector<Connection> connectSockets{};

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

	bool KalaServerCore::Initialize(
		string_view newServerName,
		const path& newServerRoot,
		vector<string> newServerDomains,
		string_view newServerIP,
		u16 newServerPort,
		bool requireCloudflare)
	{
		Log::Print(
			"Starting to initialize server '" + string(newServerName) + "'.",
			"SERVER",
			LogType::LOG_INFO);

		if (newServerName.empty()
			|| newServerName.length() > 50)
		{
			Log::Print(
				"Failed to initialize server because its name is empty or too long!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!exists(newServerRoot))
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its server root '" + newServerRoot.string() + "' is empty or does not exist!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (newServerDomains.empty())
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because it has no assigned domains!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		for (const auto& d : newServerDomains)
		{
			if (d.empty())
			{
				Log::Print(
					"Server '" + string(newServerName) + "' did not initialize all domains because one of its assigned domains is empty!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				continue;
			}
			if (d.find('.') == string::npos)
			{
				Log::Print(
					"Server '" + string(newServerName) + "' did not initialize all domains because domain '" + d + "' has no extension splitters!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				continue;
			}

			vector<string> split = SplitString(d, ".");
			if (split.size() != 2)
			{
				Log::Print(
					"Server '" + string(newServerName) + "' did not initialize all domains because domain '" + d + "' has a malformed structure!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				continue;
			}

			serverDomains.push_back(d);
		}

		if (MIN_PORT_RANGE == 0)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because the MIN_PORT_RANGE value was set to 0!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (MIN_PORT_RANGE > MAX_PORT_RANGE)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because the MIN_PORT_RANGE value was set higher than the MAX_PORT_RANGE value!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (newServerPort < MIN_PORT_RANGE
			|| newServerPort > MAX_PORT_RANGE)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its port '" + to_string(newServerPort) + "' is out of range!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!IsValidIP(newServerIP))
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its IP '" + string(newServerIP) + "' is not a valid IP address!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

#ifdef _WIN32
		if (!startedWSA)
		{
			u32 errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (errorCode != 0)
			{
				ForceClose(
					"Server init error",
					"Failed to call WSASTARTUP! Error code: " + to_string(errorCode));

				return false;
			}
			startedWSA = true;
		}
#endif

		serverName = newServerName;
		serverRoot = newServerRoot;
		serverIP = newServerIP;
		serverPort = newServerPort;

		cloudflareRequired = requireCloudflare;

		isInitialized = true;
		if (!cloudflareRequired) isReady = true;

		string output = 
			"Created new server '" + serverName + "' "
			"with root '" + serverRoot.string() + "', "
			"domains ";

		for (const auto& d : serverDomains)
		{
			output += "'" + d + "'" + ", ";
		}

		output += "IP '" + serverIP + "' ";

		output += "and port '" + to_string(serverPort) + "'!";

		Log::Print(
			output,
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool KalaServerCore::IsInitialized() { return isInitialized; }

	void KalaServerCore::Update()
	{
		if (!isInitialized)
		{
			Log::Print(
				"Cannot update server '" + serverName + "' because it has not been initialized!",
				"SERVER_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!isReady)
		{
			Log::Print(
				"Cannot update server '" + serverName + "' because it is not ready for use yet!",
				"SERVER_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!HasInternet())
		{
			Log::Print(
				"Cannot update server '" + serverName + "' because it has no internet!",
				"SERVER_CORE",
				LogType::LOG_ERROR,
				2);

			return;
		}


	}

	bool KalaServerCore::IsReady() { return isReady; }

	bool KalaServerCore::IsCloudflareRequired() { return cloudflareRequired; }

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
					"Failed to call WSASTARTUP! Error code: " + to_string(errorCode));

				return false;
			}
			startedWSA = true;
		}

		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
		{
			string failReason = KalaServerCore::ErrorToString(WSAGetLastError());

			Log::Print(
				"Failed to check internet state for server '" + serverName + "' because socket creation failed! Reason: " + failReason,
				"INTERNET_ACCESS",
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
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
		{
			Log::Print(
				"Failed to check internet state for server '" + serverName + "' because socket creation failed!",
				"INTERNET_ACCESS",
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
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif

			return false;
		}

#ifdef _WIN32
		bool ok = (connect(sock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR);
		closesocket(sock);
#else
		bool ok = (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0);
		close(sock);
#endif

		cachedResult = ok;
		lastCheckedTime = now;

		return cachedResult;
	}

	string_view KalaServerCore::GetServerName() { return serverName; }
	const path& KalaServerCore::GetServerRoot() { return serverRoot; }
	const vector<string>& KalaServerCore::GetServerDomains() { return serverDomains; };
	string_view KalaServerCore::GetServerIP() { return serverIP; }
	u16 KalaServerCore::GetServerPort() { return serverPort; }

	const Connection& KalaServerCore::GetListenerSocket() { return listenerSocket; }

	const vector<Connection> KalaServerCore::GetConnectSockets()
	{ 
		static vector<Connection> connectSocketView{};

		connectSocketView.clear();
		connectSocketView.reserve(connectSockets.size());

		for (const auto& c : connectSockets)
		{
			connectSocketView.push_back(c);
		}

		return connectSocketView;
	}

	void KalaServerCore::DisconnectConnectedUser(uintptr_t targetSocket)
	{
		if (!KalaServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + string(KalaServerCore::GetServerName()) + "' because the server is not running or not ready!",
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
				"Failed to disconnect target via socket for server '" + string(KalaServerCore::GetServerName()) + "' because the socket is unassigned or invalid!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Connection targetUser{};

		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			ksocket sock =
#ifdef _WIN32
				ToVar<SOCKET>(it->connectionSocket);
#else
				ToVar<int>(i->connectionSocket);
#endif

			if (sock == target)
			{
				targetUser = std::move(*it);
				connectSockets.erase(it);

				break;
			}
		}

		if (targetUser.connectionSocket == NULL)
		{
			Log::Print(
				"Failed to disconnect target via socket for server '" + string(KalaServerCore::GetServerName()) + "' because the target socket was not found!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		targetUser.isRunning = false;

#ifdef _WIN32
		ksocket cs = ToVar<SOCKET>(targetUser.connectionSocket);
		if (cs != invalid_socket)
		{
			shutdown(cs, SD_BOTH);
			closesocket(cs);
		}
#else
		ksocket cs = ToVar<int>(targetUser.connectionSocket);
		if (cs != invalid_socket)
		{
			shutdown(cs, SHUT_RDWR);
			close(cs);
		}
#endif

		Log::Print(
			"Disconnected target via socket for server '" + string(KalaServerCore::GetServerName()) + "'!",
			"DISCONNECT_TARGET",
			LogType::LOG_SUCCESS);
	}
	void KalaServerCore::DisconnectConnectedUser(string_view targetIP)
	{
				if (!KalaServerCore::IsReady())
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) + "' for server '" + string(KalaServerCore::GetServerName()) + "' because the server is not running or not ready!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!KalaServerCore::IsValidIP(targetIP))
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) + "' for server '" + string(KalaServerCore::GetServerName()) + "' because the IP structure is invalid!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Connection targetUser{};

		for (auto it = connectSockets.begin(); it != connectSockets.end(); ++it)
		{
			string connectionIP = it->connectionIP;

			if (connectionIP == targetIP)
			{
				targetUser = std::move(*it);
				connectSockets.erase(it);

				break;
			}
		}

		if (targetUser.connectionSocket == NULL)
		{
			Log::Print(
				"Failed to disconnect target via IP '" + string(targetIP) + "' for server '" + string(KalaServerCore::GetServerName()) + "' because the target IP was not found!",
				"DISCONNECT_TARGET",
				LogType::LOG_ERROR,
				2);

			return;
		}

		targetUser.isRunning = false;

#ifdef _WIN32
		ksocket cs = ToVar<SOCKET>(targetUser.connectionSocket);
		if (cs != invalid_socket)
		{
			shutdown(cs, SD_BOTH);
			closesocket(cs);
		}
#else
		ksocket cs = ToVar<int>(targetUser,connectionSocket);
		if (cs != invalid_socket)
		{
			shutdown(cs, SHUT_RDWR);
			close(cs);
		}
#endif

		Log::Print(
			"Disconnected target via IP '" + string(targetIP) + "' for server '" + string(KalaServerCore::GetServerName()) + "'!",
			"DISCONNECT_TARGET",
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

	bool KalaServerCore::BanIP(string_view targetIP)
	{
		for (const auto& b : bannedIPs)
		{
			if (b.targetIP == targetIP)
			{
				Log::Print(
					"Failed to ban IP '" + string(targetIP) + "' because it is already banned!",
					"SERVER",
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
			"SERVER",
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
				"SERVER",
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
				"SERVER",
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
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		//TODO: add setup

		return true;
	}

	const vector<BannedIP>& KalaServerCore::GetBannedIPs() { return bannedIPs; }

	bool KalaServerCore::AddRoute(const DomainRoute& newRoute)
	{
		bool foundDomain{};
		for (const auto& d : serverDomains)
		{
			if (d == newRoute.domain)
			{
				foundDomain = true;
				break;
			}
		}

		if (!foundDomain)
		{
			Log::Print(
				"Failed to add new route with domain '" + newRoute.domain + "' and route '" + newRoute.route + "' because the domain does not exist!",
				"SERVER",
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
					"SERVER",
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
				"SERVER",
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
					"SERVER",
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
			"SERVER",
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
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		routes.erase((it), routes.end());

		Log::Print(
			"Removed existing domain '" + existingRoute.domain + "' with route '" + existingRoute.route + "'.",
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool KalaServerCore::SaveRoutesToDisk(const path& targetPath)
	{
		if (routes.empty())
		{
			Log::Print(
				"There are no routes to save to disk.",
				"SERVER",
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
				"SERVER",
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
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		//TODO: add setup

		return true;
	}

	const vector<DomainRoute>& KalaServerCore::GetRoutes() { return routes; }

	bool KalaServerCore::AddBlacklistedKeyword(string_view newKeyword)
	{
		for (const auto& r : blacklistedKeywords)
		{
			if (r == newKeyword)
			{
				Log::Print(
					"Failed to add new blacklisted keyword '" + string(newKeyword) + "' because it has already been added!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				return false;
			}
		}

		blacklistedKeywords.push_back(string(newKeyword));

		Log::Print(
			"Added new blacklisted keyword '" + string(newKeyword) + "'!",
			"SERVER",
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
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		blacklistedKeywords.erase((it), blacklistedKeywords.end());

		Log::Print(
			"Removed existing blacklisted keyword '" + string(existingKeyword) + "'.",
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool KalaServerCore::SaveBlacklistedKeywordsToDisk(const path& targetPath)
	{
		if (routes.empty())
		{
			Log::Print(
				"There are no blacklisted keywords to save to disk.",
				"SERVER",
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
				"SERVER",
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
				"SERVER",
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
		if (!KalaServerCore::IsReady())
		{
			Log::Print(
				"Cannot shut down the server because it is not running or not ready!",
				"SERVER_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (listenerSocket.connectionSocket == NULL)
		{
			Log::Print(
				"Failed to disconnect listener for server '" + string(KalaServerCore::GetServerName()) + "' because the server has no listener socket!",
				"LISTENER_DISCONNECT",
				LogType::LOG_WARNING);

			return;
		}

		ksocket ls = 
#ifdef _WIN32
			ToVar<SOCKET>(listenerSocket.connectionSocket);
#else
			ToVar<int>(listenerSocket.connectionSocket);
#endif

		if (ls == UNASSIGNED_SOCKET_VALUE)
		{
			Log::Print(
				"Failed to disconnect listener for server '" + string(KalaServerCore::GetServerName()) + "' because the server has not assigned a listener socket!",
				"LISTENER_DISCONNECT",
				LogType::LOG_WARNING);

			return;
		}

		listenerSocket.isRunning = false;

#ifdef _WIN32
		ksocket thisls = ToVar<SOCKET>(listenerSocket.connectionSocket);
		if (thisls != invalid_socket)
		{
			shutdown(thisls, SD_BOTH);
			closesocket(thisls);
		}
#else
		ksocket thisls = ToVar<int>(listenerSocket.connectionSocket);
		if (thisls != invalid_socket)
		{
			shutdown(thisls, SHUT_RDWR);
			close(thisls);
		}
#endif

		vector<Connection> cconnects{};

		cconnects = std::move(connectSockets);

		for (auto& conn : cconnects)
		{
			conn.isRunning = false;

#ifdef _WIN32
			ksocket cs = ToVar<SOCKET>(conn.connectionSocket);
			if (cs != invalid_socket)
			{
				shutdown(cs, SD_BOTH);
				closesocket(cs);
			}
#else
			ksocket cs = ToVar<int>(conn.connectionSocket);
			if (cs != invalid_socket)
			{
				shutdown(cs, SHUT_RDWR);
				close(cs);
			}
#endif
		}
		
#ifdef _WIN32
		if (startedWSA)
		{
			WSACleanup();
			startedWSA = false;
		}
#endif
	}

	void KalaServerCore::SetServerReadyState(bool state) { isReady = state; }
}