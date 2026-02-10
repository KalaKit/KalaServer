//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>

#include "KalaHeaders/core_utils.hpp"

namespace KalaServer::Server
{
	using std::filesystem::path;

	using u8 = uint8_t;

	class LIB_API Cloudflare
	{
	public:
		//Start up the Cloudflare tunnel,
		//pass the cloudflare tunnel exe path where its ran from
		//and pass the cloudflare folder where the json and cert files will live at
		static bool Initialize(
			const path& cloudflareExePath,
			const path& cloudflareFolder);

		static bool IsInitialized();
		static bool IsHealthy(u8 connection);

		static bool IsTunnelAlive();

		//Shut down the Cloudflare tunnel
		static void Shutdown();
	};
}