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
#include "response/response_418.hpp"

using KalaKit::Core::Server;
using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PrintData;
using KalaKit::Core::PopupData;
using KalaKit::Core::EmailData;
using KalaKit::Core::BanClientData;
using KalaKit::ResponseSystem::Response_418;

using KalaKit::Core::ReceiverPayload;

using std::string;
using std::to_string;
using std::unique_ptr;
using std::make_unique;
using std::get_if;
using std::pair;
using std::vector;

static void BanClient(EventType type, BanClientData banClientData);

static void PrintType(EventType type, const string& msg)
{
	PrintData pd = {
		.indentationLength = 2,
		.addTimeStamp = true,
		.severity = type,
		.customTag = "BAN-CLIENT",
		.message = msg
	};
	unique_ptr<Event> event = make_unique<Event>();
	event->SendEvent(EventType::event_print_console_message, pd);
};

namespace KalaKit::Core
{
	bool Event::SendEvent(EventType type, BanClientData banClientData)
	{
		if (type != EventType::event_client_was_banned_for_blacklisted_route
			&& type != EventType::event_client_was_banned_for_rate_limit
			&& type != EventType::event_already_banned_client_connected)
		{
			PrintType(
				EventType::event_severity_error,
				"Only event types 'event_client_was_banned_for_blacklisted_route', "
				"'event_client_was_banned_for_rate_limit' and "
				"'event_already_banned_client_connected'"
				"are allowed in 'Ban client' event!");
			return false;
		}
		if (banClientData.ip.empty())
		{
			PrintType(
				EventType::event_severity_error,
				"There must be an ip passed to 'Ban client' event!");
			return false;
		}
		if (banClientData.socket == 0)
		{
			PrintType(
				EventType::event_severity_error,
				"There must be a socket passed to 'Ban client' event!");
			return false;
		}
		if (banClientData.reason.empty())
		{
			PrintType(
				EventType::event_severity_error,
				"There must be a reason passed to 'Ban client' event");
			return false;
		}
		if (banClientData.events.empty())
		{
			PrintType(
				EventType::event_severity_error,
				"There must be atleast one ban reason why you want to send a receiver event from 'Ban client' event!");
			return false;
		}
		bool foundInvalidEvent = false;
		for (const auto& e : banClientData.events)
		{
			if (e.first != EventType::event_client_was_banned_for_blacklisted_route
				&& e.first != EventType::event_client_was_banned_for_rate_limit
				&& e.first != EventType::event_already_banned_client_connected)
			{
				PrintType(
					EventType::event_severity_error,
					"Only event types 'event_client_was_banned_for_blacklisted_route', "
					"'event_client_was_banned_for_rate_limit' and "
					"'event_already_banned_client_connected'"
					"are allowed in events vector for 'Ban client' event!");
				foundInvalidEvent = true;
				break;
			}
		}
		if (foundInvalidEvent) return false;
		BanClient(type, banClientData);
		return true;
	}
}

static void BanClient(EventType type, BanClientData banClientData)
{
	pair<string, string> bannedClient =
	{
		banClientData.ip, banClientData.reason
	};
	bool banSuccess = (Server::server->BanClient(bannedClient));

	if (!banSuccess
		&& (type == EventType::event_client_was_banned_for_blacklisted_route
		|| type == EventType::event_client_was_banned_for_rate_limit))
	{
		return;
	}

	auto respBanned = make_unique<Response_418>();
	respBanned->Init(
		banClientData.socket,
		banClientData.ip,
		banClientData.reason,
		"text/html");

	bool foundThisEvent = false;
	vector<ReceiverPayload> payload{};
	for (const auto& e : banClientData.events)
	{
		if (e.first == type
			&& !e.second.empty())
		{
			payload = e.second;
			foundThisEvent = true;
			break;
		}
	}
	if (!foundThisEvent) return; //silent return because this ban event was not asked for

	string fullData{};
	if (type == EventType::event_already_banned_client_connected)
	{
		fullData = "==== ALREADY BANNED USER ATTEMPTED TO CONNECT ======\n";
	}
	else if (type == EventType::event_client_was_banned_for_blacklisted_route)
	{
		fullData = "======= BANNED CLIENT FOR BLACKLISTED ROUTE ========\n";
	}
	else if (type == EventType::event_client_was_banned_for_rate_limit)
	{
		fullData = "=========== BANNED CLIENT FOR RATE LIMIT ===========\n";
	}
	fullData +=
		" IP     : " + banClientData.ip + "\n"
		" Socket : " + to_string(banClientData.socket) + "\n"
		" Reason : " + banClientData.reason + "\n"
		"====================================================\n";

	for (auto& pl : payload)
	{
		if (PrintData* data = get_if<PrintData>(&pl))
		{
			data->indentationLength = 0;
			data->addTimeStamp = false;
			data->customTag = "";
			data->message = fullData;
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_console_message, *data);
		}
		else if (PopupData* data = get_if<PopupData>(&pl))
		{
			data->message = fullData;
			unique_ptr<Event> fsEvent = make_unique<Event>();
			fsEvent->SendEvent(EventType::event_create_popup, *data);
		}
		else if (EmailData* data = get_if<EmailData>(&pl))
		{
			data->subject =
				type == EventType::event_already_banned_client_connected
				? "Already banned client attempted to reconnect"
				: "Client was banned";
			data->body = fullData;
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_send_email, *data);
		}
	}
}