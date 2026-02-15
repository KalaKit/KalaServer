//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <filesystem>
#include <string_view>
#include <thread>

#include "KalaHeaders/core_utils.hpp"

namespace KalaServer::Server
{
	using std::filesystem::path;
	using std::string_view;
	using std::thread;

	using u8 = uint8_t;

	class LIB_API Cloudflare
	{
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

		static bool IsTunnelHealthy();
		static bool IsTunnelAlive();

		//Shut down the Cloudflare tunnel
		static void Shutdown();
	private:
		static bool RunTunnel(string_view command);
		static void PipeCloudflareMessages(uintptr_t readPipe);

		static thread cfThread;
	};
}