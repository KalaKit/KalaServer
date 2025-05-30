//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <Windows.h>
#include <shellapi.h>
#include <iostream>

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaKit::DNS::CloudFlare;
using KalaKit::DNS::CustomDNS;

using std::cout;

namespace KalaKit::Core
{
	bool KalaServer::Run()
	{
		bool isServerRunning = Server::server->Run();

		return isServerRunning;
	}

	bool KalaServer::IsRunningAsAdmin()
	{
		BOOL isElevated = FALSE;
		HANDLE token = nullptr;

		if (OpenProcessToken(
			GetCurrentProcess(),
			TOKEN_QUERY,
			&token))
		{
			TOKEN_ELEVATION elevation{};
			DWORD size = sizeof(elevation);

			if (GetTokenInformation(
				token,
				TokenElevation,
				&elevation,
				sizeof(elevation),
				&size))
			{
				isElevated = elevation.TokenIsElevated;
			}
			CloseHandle(token);
		}

		return isElevated;
	}

	void KalaServer::PrintConsoleMessage(
		ConsoleMessageType type,
		const string& message)
	{
		string targetType{};

		switch (type)
		{
		case ConsoleMessageType::Type_Error:
			targetType = "[ERROR] ";
			break;
		case ConsoleMessageType::Type_Warning:
			targetType = "[WARNING] ";
			break;
		case ConsoleMessageType::Type_Message:
			targetType = "";
			break;
		}

		string result = targetType + message;
		cout << result + "\n";
	}

	void KalaServer::CreatePopup(
		PopupReason reason,
		const string& message)
	{
		string popupTitle{};
		string serverName = "Server";
		if (Server::server != nullptr
			&& Server::server->GetServerName() != "")
		{
			serverName = Server::server->GetServerName();
		}

		if (reason == PopupReason::Reason_Error)
		{
			popupTitle = serverName + " error";

			MessageBoxA(
				nullptr,
				message.c_str(),
				popupTitle.c_str(),
				MB_ICONERROR
				| MB_OK);

			isRunning = false;
		}
		else if (reason == PopupReason::Reason_Warning)
		{
			popupTitle = serverName + "  warning";

			MessageBoxA(
				nullptr,
				message.c_str(),
				popupTitle.c_str(),
				MB_ICONWARNING
				| MB_OK);
		}
	}

	void KalaServer::Quit()
	{
		Server::server->Quit();

		if (CloudFlare::IsRunning()) CloudFlare::Quit();
		if (CustomDNS::IsRunning()) CustomDNS::Quit();
		
		exit(0);
	}
}