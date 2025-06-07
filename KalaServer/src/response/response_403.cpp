//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/core.hpp"
#include "core/server.hpp"
#include "response/response_403.hpp"

using KalaKit::Core::Server;
using KalaKit::Core::KalaServer;
using KalaKit::Core::ConsoleMessageType;

using KalaKit::Core::Server;

namespace KalaKit::ResponseSystem
{
	void Response_403::Init(
		uintptr_t targetClientSocket,
		const string& targetClientIP,
		const string& targetRoute,
		const string& targetContentType)
	{
		uintptr_t clientSocket = targetClientSocket;
		string clientIP = targetClientIP;
		string route = targetRoute;
		string contentType = targetContentType;
		string statusLine = "HTTP/1.1 403 Forbidden";

		vector<char> body = Server::server->ServeFile(Server::server->errorMessage.error403);

		if (body.empty())
		{
			KalaServer::PrintConsoleMessage(
				0,
				false,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"403 response file body was empty!");

			string newBody =
				"<html>"
				"	<body>"
				"		<h1>403 Forbidden</h1>"
				"		<p>You do not have permission to access this resource.</p>"
				"	</body>"
				"</html>";
			body = vector(newBody.begin(), newBody.end());
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