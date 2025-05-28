//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

namespace KalaServer
{
	class Core
	{
	public:
		static void Run() const;

		static void PrintConsoleMessage(
			ConsoleMessageType type, 
			const string& message);

		static void CreatePopup(
			PopupReason reason,
			const string& message);
			
		static void Quit();
	};
}