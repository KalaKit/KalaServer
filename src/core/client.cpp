//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <wininet.h>
#include <sstream>
#include <algorithm>
#include <filesystem>

#include "core/client.hpp"
#include "core/core.hpp"
#include "core/server.hpp"
#include "core/event.hpp"
#include "response/response_200.hpp"
#include "response/response_206.hpp"
#include "response/response_404.hpp"
#include "response/response_403.hpp"
#include "response/response_418.hpp"
#include "response/response_500.hpp"

using KalaKit::ResponseSystem::Response_200;
using KalaKit::ResponseSystem::Response_206;
using KalaKit::ResponseSystem::Response_404;
using KalaKit::ResponseSystem::Response_403;
using KalaKit::ResponseSystem::Response_500;
using KalaKit::ResponseSystem::Response_418;

using std::istringstream;
using std::exception;
using std::transform;
using std::to_string;
using std::thread;
using std::lock_guard;
using std::make_unique;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;
using std::filesystem::path;

namespace KalaKit::Core
{
	string Client::ExtractHeaderValue(
		const string& request,
		const string& headerName)
	{
		istringstream stream(request);
		string line;
		string prefix = headerName + ": ";

		while (getline(stream, line))
		{
			if (line.substr(0, prefix.size()) == prefix)
			{
				return line.substr(prefix.size());
			}
		}
		return "";
	}

	string Client::ExtractHeader(
		const string& request,
		const string& headerName)
	{
		string result{};
		istringstream stream(request);

		string line{};
		while (getline(stream, line))
		{
			size_t colonPos = line.find(':');
			if (colonPos != string::npos)
			{
				string key = line.substr(0, colonPos);
				string value = line.substr(colonPos + 1);

				//trim whitespace

				key.erase(remove_if(key.begin(), key.end(), ::isspace), key.end());
				value.erase(0, value.find_first_not_of(" \t\r\n"));
				value.erase(value.find_last_not_of(" \t\r\n") + 1);

				//case-insensitive compare

				transform(key.begin(), key.end(), key.begin(), ::tolower);
				string lowerHeader = headerName;
				transform(lowerHeader.begin(), lowerHeader.end(), lowerHeader.begin(), ::tolower);

				if (key == lowerHeader)
				{
					size_t comma = value.find(',');
					if (comma != string::npos)
					{
						value = value.substr(0, comma);
					}

					return value;
				}
			}
		}
		return result;
	}

