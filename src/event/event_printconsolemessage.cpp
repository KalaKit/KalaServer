//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <iostream>
#include <chrono>
#include <string>
#include <memory>

#include "core/event.hpp"

using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PrintData;

using std::cout;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::chrono::system_clock;
using std::time_t;
using std::tm;
using std::snprintf;
using std::string;
using std::unique_ptr;
using std::make_unique;

static void PrintConsoleMessage(EventType eventType, const PrintData& printData);

namespace KalaKit::Core
{
	void Event::PrintNewLine()
	{
		PrintData emptyData =
		{
			.indentationLength = 0,
			.addTimeStamp = false,
			.severity = EventType::event_severity_error,
			.customTag = "",
			.message = ""
		};
		PrintConsoleMessage(EventType::event_print_console_message, emptyData);
	}

	void Event::SendEvent(EventType type, const PrintData& printData)
	{
		if (type != EventType::event_print_console_message)
		{
			PrintData pd =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = EventType::event_severity_error,
				.customTag = "SERVER",
				.message = "Only event type 'event_print_console_message' is allowed in 'Print to console' event!"
			};
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_console_message, pd);
			return;
		}
		if (printData.severity != EventType::event_severity_message
			&& printData.severity != EventType::event_severity_debug
			&& printData.severity != EventType::event_severity_warning
			&& printData.severity != EventType::event_severity_error)
		{
			PrintData pd =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = EventType::event_severity_error,
				.customTag = "SERVER",
				.message = "Invalid severity type was passed to 'Print to console' event!"
			};
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_console_message, pd);
			return;
		}
		PrintConsoleMessage(type, printData);
	}
}

static void PrintConsoleMessage(EventType eventType, const PrintData& printData)
{
	string result{};
	string indentationContent{};
	string customTagContent{};
	string timeStampContent{};
	string targetTypeContent{};

#ifndef _DEBUG
	if (eventType == EventType::event_severity_debug) return;
#endif

	if (printData.indentationLength > 0)
	{
		indentationContent = string(printData.indentationLength, ' ');
	}

	if (printData.addTimeStamp)
	{
		auto now = system_clock::now();
		auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
		time_t now_c = system_clock::to_time_t(now);
		tm localTime{};
		localtime_s(&localTime, &now_c);

		char timeBuffer[16]{};
		snprintf(
			timeBuffer,
			sizeof(timeBuffer),
			"[%02d:%02d:%02d:%03lld] ",
			localTime.tm_hour,
			localTime.tm_min,
			localTime.tm_sec,
			static_cast<long long>(ms.count()));

		timeStampContent = timeBuffer;
	}

	if (printData.customTag != "") customTagContent = "[" + printData.customTag + "] ";

	switch (eventType)
	{
	case EventType::event_severity_error:
		targetTypeContent = "[ERROR] ";
		break;
	case EventType::event_severity_warning:
		targetTypeContent = "[WARNING] ";
		break;
	case EventType::event_severity_debug:
		targetTypeContent = "[DEBUG] ";
		break;
	}

	result =
		indentationContent
		+ timeStampContent
		+ customTagContent
		+ targetTypeContent
		+ printData.message;

	cout << result + "\n";
}