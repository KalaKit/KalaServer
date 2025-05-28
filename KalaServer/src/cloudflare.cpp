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
using std::replace;

namespace KalaServer
{
	static HANDLE cloudFlareWindowProcess{};
	static HANDLE cloudFlareCertProcess{};
	static HANDLE cloudFlareCreateTunnelProcess{};
	static HANDLE cloudFlareTunnelProcess{};

	string cloudFlaredName{};
	string cloudflaredPath{};
	string cloudFlaredParentPath{};

	string cloudFlareFolder{};
	string certPath{};
	string tunnelConfigPath{};

	bool CloudFlare::RunCloudflared()
	{
		if (DNS::IsRunning())
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Cannot run DNS and cloudflared together!");
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
			return false;
		}

		SetReusedParameters();

		if (!CreateHeadlessWindow()) return false;

		isRunning = true;
		return true;
	}

	void CloudFlare::SetReusedParameters()
	{
		cloudFlaredParentPath = current_path().string();

		cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		certPath = path(path(cloudFlareFolder) / "cert.pem").string();
		string tunnelConfigName = Server::server->GetServerName() + ".json";
		tunnelConfigPath = path(path(cloudFlareFolder) / tunnelConfigName).string();
	}

	bool CloudFlare::CreateHeadlessWindow()
	{
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
		if (!CreateProcessW
		(
			wExePath.c_str(),          //path to the executable
			const_cast<LPWSTR>(wCommands.c_str()), //command line arguments
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
			//retrieve the error code and print a descriptive error message
			LPVOID lpMsgBuf = nullptr;
			DWORD dw = GetLastError();
			FormatMessageW(
				FORMAT_MESSAGE_ALLOCATE_BUFFER
				| FORMAT_MESSAGE_FROM_SYSTEM
				| FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPWSTR)&lpMsgBuf, 0, nullptr);

			if (lpMsgBuf)
			{
				wstring wideMessage(reinterpret_cast<wchar_t*>(lpMsgBuf));

				int sizeNeeded = WideCharToMultiByte(
					CP_UTF8,
					0,
					wideMessage.c_str(),
					-1,
					nullptr,
					0,
					nullptr,
					nullptr);

				if (sizeNeeded > 0)
				{
					string message(sizeNeeded, 0);

					WideCharToMultiByte(
						CP_UTF8,
						0,
						wideMessage.c_str(),
						-1,
						&message[0],
						sizeNeeded,
						nullptr,
						nullptr);

					Core::PrintConsoleMessage(
						ConsoleMessageType::Type_Error,
						message);
				}
				LocalFree(lpMsgBuf);
			}

			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start cloudflared!"
				"\n\n"
				"Reason:"
				"\n"
				"Failed to create cloudflared process!");
			return false;
		}

		//close thread handle and store process
		cloudFlareWindowProcess = pi.hProcess;
		CloseHandle(pi.hThread);

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Created cloudflared process successfully. PID: " + to_string(pi.dwProcessId));

		if (!exists(certPath)) CreateCert();
		else RunTunnel();

		return true;
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

		//close thread handle and store process
		CloseHandle(pi.hThread);
		cloudFlareCertProcess = pi.hProcess;

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"Launched browser to authorize with CloudFlare. PID: " + to_string(pi.dwProcessId));

		WaitForSingleObject(pi.hProcess, INFINITE);

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

		//close thread handle and store process
		CloseHandle(pi.hThread);
		cloudFlareCreateTunnelProcess = pi.hProcess;

		WaitForSingleObject(pi.hProcess, INFINITE);

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

		//close thread handle and store process
		CloseHandle(pi.hThread);
		cloudFlareCreateTunnelProcess = pi.hProcess;

		WaitForSingleObject(pi.hProcess, INFINITE);

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

		if (cloudFlareWindowProcess)
		{
			TerminateProcess(cloudFlareWindowProcess, 0);
			CloseHandle(cloudFlareWindowProcess);
			cloudFlareWindowProcess = nullptr;
		}

		if (cloudFlareCertProcess)
		{
			TerminateProcess(cloudFlareCertProcess, 0);
			CloseHandle(cloudFlareCertProcess);
			cloudFlareCertProcess = nullptr;
		}

		if (cloudFlareCreateTunnelProcess)
		{
			TerminateProcess(cloudFlareCreateTunnelProcess, 0);
			CloseHandle(cloudFlareCreateTunnelProcess);
			cloudFlareCreateTunnelProcess = nullptr;
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