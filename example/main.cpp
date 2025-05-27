//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <filesystem>

#include "http_server.hpp"

using KalaServer::Server;

using std::string;
using std::filesystem::current_path;
using std::map;

int main()
{
	static string fullPath = (current_path() / "static").string();

	map<string, string> initialRoutes{};
	initialRoutes["/"] = fullPath + "/index.html";
	initialRoutes["/about"] = fullPath + "/about.html";

	Server::Initialize(
		"static", 
		8080,
		initialRoutes);

	Server::server->Run();
}