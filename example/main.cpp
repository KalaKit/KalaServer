#include <string>
#include <iostream>

#include "http_server.hpp"

using MiniWeb::Server;

using std::string;
using std::cout;
using std::cin;

int main()
{
	Server server(8080);
	server.Route("/hello", [](const string&)
		{
			return string("<h1>Hello from MiniWeb!< / h1>");
		});
	server.Run();

	cout << "MiniWeb is open!\n";
	cin.get();

	return 0;
}