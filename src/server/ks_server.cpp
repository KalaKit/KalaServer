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

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/string_utils.hpp"

#include "server/ks_server.hpp"
#include "server/ks_cloudflare.hpp"
#include "server/ks_connect.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaString::SplitString;

using KalaServer::Core::KalaServerCore;

using std::to_string;
using std::string;

namespace KalaServer::Server
{
	static bool isInitialized{};
	static bool isReady{};

	static bool cloudflareRequired{};
	static bool isHealthy{};

#ifdef _WIN32
	static bool startedWSA{};
	WSADATA wsaData{};
#endif

	static string serverName{};
	static path serverRoot{};
	static vector<string> serverDomains{};
	static string serverIP{};
	static u16 serverPort{};

	bool ServerCore::Initialize(
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

		if (!Connect::IsValidIP(newServerIP))
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
					"Failed to check internet state for server '" + serverName + "' because WSAStartup failed!",
					"INTERNET_ACCESS",
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

	bool ServerCore::IsInitialized() { return isInitialized; }

	bool ServerCore::IsReady() { return isReady; }

	bool ServerCore::IsCloudflareRequired() { return cloudflareRequired; }

	bool ServerCore::HasInternet()
	{
#ifdef _WIN32
		SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
		{
			Log::Print(
				"Failed to check internet state for server '" + serverName + "' because socket creation failed!",
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

	bool ServerCore::IsHealthy()
	{
		return cloudflareRequired
			? Cloudflare::IsTunnelAlive()
			&& Cloudflare::IsTunnelHealthy()
			: true;
	}

	string_view ServerCore::GetServerName() { return serverName; }
	const path& ServerCore::GetServerRoot() { return serverRoot; }
	const vector<string>& ServerCore::GetServerDomains() { return serverDomains; };
	string_view ServerCore::GetServerIP() { return serverIP; }
	u16 ServerCore::GetServerPort() { return serverPort; }

	void ServerCore::Shutdown()
	{
		if (!ServerCore::IsReady())
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

	void ServerCore::SetServerReadyState(bool state) { isReady = state; }
}