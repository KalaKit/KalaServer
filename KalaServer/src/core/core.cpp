//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <Windows.h>
#include <shellapi.h>
#include <iostream>
#include <chrono>

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaKit::DNS::CloudFlare;
using KalaKit::DNS::CustomDNS;

using std::cout;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using std::time_t;
using std::tm;
using std::snprintf;

namespace KalaKit::Core
{
	void KalaServer::Run()
	{
		//gui etc runtime loop will go here in the future
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
		unsigned int indentationLength,
		bool addTimeStamp,
		ConsoleMessageType type,
		const string& customTag,
		const string& message)
	{
		string result{};
		string indentationContent{};
		string customTagContent{};
		string timeStampContent{};
		string targetTypeContent{};

#ifndef _DEBUG
		if (type == ConsoleMessageType::Type_Debug) return;
#endif

		if (indentationLength > 0)
		{
			indentationContent = string(indentationLength, ' ');
		}

		if (addTimeStamp)
		{
			auto now = system_clock::now();
			auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
			time_t now_c = system_clock::to_time_t(now);
			tm localTime{};
			localtime_s(&localTime, &now_c);

			char timeBuffer[16]{};
			snprintf(
				timeBuffer,
				sizeof(timeBuffer),
				"[%02d:%02d:%02d:%03lld] ",
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				static_cast<long long>(ms.count()));

			timeStampContent = timeBuffer;
		}

		if (customTag != "") customTagContent = customTag + " ";

		switch (type)
		{
		case ConsoleMessageType::Type_Error:
			targetTypeContent = "[ERROR] ";
			break;
		case ConsoleMessageType::Type_Warning:
			targetTypeContent = "[WARNING] ";
			break;
		case ConsoleMessageType::Type_Debug:
			targetTypeContent = "[DEBUG] ";
			break;
		}

		result = 
			indentationContent
			+ timeStampContent
			+ customTagContent
			+ targetTypeContent
			+ message;

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