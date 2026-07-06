//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include <filesystem>
#include <string>
#include <array>
#include <vector>
#include <sstream>
#include <fstream>
#include <iterator>

#include "core_utils.hpp"
#include "log_utils.hpp"
#include "thread_utils.hpp"
#include "string_utils.hpp"

#include "core/ks_cloudflare.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaCore::FromVar;
using KalaHeaders::KalaCore::ToVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaThread::joinable_thread;

using KalaHeaders::KalaString::SplitString;

using KalaServer::Core::KalaServerCore;

using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::current_path;
using std::filesystem::file_time_type;
using std::string;
using std::string_view;
using std::to_string;
using std::array;
using std::vector;
using std::istringstream;
using std::ofstream;
using std::ifstream;
using std::istreambuf_iterator;
using std::ios;

#ifdef _WIN32
using std::wstring;
#endif

static bool isVerboseLoggingEnabled{ false };

static string validTunnelName{};
static path validCFExePath{};
static path validCFFolderPath{};

static path cfCertFile{};
static path cfJsonFile{};
static string cfTunnelID{};

static uintptr_t tunnelHandle{};

static bool CreateCertFile();
static bool CreateTunnelCredentials();
static bool RouteTunnel();
static bool CreateConfigFile(string& outCommand);

static bool CreateCloudflareProcess(
	string_view command,
	string_view failureReason,
	uintptr_t writePipe = {});

static string GetTunnelID(string_view tunnelName)
{
	array<char, 4096> buffer{};
	string output{};

	string command = validCFExePath.string() + " tunnel list";

#ifdef _WIN32
	FILE* pipe = _popen(command.c_str(), "r");
#else
	FILE* pipe = popen(command.c_str(), "r");
#endif

	if (!pipe) return {};

	while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
	{
		output += buffer.data();
	}

#ifdef _WIN32
	_pclose(pipe);
#else
	pclose(pipe);
#endif

	istringstream stream(output);
	string line{};

	while (getline(stream, line))
	{
		if (line.find(tunnelName) != string::npos)
		{
			istringstream lineStream(line);
			string id{}, name{};
			lineStream >> id >> name;
			return name == tunnelName ? id : "";
		}
	}

	return {};
}

#ifdef _WIN32
static wstring ToWide(const string& input);
#endif

namespace KalaServer::Core
{
	thread Cloudflare::cfThread{};

	static bool isInitialized{};

	static bool isFirstHealthy{};
	static bool isSecondHealthy{};
	static bool isThirdHealthy{};
	static bool isFourthHealthy{};

	void Cloudflare::SetVerboseLoggingState(bool state) { isVerboseLoggingEnabled = state; }

