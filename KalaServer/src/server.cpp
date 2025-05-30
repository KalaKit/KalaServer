//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <map>
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "core.hpp"
#include "server.hpp"
#include "cloudflare.hpp"
#include "dns.hpp"

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
		const string& serverName,
		const string& domainName,
		const ErrorMessage& errorMessage,
		const string& whitelistedRoutesFolder,
		const vector<string>& extensions)
	{
		if (!Core::IsRunningAsAdmin())
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"This program must be ran with admin privileges!");
			return;
		}

		if (serverName == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with empty server name!");
			return;
		}
		if (domainName == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with empty domain name!");
			return;
		}
		
		server = make_unique<Server>(
			port,
			serverName,
			domainName,
			errorMessage,
			whitelistedRoutesFolder,
			extensions);

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Initializing " + Server::server->GetServerName() + " ..."
			"\n\n"
			"=============================="
			"\n"
			"Adding core routes..."
			"\n"
			"=============================="
			"\n");

		server->AddInitialWhitelistedRoutes();
		
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"\n"
			"=============================="
			"\n"
			"Connecting to network..."
			"\n"
			"=============================="
			"\n");
		
		WSADATA wsaData{};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"WSAStartup failed!");
		}

		SOCKET thisSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		server->serverSocket = static_cast<uintptr_t>(thisSocket);

		if (thisSocket == INVALID_SOCKET)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Socket creation failed!");
		}

		sockaddr_in serverAddr{};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(port);

		if (bind(
			thisSocket,
			(sockaddr*)&serverAddr,
			sizeof(serverAddr)) == SOCKET_ERROR)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Bind failed!");
		}

		if (listen(thisSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Listen failed!");
		}

		cout << "Server is running on port '" << server->port << "'.\n";

		Core::isRunning = true;

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"\n"
			"=============================="
			"\n"
			"Initialization finished!"
			"\n"
			"=============================="
			"\n");
	}

	void Server::AddInitialWhitelistedRoutes()
	{
		if (server->whitelistedRoutesFolder == ""
			|| !exists(current_path() / server->whitelistedRoutesFolder))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Whitelisted routes root folder '" + server->whitelistedRoutesFolder + "' is empty or does not exist!");
			return;
		}

		for (const auto& route : recursive_directory_iterator(server->whitelistedRoutesFolder))
		{
			string cleanedRoute = path(route).generic_string();

			if (!is_regular_file(route)
				|| path(route).extension() != ".html")
			{
				continue;
			}

			//get clean route
			path relativePath = relative(route.path(), server->whitelistedRoutesFolder);
			relativePath.replace_extension("");
			string correctRootPath = "/" + relativePath.generic_string();

			//fix index.html root path
			if (correctRootPath == "/index") correctRootPath = "/";

			//use actual file path directly
			string correctFilePath = route.path().generic_string();

			server->AddNewWhitelistedRoute(correctRootPath, correctFilePath);
		}
	}

	void Server::AddNewWhitelistedRoute(const string& rootPath, const string& filePath)
	{
		if (server->whitelistedRoutes.contains(rootPath))
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Route '" + rootPath + "' has already been whitelisted!");
			return;
		}

		path fullPath = path(filePath);
		if (!exists(fullPath))
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Page path '" + filePath + "' does not exist!.");

			return;
		}

		server->whitelistedRoutes[rootPath] = filePath;

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Added new route '" + rootPath + "'");
	}
	void Server::AddNewWhitelistedExtension(const string& newExtension)
	{
		for (const auto& extension : server->whitelistedExtensions)
		{
			if (extension == newExtension)
			{
				Core::PrintConsoleMessage(
					ConsoleMessageType::Type_Warning,
					"Extension '" + extension + "' has already been whitelisted!");
				return;
			}
		}

		server->whitelistedExtensions.push_back(newExtension);
		Core::PrintConsoleMessage(
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
			Core::PrintConsoleMessage(
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
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Extension '" + thisExtension + "' cannot be removed because it hasn't been whitelisted!");
		}
	}

	string Server::ServeFile(const string& route)
	{
		auto it = server->whitelistedRoutes.find(route);
		if (it != whitelistedRoutes.end())
		{
			path fullFilePath = it->second;
			ifstream file(fullFilePath);
			if (file)
			{
				stringstream buffer{};
				buffer << file.rdbuf();
				return buffer.str();
			}
		}

		return "";
	}

	bool Server::Run() const
	{
		if (!CloudFlare::IsRunning()
			&& !DNS::IsRunning())
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Neither cloudflared or dns was started! Please run atleast one of them.");
			return false;
		}
		
		for (int i = 0; i < 600; ++i)
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"Server is alive...");
			Sleep(100);
		}

		SOCKET thisSocket = static_cast<SOCKET>(server->serverSocket);
		SOCKET clientSocket = accept(thisSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Accept failed!");
			return false;
		}

		char buffer[2048] = {};
		int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
		if (bytesReceived > 0)
		{
			string request(buffer);
			string filePath = "/";

			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"Raw HTTP request:\n" + request);

			if (request.starts_with("GET "))
			{
				size_t pathStart = 4;
				size_t pathEnd = request.find(' ', pathStart);
				if (pathEnd != string::npos)
				{
					filePath = request.substr(pathStart, pathEnd - pathStart);
				}
			}

			string body{};
			string statusLine = "HTTP/1.1 200 OK";

			if (!server->RouteExists(filePath))
			{
				Core::PrintConsoleMessage(
					ConsoleMessageType::Type_Message,
					"User tried to access non-existing route '" + filePath + "'!");

				string result = server->ServeFile(server->errorMessage.error404);
				if (result == "")
				{
					Core::PrintConsoleMessage(
						ConsoleMessageType::Type_Error,
						"Failed to load 404 error page!");
				}
				else body = result;

				statusLine = "HTTP/1.1 404 Not Found";
			}
			else
			{
				bool isAllowedFile =
					filePath.find_last_of('.') == string::npos
					|| server->ExtensionExists(path(filePath).extension().generic_string());

				if (!isAllowedFile)
				{
					Core::PrintConsoleMessage(
						ConsoleMessageType::Type_Warning,
						"User tried to access forbidden route '" + filePath + "' from path '" + server->whitelistedRoutes[filePath] + "'.");

					string result = server->ServeFile(server->errorMessage.error403);
					if (result == "")
					{
						Core::PrintConsoleMessage(
							ConsoleMessageType::Type_Error,
							"Failed to load 403 error page!");
					}
					else body = result;

					statusLine = "HTTP/1.1 403 Forbidden";
				}
				else
				{
					Core::PrintConsoleMessage(
						ConsoleMessageType::Type_Message,
						"Loading route '" + filePath + "' from path '" + server->whitelistedRoutes[filePath] + "'.");

					try
					{
						string result = server->ServeFile(filePath);
						if (result == "")
						{
							string errorResult = server->ServeFile(server->errorMessage.error500);
							if (errorResult == "")
							{
								Core::PrintConsoleMessage(
									ConsoleMessageType::Type_Error,
									"Failed to load 500 error page!");
							}
							else body = errorResult;
						}
						else body = result;
					}
					catch (const exception& e)
					{
						Core::PrintConsoleMessage(
							ConsoleMessageType::Type_Error,
							"Error 500 when requesting page ("
							+ filePath
							+ ").\nError:\n"
							+ e.what());

						string result = server->ServeFile(server->errorMessage.error500);
						if (result == "")
						{
							Core::PrintConsoleMessage(
								ConsoleMessageType::Type_Error,
								"Failed to load 500 error page!");
						}
						else body = result;

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

		return true;
	}

	void Server::Quit()
	{
		if (Server::server != nullptr)
		{
			SOCKET thisSocket = static_cast<SOCKET>(server->serverSocket);
			if (thisSocket != INVALID_SOCKET)
			{
				closesocket(thisSocket);
				thisSocket = INVALID_SOCKET;
				server->serverSocket = 0;
			}

			WSACleanup();
		}
	}
}