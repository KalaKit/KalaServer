//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <filesystem>
#include <vector>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"
#include "server/ks_cloudflare.hpp"

namespace KalaServer::Server
{
	using std::string;
	using std::string_view;
	using std::filesystem::path;
	using std::vector;

	using KalaHeaders::KalaThread::abool;

	using u16 = uint16_t;
	using u32 = uint32_t;

	//Minimum port, 1-1023 requires admin/root
	constexpr u16 MIN_PORT_RANGE = 1u;
	//Maximum port, cannot go past 16-bit unsigned integer TCP and UDP port fields
	constexpr u16 MAX_PORT_RANGE = 65535u;

	class LIB_API ServerCore
	{
	friend class Cloudflare;
	public:
		//Initialize a new server on this process.
		//Server name helps distinguish this server from other servers.
		//Server root is the true origin where the server will expose
		//routes from relative to where the process was run.
		//Port is the local TCP port this server binds to and listens on.
		//Domains are the allowed hostnames this server will accept connections for.
		//Set requireCloudflare to false if you dont want to use Cloudflare tunnel,
		//otherwise it must be enabled before any connections are accepted.
		static bool Initialize(
			string_view serverName,
			const path& serverRoot,
			vector<string> serverDomains,
			u16 port,
			bool requireCloudflare);

		//Returns true if this server instance has been initialized successfully
		static bool IsInitialized();

		//Returns true if the server Cloudflare backend has been initialized successfully,
		//the server cannot be started if its not ready yet, even if its instance is already initialized
		static bool IsReady();

		//Returns true if user set requireCloudflare as true during ServerCore::Initialize
		static bool IsCloudflareRequired();

		//Returns true if this process can reach http://1.1.1.1 on port 53
		static bool HasInternet();

		//Returns false if server cannot connect to google.com
		//or if cloudflare tunnel is not healthy if cloudflare is required
		static bool IsHealthy();

		static const string& GetServerName();
		static const path& GetServerRoot();
		static const vector<string>& GetDomains();
		static u16 GetPort();

		//Close all sockets and clear all server resources
		static void Shutdown();
	private:
		static void SetServerReadyState(bool state);
	};
}