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

using KalaKit::Core::KalaServer;
using KalaKit::Core::Server;
using KalaKit::Core::ErrorMessage;
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

int main()
{
	unsigned int port = 30000;

	unsigned int healthTimer = 600; //600 seconds (10 minutes) until health message ping

	string serverName = "KalaServer";
	string domainName = "thekalakit.com";
	
	string tunnelName = "KalaServer";

	ErrorMessage msg{};
	msg.error403 = "/errors/403";
	msg.error404 = "/errors/404";
	msg.error418 = "/errors/418";
	msg.error500 = "/errors/500";
	
	static const string whitelistedRoutesFolder = "content";

	static const vector<string> whitelistedExtensions =
	{
		".html", ".css", ".js", ".txt",
		".png", ".jpg", ".jpeg", ".ico",
		".mp3", ".wav", ".flac", ".ogg",
		".webp", ".webm", ".mp4", ".gif"
	};

	static const vector<string> blacklistedKeywords =
	{
		//CMS / Framework probes
		"wp", "joomla", "drupal", "magento", "prestashop", "template", "theme", "skins",

		//admin panels / privileged access points
		"admin", "cpanel", "panel", "adminer", "controlpanel", "sito",

		//malware kits / scams / crypto abuse
		"twint", "lkk", "btc", "eth", "monero", "wallet", "crypto",
		"stealer", "inject", "skimmer", "grabber", "phish", "scam",
		"cryptojack", "ransom",

		//e-commerce abuse / payment hijack
		"cart", "checkout", "pay", "invoice", "order", "billing",
		"paypal", "stripe",

		//exploit routes / system file probes
		"cgi-bin", "shell", "backdoor", "cmd", "exploit", "passwd",
		"proc", "env", "id_rsa", "vuln", "sqlmap",

		//configs / database / dump / backups
		"config.php", "database", "dump", "db", "mysql", "sqlite", ".env", "phpinfo",

		//static file exposure
		".git", ".svn", ".hg", "index.php~",

		//web API abuse / dev endpoints
		"swagger", "graphql", "actuator", "metrics", "debug", "logs", "monitoring",

		//archive/fallback scan attempts
		"old", "archive", "test", "dev", "staging",

		//suspicious extensions / file types
		".tar", ".zip", ".gz", ".7z", ".rar", ".log"
	};

	Server::Initialize(
		port,
		healthTimer,
		serverName,
		domainName,
		msg,
		whitelistedRoutesFolder,
		blacklistedKeywords,
		whitelistedExtensions);

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