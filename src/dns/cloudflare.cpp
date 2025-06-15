//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <filesystem>
#include <string>
#include <Windows.h>
#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>
#include <set>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

#include "core/core.hpp"
#include "core/server.hpp"
#include "dns/cloudflare.hpp"
#include "dns/dns.hpp"
#include "core/event.hpp"

using KalaKit::Core::KalaServer;
using KalaKit::Core::Server;
using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::PrintData;
using KalaKit::Core::PopupData;

using KalaKit::Core::sev_m;
using KalaKit::Core::sev_d;
using KalaKit::Core::sev_w;
using KalaKit::Core::sev_e;
using KalaKit::Core::rec_c;
using KalaKit::Core::rec_p;
using KalaKit::Core::rec_e;

using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::directory_iterator;
using std::filesystem::file_time_type;
using std::string;
using std::wstring;
using std::to_string;
using std::ofstream;
using std::ifstream;
using std::stringstream;
using std::replace;
using std::istringstream;
using std::thread;
using std::set;
using std::unique_ptr;
using std::make_unique;

namespace KalaKit::DNS
{
	static string tunnelID{};
	static string tunnelIDFilePath{};

	bool CloudFlare::Initialize(
		bool shouldCloseCloudflaredAtShutdown,
		const string& newTunnelName)
	{
		if (Server::server == nullptr)
		{
			PrintData csData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CLOUDFLARE",
				.message = "Cannot initialize cloudflared if server has not yet been initialized!"
			};
			unique_ptr<Event> csEvent = make_unique<Event>();
			csEvent->SendEvent(rec_c, csData);
			return false;
		}

