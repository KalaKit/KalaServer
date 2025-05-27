//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <vector>

#include "http_server.hpp"

using KalaServer::Server;

using std::string;
using std::vector;

int main()
{
	static const vector<string> whitelistedExtensions = 
	{
		".png", ".jpg", ".jpeg", ".gif",
		".mp3", ".wav", ".flac", ".ogg",
		".webp", ".mp4", ".webm"
	};	
	static const vector<string> whitelistedRoutes = 
	{
		"/images/",
		"/videos/",
		"/pages/"
	};
	
	Server::Initialize(
		8080,
		whitelistedExtensions,
		whitelistedRoutes);
		
	Server::server->Run();
}