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
		/// Start up cloudflared. Must not be running dns with this.
		/// Returns true if initialized successfully.
		/// </summary>
		static bool Initialize(
			bool shouldCloseCloudflaredAtShutdown,
			const string& tunnelName,
			const string& tunnelTokenFilePath,
			const string& tunnelIDFilePath,
			const string& accountTagFilePath);

		static bool IsRunning() { return isRunning; }

		/// <summary>
		/// Shut down cloudflared.
		/// </summary>
		static void Quit();
	private:
		static inline bool closeCloudflaredAtShutdown = true;
	
		static inline bool isInitializing = false;
		static inline bool isRunning = false;
		
		static inline string tunnelName;
		
		static inline string tunnelToken;
		static inline string tunnelTokenFilePath;
		
		static inline string tunnelID;
		static inline string tunnelIDFilePath;
		
		static inline string accountTag;
		static inline string accountTagFilePath;
		
		/// <summary>
		/// Returns true if all the data passed to cloudflared is valid.
		/// </summary>
		static bool CloudflarePreInitializeCheck(
			const string& tunnelName,
			const string& tunnelTokenFilePath,
			const string& tunnelIDFilePath,
			const string& accountTagFilePath);

		/// <summary>
		/// Returns the value of tunnel token, id or tag as a string from its file.
		/// </summary>
		static string GetTextFileValue(
			const string& textPath,
			int minLength = 32, 
			int maxLength = 300);

		/// <summary>
		/// Runs 'cloudflared.exe tunnel login'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static void CreateCert();

		/// <summary>
		/// Runs 'cloudflared.exe tunnel token/create <tunnelName>'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static void CreateTunnelCredentials();

		/// <summary>
		/// Runs 'cloudflared.exe tunnel list'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static bool TunnelExists();
		
		/// <summary>
		/// Checks whether the cloudflared service has already been installed or not on this machine.
		/// </summary>
		static bool ServiceExists();

		/// <summary>
		/// Runs 'cloudflared.exe service install <token>'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static void InstallTunnel();

		/// <summary>
		/// Runs 'tunnel run <tunnelName>'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static void RunTunnel();
	};
}