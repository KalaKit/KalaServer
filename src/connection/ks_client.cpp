//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "connection/ks_client.hpp"

#if defined(KWIN_ANY)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <linux/limits.h>
#include <unistd.h>
#endif

#include "log_utils.hpp"
#include "string_utils.hpp"

#include "core/ks_core.hpp"

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::FromVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaString::IsStringInRange;

using KalaServer::Core::KalaServerCore;
using KalaServer::Core::KalaServerRegistry;
using KalaServer::Connection::Client;

using std::to_string;
using std::unique_ptr;
using std::make_unique;

#if defined(KWIN_ANY)
using ksocket = SOCKET;
constexpr ksocket invalid_socket = INVALID_SOCKET;
#define kclose(sock) closesocket(sock)
#else
using ksocket = int;
constexpr ksocket invalid_socket = -1;
#define kclose(sock) close(sock)
#endif

static KalaServerRegistry<Client> registry{};

namespace KalaServer::Connection
{
    KalaServerRegistry<Client>& Client::GetRegistry() { return registry; }

    Client* Client::Initialize(string_view username)
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to initialize client because KalaServer was not initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return nullptr;
        }

        if (username.empty())
        {
            Log::Print(
                "Failed to initialize client because the username was empty!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return nullptr;
        }
        if (!IsStringInRange(username, MIN_CLIENT_NAME_LENGTH, MAX_CLIENT_NAME_LENGTH))
        {
            Log::Print(
                "Failed to initialize client because the username length was out of range!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return nullptr;
        }

        for (const Client* c : registry.GetAllContent())
        {
            if (c->username == username)
            {
                Log::Print(
                    "Failed to initialize client because the username '" + string(username) + "' is already in use!",
                    "KS_CLIENT",
                    LogType::LOG_WARNING);

                return nullptr;
            }
        }

		u32 newID = KalaServerCore::GetGlobalID() + 1;
		KalaServerCore::SetGlobalID(newID);

		unique_ptr<Client> newClient = make_unique<Client>();
		Client* clientPtr = newClient.get();

        clientPtr->ID = newID;
        clientPtr->username = username;

		string err = registry.AddContent(
			newID, 
			std::move(newClient));
		if (!err.empty())
		{
			KalaServerCore::ForceClose(
				"KalaServer Client error",
				"Failed to initialize client! Reason: " + err);
		}

		Log::Print(
			"Created new client '" + to_string(newID) + "'!",
			"KS_CLIENT",
			LogType::LOG_SUCCESS);

		return clientPtr;
    }

    u32 Client::GetID() const { return ID; }
    u32 Client::GetServerID() const { return serverID; }

    void Client::ConnectToServer(u32 serverID)
    {

    }
    void Client::ConnectToServer(u16 serverPort)
    {

    }
    void Client::DisconnectFromServer()
    {

    }

    const string& Client::GetUsername() const { return username; }
    void Client::SetUsername(string_view newValue)
    {

    }

    void Client::Destroy()
    {
		string err = registry.DestroyContent(ID);
		if (!err.empty())
		{
			KalaServerCore::ForceClose(
				"KalaServer Client error",
				"Failed to destroy client! Reason: " + err);
		}
    }

    Client::~Client()
    {
		Log::Print(
			"Destroying client '" + to_string(ID) + "'.",
			"KS_CLIENT",
			LogType::LOG_INFO);
    }
}