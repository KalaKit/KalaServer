//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/core.hpp"
#include "core/server.hpp"
#include "response/response_206.hpp"

using KalaKit::Core::Server;
using KalaKit::Core::KalaServer;
using KalaKit::Core::ConsoleMessageType;
using KalaKit::Core::Server;

using std::to_string;

namespace KalaKit::ResponseSystem
{
	void Response_206::Init(
		uintptr_t targetClientSocket,
		const string& targetClientIP,
		const string& targetRoute,
		const string& targetContentType)
	{
		uintptr_t clientSocket = targetClientSocket;
		string clientIP = targetClientIP;
		string route = targetRoute;
		string contentType = targetContentType;
		string statusLine = "HTTP/1.1 206 Partial Content";

		size_t rangeStart = this->rangeStart;
		size_t rangeEnd = this->rangeEnd;
		size_t totalSize = 0;
		bool sliced = false;

		vector<char> body = Server::server->ServeFile(
			targetRoute,
			rangeStart,
			rangeEnd,
			totalSize,
			sliced);

		if (body.empty())
		{
			KalaServer::PrintConsoleMessage(
				0,
				false,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"206 response file body was empty!");

			string newBody =
				"<html>"
				"	<body>"
				"		<h1>206 Partial Content</h1>"
				"		<p>File not found or empty.</p>"
				"	</body>"
				"</html>";
			body = vector(newBody.begin(), newBody.end());
			sliced = false;
			statusLine = "HTTP/1.1 404 Not Found";
		}

		if (sliced)
		{
			this->hasRange = true;
			this->rangeStart = rangeStart;
			this->rangeEnd = rangeEnd == 0
				? rangeStart + body.size() - 1
				: rangeEnd;
			this->totalSize = totalSize;
			this->contentRange =
				"bytes " + to_string(rangeStart) + "-" +
				to_string(this->rangeEnd) + "/" +
				to_string(totalSize);

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