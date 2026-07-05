//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <string_view>
#include <thread>

#include "core_utils.hpp"

namespace KalaServer::Core
{
	using std::filesystem::path;
	using std::string_view;
	using std::thread;

	using u8 = uint8_t;

	class LIB_API Cloudflare
	{
	friend class KalaServerCore;
	public:
		static void SetVerboseLoggingState(bool state);

		//Start up the Cloudflare tunnel,
		//pass the cloudflare tunnel exe path where its ran from
		//and pass the cloudflare folder where the json and cert files will live at
		static bool Initialize(
			string_view tunnelName,
			const path& cloudflareExePath,
			const path& cloudflareFolder);

		static bool IsInitialized();

		//Returns false if server cannot connect to google.com
		//or if cloudflare tunnel is not healthy if cloudflare is required
		static bool IsHealthy();

		//Shut down the Cloudflare tunnel
		static void Shutdown();
	private:
		static bool RunTunnel(string_view command);
		static void PipeCloudflareMessages(uintptr_t readPipe);

		static bool IsTunnelHealthy();
		static bool IsTunnelAlive();

		static thread cfThread;
	};
}