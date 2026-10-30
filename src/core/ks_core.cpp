//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "core/ks_core.hpp"

#define WIN32_LEAN_AND_MEAN

#if defined(KWIN_ANY)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <arpa/inet.h>
#include <linux/limits.h>
#include <unistd.h>
#include <csignal>
#endif

#include <chrono>

#include "log_utils.hpp"
#include "string_utils.hpp"

#include "connection/ks_server.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaLog::TimeFormat;
using KalaHeaders::KalaLog::DateFormat;

using KalaHeaders::KalaString::TrimString;

using KalaServer::Connection::Server;

using std::string;
using std::to_string;
using std::filesystem::path;
using std::function;
using std::source_location;
using std::chrono::duration_cast;
using std::chrono::steady_clock;
using std::chrono::milliseconds;

#if defined(KWIN_ANY)
using std::wstring;

using ksocket = SOCKET;
constexpr ksocket invalid_socket = INVALID_SOCKET;
#define kclose(sock) closesocket(sock)
#else
using ksocket = int;
constexpr ksocket invalid_socket = -1;
#define kclose(sock) close(sock)
#endif

static bool isInitialized{};
static bool readyToCheckInternetState{};
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
		WSADATA wsaData{};
		u32 errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (errorCode != 0)
		{
			ForceClose(
				"KalaServer init error",
				"Failed to call WSASTARTUP! Error code: " + KalaServerCore::ErrorToString(errorCode));
		}
#endif

		readyToCheckInternetState = true;

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

	string KalaServerCore::ErrorToString(int error)
	{
		static string empty{};
#if defined(KWIN_ANY)
		auto to_short = [](const wstring& str)
			{
				if (str.empty()) return empty;

				int size_needed = WideCharToMultiByte(
					CP_UTF8,
					0,
					str.data(),
					scast<int>(str.size()),
					nullptr,
					0,
					nullptr,
					nullptr);

				if (size_needed <= 0) return empty;

				string result(size_needed, 0);

				if (WideCharToMultiByte(
					CP_UTF8,
					0,
					str.data(),
					scast<int>(str.size()),
					result.data(),
					size_needed,
					nullptr,
					nullptr) <= 0)
				{
					return empty;
				}

				return result;
			};

		LPWSTR buffer{};

		FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER
			| FORMAT_MESSAGE_FROM_SYSTEM
			| FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&buffer,
			0,
			nullptr);

		wstring wmsg = buffer ? buffer : L"Unknown error";

		if (buffer) LocalFree(buffer);

		string ts{};
		string _ = TrimString(to_short(wmsg) + " [" + to_string(error) + "]", ts);
		return ts;
#else
		return string(strerror(error)) + " [" + to_string(error) + "]";
#endif
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
		if (!readyToCheckInternetState)
		{
			Log::Print(
				"Failed to check for internet access because KalaServer has not yet been initalized!",
				"KS_CORE",
				LogType::LOG_WARNING);

			return false;
		}

		static bool cachedResult{};
		static u64 lastCheckedTime{};

		u64 now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
		if (now - lastCheckedTime < INTERNET_CHECK_WAIT_PERIOD_S * 1000ULL) return cachedResult;

#if defined (KWIN_ANY)
		ksocket sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
		{
			string err = ErrorToString(WSAGetLastError());

			Log::Print(
				"Failed to check internet state for IP '" + string(IP) + "' because socket creation failed! Reason: " + err,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		u_long mode = 1;
		if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR)
		{
			string err = ErrorToString(WSAGetLastError());

			Log::Print(
				"Failed to enable nonblocking socket mode! Reason: " + err,
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			kclose(sock);
			return false;
		}
#else
		ksocket sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
		{
			Log::Print(
				"Failed to check internet state for IP '" + string(IP) + "' because socket creation failed!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		int flags = fcntl(sock, F_GETFL, 0);

		if (flags == -1
			|| fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
		{
			Log::Print(
				"Failed to enable nonblocking socket mode!",
				"KS_CORE",
				LogType::LOG_ERROR,
				2);

			kclose(sock);
			return false;
		}
#endif

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(53);
		if (inet_pton(AF_INET, "1.1.1.1", &addr.sin_addr) != 1)
		{
			kclose(sock);
			return false;
		}

		int result = connect(sock, (sockaddr*)&addr, sizeof(addr));

#if defined(KWIN_ANY)
		bool inProgress = 
			result == SOCKET_ERROR
			&& WSAGetLastError() == WSAEWOULDBLOCK;
#else
		bool inProgress = 
			result == -1
			&& errno == EINPROGRESS;
#endif

		cachedResult = result == 0;

		if (!cachedResult
			&& inProgress)
		{
			fd_set writeSet{};
			FD_ZERO(&writeSet);
			FD_SET(sock, &writeSet);

			timeval timeout{};
			timeout.tv_sec = 1;

#if defined(KWIN_ANY)
			int ready = select(
				0,
				nullptr,
				&writeSet,
				nullptr,
				&timeout);
#else
			int ready = select(
				sock + 1,
				nullptr,
				&writeSet,
				nullptr,
				&timeout);
#endif
			
			if (ready > 0)
			{
				int socketError{};

#if defined(KWIN_ANY)
				int errorSize = sizeof(socketError);
				if (getsockopt(
					sock,
					SOL_SOCKET,
					SO_ERROR,
					(char*)&socketError,
					&errorSize) == 0)
#else
				socklen_t errorSize = sizeof(socketError);
				if (getsockopt(
					sock,
					SOL_SOCKET,
					SO_ERROR,
					&socketError,
					&errorSize) == 0)
#endif
				{
					cachedResult = socketError == 0;
				}
			}
		}

		kclose(sock);

		lastCheckedTime = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

		return cachedResult;
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

		readyToCheckInternetState = false;
		isInitialized = false;
		IP.clear();

#if defined(KWIN_ANY)
		WSACleanup();
#endif

		if (closeExecutableAfterShutdown) exit(0);
	}
}