	void Client::ParseByteRange(
		const string& header,
		size_t& outStart,
		size_t& outEnd)
	{
		if (header.empty()
			|| header.find("bytes=") != 0)
		{
			return;
		}

		//skip "bytes="
		string rangePart = header.substr(6);
		size_t dash = rangePart.find('-');
		if (dash == string::npos) return;

		string startStr = rangePart.substr(0, dash);
		string endStr = rangePart.substr(dash + 1);

		size_t startFirst = startStr.find_first_not_of(" \r\n\t");
		size_t startLast = startStr.find_last_not_of(" \r\n\t");
		if (startFirst != string::npos
			&& startLast != string::npos)
		{
			startStr = startStr.substr(startFirst, startLast - startFirst + 1);
		}
		else startStr.clear();

		size_t endFirst = endStr.find_first_not_of(" \r\n\t");
		size_t endLast = endStr.find_last_not_of(" \r\n\t");
		if (endFirst != string::npos
			&& endLast != string::npos)
		{
			endStr = endStr.substr(endFirst, endLast - endFirst + 1);
		}
		else endStr.clear();

		try
		{
			if (startStr.empty())
			{
				PrintData sbData =
				{
					.indentationLength = 2,
					.addTimeStamp = true,
					.severity = sev_e,
					.customTag = "CLIENT",
					.message = "Start byte in ParseByeRange is missing for header '" + header + "'!"
				};
				unique_ptr<Event> sbEvent = make_unique<Event>();
				sbEvent->SendEvent(EventType::event_severity_error, sbData);
				return;
			}

			outStart = static_cast<size_t>(stoull(startStr));
			outEnd = endStr.empty() ? 0 : static_cast<size_t>(stoull(endStr));
			return;
		}
		catch (const exception& e)
		{
			PrintData hbData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CLIENT",
				.message = 
					"Failed to parse header byte range for header '" + header
					+ "'!\nReason: " + e.what()
			};
			unique_ptr<Event> hbEvent = make_unique<Event>();
			hbEvent->SendEvent(EventType::event_severity_error, hbData);
			return;
		}
	}

	void Client::HandleClient(uintptr_t socket)
	{
		//
		// START CLIENT CONNECTION
		//

		PrintData seData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_d,
			.customTag = "CLIENT",
			.message = "Socket [" + to_string(socket) + "] entered handle client thread..."
		};
		unique_ptr<Event> seEvent = make_unique<Event>();
		seEvent->SendEvent(EventType::event_severity_debug, seData);

		SOCKET rawClientSocket = static_cast<SOCKET>(socket);
		uintptr_t clientSocket = static_cast<uintptr_t>(rawClientSocket);

		DWORD timeout = 1000;
		setsockopt(
			rawClientSocket,
			SOL_SOCKET,
			SO_RCVTIMEO,
			(char*)&timeout,
			sizeof(timeout));

		{
			lock_guard socketInsertLock(Server::server->clientSocketsMutex);
			Server::server->activeClientSockets.insert(socket);
		}

		char buffer[2048] = {};
		int bytesReceived = recv(rawClientSocket, buffer, sizeof(buffer) - 1, 0);

		if (bytesReceived == SOCKET_ERROR)
		{
			//
			// INVALID SOCKET CONNECTION - CLOSE SOCKET
			//

			DWORD err = WSAGetLastError();
			if (err == WSAETIMEDOUT)
			{
				PrintData stData =
				{
					.indentationLength = 2,
					.addTimeStamp = true,
					.severity = sev_w,
					.customTag = "CLIENT",
					.message = "Socket [" + to_string(socket) + "] timed out after 5 seconds without sending any data."
				};
				unique_ptr<Event> stEvent = make_unique<Event>();
				stEvent->SendEvent(EventType::event_severity_warning, stData);
			}
			else
			{
				PrintData srData =
				{
					.indentationLength = 2,
					.addTimeStamp = true,
					.severity = sev_w,
					.customTag = "CLIENT",
					.message = "Socket [" + to_string(socket) + "] recv() failed with error: " + to_string(err)
				};
				unique_ptr<Event> srEvent = make_unique<Event>();
				srEvent->SendEvent(EventType::event_severity_warning, srData);
			}

			this->SocketCleanup(socket);
			closesocket(rawClientSocket);
			return;
		}
		else if (bytesReceived == 0)
		{
			//
			// EMPTY SOCKET - CLOSE SOCKET
			//

			PrintData sdData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_w,
				.customTag = "CLIENT",
				.message = "Socket [" + to_string(socket) + "] disconnected without sending any data."
			};
			unique_ptr<Event> sdEvent = make_unique<Event>();
			sdEvent->SendEvent(EventType::event_severity_warning, sdData);

			this->SocketCleanup(socket);
			closesocket(rawClientSocket);
			return;
		}

		string request(buffer);
		string route = "/";

		PrintData shData =
		{
			.indentationLength = 2,
			.addTimeStamp = true,
			.severity = sev_d,
			.customTag = "CLIENT",
			.message = "Socket [" + to_string(socket) + "] raw HTTP request:\n" + request
		};
		unique_ptr<Event> shEvent = make_unique<Event>();
		shEvent->SendEvent(EventType::event_severity_debug, shData);

		if (request.starts_with("GET "))
		{
			size_t pathStart = 4;
			size_t pathEnd = request.find(' ', pathStart);
			if (pathEnd != string::npos)
			{
				route = request.substr(pathStart, pathEnd - pathStart);
			}
		}

		string cleanRoute = route;
		size_t q = cleanRoute.find('?');
		if (q != string::npos)
		{
			cleanRoute = cleanRoute.substr(0, q);
		}

		string clientIP = this->ExtractHeader(
			request,
			"Cf-Connecting-Ip");

		if (clientIP.empty())
		{
			clientIP = "unknown-ip";

			PrintData frData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CLIENT",
				.message = 
					"Failed to get client IP for socket [" + to_string(socket) + "]!"
					"\n"
					"Full request dump :"
					"\n" +
					request
			};
			unique_ptr<Event> frEvent = make_unique<Event>();
			frEvent->SendEvent(EventType::event_severity_warning, frData);
		}

		bool isHost = Server::server->IsHost(clientIP);
		if (isHost)
		{
			PrintData ciData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.severity = sev_m,
				.customTag = "CLIENT",
				.message = "Client [" + to_string(socket) + " - '" + clientIP + "'] is server host."
			};
			unique_ptr<Event> ciEvent = make_unique<Event>();
			ciEvent->SendEvent(EventType::event_severity_message, ciData);
			clientIP = "host";

			//always update routes and their access levels if host joins any route
			Server::server->canUpdateWhitelistedRoutes = true;
			Server::server->canUpdateRouteAccess = true;
		}

		pair<string, string> whitelistedClient = Server::server->IsWhitelistedClient(clientIP);
		pair<string, string> bannedClient = Server::server->IsBannedClient(clientIP);

		//
		// CHECK IF CLIENT IS WHITELISTED OR BANNED
		//

		if (isHost
			|| whitelistedClient.first != "")
		{
			PrintData wcData =
			{
				.indentationLength = 0,
				.addTimeStamp = false,
				.severity = sev_m,
				.customTag = "",
				.message = 
					"=============== WHITELISTED CLIENT =================\n"
					" IP     : " + clientIP + "\n"
					" Socket : " + to_string(clientSocket) + "\n"
					" Reason : " + whitelistedClient.second + "\n"
					"====================================================\n"
			};
			unique_ptr<Event> wcEvent = make_unique<Event>();
			wcEvent->SendEvent(EventType::event_severity_message, wcData);
		}
		else
		{
			if (bannedClient.first != "")
			{
				PrintData buData =
				{
					.indentationLength = 0,
					.addTimeStamp = false,
					.severity = sev_m,
					.customTag = "",
					.message =
						"======= BANNED USER REPEATED ACCESS ATTEMPT ========\n"
						" IP     : " + clientIP + "\n"
						" Socket : " + to_string(clientSocket) + "\n"
						" Reason : " + bannedClient.second + "\n"
						"====================================================\n"
				};
				unique_ptr<Event> buEvent = make_unique<Event>();
				buEvent->SendEvent(EventType::event_severity_message, buData);

				sleep_for(seconds(30));

				vector<EventType> ev = Server::server->emailSenderData.events;
				EventType e = EventType::event_already_banned_client_connected;
				bool bannedClientReconnectedEvent =
					find(ev.begin(), ev.end(), e) != ev.end();

				if (bannedClientReconnectedEvent)
				{
					vector<string> receivers_email = { Server::server->emailSenderData.username };
					EmailData emailData =
					{
						.smtpServer = "smtp.gmail.com",
						.username = Server::server->emailSenderData.username,
						.password = Server::server->emailSenderData.password,
						.sender = Server::server->emailSenderData.username,
						.receivers_email = receivers_email,
						.subject = "Banned client reconnected",
						.body = "Already banned client " + clientIP + " tried to reconnect to  '" + cleanRoute + "'!"
					};
					unique_ptr<Event> event = make_unique<Event>();
					event->SendEvent(e, emailData);
				}

				auto respBanned = make_unique<Response_418>();
				respBanned->Init(
					rawClientSocket,
					clientIP,
					cleanRoute,
					"text/html");
				this->SocketCleanup(socket);
				return;
			}
			else if (bannedClient.first == ""
				&& !Server::server->blacklistedKeywords.empty()
				&& Server::server->IsBlacklistedRoute(cleanRoute))
			{
				PrintData bcData =
				{
					.indentationLength = 0,
					.addTimeStamp = false,
					.severity = sev_m,
					.customTag = "",
					.message =
						"=============== BANNED CLIENT ======================\n"
						" IP     : " + clientIP + "\n"
						" Socket : " + to_string(clientSocket) + "\n"
						" Reason : " + cleanRoute + "\n"
						"====================================================\n"
				};
				unique_ptr<Event> bcEvent = make_unique<Event>();
				bcEvent->SendEvent(EventType::event_severity_message, bcData);

				sleep_for(milliseconds(5));

				vector<EventType> ev = Server::server->emailSenderData.events;
				EventType e = EventType::event_client_was_banned;
#pragma warning(push)
#pragma warning(disable: 26117)
				bool clientWasBannedEvent =
					find(ev.begin(), ev.end(), e) != ev.end(); //"Releasing unheld lock" false positive
#pragma warning(pop)

				if (clientWasBannedEvent)
				{
					vector<string> receivers_email = { Server::server->emailSenderData.username };
					EmailData emailData =
					{
						.smtpServer = "smtp.gmail.com",
						.username = Server::server->emailSenderData.username,
						.password = Server::server->emailSenderData.password,
						.sender = Server::server->emailSenderData.username,
						.receivers_email = receivers_email,
						.subject = "Client connected to blacklisted route",
						.body = "client " + clientIP + " was banned because they tried to access route '" + cleanRoute + "'!"
					};
					unique_ptr<Event> event = make_unique<Event>();
					event->SendEvent(e, emailData);
				}

				pair<string, string> bannedClient{};
				bannedClient.first = clientIP;
				bannedClient.second = cleanRoute;

				if (Server::server->BanClient(bannedClient))
				{
					auto respBanned = make_unique<Response_418>();
					respBanned->Init(
						clientSocket,
						clientIP,
						cleanRoute,
						"text/html");
				}

				this->SocketCleanup(socket);
				return;
			}
		}

		if (Server::server->rateLimitTimer > 0
			&& !isHost
			&& whitelistedClient.first == "")
		{
			lock_guard<mutex> counterLock(counterMutex);
			int& count = requestCounter[clientIP][route];
			count++;

			if (count > Server::server->rateLimitTimer)
			{
				string reason = "Exceeded allowed connection rate limit of " + to_string(Server::server->rateLimitTimer) + " times per second";
				PrintData srData =
				{
					.indentationLength = 0,
					.addTimeStamp = false,
					.severity = sev_m,
					.customTag = "",
					.message = 
						"=============== BANNED CLIENT ======================\n"
						" IP     : " + clientIP + "\n"
						" Socket : " + to_string(clientSocket) + "\n"
						" Reason : " + reason + "\n"
						"====================================================\n"
				};
				unique_ptr<Event> srEvent = make_unique<Event>();
				srEvent->SendEvent(EventType::event_severity_message, srData);

				sleep_for(milliseconds(5));

				vector<EventType> ev = Server::server->emailSenderData.events;
				EventType e = EventType::event_client_was_banned;
#pragma warning(push)
#pragma warning(disable: 26110)
				bool clientWasBannedEvent =
					find(ev.begin(), ev.end(), e) != ev.end(); //"Releasing unheld lock" false positive
#pragma warning(pop)

				if (clientWasBannedEvent)
				{
					vector<string> receivers_email = { Server::server->emailSenderData.username };
					EmailData emailData =
					{
						.smtpServer = "smtp.gmail.com",
						.username = Server::server->emailSenderData.username,
						.password = Server::server->emailSenderData.password,
						.sender = Server::server->emailSenderData.username,
						.receivers_email = receivers_email,
						.subject = "Client exceeded rate limit in KalaKit website",
						.body = "client " + clientIP + " was banned because they exceeded the rate limit of '" + to_string(Server::server->rateLimitTimer) + "'!"
					};
					unique_ptr<Event> event = make_unique<Event>();
					event->SendEvent(e, emailData);
				}

				pair<string, string> bannedClient{};
				bannedClient.first = clientIP;
				bannedClient.second = reason;

				if (Server::server->BanClient(bannedClient))
				{
					auto respBanned = make_unique<Response_418>();
					respBanned->Init(
						clientSocket,
						clientIP,
						cleanRoute,
						"text/html");
				}

				this->SocketCleanup(socket);
				return;
			}
		}

		PrintData scData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLIENT",
			.message = "New client successfully connected [" + to_string(socket) + " - '" + clientIP + "']!"
		};
		unique_ptr<Event> scEvent = make_unique<Event>();
		scEvent->SendEvent(EventType::event_severity_message, scData);

		string body{};
		string statusLine = "HTTP/1.1 200 OK";

		//
		// CHECK IF ROUTE EXISTS
		//

		Server::server->GetWhitelistedRoutes();

		Route foundRoute{};
		for (const auto& r : Server::server->whitelistedRoutes)
		{
			if (r.route == cleanRoute)
			{
				foundRoute = r;
				break;
			}
		}

		if (foundRoute.route.empty())
		{
			PrintData caData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_w,
				.customTag = "CLIENT",
				.message = 
					"Client ["
					+ to_string(socket) + " - '" + clientIP + "'] tried to access non-existing route '"
					+ cleanRoute + "'!"
			};
			unique_ptr<Event> caEvent = make_unique<Event>();
			caEvent->SendEvent(EventType::event_severity_message, caData);

			auto resp404 = make_unique<Response_404>();
			resp404->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(socket);
