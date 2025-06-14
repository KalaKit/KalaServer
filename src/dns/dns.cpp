//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <filesystem>
#include <string>
#include <memory>

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"
#include "core/event.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::Server;
using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PrintData;

using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::path;
using std::string;
using std::unique_ptr;
using std::make_unique;

namespace KalaKit::DNS
{
	bool CustomDNS::RunDNS()
	{
		if (Server::server == nullptr)
		{
			PrintData initData =
			{
				.indentationLength = 0,
				.addTimeStamp = false,
				.customTag = "CUSTOM_DNS",
				.message = "Cannot initialize dns if server has not yet been initialized!"
			};
			unique_ptr<Event> initEvent = make_unique<Event>();
			initEvent->SendEvent(EventType::event_print_message, initData);
			return false;
		}

		if (isInitializing)
		{
			PrintData init2Data =
			{
				.indentationLength = 0,
				.addTimeStamp = false,
				.customTag = "CUSTOM_DNS",
				.message = "Cannot initialize dns while it is already being initialized!"
			};
			unique_ptr<Event> init2Event = make_unique<Event>();
			init2Event->SendEvent(EventType::event_print_message, init2Data);
			return false;
		}
		isInitializing = true;
		
		if (CloudFlare::IsRunning())
		{
			string popup =
				"Failed to start DNS!"
				"\n\n"
				"Reason:"
				"\n"
				"Cannot run DNS and cloudflared together!";
			unique_ptr<Event> cfEvent = make_unique<Event>();
			cfEvent->SendEvent(EventType::event_popup_error, popup);
			isInitializing = false;
			return false;
		}

		isInitializing = false;
		isRunning = true;

		PrintData dnsData =
		{
			.indentationLength = 0,
			.addTimeStamp = false,
			.customTag = "CUSTOM_DNS",
			.message = "DNS is currently a placeholder! This does nothing."
		};
		unique_ptr<Event> dnsEvent = make_unique<Event>();
		dnsEvent->SendEvent(EventType::event_print_message, dnsData);

		return true;
	}

	void CustomDNS::Quit()
	{
		if (!isRunning)
		{
			PrintData sdData =
			{
				.indentationLength = 0,
				.addTimeStamp = false,
				.customTag = "CUSTOM_DNS",
				.message = "Cannot shut down dns because it hasn't been started!"
			};
			unique_ptr<Event> sdEvent = make_unique<Event>();
			sdEvent->SendEvent(EventType::event_print_message, sdData);
			return;
		}

		PrintData stData =
		{
			.indentationLength = 0,
			.addTimeStamp = false,
			.customTag = "CUSTOM_DNS",
			.message = "DNS was successfully shut down!"
		};
		unique_ptr<Event> stEvent = make_unique<Event>();
		stEvent->SendEvent(EventType::event_print_message, stData);
	}
}