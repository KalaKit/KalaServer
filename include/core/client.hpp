//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

#include "core/server.hpp"

namespace KalaKit::Core
{
	using std::string;
	
	using KalaKit::Core::Server;

	class Client
	{
	public:
		//Parse headers from raw HTTP string.
		string ExtractHeaderValue(
			const string& request,
			const string& headerName);

		//Header parser for getting the results from a cloudflared header.
		string ExtractHeader(
			const string& request,
			const string& headerName);

		//Parse byte range from header.
		void ParseByteRange(
			const string& header,
			size_t& outStart,
			size_t& outEnd);

		//Handle each client in its own thread.
		void HandleClient(uintptr_t socket);
	private:
		//First steps to initialize client connection
		bool ConnectionStart(
			string& method,
			uintptr_t& clientSocket,
			string& clientIP,
			string& route,
			string& cleanRoute,
			bool& isHost,
			bool& wantsToDownload,
			bool& wantsToUpload);

		//Checks for client whitelist status and client banned status
		bool CanConnect(
			uintptr_t clientSocket,
			string clientIP,
			string cleanRoute,
			bool& isHost);

		//Checks if route and extension are allowed
		bool IsValidRoute(
			uintptr_t clientSocket,
			string clientIP,
			string cleanRoute,
			Route& foundRoute);

		//Client fills contact form -> server sends email to host
		bool SendEmail(
			string method,
			const string& request,
			uintptr_t clientSocket,
			string clientIP,
			string cleanRoute,
			Route& foundRoute);

		void SocketCleanup(uintptr_t clientSocket);
	};
}