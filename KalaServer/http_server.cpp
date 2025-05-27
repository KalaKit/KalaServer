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
			"Initializing KalaServer..."
			"\n\n"
			"=============================="
			"\n"
			"Adding core extensions..."
			"\n"
			"=============================="
			"\n");

		for (const auto& extension : extensions)
		{
			server->AddNewWhitelistedExtension(extension);
		}

		server->PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"\n"
			"=============================="
			"\n"
			"Adding core routes..."
			"\n"
			"=============================="
			"\n");

		server->AddInitialWhitelistedRoutes(routes);

		server->PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"\n"
			"=============================="
			"\n"
			"Initialization finished!"
			"\n"
			"=============================="
			"\n");
	}

	void Server::AddInitialWhitelistedRoutes(const vector<string>& whitelistedRoutes)
	{
		for (const auto& route : whitelistedRoutes)
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
				return;
			}

			for (const auto& file : recursive_directory_iterator(baseDir))
			{
				if (!is_regular_file(file.status())) continue;

				path relativePath = relative(file.path(), current_path());
				string fullRoute = "/" + relativePath.generic_string();

				server->AddNewWhitelistedRoute(fullRoute);
			}

			server->AddNewWhitelistedRoute(route);
		}
	}

	void Server::AddNewWhitelistedRoute(const string& newRoute)
	{
		if (server->whitelistedRoutes.contains(newRoute))
		{
			PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Route '" + newRoute + "' has already been whitelisted!");
			return;
		}

		path fullPath = current_path() / path(newRoute).relative_path();
		if (!exists(fullPath))
		{
			PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Failed to create full path for route '" + newRoute + "'! Attempted full path was '" + fullPath.string() + "'.");
		}

		server->whitelistedRoutes[newRoute] = fullPath.string();

		PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Added new route '" + newRoute + "'");
	}
	void Server::AddNewWhitelistedExtension(const string& newExtension)
	{
		for (const auto& extension : server->whitelistedExtensions)
		{
			if (extension == newExtension)
			{
				PrintConsoleMessage(
					ConsoleMessageType::Type_Warning,
					"Extension '" + extension + "' has already been whitelisted!");
				return;
			}
		}

		server->whitelistedExtensions.push_back(newExtension);
		PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Added new extension '" + newExtension + "'");
	}

	void Server::RemoveWhitelistedRoute(const string& thisRoute)
	{
		string foundRoute{};
		if (server->whitelistedRoutes.contains(thisRoute))
		{
			foundRoute = thisRoute;
		}

		if (foundRoute == "")
		{
			PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Route '" + thisRoute + "' cannot be removed because it hasn't been whitelisted!");
		}
	}
	void Server::RemoveWhitelistedExtension(const string& thisExtension)
	{
		string foundExtension{};
		for (const auto& extension : server->whitelistedExtensions)
		{
			if (extension == thisExtension)
			{
				foundExtension = extension;
				break;
			}
		}

		if (foundExtension == "")
		{
			PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Extension '" + thisExtension + "' cannot be removed because it hasn't been whitelisted!");
		}
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
				string filePath = "/";

				size_t pathStart = request.find("GET ") + 4;
				size_t pathEnd = request.find(' ', pathStart);
				if (pathStart != string::npos
					&& pathEnd != string::npos)
				{
					filePath = request.substr(pathStart, pathEnd - pathStart);
				}

				string body{};
				string statusLine = "HTTP/1.1 200 OK";
#ifdef _DEBUG
				server->PrintConsoleMessage(
					ConsoleMessageType::Type_Message,
					"Loading path: '" + filePath + "'");
#endif
				bool isAllowedFile =
					filePath.find_last_of('.') == string::npos
					|| server->ExtensionExists(path(filePath).extension().string());

				if (!isAllowedFile)
				{
#ifdef _DEBUG
					server->PrintConsoleMessage(
						ConsoleMessageType::Type_Error,
						"Error 403 when requesting file or path '" + filePath + "'!");
#endif
					server->ServeFile(path(current_path() / "pages/errors/403.html").string());
					statusLine = "HTTP/1.1 403 Forbidden";
				}
				else
				{
					if (!server->RouteExists(filePath))
					{
#ifdef _DEBUG
						server->PrintConsoleMessage(
							ConsoleMessageType::Type_Error,
							"Error 404 when requesting file or path '" + filePath + "'!");
#endif
						server->ServeFile(path(current_path() / "pages/errors/404.html").string());
						statusLine = "HTTP/1.1 404 Not Found";
					}
					else
					{
						try
						{
							auto it = server->whitelistedRoutes.find(filePath);
							if (it == server->whitelistedRoutes.end())
							{
								server->PrintConsoleMessage(
									ConsoleMessageType::Type_Error,
									"Route '" + filePath + "' is allowed but not mapped to any file!");

								server->ServeFile(path(current_path() / "pages/errors/500.html").string());
								statusLine = "HTTP/1.1 500 Internal Server Error";
							}
							else
							{
								server->PrintConsoleMessage(
									ConsoleMessageType::Type_Message,
									"Serving route '" + it->second + "'.");
								body = server->ServeFile(it->second);
							}
						}
						catch (const exception& e)
						{
							server->PrintConsoleMessage(
								ConsoleMessageType::Type_Error,
								"Error 500 when requesting page ("
								+ filePath
								+ ").\nError:\n"
								+ e.what());

							server->ServeFile(path(current_path() / "pages/errors/500.html").string());
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