//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <iostream>

#include "KalaHeaders/log_utils.hpp"

#include "KalaWindow/include/core/core.hpp"

#include "core/core_program.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;

using KalaWindow::Core::KalaWindowCore;

using KalaKit::DNS::CloudFlare;
using KalaKit::DNS::CustomDNS;

using std::cout;
using std::cin;

static bool IsAdmin();
static bool RunAsAdmin();

namespace KalaServer::Core
{
	void KalaServerCore::Initialize()
	{
		if (!IsAdmin())
		{
			Log::Print(
				"Not running as admin, requesting elevation.",
				"KalaServer",
				LogType::LOG_INFO);

			if (!RunAsAdmin())
			{
				KalaWindowCore::ForceClose(
					"KalaServer initalization error",
					"Failed to start KalaServer! You must launch as admin to use this program.");

				quick_exit(1);
			}
		}
		else
		{
			isInitialized = true;

			Run();
		}
	}

	void KalaServerCore::Run()
	{
		Log::Print(
			"Reached runtime loop. Press 'Enter' to exit.",
			"KalaServer",
			LogType::LOG_SUCCESS);

		cin.get();
	}

	void KalaServerCore::Shutdown()
	{
		Server::server->Quit();

		if (CloudFlare::IsRunning()) CloudFlare::Quit();
		if (CustomDNS::IsRunning()) CustomDNS::Quit();
	}
}

bool IsAdmin()
{
#ifdef _WIN32
	HANDLE token = nullptr;
	BOOL isElevated = FALSE;

	if (!OpenProcessToken(
		GetCurrentProcess(),
		TOKEN_QUERY,
		&token))
	{
		return false;
	}

	TOKEN_ELEVATION elevation{};
	DWORD returnedSize{};

	if (GetTokenInformation(
		token,
		TokenElevation,
		&elevation,
		sizeof(elevation),
		&returnedSize))
	{
		isElevated = elevation.TokenIsElevated;
	}

	CloseHandle(token);
	return isElevated == TRUE;

#endif

	return false;
}

bool RunAsAdmin()
{
#ifdef _WIN32
	char exePath[MAX_PATH];
	GetModuleFileNameA(nullptr, exePath, MAX_PATH);

	SHELLEXECUTEINFOA sei{};
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = "runas";
	sei.lpFile = exePath;
	sei.nShow = SW_SHOWNORMAL;

	if (!ShellExecuteExA(&sei))
	{
		DWORD err = GetLastError();
		if (err == ERROR_CANCELLED)
		{
			Log::Print(
				"User cancelled elevation request, aborting initialization!",
				"KalaServer",
				LogType::LOG_ERROR,
				2);

			KalaWindowCore::ForceClose(
				"KalaServer initalization error",
				"Failed to start KalaServer! You must launch as admin to use this program.");

			return false;
		}
	}

	return true;
#endif

	return false;
}