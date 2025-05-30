//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/server.hpp"
#include "response/response_404.hpp"

using KalaKit::Core::Server;

namespace KalaKit::ResponseSystem
{
	void Response_404::Init(
		const std::string& newRoute,
		const std::string& newClientIP,
		uintptr_t newClientSocket)
	{
		route = newRoute;
		clientIP = newClientIP;
		clientSocket = newClientSocket;

		statusLine = "HTTP/1.1 404 Not Found";
		contentType = "text/html";

		body = Server::server->ServeFile(route);

		if (body.empty())
		{
			body =
				"<html>"
				"	<body>"
				"		<h1>404 Not Found</h1>"
				"		<p>The requested resource could not be found.</p>"
				"	</body>"
				"</html>";
		}

		Send();
	}
}