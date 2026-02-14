//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "KalaHeaders/core_utils.hpp"
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
#include <vector>
#include <array>
#include <sstream>
#include <fstream>
#include <iterator>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/string_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"

#include "server/ks_cloudflare.hpp"
#include "server/ks_server.hpp"
#include "core/ks_core.hpp"

using KalaHeaders::KalaCore::FromVar;
using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
using KalaHeaders::KalaString::SplitString;
using KalaHeaders::KalaThread::jthread;

using KalaServer::Core::KalaServerCore;
using KalaServer::Server::ServerCore;
using KalaServer::Server::Cloudflare;

using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::current_path;
using std::filesystem::directory_iterator;
using std::filesystem::last_write_time;
using std::filesystem::file_time_type;
using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::array;
using std::istringstream;
using std::ofstream;
using std::ifstream;
using std::istreambuf_iterator;
using std::ios;

#ifdef _WIN32
using std::wstring;
#endif

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
static bool RunTunnel(string_view command);

static bool CreateCloudflareProcess(
	string_view command,
	string_view failureReason);

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

namespace KalaServer::Server
{
	thread Cloudflare::cfThread{};
	abool Cloudflare::isRunning{ false };

	static bool isInitialized{};

	static bool isFirstHealthy{};
	static bool isSecondHealthy{};
	static bool isThirdHealthy{};
	static bool isFourthHealthy{};

