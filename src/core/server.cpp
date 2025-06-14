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
#include <algorithm>

#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "ws2_32.lib")

#include "core/core.hpp"
#include "core/server.hpp"
#include "core/client.hpp"
#include "core/event.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaKit::DNS::CloudFlare;
using KalaKit::DNS::CustomDNS;

using std::unordered_map;
using std::exit;
using std::to_string;
using std::ifstream;
using std::stringstream;
using std::ostringstream;
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
using std::streamsize;
using std::streamoff;
using std::min;
using std::find;

namespace KalaKit::Core
{	
	//Routes only registered users can access
	vector<string> registeredRoutes{};
	//Routes only admins and host can access
	vector<string> adminRoutes{};

	atomic<bool> canUpdateWhitelistedIPs{ true };
	//All IPs that will always bypass ban filter
	vector<pair<string, string>> whitelistedIPs{};

	atomic<bool> canUpdateBannedIPs{ true };
	//All IPs banned permanently from server unless manually removed
	vector<pair<string, string>> bannedIPs{};

	atomic<bool> canUpdateMachineIPs{ true };
	//All host IPs to check which one host currently uses
	//to confirm if IP really belongs to host or not
	vector<string> machineIPs{};

	bool Server::Initialize(
		unsigned int port,
		unsigned int healthTimer,
		unsigned int rateLimitTimer,
		const string& serverName,
		const string& domainName,
		const ErrorMessage& errorMessage,
		const DataFile& dataFile,
		const EmailSenderData& emailSenderData,
		const vector<string>& submittedRegisteredRoutes,
		const vector<string>& submittedAdminRoutes)
	{
		server = make_unique<Server>(
			port,
			healthTimer,
			rateLimitTimer,
			serverName,
			domainName,
			errorMessage,
			dataFile,
			emailSenderData);

		if (!server->PreInitializeCheck()) return false;

		PrintData iData =
		{
			.indentationLength = 0,
			.addTimeStamp = false,
			.customTag = "",
			.message = 
				"=============================="
				"\n"
				"Initializing server '" + server->GetServerName() + "'"
				"\n"
				"=============================="
				"\n"
		};
		unique_ptr<Event> iEvent = make_unique<Event>();
		iEvent->SendEvent(EventType::event_print_message, iData);

		server->GetFileData(DataFileType::datafile_extension);
		server->GetWhitelistedRoutes();
		server->GetFileData(DataFileType::datafile_whitelistedIP);
		server->GetFileData(DataFileType::datafile_bannedIP);
		server->GetFileData(DataFileType::datafile_blacklistedKeyword);

		registeredRoutes = submittedRegisteredRoutes;
		adminRoutes = submittedAdminRoutes;
		server->SetRouteAccessLevels();
		
		WSADATA wsaData{};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			unique_ptr<Event> wfEvent = make_unique<Event>();
			wfEvent->SendEvent(EventType::event_popup_error, "WSAStartup failed!");
			return false;
		}

		SOCKET thisSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		server->serverSocket = static_cast<uintptr_t>(thisSocket);

		if (thisSocket == INVALID_SOCKET)
		{
			unique_ptr<Event> scfEvent = make_unique<Event>();
			scfEvent->SendEvent(EventType::event_popup_error, "Socket creation failed!");
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
			unique_ptr<Event> bfEvent = make_unique<Event>();
			bfEvent->SendEvent(EventType::event_popup_error, "Bind failed!");
			return false;
		}

		if (listen(thisSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			unique_ptr<Event> lfEvent = make_unique<Event>();
			lfEvent->SendEvent(EventType::event_popup_error, "Listen failed!");
			return false;
		}

		unique_ptr<Event> emptyEvent = make_unique<Event>();
		emptyEvent->PrintNewLine();

		PrintData pData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.customTag = "SERVER",
			.message = "Server is running on port '" + to_string(server->port) + "'."
		};
		unique_ptr<Event> pEvent = make_unique<Event>();
		pEvent->SendEvent(EventType::event_print_message, pData);

		KalaServer::isRunning = true;

