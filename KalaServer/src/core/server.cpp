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
#include <memory>

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"
#include "response/response_ok.hpp"
#include "response/response_404.hpp"
#include "response/response_403.hpp"
#include "response/response_500.hpp"
#include "response/response_banned.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::Server;
using KalaKit::Core::ConsoleMessageType;
using KalaKit::Core::PopupReason;
using KalaKit::DNS::CloudFlare;
using KalaKit::DNS::CustomDNS;
using KalaKit::ResponseSystem::Response_OK;
using KalaKit::ResponseSystem::Response_404;
using KalaKit::ResponseSystem::Response_403;
using KalaKit::ResponseSystem::Response_500;
using KalaKit::ResponseSystem::Response_Banned;

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
using std::make_unique;
using std::unique_ptr;
using std::istringstream;
using std::thread;
using std::lock_guard;

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
		istringstream ss(route);
		string segment{};

		while (getline(ss, segment, '/'))
		{
			if (segment.empty()) continue;

			for (const auto& keyword : blacklistedKeywords)
			{
				if (segment.find(keyword) != string::npos) return true;
			}
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

		auto respBanned = make_unique<Response_Banned>();
		respBanned->Init(
			target.reason,
			target.IP,
			clientSocket);
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

	void Server::Start() const
	{
		if (!CloudFlare::IsRunning()
			&& !CustomDNS::IsRunning())
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Neither cloudflared or dns was started! Please run atleast one of them.");
			return;
		}

		thread([]
		{
			SOCKET thisSocket = static_cast<SOCKET>(server->serverSocket);
			while (KalaServer::isRunning)
			{
				SOCKET clientSocket = accept(thisSocket, nullptr, nullptr);
				if (clientSocket == INVALID_SOCKET)
				{
					KalaServer::PrintConsoleMessage(
						ConsoleMessageType::Type_Error,
						"Accept failed: " + to_string(WSAGetLastError()));
					return;
				}
				else
				{
					thread([clientSocket]
						{
							Server::server->HandleClient(static_cast<uintptr_t>(clientSocket));
						}).detach();
				}
			}
		}).detach();

		thread([]
		{
			while (KalaServer::isRunning)
			{
				KalaServer::PrintConsoleMessage(
					ConsoleMessageType::Type_Message,
					"Server is still alive...");
				Sleep(5000);
			}
		}).detach();
	}

	void Server::HandleClient(uintptr_t socket)
	{
		KalaServer::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Entered handle client thread...");

		SOCKET rawSocket = static_cast<SOCKET>(socket);

		//prevent timeout - wait 5 seconds then exit

		DWORD timeout = 5000; //5 seconds
		setsockopt(
			rawSocket, 
			SOL_SOCKET,
			SO_RCVTIMEO, 
			(char*)&timeout, 
			sizeof(timeout));

		{
			lock_guard lock(server->clientSocketsMutex);
			server->activeClientSockets.insert(socket);
		}

		char buffer[2048] = {};
		int bytesReceived = recv(rawSocket, buffer, sizeof(buffer) - 1, 0);
		if (bytesReceived == 0)
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Client timed out or disconnected before sending request.");

			{
				std::lock_guard lock(server->clientSocketsMutex);
				server->activeClientSockets.erase(socket);
			}
			closesocket(rawSocket);
			return;
		}
		else
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

			string clientIP{};
			const string cfHeader = "CF-Connecting-IP:";
			size_t headerPos = request.find(cfHeader);
			if (headerPos != string::npos)
			{
				size_t start = headerPos + cfHeader.size();
				size_t end = request.find('\n', start);
				if (end != string::npos)
				{
					clientIP = request.substr(start, end - start);
					clientIP.erase(0, clientIP.find_first_not_of(" \t\r"));
					clientIP.erase(clientIP.find_last_not_of(" \t\r") + 1);
				}
			}

			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"Created new thread for user '" + clientIP + "'!");

			BannedIP banned{};
			banned.IP = clientIP;
			banned.reason = filePath;

			if (!server->blacklistedKeywords.empty())
			{
				if (server->IsBlacklistedRoute(filePath)
					&& !server->IsBannedIP(clientIP))
				{
					server->BanIP(banned, static_cast<uintptr_t>(rawSocket));
					{
						std::lock_guard lock(server->clientSocketsMutex);
						server->activeClientSockets.erase(socket);
					}
					return;
				}

				if (server->IsBannedIP(clientIP))
				{
					auto respBanned = make_unique<Response_Banned>();
					respBanned->Init(
						filePath,
						clientIP,
						rawSocket);
					{
						std::lock_guard lock(server->clientSocketsMutex);
						server->activeClientSockets.erase(socket);
					}
					return;
				}
			}

			string body{};
			string statusLine = "HTTP/1.1 200 OK";

			if (!server->RouteExists(filePath))
			{
				KalaServer::PrintConsoleMessage(
					ConsoleMessageType::Type_Message,
					"User tried to access non-existing route '" + filePath + "'!");

				auto resp404 = make_unique<Response_404>();
				resp404->Init(
					filePath,
					clientIP,
					rawSocket);
				{
					std::lock_guard lock(server->clientSocketsMutex);
					server->activeClientSockets.erase(socket);
				}
				return;
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

					auto resp403 = make_unique<Response_403>();
					resp403->Init(
						filePath,
						clientIP,
						rawSocket);
					{
						std::lock_guard lock(server->clientSocketsMutex);
						server->activeClientSockets.erase(socket);
					}
					return;
				}
				else
				{
					try
					{
						string result = server->ServeFile(filePath);
						if (result == "")
						{
							auto resp500 = make_unique<Response_500>();
							resp500->Init(
								filePath,
								clientIP,
								rawSocket);
							{
								std::lock_guard lock(server->clientSocketsMutex);
								server->activeClientSockets.erase(socket);
							}
							return;
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

						auto resp500 = make_unique<Response_500>();
						resp500->Init(
							filePath,
							clientIP,
							rawSocket);

						{
							std::lock_guard lock(server->clientSocketsMutex);
							server->activeClientSockets.erase(socket);
						}
					}
				}
			}
		}
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

			for (uintptr_t userSocket : server->activeClientSockets)
			{
				SOCKET userRawSocket = static_cast<SOCKET>(userSocket);
				shutdown(userRawSocket, SD_BOTH);
				closesocket(userRawSocket);
			}
			server->activeClientSockets.clear();

			WSACleanup();
		}
	}
}