//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <WinSock2.h>
#include <filesystem>

#include "core/core.hpp"
#include "response/response.hpp"
#include "external/kcrash.hpp"
#include "core/event.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::KalaCrash::Crash;
using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PrintData;

using std::unique_ptr;
using std::make_unique;
using std::to_string;
using std::filesystem::path;

namespace KalaKit::ResponseSystem
{
	void Response::Send(
		uintptr_t clientSocket,
		const string& clientIP,
		const string& route,
		const string& contentType,
		const string& statusLine,
		const vector<char> data,
		SendAction sendAction) const
	{
		SOCKET socket = static_cast<SOCKET>(clientSocket);

		string fullResponse =
			statusLine + "\r\n"
			"Content-Type: " + contentType + "\r\n"
			"Content-Length: " + to_string(data.size()) + "\r\n";

		if (sendAction == SendAction::send_download)
		{
			fullResponse += "Content-Disposition: attachment; filename=\""
				+ path(route).filename().string() + "\"\r\n";
		}

		fullResponse += "Connection: close\r\n";

		if (hasRange)
		{
			fullResponse += "Accept-Ranges: bytes\r\n";
			fullResponse += "Content-Range: " + contentRange + "\r\n";
		}

		fullResponse += "\r\n";

		//send headers
		send(
			socket,
			fullResponse.c_str(),
			static_cast<int>(fullResponse.size()),
			0);

		//send binary body
		if (!data.empty())
		{
			send(
				socket,
				data.data(),
				static_cast<int>(data.size()),
				0);
		}

		closesocket(socket);

		PrintData rData =
		{
			.indentationLength = 2,
			.addTimeStamp = false,
			.severity = EventType::event_severity_message,
			.customTag = "RESPONSE",
			.message =
				"[" + to_string(clientSocket) + " - '" + clientIP + "']"
				+ " -> " + route + " [" + statusLine + "]\n"
		};
		unique_ptr<Event> rEvent = make_unique<Event>();
		rEvent->SendEvent(EventType::event_print_console_message, rData);
	}
}