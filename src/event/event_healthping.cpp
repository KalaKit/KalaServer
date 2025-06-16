//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <memory>

#include "core/core.hpp"
#include "core/event.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"

using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PrintData;
using KalaKit::Core::PopupData;
using KalaKit::Core::EmailData;
using KalaKit::Core::HealthPingData;
using KalaKit::Core::Server;
using KalaKit::DNS::CloudFlare;

using std::string;
using std::unique_ptr;
using std::make_unique;
using std::get_if;

static void HealthPing(EventType type, HealthPingData healthPingData);

static void PrintType(EventType type, const string& msg)
{
	PrintData pd = {
		.indentationLength = 2,
		.addTimeStamp = true,
		.severity = type,
		.customTag = "HEALTH_PING",
		.message = msg
	};
	unique_ptr<Event> event = make_unique<Event>();
	event->SendEvent(EventType::event_print_console_message, pd);
};

namespace KalaKit::Core
{
	void Event::SendEvent(EventType type, HealthPingData healthPingData)
	{
		if (type != EventType::event_server_health_ping)
		{
			PrintType(
				EventType::event_severity_error,
				"Only event type 'event_server_health_ping' is allowed in 'Health ping' event!");
			return;
		}
		if (healthPingData.healthTimer == 0)
		{
			PrintType(
				EventType::event_severity_error,
				"Health timer must be higher than 0 in 'Health ping' event!");
			return;
		}
		if (healthPingData.healthTimer < 0.5f)
		{
			PrintType(
				EventType::event_severity_warning,
				"Health timer is extremely low in 'Health ping' event! Please consider setting it as a value of 0.5 or higher.");
		}
		HealthPing(type, healthPingData);
	}
}

static void HealthPing(EventType type, HealthPingData healthPingData)
{
	bool hasInternet = Server::server->HasInternet();
	bool isTunnelAlive = Server::server->IsTunnelAlive(CloudFlare::tunnelRunHandle);

	string internetStatus = hasInternet ? "[NET: OK]" : "[NET: FAIL]";
	string tunnelStatus = isTunnelAlive ? "[TUNNEL: OK]" : "[TUNNEL: FAIL]";

	string fullStatus =
		"Server status: " + internetStatus
		+ ", " + tunnelStatus;

	for (auto& payload : healthPingData.receivers)
	{
		if (PrintData* data = get_if<PrintData>(&payload))
		{
			data->message = fullStatus;
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_console_message, *data);
		}
		else if (PopupData* data = get_if<PopupData>(&payload))
		{
			data->message = fullStatus;
			unique_ptr<Event> fsEvent = make_unique<Event>();
			fsEvent->SendEvent(EventType::event_create_popup, *data);
		}
		else if (EmailData* data = get_if<EmailData>(&payload))
		{
			data->body = fullStatus;
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_send_email, *data);
		}
	}
}