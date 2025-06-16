//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

namespace KalaKit::Core
{
	using std::string;
	using std::vector;
	using std::variant;
	using std::unordered_map;

	struct PrintData;
	struct PopupData;
	struct EmailData;

	//A variant type representing any possible message receiver that can log this event info.
	//You can always pass an empty PrintData because it will be overwritten,
	//but PopupData and EmailData must be filled except message, body and subject.
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

		event_client_was_banned_for_blacklisted_route,
		event_client_was_banned_for_rate_limit,
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
		string smtpServer;              //Target email server name (smpt.gmail.com)
		string username;                //Required for SMTP authentication
		string password;                //Required for SMTP authentication
		string sender;                  //Who sends the email
		vector<string> receivers_email; //Every email recipient who receives the email
		string subject;                 //The title of the email
		string body;                    //The contents of the email
	};

	//Only sends any receiver events for any ban events
	//if a ban event with its own group of receiver types
	//has been added to the events pair
	struct BanClientData
	{
		string ip;                         //What IP got banned
		uintptr_t socket;                  //The clients socket that will be closed
		string reason;                     //Why did they get banned
		//Each ban event that you want to receive logs for
		//and all the receivers for this ban type
		unordered_map<EventType, vector<ReceiverPayload>> events;
	};

	//Only sends a receiver event if a receiver is added to receivers vector
	struct HealthPingData
	{
		unsigned int healthTimer;          //How often should the health timer ping one of the receivers
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

		//Ban a client
		bool SendEvent(EventType type, BanClientData banClientData);
	};
}