//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_set>

//#include "external/ktypes.hpp"
#include "core/mimetype.hpp"
#include "core/event.hpp"

namespace KalaKit::Core
{
	using std::string;
	using std::unique_ptr;
	using std::vector;
	using std::atomic;
	using std::mutex;
	using std::unordered_set;
	using std::pair;
	//using KalaKit::KalaTypes::u16;

	static inline EventType sev_m = EventType::event_severity_message;
	static inline EventType sev_d = EventType::event_severity_debug;
	static inline EventType sev_w = EventType::event_severity_warning;
	static inline EventType sev_e = EventType::event_severity_error;
	static inline EventType rec_c = EventType::event_print_console_message;
	static inline EventType rec_p = EventType::event_create_popup;
	static inline EventType rec_e = EventType::event_send_email;

	enum class AccessLevel
	{
		access_user,
		access_registered,
		access_admin
	};

	struct Route
	{
		string route;
		string filePath;
		string mimeType;
		AccessLevel accessLevel;
	};
	
	//The account info of the email sender
	struct EmailSenderData
	{
		vector<EventType> events; //Choose what events to send emails for
		string username;          //Your email account
		string password;          //Your app password (read docs/email-app-password.txt for more info)
	};

	struct ErrorMessage
	{
		string error403;
		string error404;
		string error418;
		string error500;
	};

	struct DataFile
	{
		string whitelistedRoutesFolder;
		string whitelistedExtensionsFile;
		string whitelistedIPsFile;
		string bannedIPsFile;
		string blacklistedKeywordsFile;
	};

	enum class DataFileType
	{
		datafile_extension,
		datafile_whitelistedIP,
		datafile_bannedIP,
		datafile_blacklistedKeyword
	};

	//keeps track of user attempts to routes per second
	static inline unordered_map<string, unordered_map<string, int>> requestCounter;

	static inline mutex counterMutex;

	class Server
	{
	public:
		static inline unique_ptr<Server> server;

		//File paths for server admin provided error pages,
		//loads browser defaults otherwise.
		ErrorMessage errorMessage;

		//File paths for server admin provided data files
		//for ips, extensions, keywords and others.
		DataFile dataFile;
		
		//Your email username and app password to be able to send emails
		EmailSenderData emailSenderData;

		HealthPingData healthPingData;

		atomic<bool> canUpdateWhitelistedRoutes{ true };
		atomic<bool> canUpdateRouteAccess{ true };

		mutex clientSocketsMutex;
		unordered_set<uintptr_t> activeClientSockets;

		//All IPs that will always bypass ban filter
		vector<pair<string, string>> whitelistedIPs{};

		//All IPs banned permanently from server unless manually removed
		vector<pair<string, string>> bannedIPs{};

		//All keywords that trigger a ban
		vector<string> blacklistedKeywords{};

		//All routes that exist on disk
		vector<Route> whitelistedRoutes{};

		//All extensions whose files are allowed to be opened
		vector<string> whitelistedExtensions{};

		unsigned int rateLimitTimer; //If client connections to one route per second exceed this then that client gets banned.

		Server(
			unsigned int port,
			unsigned int rateLimitTimer,
			const string& serverName,
			const string& domainName,
			const HealthPingData& healthPingData,
			const ErrorMessage& errorMessage,
			const DataFile& dataFile,
			const EmailSenderData& emailSenderData) :
			port(port),
			rateLimitTimer(rateLimitTimer),
			serverName(serverName),
			domainName(domainName),
			healthPingData(healthPingData),
			errorMessage(errorMessage),
			dataFile(dataFile),
			emailSenderData(emailSenderData) {}

		/// <summary>
		/// Initializes the server. Must be ran first before any other components.
		/// </summary>
		static bool Initialize(
			unsigned int port,
			unsigned int rateLimitTimer,
			const string& serverName,
			const string& domainName,
			const HealthPingData& healthPingData,
			const ErrorMessage& errorMessage,
			const DataFile& datafile,
			const EmailSenderData& emailSenderData,
			const vector<string>& registeredRoutes,
			const vector<string>& adminRoutes);

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

		/// <summary>
		/// Send binary data to client.
		/// </summary>
		vector<char> ServeFile(
			const string& route,
			size_t rangeStart,
			size_t rangeEnd,
			size_t& outTotalSize,
			bool& outSliced);

		/// <summary>
		/// Check whether this route is allowed to be accessed.
		/// If you access it you will get banned, it is used to keep away scrapers and bots.
		/// </summary>
		bool IsBlacklistedRoute(const string& route);

		/// <summary>
		/// Returns banned ip + reason if IP address is banned and shouldnt be allowed to access any routes.
		/// </summary>
		pair<string, string> IsBannedClient(const string& ip) const;

		/// <summary>
		/// Add info about banned ip to banned-ips.txt.
		/// </summary>
		bool BanClient(const pair<string, string>& target) const;

		/// <summary>
		/// Returns true if given IP matches any host local ipv4 or ipv6.
		/// </summary>
		bool IsHost(const string& targetIP);

		/// <summary>
		/// Returns whitelisted ip + reason if IP address is whitelisted and should always be allowed to access any existing routes.
		/// </summary>
		pair<string, string> IsWhitelistedClient(const string& ip) const;

		/// <summary>
		/// Returns true if route requres registered user access.
		/// </summary>
		bool IsRegisteredRoute(const string& route);

		/// <summary>
		/// Returns true if route requres admin access.
		/// </summary>
		bool IsAdminRoute(const string& route);

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

		bool IsServerReady() const { return isServerReady; }
		string GetServerName() { return serverName; }
		string GetDomainName() { return domainName; }

		void GetFileData(DataFileType dataFileType) const;
		void GetWhitelistedRoutes() const;

		/// <summary>
		/// Closes the server. Use Core::Quit instead of this.
		/// </summary>
		void Quit() const;
	private:
		bool PreInitializeCheck() const;

		/// <summary>
		/// Calls 'ipconfig' and stores all host IPs in machineIPs vector.
		/// </summary>
		void GetMachineIPs();

		/// <summary>
		/// Reads 'banned-ips.txt' and stores all IPs and reasons in bannedIPs pair.
		/// </summary>
		void GetBannedIPs();

		/// <summary>
		/// Reads 'whitelisted-ips.txt' and stores all IPs and reasons in whitelistedIPs pair.
		/// </summary>
		void GetWhitelistedIPs();

		/// <summary>
		/// Assign route access level dynamically based off of registered and admin route vectors.
		/// </summary>
		void SetRouteAccessLevels();

		bool isServerReady = false; //Used to check if server is ready to start.

		mutable uintptr_t serverSocket{}; //Current active socket

		unsigned int port; //Local server port
		string serverName; //The server name used for cloudflare/dns calls
		string domainName; //The domain name that is launched
	};
}