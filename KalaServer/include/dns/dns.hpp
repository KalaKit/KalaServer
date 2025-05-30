//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

namespace KalaKit::DNS
{
	class CustomDNS
	{
	public:
		/// <summary>
		/// Start up dns. Must not be running cloudflared with this.
		/// Returns true if initialized successfully.
		/// </summary>
		static bool RunDNS();

		static bool IsRunning() { return isRunning; }

		/// <summary>
		/// Shut down dns.
		/// </summary>
		static void Quit();
	private:
		static inline bool isInitializing = false;
		static inline bool isRunning = false;
	};
}