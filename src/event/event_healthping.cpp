//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <string>
#include <memory>
#include <vector>

#include "core/core.hpp"
#include "core/event.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::EmailData;
using KalaKit::Core::PrintData;
using KalaKit::Core::MessageReceiver;
using KalaKit::Core::Server;
using KalaKit::DNS::CloudFlare;

using std::string;
using std::unique_ptr;
using std::make_unique;
using std::vector;

static void HealthPing(EventType type, const vector<MessageReceiver>& receivers);

namespace KalaKit::Core
{
	void Event::SendEvent(EventType type, const vector<MessageReceiver>& receivers)
	{
		if (type != EventType::event_server_health_ping)
		{
			PrintData pd =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Invalid event type was assigned to 'health ping' event!"
			};
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_error, pd);
			return;
		}
		HealthPing(type, receivers);
	}
}

static void HealthPing(EventType type, const vector<MessageReceiver>& receivers)
{
	bool hasInternet = Server::server->HasInternet();
	bool isTunnelAlive = Server::server->IsTunnelAlive(CloudFlare::tunnelRunHandle);

	string internetStatus = hasInternet ? "[NET: OK]" : "[NET: FAIL]";
	string tunnelStatus = isTunnelAlive ? "[TUNNEL: OK]" : "[TUNNEL: FAIL]";

	string fullStatus =
		"Server status: " + internetStatus
		+ ", " + tunnelStatus;

	PrintData fsData =
	{
		.indentationLength = 2,
		.addTimeStamp = true,
		.customTag = "SERVER",
		.message = fullStatus + "\n"
	};
	unique_ptr<Event> fsEvent = make_unique<Event>();
	fsEvent->SendEvent(EventType::event_print_message, fsData);

	vector<string> theReceivers = { Server::server->emailSenderData.username };
	EmailData emailData =
	{
		.smtpServer = "smtp.gmail.com",
		.username = Server::server->emailSenderData.username,
		.password = Server::server->emailSenderData.password,
		.sender = Server::server->emailSenderData.username,
		.receivers = theReceivers,
		.subject = Server::server->GetServerName() + " health status",
		.body = fullStatus
	};

	//unique_ptr<Event> event = make_unique<Event>();
	//event->SendEvent(EventType::event_server_health_ping, server->emailData);
}