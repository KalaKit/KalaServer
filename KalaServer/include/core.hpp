//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

namespace KalaServer
{
	using std::string;

	enum class ConsoleMessageType
	{
		Type_Error,
		Type_Warning,
		Type_Message
	};

	enum class PopupReason
	{
		Reason_Error,
		Reason_Warning
	};

	class Core
	{
	public:
		//Is the server currently running
		static inline bool isRunning = false;

		static bool Run();

		static bool IsRunning() { return isRunning; }

		static void PrintConsoleMessage(
			ConsoleMessageType type, 
			const string& message);

		static void CreatePopup(
			PopupReason reason,
			const string& message);
			
		static void Quit();
	};
}