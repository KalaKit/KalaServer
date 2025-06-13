//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <Windows.h>
#include <shellapi.h>

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaKit::DNS::CloudFlare;
using KalaKit::DNS::CustomDNS;

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

	void KalaServer::Quit()
	{
		Server::server->Quit();

		if (CloudFlare::IsRunning()) CloudFlare::Quit();
		if (CustomDNS::IsRunning()) CustomDNS::Quit();
		
		exit(0);
	}
}