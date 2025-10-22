//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <iostream>
#include <chrono>
#include <algorithm>

#include "KalaHeaders/log_utils.hpp"

#include "KalaWindow/include/core/core.hpp"
#include "KalaWindow/include/core/crash.hpp"

#include "core/core_program.hpp"
#include "graphics/render.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaHeaders::Log;
using KalaHeaders::LogType;

using KalaWindow::Core::KalaWindowCore;
using KalaWindow::Core::CrashHandler;

using KalaServer::DNS::CloudFlare;
using KalaServer::DNS::CustomDNS;
using KalaServer::Graphics::Render;

using std::cout;
using std::cin;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::chrono::duration;
using std::clamp;

static bool IsAdmin();
static bool RunAsAdmin();

namespace KalaServer::Core
{
	void KalaServerCore::Initialize()
	{
		/*
		if (!IsAdmin())
		{
			Log::Print(
				"Not running as admin, requesting elevation.",
				"KALASERVER",
				LogType::LOG_INFO);

			if (!RunAsAdmin())
			{
				KalaWindowCore::ForceClose(
					"Initalization error",
					"Kalaserver requires admin to run!");

				quick_exit(1);
			}
			else quick_exit(0); //close the initializer program
		}
		*/

		CrashHandler::Initialize(
			"KalaServer",
			Shutdown);

		KalaWindowCore::SetUserShutdownFunction(Shutdown);

		Render::Initialize();
		
		isInitialized = true;
		isRunning = true;

		Run();
	}

	void KalaServerCore::UpdateDeltaTime()
	{
		auto now = steady_clock::now();
		static time_point<steady_clock> lastFrameTime = now;

		duration<f64> delta = now - lastFrameTime;
		lastFrameTime = now;

		deltaTime = clamp(delta.count(), 0.0, 0.1);
	}

	void KalaServerCore::Run()
	{
		Log::Print(
			"\n==============================\n"
			" RENDER LOOP\n"
			"==============================\n");

		while (isRunning)
		{
			UpdateDeltaTime();
			Render::Run();
		}
	}

	void KalaServerCore::Shutdown()
	{
		Server::server->Quit();

		if (CloudFlare::IsRunning()) CloudFlare::Quit();
		if (CustomDNS::IsRunning()) CustomDNS::Quit();

		Render::Shutdown();
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
				"KALASERVER",
				LogType::LOG_ERROR,
				2);

			KalaWindowCore::ForceClose(
				"Initalization error",
				"Kalaserver requires admin to run!");

			return false;
		}
	}

	return true;
#endif

	return false;
}