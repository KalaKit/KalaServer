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
#include "response/response_401.hpp"
#include "response/response_404.hpp"
#include "response/response_418.hpp"
#include "response/response_500.hpp"

using KalaKit::ResponseSystem::Response_200;
using KalaKit::ResponseSystem::Response_206;
using KalaKit::ResponseSystem::Response_401;
using KalaKit::ResponseSystem::Response_404;
using KalaKit::ResponseSystem::Response_418;
using KalaKit::ResponseSystem::Response_500;

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
				sbEvent->SendEvent(rec_c, sbData);
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
			hbEvent->SendEvent(rec_c, hbData);
			return;
		}
	}

	void Client::HandleClient(uintptr_t socket)
	{
		SOCKET rawClientSocket{};
		uintptr_t clientSocket{};

		string clientIP{};
		string route{};
		string cleanRoute{};

		bool isHost{};
		bool wantsToDownload{};

		string request{};

		bool connectStart = ConnectionStart(
			clientSocket,
			clientIP,
			route,
			cleanRoute,
			isHost,
			wantsToDownload);

		if (!connectStart)
		{
			PrintData pd =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = EventType::event_severity_error,
				.customTag = "CLIENT",
				.message = "Server failed to handle client connection at 'connection start' stage!",
			};
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_console_message, pd);

			return;
		}

		rawClientSocket = static_cast<SOCKET>(clientSocket);

		//
		// CHECK IF CLIENT IS WHITELISTED OR BANNED
		//

		bool canConnect = this->CanConnect(
			clientSocket,
			clientIP,
			cleanRoute,
			isHost);

		if (!canConnect)
		{
			PrintData pd =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = EventType::event_severity_error,
				.customTag = "CLIENT",
				.message = "Server failed to handle client connection at 'can connect' stage!",
			};
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_console_message, pd);

			return;
		}

		string body{};
		string statusLine = "HTTP/1.1 200 OK";

		//
		// CHECK IF ROUTE EXISTS
		//

		Route foundRoute{};

		bool isValidRoute = this->IsValidRoute(
			clientSocket,
			clientIP,
			cleanRoute,
			foundRoute);

		if (!isValidRoute)
		{
			PrintData pd =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = EventType::event_severity_error,
				.customTag = "CLIENT",
				.message = "Server failed to handle client connection at 'is valid route' stage!",
			};
			unique_ptr<Event> event = make_unique<Event>();
			event->SendEvent(EventType::event_print_console_message, pd);

			return;
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

		bool allowDownload =
			isClientRegistered
			|| isClientAdmin
			|| isHost;

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
			alEvent->SendEvent(rec_c, alData);

			auto resp401 = make_unique<Response_401>();
			resp401->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(socket);
			return;
		}

		if (!allowDownload
			&& wantsToDownload)
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
					+ " tried to download file '" + cleanRoute
					+ "' with auth level " + routeAuth + "!"
			};
			unique_ptr<Event> alEvent = make_unique<Event>();
			alEvent->SendEvent(rec_c, alData);

			auto resp401 = make_unique<Response_401>();
			resp401->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(socket);
			return;
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
			prEvent->SendEvent(rec_c, prData);
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
			ccEvent->SendEvent(rec_c, ccData);
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

			if (wantsToDownload
				&& !isClientAdmin
				&& !isHost
				&& totalSize > (static_cast<unsigned long long>(10 * 1024) * 1024))
			{
				string humanReadableSize = to_string(totalSize / (static_cast<unsigned long long>(1024) * 1024)) + " MB";
				string blockReason =
					"Client [" + to_string(socket) + " - '" + clientIP + "'] "
					"attempted to download large file '" + cleanRoute + "' ("
					+ to_string(totalSize) + " bytes = " + humanReadableSize + ") which exceeds the 10MB limit.";
				PrintData dData =
				{
					.indentationLength = 2,
					.addTimeStamp = true,
					.severity = sev_w,
					.customTag = "CLIENT",
					.message = blockReason
				};
				unique_ptr<Event> dEvent = make_unique<Event>();
				dEvent->SendEvent(rec_c, dData);

				auto resp401 = make_unique<Response_401>();
				resp401->Init(
					rawClientSocket,
					clientIP,
					cleanRoute,
					"text/html");
				this->SocketCleanup(socket);
				return;
			}

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
				abEvent->SendEvent(rec_c, abData);

				auto resp500 = make_unique<Response_500>();
				resp500->Init(
					rawClientSocket,
					clientIP,
					cleanRoute,
					"text/html");
				this->SocketCleanup(socket);
				return;
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

					if (wantsToDownload)
					{	
						PrintData prData =
						{
							.indentationLength = 0,
							.addTimeStamp = false,
							.severity = sev_m,
							.customTag = "",
							.message =
								"======== APPROVED CLIENT IS DOWNLOADING FILE =======\n"
								" IP     : " + clientIP + "\n"
								" Socket : " + to_string(clientSocket) + "\n"
								" Reason : " + cleanRoute + "\n"
								"====================================================\n"
						};
						unique_ptr<Event> prEvent = make_unique<Event>();
						prEvent->SendEvent(rec_c, prData);

						resp206->sendAction = KalaKit::ResponseSystem::SendAction::send_download;
					}
					else resp206->sendAction = KalaKit::ResponseSystem::SendAction::send_default;

					resp206->Init(
						rawClientSocket,
						clientIP,
						cleanRoute,
						foundRoute.mimeType);
					this->SocketCleanup(socket);
					return;
				}
				else
				{
					auto resp200 = make_unique<Response_200>();
					
					if (wantsToDownload)
					{	
						PrintData prData =
						{
							.indentationLength = 0,
							.addTimeStamp = false,
							.severity = sev_m,
							.customTag = "",
							.message =
								"======== APPROVED CLIENT IS DOWNLOADING FILE =======\n"
								" IP     : " + clientIP + "\n"
								" Socket : " + to_string(clientSocket) + "\n"
								" Reason : " + cleanRoute + "\n"
								"====================================================\n"
						};
						unique_ptr<Event> prEvent = make_unique<Event>();
						prEvent->SendEvent(rec_c, prData);

						resp200->sendAction = KalaKit::ResponseSystem::SendAction::send_download;
					}
					else resp200->sendAction = KalaKit::ResponseSystem::SendAction::send_default;

					resp200->Init(
						rawClientSocket,
						clientIP,
						cleanRoute,
						foundRoute.mimeType);
					this->SocketCleanup(socket);
					return;
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
			taEvent->SendEvent(rec_c, taData);

			auto resp500 = make_unique<Response_500>();
			resp500->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(socket);
		}
	}

	bool Client::ConnectionStart(
		uintptr_t& clientSocket,
		string& clientIP,
		string& route,
		string& cleanRoute,
		bool& isHost,
		bool& wantsToDownload)
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
			.message = "Socket [" + to_string(clientSocket) + "] entered handle client thread..."
		};
		unique_ptr<Event> seEvent = make_unique<Event>();
		seEvent->SendEvent(rec_c, seData);

		SOCKET rawClientSocket = static_cast<SOCKET>(clientSocket);
		clientSocket = static_cast<uintptr_t>(rawClientSocket);

		DWORD timeout = 1000;
		setsockopt(
			rawClientSocket,
			SOL_SOCKET,
			SO_RCVTIMEO,
			(char*)&timeout,
			sizeof(timeout));

		{
			lock_guard socketInsertLock(Server::server->clientSocketsMutex);
			Server::server->activeClientSockets.insert(clientSocket);
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
					.message = "Socket [" + to_string(clientSocket) + "] timed out after 5 seconds without sending any data."
				};
				unique_ptr<Event> stEvent = make_unique<Event>();
				stEvent->SendEvent(rec_c, stData);
			}
			else
			{
				PrintData srData =
				{
					.indentationLength = 2,
					.addTimeStamp = true,
					.severity = sev_w,
					.customTag = "CLIENT",
					.message = "Socket [" + to_string(clientSocket) + "] recv() failed with error: " + to_string(err)
				};
				unique_ptr<Event> srEvent = make_unique<Event>();
				srEvent->SendEvent(rec_c, srData);
			}

			this->SocketCleanup(clientSocket);
			closesocket(rawClientSocket);
			return false;
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
				.message = "Socket [" + to_string(clientSocket) + "] disconnected without sending any data."
			};
			unique_ptr<Event> sdEvent = make_unique<Event>();
			sdEvent->SendEvent(rec_c, sdData);

			this->SocketCleanup(clientSocket);
			closesocket(rawClientSocket);
			return false;
		}

		string request(buffer);
		route = "/";

		PrintData shData =
		{
			.indentationLength = 2,
			.addTimeStamp = true,
			.severity = sev_d,
			.customTag = "CLIENT",
			.message = "Socket [" + to_string(clientSocket) + "] raw HTTP request:\n" + request
		};
		unique_ptr<Event> shEvent = make_unique<Event>();
		shEvent->SendEvent(rec_c, shData);

		if (request.starts_with("GET "))
		{
			size_t pathStart = 4;
			size_t pathEnd = request.find(' ', pathStart);
			if (pathEnd != string::npos)
			{
				route = request.substr(pathStart, pathEnd - pathStart);
			}
		}

		cleanRoute = route;
		size_t q = route.find('?');

		wantsToDownload = false;
		if (q != string::npos)
		{
			string query = route.substr(q + 1);
			cleanRoute = route.substr(0, q);

			if (query.find("download") != string::npos)
			{
				wantsToDownload = true;
			}
		}

		clientIP = this->ExtractHeader(
			request,
			"Cf-Connecting-Ip");

		if (clientIP.empty())
		{
			clientIP = "unknown-ip";

			PrintData frData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CLIENT",
				.message =
					"Failed to get client IP for socket [" + to_string(clientSocket) + "]!"
					"\n"
					"Full request dump :"
					"\n" +
					request
			};
			unique_ptr<Event> frEvent = make_unique<Event>();
			frEvent->SendEvent(rec_c, frData);
		}

		isHost = Server::server->IsHost(clientIP);
		if (isHost)
		{
			PrintData ciData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.severity = sev_m,
				.customTag = "CLIENT",
				.message = "Client [" + to_string(clientSocket) + " - '" + clientIP + "'] is server host."
			};
			unique_ptr<Event> ciEvent = make_unique<Event>();
			ciEvent->SendEvent(rec_c, ciData);
			clientIP = "host";

			//always update available data when host joins homepage
			if (cleanRoute == "/")
			{
				Server::server->canUpdateWhitelistedRoutes = true;
				Server::server->canUpdateRouteAccess = true;
				Server::server->canUpdateWhitelistedIPs = true;
				Server::server->canUpdateBannedIPs = true;

				Server::server->GetWhitelistedRoutes();
				Server::server->SetRouteAccessLevels();
				Server::server->GetFileData(DataFileType::datafile_whitelistedIP);
				Server::server->GetFileData(DataFileType::datafile_bannedIP);
			}
		}

		return true;
	}

	bool Client::CanConnect(
		uintptr_t clientSocket,
		string clientIP,
		string cleanRoute,
		bool& isHost)
	{
		//
		// CHECK IF CLIENT IS WHITELISTED OR BANNED
		//

		pair<string, string> whitelistedClient = Server::server->IsWhitelistedClient(clientIP);
		pair<string, string> bannedClient = Server::server->IsBannedClient(clientIP);

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
			wcEvent->SendEvent(rec_c, wcData);
		}
		else
		{
			if (bannedClient.first != "")
			{
				BanClientData abcData =
				{
					.ip = clientIP,
					.socket = clientSocket,
					.reason = bannedClient.second,
					.events = Server::server->banClientData.events
				};
				unique_ptr<Event> abcEvent = make_unique<Event>();
				bool result = abcEvent->SendEvent(EventType::event_already_banned_client_connected, abcData);

				//fallback for if any parameters are invalid for already banned user,
				//prevents entire socket hang
				if (!result)
				{
					auto respBanned = make_unique<Response_418>();
					respBanned->Init(
						clientSocket,
						clientIP,
						bannedClient.second,
						"text/html");
				}

				this->SocketCleanup(clientSocket);
				return false;
			}
			else if (bannedClient.first == ""
				&& !Server::server->blacklistedKeywords.empty()
				&& Server::server->IsBlacklistedRoute(cleanRoute))
			{
				BanClientData abcData =
				{
					.ip = clientIP,
					.socket = clientSocket,
					.reason = cleanRoute,
					.events = Server::server->banClientData.events
				};
				unique_ptr<Event> abcEvent = make_unique<Event>();
				bool result = abcEvent->SendEvent(EventType::event_client_was_banned_for_blacklisted_route, abcData);

				//fallback for if any parameters are invalid for banned user,
				//prevents entire socket hang
				if (!result)
				{
					auto respBanned = make_unique<Response_418>();
					respBanned->Init(
						clientSocket,
						clientIP,
						cleanRoute,
						"text/html");
				}

				this->SocketCleanup(clientSocket);
				return false;
			}
		}

		if (Server::server->rateLimitTimer > 0
			&& !isHost
			&& whitelistedClient.first == "")
		{
			lock_guard<mutex> counterLock(Server::server->counterMutex);
			int& count = Server::server->requestCounter[clientIP][cleanRoute];
			count++;

			if (count > Server::server->rateLimitTimer)
			{
				string reason = "Exceeded allowed connection rate limit of " + to_string(Server::server->rateLimitTimer) + " times per second";
				BanClientData abcData =
				{
					.ip = clientIP,
					.socket = clientSocket,
					.reason = reason,
					.events = Server::server->banClientData.events
				};
				unique_ptr<Event> abcEvent = make_unique<Event>();
				bool result = abcEvent->SendEvent(EventType::event_client_was_banned_for_rate_limit, abcData);

				//fallback for if any parameters are invalid for banned user,
				//prevents entire socket hang
				if (!result)
				{
					auto respBanned = make_unique<Response_418>();
					respBanned->Init(
						clientSocket,
						clientIP,
						reason,
						"text/html");
				}

				this->SocketCleanup(clientSocket);
				return false;
			}
		}

		PrintData scData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLIENT",
			.message = "New client successfully connected [" + to_string(clientSocket) + " - '" + clientIP + "']!"
		};
		unique_ptr<Event> scEvent = make_unique<Event>();
		scEvent->SendEvent(rec_c, scData);

		return true;
	}

	bool Client::IsValidRoute(
		uintptr_t clientSocket,
		string clientIP,
		string cleanRoute,
		Route& foundRoute)
	{
		//
		// CHECK IF ROUTE EXISTS
		//

		SOCKET rawClientSocket = static_cast<SOCKET>(clientSocket);

		Server::server->GetWhitelistedRoutes();

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
					+ to_string(clientSocket) + " - '" + clientIP + "'] tried to access non-existing route '"
					+ cleanRoute + "'!"
			};
			unique_ptr<Event> caEvent = make_unique<Event>();
			caEvent->SendEvent(rec_c, caData);

			auto resp404 = make_unique<Response_404>();
			resp404->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(clientSocket);
			return false;
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
					+ to_string(clientSocket) + " - '" + clientIP + "'] tried to access unauthorized route '" + cleanRoute
					+ "' from path '" + foundRoute.route + "'."
			};
			unique_ptr<Event> ttEvent = make_unique<Event>();
			ttEvent->SendEvent(rec_c, ttData);

			auto resp401 = make_unique<Response_401>();
			resp401->Init(
				rawClientSocket,
				clientIP,
				cleanRoute,
				"text/html");
			this->SocketCleanup(clientSocket);
			return false;
		}

		return true;
	}

	void Client::SocketCleanup(uintptr_t clientSocket)
	{
		{
			std::lock_guard socketEraseLock(Server::server->clientSocketsMutex);
			Server::server->activeClientSockets.erase(clientSocket);
		}
	}
}