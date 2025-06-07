//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>

namespace KalaKit::ResponseSystem
{
	using std::string;
	using std::vector;

	class Response
	{
	public:
		virtual ~Response() = default;

		/// <summary>
		/// Prepare data to be sent to client.
		/// </summary>
		virtual void Init(
			uintptr_t targetClientSocket,
			const string& targetClientIP,
			const string& targetRoute,
			const string& targetContentType) = 0;

	protected:
		/// <summary>
		/// Send data to client.
		/// </summary>
		virtual void Send(
			uintptr_t clientSocket,
			const string& clientIP,
			const string& route,
			const string& contentType,
			const string& statusLine,
			const vector<char> data) const;
	};
}