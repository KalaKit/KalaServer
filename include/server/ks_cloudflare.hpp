//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

#include "KalaHeaders/core_utils.hpp"

namespace KalaServer::Server
{
	using std::string;

	using u8 = uint8_t;

	class LIB_API Cloudflare
	{
	public:
		//Start up the Cloudflare tunnel,
		//pass the cloudflare tunnel exe path where its ran from
		//and pass the cloudflare folder where the json and cert files will live at
		static bool Initialize(
			const string& cloudflareExePath,
			const string& cloudflareFolder);

		static inline bool IsInitialized() { return isInitialized; }
		static inline bool IsHealthy(u8 connection)
		{
			switch (connection)
			{
			default:
				return false;
			case 0:
				return isFirstHealthy;
				break;
			case 1:
				return isSecondHealthy;
				break;
			case 2:
				return isThirdHealthy;
				break;
			case 3:
				return isFourthHealthy;
				break;
			}
			return false;
		}

		static bool IsTunnelAlive();

		//Shut down the Cloudflare tunnel
		static void Shutdown();
	private:
		static inline bool isInitialized{};

		static inline bool isFirstHealthy{};
		static inline bool isSecondHealthy{};
		static inline bool isThirdHealthy{};
		static inline bool isFourthHealthy{};
	};
}