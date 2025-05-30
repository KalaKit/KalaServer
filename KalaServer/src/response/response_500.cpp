//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/server.hpp"
#include "response/response_500.hpp"

using KalaKit::Core::Server;

namespace KalaKit::ResponseSystem
{
	void Response_500::Init(
		const std::string& newRoute,
		const std::string& newClientIP,
		uintptr_t newClientSocket)
	{
		route = newRoute;
		clientIP = newClientIP;
		clientSocket = newClientSocket;

		statusLine = "HTTP/1.1 500 Internal Server Error";
		contentType = "text/html";

		body = Server::server->ServeFile(route);

		if (body.empty())
		{
			body =
				"<html>"
				"	<body>"
				"		<h1>500 Internal Server Error</h1>"
				"		<p>An unexpected error occurred on the server.</p>"
				"	</body>"
				"</html>";
		}

		Send();
	}
}