//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <filesystem>
#include <string>

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::Server;
using KalaKit::Core::ConsoleMessageType;
using KalaKit::Core::PopupReason;

using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::path;
using std::string;

namespace KalaKit::DNS
{
	bool CustomDNS::RunDNS()
	{
		if (Server::server == nullptr)
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Cannot initialize dns if server has not yet been initialized!");
			return false;
		}

		if (isInitializing)
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Cannot initialize dns while it is already being initialized!");
			return false;
		}
		isInitializing = true;
		
		if (CloudFlare::IsRunning())
		{
			KalaServer::CreatePopup(
				PopupReason::Reason_Error,
				"Failed to start DNS!"
				"\n\n"
				"Reason:"
				"\n"
				"Cannot run DNS and cloudflared together!");
			isInitializing = false;
			return false;
		}

		isInitializing = false;
		isRunning = true;

		KalaServer::PrintConsoleMessage(
			ConsoleMessageType::Type_Warning,
			"DNS is currently a placeholder! This does nothing.");

		return true;
	}

	void CustomDNS::Quit()
	{
		if (!isRunning)
		{
			KalaServer::PrintConsoleMessage(
				ConsoleMessageType::Type_Error,
				"Cannot shut down dns because it hasn't been started!");
			return;
		}

		KalaServer::PrintConsoleMessage(
			ConsoleMessageType::Type_Message,
			"DNS was successfully shut down!");
	}
}