#pragma warning(push)
#pragma warning(disable: 26117) //"Releasing unheld lock" false positive
			return;
#pragma warning(pop)
		}

		bool extentionExists = false;
		for (const auto& ext : Server::server->whitelistedExtensions)
		{
			if (path(cleanRoute).extension().generic_string() == ext)
			{
				extentionExists = true;
				break;
			}
		}

		//
		// CHECK IF ROUTE EXTENSION IS ALLOWED
		//

		bool isAllowedFile =
			cleanRoute.find_last_of('.') == string::npos
			|| extentionExists;

		if (!isAllowedFile)
		{
			PrintData ttData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_w,
				.customTag = "CLIENT",
				.message =
					"Client ["
					+ to_string(socket) + " - '" + clientIP + "'] tried to access forbidden route '" + cleanRoute
					+ "' from path '" + foundRoute.route + "'."
			};
			unique_ptr<Event> ttEvent = make_unique<Event>();
			ttEvent->SendEvent(EventType::event_severity_warning, ttData);

			auto resp403 = make_unique<Response_403>();
			resp403->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(socket);
#pragma warning(push)
#pragma warning(disable: 26117) //"Releasing unheld lock" false positive
			return;
#pragma warning(pop)
		}

		//
		// CHECK IF CLIENT AND ROUTE AUTH LEVEL ARE THE SAME
		//

		bool isRegisteredRoute = Server::server->IsRegisteredRoute(route);
		bool isAdminRoute = Server::server->IsAdminRoute(route);

		bool isClientRegistered = false;
		bool isClientAdmin = false;

		bool canAccessRegisteredRoute =
			isRegisteredRoute
			&& (isClientRegistered
			|| isClientAdmin
			|| isHost);

		bool canAccessAdminRoute =
			isAdminRoute
			&& (isClientAdmin
			|| isHost);

		bool allowEntrance =
			(!isRegisteredRoute
			&& !isAdminRoute)
			|| canAccessRegisteredRoute
			|| canAccessAdminRoute;

		string routeAuth = "[USER]";
		string clientAuth = "[USER]";

		if (isRegisteredRoute) routeAuth = "[REGISTERED]";
		if (isAdminRoute) routeAuth = "[ADMIN]";

		if (isClientRegistered) clientAuth = "[REGISTERED]";
		if (isClientAdmin || isHost) clientAuth = "[ADMIN]";

		if (!allowEntrance)
		{
			PrintData alData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_w,
				.customTag = "CLIENT",
				.message =
					"Client ["
					+ to_string(socket) + " - '" + clientIP + "'] with insufficient auth level " + clientAuth
					+ " tried to access route '" + cleanRoute
					+ "' with auth level " + routeAuth + "!"
			};
			unique_ptr<Event> alEvent = make_unique<Event>();
			alEvent->SendEvent(EventType::event_severity_warning, alData);

			auto resp403 = make_unique<Response_403>();
			resp403->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(socket);
