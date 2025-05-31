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
		Type_Message
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

		static void PrintConsoleMessage(
			ConsoleMessageType type, 
			const string& message);

		static void CreatePopup(
			PopupReason reason,
			const string& message);
			
		static void Quit();
	private:

	};
}