		if (isInitializing)
		{
			PrintData ciData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CLOUDFLARE",
				.message = "Cannot initialize cloudflared while it is already being initialized!"
			};
			unique_ptr<Event> ciEvent = make_unique<Event>();
			ciEvent->SendEvent(rec_c, ciData);
			return false;
		}
		
		if (!CloudflarePreInitializeCheck(newTunnelName))
		{
			return false;
		}
		
		isInitializing = true;
		
		closeCloudflaredAtShutdown = shouldCloseCloudflaredAtShutdown;
		
		if (CustomDNS::IsRunning())
		{
			PopupData crData =
			{
				.message =
					"Failed to start cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Cannot run DNS and cloudflared together!",
				.severity = sev_e
			};
			unique_ptr<Event> crEvent = make_unique<Event>();
			crEvent->SendEvent(rec_p, crData);
			isInitializing = false;
			return false;
		}

		string cloudFlaredName = "cloudflared.exe";
		string cloudflaredPath = path(current_path() / cloudFlaredName).string();
		if (!exists(cloudflaredPath))
		{
			PopupData fcData =
			{
				.message =
					"Failed to start cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to find cloudflared.exe!",
				.severity = sev_e
			};
			unique_ptr<Event> fcEvent = make_unique<Event>();
			fcEvent->SendEvent(rec_p, fcData);
			isInitializing = false;
			return false;
		}
		
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();
		if (!exists(certPath)) CreateCert();
		else
		{
			PrintData ccData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.severity = sev_m,
				.customTag = "CLOUDFLARE",
				.message = "Cloudflared cert file already exists at '" + certPath + "'. Skipping creation."
			};
			unique_ptr<Event> ccEvent = make_unique<Event>();
			ccEvent->SendEvent(rec_c, ccData);
		}

		if (tunnelID == "") CreateTunnelCredentials();
		else
		{
			string tunnelFile = tunnelID + ".json";
			tunnelIDFilePath = path(path(cloudFlareFolder) / tunnelFile).string();
			replace(tunnelIDFilePath.begin(), tunnelIDFilePath.end(), '\\', '/');
		
			if (!exists(tunnelIDFilePath)) CreateTunnelCredentials();
			else
			{
				PrintData cjData =
				{
					.indentationLength = 0,
					.addTimeStamp = true,
					.severity = sev_m,
					.customTag = "CLOUDFLARE",
					.message = "Cloudflared json file already exists at '" + tunnelIDFilePath + "'. Skipping creation."
				};
				unique_ptr<Event> cjEvent = make_unique<Event>();
				cjEvent->SendEvent(rec_c, cjData);
			}	
		}
		
		RunTunnel();

		isInitializing = false;
		isRunning = true;

		PrintData ciData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE",
			.message = "Cloudflared initialization completed."
		};
		unique_ptr<Event> ciEvent = make_unique<Event>();
		ciEvent->SendEvent(rec_c, ciData);

		return true;
	}
	
	bool CloudFlare::CloudflarePreInitializeCheck(
		const string& newTunnelName)
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();

		if (newTunnelName == "")
		{
			PopupData csData =
			{
				.message = "Cannot start cloudflared with empty tunnel name!",
				.severity = sev_e
			};
			unique_ptr<Event> csEvent = make_unique<Event>();
			csEvent->SendEvent(rec_p, csData);
			return false;
		}
		size_t tunnelNameLength = newTunnelName.length();
		if (tunnelNameLength < 3
			|| tunnelNameLength > 32)
		{
			PopupData tnData =
			{
				.message = 
					"Failed to start cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Tunnel name length '" + to_string(tunnelNameLength) + "' is out of range! Must be between 3 and 32 characters.",
				.severity = sev_e
			};
			unique_ptr<Event> tnEvent = make_unique<Event>();
			tnEvent->SendEvent(rec_p, tnData);
			return false;
		}
		tunnelName = newTunnelName;
		
		if (tunnelIDFilePath != ""
			&& path(tunnelIDFilePath).extension().string() == ".json")
		{
			tunnelID = path(tunnelIDFilePath).stem().string();
		}
		if (tunnelID == "")
		{
			string newestTunnelID{};
			file_time_type newestTime{};
			for (const auto& file : directory_iterator(cloudFlareFolder))
			{
				path filePath = file.path();
				if (filePath.extension().string() == ".json")
				{
					auto lastWrite = last_write_time(file);
					if (lastWrite > newestTime)
					{
						newestTunnelID = filePath.stem().string();
						newestTime = lastWrite;
					}
				}
			}
			if (newestTunnelID != "") tunnelID = newestTunnelID;
		}
		
		return true;
	}

	string CloudFlare::GetTextFileValue(
		const string& textFilePath,
		unsigned int minLength,
		unsigned int maxLength)
	{
		string fileResult{};

		ifstream file(textFilePath);
		if (!file)
		{
			PopupData otData =
			{
				.message =
					"Failed to start cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to open text file '" + textFilePath + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> otEvent = make_unique<Event>();
			otEvent->SendEvent(rec_p, otData);
			return "";
		}

		stringstream buffer{};
		buffer << file.rdbuf();
		fileResult = buffer.str();

		size_t textLength = fileResult.length();

		if (textLength < minLength
			|| textLength > maxLength)
		{
			PopupData tfData =
			{
				.message =
					"Failed to start cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Text file '" + textFilePath + "' text length '" + to_string(textLength) + "' is too small or too big! You probably didn't copy the text correctly.",
				.severity = sev_e
			};
			unique_ptr<Event> tfEvent = make_unique<Event>();
			tfEvent->SendEvent(rec_p, tfData);
			return "";
		}

		if (fileResult.find(" ") != string::npos)
		{
			PopupData ciData =
			{
				.message =
					"Failed to start cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Text file '" + textFilePath + "' contains incorrect info! You probably didn't copy the text correctly.",
				.severity = sev_e
			};
			unique_ptr<Event> ciEvent = make_unique<Event>();
			ciEvent->SendEvent(rec_p, ciData);
			return "";
		}

		return fileResult;
	}

	void CloudFlare::CreateCert()
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();

		if (exists(certPath))
		{
			PrintData caData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.severity = sev_m,
				.customTag = "CLOUDFLARE",
				.message = "Cloudflared cert file already exists at '" + certPath + "'. Skipping creation."
			};
			unique_ptr<Event> caEvent = make_unique<Event>();
			caEvent->SendEvent(rec_c, caData);
			return;
		}

		PrintData ntData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE",
			.message = "Creating new tunnel cert file at '" + certPath + "'."
		};
		unique_ptr<Event> ntEvent = make_unique<Event>();
		ntEvent->SendEvent(rec_c, ntData);

		string currentPath = current_path().string();
		wstring wParentFolderPath(currentPath.begin(), currentPath.end());

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		string command = "cloudflared tunnel login";
		PrintData ccData =
		{
			.indentationLength = 2,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE_COMMAND",
			.message = command
		};
		unique_ptr<Event> ccEvent = make_unique<Event>();
		ccEvent->SendEvent(rec_c, ccData);

		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si,                       //pointer to STARTUPINFO structure
			&pi                        //pointer to PROCESS_INFORMATION structure
		))
		{
			PopupData cpData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create process for creating cert for tunnel '" + tunnelName + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> cpEvent = make_unique<Event>();
			cpEvent->SendEvent(rec_p, cpData);
			return;
		}

		//close thread handle and process
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);

		PrintData lbData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE",
			.message = "Launched browser to authorize with CloudFlare. PID: " + to_string(pi.dwProcessId)
		};
		unique_ptr<Event> lbEvent = make_unique<Event>();
		lbEvent->SendEvent(rec_c, lbData);

		CloseHandle(pi.hProcess);

		if (!exists(certPath))
		{
			PopupData cfData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create tunnel cert file for tunnel '" + tunnelName + "' in '" + cloudFlareFolder + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> cfEvent = make_unique<Event>();
			cfEvent->SendEvent(rec_p, cfData);
		}
		
		PrintData ncData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE",
			.message = "Sucessfully created new cloudflared cert file for tunnel '" + tunnelName + "'."
		};
		unique_ptr<Event> ncEvent = make_unique<Event>();
		ncEvent->SendEvent(rec_c, ncData);
	}

	void CloudFlare::CreateTunnelCredentials()
	{
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		
		if (tunnelIDFilePath != ""
			&& path(tunnelIDFilePath).extension().string() == ".json")
		{
			PrintData cjData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.severity = sev_m,
				.customTag = "CLOUDFLARE",
				.message = "Cloudflared credentials json file for tunnel '" + tunnelName + "' already exists at '" + tunnelIDFilePath + "'. Skipping creation."
			};
			unique_ptr<Event> cjEvent = make_unique<Event>();
			cjEvent->SendEvent(rec_c, cjData);
			return;
		}
		
		string currentPath = current_path().string();
		wstring wParentFolderPath(currentPath.begin(), currentPath.end());
		
		//
		// DELETE OLD TUNNEL
		//

		//initialize structures for process creation
		STARTUPINFOW si1;
		PROCESS_INFORMATION pi1;
		ZeroMemory(&si1, sizeof(si1));
		ZeroMemory(&pi1, sizeof(pi1));
		si1.cb = sizeof(si1);

		string command1 = "cloudflared tunnel delete " + tunnelName;
		PrintData c1Data =
		{
			.indentationLength = 2,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE_COMMAND",
			.message = command1
		};
		unique_ptr<Event> c1Event = make_unique<Event>();
		c1Event->SendEvent(rec_c, c1Data);

		wstring wideCommand1(command1.begin(), command1.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand1.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si1,                      //pointer to STARTUPINFO structure
			&pi1                       //pointer to PROCESS_INFORMATION structure
		))
		{
			PopupData dtData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create process for deleting tunnel '" + tunnelName + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> dtEvent = make_unique<Event>();
			dtEvent->SendEvent(rec_p, dtData);
			return;
		}

		//close thread handle and process
		WaitForSingleObject(pi1.hProcess, INFINITE);
		CloseHandle(pi1.hThread);
		CloseHandle(pi1.hProcess);
		
		//
		// CREATE NEW TUNNEL
		//
		
		//initialize structures for process creation
		STARTUPINFOW si2;
		PROCESS_INFORMATION pi2;
		ZeroMemory(&si2, sizeof(si2));
		ZeroMemory(&pi2, sizeof(pi2));
		si2.cb = sizeof(si2);

		string command2 = "cloudflared tunnel create " + tunnelName;
		PrintData c2Data =
		{
			.indentationLength = 2,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE_COMMAND",
			.message = command2
		};
		unique_ptr<Event> c2Event = make_unique<Event>();
		c2Event->SendEvent(rec_c, c2Data);

		wstring wideCommand2(command2.begin(), command2.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand2.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si2,                      //pointer to STARTUPINFO structure
			&pi2                       //pointer to PROCESS_INFORMATION structure
		))
		{
			PopupData ctData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create process for creating tunnel '" + tunnelName + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> ctEvent = make_unique<Event>();
			ctEvent->SendEvent(rec_p, ctData);
			return;
		}

		//close thread handle and process
		WaitForSingleObject(pi2.hProcess, INFINITE);
		CloseHandle(pi2.hThread);
		CloseHandle(pi2.hProcess);
		
		//
		// FINISH TUNNEL CREATION
		//
		
		if (tunnelIDFilePath != ""
			&& path(tunnelIDFilePath).extension().string() == ".json"
			&& !exists(tunnelIDFilePath))
		{
			PopupData cjData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create tunnel credentials json file for tunnel '" + tunnelName + "' at '" + tunnelIDFilePath + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> cjEvent = make_unique<Event>();
			cjEvent->SendEvent(rec_p, cjData);
			return;
		}
		
		PrintData jfData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE",
			.message = "Created new cloudflared credentials json file for tunnel '" + tunnelName + "'."
		};
		unique_ptr<Event> jfEvent = make_unique<Event>();
		jfEvent->SendEvent(rec_c, jfData);
			
		PopupData bcData =
		{
			.message =
				"Before continuing to route DNS, you must manually insert the tunnel ID into config.yml.\n\n"
				"Please update the following fields:\n"
				"  tunnel: <insert tunnel ID here>\n"
				"  credentials-file: <insert full path to .json file>\n\n"
				""
				"  Make sure to also update the DNS name for your tunnel ID in cloudflare if have already done this procedure before!"
				"Press OK to continue once you've updated config.yml. If the tunnel ID is incorrect, DNS routing will fail and 'cloudflared tunnel run' will not work.",
			.severity = sev_w
		};
		unique_ptr<Event> bcEvent = make_unique<Event>();
		bcEvent->SendEvent(rec_p, bcData);
			
		RouteTunnel();
	}

	void CloudFlare::RouteTunnel()
	{
		string currentPath = current_path().string();
		wstring wParentFolderPath(currentPath.begin(), currentPath.end());

		//
		// ROOT DOMAIN
		//

		//initialize structures for process creation
		STARTUPINFOW si1;
		PROCESS_INFORMATION p1;
		ZeroMemory(&si1, sizeof(si1));
		ZeroMemory(&p1, sizeof(p1));
		si1.cb = sizeof(si1);

		string command1 = "cloudflared tunnel route dns " + Server::server->GetDomainName() + " " + tunnelName;
		PrintData c1Data =
		{
			.indentationLength = 2,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE_COMMAND",
			.message = command1
		};
		unique_ptr<Event> c1Event = make_unique<Event>();
		c1Event->SendEvent(rec_c, c1Data);

		wstring wideCommand1(command1.begin(), command1.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand1.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si1,                       //pointer to STARTUPINFO structure
			&p1                        //pointer to PROCESS_INFORMATION structure
		))
		{
			PopupData rdData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create process for routing dns for tunnel '" + tunnelName + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> rdEvent = make_unique<Event>();
			rdEvent->SendEvent(rec_p, rdData);
			return;
		}

		//close thread handle and process
		WaitForSingleObject(p1.hProcess, INFINITE);
		CloseHandle(p1.hThread);
		CloseHandle(p1.hProcess);

		//
		// SUBDOMAIN
		//

		//initialize structures for process creation
		STARTUPINFOW si2;
		PROCESS_INFORMATION pi2;
		ZeroMemory(&si2, sizeof(si2));
		ZeroMemory(&pi2, sizeof(pi2));
		si2.cb = sizeof(si2);

		string command2 = "cloudflared tunnel route dns www." + Server::server->GetDomainName() + " " + tunnelName;
		PrintData c2Data =
		{
			.indentationLength = 2,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE_COMMAND",
			.message = command2
		};
		unique_ptr<Event> c2Event = make_unique<Event>();
		c2Event->SendEvent(rec_c, c2Data);

		wstring wideCommand2(command2.begin(), command2.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand2.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			FALSE,                     //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si2,                       //pointer to STARTUPINFO structure
			&pi2                        //pointer to PROCESS_INFORMATION structure
		))
		{
			PopupData rdData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create process for routing dns for tunnel '" + tunnelName + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> rdEvent = make_unique<Event>();
			rdEvent->SendEvent(rec_p, rdData);
			return;
		}

		//close thread handle and process
		WaitForSingleObject(pi2.hProcess, INFINITE);
		CloseHandle(pi2.hThread);
		CloseHandle(pi2.hProcess);
		
		PrintData rcData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE",
			.message = "Successfully routed cloudflared dns for tunnel '" + tunnelName + "'."
		};
		unique_ptr<Event> rcEvent = make_unique<Event>();
		rcEvent->SendEvent(rec_c, rcData);
	}

	void CloudFlare::RunTunnel()
	{	
		string cloudFlareFolder = (path(getenv("USERPROFILE")) / ".cloudflared").string();
		string certPath = path(path(cloudFlareFolder) / "cert.pem").string();
		string configPath = path(path(cloudFlareFolder) / "config.yml").string();
	
		HANDLE hReadPipe{};
		HANDLE hWritePipe{};
		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = nullptr;

		if (!CreatePipe(
			&hReadPipe,
			&hWritePipe,
			&sa,
			0))
		{
			PopupData rdData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create read/write pipe for tunnel '" + tunnelName + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> rwEvent = make_unique<Event>();
			rwEvent->SendEvent(rec_p, rdData);
			return;
		}
		if (!SetHandleInformation(
			hReadPipe,
			HANDLE_FLAG_INHERIT,
			0))
		{
			PopupData phData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to set up pipe handle inheritance for tunnel '" + tunnelName + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> phEvent = make_unique<Event>();
			phEvent->SendEvent(rec_p, phData);
			return;
		}

		//initialize structures for process creation
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		si.dwFlags |= STARTF_USESTDHANDLES;
		si.hStdOutput = hWritePipe;
		si.hStdError = hWritePipe;

		string currentPath = current_path().string();
		wstring wParentFolderPath(currentPath.begin(), currentPath.end());

#ifdef _DEBUG
		string command = 
			"cloudflared --origin-ca-pool " + certPath 
			+ " --config " + configPath 
			+ " --loglevel debug" 
			+ " tunnel run " + tunnelName;
#else
		string command =
			"cloudflared --origin-ca-pool " + certPath
			+ " --config " + configPath
			+ " tunnel run " + tunnelName;
#endif

		PrintData ccData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE_COMMAND",
			.message = command
		};
		unique_ptr<Event> ccEvent = make_unique<Event>();
		ccEvent->SendEvent(rec_c, ccData);

		wstring wideCommand(command.begin(), command.end());
		if (!CreateProcessW
		(
			nullptr,                   //path to the executable
			const_cast<LPWSTR>(wideCommand.c_str()), //command line arguments
			nullptr,                   //process handle not inheritable
			nullptr,                   //thread handle not inheritable
			TRUE,                      //handle inheritance
			0,                         //creation flags
			nullptr,                   //use parent's environment block
			wParentFolderPath.c_str(), //use parent's starting directory
			&si,                       //pointer to STARTUPINFO structure
			&pi                        //pointer to PROCESS_INFORMATION structure
		))
		{
			PopupData rtData =
			{
				.message =
					"Failed to set up cloudflared!"
					"\n\n"
					"Reason:"
					"\n"
					"Failed to create process for running tunnel '" + tunnelName + "'!",
				.severity = sev_e
			};
			unique_ptr<Event> rtEvent = make_unique<Event>();
			rtEvent->SendEvent(rec_p, rtData);
			return;
		}

		//close thread handle and process
		CloseHandle(pi.hThread);
		CloseHandle(hWritePipe);
		tunnelRunHandle = reinterpret_cast<uintptr_t>(pi.hProcess);
		
		PrintData rcData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE",
			.message = "Running cloudflared tunnel."
		};
		unique_ptr<Event> rcEvent = make_unique<Event>();
		rcEvent->SendEvent(rec_c, rcData);

		void* convertedHandle = reinterpret_cast<void*>(hReadPipe);
		PipeCloudflareMessages(convertedHandle);
	}

	void CloudFlare::PipeCloudflareMessages(void* handle)
	{
		HANDLE hReadPipe = reinterpret_cast<HANDLE>(handle);

		thread([hReadPipe]
		{
			PrintData pcData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.severity = sev_m,
				.customTag = "CLOUDFLARE",
				.message = "Piping cloudflare messages to internal console."
			};
			unique_ptr<Event> pcEvent = make_unique<Event>();
			pcEvent->SendEvent(rec_c, pcData);

			char buffer[2048]{};
			DWORD bytesRead = 0;

			while (ReadFile(
				hReadPipe,
				buffer,
				sizeof(buffer) - 1,
				&bytesRead,
				nullptr)
				&& bytesRead > 0)
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

						if (index == 0) isConn0Healthy = true;
						else if (index == 1) isConn1Healthy = true;
						else if (index == 2) isConn2Healthy = true;
						else if (index == 3) isConn3Healthy = true;

						if (Server::server->IsServerReady())
						{
							PrintData chData =
							{
								.indentationLength = 0,
								.addTimeStamp = true,
								.severity = sev_w,
								.customTag = "CLOUDFLARE",
								.message = "Connection '" + to_string(index) + "' has been marked healthy!"
							};
							unique_ptr<Event> chEvent = make_unique<Event>();
							chEvent->SendEvent(rec_p, chData);
						}

						if (isConn0Healthy
							&& isConn1Healthy
							&& isConn2Healthy
							&& isConn3Healthy
							&& !Server::server->IsServerReady())
						{
							Server::server->SetServerReadyState(true);
							Server::server->Start();
						}
					}

					if (line.find("Unregistered tunnel connection connIndex=") != string::npos)
					{
						size_t pos = line.find("connIndex=") + strlen("connIndex=");
						int index = stoi(line.substr(pos));

						if (index == 0) isConn0Healthy = false;
						else if (index == 1) isConn1Healthy = false;
						else if (index == 2) isConn2Healthy = false;
						else if (index == 3) isConn3Healthy = false;

						PrintData ccData =
						{
							.indentationLength = 0,
							.addTimeStamp = true,
							.severity = sev_w,
							.customTag = "CLOUDFLARE",
							.message = 
								"Connection '" + to_string(index) + "' has been marked unhealthy."
								"Cloudflare could not maintain a tunnel through it."
						};
						unique_ptr<Event> ccEvent = make_unique<Event>();
						ccEvent->SendEvent(rec_c, ccData);
					}

					//strip cloudflare timestamp

					size_t prefixEnd = line.find(' ', 24);
					if (line.size() < 28)
					{
						PrintData clData =
						{
							.indentationLength = 0,
							.addTimeStamp = true,
							.severity = sev_m,
							.customTag = "CLOUDFLARE_LOG",
							.message = line
						};
						unique_ptr<Event> clEvent = make_unique<Event>();
						clEvent->SendEvent(rec_c, clData);
						continue;
					}

					//extract message type

					string typeStr = line.substr(21, 3);
					EventType type = sev_m;
					if (typeStr == "ERR") type = sev_e;
					else if (typeStr == "WRN") type = sev_w;
					else if (typeStr == "DBG") type = sev_d;

					//extract rest of message after message type

					string message = line.substr(25);

					PrintData cl2Data =
					{
						.indentationLength = 0,
						.addTimeStamp = true,
						.severity = type,
						.customTag = "CLOUDFLARE_LOG",
						.message = message
					};
					unique_ptr<Event> cl2Event = make_unique<Event>();
					cl2Event->SendEvent(rec_c, cl2Data);
				}
			}
			CloseHandle(hReadPipe);
		}).detach();
	}

	void CloudFlare::Quit()
	{
		if (!isRunning)
		{
			PrintData sdData =
			{
				.indentationLength = 2,
				.addTimeStamp = true,
				.severity = sev_e,
				.customTag = "CLOUDFLARE",
				.message = "Cannot shut down cloudflared because it hasn't been started!"
			};
			unique_ptr<Event> sdEvent = make_unique<Event>();
			sdEvent->SendEvent(rec_c, sdData);
			return;
		}

		HANDLE rawTunnelRunHandle = reinterpret_cast<HANDLE>(tunnelRunHandle);

		if (rawTunnelRunHandle
			&& rawTunnelRunHandle != INVALID_HANDLE_VALUE)

		{
			if (closeCloudflaredAtShutdown
				&& tunnelRunHandle)
			{
				TerminateProcess(rawTunnelRunHandle, 0);
				CloseHandle(rawTunnelRunHandle);
				tunnelRunHandle = NULL;
			}
		}
		else
		{
			PrintData sdData =
			{
				.indentationLength = 0,
				.addTimeStamp = true,
				.severity = sev_m,
				.customTag = "CLOUDFLARE",
				.message = "Cloudflared was shut down."
			};
			unique_ptr<Event> sdEvent = make_unique<Event>();
			sdEvent->SendEvent(rec_c, sdData);
		}
		
		isRunning = false;

		PrintData cwData =
		{
			.indentationLength = 0,
			.addTimeStamp = true,
			.severity = sev_m,
			.customTag = "CLOUDFLARE",
			.message = "Cloudflared was successfully shut down!"
		};
		unique_ptr<Event> cwEvent = make_unique<Event>();
		cwEvent->SendEvent(rec_c, cwData);
	}
}