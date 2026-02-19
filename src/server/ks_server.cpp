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

#include "server/ks_server.hpp"
#include "server/ks_cloudflare.hpp"
#include "server/ks_connect.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

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

	static u16 port{};
	static string serverName{};
	static string domainName{};
	static path serverRoot{};

	bool ServerCore::Initialize(
		u16 newPort,
		string_view newServerName,
		string_view newDomainName,
		const path& newServerRoot,
		bool requireCloudflare)
	{
		Log::Print(
			"Starting to initialize server '" + string(newServerName) 
			+ "' at port '" + to_string(newPort) 
			+ "' with domain '" + string(newDomainName)
			+ "' and server root '" + newServerRoot.string() + "'",
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

		if (!exists(newServerRoot))
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its server root '" + newServerRoot.string() + "' is empty or does not exist!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
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

#ifdef _WIN32
		if (!startedWSA)
		{
			if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			{
				Log::Print(
					"Failed to check internet state for server '" + ServerCore::GetServerName() + "' because WSAStartup failed!",
					"INTERNET_ACCESS",
					LogType::LOG_ERROR,
					2);

				WSACleanup();

				return false;
			}
			startedWSA = true;
		}
#endif

		port = newPort;
		serverName = newServerName;
		domainName = newDomainName;
		serverRoot = newServerRoot;

		cloudflareRequired = requireCloudflare;

		isInitialized = true;
		if (!cloudflareRequired) isReady = true;

		Log::Print(
			"Created new server '" + serverName + "'!",
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
				"Failed to check internet state for server '" + ServerCore::GetServerName() + "' because socket creation failed!",
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
				"Failed to check internet state for server '" + ServerCore::GetServerName() + "' because socket creation failed!",
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

	u16 ServerCore::GetPort() { return port; }
	const string& ServerCore::GetServerName() { return serverName; }
	const string& ServerCore::GetDomainName() { return domainName; }
	const path& ServerCore::GetServerRoot() { return serverRoot; }

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