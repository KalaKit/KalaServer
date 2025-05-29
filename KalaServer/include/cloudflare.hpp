//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

namespace KalaServer
{
	class CloudFlare
	{
	public:
		/// <summary>
		/// Start up dns. Must not be running dns with this.
		/// Returns true if initialized successfully.
		/// </summary>
		static bool RunCloudflared();

		static bool IsInitializing() { return isInitializing; }
		static bool IsRunning() { return isRunning; }

		/// <summary>
		/// Shut down cloudflared.
		/// </summary>
		static void Quit();
	private:
		static inline bool isInitializing = true;
		static inline bool isRunning = false;
		
		static string GetTunnelCommandFile(const string& tunnelCommandFilePath);

		static void SetReusedParameters();

		/// <summary>
		/// Creates the window cloudflared will run inside of.
		/// </summary>
		static bool CreateHeadlessWindow();

		/// <summary>
		/// Runs 'cloudflared.exe tunnel login'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static void CreateCert();

		/// <summary>
		/// Runs 'cloudflared.exe tunnel create kalaserver'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static void CreateTunnel();

		/// <summary>
		/// Manually creates and fills the config file.
		/// </summary>
		static void CreateTunnelConfigFile();

		/// <summary>
		/// Runs 'cloudflared tunnel route dns <domain> <name>'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static void RouteDNS();

		/// <summary>
		/// Runs 'tunnel run kalaserver'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		/// <returns></returns>
		static void RunTunnel();
	};
}