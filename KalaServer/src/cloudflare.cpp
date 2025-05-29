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
	static HANDLE cloudFlareTunnelProcess{};

	string cloudFlaredName{};
	string cloudflaredPath{};
	string cloudFlaredParentPath{};

	string cloudFlareFolder{};
	string certPath{};
	string tunnelConfigPath{};

	bool CloudFlare::Initialize(
		const string& tunnelName,
		const string& tunnelFilePath)
	{
		if (isInitializing)
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Cannot initialize cloudflared while it is already being initialized!");
			return false;
		}
		
		if (tunnelName == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start cloudflared with empty tunnel name name!");
			return false;
		}
		if (tunnelFilePath == "")
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Cannot start cloudflared with empty tunnel file path!");
			return false;
		}
		
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

		cloudFlaredName = "cloudflared.exe";
		cloudflaredPath = path(current_path() / cloudFlaredName).string();
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

		SetReusedParameters();

		if (!exists(certPath)) CreateCert();
		else if (!TunnelExists()) CreateTunnel();
		else RunTunnel();

		isInitializing = false;
		isRunning = true;

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Cloudflared initialization completed.");

		return true;
	}
	
	string CloudFlare::GetTunnelCommandFile()
	{
		if (!exists(tunnelCommandFile))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel command file '" + tunnelCommandFile + "' does not exist!");
			return "";
		}
		
		ifstream file(tunnelCommandFile);
		if (!file)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to open tunnel command file '" + tunnelCommandFile + "'!");
			return "";
		}
		
		stringstream buffer;
		buffer << file.rdbuf();
		if (buffer.str().empty())
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Tunnel command file '" + tunnelCommandFile + "' is empty!");
			return "";
		}
		return buffer.str();
	}

	void CloudFlare::SetReusedParameters()
	{
		cloudFlaredParentPath = current_path().string();

		cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		certPath = path(path(cloudFlareFolder) / "cert.pem").string();
		string tunnelConfigName = Server::server->GetServerName() + ".json";
		tunnelConfigPath = path(path(cloudFlareFolder) / tunnelConfigName).string();
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

		wstring wParentFolderPath(cloudFlaredParentPath.begin(), cloudFlaredParentPath.end());
		wstring wExePath(cloudflaredPath.begin(), cloudflaredPath.end());

		string command = "tunnel list";
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [Cloudflared command] " + command);

		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			wExePath.c_str(),          //path to the executable
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
		
		return output.find(tunnelName) != string::npos;
	}

	void CloudFlare::CreateCert()
	{
		wstring wParentFolderPath(cloudFlaredParentPath.begin(), cloudFlaredParentPath.end());
		wstring wExePath(cloudflaredPath.begin(), cloudflaredPath.end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		string command = "tunnel login";
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [Cloudflared command] " + command);

		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			wExePath.c_str(),          //path to the executable
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
		else CreateTunnel();
	}

	void CloudFlare::CreateTunnel()
	{
		if (TunnelExists()) RunTunnel();

		string serverName = Server::server->GetServerName();

		wstring wParentFolderPath(cloudFlaredParentPath.begin(), cloudFlaredParentPath.end());
		wstring wExePath(cloudflaredPath.begin(), cloudflaredPath.end());
		string commands{};
		wstring wCommands(commands.begin(), commands.end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		string command = "tunnel create " + serverName;
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [Cloudflared command] " + command);

		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			wExePath.c_str(),          //path to the executable
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
				"Failed to create process for creating tunnel!");
			return;
		}

		//close thread handle and process
		CloseHandle(pi.hThread);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Created cloudflared tunnel. PID: " + to_string(pi.dwProcessId));

		CreateTunnelConfigFile();
	}

	void CloudFlare::CreateTunnelConfigFile()
	{
		//get tunnel id by parsing from tunnel list
		FILE* pipe = _popen("cloudflared tunnel list --output json", "r");
		if (!pipe)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to open pipe for creating tunnel config file!");
			return;
		}

		char buffer[4096];
		string result{};
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		{
			result += buffer;
		}
		_pclose(pipe);

		size_t namePos = result.find(Server::server->GetServerName());
		if (namePos == string::npos)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Did not find tunnel name '" + Server::server->GetServerName() + "' in cloudflared list!");
			return;
		}
		
		size_t idStart = result.rfind("\"id\":\"", namePos);
		if (idStart == string::npos)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Did not find tunnel ID near tunnel name in cloudflared list output!");
			return;
		}

		idStart += 6;
		size_t idEnd = result.find("\"", idStart);
		string tunnelID = result.substr(idStart, idEnd - idStart);

		string cleanedCredentialsPath = cloudFlareFolder + "\\" + tunnelID + ".json";
		replace(cleanedCredentialsPath.begin(), cleanedCredentialsPath.end(), '\\', '/');
		string configContent =
			"{\n"
			"  \"tunnel\": \"" + tunnelID + "\",\n"
			"  \"credentials-file\": \"" + cleanedCredentialsPath + "\"\n"
			"}\n";

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Attempting to create tunnel config file at: " + tunnelConfigPath);

		ofstream configFile(tunnelConfigPath);
		if (!configFile)
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to open tunnel config file '" + tunnelConfigPath + "' for writing!");
			return;
		}

		configFile << configContent;
		configFile.close();

		if (!exists(tunnelConfigPath))
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to set up cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create tunnel config file to '" + tunnelConfigPath + "'!");
			return;
		}

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Wrote tunnel config: " + tunnelConfigPath);

		RouteDNS();
	}

	void CloudFlare::RouteDNS()
	{
		string serverName = Server::server->GetServerName();
		string domainName = Server::server->GetDomainName();

		wstring wParentFolderPath(cloudFlaredParentPath.begin(), cloudFlaredParentPath.end());
		wstring wExePath(cloudflaredPath.begin(), cloudflaredPath.end());
		string commands{};
		wstring wCommands(commands.begin(), commands.end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		string command =
			"tunnel route dns "
			+ serverName
			+ " "
			+ domainName;
		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"  [Cloudflared command] " + command);

		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			wExePath.c_str(),          //path to the executable
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
				"Failed to create process for routing dns!");
			return;
		}

		//close thread handle and process
		CloseHandle(pi.hThread);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Routed tunnel dns. PID: " + to_string(pi.dwProcessId));

		RunTunnel();
	}

	void CloudFlare::RunTunnel()
	{
		string serverName = Server::server->GetServerName();

		wstring wParentFolderPath(cloudFlaredParentPath.begin(), cloudFlaredParentPath.end());
		wstring wExePath(cloudflaredPath.begin(), cloudflaredPath.end());
		string commands{};
		wstring wCommands(commands.begin(), commands.end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		//create the new process
		string command = "tunnel run " + serverName;
		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			wExePath.c_str(),          //path to the executable
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