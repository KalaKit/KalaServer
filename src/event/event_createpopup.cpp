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
using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PopupData;
using KalaKit::Core::Server;

using std::string;
using std::unique_ptr;
using std::make_unique;

static void CreatePopup(EventType type, const PopupData& popupData);

namespace KalaKit::Core
{
	void Event::SendEvent(EventType type, const PopupData& popupData)
	{
		if (type != EventType::event_print_console_message)
		{
			PrintData pd =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.customTag = "SERVER",
				.message = "Only event type 'event_print_console_message' is allowed in 'Create popup' event!"
			};
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_severity_error, pd);
			return;
		}
		CreatePopup(type, popupData);
	}
}

static void CreatePopup(EventType type, const PopupData& popupData)
{
	string popupTitle{};
	string serverName = "Server";
	if (Server::server != nullptr
		&& Server::server->GetServerName() != "")
	{
		serverName = Server::server->GetServerName();
	}

	if (type == EventType::event_severity_error)
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

		Crash(popupData.message);
	}
	else if (type == EventType::event_severity_warning)
	{
		popupTitle = serverName + "  warning";

		MessageBoxA(
			nullptr,
			popupData.message.c_str(),
			popupTitle.c_str(),
			MB_ICONWARNING
			| MB_OK);
	}
}