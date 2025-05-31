//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

namespace KalaKit::ResponseSystem
{
	using std::string;

	class Response
	{
	public:
		virtual ~Response() = default;

		/// <summary>
		/// Prepare data to be sent to client.
		/// </summary>
		virtual void Init(
			const string& route,
			const string& clientIP,
			uintptr_t clientSocket) = 0;

		const string& GetStatusLine() const { return statusLine; }
		const string& GetContentType() const { return contentType; }
		const string& GetBody() const { return body; }
		const string& GetRoute() const { return route; }
		const string& GetclientIP() const { return clientIP; }
		uintptr_t GetClientSocket() const { return clientSocket; }
	protected:
		/// <summary>
		/// Send data to client.
		/// </summary>
		virtual void Send() const;

		string statusLine{};
		string contentType{};
		string body{};

		string route{};
		string clientIP{};
		uintptr_t clientSocket{};
	};
}