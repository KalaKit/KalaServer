//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/core.hpp"
#include "core/server.hpp"
#include "response/response_500.hpp"

using KalaKit::Core::Server;
using KalaKit::Core::KalaServer;
using KalaKit::Core::ConsoleMessageType;

using KalaKit::Core::Server;

namespace KalaKit::ResponseSystem
{
	void Response_500::Init(
		uintptr_t targetClientSocket,
		const string& targetClientIP,
		const string& targetRoute,
		const string& targetContentType)
	{
		uintptr_t clientSocket = targetClientSocket;
		string clientIP = targetClientIP;
		string route = targetRoute;
		string contentType = targetContentType;
		string statusLine = "HTTP/1.1 500 Internal Server Error";
		string body = Server::server->ServeFile(Server::server->errorMessage.error500);

		if (body.empty())
		{
			KalaServer::PrintConsoleMessage(
				0,
				false,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"500 response file body was empty!");

			body =
				"<html>"
				"	<body>"
				"		<h1>500 Internal Server Error</h1>"
				"		<p>An unexpected error occurred on the server.</p>"
				"	</body>"
				"</html>";
		}

		Send(
			clientSocket,
			clientIP,
			route,
			contentType,
			statusLine,
			body);
	}
}