//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "server/ks_cloudflare.hpp"
#include "server/ks_server.hpp"

namespace KalaServer::Server
{
	void CloudFlare::Initialize()
	{

	}

	bool CloudFlare::IsTunnelAlive()
	{
		if (ServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot check for internet access because the server has not been initialized!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Cannot check for internet access because the server is not ready!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);
		}

		if (!CloudFlare::IsInitialized())
		{
			Log::Print(
				"Cannot check for internet access because CloudFlare tunnel has not been initialized!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!CloudFlare::IsRunning())
		{
			Log::Print(
				"Cannot check for internet access because CloudFlare tunnel is not running!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return false;
	}
}