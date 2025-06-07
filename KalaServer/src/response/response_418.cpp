//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/core.hpp"
#include "core/server.hpp"
#include "response/response_418.hpp"

using KalaKit::Core::Server;
using KalaKit::Core::KalaServer;
using KalaKit::Core::ConsoleMessageType;

namespace KalaKit::ResponseSystem
{
	void Response_418::Init(
		uintptr_t targetClientSocket,
		const string& targetClientIP,
		const string& targetRoute,
		const string& targetContentType)
	{
		uintptr_t clientSocket = targetClientSocket;
		string clientIP = targetClientIP;
		string route = targetRoute;
		string contentType = targetContentType;
		string statusLine = "HTTP/1.1 418 I'm a teapot";

		size_t totalSize = 0;
		bool sliced = false;
		vector<char> body = Server::server->ServeFile(
			Server::server->errorMessage.error418,
			0,
			0,
			totalSize,
			sliced);

		if (body.empty())
		{
			KalaServer::PrintConsoleMessage(
				0,
				false,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"418 response file body was empty!");

			string newBody =
				"<html>"
				"	<body>"
				"		<h1>418 I'm a teapot</h1>"
				"		<p>Your requests are so obvious and robotic, we decided to ban you with a joke status code.</p>"
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