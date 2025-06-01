//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <filesystem>
#include <string>
#include <Windows.h>
#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>
#include <set>

#pragma comment(lib, "ws2_32.lib")

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::Server;
using KalaKit::Core::ConsoleMessageType;
using KalaKit::Core::PopupReason;

using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::directory_iterator;
using std::filesystem::file_time_type;
using std::string;
using std::wstring;
using std::to_string;
using std::ofstream;
using std::ifstream;
using std::stringstream;
using std::replace;
using std::istringstream;
using std::thread;
using std::set;

namespace KalaKit::DNS
{
	static string tunnelID{};
	static string tunnelIDFilePath{};

	bool CloudFlare::Initialize(
		bool shouldCloseCloudflaredAtShutdown,
		const string& newTunnelName)
	{
		if (Server::server == nullptr)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"CLOUDFLARE",
				"Cannot initialize cloudflared if server has not yet been initialized!");
			return false;
		}

		if (isInitializing)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"CLOUDFLARE",
				"[CLOUDFLARE] Cannot initialize cloudflared while it is already being initialized!");
			return false;
		}
		
		if (!CloudflarePreInitializeCheck(newTunnelName))
		{
			return false;
		}
		
		isInitializing = true;
		
		closeCloudflaredAtShutdown = shouldCloseCloudflaredAtShutdown;
		
		if (CustomDNS::IsRunning())
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Cannot run DNS and cloudflared together!");
			isInitializing = false;
			return false;
		}

		string cloudFlaredName = "cloudflared.exe";
		string cloudflaredPath = path(current_path() / cloudFlaredName).string();
		if (!exists(cloudflaredPath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to find cloudflared.exe!");
			isInitializing = false;
			return false;
		}
		
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();
		if (!exists(certPath)) CreateCert();
		else
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Message,
				"CLOUDFLARE",
				"Cloudflared cert file already exists at '" + certPath + "'. Skipping creation.");
		}

		if (tunnelID == "") CreateTunnelCredentials();
		else
		{
			string tunnelFile = tunnelID + ".json";
			tunnelIDFilePath = path(path(cloudFlareFolder) / tunnelFile).string();
			replace(tunnelIDFilePath.begin(), tunnelIDFilePath.end(), '\\', '/');
		
			if (!exists(tunnelIDFilePath)) CreateTunnelCredentials();
			else
			{
				KalaServer::PrintConsoleMessage(
					0,
					true,
					ConsoleMessageType::Type_Message,
					"CLOUDFLARE",
					"Cloudflared json file already exists at '" + tunnelIDFilePath + "'. Skipping creation.");
			}	
		}
		
		RunTunnel();

		isInitializing = false;
		isRunning = true;

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE",
			"Cloudflared initialization completed.");

		return true;
	}
	
	bool CloudFlare::CloudflarePreInitializeCheck(
		const string& newTunnelName)
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();

		if (newTunnelName == "")
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start cloudflared with empty tunnel name!");
			return false;
		}
		size_t tunnelNameLength = newTunnelName.length();
		if (tunnelNameLength < 3
			|| tunnelNameLength > 32)
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel name length '" + to_string(tunnelNameLength) + "' is out of range! Must be between 3 and 32 characters.");
			return false;
		}
		tunnelName = newTunnelName;
		
		if (tunnelIDFilePath != ""
			&& path(tunnelIDFilePath).extension().string() == ".json")
		{
			tunnelID = path(tunnelIDFilePath).stem().string();
		}
		if (tunnelID == "")
		{
			string newestTunnelID{};
			file_time_type newestTime{};
			for (const auto& file : directory_iterator(cloudFlareFolder))
			{
				path filePath = file.path();
				if (filePath.extension().string() == ".json")
				{
					auto lastWrite = last_write_time(file);
					if (lastWrite > newestTime)
					{
						newestTunnelID = filePath.stem().string();
						newestTime = lastWrite;
					}
				}
			}
			if (newestTunnelID != "") tunnelID = newestTunnelID;
		}
		
		return true;
	}

	string CloudFlare::GetTextFileValue(
		const string& textFilePath,
		int minLength,
		int maxLength)
	{
		string fileResult{};

		ifstream file(textFilePath);
		if (!file)
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to open text file '" + textFilePath + "'!");
			return "";
		}

		stringstream buffer{};
		buffer << file.rdbuf();
		fileResult = buffer.str();

		size_t textLength = fileResult.length();

		if (textLength < minLength
			|| textLength > maxLength)
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Text file '" + textFilePath + "' text length '" + to_string(textLength) + "' is too small or too big! You probably didn't copy the text correctly.");
			return "";
		}

		if (fileResult.find(" ") != string::npos)
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Text file '" + textFilePath + "' contains incorrect info! You probably didn't copy the text correctly.");
			return "";
		}

		return fileResult;
	}

	void CloudFlare::CreateCert()
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();

		if (exists(certPath))
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Message,
				"CLOUDFLARE",
				"Cloudflared cert file already exists at '" + certPath + "'. Skipping creation.");
			return;
		}

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE",
			"Creating new tunnel cert file at '" + certPath + "'.");

		string currentPath = current_path().string();
		wstring wParentFolderPath(currentPath.begin(), currentPath.end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		string command = "cloudflared tunnel login";
		KalaServer::PrintConsoleMessage(
			2,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE_COMMAND",
			command);

		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si,                       //pointer to STARTUPINFO structure
			&pi                        //pointer to PROCESS_INFORMATION structure
		))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create process for creating cert for tunnel '" + tunnelName + "'!");
			return;
		}

		//close thread handle and process
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE",
			"Launched browser to authorize with CloudFlare. PID: " + to_string(pi.dwProcessId));

		CloseHandle(pi.hProcess);

		if (!exists(certPath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create tunnel cert file for tunnel '" + tunnelName + "' in '" + cloudFlareFolder + "'!");
		}
		
		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE",
			"Sucessfully created new cloudflared cert file for tunnel '" + tunnelName + "'.");
	}

	void CloudFlare::CreateTunnelCredentials()
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		
		if (tunnelIDFilePath != ""
			&& path(tunnelIDFilePath).extension().string() == ".json")
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Message,
				"CLOUDFLARE",
				"Cloudflared credentials json file for tunnel '" + tunnelName + "' already exists at '" + tunnelIDFilePath + "'. Skipping creation.");
			return;
		}
		
		string currentPath = current_path().string();
		wstring wParentFolderPath(currentPath.begin(), currentPath.end());
		
		//
		// DELETE OLD TUNNEL
		//

		//initialize structures for process creation
		STARTUPINFOW si1;
		PROCESS_INFORMATION pi1;
		ZeroMemory(&si1, sizeof(si1));
		ZeroMemory(&pi1, sizeof(pi1));
		si1.cb = sizeof(si1);

		string command1 = "cloudflared tunnel delete " + tunnelName;
		KalaServer::PrintConsoleMessage(
			2,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE_COMMAND",
			command1);

		wstring wideCommand1(command1.begin(), command1.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand1.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si1,                      //pointer to STARTUPINFO structure
			&pi1                       //pointer to PROCESS_INFORMATION structure
		))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create process for deleting tunnel '" + tunnelName + "'!");
			return;
		}

		//close thread handle and process
		WaitForSingleObject(pi1.hProcess, INFINITE);
		CloseHandle(pi1.hThread);
		CloseHandle(pi1.hProcess);
		
		//
		// CREATE NEW TUNNEL
		//
		
		//initialize structures for process creation
		STARTUPINFOW si2;
		PROCESS_INFORMATION pi2;
		ZeroMemory(&si2, sizeof(si2));
		ZeroMemory(&pi2, sizeof(pi2));
		si2.cb = sizeof(si2);

		string command2 = "cloudflared tunnel create " + tunnelName;
		KalaServer::PrintConsoleMessage(
			2,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE_COMMAND",
			command2);

		wstring wideCommand2(command2.begin(), command2.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand2.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si2,                      //pointer to STARTUPINFO structure
			&pi2                       //pointer to PROCESS_INFORMATION structure
		))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create process for creating tunnel '" + tunnelName + "'!");
			return;
		}

		//close thread handle and process
		WaitForSingleObject(pi2.hProcess, INFINITE);
		CloseHandle(pi2.hThread);
		CloseHandle(pi2.hProcess);
		
		//
		// FINISH TUNNEL CREATION
		//
		
		if (tunnelIDFilePath != ""
			&& path(tunnelIDFilePath).extension().string() == ".json"
			&& !exists(tunnelIDFilePath))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create tunnel credentials json file for tunnel '" + tunnelName + "' at '" + tunnelIDFilePath + "'!");
			return;
		}
		
		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE",
			"Created new cloudflared credentials json file for tunnel '" + tunnelName + "'.");
			
		KalaServer::CreatePopup(
			PopupReason::Reason_Warning,
			"Before continuing to route DNS, you must manually insert the tunnel ID into config.yml.\n\n"
			"Please update the following fields:\n"
			"  tunnel: <insert tunnel ID here>\n"
			"  credentials-file: <insert full path to .json file>\n\n"
			"Press OK to continue once you've updated config.yml. If the tunnel ID is incorrect, DNS routing will fail and 'cloudflared tunnel run' will not work."
		);
			
		RouteTunnel();
	}

	void CloudFlare::RouteTunnel()
	{
		string currentPath = current_path().string();
		wstring wParentFolderPath(currentPath.begin(), currentPath.end());

		//
		// ROOT DOMAIN
		//

		//initialize structures for process creation
		STARTUPINFOW si1;
		PROCESS_INFORMATION p1;
		ZeroMemory(&si1, sizeof(si1));
		ZeroMemory(&p1, sizeof(p1));
		si1.cb = sizeof(si1);

		string command1 = "cloudflared tunnel route dns " + Server::server->GetDomainName() + " " + tunnelName;
		KalaServer::PrintConsoleMessage(
			2,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE_COMMAND",
			command1);

		wstring wideCommand1(command1.begin(), command1.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand1.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si1,                       //pointer to STARTUPINFO structure
			&p1                        //pointer to PROCESS_INFORMATION structure
		))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create process for routing dns for tunnel '" + tunnelName + "'!");
			return;
		}

		//close thread handle and process
		WaitForSingleObject(p1.hProcess, INFINITE);
		CloseHandle(p1.hThread);
		CloseHandle(p1.hProcess);

		//
		// SUBDOMAIN
		//

		//initialize structures for process creation
		STARTUPINFOW si2;
		PROCESS_INFORMATION pi2;
		ZeroMemory(&si2, sizeof(si2));
		ZeroMemory(&pi2, sizeof(pi2));
		si2.cb = sizeof(si2);

		string command2 = "cloudflared tunnel route dns www." + Server::server->GetDomainName() + " " + tunnelName;
		KalaServer::PrintConsoleMessage(
			2,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE_COMMAND",
			command2);

		wstring wideCommand2(command2.begin(), command2.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand2.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si2,                       //pointer to STARTUPINFO structure
			&pi2                        //pointer to PROCESS_INFORMATION structure
		))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create process for routing dns for tunnel '" + tunnelName + "'!");
			return;
		}

		//close thread handle and process
		WaitForSingleObject(pi2.hProcess, INFINITE);
		CloseHandle(pi2.hThread);
		CloseHandle(pi2.hProcess);
		
		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE",
			"Successfully routed cloudflared dns for tunnel '" + tunnelName + "'.");
	}

	void CloudFlare::RunTunnel()
	{	
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();
		string configPath = path(path(cloudFlareFolder) / "config.yml").string();
	
		HANDLE hReadPipe{};
		HANDLE hWritePipe{};
		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = nullptr;

		if (!CreatePipe(
			&hReadPipe,
			&hWritePipe,
			&sa,
			0))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create read/write pipe for tunnel '" + tunnelName + "'!");
			return;
		}
		if (!SetHandleInformation(
			hReadPipe,
			HANDLE_FLAG_INHERIT,
			0))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to set up pipe handle inheritance for tunnel '" + tunnelName + "'!");
			return;
		}

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags |= STARTF_USESTDHANDLES;
		si.hStdOutput = hWritePipe;
		si.hStdError = hWritePipe;

		string currentPath = current_path().string();
		wstring wParentFolderPath(currentPath.begin(), currentPath.end());

