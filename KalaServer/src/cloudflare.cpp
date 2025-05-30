//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <filesystem>
#include <string>
#include <Windows.h>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

#include "core.hpp"
#include "server.hpp"
#include "cloudflare.hpp"
#include "dns.hpp"

using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::path;
using std::string;
using std::wstring;
using std::to_string;
using std::ofstream;
using std::ifstream;
using std::stringstream;
using std::replace;

namespace KalaServer
{
	bool CloudFlare::Initialize(
		bool shouldCloseCloudflaredAtShutdown,
		const string& newTunnelName,
		const string& newTunnelTokenFilePath,
		const string& newTunnelIDFilePath,
		const string& newAccountTagFilePath)
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
			newTunnelTokenFilePath,
			newTunnelIDFilePath,
			newAccountTagFilePath))
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

		if (!exists(tunnelTokenFilePath)) CreateTunnelCredentials();
		
		if (!TunnelExists()) InstallTunnel();
		
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
		const string& newTunnelTokenFilePath,
		const string& newTunnelIDFilePath,
		const string& newAccountTagFilePath)
	{
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
		
		if (newTunnelIDFilePath == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel id file path is empty!");
			return false;
		}
		if (!exists(newTunnelIDFilePath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel ID file path '" + newTunnelIDFilePath + "' does not exist!");
			return false;
		}
		string tunnelIDResult = 
			GetTextFileValue(
			newTunnelIDFilePath,
			36, 
			36);
		if (tunnelIDResult == "") return false;
		tunnelID = tunnelIDResult;
		tunnelIDFilePath = newTunnelIDFilePath;
		
		if (newAccountTagFilePath == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Account tag file path is empty!");
			return false;
		}
		if (!exists(newAccountTagFilePath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Account tag file path '" + newAccountTagFilePath + "' does not exist!");
			return false;
		}
		string accountTagResult = 
			GetTextFileValue(
			newAccountTagFilePath,
			32, 
			32);
		if (accountTagResult == "") return false;
		accountTag = accountTagResult;
		accountTagFilePath = newAccountTagFilePath;
		
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

		wstring wParentFolderPath(current_path().string().begin(), current_path().string().end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		string command = "cloudflared.exe tunnel login";
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [Cloudflared command] " + command);

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
				"Failed to create process for creating cert!");
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
				"Failed to create tunnel cert file in '" + cloudFlareFolder + "'!");
		}
	}

	void CloudFlare::CreateTunnelCredentials()
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		
		string tunnelFile = tunnelID + ".json";
		string tunnelFilePath = path(path(cloudFlareFolder) / tunnelFile).string();
		replace(tunnelFilePath.begin(), tunnelFilePath.end(), '\\', '/');
		
		string configFile = "config.yml";
		string configFilePath = path(path(cloudFlareFolder) / configFile).string();
		replace(configFilePath.begin(), configFilePath.end(), '\\', '/');
		
		if (exists(tunnelFilePath)
			&& exists(configFilePath))
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"  [CLOUDFLARE_MESSAGE] Cloudflared credentials .json and .yml files already exist at '" + cloudFlareFolder + "'. Skipping creation.");
			return;
		}
		
		//
		// CREATE .json FILE
		//
		
		if (!exists(tunnelFilePath))
		{
			Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Warning,
			"Creating new tunnel credentials json file at '" + tunnelFilePath + "'.");
		
			ofstream tunnelFile(tunnelFilePath);
			if (!tunnelFile)
			{
				Core::CreatePopup(
					PopupReason::Reason_Error,
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Unable to open tunnel credentials json file for editing at '" + tunnelFilePath + "'!");
				return;
			}
		
			tunnelFile << "{\n";
			tunnelFile << "  \"TunnelID\": \"" << tunnelID << "\",\n";
			tunnelFile << "  \"TunnelSecret\": \"" << tunnelToken << "\",\n";
			tunnelFile << "  \"AccountTag\": \"" << accountTag << "\",\n";
			tunnelFile << "  \"TunnelName\": \"" << tunnelName << "\"\n";
			tunnelFile << "}\n";
		
			tunnelFile.close();
		}
		
		//
		// CREATE .yml FILE
		//
		
		if (!exists(configFilePath))
		{				
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Creating new tunnel credentials yml file at '" + configFilePath + "'.");
		
			ofstream configFile(configFilePath);
			if (!configFile)
			{
				Core::CreatePopup(
					PopupReason::Reason_Error,
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Unable to open tunnel credentials yml file for editing at '" + configFilePath + "'!");
				return;
			}
		
			configFile << "tunnel: " << tunnelID << "\n";
			configFile << "credentials-file: " << tunnelFilePath << "\n";
			configFile << "\n";
			configFile << "ingress: \n";
			configFile << "  - hostname: " << Server::server->GetDomainName() << "\n";
			configFile << "    service: http://127.0.0.1:80\n";
			configFile << "  - service: http_status:404\n";
		
			configFile.close();
		}
		
		//
		// FINISH
		//
		
		if (!exists(tunnelFilePath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create tunnel credentials json file for tunnel '" + tunnelName + "' at '" + tunnelFilePath + "'!");
		}
		
		if (!exists(configFilePath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create tunnel credentials yml file for tunnel '" + tunnelName + "' at '" + configFilePath + "'!");
		}
		
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_SUCCESS] Created new cloudflared credentials json file for tunnel '" + tunnelName + "' at '" + tunnelFilePath + "' and yml file at '" + configFilePath + "'.");
	}
	
	bool CloudFlare::TunnelExists()
	{
		HANDLE hReadPipe{};
		HANDLE hWritePipe{};
		SECURITY_ATTRIBUTES sa
		{
			sizeof(SECURITY_ATTRIBUTES),
			nullptr,
			TRUE
		};

		if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create pipe for cloudflared tunnel list!");
			return false;
		}

		SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

		//initialize structures for process creation
		STARTUPINFOW si{};
		PROCESS_INFORMATION pi{};
		si.cb = sizeof(si);
		si.dwFlags |= STARTF_USESTDHANDLES;
		si.hStdOutput = hWritePipe;
		si.hStdError = hWritePipe;

		string parentFolderPath = current_path().string();
		wstring wParentFolderPath(parentFolderPath.begin(), parentFolderPath.end());

		string command = "cloudflared.exe tunnel list";
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [Cloudflared command] " + command);

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
			CloseHandle(hReadPipe);
			CloseHandle(hWritePipe);

			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create process for listing tunnels!");
			return false;
		}

		//close thread handle and store process
		CloseHandle(hWritePipe);
		CloseHandle(pi.hThread);
		
		WaitForSingleObject(pi.hProcess, INFINITE);

		//read output from the pipe
		string output{};
		char buffer[512]{};
		DWORD bytesRead = 0;
		while (ReadFile(
			hReadPipe,
			buffer,
			sizeof(buffer) - 1,
			&bytesRead,
			nullptr))
		{
			if (bytesRead == 0) break;
			buffer[bytesRead] = '\0';
			output += buffer;
		}

		CloseHandle(hReadPipe);
		CloseHandle(pi.hProcess);

		bool tunnelExists = output.find(tunnelName) != string::npos;
		if (!tunnelExists)
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"Tunnel with the name '" + tunnelName + "' does not exist. Starting a new one.");
		}
		
		return tunnelExists;
	}
	
	bool CloudFlare::ServiceExists()
	{
		string serviceName = "Cloudflared";

		SC_HANDLE scm = OpenSCManager(
			nullptr,
			nullptr,
			SC_MANAGER_ENUMERATE_SERVICE);
		if (!scm)
		{
			return false;
		}

		bool found = false;
		ENUM_SERVICE_STATUS_PROCESS services[1024];
		DWORD bytesNeeded = 0, servicesReturned = 0, resumeHandle = 0;

		if (EnumServicesStatusExA(
			scm,
			SC_ENUM_PROCESS_INFO,
			SERVICE_WIN32,
			SERVICE_STATE_ALL,
			reinterpret_cast<LPBYTE>(&services),
			sizeof(services),
			&bytesNeeded,
			&servicesReturned,
			&resumeHandle,
			nullptr))
		{
			for (DWORD i = 0; i < servicesReturned; ++i)
			{
				if (serviceName == services[i].lpServiceName)
				{
					found = true;
					break;
				}
			}
		}
		CloseServiceHandle(scm);
		return found;
	}

	void CloudFlare::InstallTunnel()
	{
		if (TunnelExists()
			|| ServiceExists())
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"  [CLOUDFLARE_MESSAGE] Cloudflared service is already installed. Skipping installation.");

			RunTunnel();

			return;
		}
		
		string command = "cloudflared.exe service install " + tunnelToken;
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_COMMAND] " + command);
			
		int result = system(command.c_str());
		
		if (result != 0)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create cloudflared tunnel '" + tunnelName + "'! Exit code: " + to_string(result));
			return;
		}
		
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLARE_SUCCESS] Created new cloudflared tunnel '" + tunnelName + "'.");
	}

	void CloudFlare::RunTunnel()
	{	
		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		string parentFolderPath = current_path().string();
		wstring wParentFolderPath(parentFolderPath.begin(), parentFolderPath.end());

		string command = "cloudflared.exe tunnel run " + tunnelName;
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [CLOUDFLFARE_COMMAND] " + command);

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
		CloseHandle(pi.hProcess);
		
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

		if (closeCloudflaredAtShutdown)
		{
			int result = system("taskkill /IM cloudflared.exe /F");
		
			if (result != 0)
			{
				Core::CreatePopup(
					PopupReason::Reason_Warning,
					"Failed to terminate cloudflared.exe! Exit code: " + to_string(result));
			}
			else
			{
				Core::PrintConsoleMessage(
					ConsoleMessageType::Type_Message,
					"  [CLOUDFLARE_SUCCESS] Cloudflared.exe was terminated.");
			}	
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