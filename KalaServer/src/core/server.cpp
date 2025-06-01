//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <wininet.h>
#include <map>
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <memory>

#pragma comment(lib, "Wininet.lib")

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"
#include "response/response_200.hpp"
#include "response/response_404.hpp"
#include "response/response_403.hpp"
#include "response/response_418.hpp"
#include "response/response_500.hpp"

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
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using std::wstring;
using std::atomic_bool;

namespace KalaKit::Core
{
	atomic<bool> canUpdateBannedIPs{ false };
	vector<pair<string, string>> bannedIPs{};

	atomic<bool> canUpdateMachineIPs{ false };
	vector<string> machineIPs{};

	void Server::Initialize(
		unsigned int port,
		unsigned int healthTimer,
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
			healthTimer,
			serverName,
			domainName,
			errorMessage,
			whitelistedRoutesFolder,
			blacklistedKeywords,
			extensions);

		KalaServer::PrintConsoleMessage(
			0,
			false,
			ConsoleMessageType::Type_Message,
			"",
			"=============================="
			"\n"
			"Initializing server '" + Server::server->GetServerName() + "'"
			"\n"
			"=============================="
			"\n");

		server->bannedBotsFile = (path(current_path() / "banned-ips.txt")).string();
		string bannedBotsFile = server->GetBannedBotsFilePath();
		if (!exists(bannedBotsFile))
		{
			ofstream log(bannedBotsFile);
			if (!log)
			{
				KalaServer::CreatePopup(
					PopupReason::Reason_Error,
					"Failed to create banned-ips.txt!");
				return;
			}
			log.close();

			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Message,
				"SERVER",
				"Created 'banned-ips.txt' at root directory.\n");
		}

		server->AddInitialWhitelistedRoutes();
		
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

		KalaServer::PrintConsoleMessage(
			0,
			false,
			ConsoleMessageType::Type_Message,
			"",
			"");

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
			"Server is running on port '" + to_string(server->port) + "'.");

		KalaServer::isRunning = true;

		KalaServer::PrintConsoleMessage(
			0,
			false,
			ConsoleMessageType::Type_Message,
			"",
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

	pair<string, string> Server::IsBannedClient(const string& targetIP) const
	{
		pair<string, string> banReason{};

		if (bannedIPs.size() == 0)
		{
			canUpdateBannedIPs = true;
			Server::server->GetBannedIPs();
		}

		if (canUpdateBannedIPs) Server::server->GetBannedIPs();

		for (const pair<string, string>& bannedClient : bannedIPs)
		{
			if (bannedClient.first == targetIP)
			{
				banReason.first = bannedClient.first;
				banReason.second = bannedClient.second;
			}
		}

		return banReason;
	}

	bool Server::BanIP(const pair<string, string>& target) const
	{
		string bannedBotsPath = server->GetBannedBotsFilePath();

		ifstream readFile(bannedBotsPath);
		if (!readFile)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Failed to read 'banned-ips.txt' to ban IP!");
			return false;
		}

		string line;
		while (getline(readFile, line))
		{
			auto delimiterPos = line.find('|');
			if (delimiterPos != string::npos)
			{
				string ip = line.substr(0, delimiterPos);
				string cleanedIP = ip.substr(0, ip.find_last_not_of(" \t") + 1);

				if (cleanedIP == target.first) return false; //user is already banned
			}
		}

		readFile.close();

		ofstream writeFile(bannedBotsPath, ios::app);
		if (!writeFile)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Failed to write into 'banned-ips.txt' to ban IP!");
			return false;
		}

		writeFile << target.first << " | " << target.second << "\n";
		bannedIPs.push_back(target);

		writeFile.close();

		return true;
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

			bool isWhiteListed = false;
			for (const auto& ext : Server::server->whitelistedExtensions)
			{
				if (path(route).extension() == ext)
				{
					isWhiteListed = true;
					break;
				}
			}
			if (!isWhiteListed) continue;

			//get clean route for html files
			string correctRootPath{};
			if (path(route).extension() == ".html")
			{
				path relativePath = relative(route.path(), server->whitelistedRoutesFolder);
				relativePath.replace_extension("");
				correctRootPath = "/" + relativePath.generic_string();

				//fix index.html root path
				if (correctRootPath == "/index") correctRootPath = "/";
			}
			else
			{
				path relativePath = relative(route.path(), server->whitelistedRoutesFolder);
				correctRootPath = "/" + relativePath.generic_string();
			}

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
				0,
				true,
				ConsoleMessageType::Type_Warning,
				"SERVER",
				"Route '" + rootPath + "' has already been whitelisted!");
			return;
		}

		path fullPath = path(filePath);
		if (!exists(fullPath))
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Page path '" + filePath + "' does not exist!.");

			return;
		}

		server->whitelistedRoutes[rootPath] = filePath;

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
			"Added new route '" + rootPath + "'");
	}
	void Server::AddNewWhitelistedExtension(const string& newExtension) const
	{
		for (const auto& extension : server->whitelistedExtensions)
		{
			if (extension == newExtension)
			{
				KalaServer::PrintConsoleMessage(
					0,
					true,
					ConsoleMessageType::Type_Warning,
					"SERVER",
					"Extension '" + extension + "' has already been whitelisted!");
				return;
			}
		}

		server->whitelistedExtensions.push_back(newExtension);
		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
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
				0,
				true,
				ConsoleMessageType::Type_Warning,
				"SERVER",
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
				0,
				true,
				ConsoleMessageType::Type_Warning,
				"SERVER",
				"Extension '" + thisExtension + "' cannot be removed because it hasn't been whitelisted!");
		}
	}

	string Server::ServeFile(const string& route)
	{
		if (route.empty())
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVE-FILE",
				"Serve received empty route!");
			return "";
		}

		auto it = server->whitelistedRoutes.find(route);
		if (it == server->whitelistedRoutes.end())
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVE-FILE",
				"Route '" + route + "' is not whitelisted!");
			return "";
		}

		try
		{
			path fullFilePath = it->second;
			ifstream file(fullFilePath);
			if (!file)
			{
				KalaServer::PrintConsoleMessage(
					0,
					true,
					ConsoleMessageType::Type_Error,
					"SERVE-FILE",
					"Failed to open file '" + fullFilePath.generic_string() + "'!");
				return "";
			}

			stringstream buffer{};
			buffer << file.rdbuf();
			return buffer.str();
		}
		catch (const exception& e)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVE-FILE",
				"Exception while serving route '" + route + "':\n" + e.what());
			return "";
		}

		return "";
	}

	bool Server::HasInternet()
	{
		if (CloudFlare::tunnelName == "")
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Cannot check for internet access because tunnel name has not been assigned.");
			return false;
		}

		const string url = "https://www.google.com";
		const wstring wideTunnelName(url.begin(), url.end());
		HINTERNET hInternet = InternetOpenW(
			const_cast<LPWSTR>(wideTunnelName.c_str()),
			INTERNET_OPEN_TYPE_DIRECT,
			NULL,
			NULL,
			0);

		if (!hInternet) return false;

		HINTERNET hUrl = InternetOpenUrlW(
			hInternet,
			wstring(url.begin(), url.end()).c_str(),
			NULL,
			0,
			INTERNET_FLAG_NO_UI,
			0);
		bool result = (hUrl != nullptr);

		if (hUrl) InternetCloseHandle(hUrl);
		if (hInternet) InternetCloseHandle(hInternet);

		return result;
	}

	bool Server::IsTunnelAlive(uintptr_t tunnelHandle)
	{
		if (tunnelHandle == NULL)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Cannot check for tunnel state because tunnel '" + CloudFlare::tunnelName + "' is not running.");
			return false;
		}

		HANDLE rawTunnelHandle = reinterpret_cast<HANDLE>(tunnelHandle);

		if (rawTunnelHandle == INVALID_HANDLE_VALUE)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Cannot check for tunnel state because handle for tunnel '" + CloudFlare::tunnelName + "' is invalid.");
			return false;
		}

		return
			rawTunnelHandle
			&& WaitForSingleObject(rawTunnelHandle, 0)
			== WAIT_TIMEOUT;
	}

	void Server::Start() const
	{
		if (!Server::server->isServerReady)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Server '" + Server::server->serverName + "' is not ready to start! Do not call this manually.");
			return;
		}

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
			KalaServer::PrintConsoleMessage(
				0,
				false,
				ConsoleMessageType::Type_Message,
				"",
				"\n"
				"=============================="
				"\n"
				"Ready to accept connections!"
				"\n"
				"=============================="
				"\n");

			SOCKET thisSocket = static_cast<SOCKET>(server->serverSocket);
			while (KalaServer::isRunning)
			{
				bool isHealthy = CloudFlare::IsConnHealthy(0)
					&& CloudFlare::IsConnHealthy(1)
					&& CloudFlare::IsConnHealthy(2)
					&& CloudFlare::IsConnHealthy(3);

				if (!isHealthy)
				{
					bool isOnline =
						Server::server->IsTunnelAlive(CloudFlare::tunnelRunHandle)
						&& Server::server->HasInternet();

					if (isOnline) continue;

					sleep_for(seconds(1)); //wait a second instead of spamming full check every frame
					continue;
				}

				SOCKET clientSocket = accept(thisSocket, nullptr, nullptr);
				if (clientSocket == INVALID_SOCKET)
				{
					KalaServer::PrintConsoleMessage(
						2,
						true,
						ConsoleMessageType::Type_Error,
						"CLIENT",
						"Accept failed: " + to_string(WSAGetLastError()));
					continue;
				}
				else
				{
					thread([clientSocket]
						{
							Server::server->HandleClient(static_cast<uintptr_t>(clientSocket));
						}).detach();
				}

				sleep_for(milliseconds(5));
			}
		}).detach();

		unsigned int healthTimer = Server::server->healthTimer;
		if (healthTimer > 0)
		{
			thread([healthTimer]
			{
				while (KalaServer::isRunning)
				{
					bool hasInternet = Server::server->HasInternet();
					bool isTunnelAlive = Server::server->IsTunnelAlive(CloudFlare::tunnelRunHandle);

					string internetStatus = hasInternet ? "[NET: OK]" : "[NET: FAIL]";
					string tunnelStatus = isTunnelAlive ? "[TUNNEL: OK]" : "[TUNNEL: FAIL]";

					string fullStatus =
						"Server status: " + internetStatus
						+ ", " + tunnelStatus;

					KalaServer::PrintConsoleMessage(
						0,
						true,
						ConsoleMessageType::Type_Message,
						"SERVER",
						fullStatus + "\n");
					sleep_for(seconds(healthTimer));
				}
			}).detach();
		}

		thread([]
		{
			while (KalaServer::isRunning)
			{
				if (!canUpdateMachineIPs) canUpdateMachineIPs = true;

				sleep_for(seconds(300));
			}
		}).detach();

		thread([]
		{
			while (KalaServer::isRunning)
			{
				if (!canUpdateBannedIPs) canUpdateBannedIPs = true;

				sleep_for(seconds(300));
			}
		}).detach();
	}

	void Server::GetMachineIPs()
	{
		if (!canUpdateMachineIPs) return;

		machineIPs.clear();

		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = nullptr;

		HANDLE hReadPipe = nullptr;
		HANDLE hWritePipe = nullptr;
		if (!CreatePipe(
			&hReadPipe,
			&hWritePipe,
			&sa,
			0))
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Failed to create read/write pipe for storing host IPs!");
			return;
		}
		if (!SetHandleInformation(
			hReadPipe,
			HANDLE_FLAG_INHERIT,
			0))
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Failed to set up pipe handle inheritance for storing host IPs!");
			return;
		}

		STARTUPINFOW si{};
		PROCESS_INFORMATION pi{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = hWritePipe;
		si.hStdError = hWritePipe;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		wchar_t command[] = L"ipconfig";

		if (!CreateProcessW(
			nullptr,
			command,
			nullptr,
			nullptr,
			TRUE,
			CREATE_NO_WINDOW,
			nullptr,
			nullptr,
			&si,
			&pi))
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Failed to create process for getting host IPs for storing host IPs!");

			CloseHandle(hWritePipe);
			CloseHandle(hReadPipe);

			return;
		}

		CloseHandle(hWritePipe);

		stringstream output{};
		char buffer[4096]{};
		DWORD bytesRead = 0;

		while (ReadFile(
			hReadPipe,
			buffer,
			sizeof(buffer) - 1,
			&bytesRead,
			nullptr)
			&& bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			output << buffer;
		}

		CloseHandle(hReadPipe);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		//parse output

		string line{};
		while (getline(output, line))
		{
			size_t pos = line.find(": ");
			if (pos == string::npos) continue;

			string key = line.substr(0, pos);
			string value = line.substr(pos + 2);
			erase_if(value, ::isspace); //remove all spaces

			if (key.find("IPv4") != string::npos
				|| key.find("IPv6") != string::npos)
			{
				size_t zone = value.find('%');
				if (zone != string::npos)
				{
					value = value.substr(0, zone);
				}

				machineIPs.push_back(value);
			}
		}

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
			"Refreshed machine IPs");

		canUpdateMachineIPs = false;
	}

	void Server::GetBannedIPs()
	{
		if (!canUpdateBannedIPs) return;

		bannedIPs.clear();

		string bannedBotsFile = server->GetBannedBotsFilePath();

		bool result = false;

		ifstream file(bannedBotsFile);
		if (!file)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Failed to open 'banned-ips.txt' to check if IP is banned or not!");
			return;
		}

		string line;
		while (getline(file, line))
		{
			if (line.find("#") != string::npos
				|| line.find("=") != string::npos
				|| line.find("|") == string::npos
				|| line.empty())
			{
				continue;
			}

			auto delimiterPos = line.find('|');
			if (delimiterPos != string::npos)
			{
				string ip = line.substr(0, delimiterPos);
				string reason = line.substr(delimiterPos + 1);

				erase_if(ip, ::isspace);

				auto cleanedReason = find_if(reason.begin(), reason.end(), ::isspace);
				if (cleanedReason != reason.end()) reason.erase(cleanedReason);

				pair<string, string> newBannedIPsPair{};

				newBannedIPsPair.first = ip;
				newBannedIPsPair.second = reason;

				bannedIPs.push_back(newBannedIPsPair);
			}
		}

		file.close();

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
			"Refreshed banned IPs");

		canUpdateBannedIPs = false;
	}

	bool Server::IsHost(const string& targetIP)
	{
		if (machineIPs.size() == 0)
		{
			canUpdateMachineIPs = true;
			Server::server->GetMachineIPs();
		}

		if (canUpdateMachineIPs) Server::server->GetMachineIPs();

		for (const auto& ip : machineIPs)
		{
			if (ip == targetIP) return true;
		}

		return false;
	}

	string Server::ExtractHeader(
		const string& request, 
		const string& headerName)
	{
		string result{};
		istringstream stream(request);

		string line{};
		while (getline(stream, line))
		{
			size_t colonPos = line.find(':');
			if (colonPos != string::npos)
			{
				string key = line.substr(0, colonPos);
				string value = line.substr(colonPos + 1);

				//trim whitespace

				key.erase(remove_if(key.begin(), key.end(), ::isspace), key.end());
				value.erase(0, value.find_first_not_of(" \t\r\n"));
				value.erase(value.find_last_not_of(" \t\r\n") + 1);

				//case-insensitive compare

				transform(key.begin(), key.end(), key.begin(), ::tolower);
				string lowerHeader = headerName;
				transform(lowerHeader.begin(), lowerHeader.end(), lowerHeader.begin(), ::tolower);

				if (key == lowerHeader)
				{
					size_t comma = value.find(',');
					if (comma != string::npos)
					{
						value = value.substr(0, comma);
					}

					return value;
				}
			}
		}
		return result;
	}

	void Server::HandleClient(uintptr_t socket)
	{
		KalaServer::PrintConsoleMessage(
			2,
			true,
			ConsoleMessageType::Type_Debug,
			"CLIENT",
			"Socket [" + to_string(socket) + "] entered handle client thread...");

		SOCKET rawClientSocket = static_cast<SOCKET>(socket);
		uintptr_t clientSocket = static_cast<uintptr_t>(rawClientSocket);

		DWORD timeout = 1000;
		setsockopt(
			rawClientSocket,
			SOL_SOCKET,
			SO_RCVTIMEO,
			(char*)&timeout,
			sizeof(timeout));

		{
			lock_guard lock(server->clientSocketsMutex);
			server->activeClientSockets.insert(socket);
		}

		char buffer[2048] = {};
		int bytesReceived = recv(rawClientSocket, buffer, sizeof(buffer) - 1, 0);

		if (bytesReceived == SOCKET_ERROR)
		{
			DWORD err = WSAGetLastError();
			if (err == WSAETIMEDOUT)
			{
				KalaServer::PrintConsoleMessage(
					2,
					true,
					ConsoleMessageType::Type_Warning,
					"CLIENT",
					"Socket [" + to_string(socket) + "] timed out after 5 seconds without sending any data.");
			}
			else
			{
				KalaServer::PrintConsoleMessage(
					2,
					true,
					ConsoleMessageType::Type_Warning,
					"CLIENT",
					"Socket [" + to_string(socket) + "] recv() failed with error: " + to_string(err));
			}

			Server::server->SocketCleanup(socket);
			closesocket(rawClientSocket);
			return;
		}
		else if (bytesReceived == 0)
		{
			KalaServer::PrintConsoleMessage(
				2,
				true,
				ConsoleMessageType::Type_Warning,
				"CLIENT",
				"Socket [" + to_string(socket) + "] disconnected without sending any data.");

			Server::server->SocketCleanup(socket);
			closesocket(rawClientSocket);
			return;
		}
		else
		{
			string request(buffer);
			string filePath = "/";

			KalaServer::PrintConsoleMessage(
				2,
				true,
				ConsoleMessageType::Type_Debug,
				"CLIENT",
				"Socket [" + to_string(socket) + "] raw HTTP request:\n" + request);

			if (request.starts_with("GET "))
			{
				size_t pathStart = 4;
				size_t pathEnd = request.find(' ', pathStart);
				if (pathEnd != string::npos)
				{
					filePath = request.substr(pathStart, pathEnd - pathStart);
				}
			}

			//attempts to get client ip from cloudflare header

			string clientIP = ExtractHeader(
				request,
				"Cf-Connecting-Ip");

			if (clientIP.empty())
			{
				clientIP = "unknown-ip";

				KalaServer::PrintConsoleMessage(
					0,
					true,
					ConsoleMessageType::Type_Warning,
					"SERVER",
					"Failed to get client IP for socket [" + to_string(socket) + "]!"
					"\n"
					"Full request dump :"
					"\n" +
					request);
			}

			bool isHost = IsHost(clientIP);
			if (isHost)
			{
				KalaServer::PrintConsoleMessage(
					0,
					true,
					ConsoleMessageType::Type_Message,
					"SERVER",
					"Client [" + to_string(socket) + " - '" + clientIP + "'] is server host.");
				clientIP = "host";
			}

			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Message,
				"SERVER",
				"New client successfully connected [" + to_string(socket) + " - '" + clientIP + "']!");

			pair<string, string> bannedClient = server->IsBannedClient(clientIP);
			if (bannedClient.first != "")
			{
				KalaServer::PrintConsoleMessage(
					0,
					false,
					ConsoleMessageType::Type_Message,
					"",
					"\n"
					"======= BANNED USER REPEATED ACCESS ATTEMPT ========\n"
					" IP     : " + clientIP + "\n"
					" Socket : " + to_string(clientSocket) + "\n"
					" Reason : " + bannedClient.second + "\n"
					"====================================================\n");

				sleep_for(milliseconds(5));
				auto respBanned = make_unique<Response_Banned>();
				respBanned->Init(
					rawClientSocket,
					clientIP,
					filePath,
					"text/html");
				Server::server->SocketCleanup(socket);
				return;
			}
			else if (bannedClient.first == ""
					 && !server->blacklistedKeywords.empty()
					 && server->IsBlacklistedRoute(filePath))
			{
				KalaServer::PrintConsoleMessage(
					0,
					false,
					ConsoleMessageType::Type_Message,
					"",
					"\n"
					"=============== BANNED CLIENT ======================\n"
					" IP     : " + clientIP + "\n"
					" Socket : " + to_string(clientSocket) + "\n"
					" Reason : " + filePath + "\n"
					"====================================================\n");

				sleep_for(milliseconds(5));

				pair<string, string> bannedClient{};
				bannedClient.first = clientIP;
				bannedClient.second = filePath;

				if (server->BanIP(bannedClient))
				{
					auto respBanned = make_unique<Response_Banned>();
					respBanned->Init(
						clientSocket,
						clientIP,
						filePath,
						"text/html");
				}

				Server::server->SocketCleanup(socket);
				return;
			}

			string body{};
			string statusLine = "HTTP/1.1 200 OK";

			if (!server->RouteExists(filePath))
			{
				KalaServer::PrintConsoleMessage(
					2,
					true,
					ConsoleMessageType::Type_Message,
					"CLIENT",
					"Client [" 
					+ to_string(socket) + " - '" + clientIP + "'] tried to access non-existing route '" 
					+ filePath + "'!");

				auto resp404 = make_unique<Response_404>();
				resp404->Init(
					rawClientSocket,
					clientIP,
					filePath,
					"text/html");
				Server::server->SocketCleanup(socket);
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
						2,
						true,
						ConsoleMessageType::Type_Warning,
						"CLIENT",
						"Client [" 
						+ to_string(socket) + " - '" + clientIP + "'] tried to access forbidden route '" + filePath 
						+ "' from path '" + server->whitelistedRoutes[filePath] + "'.");

					auto resp403 = make_unique<Response_403>();
					resp403->Init(
						rawClientSocket,
						clientIP,
						filePath,
						"text/html");
					Server::server->SocketCleanup(socket);
					return;
				}
				else
				{
					try
					{
						string result = server->ServeFile(filePath);
						if (result == "")
						{
							KalaServer::PrintConsoleMessage(
								2,
								true,
								ConsoleMessageType::Type_Error,
								"CLIENT",
								"Client ["
								+ to_string(socket) + " - '" + clientIP + "'] tried to access broken route '"
								+ filePath + "'.");

							auto resp500 = make_unique<Response_500>();
							resp500->Init(
								rawClientSocket,
								clientIP,
								filePath,
								"text/html");
							Server::server->SocketCleanup(socket);
							return;
						}
						else
						{
							auto resp200 = make_unique<Response_OK>();
							resp200->Init(
								rawClientSocket,
								clientIP,
								filePath,
								"text/html");
							Server::server->SocketCleanup(socket);
							return;
						}
					}
					catch (const exception& e)
					{
						KalaServer::PrintConsoleMessage(
							2,
							true,
							ConsoleMessageType::Type_Error,
							"CLIENT",
							"Client ["
							+ to_string(socket) + " - '" + clientIP + "'] tried to access broken route '"
							+ filePath + "'.\nError:\n" + e.what());

						auto resp500 = make_unique<Response_500>();
						resp500->Init(
							rawClientSocket,
							clientIP,
							filePath,
							"text/html");
						Server::server->SocketCleanup(socket);
					}
				}
			}
		}
	}

	void Server::SocketCleanup(uintptr_t clientSocket)
	{
		{
			std::lock_guard lock(server->clientSocketsMutex);
			server->activeClientSockets.erase(clientSocket);
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