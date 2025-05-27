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

		Server(int port) : port(port) {}

		static void Initialize(
			int port, 
			const vector<string>& routes, 
			const vector<string>& extensions);
			
		void AddNewWhitelistedRoute(const string& newRoute)
		{
			for (const auto& route : whitelistedRoutes)
			{
				if (route == newRoute)
				{
					PrintConsoleMessage(
						ConsoleMessageType::Type_Warning,
						"Route '" + route + "' has already been whitelisted!");
					return;
				}
			}
			
			whitelistedRoutes.push_back(newRoute);
		}
		void AddNewWhitelistedExtension(const string& newExtension)
		{
			for (const auto& extension : whitelistedExtensions)
			{
				if (extension == newExtension)
				{
					PrintConsoleMessage(
						ConsoleMessageType::Type_Warning,
						"Extension '" + extension + "' has already been whitelisted!");
					return;
				}
			}
			
			whitelistedExtensions.push_back(newExtension);
		}
		
		void RemoveWhitelistedRoute(const string& thisRoute)
		{
			string foundRoute{};
			for (const auto& route : whitelistedRoutes)
			{
				if (route == thisRoute)
				{
					foundRoute = route;
					break;
				}
			}
			
			if (foundRoute == "")
			{
				PrintConsoleMessage(
					ConsoleMessageType::Type_Warning,
					"Route '" + thisRoute + "' cannot be removed because it hasn't been whitelisted!");
			}
		}
		void RemoveWhitelistedExtension(const string& thisExtension)
		{
			string foundExtension{};
			for (const auto& extension : whitelistedExtensions)
			{
				if (extension == thisExtension)
				{
					foundExtension = extension;
					break;
				}
			}
			
			if (foundExtension == "")
			{
				PrintConsoleMessage(
					ConsoleMessageType::Type_Warning,
					"Extension '" + thisExtension + "' cannot be removed because it hasn't been whitelisted!");
			}
		}
		
		bool RouteExists(const string& thisRoute)
		{
			for (const auto& route : whitelistedRoutes)
			{
				if (route == thisRoute) return true;
			}
			return false;
		}
		bool ExtensionExists(const string& thisExtension)
		{
			for (const auto& extension : whitelistedExtensions)
			{
				if (extension == thisExtension) return true;
			}
			return false;
		}
			
		vector<string> GetWhitelistedRoutes() { return whitelistedRoutes; }
		vector<string> GetWhitelistedExtensions() { return whitelistedExtensions; }

		string ServeFile(const string& route);

		void Run() const;

		void PrintConsoleMessage(ConsoleMessageType type, const string& message);

		void CreatePopup(
			PopupReason reason,
			const string& message);
		void Quit();
	private:
		void AddInitialWhitelistedRoutes(const vector<string>& routes);
	
		bool running = false;
		mutable SOCKET serverSocket = INVALID_SOCKET;
		vector<string> whitelistedRoutes{};
		vector<string> whitelistedExtensions{};
		map<string, string> routes;

		int port;
	};
}