//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/core.hpp"
#include "core/server.hpp"
#include "response/response_200.hpp"
#include "core/event.hpp"

using KalaKit::Core::Server;
using KalaKit::Core::KalaServer;
using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PrintData;

using std::unique_ptr;
using std::make_unique;

namespace KalaKit::ResponseSystem
{
	void Response_200::Init(
		uintptr_t targetClientSocket,
		const string& targetClientIP,
		const string& targetRoute,
		const string& targetContentType)
	{
		uintptr_t clientSocket = targetClientSocket;
		string clientIP = targetClientIP;
		string route = targetRoute;
		string contentType = targetContentType;
		string statusLine = "HTTP/1.1 200 OK";

		size_t totalSize = 0;
		bool sliced = false;
		vector<char> body = Server::server->ServeFile(
			targetRoute,
			0,
			0,
			totalSize,
			sliced);

		if (body.empty())
		{
			PrintData rData =
			{
				.indentationLength = 2,
				.addTimeStamp = false,
				.severity = EventType::event_severity_error,
				.customTag = "RESPONSE",
				.message = "200 response file body was empty!"
			};
			unique_ptr<Event> rEvent = make_unique<Event>();
			rEvent->SendEvent(EventType::event_print_console_message, rData);

			string newBody =
				"<html>"
				"	<body>"
				"		<h1>200 OK</h1>"
				"		<p>File not found or empty.</p>"
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
			body,
			sendAction);
	}
}