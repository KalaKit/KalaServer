//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"
//#include "external/ktypes.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::Server;
using KalaKit::Core::ErrorMessage;
using KalaKit::Core::DataFile;
using KalaKit::Core::ConsoleMessageType;
using KalaKit::Core::PopupReason;
using KalaKit::DNS::CloudFlare;
using KalaKit::DNS::CustomDNS;

using std::string;
using std::vector;
using std::filesystem::current_path;
using std::filesystem::path;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;
//using KalaKit::KalaTypes::u16;

int main()
{
	unsigned int port = 30000;

	unsigned int healthTimer = 600; //600 seconds (10 minutes) until health message ping

	string serverName = "KalaServer";
	string domainName = "thekalakit.com";
	
	string tunnelName = "KalaServer";

	ErrorMessage msg =
	{
		.error403 = "/errors/403",
		.error404 = "/errors/404",
		.error418 = "/errors/418",
		.error500 = "/errors/500"
	};
	DataFile dataFile =
	{
		.whitelistedRoutesFolder = "content",
		.whitelistedExtensionsFile = "whitelisted-extensions.txt",
		.whitelistedIPsFile = "whitelisted-ips.txt",
		.bannedIPsFile = "banned-ips.txt",
		.blacklistedKeywordsFile = "blacklisted-keywords.txt"
	};

	vector<string> registeredRoutes{};
	vector<string> adminRoutes
	{
		"/admin"
	};

	bool success = Server::Initialize(
		port,
		healthTimer,
		serverName,
		domainName,
		msg,
		dataFile,
		registeredRoutes,
		adminRoutes);

	if (!success) return 0;

	CloudFlare::Initialize(
		true,                 //shut down cloudflared at server exit
		tunnelName);
	
	//do not run dns and cloudflared together
	//DNS::RunDNS();
		
	KalaServer::PrintConsoleMessage(
		0,
		true,
		ConsoleMessageType::Type_Message,
		"SERVER",
		"Reached render loop successfully!");

	while (KalaServer::isRunning)
	{
		KalaServer::Run();
		sleep_for(milliseconds(10));
	}

	KalaServer::Quit();
}