//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

namespace KalaKit::Core
{
	using std::string;

	enum class ConsoleMessageType
	{
		Type_Error,
		Type_Warning,
		Type_Message,
		Type_Debug
	};

	enum class PopupReason
	{
		Reason_Error,
		Reason_Warning
	};

	class KalaServer
	{
	public:
		//Is the server currently running
		static inline bool isRunning = false;

		static bool IsRunningAsAdmin();

		/// <summary>
		/// Runs the server every frame.
		/// </summary>
		static void Run();

		/// <summary>
		/// Prints a message to the server console
		/// </summary>
		/// <param name="indentationLength">How big of an indentation, if any, should be added?</param>
		/// <param name="addTimeStamp">Should a timestamp be added?</param>
		/// <param name="type">What type of a message is this?</param>
		/// <param name="customTag">An additional custom tag for extra logging.</param>
		/// <param name="message">The contents of the message.</param>
		static void PrintConsoleMessage(
			unsigned int indentationLength,
			bool addTimeStamp,
			ConsoleMessageType type,
			const string& customTag,
			const string& message);

		static void CreatePopup(
			PopupReason reason,
			const string& message);
			
		static void Quit();
	private:

	};
}