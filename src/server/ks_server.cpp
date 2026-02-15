//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <curl/curl.h>
#include <curl/easy.h>

#include <string>

#include "KalaHeaders/log_utils.hpp"

#include "server/ks_server.hpp"
#include "server/ks_cloudflare.hpp"
#include "server/ks_connect.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaServer::Core::KalaServerCore;

using std::to_string;
using std::string;

static void InitializeCurl()
{
	static bool curlInitialized{};

	if (curlInitialized) return;

	bool success = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;

	if (success) curlInitialized = true;
	else
	{
		KalaServerCore::ForceClose(
			"Curl error",
			"Failed to initialize Curl!");
	}
}

namespace KalaServer::Server
{
	static bool isInitialized{};
	static bool isReady{};

	static u32 ID{};

	static u16 port{};
	static string serverName{};
	static string domainName{};
	static path serverRoot{};

	bool ServerCore::Initialize(
		u16 newPort,
		string_view newServerName,
		string_view newDomainName,
		const path& newServerRoot)
	{
		Log::Print(
			"Starting to initialize server '" + string(newServerName) 
			+ "' at port '" + to_string(newPort) 
			+ "' with domain '" + string(newDomainName)
			+ "' and server root '" + newServerRoot.string(),
			"CLOUDFLARE",
			LogType::LOG_INFO);

		if (newPort < MIN_PORT_RANGE
			|| newPort > MAX_PORT_RANGE)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its port '" + to_string(newPort) + "' is out of range!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (newServerName.empty()
			|| newServerName.length() > 50)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its name is empty or too long!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (newDomainName.empty()
			|| newDomainName.length() > 50)
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its domain name '" + string(newDomainName) + "' is empty or too long!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!exists(newServerRoot))
		{
			Log::Print(
				"Failed to initialize server '" + string(newServerName) + "' because its server root '" + newServerRoot.string() + "' is empty or does not exist!",
				"SERVER",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		port = newPort;
		serverName = newServerName;
		domainName = newDomainName;
		serverRoot = newServerRoot;

		isInitialized = true;

		Log::Print(
			"Created new server '" + serverName + "'!",
			"SERVER",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool ServerCore::IsInitialized() { return isInitialized; }

	bool ServerCore::IsReady() { return isReady; }

	bool ServerCore::HasInternet()
	{
		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot check for internet access because the server has not been initialized!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Cannot check for internet access because the server is not ready!",
				"INTERNET_ACCESS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		const string testPage = "https://www.google.com";

		InitializeCurl();

		CURL* curl = curl_easy_init();
		if (!curl)
		{
			KalaServerCore::ForceClose(
				"Curl error",
				"curl_easy failed!");
		}

		curl_easy_setopt(curl, CURLOPT_URL, testPage.c_str());

		//ignore response body
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

		//reasonable timeouts
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		//follow redirects (google will redirect)
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		//ignore errors
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		bool hasInternet = res == CURLE_OK;

		if (!hasInternet)
		{
			Log::Print(
				"Server does not have internet access!",
				"INTERNET_ACCESS",
				LogType::LOG_WARNING);

			return false;
		}
		else
		{
			Log::Print(
				"Server has internet access!",
				"INTERNET_ACCESS",
				LogType::LOG_SUCCESS);

			return true;
		}
	}

	u32 ServerCore::GetID() { return ID; }

	u16 ServerCore::GetPort() { return port; }
	const string& ServerCore::GetServerName() { return serverName; }
	const string& ServerCore::GetDomainName() { return domainName; }
	const path& ServerCore::GetServerRoot() { return serverRoot; }

	void ServerCore::Shutdown()
	{
		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot shut down the server because it has not been initialized!",
				"SERVER_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		Connect::DisconnectListener();
		Connect::CancelAllPackets();
	}

	void ServerCore::SetServerReadyState(bool state)
	{
		isReady = state;
	}
}