//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <WinSock2.h>
#include <string>
#include <memory>
#include <map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace KalaServer
{
	using std::string;
	using std::unique_ptr;
	using std::map;
	using std::vector;

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
			int port,
			string whitelistedRoutesFolder,
			vector<string> whitelistedExtensions)
			: port(port),
			whitelistedRoutesFolder(whitelistedRoutesFolder),
			whitelistedExtensions(whitelistedExtensions) {}

		static void Initialize(
			int port,
			const string& whitelistedRoutesFolder,
			const vector<string>& extensions);
			
		void AddNewWhitelistedRoute(const string& rootPath, const string& filePath);
		void AddNewWhitelistedExtension(const string& newExtension);
		
		void RemoveWhitelistedRoute(const string& thisRoute);
		void RemoveWhitelistedExtension(const string& thisExtension);
		
		bool RouteExists(const string& thisRoute)
		{
			return whitelistedRoutes.contains(thisRoute);
		}
		bool ExtensionExists(const string& thisExtension)
		{
			for (const auto& extension : whitelistedExtensions)
			{
				if (extension == thisExtension) return true;
			}
			return false;
		}
			
		map<string, string> GetWhitelistedRoutes() { return whitelistedRoutes; }
		vector<string> GetWhitelistedExtensions() { return whitelistedExtensions; }

		string ServeFile(const string& route);

		void Run() const;

		void PrintConsoleMessage(ConsoleMessageType type, const string& message);

		void CreatePopup(
			PopupReason reason,
			const string& message);
		void Quit();
	private:
		void AddInitialWhitelistedRoutes();
	
		bool running = false; //Is the server currently running
		mutable SOCKET serverSocket = INVALID_SOCKET; //Current active socket
		map<string, string> whitelistedRoutes{}; //All routes that are allowed to be accessed

		int port; //Local server port
		string whitelistedRoutesFolder; //The folder path relative to the server where all pages are inside of.
		vector<string> whitelistedExtensions; //All extensions that are allowed to be accessed
	};
}