	bool Cloudflare::Initialize(
		string_view tunnelName,
		const path& cfExePath,
		const path& cfFolderPath)
	{
		Log::Print(
			"Starting to initialize Cloudflare tunnel '" + string(tunnelName) + "'",
			"CLOUDFLARE",
			LogType::LOG_INFO);

		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because server core has not been initialized!",
				"CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (Cloudflare::IsInitialized())
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because it has already been initialized!",
				"CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (tunnelName.empty())
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because its name cannot be empty!",
				"CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (tunnelName.size() < 3)
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because its name '" + string(tunnelName) + "' is too short!",
				"CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (tunnelName.size() > 20)
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because its name '" + string(tunnelName) + "' is too long!",
				"CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		validTunnelName = tunnelName;

		if (!exists(cfExePath))
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because its exe path '" + cfExePath.string() + "' does not exist!",
				"CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}
		if (!exists(cfFolderPath))
		{
			Log::Print(
				"Cannot initialize Cloudflare tunnel because its folder path '" + cfFolderPath.string() + "' does not exist!",
				"CLOUDFLARE",
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
				"Cloudflare tunnel cert file already exists at '" + cfCertFile.string() + "', skipping creation and using existing one.",
				"CLOUDFLARE",
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
				"Cloudflare tunnel json file already exists at '" + cfJsonFile.string() + "', skipping creation and using existing one.",
				"CLOUDFLARE",
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

		isRunning.store(true);

		cfThread = jthread([command]()
		{
			if (!RunTunnel(command))
			{
				KalaServerCore::ForceClose(
					"Cloudflare error", 
					"Failed to create Cloudflare tunnel process!");
			}
		});

		isInitialized = true;

		ServerCore::SetCloudflareReadyState(true);

		Log::Print(
			"Initialized Cloudflare tunnel '" + validTunnelName + "'!",
			"CLOUDFLARE",
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
				"Cannot check for Clouflare tunnel status because the server has not been initialized!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Cannot check for Clouflare tunnel status because the server is not ready!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (!Cloudflare::IsInitialized())
		{
			Log::Print(
				"Cannot check for Clouflare tunnel status because it has not been initialized!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		if (tunnelHandle == 0)
		{
			Log::Print(
				"Cannot check for Clouflare tunnel status because it has not been assigned!",
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
				"Cannot check for Cloudflare tunnel '" + validTunnelName + "' status because its handle is invalid!",
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
				"Cannot check for Clouflare tunnel '" + validTunnelName + "' status because its PID is invalid!",
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
				"Failed to check Cloudflare tunnel '" + validTunnelName + "' status because its PID is wrong, gone or temporarily interrupted!",
				"TUNNEL_STATUS",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		return r == 0;
#endif
	}

	void Cloudflare::Shutdown()
	{
		if (!ServerCore::IsInitialized())
		{
			Log::Print(
				"Cannot shut down Clouflare tunnel because the server has not been initialized!",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!ServerCore::IsReady())
		{
			Log::Print(
				"Cannot shut down Clouflare tunnel because the server is not ready!",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (!Cloudflare::IsInitialized())
		{
			Log::Print(
				"Cannot shut down Cloudflare tunnel because it has not been initialized!",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_ERROR,
				2);

			return;
		}

		if (tunnelHandle == 0)
		{
			Log::Print(
				"Cannot shut down Cloudflare tunnel '" + validTunnelName + "' because it has not been assigned.",
				"CLOUDFLARE_SHUTDOWN",
				LogType::LOG_WARNING);

			return;
		}

		isRunning.store(false);

#ifdef _WIN32
		HANDLE handle = ToVar<HANDLE>(tunnelHandle);

		if (handle == INVALID_HANDLE_VALUE)
		{
			Log::Print(
				"Cannot shut down Cloudflare tunnel '" + validTunnelName + "' because its handle is invalid!",
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
				"Cannot shut down Cloudflare tunnel '" + validTunnelName + "' because its PID is invalid!",
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
			"Finished shutting down Cloudflare tunnel '" + validTunnelName + "'!",
			"CLOUDFLARE_SHUTDOWN",
			LogType::LOG_SUCCESS);
	}
}

bool CreateCertFile()
{
	Log::Print(
		"Creating new Cloudflare tunnel cert file for tunnel '" + validTunnelName + "' at '" + cfCertFile.string() + "'. "
		"A browser window or tab will now open for authentication. Do not close it until you've successfully authenticated.",
		"CLOUDFLARE",
		LogType::LOG_INFO);

	if (!CreateCloudflareProcess(
		"tunnel login", 
		"create Cloudflare cert"))
	{
		return false;
	}

	if (!exists(cfCertFile))
	{
		Log::Print(
			"Failed to create Cloudflare cert for tunnel '" + validTunnelName + "' because user did not successfully authenticate via browser!",
			"CLOUDFLARE",
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

	Log::Print(
		"Creating Cloudflare tunnel credentials.",
		"CLOUDFLARE",
		LogType::LOG_INFO);

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
			"CLOUDFLARE",
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
			"Failed to create Cloudflare json file '" + cfJsonFile.string() + "' for tunnel '" + validTunnelName + "' because newly created tunnel ID was not found!!",
			"CLOUDFLARE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	cfJsonFile = (validCFFolderPath / string(cfTunnelID + ".json"));

	if (!exists(cfJsonFile))
	{
		Log::Print(
			"Failed to create Cloudflare json file '" + cfJsonFile.string() + "' for tunnel '" + validTunnelName + "'!",
			"CLOUDFLARE",
			LogType::LOG_ERROR,
			2);

		return false;
	}

	Log::Print(
		"Created new cloudflare json file at '" + cfJsonFile.string() + "'!",
		"CLOUDFLARE",
		LogType::LOG_SUCCESS);

	return true;
}

bool RouteTunnel()
{
	Log::Print(
		"Starting to route Cloudflare tunnel '" + validTunnelName + "'.",
		"CLOUDFLARE",
		LogType::LOG_INFO);

	//
	// ROOT DOMAIN
	//

	if (!CreateCloudflareProcess(
		"tunnel route dns " + validTunnelName + " " + ServerCore::GetDomainName(), 
		"route tunnel '" + validTunnelName + "'"))
	{
		return false;
	}

	//
	// SUBDOMAIN
	//

	if (!CreateCloudflareProcess(
		"tunnel route dns " + validTunnelName + " www." + ServerCore::GetDomainName(), 
		"route tunnel '" + validTunnelName + "'"))
	{
		return false;
	}

	Log::Print(
		"Routed Cloudflare tunnel '" + validTunnelName + "'!",
		"CLOUDFLARE",
		LogType::LOG_SUCCESS);

	return true;
}

bool CreateConfigFile(string& outCommand)
{
	path certPath = validCFFolderPath / "cert.pem";
	path configPath = validCFFolderPath / "config.yml";

	string domainName = ServerCore::GetDomainName();
	string port = to_string(ServerCore::GetPort());

	string output = 
		"tunnel: " + cfTunnelID + "\n"
		+ "credentials-file: " + cfJsonFile.string() + "\n"
		+ "\n"
		+ "ingress:\n"
		+ "  - hostname: " + domainName + "\n"
		+ "    service: http://localhost:" + port + "\n"
		+ "    originRequest:\n"
		+ "      httpHostHeader: " + domainName + "\n"
		+ "      ipHeaders:\n"
		+ "        - CF-Connecting-IP\n"
		+ "\n"
		+ "  - hostname: www." + domainName + "\n"
		+ "    service: http://localhost:" + port + "\n"
		+ "    originRequest:\n"
		+ "      httpHostHeader: www." + domainName + "\n"
		+ "      ipHeaders:\n"
		+ "        - CF-Connecting-IP\n"
		+ "\n"
		+ "  - service: http_status:404\n";

	bool needsRewrite = true;

	if (exists(configPath))
	{
		ifstream in(configPath);

		if (!in.is_open())
		{
			Log::Print(
				"Failed to check contents of Cloudflare config file '" + configPath.string() + "' for tunnel '" + validTunnelName + "' to verify if it is up to date!",
				"CLOUDFLARE",
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
			Log::Print(
				"Cloudflare config file '" + configPath.string() + "' for tunnel '" + validTunnelName + "' does not exist and will be made.",
				"CLOUDFLARE",
				LogType::LOG_INFO);
		}
		else
		{
			Log::Print(
				"Cloudflare config file '" + configPath.string() + "' for tunnel '" + validTunnelName + "' is out of date and will be rewritten.",
				"CLOUDFLARE",
				LogType::LOG_INFO);
		}

		ofstream file(configPath, ios::trunc);

		if (!file.is_open())
		{
			Log::Print(
				"Failed to create Cloudflare config file to '" + configPath.string() + "' for tunnel '" + validTunnelName + "'!",
				"CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		file << output;

		if (!file.good())
		{
			Log::Print(
				"Failed to write into newly created Cloudflare config file '" + configPath.string() + "' for tunnel '" + validTunnelName + "'!",
				"CLOUDFLARE",
				LogType::LOG_ERROR,
				2);

			return false;
		}

		file.close();
	}
	else
	{
		Log::Print(
			"Cloudflare config file already exists at '" + configPath.string() + "', skipping creation and using existing one.",
			"CLOUDFLARE",
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

bool RunTunnel(string_view command)
{
	return CreateCloudflareProcess(
			command, 
			"run tunnel '" + validTunnelName + "'");
}

bool CreateCloudflareProcess(
		string_view command, 
		string_view failureReason)
	{
#ifdef _WIN32
	STARTUPINFOW si{};
	PROCESS_INFORMATION pi{};
	si.cb = sizeof(si);

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

	//wait for user to finish

	if (failureReason == "run tunnel '" + validTunnelName + "'")
	{
		tunnelHandle = FromVar(pi.hProcess);

		CloseHandle(pi.hThread);
	}
	else
	{
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