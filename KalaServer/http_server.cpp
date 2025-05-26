#include <WS2tcpip.h>
#include <iostream>
#include <map>
#include <Windows.h>

#include "http_server.hpp"

using std::cout;
using std::map;
using std::exit;
using std::to_string;

namespace KalaServer
{
	static map<string, RouteHandler> routes;

	void Server::Route(const string& path, RouteHandler handler)
	{
		routes[path] = handler;
		cout << "[Route] Registered: " << path << "\n";
	}

	void Server::Run() const
	{
		WSADATA wsaData{};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			server->CreatePopup(
				PopupReason::Reason_Error,
				"WSAStartup failed!");
		}

		server->serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (serverSocket == INVALID_SOCKET)
		{
			server->CreatePopup(
				PopupReason::Reason_Error,
				"Socket creation failed!");
		}

		sockaddr_in serverAddr{};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(port);

		if (bind(
			server->serverSocket,
			(sockaddr*)&serverAddr,
			sizeof(serverAddr)) == SOCKET_ERROR)
		{
			server->CreatePopup(
				PopupReason::Reason_Error,
				"Bind failed!");
		}

		if (listen(server->serverSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			server->CreatePopup(
				PopupReason::Reason_Error,
				"Listen failed!");
		}

		cout << "Server is running on http://localhost:" << server->port << "\n";

		server->running = true;
		while (running)
		{
			SOCKET clientSocket = accept(server->serverSocket, nullptr, nullptr);
			if (clientSocket == INVALID_SOCKET)
			{
				server->CreatePopup(
					PopupReason::Reason_Error,
					"Accept failed!");
			}

			char buffer[2048] = {};
			int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
			if (bytesReceived > 0)
			{
				string request(buffer);
				string path = "/";

				size_t pathStart = request.find("GET ") + 4;
				size_t pathEnd = request.find(' ', pathStart);
				if (pathStart != string::npos
					&& pathEnd != string::npos)
				{
					path = request.substr(pathStart, pathEnd - pathStart);
				}

				string body{};
				if (routes.contains(path))
				{
					body = routes[path](request);
				}
				else
				{
					body = "<h1>404 Not Found!</h1>";
				}

				string response =
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: text/html\r\n"
					"Content-Length: " + to_string(body.size()) + "\r\n"
					"Connection: close\r\n\r\n" + body;

				send(
					clientSocket,
					response.c_str(),
					static_cast<int>(response.size()),
					0);
			}

			closesocket(clientSocket);
		}

		closesocket(server->serverSocket);
		WSACleanup();
	}

	void Server::CreatePopup(
		PopupReason reason,
		const string& message)
	{
		string popupTitle{};

		if (reason == PopupReason::Reason_Error)
		{
			popupTitle = "KalaServer error";

			MessageBoxA(
				nullptr,
				message.c_str(),
				popupTitle.c_str(),
				MB_ICONERROR
				| MB_OK);

			Quit();
		}
		else if (reason == PopupReason::Reason_Warning)
		{
			popupTitle = "KalaServer warning";

			MessageBoxA(
				nullptr,
				message.c_str(),
				popupTitle.c_str(),
				MB_ICONWARNING
				| MB_OK);
		}
	}

	void Server::Quit()
	{
		if (serverSocket != INVALID_SOCKET)
		{
			closesocket(serverSocket);
			serverSocket = INVALID_SOCKET;
		}

		WSACleanup();
		exit(0);
	}
}