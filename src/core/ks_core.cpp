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
#include <unistd.h>
#endif

#include <string>

#include "log_utils.hpp"
#include "string_utils.hpp"
#include "file_utils.hpp"
#include "thread_utils.hpp"

#include "core/ks_core.hpp"
#include "core/ks_cloudflare.hpp"
#include "core/ks_connect.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

using KalaHeaders::KalaString::SplitString;
using KalaHeaders::KalaString::TrimString;

using KalaHeaders::KalaThread::lockwait_m;
using KalaHeaders::KalaThread::unlock_m;

using KalaHeaders::KalaFile::WriteLinesToFile;
using KalaHeaders::KalaFile::ReadLinesFromFile;

using KalaServer::Core::KalaServerCore;

using std::to_string;
using std::string;

#ifdef _WIN32
using std::wstring;
#endif

namespace KalaServer::Core
{
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

	static vector<BannedIP> bannedIPs{};
	static mutex m_bannedIPs{};

	static vector<DomainRoute> routes{};
	static mutex m_routes{};

	static vector<string> blacklistedKeywords{};
	static mutex m_blacklistedKeywords{};

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
			if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			{
				Log::Print(
					"Failed to call WSAStartup!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				WSACleanup();

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

	bool KalaServerCore::IsReady() { return isReady; }

	bool KalaServerCore::IsCloudflareRequired() { return cloudflareRequired; }

	bool KalaServerCore::HasInternet()
	{
#ifdef _WIN32
		if (!startedWSA)
		{
			if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			{
				Log::Print(
					"Failed to call WSAStartup!",
					"INTERNET_ACCESS",
					LogType::LOG_ERROR,
					2);

				WSACleanup();

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

		return ok;
	}

	bool KalaServerCore::IsHealthy()
	{
		return cloudflareRequired
			? Cloudflare::IsTunnelAlive()
			&& Cloudflare::IsTunnelHealthy()
			: true;
	}

	string_view KalaServerCore::GetServerName() { return serverName; }
	const path& KalaServerCore::GetServerRoot() { return serverRoot; }
	const vector<string>& KalaServerCore::GetServerDomains() { return serverDomains; };
	string_view KalaServerCore::GetServerIP() { return serverIP; }
	u16 KalaServerCore::GetServerPort() { return serverPort; }

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
		lockwait_m(m_bannedIPs);
		for (const auto& b : bannedIPs)
		{
			if (b.targetIP == targetIP)
			{
				Log::Print(
					"Failed to ban IP '" + string(targetIP) + "' because it is already banned!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				unlock_m(m_bannedIPs);
				return false;
			}
		}

		bannedIPs.push_back({.targetIP = string(targetIP)});
		unlock_m(m_bannedIPs);

		return true;
	}
	bool KalaServerCore::UnbanIP(string_view targetIP)
	{
		lockwait_m(m_bannedIPs);
		for (auto it = bannedIPs.begin(); it != bannedIPs.end(); ++it)
		{
			if (it->targetIP == targetIP)
			{
				bannedIPs.erase(it);

				unlock_m(m_bannedIPs);
				return false;
			}
		}
		unlock_m(m_bannedIPs);

		Log::Print(
			"Failed to unban IP '" + string(targetIP) + "' because it has not been banned!",
			"SERVER",
			LogType::LOG_ERROR,
			2);

		return true;
	}

	bool KalaServerCore::SaveBannedIPsToDisk(const path& targetPath)
	{
		lockwait_m(m_bannedIPs);
		if (bannedIPs.empty())
		{
			Log::Print(
				"There are no banned IPs to save to disk.",
				"SERVER",
				LogType::LOG_INFO);

			unlock_m(m_bannedIPs);
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

			unlock_m(m_bannedIPs);
			return false;
		}

		unlock_m(m_bannedIPs);
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
	mutex& KalaServerCore::GetBannedIPsMutex() { return m_bannedIPs; }

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

		lockwait_m(m_routes);

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

				unlock_m(m_routes);

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

			unlock_m(m_routes);

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

				unlock_m(m_routes);

				return false;
			}
		}

		routes.push_back(
			{ 
				.domain = newRoute.domain, 
				.route = newRoute.route, 
				.routePath = cleanedPath 
			});

		unlock_m(m_routes);

		Log::Print(
			"Added new domain '" + newRoute.domain + "' with route '" + newRoute.route + "' and path '" + cleanedPath.string() + "'!",
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}
	bool KalaServerCore::RemoveRoute(const DomainRoute& existingRoute)
	{
		lockwait_m(m_routes);

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

			unlock_m(m_routes);

			return false;
		}

		routes.erase((it), routes.end());

		unlock_m(m_routes);

		Log::Print(
			"Removed existing domain '" + existingRoute.domain + "' with route '" + existingRoute.route + "'.",
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool KalaServerCore::SaveRoutesToDisk(const path& targetPath)
	{
		lockwait_m(m_routes);
		if (routes.empty())
		{
			Log::Print(
				"There are no routes to save to disk.",
				"SERVER",
				LogType::LOG_INFO);

			unlock_m(m_routes);
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

			unlock_m(m_routes);
			return false;
		}

		unlock_m(m_routes);
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
	mutex& KalaServerCore::GetRoutesMutex() { return m_routes; }

	bool KalaServerCore::AddBlacklistedKeyword(string_view newKeyword)
	{
		lockwait_m(m_blacklistedKeywords);

		for (const auto& r : blacklistedKeywords)
		{
			if (r == newKeyword)
			{
				Log::Print(
					"Failed to add new blacklisted keyword '" + string(newKeyword) + "' because it has already been added!",
					"SERVER",
					LogType::LOG_ERROR,
					2);

				unlock_m(m_blacklistedKeywords);

				return false;
			}
		}

		blacklistedKeywords.push_back(string(newKeyword));

		unlock_m(m_blacklistedKeywords);

		Log::Print(
			"Added new blacklisted keyword '" + string(newKeyword) + "'!",
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}
	bool KalaServerCore::RemoveBlacklistedKeyword(string_view existingKeyword)
	{
		lockwait_m(m_blacklistedKeywords);

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

			unlock_m(m_blacklistedKeywords);

			return false;
		}

		blacklistedKeywords.erase((it), blacklistedKeywords.end());

		unlock_m(m_blacklistedKeywords);

		Log::Print(
			"Removed existing blacklisted keyword '" + string(existingKeyword) + "'.",
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool KalaServerCore::SaveBlacklistedKeywordsToDisk(const path& targetPath)
	{
		lockwait_m(m_blacklistedKeywords);
		if (routes.empty())
		{
			Log::Print(
				"There are no blacklisted keywords to save to disk.",
				"SERVER",
				LogType::LOG_INFO);

			unlock_m(m_blacklistedKeywords);
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

			unlock_m(m_blacklistedKeywords);
			return false;
		}

		unlock_m(m_blacklistedKeywords);
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
	mutex& KalaServerCore::GetBlacklistedKeywordsMutex() { return m_blacklistedKeywords; }

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

		Connect::DisconnectListener();
		
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