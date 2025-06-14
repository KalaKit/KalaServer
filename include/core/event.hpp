//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>

namespace KalaKit::Core
{
	using std::string;
	using std::vector;

	enum class EventType
	{
		event_none,

		//server log events

		event_print_message,
		event_print_debug,
		event_print_warning,
		event_print_error,
		event_popup_warning,
		event_popup_error,

		//server action events

		event_server_health_ping,
		event_send_email,
		event_refreshed_whitelisted_ips,
		event_refreshed_banned_ips,
		event_refreshed_blacklisted_keywords,
		event_refreshed_whitelisted_routes,
		event_refreshed_route_access,

		//client events

		event_banned_for_accessing_blacklisted_route,
		event_banned_for_exceeding_rate_limit,
		event_already_banned_client_connected,
		event_client_connected_to_registered_route,
		event_client_connected_to_admin_route
	};

	struct PrintData
	{
		unsigned int indentationLength; //How big of an indentation, if any, should be added?
		bool addTimeStamp;              //Should a timestamp be added?
		string customTag;               //An additional custom tag for extra logging.
		string message;                 //The contents of the message.
	};

	//Send an email to a smtp server like gmail from this server.
	//Uses STARTTLS on port 587
	struct EmailData
	{
		string smtpServer;        //Target email server name (smpt.gmail.com)
		string username;          //Required for SMTP authentication
		string password;          //Required for SMTP authentication
		string sender;            //Who sends the email
		vector<string> receivers; //Everyone who receives the email
		string subject;           //The title of the email
		string body;              //The contents of the email
	};

	class Event
	{
	public:
		//Simply print a new empty line to console
		void PrintNewLine();

		//Print a message to the console
		void SendEvent(EventType type, const PrintData& printData);
		//Create a popup
		void SendEvent(EventType type, const string& message);
		//Send event to email
		void SendEvent(EventType type, const EmailData& emailData);
	};
}