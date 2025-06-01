//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_set>

namespace KalaKit::Core
{
	using std::string;
	using std::unique_ptr;
	using std::map;
	using std::vector;
	using std::atomic;
	using std::mutex;
	using std::unordered_set;

	struct ErrorMessage
	{
		string error403;
		string error404;
		string error418;
		string error500;
	};

	struct BannedIP
	{
		string IP;
		string reason;
	};
	
	class Server
	{
	public:
		static inline unique_ptr<Server> server;

		//File paths for server admin provided error pages,
		//loads browser defaults otherwise.
		ErrorMessage errorMessage;

		Server(
			unsigned int port,
			unsigned int healthTimer,
			string serverName,
			string domainName,
			ErrorMessage errorMessages,
			string whitelistedRoutesFolder,
			vector<string> blacklistedKeywords,
			vector<string> whitelistedExtensions) :
			port(port),
			healthTimer(healthTimer),
			serverName(serverName),
			domainName(domainName),
			errorMessage(errorMessage),
			whitelistedRoutesFolder(whitelistedRoutesFolder),
			blacklistedKeywords(blacklistedKeywords),
			whitelistedExtensions(whitelistedExtensions) {
		}

		/// <summary>
		/// Initializes the server. Must be ran first before any other components.
		/// </summary>
		static void Initialize(
			unsigned int port,
			unsigned int healthTimer,
			const string& serverName,
			const string& domainName,
			const ErrorMessage& errorMessage,
			const string& whitelistedRoutesFolder,
			const vector<string>& blacklistedKeywords,
			const vector<string>& extensions);

		/// <summary>
		/// Starts up the server accept loop and health status report.
		/// Run this only once, not every frame.
		/// </summary>
		/// <param name="healthTimer">How often should the health report be sent (seconds)?</param>
		void Start() const;

		/// <summary>
		/// Returns true if a connection to google.com can be made.
		/// </summary>
		bool HasInternet();

		/// <summary>
		/// Returns true if the current server tunnel is alive and healthy.
		/// </summary>
		bool IsTunnelAlive(uintptr_t tunnelHandle);

		string ServeFile(const string& route);

		string GetBannedBotsFilePath() { return bannedBotsFile; }

		/// <summary>
		/// Check whether this route is allowed to be accessed.
		/// If you access it you will get banned, it is used to keep away scrapers and bots.
		/// </summary>
		bool IsBlacklistedRoute(const string& route);

		/// <summary>
		/// Returns banned ip + reason if IP address is banned and shouldnt be allowed to access any routes.
		/// </summary>
		bool IsBannedIP(const string& ip) const;

		/// <summary>
		/// Add info about banned ip to banned-bots.txt.
		/// </summary>
		/// <param name="target"></param>
		void BanIP(
			const BannedIP& target,
			uintptr_t clientSocket) const;

		/// <summary>
		/// Returns true if given IP matches any host local ipv4 or ipv6.
		/// </summary>
		bool IsHost(const string& targetIP);

		/// <summary>
		/// Allows server to start accepting connections. Do not call manually.
		/// </summary>
		void SetServerReadyState(bool newReadyState) { isServerReady = newReadyState; };

		void SetServerName(const string& newServerName) { serverName = newServerName; }
		void SetDomainName(const string& newDomainName) { domainName = newDomainName; }

		void AddNewWhitelistedRoute(const string& rootPath, const string& filePath) const;
		void AddNewWhitelistedExtension(const string& newExtension) const;

		void RemoveWhitelistedRoute(const string& thisRoute) const;
		void RemoveWhitelistedExtension(const string& thisExtension) const;

		bool RouteExists(const string& thisRoute) const
		{
			return whitelistedRoutes.contains(thisRoute);
		}
		bool ExtensionExists(const string& thisExtension)
		{
			for (const auto& extension : whitelistedExtensions)
			{
				if (extension == thisExtension) return true;
			}
			return false;
		}

		bool IsServerReady() const { return isServerReady; }
		string GetServerName() { return serverName; }
		string GetDomainName() { return domainName; }

		map<string, string> GetWhitelistedRoutes() { return whitelistedRoutes; }
		vector<string> GetWhitelistedExtensions() { return whitelistedExtensions; }

		/// <summary>
		/// Closes the server. Use Core::Quit instead of this.
		/// </summary>
		void Quit() const;
	private:
		void AddInitialWhitelistedRoutes() const;

		/// <summary>
		/// Header parser for getting the results from a cloudflared header.
		/// </summary>
		string ExtractHeader(
			const string& request,
			const string& headerName);

		/// <summary>
		/// Calls 'ipconfig' and stores all host IPs in machineIPs vector.
		/// </summary>
		void UpdateIPs();

		/// <summary>
		/// Handle each client in its own thread.
		/// </summary>
		void HandleClient(uintptr_t);

		void SocketCleanup(uintptr_t clientSocket);

		bool isServerReady = false; //Used to check if server is ready to start.

		mutable uintptr_t serverSocket{}; //Current active socket
		mutex clientSocketsMutex;
		unordered_set<uintptr_t> activeClientSockets;

		map<string, string> whitelistedRoutes{}; //All routes that are allowed to be accessed
		string bannedBotsFile{}; //The path to the banned bots file.

		unsigned int port; //Local server port
		unsigned int healthTimer; //Countdown until server reports health check.
		string serverName; //The server name used for cloudflare/dns calls
		string domainName; //The domain name that is launched

		string whitelistedRoutesFolder; //The folder path relative to the server where all pages are inside of.
		vector<string> blacklistedKeywords; //All keywords that will ban you if you try to access any route with one of them inside it
		vector<string> whitelistedExtensions; //All extensions that are allowed to be accessed
	};
}