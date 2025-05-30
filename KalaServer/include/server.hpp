//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>

namespace KalaServer
{
	using std::string;
	using std::unique_ptr;
	using std::map;
	using std::vector;

	struct ErrorMessage
	{
		string error403;
		string error404;
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

		Server(
			int port,
			string serverName,
			string domainName,
			ErrorMessage errorMessages,
			string whitelistedRoutesFolder,
			vector<string> blacklistedKeywords,
			vector<string> whitelistedExtensions) :
			port(port),
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
			int port,
			const string& serverName,
			const string& domainName,
			const ErrorMessage& errorMessage,
			const string& whitelistedRoutesFolder,
			const vector<string>& blacklistedKeywords,
			const vector<string>& extensions);

		/// <summary>
		/// Runs the server loop. Use Core::Run instead of this.
		/// </summary>
		bool Run() const;

		/// <summary>
		/// Closes the server. Use Core::Quit instead of this.
		/// </summary>
		void Quit();

		string GetBannedBotsFilePath() { return bannedBotsFile; }

		/// <summary>
		/// Check whether this route is allowed to be accessed.
		/// If you access it you will get banned, it is used to keep away scrapers and bots.
		/// </summary>
		bool IsBlacklistedRoute(const string& route);

		/// <summary>
		/// Returns banned ip + reason if IP address is banned and shouldnt be allowed to access any routes.
		/// </summary>
		bool IsBannedIP(const string& ip);

		/// <summary>
		/// Throw a simple banned message for a banned IP trying to access any route.
		/// </summary>
		void StopBannedIP(
			const BannedIP& target,
			uintptr_t clientSocket);
		/// <summary>
		/// Add info about banned ip to banned-bots.txt.
		/// </summary>
		/// <param name="target"></param>
		void BanIP(
			const BannedIP& target,
			uintptr_t clientSocket);

		void SetServerName(const string& newServerName) { serverName = newServerName; }
		void SetDomainName(const string& newDomainName) { domainName = newDomainName; }

		void AddNewWhitelistedRoute(const string& rootPath, const string& filePath);
		void AddNewWhitelistedExtension(const string& newExtension);

		void RemoveWhitelistedRoute(const string& thisRoute);
		void RemoveWhitelistedExtension(const string& thisExtension);

		bool RouteExists(const string& thisRoute)
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

		string GetServerName() { return serverName; }
		string GetDomainName() { return domainName; }

		map<string, string> GetWhitelistedRoutes() { return whitelistedRoutes; }
		vector<string> GetWhitelistedExtensions() { return whitelistedExtensions; }

		string ServeFile(const string& route);
	private:
		void AddInitialWhitelistedRoutes();

		mutable uintptr_t serverSocket{}; //Current active socket
		map<string, string> whitelistedRoutes{}; //All routes that are allowed to be accessed
		string bannedBotsFile{}; //The path to the banned bots file.

		int port; //Local server port
		string serverName; //The server name used for cloudflare/dns calls
		string domainName; //The domain name that is launched

		ErrorMessage errorMessage; //File paths for server admin provided error pages, loads browser defaults otherwise.
		string whitelistedRoutesFolder; //The folder path relative to the server where all pages are inside of.
		vector<string> blacklistedKeywords; //All keywords that will ban you if you try to access any route with one of them inside it
		vector<string> whitelistedExtensions; //All extensions that are allowed to be accessed
	};
}