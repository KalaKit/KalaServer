//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <vector>
#include <filesystem>

#include "core.hpp"
#include "server.hpp"
#include "cloudflare.hpp"
#include "dns.hpp"

using KalaServer::Core;
using KalaServer::Server;
using KalaServer::ErrorMessage;
using KalaServer::ConsoleMessageType;
using KalaServer::PopupReason;
using KalaServer::CloudFlare;
using KalaServer::DNS;

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
	
	string tunnelIDFileName = "tunnelid.txt";
	string tunnelIDFilePath = path(current_path() / tunnelIDFileName).string();
	
	string accountTagFileName = "accounttag.txt";
	string accountTagFilePath = path(current_path() / accountTagFileName).string();

	ErrorMessage msg{};
	msg.error403 = "/errors/403";
	msg.error404 = "/errors/404";
	msg.error500 = "/errors/500";
	
	static const string whitelistedRoutesFolder = "content/pages";

	static const vector<string> whitelistedExtensions =
	{
		".png", ".jpg", ".jpeg", ".ico",
		".mp3", ".wav", ".flac", ".ogg",
		".webp", ".webm", ".mp4", ".gif"
	};

	Server::Initialize(
		port,
		serverName,
		domainName,
		msg,
		whitelistedRoutesFolder,
		whitelistedExtensions);

	CloudFlare::Initialize(
		true,                 //shut down cloudflared at server exit
		tunnelName,
		tunnelTokenFilePath,
		tunnelIDFilePath,
		accountTagFilePath);
	
	//do not run dns and cloudflared together
	//DNS::RunDNS();
		
	Core::PrintConsoleMessage(
		ConsoleMessageType::Type_Message,
		"Reached render loop successfully!");

	while (Core::isRunning)
	{
		Core::Run();
	}

	Core::Quit();
}