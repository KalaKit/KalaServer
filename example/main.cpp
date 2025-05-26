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
using std::make_unique;

static string ServeFile(const string& fileName)
{
	path fullFilePath = path("static/" + fileName);
	fstream file(fullFilePath);

	stringstream buffer{};
	buffer << file.rdbuf();
	return buffer.str();
}

int main()
{
	cout << "Initializing KalaServer...\n";

	Server::server = make_unique<Server>(8080);

	Server::server->Route("/", [](const string&)
		{
			return ServeFile("index.html");
		});

	Server::server->Route("/about", [](const string&)
		{
			return ServeFile("about.html");
		});

	Server::server->Run();

	Server::server->Quit();
}