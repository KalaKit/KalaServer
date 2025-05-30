//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <filesystem>
#include <string>
#include <Windows.h>
#include <fstream>
#include <chrono>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

#include "core.hpp"
#include "server.hpp"
#include "cloudflare.hpp"
#include "dns.hpp"

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
using std::cin;
using std::cout;

namespace KalaServer
{
	static HANDLE tunnelRunHandle{};
	
	bool CloudFlare::Initialize(
		bool shouldCloseCloudflaredAtShutdown,
		const string& newTunnelName,
		const string& newTunnelTokenFilePath)
	{
		if (Server::server == nullptr)
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Cannot initialize cloudflared if server has not yet been initialized!");
			return false;
		}

		if (isInitializing)
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Cannot initialize cloudflared while it is already being initialized!");
			return false;
		}
		
		if (!CloudflarePreInitializeCheck(
			newTunnelName,
			newTunnelTokenFilePath))
		{
			return false;
		}
		
		isInitializing = true;
		
		closeCloudflaredAtShutdown = shouldCloseCloudflaredAtShutdown;
		
		if (DNS::IsRunning())
		{
			Core::CreatePopup(
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
			Core::CreatePopup(
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
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"  [CLOUDFLARE_MESSAGE] Cloudflared cert file already exists at '" + certPath + "'. Skipping creation.");
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
				Core::PrintConsoleMessage(
					ConsoleMessageType::Type_Message,
					"  [CLOUDFLARE_MESSAGE] Cloudflared json file already exists at '" + tunnelIDFilePath + "'. Skipping creation.");
			}	
		}
		
		RunTunnel();

		isInitializing = false;
		isRunning = true;

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Cloudflared initialization completed.");

		return true;
	}
	
	bool CloudFlare::CloudflarePreInitializeCheck(
		const string& newTunnelName,
		const string& newTunnelTokenFilePath)
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();

		if (newTunnelName == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start cloudflared with empty tunnel name!");
			return false;
		}
		size_t tunnelNameLength = newTunnelName.length();
		if (tunnelNameLength < 3
			|| tunnelNameLength > 32)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel name length '" + to_string(tunnelNameLength) + "' is out of range! Must be between 3 and 32 characters.");
			return false;
		}
		tunnelName = newTunnelName;

		if (newTunnelTokenFilePath == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel token file path '" + newTunnelTokenFilePath + "' is empty!");
			return false;
		}
		if (!exists(newTunnelTokenFilePath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel token file path '" + newTunnelTokenFilePath + "' does not exist!");
			return false;
		}
		string tunnelTokenResult = 
			GetTextFileValue(
			newTunnelTokenFilePath,
			150, 
			300);
		if (tunnelTokenResult == "") return false;
		tunnelToken = tunnelTokenResult;
		tunnelTokenFilePath = newTunnelTokenFilePath;
		
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
			Core::CreatePopup(
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
			Core::CreatePopup(
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
			Core::CreatePopup(
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
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"  [CLOUDFLARE_MESSAGE] Cloudflared cert file already exists at '" + certPath + "'. Skipping creation.");
			return;
		}

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
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
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_COMMAND] " + command);

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
			Core::CreatePopup(
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

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Launched browser to authorize with CloudFlare. PID: " + to_string(pi.dwProcessId));

		CloseHandle(pi.hProcess);

		if (!exists(certPath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create tunnel cert file for tunnel '" + tunnelName + "' in '" + cloudFlareFolder + "'!");
		}
		
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_SUCCESS] Created new cloudflared cert file for tunnel '" + tunnelName + "'.");
	}

	void CloudFlare::CreateTunnelCredentials()
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		
		if (tunnelIDFilePath != ""
			&& path(tunnelIDFilePath).extension().string() == ".json")
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"  [CLOUDFLARE_MESSAGE] Cloudflared credentials json file for tunnel '" + tunnelName + "' already exists at '" + tunnelIDFilePath + "'. Skipping creation.");
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
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_COMMAND] " + command1);

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
			Core::CreatePopup(
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
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_COMMAND] " + command2);

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
			Core::CreatePopup(
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
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create tunnel credentials json file for tunnel '" + tunnelName + "' at '" + tunnelIDFilePath + "'!");
			return;
		}
		
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_SUCCESS] Created new cloudflared credentials json file for tunnel '" + tunnelName + "'.");
			
		Core::CreatePopup(
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
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_COMMAND] " + command1);

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
			Core::CreatePopup(
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
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_COMMAND] " + command2);

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
			Core::CreatePopup(
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
		
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_SUCCESS] Routed cloudflared dns for tunnel '" + tunnelName + "'.");
	}

	void CloudFlare::RunTunnel()
	{	
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();
		string configPath = path(path(cloudFlareFolder) / "config.yml").string();
	
		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

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
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_COMMAND] " + command);

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
			Core::CreatePopup(
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
		tunnelRunHandle = pi.hProcess;
		
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_SUCCESS] Running cloudflared tunnel.");
	}

	void CloudFlare::Quit()
	{
		if (!isRunning)
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Cannot shut down cloudflared because it hasn't been started!");
			return;
		}

		if (closeCloudflaredAtShutdown
			&& tunnelRunHandle)
		{
			TerminateProcess(tunnelRunHandle, 0);
			CloseHandle(tunnelRunHandle);
			tunnelRunHandle = nullptr;	
		}
		else
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"  [CLOUDFLARE_SUCCESS] Cloudflared was shut down.");
		}
		
		isRunning = false;

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Cloudflared was successfully shut down!");
	}
}