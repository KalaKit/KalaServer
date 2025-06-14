//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <windows.h>
#include <string>
#include <memory>

#include "core/event.hpp"
#include "core/server.hpp"
#include "external/kcrash.hpp"

using KalaKit::KalaCrash::Crash;
using KalaKit::Core::EventType;
using KalaKit::Core::Server;

using std::string;
using std::unique_ptr;
using std::make_unique;

static void CreatePopup(EventType type, const string& message);

namespace KalaKit::Core
{
	void Event::SendEvent(EventType type, const string& message)
	{
		if (type != EventType::event_popup_warning
			&& type != EventType::event_popup_error)
		{
			PrintData pd =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Invalid event type was assigned to popup event!"
			};
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_error, pd);
			return;
		}
		CreatePopup(type, message);
	}
}

static void CreatePopup(EventType type, const string& message)
{
	string popupTitle{};
	string serverName = "Server";
	if (Server::server != nullptr
		&& Server::server->GetServerName() != "")
	{
		serverName = Server::server->GetServerName();
	}

	if (type == EventType::event_popup_error)
	{
		popupTitle = serverName + " error";

		/*
		MessageBoxA(
			nullptr,
			message.c_str(),
			popupTitle.c_str(),
			MB_ICONERROR
			| MB_OK);
		*/

		Crash(message);
	}
	else if (type == EventType::event_popup_warning)
	{
		popupTitle = serverName + "  warning";

		MessageBoxA(
			nullptr,
			message.c_str(),
			popupTitle.c_str(),
			MB_ICONWARNING
			| MB_OK);
	}
}