#pragma warning(push)
#pragma warning(disable: 26117) //"Releasing unheld lock" false positive
			return;
#pragma warning(pop)
		}

		if ((isClientRegistered
			|| isClientAdmin
			|| isHost)
			&& isRegisteredRoute)
		{
			PrintData prData =
			{
				.indentationLength = 0,
				.addTimeStamp = false,
				.severity = sev_m,
				.customTag = "",
				.message =
					"==== APPROVED CLIENT CONNECTED TO PRIVATE ROUTE ====\n"
					" IP     : " + clientIP + "\n"
					" Socket : " + to_string(clientSocket) + "\n"
					" Reason : " + cleanRoute + "\n"
					"====================================================\n"
			};
			unique_ptr<Event> prEvent = make_unique<Event>();
			prEvent->SendEvent(EventType::event_severity_message, prData);
		}

		if ((isClientAdmin
			|| isHost)
			&& (isRegisteredRoute
				|| isAdminRoute))
		{
			PrintData ccData =
			{
				.indentationLength = 0,
				.addTimeStamp = false,
				.severity = sev_m,
				.customTag = "",
				.message =
					"===== APPROVED CLIENT CONNECTED TO ADMIN ROUTE =====\n"
					" IP     : " + clientIP + "\n"
					" Socket : " + to_string(clientSocket) + "\n"
					" Reason : " + cleanRoute + "\n"
					"====================================================\n"
			};
			unique_ptr<Event> ccEvent = make_unique<Event>();
			ccEvent->SendEvent(EventType::event_severity_message, ccData);
		}

		try
		{
			size_t rangeStart = 0;
			size_t rangeEnd = 0;

			string rangeHeader = this->ExtractHeaderValue(
				request,
				"Range");
			this->ParseByteRange(
				rangeHeader,
				rangeStart,
				rangeEnd);

			size_t totalSize = 0;
			bool sliced = false;
			vector<char> result = Server::server->ServeFile(
				cleanRoute,
				rangeStart,
				rangeEnd,
				totalSize,
				sliced);

			if (result.empty())
			{
				PrintData abData =
				{
					.indentationLength = 2,
					.addTimeStamp = true,
					.severity = sev_w,
					.customTag = "CLIENT",
					.message =
						"Client ["
						+ to_string(socket) + " - '" + clientIP + "'] tried to access broken route '"
						+ cleanRoute + "'."
				};
				unique_ptr<Event> abEvent = make_unique<Event>();
				abEvent->SendEvent(EventType::event_severity_error, abData);

				auto resp500 = make_unique<Response_500>();
				resp500->Init(
					rawClientSocket,
					clientIP,
					cleanRoute,
					"text/html");
				this->SocketCleanup(socket);
#pragma warning(push)
#pragma warning(disable: 26117) //"Releasing unheld lock" false positive
				return;
#pragma warning(pop)
			}
			else
			{
				if (sliced)
				{
					auto resp206 = make_unique<Response_206>();

					resp206->hasRange = true;
					resp206->rangeStart = rangeStart;
					resp206->rangeEnd = rangeStart + result.size() - 1;
					resp206->totalSize = totalSize;
					resp206->contentRange =
						"bytes 0-"
						+ to_string(result.size() - 1)
						+ "/"
						+ to_string(totalSize);

					resp206->Init(
						rawClientSocket,
						clientIP,
						cleanRoute,
						foundRoute.mimeType);
					this->SocketCleanup(socket);
#pragma warning(push)
#pragma warning(disable: 26117) //"Releasing unheld lock" false positive
					return;
#pragma warning(pop)
				}
				else
				{
					auto resp200 = make_unique<Response_200>();
					resp200->Init(
						rawClientSocket,
						clientIP,
						cleanRoute,
						foundRoute.mimeType);
					this->SocketCleanup(socket);
#pragma warning(push)
#pragma warning(disable: 26117) //"Releasing unheld lock" false positive
					return;
#pragma warning(pop)
				}
			}
		}
		catch (const exception& e)
		{
			PrintData taData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_w,
				.customTag = "CLIENT",
				.message =
					"Client ["
					+ to_string(socket) + " - '" + clientIP + "'] tried to access broken route '"
					+ cleanRoute + "'.\nError:\n" + e.what()
			};
			unique_ptr<Event> taEvent = make_unique<Event>();
			taEvent->SendEvent(EventType::event_severity_error, taData);

			auto resp500 = make_unique<Response_500>();
			resp500->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(socket);
		}
	}

	void Client::SocketCleanup(uintptr_t clientSocket)
	{
		{
			std::lock_guard socketEraseLock(Server::server->clientSocketsMutex);
			Server::server->activeClientSockets.erase(clientSocket);
		}
	}
}