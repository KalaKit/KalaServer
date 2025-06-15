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
using KalaKit::Core::PopupData;

using KalaKit::Core::sev_m;
using KalaKit::Core::sev_d;
using KalaKit::Core::sev_w;
using KalaKit::Core::sev_e;
using KalaKit::Core::rec_c;
using KalaKit::Core::rec_p;
using KalaKit::Core::rec_e;

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
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CUSTOM_DNS",
				.message = "Cannot initialize dns if server has not yet been initialized!"
			};
			unique_ptr<Event> initEvent = make_unique<Event>();
			initEvent->SendEvent(rec_c, initData);
			return false;
		}

		if (isInitializing)
		{
			PrintData init2Data =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CUSTOM_DNS",
				.message = "Cannot initialize dns while it is already being initialized!"
			};
			unique_ptr<Event> init2Event = make_unique<Event>();
			init2Event->SendEvent(rec_c, init2Data);
			return false;
		}
		isInitializing = true;
		
		if (CloudFlare::IsRunning())
		{
			PopupData cfData =
			{
				.message =
					"Failed to start DNS!"
					"\n\n"
					"Reason:"
					"\n"
					"Cannot run DNS and cloudflared together!",
				.severity = sev_e
			};
			unique_ptr<Event> cfEvent = make_unique<Event>();
			cfEvent->SendEvent(rec_p, cfData);
			isInitializing = false;
			return false;
		}

		isInitializing = false;
		isRunning = true;

		PrintData dnsData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_w,
			.customTag = "CUSTOM_DNS",
			.message = "DNS is currently a placeholder! This does nothing."
		};
		unique_ptr<Event> dnsEvent = make_unique<Event>();
		dnsEvent->SendEvent(rec_c, dnsData);

		return true;
	}

	void CustomDNS::Quit()
	{
		if (!isRunning)
		{
			PrintData sdData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CUSTOM_DNS",
				.message = "Cannot shut down dns because it hasn't been started!"
			};
			unique_ptr<Event> sdEvent = make_unique<Event>();
			sdEvent->SendEvent(rec_c, sdData);
			return;
		}

		PrintData stData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CUSTOM_DNS",
			.message = "DNS was successfully shut down!"
		};
		unique_ptr<Event> stEvent = make_unique<Event>();
		stEvent->SendEvent(rec_c, stData);
	}
}