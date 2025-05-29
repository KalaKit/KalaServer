//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <Windows.h>
#include <shellapi.h>
#include <iostream>

#include "core.hpp"
#include "server.hpp"
#include "cloudflare.hpp"
#include "dns.hpp"

using std::cout;

namespace KalaServer
{
	bool Core::Run()
	{
		bool isServerRunning = Server::server->Run();

		return isServerRunning;
	}

	bool Core::IsRunningAsAdmin()
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

	void Core::PrintConsoleMessage(
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

	void Core::CreatePopup(
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

	void Core::Quit()
	{
		Server::server->Quit();

		if (CloudFlare::IsRunning()) CloudFlare::Quit();
		if (DNS::IsRunning()) DNS::Quit();
		
		exit(0);
	}
}