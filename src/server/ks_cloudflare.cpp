//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

#include <filesystem>
#include <string>

#include "KalaHeaders/log_utils.hpp"

#include "server/ks_cloudflare.hpp"
#include "server/ks_server.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::current_path;
using std::filesystem::directory_iterator;
using std::filesystem::last_write_time;
using std::filesystem::file_time_type;
using std::string;
using std::to_string;

#ifdef _WIN32
using std::wstring;
#endif

static string cloudflareCertFile{};
static string cloudflareTunnelID{};
static string cloudflareJsonFile{};

static uintptr_t tunnelHandle{};

static bool CreateCertFile();
static bool CreateTunnelCredentials();
static bool RouteTunnel();
static bool RunTunnel();

#ifdef _WIN32
static wstring ToWide(const string& input);
#endif

namespace KalaServer::Server
{
	static bool isInitialized{};

	static bool isFirstHealthy{};
	static bool isSecondHealthy{};
	static bool isThirdHealthy{};
	static bool isFourthHealthy{};

	bool Cloudflare::Initialize(
		const string& cloudflareExePath,
		const string& cloudflareFolder)
	{
		if (Cloudflare::IsInitialized())
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because it has already been initialized!",
				"CLOUDFLARE_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!exists(cloudflareExePath))
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because its path '" + cloudflareExePath + "' does not exist!",
				"CLOUDFLARE_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!exists(cloudflareFolder))
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because its cert path '" + cloudflareFolder + "' does not exist!",
				"CLOUDFLARE_INIT",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (cloudflareCertFile.empty())
		{
			cloudflareCertFile = path(path(cloudflareFolder) / "cert.pem").string();
		}
		
		if (!exists(cloudflareCertFile)
			&& !CreateCertFile())
		{
			return false;
		}

		if (cloudflareJsonFile.empty())
		{
			file_time_type newestTime{};

			for (const auto& f : directory_iterator(cloudflareFolder))
			{
				path tf = path(f);
				if (tf.extension() == ".json")
				{
					auto lastWrite = last_write_time(tf);
					if (lastWrite > newestTime)
					{
						cloudflareTunnelID = tf.stem().string();
						newestTime = lastWrite;
					}
				}
			}

			cloudflareJsonFile = path(path(cloudflareFolder) / (cloudflareTunnelID + ".json")).string();
		}

		if (exists(cloudflareJsonFile))
		{
			Log::Print(
				"Cloudflare tunnel file already exists, skipping creation and using existing one with ID '" + cloudflareTunnelID + "'.",
				"CLOUDFLARE_INIT",
				LogType::LOG_INFO);
		}
		else
		{
			if (!CreateTunnelCredentials()
				|| !RouteTunnel())
			{
				return false;
			}
		}

		if (!RunTunnel()) return false;

		isInitialized = true;

		Log::Print(
			"Initialized Cloudflare tunnel!",
			"CLOUDFLARE_INIT",
			LogType::LOG_SUCCESS);

		return true;
	}

	bool Cloudflare::IsInitialized() { return isInitialized; }
	bool Cloudflare::IsHealthy(u8 connection)
	{
		switch (connection)
		{
		default:
			return false;
		case 0:
			return isFirstHealthy;
			break;
		case 1:
			return isSecondHealthy;
			break;
		case 2:
			return isThirdHealthy;
			break;
		case 3:
			return isFourthHealthy;
			break;
		}
		return false;
	}

