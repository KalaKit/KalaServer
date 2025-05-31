//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/server.hpp"
#include "response/response_banned.hpp"

using KalaKit::Core::Server;

namespace KalaKit::ResponseSystem
{
	void Response_Banned::Init(
		const std::string& newRoute,
		const std::string& newClientIP,
		uintptr_t newClientSocket)
	{
		route = newRoute;
		clientIP = newClientIP;
		clientSocket = newClientSocket;

		statusLine = "HTTP/1.1 418 I'm a teapot";
		contentType = "text/html";

		body = Server::server->ServeFile(Server::server->errorMessage.error403);

		if (body.empty())
		{
			body =
				"<html>"
				"	<body>"
				"		<h1>418 I'm a teapot</h1>"
				"		<p>Your requests are so obvious and robotic, we decided to ban you with a joke status code.</p>"
				"	</body>"
				"</html>";
		}

		Send();
	}
}