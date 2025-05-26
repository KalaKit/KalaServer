#include <iostream>

#include "http_server.hpp"

using std::cout;

namespace MiniWeb
{
	Server::Server
		(int port)
		: port(port) {}

	void Server::Route(const string& path, RouteHandler handler)
	{
		cout << "[Route] Registered: " << path << "\n";
	}

	void Server::Run() const
	{
		cout << "Running server on port " << port << "\n";
	}
}