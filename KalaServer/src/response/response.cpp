//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <WinSock2.h>

#include "core/core.hpp"
#include "response/response.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::ConsoleMessageType;

using std::to_string;

namespace KalaKit::ResponseSystem
{
	void Response::Send(
		uintptr_t clientSocket,
		const string& clientIP,
		const string& route,
		const string& contentType,
		const string& statusLine,
		const string& body) const
	{
		SOCKET socket = static_cast<SOCKET>(clientSocket);

		string fullResponse =
			statusLine + "\r\n"
			"Content-Type: " + contentType + "\r\n"
			"Content-Length: " + to_string(body.size()) + "\r\n"
			"Connection: close\r\n\r\n" + body;

		send(
			socket,
			fullResponse.c_str(),
			static_cast<int>(fullResponse.size()),
			0);
		closesocket(socket);

		KalaServer::PrintConsoleMessage(
			2,
			true,
			ConsoleMessageType::Type_Message,
			"RESPONSE",
			"[" + to_string(clientSocket) + " - '" + clientIP + "']"
			+ " -> " 
			+ route
			+ " ["
			+ statusLine
			+ "]");
	}
}