		PrintData ifData =
		{
			.indentationLength = 0,
			.addTimeStamp = false,
			.customTag = "",
			.message = 
				"\n"
				"=============================="
				"\n"
				"Initialization finished!"
				"\n"
				"=============================="
				"\n"
		};
		unique_ptr<Event> ifEvent = make_unique<Event>();
		ifEvent->SendEvent(EventType::event_print_message, ifData);

		return true;
	}

	bool Server::PreInitializeCheck() const
	{
		if (!KalaServer::IsRunningAsAdmin())
		{
			unique_ptr<Event> apEvent = make_unique<Event>();
			apEvent->SendEvent(EventType::event_popup_error, "This program must be ran with admin privileges!");
			return false;
		}

		if (serverName == "")
		{
			unique_ptr<Event> esnEvent = make_unique<Event>();
			esnEvent->SendEvent(EventType::event_popup_error, "Cannot start server with empty server name!");
			return false;
		}
		if (domainName == "")
		{
			unique_ptr<Event> ednEvent = make_unique<Event>();
			ednEvent->SendEvent(EventType::event_popup_error, "Cannot start server with empty domain name!");
			return false;
		}

		string whitelistedRoutesFolderPath = (current_path() / server->dataFile.whitelistedRoutesFolder).string();
		if (!exists(whitelistedRoutesFolderPath))
		{
			string wrMessage = "Cannot start server with invalid whitelisted routes folder '" + whitelistedRoutesFolderPath + "'!";
			unique_ptr<Event> wrEvent = make_unique<Event>();
			wrEvent->SendEvent(EventType::event_popup_error, wrMessage);
			return false;
		}

		string whitelistedExtensionsFilePath = (
			current_path() / server->dataFile.whitelistedExtensionsFile).string();
		if (!exists(whitelistedExtensionsFilePath))
		{
			string weMessage = "Cannot start server with invalid whitelisted extensions file '" + whitelistedExtensionsFilePath + "'!";
			unique_ptr<Event> weEvent = make_unique<Event>();
			weEvent->SendEvent(EventType::event_popup_error, weMessage);
			return false;
		}

		string whitelistedIPsFilePath = (
			current_path() / server->dataFile.whitelistedIPsFile).string();
		if (!exists(whitelistedIPsFilePath))
		{
			string wiMessage = "Cannot start server with invalid whitelisted IPs file '" + whitelistedIPsFilePath + "'!";
			unique_ptr<Event> wiEvent = make_unique<Event>();
			wiEvent->SendEvent(EventType::event_popup_error, wiMessage);
			return false;
		}

		string bannedIPsFilePath = (
			current_path() / server->dataFile.bannedIPsFile).string();
		if (!exists(bannedIPsFilePath))
		{
			string biMessage = "Cannot start server with invalid banned IPs file '" + bannedIPsFilePath + "'!";
			unique_ptr<Event> biEvent = make_unique<Event>();
			biEvent->SendEvent(EventType::event_popup_error, biMessage);
			return false;
		}

		string blacklistedKeywordsFilePath = (
			current_path() / server->dataFile.blacklistedKeywordsFile).string();
		if (!exists(blacklistedKeywordsFilePath))
		{
			string bkMessage = "Cannot start server with invalid blacklisted keywords file '" + blacklistedKeywordsFilePath + "'!";
			unique_ptr<Event> bkEvent = make_unique<Event>();
			bkEvent->SendEvent(EventType::event_popup_error, bkMessage);
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
			if (bannedClient.first == targetIP
				|| targetIP.rfind(bannedClient.first, 0) == 0)
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
			PrintData biData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Failed to read 'banned-ips.txt' to ban IP!"
			};
			unique_ptr<Event> biEvent = make_unique<Event>();
			biEvent->SendEvent(EventType::event_print_error, biData);
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
			PrintData biData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Failed to write into 'banned-ips.txt' to ban IP!"
			};
			unique_ptr<Event> biEvent = make_unique<Event>();
			biEvent->SendEvent(EventType::event_print_error, biData);
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
			string rrfMessage = "Whitelisted routes root folder has not been assigned!";
			unique_ptr<Event> rrfEvent = make_unique<Event>();
			rrfEvent->SendEvent(EventType::event_popup_error, rrfMessage);
			return;
		}

		path routePath = current_path() / routeFolder;
		if (!exists(routePath))
		{
			string wrfMessage = "Whitelisted routes root folder '" + server->dataFile.whitelistedRoutesFolder + "' does not exist!";
			unique_ptr<Event> wrfEvent = make_unique<Event>();
			wrfEvent->SendEvent(EventType::event_popup_error, wrfMessage);
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
			string correctRoute{};
			if (path(route).extension() == ".html")
			{
				path relativePath = relative(route.path(), server->dataFile.whitelistedRoutesFolder);
				relativePath.replace_extension("");
				correctRoute = "/" + relativePath.generic_string();

				//fix index.html route
				if (correctRoute == "/index") correctRoute = "/";
			}
			else
			{
				path relativePath = relative(route.path(), server->dataFile.whitelistedRoutesFolder);
				correctRoute = "/" + relativePath.generic_string();
			}

			//use actual route directly
			string correctFilePath = route.path().generic_string();

			server->AddNewWhitelistedRoute(correctRoute, correctFilePath);
		}

		PrintData rwrData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.customTag = "SERVER",
			.message = "Refreshed whitelisted routes"
		};
		unique_ptr<Event> rwrEvent = make_unique<Event>();
		rwrEvent->SendEvent(EventType::event_print_message, rwrData);

		server->canUpdateWhitelistedRoutes = false;
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
			server->whitelistedExtensions.clear();
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
			server->blacklistedKeywords.clear();
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
			PrintData ffData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Failed to open '" + path(filePath).filename().string() + "'!"
			};
			unique_ptr<Event> ffEvent = make_unique<Event>();
			ffEvent->SendEvent(EventType::event_print_message, ffData);
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
				server->whitelistedExtensions.push_back(resultValue.first);
			}
			break;
		case DataFileType::datafile_whitelistedIP:
			for (const pair<string, string> resultValue : result)
			{
				whitelistedIPs.push_back(resultValue);
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
				server->blacklistedKeywords.push_back(resultValue.first);
			}
			break;
		}

		PrintData rtData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.customTag = "SERVER",
			.message = "Refreshed " + resultType
		};
		unique_ptr<Event> rtEvent = make_unique<Event>();
		rtEvent->SendEvent(EventType::event_print_message, rtData);
	}

	void Server::AddNewWhitelistedRoute(const string& route, const string& filePath) const
	{
		for (const auto& r : whitelistedRoutes)
		{
			if (r.route == route) return;
		}

		path fullPath = path(filePath);
		if (!exists(fullPath))
		{
			PrintData ppData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Page path '" + filePath + "' does not exist!."
			};
			unique_ptr<Event> ppEvent = make_unique<Event>();
			ppEvent->SendEvent(EventType::event_print_error, ppData);

			return;
		}

		string s_mimeType = path(filePath).extension().string();
		string_view sv_mimeType(s_mimeType);

		string_view sv_newMimeType = GetMimeType(sv_mimeType);
		string s_newMimeType(sv_newMimeType);

		Route newRoute =
		{
			.route = route,
			.filePath = filePath,
			.mimeType = s_newMimeType
		};

		server->whitelistedRoutes.push_back(newRoute);

		PrintData arData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.customTag = "SERVER",
			.message = "\n  Added route '" + route + "'\n  file path   '" + filePath + "'\n  mime type   '" + s_newMimeType + "'!"
		};
		unique_ptr<Event> arEvent = make_unique<Event>();
		arEvent->SendEvent(EventType::event_print_message, arData);
	}
	void Server::AddNewWhitelistedExtension(const string& newExtension) const
	{
		for (const auto& extension : whitelistedExtensions)
		{
			if (extension == newExtension) return;
		}

		server->whitelistedExtensions.push_back(newExtension);
		PrintData neData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.customTag = "SERVER",
			.message = "Added new extension '" + newExtension + "'"
		};
		unique_ptr<Event> neEvent = make_unique<Event>();
		neEvent->SendEvent(EventType::event_print_message, neData);
	}

	void Server::RemoveWhitelistedRoute(const string& thisRoute) const
	{
		GetWhitelistedRoutes();

		string foundRoute{};
		for (const auto& r : whitelistedRoutes)
		{
			if (r.route == thisRoute)
			{
				foundRoute = thisRoute;
				break;
			}
		}

		if (foundRoute == "")
		{
			PrintData rrData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Route '" + thisRoute + "' cannot be removed because it hasn't been whitelisted!"
			};
			unique_ptr<Event> rrEvent = make_unique<Event>();
			rrEvent->SendEvent(EventType::event_print_warning, rrData);
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
			PrintData erData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Extension '" + thisExtension + "' cannot be removed because it hasn't been whitelisted!"
			};
			unique_ptr<Event> erEvent = make_unique<Event>();
			erEvent->SendEvent(EventType::event_print_warning, erData);
		}
	}

	vector<char> Server::ServeFile(
		const string& route,
		size_t rangeStart,
		size_t rangeEnd,
		size_t& outTotalSize,
		bool& outSliced)
	{
		outSliced = false;
		outTotalSize = 0;

		if (route.empty())
		{
			PrintData seData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVE-FILE",
				.message = "Serve received empty route!"
			};
			unique_ptr<Event> seEvent = make_unique<Event>();
			seEvent->SendEvent(EventType::event_print_error, seData);
			return {};
		}

		server->GetWhitelistedRoutes();

		Route foundRoute{};
		for (const auto& r : whitelistedRoutes)
		{
			if (r.route == route)
			{
				foundRoute = r;
				break;
			}
		}
		if (foundRoute.route.empty())
		{
			PrintData rwData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVE-FILE",
				.message = "Route '" + route + "' is not whitelisted!"
			};
			unique_ptr<Event> rwEvent = make_unique<Event>();
			rwEvent->SendEvent(EventType::event_print_error, rwData);
			return {};
		}

		try
		{
			path fullFilePath = foundRoute.filePath;
			ifstream file(fullFilePath, ios::binary);
			if (!file)
			{
				PrintData ofData =
				{
					.indentationLength = 0,
					.addTimeStamp = true,
					.customTag = "SERVE-FILE",
					.message = "Failed to open file '" + fullFilePath.generic_string() + "'!"
				};
				unique_ptr<Event> ofEvent = make_unique<Event>();
				ofEvent->SendEvent(EventType::event_print_error, ofData);
				return {};
			}

			//get size
			file.seekg(0, ios::end);
			streamsize fileSize = file.tellg();
			file.seekg(0, ios::beg);
			outTotalSize = static_cast<size_t>(fileSize);

			if (rangeStart >= outTotalSize)
			{
				PrintData rsData =
				{
					.indentationLength = 0,
					.addTimeStamp = true,
					.customTag = "SERVE-FILE",
					.message = 
						"Range start (" + to_string(rangeStart) +
						") is beyond total size (" + to_string(outTotalSize) + ")!"
				};
				unique_ptr<Event> rsEvent = make_unique<Event>();
				rsEvent->SendEvent(EventType::event_print_error, rsData);
				return {};
			}

			//extract extension
			string ext = fullFilePath.extension().string();
			transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

			const unordered_set<string> alwaysFull
			{
				".html", ".css", ".js", ".txt"
			};

			bool hasExtension = !ext.empty();
			bool isAlwaysFull =
				!hasExtension
				|| alwaysFull.count(ext) > 0;
			bool isLarge = fileSize > static_cast<long long>(10 * 1024) * 1024; //> 10MB

			if (!isAlwaysFull
				&& isLarge)
			{
				if (rangeEnd == 0
					|| rangeEnd >= outTotalSize)
				{
					rangeEnd = outTotalSize - 1;
				}

				size_t sliceSize = min(rangeEnd - rangeStart + 1, outTotalSize - rangeStart);
				file.seekg(static_cast<streamoff>(rangeStart), ios::beg);

				vector<char> buffer(sliceSize);
				file.read(buffer.data(), static_cast<streamsize>(sliceSize));
				outSliced = true;
				return buffer;
			}
			else
			{
				vector<char> buffer(static_cast<size_t>(fileSize));
				file.read(buffer.data(), fileSize);
				return buffer;
			}
		}
		catch (const exception& e)
		{
			PrintData erData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVE-FILE",
				.message = "Exception while serving route '" + route + "':\n" + e.what()
			};
			unique_ptr<Event> erEvent = make_unique<Event>();
			erEvent->SendEvent(EventType::event_print_error, erData);
			return {};
		}

		return {};
	}

	bool Server::HasInternet()
	{
		if (CloudFlare::tunnelName == "")
		{
			PrintData iaData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Cannot check for internet access because tunnel name has not been assigned."
			};
			unique_ptr<Event> iaEvent = make_unique<Event>();
			iaEvent->SendEvent(EventType::event_print_error, iaData);
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
			PrintData tsData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Cannot check for tunnel state because tunnel '" + CloudFlare::tunnelName + "' is not running."
			};
			unique_ptr<Event> tsEvent = make_unique<Event>();
			tsEvent->SendEvent(EventType::event_print_error, tsData);
			return false;
		}

		HANDLE rawTunnelHandle = reinterpret_cast<HANDLE>(tunnelHandle);

		if (rawTunnelHandle == INVALID_HANDLE_VALUE)
		{
			PrintData tsData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Cannot check for tunnel state because handle for tunnel '" + CloudFlare::tunnelName + "' is invalid."
			};
			unique_ptr<Event> tsEvent = make_unique<Event>();
			tsEvent->SendEvent(EventType::event_print_error, tsData);
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
			PrintData srData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Server '" + server->serverName + "' is not ready to start! Do not call this manually."
			};
			unique_ptr<Event> srEvent = make_unique<Event>();
			srEvent->SendEvent(EventType::event_print_error, srData);
			return;
		}

		if (!CloudFlare::IsRunning()
			&& !CustomDNS::IsRunning())
		{
			PrintData ncData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Neither cloudflared or dns was started! Please run atleast one of them."
			};
			unique_ptr<Event> ncEvent = make_unique<Event>();
			ncEvent->SendEvent(EventType::event_print_error, ncData);
			return;
		}

		thread([]
		{
			PrintData acData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "",
				.message = 
				"\n"
				"=============================="
				"\n"
				"Ready to accept connections!"
				"\n"
				"=============================="
				"\n"
			};
			unique_ptr<Event> acEvent = make_unique<Event>();
			acEvent->SendEvent(EventType::event_print_error, acData);

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
					PrintData afData =
					{
						.indentationLength = 0,
						.addTimeStamp = true,
						.customTag = "SERVER",
						.message = "Accept failed: " + to_string(WSAGetLastError())
					};
					unique_ptr<Event> afEvent = make_unique<Event>();
					afEvent->SendEvent(EventType::event_print_error, afData);
					continue;
				}
				else
				{
					thread([clientSocket]
					{
						unique_ptr<Client> client = make_unique<Client>();
						client->HandleClient(static_cast<uintptr_t>(clientSocket));
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

					PrintData fsData =
					{
						.indentationLength = 0,
						.addTimeStamp = true,
						.customTag = "SERVER",
						.message = fullStatus + "\n"
					};
					unique_ptr<Event> fsEvent = make_unique<Event>();
					fsEvent->SendEvent(EventType::event_print_error, fsData);

					vector<string> receivers = { server->emailSenderData.username };
					server->emailData =
					{
						.smtpServer = "smtp.gmail.com",
						.username = server->emailSenderData.username,
						.password = server->emailSenderData.password,
						.sender = server->emailSenderData.username,
						.receivers = receivers,
						.subject = "KalaServer health status",
						.body = fullStatus
					};

					unique_ptr<Event> event = make_unique<Event>();
					event->SendEvent(EventType::event_server_health_ping, server->emailData);

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
				if (!canUpdateWhitelistedIPs) canUpdateWhitelistedIPs = true;

				sleep_for(seconds(60));
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
				if (!server->canUpdateWhitelistedRoutes) server->canUpdateWhitelistedRoutes = true;

				sleep_for(seconds(60));
			}
		}).detach();

		thread([]
		{
			while (KalaServer::isRunning)
			{
				if (!server->canUpdateRouteAccess) server->canUpdateRouteAccess = true;

				sleep_for(seconds(60));
			}
		}).detach();

		thread([] 
		{
			while (KalaServer::isRunning)
			{
				sleep_for(seconds(1));
				lock_guard<mutex> lock(counterMutex);
				requestCounter.clear();
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
			PrintData fcData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Failed to create read/write pipe for storing host IPs!"
			};
			unique_ptr<Event> fcEvent = make_unique<Event>();
			fcEvent->SendEvent(EventType::event_print_error, fcData);
			return;
		}
		if (!SetHandleInformation(
			hReadPipe,
			HANDLE_FLAG_INHERIT,
			0))
		{
			PrintData phData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Failed to set up pipe handle inheritance for storing host IPs!"
			};
			unique_ptr<Event> phEvent = make_unique<Event>();
			phEvent->SendEvent(EventType::event_print_error, phData);
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
			PrintData cpData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Failed to create process for getting host IPs for storing host IPs!"
			};
			unique_ptr<Event> cpEvent = make_unique<Event>();
			cpEvent->SendEvent(EventType::event_print_error, cpData);

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

		PrintData miData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.customTag = "SERVER",
			.message = "Refreshed machine IPs"
		};
		unique_ptr<Event> miEvent = make_unique<Event>();
		miEvent->SendEvent(EventType::event_print_error, miData);

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

	pair<string, string> Server::IsWhitelistedClient(const string& targetIP) const
	{
		pair<string, string> whitelistedReason{};

		if (whitelistedIPs.size() == 0)
		{
			canUpdateWhitelistedIPs = true;
			server->GetWhitelistedIPs();
		}

		if (canUpdateWhitelistedIPs) server->GetWhitelistedIPs();

		for (const pair<string, string>& whitelistedClient : whitelistedIPs)
		{
			if (whitelistedClient.first == targetIP
				|| targetIP.rfind(whitelistedClient.first, 0) == 0)
			{
				whitelistedReason.first = whitelistedClient.first;
				whitelistedReason.second = whitelistedClient.second;
			}
		}

		return whitelistedReason;
	}

	void Server::GetWhitelistedIPs()
	{
		if (!canUpdateWhitelistedIPs) return;

		server->GetFileData(DataFileType::datafile_whitelistedIP);

		canUpdateWhitelistedIPs = false;
	}

	bool Server::IsRegisteredRoute(const string& route)
	{
		if (canUpdateRouteAccess)
		{
			server->SetRouteAccessLevels();
			canUpdateRouteAccess = false;
		}

		for (const auto& registeredRoute : registeredRoutes)
		{
			if (route == registeredRoute) return true;
		}

		return false;
	}

	bool Server::IsAdminRoute(const string& route)
	{
		if (canUpdateRouteAccess)
		{
			server->SetRouteAccessLevels();
			canUpdateRouteAccess = false;
		}

		for (const auto& adminRoute : adminRoutes)
		{
			if (route == adminRoute) return true;
		}

		return false;
	}

	void Server::SetRouteAccessLevels()
	{
		if (!canUpdateRouteAccess) return;

		vector<string> currentRegisteredRoutes = registeredRoutes;
		vector<string> currentAdminRoutes = adminRoutes;

		//remove invalid registered routes
		if (!currentRegisteredRoutes.empty())
		{
			vector<string> cleanedRegisteredRoutes{};
			vector<string> invalidRegisteredRoutes{};
			for (const auto& registeredRoute : currentRegisteredRoutes)
			{
				bool validRoute = false;
				for (const auto& r : whitelistedRoutes)
				{
					if (r.route == registeredRoute)
					{
						validRoute = true;
						break;
					}
				}

				if (!validRoute)
				{
					invalidRegisteredRoutes.push_back(registeredRoute);
				}
				else
				{
					cleanedRegisteredRoutes.push_back(registeredRoute);
				}
			}
			if (!invalidRegisteredRoutes.empty())
			{
				for (const auto& invalidRegisteredRoute : invalidRegisteredRoutes)
				{
					PrintData rrData =
					{
						.indentationLength = 0,
						.addTimeStamp = true,
						.customTag = "SERVER",
						.message = "Registered route '" + invalidRegisteredRoute + "' is not whitelisted! Cannot add to allowed registered routes..."
					};
					unique_ptr<Event> rrEvent = make_unique<Event>();
					rrEvent->SendEvent(EventType::event_print_error, rrData);
				}
			}

			registeredRoutes = cleanedRegisteredRoutes;
		}

		//remove invalid admin routes
		if (!currentAdminRoutes.empty())
		{
			vector<string> cleanedAdminRoutes{};
			vector<string> invalidAdminRoutes{};
			for (const auto& adminRoute : currentAdminRoutes)
			{
				bool validRoute = false;
				for (const auto& r : whitelistedRoutes)
				{
					if (r.route == adminRoute)
					{
						validRoute = true;
						break;
					}
				}

				if (!validRoute)
				{
					invalidAdminRoutes.push_back(adminRoute);
				}
				else
				{
					cleanedAdminRoutes.push_back(adminRoute);
				}
			}
			if (!invalidAdminRoutes.empty())
			{
				for (const auto& invalidAdminRoute : invalidAdminRoutes)
				{
					PrintData arData =
					{
						.indentationLength = 0,
						.addTimeStamp = true,
						.customTag = "SERVER",
						.message = "Admin route '" + invalidAdminRoute + "' is not whitelisted! Cannot add to allowed admin routes..."
					};
					unique_ptr<Event> arEvent = make_unique<Event>();
					arEvent->SendEvent(EventType::event_print_error, arData);
				}
			}

			adminRoutes = cleanedAdminRoutes;
		}

		//confirm admin routes dont have same routes as registered routes
		if (!adminRoutes.empty())
		{
			vector<string> finalAdminRoutes{};
			for (auto& adminRoute : adminRoutes)
			{
				if (find(
					registeredRoutes.begin(),
					registeredRoutes.end(),
					adminRoute)
					== registeredRoutes.end())
				{
					finalAdminRoutes.push_back(adminRoute);
				}
			}
			adminRoutes = finalAdminRoutes;
		}

		//assign route access levels
		for (auto& r : whitelistedRoutes)
		{
			r.accessLevel = AccessLevel::access_user;

			for (const auto& registeredRoute : registeredRoutes)
			{
				if (r.route == registeredRoute)
				{
					r.accessLevel = AccessLevel::access_registered;

					PrintData srData =
					{
						.indentationLength = 0,
						.addTimeStamp = true,
						.customTag = "SERVER",
						.message = "Set route '" + r.route + "' access level to 'registered'"
					};
					unique_ptr<Event> srEvent = make_unique<Event>();
					srEvent->SendEvent(EventType::event_print_error, srData);

					break;
				}
			}

			for (const auto& adminRoute : adminRoutes)
			{
				if (r.route == adminRoute)
				{
					r.accessLevel = AccessLevel::access_admin;

					PrintData srData =
					{
						.indentationLength = 0,
						.addTimeStamp = true,
						.customTag = "SERVER",
						.message = "Set route '" + r.route + "' access level to 'admin'"
					};
					unique_ptr<Event> srEvent = make_unique<Event>();
					srEvent->SendEvent(EventType::event_print_error, srData);

					break;
				}
			}
		}

		PrintData raData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.customTag = "SERVER",
			.message = "Refreshed route access levels"
		};
		unique_ptr<Event> raEvent = make_unique<Event>();
		raEvent->SendEvent(EventType::event_print_error, raData);
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