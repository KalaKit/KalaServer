//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <filesystem>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"
#include "server/ks_cloudflare.hpp"

namespace KalaServer::Server
{
	using std::string;
	using std::string_view;
	using std::filesystem::path;

	using KalaHeaders::KalaThread::abool;

	using u16 = uint16_t;
	using u32 = uint32_t;

	constexpr u16 MIN_PORT_RANGE = 1024;
	constexpr u16 MAX_PORT_RANGE = 65535;

	class LIB_API ServerCore
	{
	friend class Cloudflare;
	public:
		//Initialize a new server on this process.
		//Port is where your server connects to Cloudflare.
		//Server name helps distinguish this server from other servers.
		//Domain name is th domain name with extension inserted to url path in browser.
		//Server root is the true origin where the server will expose routes from relative to where the process was ran from.
		static bool Initialize(
			u16 port,
			string_view serverName,
			string_view domainName,
			const path& serverRoot);

		//Returns true if this server instance has been initialized successfully
		static bool IsInitialized();

		//Returns true if the server Cloudflare backend has been initialized successfully,
		//the server cannot be started if its not ready yet, even if its instance is already initialized
		static bool IsReady();

		//Returns false if the server fails to connect to google.com
		static bool HasInternet();

		static u32 GetID();

		static u16 GetPort();
		static const string& GetServerName();
		static const string& GetDomainName();
		static const path& GetServerRoot();

		//Close all sockets and clear all server resources
		static void Shutdown();
	private:
		static void SetServerReadyState(bool state);
	};
}