#ifdef _DEBUG
		string command = 
			"cloudflared --origin-ca-pool " + certPath 
			+ " --config " + configPath 
			+ " --loglevel debug" 
			+ " tunnel run " + tunnelName;
#else
		string command =
			"cloudflared --origin-ca-pool " + certPath
			+ " --config " + configPath
			+ " tunnel run " + tunnelName;
#endif

		KalaServer::PrintConsoleMessage(
			2,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE_COMMAND",
			command);

		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			TRUE,                      //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si,                       //pointer to STARTUPINFO structure
			&pi                        //pointer to PROCESS_INFORMATION structure
		))
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create process for running tunnel '" + tunnelName + "'!");
			return;
		}

		//close thread handle and process
		CloseHandle(pi.hThread);
		CloseHandle(hWritePipe);
		tunnelRunHandle = reinterpret_cast<uintptr_t>(pi.hProcess);
		
		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE",
			"Running cloudflared tunnel.");

		void* convertedHandle = reinterpret_cast<void*>(hReadPipe);
		PipeCloudflareMessages(convertedHandle);
	}

	void CloudFlare::PipeCloudflareMessages(void* handle)
	{
		HANDLE hReadPipe = reinterpret_cast<HANDLE>(handle);

		thread([hReadPipe]
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Message,
				"CLOUDFLARE",
				"Piping cloudflare messages to internal console.");

			char buffer[2048]{};
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
				istringstream stream(buffer);
				string line{};
				while (getline(stream, line))
				{
					if (line.find("Registered tunnel connection connIndex=") != string::npos)
					{
						size_t pos = line.find("connIndex=") + strlen("connIndex=");
						int index = stoi(line.substr(pos));

						if (index == 0) isConn0Healthy = true;
						else if (index == 1) isConn1Healthy = true;
						else if (index == 2) isConn2Healthy = true;
						else if (index == 3) isConn3Healthy = true;

						if (Server::server->IsServerReady())
						{
							KalaServer::PrintConsoleMessage(
								0,
								true,
								ConsoleMessageType::Type_Message,
								"CLOUDFLARE",
								"Connection '" + to_string(index) + "' has been marked healthy!");
						}

						if (isConn0Healthy
							&& isConn1Healthy
							&& isConn2Healthy
							&& isConn3Healthy
							&& !Server::server->IsServerReady())
						{
							Server::server->SetServerReadyState(true);
							Server::server->Start();
						}
					}

					if (line.find("Unregistered tunnel connection connIndex=") != string::npos)
					{
						size_t pos = line.find("connIndex=") + strlen("connIndex=");
						int index = stoi(line.substr(pos));

						if (index == 0) isConn0Healthy = false;
						else if (index == 1) isConn1Healthy = false;
						else if (index == 2) isConn2Healthy = false;
						else if (index == 3) isConn3Healthy = false;

						KalaServer::PrintConsoleMessage(
							0,
							true,
							ConsoleMessageType::Type_Warning,
							"CLOUDFLARE",
							"Connection '" + to_string(index) + "' has been marked unhealthy."
							"Cloudflare could not maintain a tunnel through it.");
					}

					//strip cloudflare timestamp

					size_t prefixEnd = line.find(' ', 24);
					if (line.size() < 28)
					{
						KalaServer::PrintConsoleMessage(
							2,
							true,
							ConsoleMessageType::Type_Message,
							"CLOUDFLARE_LOG",
							line);
						continue;
					}

					//extract message type

					string typeStr = line.substr(21, 3);
					ConsoleMessageType type = ConsoleMessageType::Type_Message;
					if (typeStr == "ERR") type = ConsoleMessageType::Type_Error;
					else if (typeStr == "WRN") type = ConsoleMessageType::Type_Warning;
					else if (typeStr == "DBG") type = ConsoleMessageType::Type_Debug;

					//extract rest of message after message type

					string message = line.substr(25);

					KalaServer::PrintConsoleMessage(
						2,
						true,
						type,
						"CLOUDFLARE_LOG",
						message);
				}
			}
			CloseHandle(hReadPipe);
		}).detach();
	}

	void CloudFlare::Quit()
	{
		if (!isRunning)
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Error,
				"CLOUDFLARE",
				"Cannot shut down cloudflared because it hasn't been started!");
			return;
		}

		HANDLE rawTunnelRunHandle = reinterpret_cast<HANDLE>(tunnelRunHandle);

		if (rawTunnelRunHandle
			&& rawTunnelRunHandle != INVALID_HANDLE_VALUE)

		{
			if (closeCloudflaredAtShutdown
				&& tunnelRunHandle)
			{
				TerminateProcess(rawTunnelRunHandle, 0);
				CloseHandle(rawTunnelRunHandle);
				tunnelRunHandle = NULL;
			}
		}
		else
		{
			KalaServer::PrintConsoleMessage(
				0,
				true,
				ConsoleMessageType::Type_Message,
				"CLOUDFLARE",
				"Cloudflared was shut down.");
		}
		
		isRunning = false;

		KalaServer::PrintConsoleMessage(
			0,
			true,
			ConsoleMessageType::Type_Message,
			"CLOUDFLARE",
			"Cloudflared was successfully shut down!");
	}
}