	bool Cloudflare::IsTunnelAlive()
	{
		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot check for tunnel status because the server has not been initialized!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Cannot check for tunnel status because the server is not ready!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!Cloudflare::IsInitialized())
		{
			Log::Print(
				"Cannot check for tunnel status because Cloudflare tunnel has not been initialized!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (tunnelHandle == 0)
		{
			Log::Print(
				"Cannot check for tunnel status because Cloudflare tunnel is NULL!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

#ifdef _WIN32
		HANDLE handle = rcast<HANDLE>(tunnelHandle);

		if (handle == INVALID_HANDLE_VALUE)
		{
			Log::Print(
				"Cannot check for tunnel status because created Cloudflare tunnel is invalid!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
#else
		pid_t pid = tunnelHandle;

		int status{};
		pid_t r = waitpid(pid, &status, WNOHANG);

		return r == 0;
#endif
	}

	void Cloudflare::Shutdown()
	{
		if (!Cloudflare::IsInitialized())
		{
			Log::Print(
				"Cannot shut down Cloudflare tunnel because it has not been initialized!",
				"CLOUDFLARE_QUIT",
				LogType::LOG_ERROR,
				2);

			return;
		}

#ifdef _WIN32
		HANDLE handle = rcast<HANDLE>(tunnelHandle);

		if (handle == INVALID_HANDLE_VALUE)
		{
			Log::Print(
				"Cannot shut down Cloudflare tunnel because its handle is invalid!",
				"CLOUDFLARE_QUIT",
				LogType::LOG_ERROR,
				2);

			return;
		}

		TerminateProcess(handle, 0);
		CloseHandle(handle);
#else
		//TODO: add linux equivalent
#endif

		tunnelHandle = 0;

		isInitialized = false;

		Log::Print(
			"Finished shutting down Cloudflare tunnel!",
			"CLOUDFLARE_QUIT",
			LogType::LOG_SUCCESS);
	}
}

bool CreateCertFile()
{
	Log::Print(
		"Creating new Cloudflare tunnel cert file at '" + cloudflareCertFile + "'.",
		"CLOUDFLARE_INIT",
		LogType::LOG_INFO);

#ifdef _WIN32
	STARTUPINFOW si{};
	PROCESS_INFORMATION pi{};
	si.cb = sizeof(si);

	wstring currParent = ToWide(path(current_path()).string());
	wstring command = ToWide("cloudflared tunnel login");

	if (!CreateProcessW(
		nullptr,
		command.data(),
		nullptr,
		nullptr,
		FALSE,
		0,
		nullptr,
		currParent.c_str(),
		&si,
		&pi))
	{
		Log::Print(
			"Failed to create Cloudflare cert because Cloudflare tunnel process failed to start!",
			"CLOUDFLARE_INIT",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	Log::Print(
		"Launched brower to authorize with Cloudflare. PID: " + to_string(pi.dwProcessId),
		"CLOUDFLARE_INIT",
		LogType::LOG_INFO);

	//wait for user to do their thing with cloudflare,
	//and clean up after the process closes
	WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
#else
	//TODO: add linux equivalent
#endif

	if (!exists(cloudflareCertFile))
	{
		Log::Print(
			"Failed to create Cloudflare cert because user did not successfully authenticate via browser!",
			"CLOUDFLARE_INIT",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	return true;
}

bool CreateTunnelCredentials()
{
	Log::Print(
		"Creating Cloudflare tunnel credentials.",
		"CLOUDFLARE_INIT",
		LogType::LOG_INFO);

	return true;
}

bool RouteTunnel()
{
	Log::Print(
		"Starting to route Cloudflare tunnel.",
		"CLOUDFLARE_INIT",
		LogType::LOG_INFO);

	return true;
}

bool RunTunnel()
{
	Log::Print(
		"Starting to run Cloudflare tunnel.",
		"CLOUDFLARE_INIT",
		LogType::LOG_INFO);

	return true;
}

#ifdef _WIN32
wstring ToWide(const string& input)
{
	if (input.empty()) return wstring();

	int size_needed = MultiByteToWideChar(
		CP_UTF8,
		0,
		input.data(),
		scast<int>(input.size()),
		nullptr,
		0);

	wstring wstr(size_needed - 1, 0);

	MultiByteToWideChar(
		CP_UTF8,
		0,
		input.data(),
		scast<int>(input.size()),
		wstr.data(),
		size_needed);

	return wstr;
}
#endif