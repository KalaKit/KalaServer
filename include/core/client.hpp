//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

namespace KalaKit::Core
{
	using std::string;

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


		void SocketCleanup(uintptr_t clientSocket);
	};
}