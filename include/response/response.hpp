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

	enum class SendAction
	{
		send_default,
		send_download
	};

	class Response
	{
	public:
		bool hasRange = false;
		size_t rangeStart = 0;
		size_t rangeEnd = 0;
		size_t totalSize = 0;
		string contentRange{};
		SendAction sendAction;

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
			const vector<char> data,
			SendAction sendAction = SendAction::send_default) const;
	};
}