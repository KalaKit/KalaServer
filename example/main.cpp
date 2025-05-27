//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <vector>

#include "http_server.hpp"

using KalaServer::Server;
using KalaServer::ErrorMessage;

using std::string;
using std::vector;

int main()
{
	static const vector<string> whitelistedExtensions = 
	{
		".png", ".jpg", ".jpeg", ".ico",
		".mp3", ".wav", ".flac", ".ogg",
		".webp", ".webm", ".mp4", ".gif"
	};	
	
	static const string whitelistedRoutesFolder = "content/pages";

	ErrorMessage msg{};
	msg.error403 = "/errors/403";
	msg.error404 = "/errors/404";
	msg.error500 = "/errors/500";

	Server::Initialize(
		8080,
		msg,
		whitelistedRoutesFolder,
		whitelistedExtensions);
		
	Server::server->Run();
}