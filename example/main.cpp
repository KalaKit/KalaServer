#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "http_server.hpp"

using KalaServer::Server;

using std::string;
using std::cout;
using std::cin;
using std::fstream;
using std::stringstream;
using std::filesystem::path;
using std::filesystem::current_path;
using std::make_unique;

int main()
{
	cout << "Initializing KalaServer...\n";

	Server::server = make_unique<Server>(8080);

	static string fullPath = (current_path() / "static").string();
	cout << "full index path: " << fullPath << "/index.html\n";
	Server::server->Route("/", [](const string&)
		{
			return Server::server->ServeFile(fullPath + "/index.html");
		});

	Server::server->Route("/about", [](const string&)
		{
			return Server::server->ServeFile(fullPath + "/about.html");
		});

	Server::server->Run();

	Server::server->Quit();
}