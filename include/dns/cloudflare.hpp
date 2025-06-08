//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

//#include "external/ktypes.hpp"

namespace KalaKit::DNS
{
	using std::string;
	//using KalaKit::KalaTypes::i16;

	class CloudFlare
	{
	public:
		static inline string tunnelName{};
		static inline uintptr_t tunnelRunHandle{};

		/// <summary>
		/// Start up cloudflared. Must not be running dns with this.
		/// Returns true if initialized successfully.
		/// </summary>
		static bool Initialize(
			bool shouldCloseCloudflaredAtShutdown,
			const string& tunnelName);

		static bool IsRunning() { return isRunning; }

		static bool IsConnHealthy(unsigned int conn)
		{
			switch (conn)
			{
			default:
				return false;
			case 0:
				return isConn0Healthy;
				break;
			case 1:
				return isConn1Healthy;
				break;
			case 2:
				return isConn2Healthy;
				break;
			case 3:
				return isConn3Healthy;
				break;
			}
			return false;
		}

		/// <summary>
		/// Shut down cloudflared.
		/// </summary>
		static void Quit();
	private:
		static inline bool closeCloudflaredAtShutdown = true;
	
		static inline bool isInitializing = false;
		static inline bool isRunning = false;

		static inline bool isConn0Healthy = false;
		static inline bool isConn1Healthy = false;
		static inline bool isConn2Healthy = false;
		static inline bool isConn3Healthy = false;
		
		/// <summary>
		/// Returns true if all the data passed to cloudflared is valid.
		/// </summary>
		static bool CloudflarePreInitializeCheck(const string& tunnelName);

		/// <summary>
		/// Returns the value of tunnel token, id or tag as a string from its file.
		/// </summary>
		static string GetTextFileValue(
			const string& textPath,
			unsigned int minLength = 32, 
			unsigned int maxLength = 300);

		/// <summary>
		/// Runs 'cloudflared tunnel login'
		/// through cloudflared as a separate process.
		/// </summary>
		static void CreateCert();

		/// <summary>
		/// Runs 'cloudflared tunnel delete <tunnelName>'
		/// and 'cloudflared tunnel create <tunnelName>'
		/// through cloudflared as a separate process.
		/// </summary>
		static void CreateTunnelCredentials();

		/// <summary>
		/// Runs 'cloudflared route dns thekalakit.com KalaServer'
		/// through cloudflared as a separate process.
		/// </summary>
		static void RouteTunnel();

		/// <summary>
		/// Runs 'tunnel run <tunnelName>'
		/// through cloudflared as a separate process.
		/// </summary>
		static void RunTunnel();

		/// <summary>
		/// Pipes all cloudflare messages to internal console messaging system for better style.
		/// </summary>
		static void PipeCloudflareMessages(void* handle);
	};
}