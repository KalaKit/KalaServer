//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <chrono>

#include "core_utils.hpp"
#include "thread_utils.hpp"

#include "server/ks_cloudflare.hpp"

namespace KalaServer::Server
{
	using std::string;
	using std::string_view;
	using std::filesystem::path;
	using std::vector;
	using std::chrono::steady_clock;

	using KalaHeaders::KalaThread::abool;
	using KalaHeaders::KalaThread::mutex;

	using u16 = uint16_t;
	using u32 = uint32_t;

	//Minimum port, 1-1023 requires admin/root
	constexpr u16 MIN_PORT_RANGE = 1u;
	//Maximum port, cannot go past 16-bit unsigned integer TCP and UDP port fields
	constexpr u16 MAX_PORT_RANGE = 65535u;

	struct LIB_API DomainRoute
	{
		string domain{};  //the domain this route is added to
		string route{};   //how the route is displayed in the url
		path routePath{}; //path relative to executable path or full path

		bool operator==(const DomainRoute&) const = default;
	};

	struct LIB_API BannedIP
	{
		string targetIP{};

		//Leave unassigned to mark as permanent ban
		steady_clock::time_point expiresAt{};
	};

	class LIB_API ServerCore
	{
	friend class Cloudflare;
	public:
		//Initialize a new server on this process.
		//Server name helps distinguish this server from other servers.
		//Server root is the true origin where the server will expose
		//routes from relative to where the process was run.
		//Server domains are the allowed hostnames this server will accept connections for.
		//Server IP is the IP address users will connect to.
		//Server port is the local TCP port this server binds to and listens on.
		//Set requireCloudflare to false if you dont want to use Cloudflare tunnel,
		//otherwise it must be enabled before any connections are accepted.
		static bool Initialize(
			string_view serverName,
			const path& serverRoot,
			vector<string> serverDomains,
			string_view serverIP,
			u16 serverPort,
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

		static string_view GetServerName();
		static const path& GetServerRoot();
		static const vector<string>& GetServerDomains();
		static string_view GetServerIP();
		static u16 GetServerPort();

		//Does this IP match any valid ipv4 or ipv6 structure
		static bool IsValidIP(string_view targetIP);

		//Ban IP, doesn't matter if it is currently connected or not
		static bool BanIP(string_view targetIP);
		//Unban existing IP, doesn't matter if its currently connected or not
		static bool UnbanIP(string_view targetIP);

		//Saves all existing banned ips to disk, overwrites existing txt file
		static bool SaveBannedIPsToDisk(const path& targetPath);
		//Loads all saved banned ips and appends to current list, duplicates are skipped
		static bool LoadBannedIPsFromDisk(const path& targetPath);

		//Returns mutable ref to existing banned IPs
		static const vector<BannedIP>& GetBannedIPs();

		//Returns the mutex that must be used for all the getters and setters for banned IPs
		static mutex& GetBannedIPsMutex();

		//Add new route, cannot add add duplicates if domain+route matches,
		//cannot add routes if their path matches any existing route path of the same domain
		static bool AddRoute(const DomainRoute& newRoute);
		//Remove existing route
		static bool RemoveRoute(const DomainRoute& existingRoute);

		//Saves all existing routes to disk, overwrites existing txt file
		static bool SaveRoutesToDisk(const path& targetPath);
		//Loads all saved routes and appends to current list, duplicates are skipped
		static bool LoadRoutesFromDisk(const path& targetPath);

		//Returns mutable ref to existing routes
		static const vector<DomainRoute>& GetRoutes();

		//Returns the mutex that must be used for all the getters and setters for routes
		static mutex& GetRoutesMutex();

		//Add new blacklisted keyword, cannot add duplicates
		static bool AddBlacklistedKeyword(string_view newKeyword);
		//Remove existing blacklisted keyword
		static bool RemoveBlacklistedKeyword(string_view existingKeyword);

		//Saves all existing blacklisted keywords to disk, overwrites existing txt file
		static bool SaveBlacklistedKeywordsToDisk(const path& targetPath);
		//Loads all saved blacklisted keywords and appends to current list, duplicates are skipped
		static bool LoadBlacklistedKeywordsFromDisk(const path& targetPath);

		//Returns mutable ref to existing blacklisted keywords
		static const vector<string>& GetBlacklistedKeywords();

		//Returns the mutex that must be used for all the getters and setters for blacklisted keywords
		static mutex& GetBlacklistedKeywordsMutex();

		//Close all sockets and clear all server resources
		static void Shutdown();
	private:
		static void SetServerReadyState(bool state);
	};
}