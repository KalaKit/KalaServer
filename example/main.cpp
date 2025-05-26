#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "http_server.hpp"

using MiniWeb::Server;

using std::string;
using std::cout;
using std::cin;
using std::fstream;
using std::stringstream;
using std::filesystem::path;

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
	cout << "Initializing MiniWeb...\n";

	Server server(8080);

	server.Route("/", [](const string&)
		{
			return ServeFile("index.html");
		});

	server.Route("/about", [](const string&)
		{
			return ServeFile("about.html");
		});

	server.Run();

	cout << "MiniWeb is open! Press any key to exit...\n";
	cin.get();

	return 0;
}