	bool Cloudflare::Initialize(
		string_view tunnelName,
		const path& cfExePath,
		const path& cfFolderPath)
	{
		if (!KalaServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot initialize server Cloudflare tunnel because the server is not running or not ready!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		Log::Print(
			"Starting to initialize server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + string(tunnelName) + "'",
			"KS_CLOUDFLARE",
			LogType::LOG_INFO);

		if (Cloudflare::IsInitialized())
		{
			Log::Print(
				"Cannot initialize server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel because it has already been initialized!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (tunnelName.empty())
		{
			Log::Print(
				"Cannot initialize server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel because its name cannot be empty!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (tunnelName.size() < 3)
		{
			Log::Print(
				"Cannot initialize server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel because its name '" + string(tunnelName) + "' is too short!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (tunnelName.size() > 20)
		{
			Log::Print(
				"Cannot initialize server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel because its name '" + string(tunnelName) + "' is too long!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		validTunnelName = tunnelName;

		if (!exists(cfExePath))
		{
			Log::Print(
				"Cannot initialize server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel because its exe path '" + cfExePath.string() + "' does not exist!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (!exists(cfFolderPath))
		{
			Log::Print(
				"Cannot initialize server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel because its folder path '" + cfFolderPath.string() + "' does not exist!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		validCFExePath = cfExePath;
		validCFFolderPath = cfFolderPath;

		if (cfCertFile.empty()) cfCertFile = cfFolderPath / "cert.pem";
		if (exists(cfCertFile))
		{
			Log::Print(
				"Server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel cert file already exists at '" + cfCertFile.string() + "', skipping creation and using existing one.",
				"KS_CLOUDFLARE",
				LogType::LOG_INFO);
		}
		else
		{
			if (!CreateCertFile()) return false;
		}

		if (cfTunnelID.empty()
			|| cfJsonFile.empty())
		{
			cfTunnelID = GetTunnelID(validTunnelName);

			cfJsonFile = cfFolderPath / string(cfTunnelID + ".json");
		}

		if (exists(cfJsonFile))
		{
			Log::Print(
				"Server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel json file already exists at '" + cfJsonFile.string() + "', skipping creation and using existing one.",
				"KS_CLOUDFLARE",
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

		string command{};
		if (!CreateConfigFile(command))
		{
			return false;
		}

		isInitialized = true;

		Log::Print(
			"Initialized server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "' and starting run process.",
			"KS_CLOUDFLARE",
			LogType::LOG_INFO);

		cfThread = joinable_thread([command]()
		{
			if (!RunTunnel(command))
			{
				KalaServerCore::ForceClose(
					"Cloudflare error", 
					"Failed to create server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel process!");
			}
		});

		return true;
	}

	bool Cloudflare::IsInitialized() { return isInitialized; }

	bool Cloudflare::IsHealthy()
	{
		if (!KalaServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot check Cloudflare health because server '" + string(KalaServerCore::GetServerName()) + "' has not been initialized!",
				"CLOUDFLARE_HEALTH_CHECK",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!isInitialized)
		{
			Log::Print(
				"Cannot check Cloudflare health because it has not been initialized!",
				"CLOUDFLARE_HEALTH_CHECK",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return KalaServerCore::IsCloudflareRequired()
			? IsTunnelAlive()
			&& IsTunnelHealthy()
			: true;
	}

	void Cloudflare::Shutdown()
	{
		if (!KalaServerCore::IsReady())
		{
			Log::Print(
				"Cannot shut down server '" + string(KalaServerCore::GetServerName()) + "' Clouflare tunnel because the server is not ready!",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!Cloudflare::IsInitialized())
		{
			Log::Print(
				"Cannot shut down server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel because it has not been initialized!",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (tunnelHandle == 0)
		{
			Log::Print(
				"Cannot shut down server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "' because it has not been assigned.",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_WARNING);

			return;
		}

#ifdef _WIN32
		HANDLE handle = ToVar<HANDLE>(tunnelHandle);

		if (handle == INVALID_HANDLE_VALUE)
		{
			Log::Print(
				"Cannot shut down server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "' because its handle is invalid!",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		TerminateProcess(handle, 0);
		CloseHandle(handle);
#else
		pid_t pid = ToVar<pid_t>(tunnelHandle);

		if (pid <= 0)
		{
			Log::Print(
				"Cannot shut down server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "' because its PID is invalid!",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		kill(pid, SIGKILL);
		waitpid(pid, nullptr, 0);
#endif

		tunnelHandle = 0;

		if (cfThread.joinable()) cfThread.join();

		isInitialized = false;

		Log::Print(
			"Finished shutting down server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "'!",
			"CLOUDFLARE_SHUTDOWN",
			LogType::LOG_SUCCESS);
	}

	bool Cloudflare::RunTunnel(string_view command)
	{
#ifdef _WIN32
		HANDLE readPipe{};
		HANDLE writePipe{};

		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = {};

		if (!CreatePipe(
			&readPipe,
			&writePipe,
			&sa,
			0))
		{
			Log::Print(
				"Failed to create server '" + string(KalaServerCore::GetServerName()) + "' read/write pipe for tunnel '" + validTunnelName + "'!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (!SetHandleInformation(
			readPipe,
			HANDLE_FLAG_INHERIT,
			0))
		{
			Log::Print(
				"Failed to set up server '" + string(KalaServerCore::GetServerName()) + "' pipe handle inheritance for tunnel '" + validTunnelName + "'!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
#else
		uintptr_t readPipe{};
		uintptr_t writePipe{};

		int pipefd[2];
		if (pipe(pipefd) == -1)
		{
			Log::Print(
				"Failed to create server '" + string(KalaServerCore::GetServerName()) + "' read/write pipe for tunnel '" + validTunnelName + "'!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		readPipe = scast<uintptr_t>(pipefd[0]);
		writePipe = scast<uintptr_t>(pipefd[1]);

		int flags = fcntl(pipefd[0], F_GETFD);
		if (flags == -1
			|| fcntl(pipefd[0], F_SETFD, flags | FD_CLOEXEC) == -1)
		{
			Log::Print(
				"Failed to set up server '" + string(KalaServerCore::GetServerName()) + "' pipe handle inheritance for tunnel '" + validTunnelName + "'!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			close(pipefd[0]);
			close(pipefd[1]);

			return false;
		}
#endif

		bool runTunnel = CreateCloudflareProcess(
				command, 
				"run tunnel '" + validTunnelName + "'",
				FromVar(writePipe));

		if (!runTunnel) return false;

		PipeCloudflareMessages(FromVar(readPipe));

		return true;
	}

	void Cloudflare::PipeCloudflareMessages(uintptr_t readPipe)
	{
		if (isVerboseLoggingEnabled)
		{
			Log::Print(
				"Piping server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare messages to internal console for tunnel '" + validTunnelName + "'!",
				"KS_CLOUDFLARE",
				LogType::LOG_INFO);
		}

		char buffer[2048]{};	

#ifdef _WIN32
		HANDLE pipe = ToVar<HANDLE>(readPipe);
		DWORD bytesRead{};

		auto readLoop = [&bytesRead, &pipe, &buffer]() -> bool
			{
				return ReadFile(
					pipe,
					buffer,
					sizeof(buffer) - 1,
					&bytesRead,
					nullptr) 
					&& bytesRead > 0;
			};
#else
		int pipe = scast<int>(readPipe);
		ssize_t bytesRead{};

		auto readLoop = [&bytesRead, &pipe, &buffer]() -> bool
			{
				bytesRead = read(
					pipe,
					buffer,
					sizeof(buffer) - 1);

				return bytesRead > 0;
			};
#endif
		
		while (readLoop())
		{
			buffer[bytesRead] = '\0';
			istringstream stream(buffer);
			string line{};

			while (getline(stream, line))
			{
				if (line.find("Registered tunnel connection connIndex=") != string::npos)
				{
					size_t pos = line.find("connIndex=") + strlen("connIndex=");
					int index = stoi(line.substr(pos));

					if (index == 0) isFirstHealthy = true;
					else if (index == 1) isSecondHealthy = true;
					else if (index == 2) isThirdHealthy = true;
					else if (index == 3) isFourthHealthy = true;

					Log::Print(
						"Server '" + string(KalaServerCore::GetServerName()) + "' connection '" + to_string(index) + "' for tunnel '" + validTunnelName + "' has been marked healthy!",
						"KS_CLOUDFLARE",
						LogType::LOG_INFO);

					if (isFirstHealthy
						&& isSecondHealthy
						&& isThirdHealthy
						&& isFourthHealthy
						&& !KalaServerCore::IsReady())
					{
						KalaServerCore::SetServerReadyState(true);

						Log::Print(
							"Server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "' has connected successfully and server '" + string(KalaServerCore::GetServerName()) + "' is ready!",
							"KS_CLOUDFLARE",
							LogType::LOG_SUCCESS);
					}
				}

				if (line.find("Unregistered tunnel connection connIndex=") != string::npos)
				{
					size_t pos = line.find("connIndex=") + strlen("connIndex=");
					int index = stoi(line.substr(pos));

					if (index == 0) isFirstHealthy = false;
					else if (index == 1) isSecondHealthy = false;
					else if (index == 2) isThirdHealthy = false;
					else if (index == 3) isFourthHealthy = false;

					Log::Print(
						"Server '" + string(KalaServerCore::GetServerName()) + "' connection '" + to_string(index) + "' for tunnel '" + validTunnelName + "' has been marked unhealthy.",
						"KS_CLOUDFLARE",
						LogType::LOG_WARNING);
				}

				//regular cloudflare message

				if (line.size() < 28
					&& isVerboseLoggingEnabled)
				{
					Log::Print(
						line,
						"CLOUDFLARE_TUNNEL",
						LogType::LOG_INFO);

					continue;
				}

				//cloudflare message with a type

				string typeStr = line.substr(21, 3);
				LogType type = LogType::LOG_INFO;

				if (typeStr == "ERR") type = LogType::LOG_ERROR;
				if (typeStr == "WRN") type = LogType::LOG_WARNING;
				if (typeStr == "DBG") type = LogType::LOG_DEBUG;

				string message = line.substr(25);

				bool debugEnabled{};
#ifdef _DEBUG
				debugEnabled = true;
#endif

				if (isVerboseLoggingEnabled
					|| type == LogType::LOG_ERROR
					|| (type == LogType::LOG_DEBUG
					&& debugEnabled))
				{
					Log::Print(
						line,
						"CLOUDFLARE_TUNNEL",
						type);		
				}
			}
		}
	}

	bool Cloudflare::IsTunnelHealthy()
	{
		return isFirstHealthy
			&& isSecondHealthy
			&& isThirdHealthy
			&& isFourthHealthy;
	}

	bool Cloudflare::IsTunnelAlive()
	{
		if (!KalaServerCore::IsCloudflareRequired())
		{
			Log::Print(
				"Cannot check for server '" + string(KalaServerCore::GetServerName()) + "' Clouflare tunnel alive state because the server does not require a Cloudflare tunnel!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!KalaServerCore::IsReady())
		{
			Log::Print(
				"Cannot check for server '" + string(KalaServerCore::GetServerName()) + "' Clouflare tunnel alive state because the server is not running or not ready!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (tunnelHandle == 0)
		{
			Log::Print(
				"Cannot check for server '" + string(KalaServerCore::GetServerName()) + "' Clouflare tunnel alive state because it has not been assigned!",
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
				"Cannot check for server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "' alive state because its handle is invalid!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
#else
		pid_t pid = tunnelHandle;

		if (pid <= 0)
		{
			Log::Print(
				"Cannot check for server '" + string(KalaServerCore::GetServerName()) + "' Clouflare tunnel '" + validTunnelName + "' alive state because its PID is invalid!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		int status{};
		pid_t r = waitpid(pid, &status, WNOHANG);

		if (r == -1)
		{
			Log::Print(
				"Failed to check server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "' alive state because its PID is wrong, gone or temporarily interrupted!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return r == 0;
#endif
	}
}

bool CreateCertFile()
{
	if (isVerboseLoggingEnabled)
	{
		Log::Print(
			"Creating new Cloudflare tunnel cert file for server '" + string(KalaServerCore::GetServerName()) + "' tunnel '" + validTunnelName + "' at '" + cfCertFile.string() + "'. "
			"A browser window or tab will now open for authentication. Do not close it until you've successfully authenticated.",
			"KS_CLOUDFLARE",
			LogType::LOG_INFO);
	}

	if (!CreateCloudflareProcess(
		"tunnel login", 
		"create Cloudflare cert"))
	{
		return false;
	}

	if (!exists(cfCertFile))
	{
		Log::Print(
			"Failed to create Cloudflare cert for server '" + string(KalaServerCore::GetServerName()) + "' tunnel '" + validTunnelName + "' because user did not successfully authenticate via browser!",
			"KS_CLOUDFLARE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	return true;
}

bool CreateTunnelCredentials()
{
	cfTunnelID = GetTunnelID(validTunnelName);
	cfJsonFile = (validCFFolderPath / string(cfTunnelID + ".json"));

	if (isVerboseLoggingEnabled)
	{
		Log::Print(
			"Creating server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel credentials.",
			"KS_CLOUDFLARE",
			LogType::LOG_INFO);
	}

	if (!cfTunnelID.empty())
	{
		if (!CreateCloudflareProcess(
			"tunnel delete " + validTunnelName, 
			"delete Cloudflare tunnel '" + validTunnelName + "'"))
		{
			return false;
		}

		Log::Print(
			"Deleted existing Cloudflare tunnel '" + validTunnelName + "'.",
			"KS_CLOUDFLARE",
			LogType::LOG_INFO);
	}

	if (!CreateCloudflareProcess(
		"tunnel create " + validTunnelName, 
		"create Cloudflare tunnel '" + validTunnelName + "'"))
	{
		return false;
	}

	cfTunnelID = GetTunnelID(validTunnelName);

	if (cfTunnelID.empty())
	{
		Log::Print(
			"Failed to create server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare json file '" + cfJsonFile.string() + "' for tunnel '" + validTunnelName + "' because newly created tunnel ID was not found!!",
			"KS_CLOUDFLARE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	cfJsonFile = (validCFFolderPath / string(cfTunnelID + ".json"));

	if (!exists(cfJsonFile))
	{
		Log::Print(
			"Failed to create server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare json file '" + cfJsonFile.string() + "' for tunnel '" + validTunnelName + "'!",
			"KS_CLOUDFLARE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	Log::Print(
		"Created new server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare json file at '" + cfJsonFile.string() + "'!",
		"KS_CLOUDFLARE",
		LogType::LOG_SUCCESS);

	return true;
}

bool RouteTunnel()
{
	if (isVerboseLoggingEnabled)
	{
		Log::Print(
			"Starting to route server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "'.",
			"KS_CLOUDFLARE",
			LogType::LOG_INFO);
	}

	//
	// ROOT DOMAIN
	//

	for (const auto& d : KalaServerCore::GetDomains())
	{
		if (!CreateCloudflareProcess(
			"tunnel route dns " + validTunnelName + " " + d, 
			"route tunnel '" + validTunnelName + "'"))
		{
			return false;
		}

		//
		// SUBDOMAIN
		//

		if (!CreateCloudflareProcess(
			"tunnel route dns " + validTunnelName + " www." + d, 
			"route tunnel '" + validTunnelName + "'"))
		{
			return false;
		}
	}

	Log::Print(
		"Routed server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare tunnel '" + validTunnelName + "'!",
		"KS_CLOUDFLARE",
		LogType::LOG_SUCCESS);

	return true;
}

bool CreateConfigFile(string& outCommand)
{
	path certPath = validCFFolderPath / "cert.pem";
	path configPath = validCFFolderPath / "config.yml";

	if (isVerboseLoggingEnabled)
	{
		Log::Print(
			"Starting to create server '" + string(KalaServerCore::GetServerName()) + "' config file '" + configPath.string() + "'.",
			"KS_CLOUDFLARE",
			LogType::LOG_INFO);
	}

	string port = to_string(KalaServerCore::GetServerPort());

	string output = 
		"tunnel: " + cfTunnelID + "\n"
		+ "credentials-file: " + cfJsonFile.string() + "\n"
		+ "\n"
		+ "ingress:\n";

	for (const auto& d : KalaServerCore::GetDomains())
	{
		output += "  - hostname: " + d + "\n"
			+ "    service: http://localhost:" + port + "\n"
			+ "    originRequest:\n"
			+ "      httpHostHeader: " + d + "\n"
			+ "      ipHeaders:\n"
			+ "        - CF-Connecting-IP\n"
			+ "\n"
			+ "  - hostname: www." + d + "\n"
			+ "    service: http://localhost:" + port + "\n"
			+ "    originRequest:\n"
			+ "      httpHostHeader: www." + d + "\n"
			+ "      ipHeaders:\n"
			+ "        - CF-Connecting-IP\n";
	}

	output += "\n  - service: http_status:404\n";

	bool needsRewrite = true;

	if (exists(configPath))
	{
		ifstream in(configPath);

		if (!in.is_open())
		{
			Log::Print(
				"Failed to check contents of server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare config file '" + configPath.string() + "' for tunnel '" + validTunnelName + "' to verify if it is up to date!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		string existing(
			(istreambuf_iterator<char>(in)),
			istreambuf_iterator<char>());

		if (existing == output) needsRewrite = false;

		in.close();
	}

	if (needsRewrite)
	{
		if (!exists(configPath))
		{
			if (isVerboseLoggingEnabled)
			{
				Log::Print(
					"Server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare config file '" + configPath.string() + "' for tunnel '" + validTunnelName + "' does not exist and will be made.",
					"KS_CLOUDFLARE",
					LogType::LOG_INFO);
			}
		}
		else
		{
			if (isVerboseLoggingEnabled)
			{
				Log::Print(
					"Server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare config file '" + configPath.string() + "' for tunnel '" + validTunnelName + "' is out of date and will be rewritten.",
					"KS_CLOUDFLARE",
					LogType::LOG_INFO);
			}
		}

		ofstream file(configPath, ios::trunc);

		if (!file.is_open())
		{
			Log::Print(
				"Failed to create server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare config file to '" + configPath.string() + "' for tunnel '" + validTunnelName + "'!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		file << output;

		if (!file.good())
		{
			Log::Print(
				"Failed to write into newly created server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare config file '" + configPath.string() + "' for tunnel '" + validTunnelName + "'!",
				"KS_CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		file.close();
	}
	else
	{
		Log::Print(
			"Server '" + string(KalaServerCore::GetServerName()) + "' Cloudflare config file already exists at '" + configPath.string() + "', skipping creation and using existing one.",
			"KS_CLOUDFLARE",
			LogType::LOG_INFO);
	}

	outCommand = 
#ifdef _WIN32
		"--origin-ca-pool \"" + certPath.string() + "\""
		+ " --config \"" + configPath.string() + "\""
#else
		"--origin-ca-pool " + certPath.string()
		+ " --config " + configPath.string()
#endif
#ifdef _DEBUG
		+ " --loglevel debug"
#endif
		+ " tunnel run " + validTunnelName;

	return true;
}

bool CreateCloudflareProcess(
		string_view command, 
		string_view failureReason,
		uintptr_t writePipe)
	{
		if (isVerboseLoggingEnabled)
		{
			Log::Print(
				"Starting to create server '" + string(KalaServerCore::GetServerName()) + "' process with command '" + string(command) + "' for tunnel '" + validTunnelName + "'",
				"KS_CLOUDFLARE",
				LogType::LOG_INFO);
		}

#ifdef _WIN32
	STARTUPINFOW si{};
	PROCESS_INFORMATION pi{};
	si.cb = sizeof(si);

	if (writePipe != 0)
	{
		si.dwFlags |= STARTF_USESTDHANDLES;
		si.hStdOutput = ToVar<HANDLE>(writePipe);
		si.hStdError = ToVar<HANDLE>(writePipe);
	}

	wstring currParent = ToWide(path(current_path()).string());
	wstring winCommand =
		L"\"" + ToWide(validCFExePath.string()) + L"\" " + ToWide(string(command));

	if (!CreateProcessW(
		nullptr,
		&winCommand[0],
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
			"Failed to " + string(failureReason) + " because Cloudflare tunnel process failed to start!",
			"PROCESS_ERROR",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (failureReason == "run tunnel '" + validTunnelName + "'")
	{
		CloseHandle(pi.hThread);
		CloseHandle(ToVar<HANDLE>(writePipe));

		tunnelHandle = FromVar(pi.hProcess);
	}
	else
	{
		//wait for user to finish

		WaitForSingleObject(pi.hProcess, INFINITE);

		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
#else
	int pipefd[2];
	pipe(pipefd);

	fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

	pid_t pid = fork();
	if (pid < 0)
	{
		KalaServerCore::ForceClose(
			"Process error",
			"Failed to " + string(failureReason) + " because new process for authentication couldn't be created!");
	}
	if (pid == 0)
	{
		//close child
		close(pipefd[0]);

		if (writePipe != 0)
		{
			if (dup2(writePipe, STDOUT_FILENO) == -1
				|| dup2(writePipe, STDERR_FILENO) == -1)
			{
				_exit(-130);
			}
			close(writePipe);
		}

		string targetPathStr = validCFExePath.string();
		vector<char*> commands{ targetPathStr.data() };
		vector<string> split = SplitString(command, " ");
		for (auto& s : split) commands.push_back(s.data());
		commands.push_back(nullptr);

		execvp(commands[0], commands.data());

		//exec failed
		int err = errno;
		write(pipefd[1], &err, sizeof(err));
		close(pipefd[1]);
		_exit(127); //exits child
	}

	//parent
	close(pipefd[1]);

	if (writePipe != 0) close(writePipe);

	int err{};
	ssize_t n = read(pipefd[0], &err, sizeof(err));
	close(pipefd[0]);

	if (n != 0)
	{
		Log::Print(
			"Failed to " + string(failureReason) + " because authentication process failed to start! Error code: " + to_string(err),
			"PROCESS_ERROR",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	if (failureReason == "run tunnel '" + validTunnelName + "'")
	{
		tunnelHandle = FromVar(pid);
		return true;
	}
	else
	{
		//wait for user to finish

		int status{};
		waitpid(pid, &status, 0);

		//interpret exit
		if (WIFEXITED(status))
		{
			int exit_code = WEXITSTATUS(status);
			
			if (exit_code != 0)
			{
				Log::Print(
					"Failed to " + string(failureReason) + " because authentication process was exited unexpectedly! Error code: " + to_string(exit_code),
					"PROCESS_ERROR",
					LogType::LOG_ERROR,
					2);

				return false;
			}
		}
		else if (WIFSIGNALED(status))
		{
			int sig = WTERMSIG(status);

			Log::Print(
				"Failed to " + string(failureReason) + " because authentication process was closed unexpectedly by a signal! Error code: " + to_string(sig),
				"PROCESS_ERROR",
				LogType::LOG_ERROR,
				2);

			return false;
		}
	}
#endif

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