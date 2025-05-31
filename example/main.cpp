//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <vector>
#include <filesystem>

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

int main()
{
	int port = 80;

	string serverName = "KalaServer";
	string domainName = "thekalakit.com";
	
	string tunnelName = "KalaServer";

	string tunnelTokenFileName = "tunneltoken.txt";
	string tunnelTokenFilePath = path(current_path() / tunnelTokenFileName).string();

	ErrorMessage msg{};
	msg.error403 = "/errors/403";
	msg.error404 = "/errors/404";
	msg.error418 = "/errors/418";
	msg.error500 = "/errors/500";
	
	static const string whitelistedRoutesFolder = "content/pages";

	static const vector<string> whitelistedExtensions =
	{
		".png", ".jpg", ".jpeg", ".ico",
		".mp3", ".wav", ".flac", ".ogg",
		".webp", ".webm", ".mp4", ".gif"
	};

	static const std::vector<std::string> blacklistedKeywords = 
	{
		/*
		//CMS / WordPress
		"wp", "wp1", "wordpress", "includes",
		"blog", "cms", "xml", "php",

		//admin / Control Panels
		"admin", "sito", "login", "register",

		//shop / Web presence
		"shop", "web",

		//years (common archive or crawl targets)
		"2026", "2025", "2024", "2023",
		"2022", "2021", "2020", "2019"
		*/
	};

	Server::Initialize(
		port,
		serverName,
		domainName,
		msg,
		whitelistedRoutesFolder,
		blacklistedKeywords,
		whitelistedExtensions);

	CloudFlare::Initialize(
		true,                 //shut down cloudflared at server exit
		tunnelName,
		tunnelTokenFilePath);
	
	//do not run dns and cloudflared together
	//DNS::RunDNS();
		
	KalaServer::PrintConsoleMessage(
		ConsoleMessageType::Type_Message,
		"Reached render loop successfully!");

	Server::server->Start();

	while (KalaServer::isRunning)
	{
		KalaServer::Run();
	}

	KalaServer::Quit();
}