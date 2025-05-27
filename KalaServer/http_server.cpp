//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <WS2tcpip.h>
#include <iostream>
#include <map>
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "http_server.hpp"

using std::cout;
using std::map;
using std::exit;
using std::to_string;
using std::ifstream;
using std::stringstream;
using std::exception;
using std::filesystem::path;
using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::recursive_directory_iterator;
using std::make_unique;

namespace KalaServer
{
	void Server::Initialize(
		int port, 
		const vector<string>& routes, 
		const vector<string>& extensions)
	{
		server = make_unique<Server>(port);

		server->PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Initializing KalaServer...");

		server->AddWhitelistedRoute("/errors/403.html", routeOrigin + "/errors/403.html");
		server->Route("/errors/404.html", routeOrigin + "/errors/404.html");
		server->Route("/errors/500.html", routeOrigin + "/errors/500.html");

		for (const auto& extension : whitelistedExtensions)
		{
			Server::server->AddWhitelistedExtension(extension);
		}
		AddInitialRoutes(routes);
	}
	
	void Server::AddInitialWhitelistedRoutes(const vector<string> whitelistedRoutes)
	{
		string cleanedRoute = route;
		if (!cleanedRoute.empty())
		{
			if (cleanedRoute.front() == '/') cleanedRoute = cleanedRoute.substr(1);
			if (cleanedRoute.back() == '/')
			{
				cleanedRoute = cleanedRoute.substr(0, cleanedRoute.size() - 1);	
			}
		}
		
		path baseDir = current_path() / cleanedRoute;
		
		if (!exists(baseDir)
			|| !is_directory(baseDir))
		{
			continue;
		}
		
		for (const auto& file : recursive_directory_iterator(baseDir))
		{
			if (!is_regular_file(file.status())) continue;
			
			path relative = relative(file.path(), current_path());
			string fullRoute = "/" + relative.generic_string();
			
			server->AddWhitelistedRoute(fullRoute);
		}
		
		server->AddWhitelistedRoute(route);
	}

	string Server::ServeFile(const string& filePath)
	{
		path fullFilePath = path(filePath);
		ifstream file(fullFilePath);
		if (file)
		{
			stringstream buffer{};
			buffer << file.rdbuf();
			return buffer.str();
		}
		else
		{
			server->PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Page '" + path(filePath).filename().string() + "' does not exist!");

			ifstream notFoundFile("static/errors/404.html");
			if (notFoundFile)
			{
				stringstream buffer{};
				buffer << notFoundFile.rdbuf();
				return buffer.str();
			}
		}
		return "";
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
				string statusLine = "HTTP/1.1 200 OK";
#ifdef _DEBUG
				server->PrintConsoleMessage(
					ConsoleMessageType::Type_Message,
					"Resolved path: '" + path + "'");
#endif
				bool isAllowedFile =
					path.find_last_of('.') == string::npos
					|| server->ExtensionExists(path.extension().string());

				if (!isAllowedFile)
				{
#ifdef _DEBUG
					server->PrintConsoleMessage(
						ConsoleMessageType::Type_Error,
						"Error 403 when requesting file or path '" + path + "'!");
#endif
					string result = server->ServeFile(path(current_path() / "pages/errors/403.html").string());
					if (result == "")
					{
#ifdef _DEBUG
						server->PrintConsoleMessage(
							ConsoleMessageType::Type_Error,
							"File or page for error 403 cannot be accessed!");
#endif
						body = "<h1>403 Forbidden</h1>";
					}
					statusLine = "HTTP/1.1 403 Forbidden";
				}
				else
				{
					if (!RouteExists(path))
					{
						string result = server->ServeFile(path(current_path() / "pages/errors/404.html").string());
						if (result == "")
						{
#ifdef _DEBUG
							server->PrintConsoleMessage(
								ConsoleMessageType::Type_Error,
								"Page for error 404 cannot be accessed!");
#endif
							body = "<h1>404 Not Found</h1>";
						}
						statusLine = "HTTP/1.1 404 Not Found";
					}
					else
					{
						try
						{
							body = server->ServeFile(server->routes[path]);
						}
						catch (const exception& e)
						{
#ifdef _DEBUG
							server->PrintConsoleMessage(
								ConsoleMessageType::Type_Error,
								"Error 500 when requesting page ("
								+ path
								+ ").\nError:\n"
								+ e.what());
#endif
							string result = server->ServeFile(path(current_path() / "pages/errors/403.html").string());
							if (result == "")
							{
#ifdef _DEBUG
								server->PrintConsoleMessage(
									ConsoleMessageType::Type_Error,
									"Page for error 500 cannot be accessed!");
#endif
								body = "<h1>500 Not Found</h1>";
							}
							statusLine = "HTTP/1.1 500 Internal Server Error";
						}
					}
				}

				string response =
					statusLine + "\r\n"
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

	void Server::PrintConsoleMessage(
		ConsoleMessageType type,
		const string& message)
	{
		string targetType{};

		switch (type)
		{
		case ConsoleMessageType::Type_Error:
			targetType = "[ERROR] ";
			break;
		case ConsoleMessageType::Type_Warning:
			targetType = "[WARNING] ";
			break;
		case ConsoleMessageType::Type_Message:
			targetType = "";
			break;
		}

		string result = targetType + message;
		cout << result + "\n";
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