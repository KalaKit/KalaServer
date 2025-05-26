//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <WinSock2.h>
#include <string>
#include <memory>
#include <map>

#pragma comment(lib, "ws2_32.lib")

namespace KalaServer
{
	using std::string;
	using std::unique_ptr;
	using std::map;

	enum class PopupReason
	{
		Reason_Error,
		Reason_Warning
	};

	enum class ConsoleMessageType
	{
		Type_Error,
		Type_Warning,
		Type_Message
	};

	class Server
	{
	public:
		static inline unique_ptr<Server> server;

		Server(
			string routeOrigin,
			int port) :
			routeOrigin(routeOrigin),
			port(port) {}

		static void Initialize(
			const string& routeOrigin, 
			int port,
			map<string, string> initialRoutes);

		string ServeFile(const string& route);
		void Route(const string& route, const string& fullPath);

		void Run() const;

		void PrintConsoleMessage(ConsoleMessageType type, const string& message);

		void CreatePopup(
			PopupReason reason,
			const string& message);
		void Quit();
	private:
		bool running = false;
		mutable SOCKET serverSocket = INVALID_SOCKET;

		string routeOrigin;
		int port;
	};
}