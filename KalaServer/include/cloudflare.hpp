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
			const string& tunnelName,
			const string& tunnelTokenFilePath);

		static bool IsRunning() { return isRunning; }

		/// <summary>
		/// Shut down cloudflared.
		/// </summary>
		static void Quit();
	private:
		static inline bool isInitializing = false;
		static inline bool isRunning = false;
		
		static inline string tunnelName;
		static inline string tunnelToken;
		static inline string tunnelTokenFilePath;
		
		/// <summary>
		/// Returns the tunnel token as a string from its file.
		/// </summary>
		static string GetTunnelTokenText();

		/// <summary>
		/// Trim the tunnel token from any unexpected newlines or spaces.
		/// </summary>
		/// <param name="newTunnelToken"></param>
		/// <returns></returns>
		static string TrimTunnelToken(const string& newTunnelToken);

		/// <summary>
		/// Runs 'cloudflared.exe tunnel list'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static bool TunnelExists();

		/// <summary>
		/// Runs 'cloudflared.exe service install <token>'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		static void CreateTunnel();

		/// <summary>
		/// Runs 'tunnel run kalaserver'
		/// through cloudflared.exe as a separate process.
		/// </summary>
		/// <returns></returns>
		static void RunTunnel();
	};
}