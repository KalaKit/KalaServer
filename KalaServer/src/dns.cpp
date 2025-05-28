//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <filesystem>
#include <string>

#include "core.hpp"
#include "dns.hpp"
#include "cloudflare.hpp"

using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::path;
using std::string;

namespace KalaServer
{
	bool DNS::RunDNS()
	{
		if (CloudFlare::IsRunning())
		{
			Core::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start DNS!"
				"\n\n"
				"Reason:"
				"\n"
				"Cannot run DNS and cloudflared together!");
			return false;
		}

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Warning,
			"DNS is currently a placeholder! This does nothing.");

		isRunning = true;
		return true;
	}

	void DNS::Quit()
	{
		if (!isRunning)
		{
			Core::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Cannot shut down dns because it hasn't been started!");
			return;
		}

		Core::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"DNS was successfully shut down!");
	}
}