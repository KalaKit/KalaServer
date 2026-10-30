//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/ks_core.hpp"

#if defined(KWIN_ANY)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <linux/limits.h>
#include <unistd.h>
#include <csignal>
#endif

#include "log_utils.hpp"

#include "connection/ks_server.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

using KalaServer::Connection::Server;

using std::string;
using std::to_string;
using std::filesystem::path;
using std::function;
using std::source_location;

static bool isInitialized{};
static string IP{};

static u32 globalID{};
static path exePath{};
static function<void(string, string, const source_location&)> externalHandler{};

static function<void()> shutdownCallback{};

namespace KalaServer::Core
{
	bool KalaServerCore::IsInitialized() { return isInitialized; }
	bool KalaServerCore::Initialize(string_view newIP)
	{
		if (isInitialized)
		{
			Log::Print(
				"Failed to initialize KalaServer via IP '" 
				+ string(newIP) + "' because it is already initialized!",
				"KS_CORE",
				LogType::LOG_WARNING);

			return false;
		}

		if (!IsValidIP(newIP))
		{
			Log::Print(
				"Failed to initialize KalaServer via IP '" 
				+ string(newIP) + "' because the IP is not valid!",
				"KS_CORE",
				LogType::LOG_WARNING);

			return false;
		}

#if defined(KWIN_ANY)
		u32 errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (errorCode != 0)
		{
			ForceClose(
				"KalaServer init error",
				"Failed to call WSASTARTUP! Error code: " + KalaServerCore::ErrorToString(errorCode));
		}
#endif

		if (!HasInternet())
		{
#if defined(KWIN_ANY)
		WSACleanup();
#endif

			Log::Print(
				"Failed to initialize KalaServer via IP '" 
				+ string(newIP) + "' because a working internet connection couldn't be confirmed!",
				"KS_CORE",
				LogType::LOG_WARNING);

			return false;
		}

		IP = newIP;
		isInitialized = true;

		Log::Print(
			"Initialized KalaServer via IP '" + string(newIP) + "'!",
			"KS_CORE",
			LogType::LOG_SUCCESS);

		return true;
	}

	const string& KalaServerCore::GetIP() { return IP; }

	bool KalaServerCore::IsValidIP(string_view targetValue)
	{
		if (targetValue.empty()
			|| targetValue.find('\0') != string::npos)
		{
			return false;
		}

		string thisIP{targetValue};

		in_addr ipv4{};
		in6_addr ipv6{};

		return 
			inet_pton(AF_INET, thisIP.c_str(), &ipv4) == 1
			|| inet_pton(AF_INET6, thisIP.c_str(), &ipv6) == 1;
	}

	bool KalaServerCore::HasInternet()
	{
		
	}

	u32 KalaServerCore::GetGlobalID() { return globalID; }
	void KalaServerCore::SetGlobalID(u32 newID) { globalID = newID; }

    path KalaServerCore::GetExePath()
	{
		if (!exePath.empty()) return exePath;

		if (exePath.empty())
		{
#if defined(KWIN_ANY)
			wchar_t buffer[MAX_PATH]{};
			DWORD length = GetModuleFileNameW(
				nullptr,
				buffer,
				MAX_PATH);

			if (length > 0
				&& length < MAX_PATH)
			{	
				exePath = path(buffer);
			}
			else
			{
				ForceClose(
					"KalaGraphics viewport error",
					"Failed to get path to executable!");
			}
#else
			char buffer[PATH_MAX]{};
			ssize_t length = readlink(
				"/proc/self/exe",
				buffer,
				sizeof(buffer) - 1);

			if (length > 0)
			{
				buffer[length] = '\0';
				exePath = path(buffer);
			}
			else
			{
				KalaServerCore::ForceClose(
					"KalaServer core error",
					"Failed to get path to executable!");
			}
#endif
		}

		return exePath;
	}

	void KalaServerCore::SetExternalHandler(function<void(string, string, const source_location&)>&& newExternalHandler)
	{ 
		externalHandler = std::move(newExternalHandler);
	}

    void KalaServerCore::ForceClose(
        string_view title,
        string_view reason,
        const source_location& location)
    {
#if defined(KWIN_ANY)
		if (isInitialized) WSACleanup();
#endif

		if (externalHandler) externalHandler(string(title), string(reason), location);
		else
		{
			Log::Print(
				"\n================"
				"\nFORCE CLOSE"
				"\n================\n",
				true);

            string reasonStr = string(reason);
            reasonStr 
                += "\n\nfile: " + string(location.file_name())
                + "\nfunction: " + string(location.function_name())
                + "\nline: " + to_string(location.line())
                + "\ncolumn: " + to_string(location.column());

			Log::Print(
				reasonStr,
				title,
				LogType::LOG_ERROR,
				2,
				true,
				TimeFormat::TIME_NONE,
				DateFormat::DATE_NONE);

#if defined(KWIN_ANY)
			__debugbreak();
#else
			raise(SIGTRAP);
#endif
		}

        _Exit(1);
    }

	void SetShutdownCallback(function<void()> newShutdownCallback) { shutdownCallback = newShutdownCallback; }

	void KalaServerCore::Shutdown(bool closeExecutableAfterShutdown)
	{
		if (!isInitialized)
		{
			Log::Print(
				"Failed to shut down KalaServer because it has not been initialized!",
				"KS_CORE",
				LogType::LOG_WARNING);

			return;
		}

		vector<Server*> copy = Server::GetRegistry().GetAllContent();
		for (Server* s : copy) s->Destroy();

		Log::Print(
			"Finished shutting down KalaServer for IP '" + string(IP) + "'!",
			"KS_CORE",
			LogType::LOG_SUCCESS);

		if (shutdownCallback) shutdownCallback();

		isInitialized = false;
		IP.clear();

#if defined(KWIN_ANY)
		WSACleanup();
#endif

		if (closeExecutableAfterShutdown) exit(0);
	}
}