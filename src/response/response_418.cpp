//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>

#include "core/core.hpp"
#include "core/server.hpp"
#include "response/response_418.hpp"
#include "core/event.hpp"

using KalaKit::Core::Server;
using KalaKit::Core::KalaServer;
using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PrintData;

using std::unique_ptr;
using std::make_unique;
using std::string;
using std::find;
using std::pair;

//Looks for one of the target data parameters to replace it with the ip, reason and target email to send an email to for ban lift appeal.
static string ReplaceData(const string& body, const string& placeholder, const string& replacement)
{
	string result = body;
	size_t pos = result.find(placeholder);
	if (pos == string::npos)
	{
		PrintData oData = 
		{
			.indentationLength = 2,
			.addTimeStamp = false,
			.severity = EventType::event_severity_error,
			.customTag = "RESPONSE",
			.message = "Placeholder '" + placeholder + "' not found in 418 page!"
		};
		unique_ptr<Event> oEvent = make_unique<Event>();
		oEvent->SendEvent(EventType::event_print_console_message, oData);

		return result;
	}

	result.replace(pos, placeholder.length(), replacement);
	return result;
}

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
			PrintData rData =
			{
				.indentationLength = 0,
				.addTimeStamp = false,
				.severity = EventType::event_severity_message,
				.customTag = "RESPONSE",
				.message = "418 response file body was empty!"
			};
			unique_ptr<Event> rEvent = make_unique<Event>();
			rEvent->SendEvent(EventType::event_print_console_message, rData);

			string newBody =
				"<html>"
				"	<body>"
				"		<h1>418 I'm a teapot</h1>"
				"		<p>Your requests are so obvious and robotic, we decided to ban you with a joke status code.</p>"
				"	</body>"
				"</html>";
			body = vector(newBody.begin(), newBody.end());
		}
		else if (!body.empty()
				 && contentType == "text/html")
		{
			//convert binary to string to adjust data parameterss
			string html(body.begin(), body.end());

			//replace HTML-safe placeholders

			html = ReplaceData(html, "REPLACEME_IP", clientIP);

			string reason{};
			vector<pair<string, string>> bannedIPs = Server::server->bannedIPs;
			for (const auto& target : bannedIPs)
			{
				if (target.first == clientIP)
				{
					reason = target.second;
					break;
				}
			}
			if (reason.empty()) reason = route;

			html = ReplaceData(html, "REPLACEME_REASON", reason);
			html = ReplaceData(html, "REPLACEME_EMAIL", Server::server->emailSenderData.username);

			//convert back to binary for sending
			body = vector<char>(html.begin(), html.end());
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