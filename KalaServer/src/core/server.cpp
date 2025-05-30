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

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::Server;
using KalaKit::Core::ConsoleMessageType;
using KalaKit::Core::PopupReason;
using KalaKit::DNS::CloudFlare;
using KalaKit::DNS::CustomDNS;

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
using std::ofstream;
using std::ios;
using std::string_view;

namespace KalaKit::Core
{
	void Server::Initialize(
		int port,
		const string& serverName,
		const string& domainName,
		const ErrorMessage& errorMessage,
		const string& whitelistedRoutesFolder,
		const vector<string>& blacklistedKeywords,
		const vector<string>& extensions)
	{
		if (!KalaServer::IsRunningAsAdmin())
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"This program must be ran with admin privileges!");
			return;
		}

		if (serverName == "")
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with empty server name!");
			return;
		}
		if (domainName == "")
		{
			KalaServer::CreatePopup(
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
			blacklistedKeywords,
			extensions);

		KalaServer::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Initializing " + Server::server->GetServerName() + " ..."
			"\n\n"
			"=============================="
			"\n"
			"Adding core routes..."
			"\n"
			"=============================="
			"\n");

		server->bannedBotsFile = (path(current_path() / "banned-bots.txt")).string();
		string bannedBotsFile = server->GetBannedBotsFilePath();
		if (!exists(bannedBotsFile))
		{
			ofstream log(bannedBotsFile);
			if (!log)
			{
				KalaServer::CreatePopup(
					PopupReason::Reason_Error,
					"Failed to create banned-bots.txt!");
				return;
			}
			log.close();

			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"Created 'banned-bots.txt' at root directory.");
		}

		server->AddInitialWhitelistedRoutes();
		
		KalaServer::PrintConsoleMessage(
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
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"WSAStartup failed!");
		}

		SOCKET thisSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		server->serverSocket = static_cast<uintptr_t>(thisSocket);

		if (thisSocket == INVALID_SOCKET)
		{
			KalaServer::CreatePopup(
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
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Bind failed!");
		}

		if (listen(thisSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Listen failed!");
		}

		cout << "Server is running on port '" << server->port << "'.\n";

		KalaServer::isRunning = true;

		KalaServer::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"\n"
			"=============================="
			"\n"
			"Initialization finished!"
			"\n"
			"=============================="
			"\n");
	}

	bool Server::IsBlacklistedRoute(const string& route)
	{
		for (const auto& blacklistedKeyword : blacklistedKeywords)
		{
			if (route.find(blacklistedKeyword) != string::npos) return true;
		}

		return false;
	}

	bool Server::IsBannedIP(const string& targetIP) const
	{
		string bannedBotsFile = server->GetBannedBotsFilePath();

		bool result = false;

		ifstream file(bannedBotsFile);
		if (!file)
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Failed to open 'banned-bots.txt' to check if IP is banned or not!");
			return false;
		}

		string line;
		while (getline(file, line))
		{
			auto delimiterPos = line.find('|');
			if (delimiterPos != string::npos)
			{
				string ip = line.substr(0, delimiterPos);
				string reason = line.substr(delimiterPos + 1);
				if (ip == targetIP)
				{
					result = true;
					break;
				}
			}
		}

		file.close();

		return result;
	}

	void Server::StopBannedIP(
		const BannedIP& target,
		uintptr_t clientSocket) const
	{
		string body{};
		string statusLine = "HTTP/1.1 403 Forbidden";

		string result = server->ServeFile(server->errorMessage.error403);
		if (result == "")
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Failed to load 403 error page!");
		}
		else body = result;

		string response =
			statusLine + "\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: " + to_string(body.size()) + "\r\n"
			"Connection: close\r\n\r\n" + body;

		SOCKET clientRawSocket = static_cast<SOCKET>(clientSocket);

		send(
			clientRawSocket,
			response.c_str(), 
			static_cast<int>(response.size()), 
			0);

		closesocket(clientRawSocket);
	}

	void Server::BanIP(const BannedIP& target, uintptr_t clientSocket) const
	{
		string bannedBotsPath = server->GetBannedBotsFilePath();

		ifstream readFile(bannedBotsPath);
		if (!readFile)
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Failed to read 'banned-bots.txt' to ban IP!");
			return;
		}

		string line;
		while (getline(readFile, line))
		{
			auto delimiterPos = line.find('|');
			if (delimiterPos != string::npos)
			{
				string ip = line.substr(0, delimiterPos);
				string cleanedIP = ip.substr(0, ip.find_last_not_of(" \t") + 1);
				if (cleanedIP == target.IP) return; //user is already banned
			}
		}

		readFile.close();

		ofstream writeFile(bannedBotsPath, ios::app);
		if (!writeFile)
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Failed to write into 'banned-bots.txt' to ban IP!");
			return;
		}

		writeFile << target.IP << " | " << target.reason << "\n";

		writeFile.close();

		KalaServer::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"\n"
			"=============== BANNED IP ===============\n"
			" IP     : " + target.IP + "\n"
			" Reason : Attempted access to blacklisted route '" + target.reason + "'\n"
			"=========================================\n");

		server->StopBannedIP(target, clientSocket);
	}

	void Server::AddInitialWhitelistedRoutes() const
	{
		if (server->whitelistedRoutesFolder == ""
			|| !exists(current_path() / server->whitelistedRoutesFolder))
		{
			KalaServer::CreatePopup(
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

	void Server::AddNewWhitelistedRoute(const string& rootPath, const string& filePath) const
	{
		if (server->whitelistedRoutes.contains(rootPath))
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Route '" + rootPath + "' has already been whitelisted!");
			return;
		}

		path fullPath = path(filePath);
		if (!exists(fullPath))
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Page path '" + filePath + "' does not exist!.");

			return;
		}

		server->whitelistedRoutes[rootPath] = filePath;

		KalaServer::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Added new route '" + rootPath + "'");
	}
	void Server::AddNewWhitelistedExtension(const string& newExtension) const
	{
		for (const auto& extension : server->whitelistedExtensions)
		{
			if (extension == newExtension)
			{
				KalaServer::PrintConsoleMessage(
					ConsoleMessageType::Type_Warning,
					"Extension '" + extension + "' has already been whitelisted!");
				return;
			}
		}

		server->whitelistedExtensions.push_back(newExtension);
		KalaServer::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Added new extension '" + newExtension + "'");
	}

	void Server::RemoveWhitelistedRoute(const string& thisRoute) const
	{
		string foundRoute{};
		if (server->whitelistedRoutes.contains(thisRoute))
		{
			foundRoute = thisRoute;
		}

		if (foundRoute == "")
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Route '" + thisRoute + "' cannot be removed because it hasn't been whitelisted!");
		}
	}
	void Server::RemoveWhitelistedExtension(const string& thisExtension) const
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
			KalaServer::PrintConsoleMessage(
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
			&& !CustomDNS::IsRunning())
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Neither cloudflared or dns was started! Please run atleast one of them.");
			return false;
		}

		SOCKET thisSocket = static_cast<SOCKET>(server->serverSocket);
		SOCKET clientSocket = accept(thisSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET)
		{
			KalaServer::CreatePopup(
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

#ifdef _DEBUG
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"Raw HTTP request:\n" + request);
#endif

			if (request.starts_with("GET "))
			{
				size_t pathStart = 4;
				size_t pathEnd = request.find(' ', pathStart);
				if (pathEnd != string::npos)
				{
					filePath = request.substr(pathStart, pathEnd - pathStart);
				}
			}

			if (!server->blacklistedKeywords.empty())
			{
				sockaddr_storage addr{};
				socklen_t addrLen = sizeof(addr);
				getpeername(
					clientSocket,
					reinterpret_cast<sockaddr*>(&addr),
					&addrLen);

				char ipStr[INET6_ADDRSTRLEN]{};
				getnameinfo(
					reinterpret_cast<sockaddr*>(&addr),
					addrLen,
					ipStr,
					sizeof(ipStr),
					nullptr,
					0,
					NI_NUMERICHOST);
				string clientIP = ipStr;

				BannedIP banned;
				banned.IP = clientIP;
				banned.reason = filePath;

				if (server->IsBlacklistedRoute(filePath)
					&& !server->IsBannedIP(banned.IP))
				{
					server->BanIP(banned, static_cast<uintptr_t>(clientSocket));
					return false;
				}

				if (server->IsBannedIP(banned.IP))
				{
					server->StopBannedIP(banned, static_cast<uintptr_t>(clientSocket));
					return false;
				}
			}

			string body{};
			string statusLine = "HTTP/1.1 200 OK";

			if (!server->RouteExists(filePath))
			{
				KalaServer::PrintConsoleMessage(
					ConsoleMessageType::Type_Message,
					"User tried to access non-existing route '" + filePath + "'!");

				string result = server->ServeFile(server->errorMessage.error404);
				if (result == "")
				{
					KalaServer::PrintConsoleMessage(
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
					KalaServer::PrintConsoleMessage(
						ConsoleMessageType::Type_Warning,
						"User tried to access forbidden route '" + filePath + "' from path '" + server->whitelistedRoutes[filePath] + "'.");

					string result = server->ServeFile(server->errorMessage.error403);
					if (result == "")
					{
						KalaServer::PrintConsoleMessage(
							ConsoleMessageType::Type_Error,
							"Failed to load 403 error page!");
					}
					else body = result;

					statusLine = "HTTP/1.1 403 Forbidden";
				}
				else
				{
					try
					{
						string result = server->ServeFile(filePath);
						if (result == "")
						{
							string errorResult = server->ServeFile(server->errorMessage.error500);
							if (errorResult == "")
							{
								KalaServer::PrintConsoleMessage(
									ConsoleMessageType::Type_Error,
									"Failed to load 500 error page!");
							}
							else body = errorResult;
						}
						else body = result;
					}
					catch (const exception& e)
					{
						KalaServer::PrintConsoleMessage(
							ConsoleMessageType::Type_Error,
							"Error 500 when requesting page ("
							+ filePath
							+ ").\nError:\n"
							+ e.what());

						string result = server->ServeFile(server->errorMessage.error500);
						if (result == "")
						{
							KalaServer::PrintConsoleMessage(
								ConsoleMessageType::Type_Error,
								"Failed to load 500 error page!");
						}
						else body = result;

						statusLine = "HTTP/1.1 500 Internal Server Error";
					}
				}
			}

			sockaddr_storage addr{};
			socklen_t addrLen = sizeof(addr);
			getpeername(
				clientSocket,
				reinterpret_cast<sockaddr*>(&addr),
				&addrLen);

			char ipStr[INET6_ADDRSTRLEN]{};
			getnameinfo(
				reinterpret_cast<sockaddr*>(&addr),
				addrLen,
				ipStr,
				sizeof(ipStr),
				nullptr,
				0,
				NI_NUMERICHOST);
			string clientIP = ipStr;

			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"========== LOADING ROUTE ==========\n"
				" IP    : " + clientIP + "\n"
				" Route : " + filePath + "\n"
				"===================================\n");

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

			closesocket(clientSocket);
		}

		return true;
	}

	void Server::Quit() const
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