#pragma once

#include <WinSock2.h>
#include <string>
#include <functional>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

namespace KalaServer
{
	using std::function;
	using std::string;
	using std::unique_ptr;
	using RouteHandler = function<string(const string& request)>;

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

		Server(int port = 8080);
		string ServeFile(const string& filePath);
		void Route(const string& path, RouteHandler handler);
		void Run() const;

		void PrintConsoleMessage(ConsoleMessageType type, const string& message);

		void CreatePopup(
			PopupReason reason,
			const string& message);
		void Quit();
	private:
		bool running = false;
		int port = -1;
		mutable SOCKET serverSocket = INVALID_SOCKET;
	};
}