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
using KalaServer::PopupReason;
using KalaServer::CloudFlare;
using KalaServer::DNS;

using std::string;
using std::vector;
using std::filesystem::exists;
using std::filesystem::current_path;
using std::filesystem::path;

static string GetTunnelCommand(const string& tunnelFilePath)
{
	if (!exists(tunnelFilePath))
	{
		
	}
}

int main()
{
	int port = 80;

	string serverName = "KalaServer";
	string tunnelName = "KalaServer";

	string tunnelFileName = "tunnelfile.txt";
	string tunnelFilePath = path(current_path() / tunnelFileName).string();
	
	string domainName = "thekalakit.com";

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
		tunnelName,
		tunnelFilePath);
	
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