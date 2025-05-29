//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <filesystem>
#include <string>
#include <Windows.h>
#include <fstream>
#include <algorithm>
#include <cctype>

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
using std::isspace;

namespace KalaServer
{
	static HANDLE cloudFlareTunnelProcess{};

	bool CloudFlare::Initialize(
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
		
		if (newTunnelName == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start cloudflared with empty tunnel name name!");
			return false;
		}
		tunnelName = newTunnelName;

		if (newTunnelTokenFilePath == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start cloudflared with empty tunnel file path!");
			return false;
		}
		tunnelTokenFilePath = newTunnelTokenFilePath;
		
		isInitializing = true;
		
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

		tunnelToken = GetTunnelTokenText();

		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();
		if (!exists(certPath)) CreateCert();

		string tunnelFile = tunnelName + ".json";
		string tunnelFilePath = path(path(cloudFlareFolder) / tunnelFile).string();
		if (!exists(tunnelFilePath)) CreateTunnelCredentials();
		if (!TunnelExists()) InstallTunnel();
		RunTunnel();

		isInitializing = false;
		isRunning = true;

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Cloudflared initialization completed.");

		return true;
	}

	string CloudFlare::GetTunnelTokenText()
	{
		string newTunnelToken{};

		if (!exists(tunnelTokenFilePath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel token text file does not exist!");
			return "";
		}

		ifstream file(tunnelTokenFilePath);
		if (!file)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to open tunnel token text file!");
			return "";
		}

		stringstream buffer{};
		buffer << file.rdbuf();
		newTunnelToken = TrimTunnelToken(buffer.str());

		size_t tokenLength = newTunnelToken.length();

		if (tokenLength < 150
			|| tokenLength > 300)
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Warning,
				"Full token:"
				"\n"
				+ newTunnelToken);

			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel token text file size '" + to_string(tokenLength) + "' is too small or too big! You probably didn't copy the token correctly.");
			return "";
		}

		if (newTunnelToken.find("cloudflared.exe service install ") != string::npos)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel token text file contains incorrect info! You probably didn't copy the token correctly.");
			return "";
		}

		return newTunnelToken;
	}

	string CloudFlare::TrimTunnelToken(const string& newTunnelToken)
	{
		size_t tokenLength = newTunnelToken.length();
		if (tokenLength == 0)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel token text file is empty!");
			return "";
		}

		size_t start = 0;
		while (start < newTunnelToken.size()
			&& isspace(static_cast<unsigned char>(newTunnelToken[start])))
		{
			++start;
		}

		size_t end = newTunnelToken.size();
		while (end > start
			&& isspace(static_cast<unsigned char>(newTunnelToken[end - 1])))
		{
			--end;
		}

		return newTunnelToken.substr(start, end - start);
	}

	void CloudFlare::CreateCert()
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();

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
		CloseHandle(pi.hThread);

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Launched browser to authorize with CloudFlare. PID: " + to_string(pi.dwProcessId));

		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);

		if (!exists(certPath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create cert through 'cloudflared.exe tunnel login'!");
		}
	}

	void CloudFlare::CreateTunnelCredentials()
	{
		string targetCommand = TunnelExists() ? "token" : "create";

		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string tunnelFile = tunnelName + ".json";
		string tunnelFilePath = path(path(cloudFlareFolder) / tunnelFile).string();

		wstring wParentFolderPath(current_path().string().begin(), current_path().string().end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		string command = "cloudflared.exe tunnel " + targetCommand + " " + tunnelName;
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
				"Failed to create process for creating credentials!");
			return;
		}

		//close thread handle and process
		CloseHandle(pi.hThread);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);

		if (!exists(tunnelFilePath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create tunnel credentials with 'cloudflared.exe tunnel " + targetCommand + "'!");
		}
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
		WaitForSingleObject(pi.hProcess, INFINITE);
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

	void CloudFlare::InstallTunnel()
	{
		if (TunnelExists()
			|| ServiceExists())
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Message,
				"Cloudflared service is already installed. Skipping installation.");

			RunTunnel();

			return;
		}

		string serverName = Server::server->GetServerName();
		string parentFolderPath = current_path().string();
		wstring wParentFolderPath(parentFolderPath.begin(), parentFolderPath.end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		//create the new process
		string command = "cloudflared.exe service install " + tunnelToken;
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
				"Failed to create process for creating cloudflared tunnel!");
			return;
		}

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Successfully created new cloudflared tunnel '" + tunnelName + "'. PID: " + to_string(pi.dwProcessId));

		//close thread handle and store process
		CloseHandle(pi.hThread);
		cloudFlareTunnelProcess = pi.hProcess;
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

	void CloudFlare::RunTunnel()
	{
		string serverName = Server::server->GetServerName();
		string parentFolderPath = current_path().string();
		wstring wParentFolderPath(parentFolderPath.begin(), parentFolderPath.end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		//create the new process
		string command = "cloudflared.exe tunnel run " + serverName;
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
				"Failed to create process for running cloudflared tunnel!");
			return;
		}

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Running cloudflared tunnel. PID: " + to_string(pi.dwProcessId));

		//close thread handle and store process
		CloseHandle(pi.hThread);
		cloudFlareTunnelProcess = pi.hProcess;
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

		if (cloudFlareTunnelProcess)
		{
			TerminateProcess(cloudFlareTunnelProcess, 0);
			CloseHandle(cloudFlareTunnelProcess);
			cloudFlareTunnelProcess = nullptr;
		}

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Cloudflared was successfully shut down!");
	}
}