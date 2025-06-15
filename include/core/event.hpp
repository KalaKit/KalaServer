//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <variant>

namespace KalaKit::Core
{
	using std::string;
	using std::vector;
	using std::variant;

	struct PrintData;
	struct PopupData;
	struct EmailData;

	//A variant type representing any possible message receiver that can log this event info.
	using ReceiverPayload = variant
	<
		PrintData,
		PopupData,
		EmailData
	>;

	enum class EventType
	{
		event_none,

		//server log events
		event_print_console_message,
		event_create_popup,
		event_severity_message,
		event_severity_debug,
		event_severity_warning,
		event_severity_error,

		//server action events

		event_server_health_ping,
		event_send_email,
		event_refreshed_whitelisted_ips,
		event_refreshed_banned_ips,
		event_refreshed_blacklisted_keywords,
		event_refreshed_whitelisted_routes,
		event_refreshed_route_access,

		//client events

		event_client_was_banned,
		event_already_banned_client_connected,
		event_client_connected_to_registered_route,
		event_client_connected_to_admin_route
	};

	struct PrintData
	{
		unsigned int indentationLength; //How big of an indentation, if any, should be added?
		bool addTimeStamp;              //Should a timestamp be added?
		EventType severity;             //What is the severity of this console message?
		string customTag;               //An additional custom tag for extra logging.
		string message;                 //The contents of the message.
	};
	
	struct PopupData
	{
		string message;
		EventType severity;
	};

	//Send an email to a smtp server like gmail from this server.
	//Uses STARTTLS on port 587
	struct EmailData
	{
		string smtpServer; //Target email server name (smpt.gmail.com)
		string username;   //Required for SMTP authentication
		string password;   //Required for SMTP authentication
		string sender;     //Who sends the email
		vector<string> receivers_email; //Every email recipient who receives the email
		vector<ReceiverPayload> receivers_internal; //Each internal receiver who receives the log about this email
		string subject;    //The title of the email
		string body;       //The contents of the email
	};

	struct Client_Banned_Data
	{
		string ip;                         //What IP got banned
		string reason;                     //Why did they get banned
		vector<ReceiverPayload> receivers; //Who will get a message about this IP being banned
	};

	struct HealthPingData
	{
		unsigned int healthTimer;
		vector<ReceiverPayload> receivers; //Who will get a message about the server health ping
	};

	class Event
	{
	public:
		//Simply print a new empty line to console
		void PrintNewLine();

		//Print a message to the console
		void SendEvent(EventType type, const PrintData& printData);
		//Create a popup
		void SendEvent(EventType type, const PopupData& message);
		//Send an email
		void SendEvent(EventType type, const EmailData& emailData);

		//Get a server health update
		void SendEvent(EventType type, HealthPingData healthPingData);
	};
}