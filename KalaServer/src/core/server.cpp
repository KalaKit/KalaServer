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

using std::unordered_map;
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
	atomic<bool> canUpdateWhitelistedRoutes{ true };
	unordered_map<string, Route> whitelistedRoutes{};

	vector<string> blacklistedKeywords{};

	vector<string> whitelistedExtensions{};

	vector<string> whitelistedIPs{};

	atomic<bool> canUpdateBannedIPs{ true };
	vector<pair<string, string>> bannedIPs{};

	atomic<bool> canUpdateMachineIPs{ true };
	vector<string> machineIPs{};

	bool Server::Initialize(
		u16 port,
		u16 healthTimer,
		const string& serverName,
		const string& domainName,
		const ErrorMessage& errorMessage,
		const DataFile& dataFile)
	{
		server = make_unique<Server>(
			port,
			healthTimer,
			serverName,
			domainName,
			errorMessage,
			dataFile);

		if (!server->PreInitializeCheck()) return false;

		KalaServer::PrintConsoleMessage(
			0,
			false,
			ConsoleMessageType::Type_Message,
			"",
			"=============================="
			"\n"
			"Initializing server '" + server->GetServerName() + "'"
			"\n"
			"=============================="
			"\n");

		server->GetFileData(DataFileType::datafile_extension);
		server->GetWhitelistedRoutes();
		server->GetFileData(DataFileType::datafile_whitelistedIP);
		server->GetFileData(DataFileType::datafile_bannedIP);
		server->GetFileData(DataFileType::datafile_blacklistedKeyword);
		
		WSADATA wsaData{};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"WSAStartup failed!");
			return false;
		}

		SOCKET thisSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		server->serverSocket = static_cast<uintptr_t>(thisSocket);

		if (thisSocket == INVALID_SOCKET)
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Socket creation failed!");
			return false;
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
			return false;
		}

		if (listen(thisSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Listen failed!");
			return false;
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

		return true;
	}

	bool Server::PreInitializeCheck() const
	{
		if (!KalaServer::IsRunningAsAdmin())
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"This program must be ran with admin privileges!");
			return false;
		}

		if (serverName == "")
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with empty server name!");
			return false;
		}
		if (domainName == "")
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with empty domain name!");
			return false;
		}

		string whitelistedRoutesFolderPath = (current_path() / server->dataFile.whitelistedRoutesFolder).string();
		if (!exists(whitelistedRoutesFolderPath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with invalid whitelisted routes folder '" + whitelistedRoutesFolderPath + "'!");
			return false;
		}

		string whitelistedExtensionsFilePath = (
			current_path() / server->dataFile.whitelistedExtensionsFile).string();
		if (!exists(whitelistedExtensionsFilePath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with invalid whitelisted extensions file '" + whitelistedExtensionsFilePath + "'!");
			return false;
		}

		string whitelistedIPsFilePath = (
			current_path() / server->dataFile.whitelistedIPsFile).string();
		if (!exists(whitelistedIPsFilePath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with invalid whitelisted IPs file '" + whitelistedIPsFilePath + "'!");
			return false;
		}

		string bannedIPsFilePath = (
			current_path() / server->dataFile.bannedIPsFile).string();
		if (!exists(bannedIPsFilePath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with invalid banned IPs file '" + bannedIPsFilePath + "'!");
			return false;
		}

		string blacklistedKeywordsFilePath = (
			current_path() / server->dataFile.blacklistedKeywordsFile).string();
		if (!exists(blacklistedKeywordsFilePath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start server with invalid blacklisted keywords file '" + blacklistedKeywordsFilePath + "'!");
			return false;
		}

		return true;
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
				if (segment.find(keyword) != string::npos)
				{
					return true;
				}
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
			server->GetBannedIPs();
		}

		if (canUpdateBannedIPs) server->GetBannedIPs();

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

	bool Server::BanClient(const pair<string, string>& target) const
	{
		ifstream readFile(server->dataFile.bannedIPsFile);
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

		ofstream writeFile(server->dataFile.bannedIPsFile, ios::app);
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

	void Server::GetWhitelistedRoutes() const
	{
		bool canContinue = 
			whitelistedRoutes.size() == 0 
			|| canUpdateWhitelistedRoutes;

		if (!canContinue) return;

		string routeFolder = server->dataFile.whitelistedRoutesFolder;
		if (routeFolder.empty())
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Whitelisted routes root folder has not been assigned!");
			return;
		}

		path routePath = current_path() / routeFolder;
		if (!exists(routePath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Whitelisted routes root folder '" + server->dataFile.whitelistedRoutesFolder + "' does not exist!");
			return;
		}

		for (const auto& route : recursive_directory_iterator(routeFolder))
		{
			string cleanedRoute = path(route).generic_string();

			bool isWhiteListed = false;

			for (const auto& ext : whitelistedExtensions)
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
				path relativePath = relative(route.path(), server->dataFile.whitelistedRoutesFolder);
				relativePath.replace_extension("");
				correctRootPath = "/" + relativePath.generic_string();

				//fix index.html root path
				if (correctRootPath == "/index") correctRootPath = "/";
			}
			else
			{
				path relativePath = relative(route.path(), server->dataFile.whitelistedRoutesFolder);
				correctRootPath = "/" + relativePath.generic_string();
			}

			//use actual file path directly
			string correctFilePath = route.path().generic_string();

			server->AddNewWhitelistedRoute(correctRootPath, correctFilePath);
		}

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
			"Refreshed whitelisted routes");

		canUpdateWhitelistedRoutes = false;
	}

	void Server::GetFileData(DataFileType dataFileType) const
	{
		string resultType{};

		vector<pair<string, string>> result{};
		string filePath{};
		string fullFilePath{};

		switch (dataFileType)
		{
		case DataFileType::datafile_extension:
			resultType = "whitelisted extensions";
			whitelistedExtensions.clear();
			filePath = server->dataFile.whitelistedExtensionsFile;
			fullFilePath = path(current_path() / server->dataFile.whitelistedExtensionsFile).string();
			break;
		case DataFileType::datafile_whitelistedIP:
			resultType = "whitelisted IPs";
			whitelistedIPs.clear();
			filePath = server->dataFile.whitelistedIPsFile;
			fullFilePath = path(current_path() / server->dataFile.whitelistedIPsFile).string();
			break;
		case DataFileType::datafile_bannedIP:
			resultType = "banned IPs";
			filePath = server->dataFile.bannedIPsFile;
			fullFilePath = path(current_path() / server->dataFile.bannedIPsFile).string();
			break;
		case DataFileType::datafile_blacklistedKeyword:
			resultType = "blacklisted keywords";
			blacklistedKeywords.clear();
			filePath = server->dataFile.blacklistedKeywordsFile;
			fullFilePath = path(current_path() / server->dataFile.blacklistedKeywordsFile).string();
			break;
		}

		bool canResetBannedIPs =
			bannedIPs.size() == 0
			|| canUpdateBannedIPs;

		if (dataFileType == DataFileType::datafile_bannedIP
			&& !canResetBannedIPs)
		{
			return;
		}

		if (dataFileType == DataFileType::datafile_bannedIP) bannedIPs.clear();

		ifstream file(fullFilePath);
		if (!file)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Failed to open '" + path(filePath).filename().string() + "'!");
			return;
		}

		string line;
		while (getline(file, line))
		{
			line.erase(0, line.find_first_not_of(" \t\r\n"));
			line.erase(line.find_last_not_of(" \t\r\n") + 1);

			if (line.empty()
				|| line.find("#") != string::npos
				|| line.find("=") != string::npos)
			{
				continue;
			}

			auto delimiterPos = line.find('|');
			if (delimiterPos != string::npos)
			{
				string key = line.substr(0, delimiterPos);
				string value = line.substr(delimiterPos + 1);

				erase_if(key, ::isspace);

				auto cleanedReason = find_if(value.begin(), value.end(), ::isspace);
				if (cleanedReason != value.end()) value.erase(cleanedReason);

				pair<string, string> newPair{};

				newPair.first = key;
				newPair.second = value;

				result.push_back(newPair);
			}
			else
			{
				string key = line.substr(0, delimiterPos);

				erase_if(key, ::isspace);

				pair<string, string> newPair{};

				newPair.first = key;
				newPair.second = "";

				result.push_back(newPair);
			}
		}

		file.close();

		switch (dataFileType)
		{
		case DataFileType::datafile_extension:
			for (const pair<string, string> resultValue : result)
			{
				whitelistedExtensions.push_back(resultValue.first);
			}
			break;
		case DataFileType::datafile_whitelistedIP:
			for (const pair<string, string> resultValue : result)
			{
				whitelistedIPs.push_back(resultValue.first);
			}
			break;
		case DataFileType::datafile_bannedIP:
			for (const pair<string, string> resultValue : result)
			{
				bannedIPs.push_back(resultValue);
			}
			break;
		case DataFileType::datafile_blacklistedKeyword:
			for (const pair<string, string> resultValue : result)
			{
				blacklistedKeywords.push_back(resultValue.first);
			}
			break;
		}

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
			"Refreshed " + resultType);
	}

	void Server::AddNewWhitelistedRoute(const string& rootPath, const string& filePath) const
	{
		for (const auto& r : whitelistedRoutes)
		{
			if (r.rootPath == rootPath) return;
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

		string s_mimeType = path(filePath).extension().string();
		string_view sv_mimeType(s_mimeType);

		string_view sv_newMimeType = GetMimeType(sv_mimeType);
		string s_newMimeType(sv_newMimeType);

		Route newRoute =
		{
			.rootPath = rootPath,
			.filePath = filePath,
			.mimeType = s_newMimeType
		};

		whitelistedRoutes.push_back(newRoute);

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
			"Added new route '" + rootPath + "'");
	}
	void Server::AddNewWhitelistedExtension(const string& newExtension) const
	{
		for (const auto& extension : whitelistedExtensions)
		{
			if (extension == newExtension) return;
		}

		whitelistedExtensions.push_back(newExtension);
		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"SERVER",
			"Added new extension '" + newExtension + "'");
	}

	void Server::RemoveWhitelistedRoute(const string& thisRoute) const
	{
		GetWhitelistedRoutes();

		string foundRoute{};
		for (const auto& r : whitelistedRoutes)
		{
			if (r.rootPath == thisRoute)
			{
				foundRoute = thisRoute;
				break;
			}
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
		for (const auto& extension : whitelistedExtensions)
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

		server->GetWhitelistedRoutes();

		Route foundRoute{};
		for (const auto& r : whitelistedRoutes)
		{
			if (r.rootPath == route)
			{
				foundRoute = r;
				break;
			}
		}
		if (foundRoute.rootPath.empty())
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
			path fullFilePath = foundRoute.filePath;
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
		if (!server->isServerReady)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"SERVER",
				"Server '" + server->serverName + "' is not ready to start! Do not call this manually.");
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
						server->IsTunnelAlive(CloudFlare::tunnelRunHandle)
						&& server->HasInternet();

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
							server->HandleClient(static_cast<uintptr_t>(clientSocket));
						}).detach();
				}

				sleep_for(milliseconds(5));
			}
		}).detach();

		unsigned int healthTimer = server->healthTimer;
		if (healthTimer > 0)
		{
			thread([healthTimer]
			{
				while (KalaServer::isRunning)
				{
					bool hasInternet = server->HasInternet();
					bool isTunnelAlive = server->IsTunnelAlive(CloudFlare::tunnelRunHandle);

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

				sleep_for(seconds(60));
			}
		}).detach();

		thread([]
		{
			while (KalaServer::isRunning)
			{
				if (!canUpdateWhitelistedRoutes) canUpdateWhitelistedRoutes = true;

				sleep_for(seconds(60));
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

		server->GetFileData(DataFileType::datafile_bannedIP);

		canUpdateBannedIPs = false;
	}

	bool Server::IsHost(const string& targetIP)
	{
		if (machineIPs.size() == 0)
		{
			canUpdateMachineIPs = true;
			server->GetMachineIPs();
		}

		if (canUpdateMachineIPs) server->GetMachineIPs();

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

			server->SocketCleanup(socket);
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

			server->SocketCleanup(socket);
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

			string clientIP = server->ExtractHeader(
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

			pair<string, string> bannedClient = server->IsBannedClient(clientIP);
			if (bannedClient.first != "")
			{
				KalaServer::PrintConsoleMessage(
					0,
					false,
					ConsoleMessageType::Type_Message,
					"",
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
				server->SocketCleanup(socket);
				return;
			}
			else if (bannedClient.first == ""
					 && !blacklistedKeywords.empty()
					 && server->IsBlacklistedRoute(filePath))
			{
				KalaServer::PrintConsoleMessage(
					0,
					false,
					ConsoleMessageType::Type_Message,
					"",
					"=============== BANNED CLIENT ======================\n"
					" IP     : " + clientIP + "\n"
					" Socket : " + to_string(clientSocket) + "\n"
					" Reason : " + filePath + "\n"
					"====================================================\n");

				sleep_for(milliseconds(5));

				pair<string, string> bannedClient{};
				bannedClient.first = clientIP;
				bannedClient.second = filePath;

				if (server->BanClient(bannedClient))
				{
					auto respBanned = make_unique<Response_Banned>();
					respBanned->Init(
						clientSocket,
						clientIP,
						filePath,
						"text/html");
				}

				server->SocketCleanup(socket);
				return;
			}

			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Message,
				"SERVER",
				"New client successfully connected [" + to_string(socket) + " - '" + clientIP + "']!");

			string body{};
			string statusLine = "HTTP/1.1 200 OK";

			GetWhitelistedRoutes();

			Route foundRoute{};
			for (const auto& r : whitelistedRoutes)
			{
				if (r.filePath == filePath)
				{
					foundRoute = r;
					break;
				}
			}
			if (foundRoute.rootPath.empty())
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
				server->SocketCleanup(socket);
				return;
			}
			else
			{
				bool extentionExists = false;
				for (const auto& ext : whitelistedExtensions)
				{
					if (path(filePath).extension().generic_string() == ext)
					{
						extentionExists = true;
						break;
					}
				}

				bool isAllowedFile =
					filePath.find_last_of('.') == string::npos
					|| extentionExists;

				if (!isAllowedFile)
				{
					KalaServer::PrintConsoleMessage(
						2,
						true,
						ConsoleMessageType::Type_Warning,
						"CLIENT",
						"Client [" 
						+ to_string(socket) + " - '" + clientIP + "'] tried to access forbidden route '" + filePath 
						+ "' from path '" + foundRoute.filePath + "'.");

					auto resp403 = make_unique<Response_403>();
					resp403->Init(
						rawClientSocket,
						clientIP,
						filePath,
						"text/html");
					server->SocketCleanup(socket);
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
							server->SocketCleanup(socket);
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
							server->SocketCleanup(socket);
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
						server->SocketCleanup(socket);
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