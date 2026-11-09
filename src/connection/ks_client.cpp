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
#include "connection/ks_link.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaString::IsStringInRange;

using KalaServer::Core::KalaServerCore;
using KalaServer::Connection::Client;

using std::string;
using std::to_string;
using std::vector;
using std::atomic;
using std::shared_mutex;
using std::scoped_lock;
using std::shared_lock;

#if defined(KWIN_ANY)
using ksocket = SOCKET;
constexpr ksocket invalid_socket = INVALID_SOCKET;
#define kclose(targetSocket) closesocket(targetSocket)
#else
using ksocket = int;
constexpr ksocket invalid_socket = -1;
#define kclose(targetSocket) close(targetSocket)
#endif

static shared_mutex userMutex{};

static bool isInitialized{};
static bool isVisible{};

static string username{};

static vector<u32> linkIDs{};

namespace KalaServer::Connection
{
    bool Client::IsInitialized()
    {
        shared_lock lock(userMutex);
        return isInitialized;
    }
    void Client::Initialize(string_view newUsername)
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to initialize client because KalaServer has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        if (KalaServerCore::IsShuttingDown())
        {
            Log::Print(
                "Failed to initialize client because KalaServer is shutting down!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        scoped_lock lock(userMutex);

        if (isInitialized)
        {
            Log::Print(
                "Failed to initialize client because it has already been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        if (newUsername.empty())
        {
            Log::Print(
                "Failed to initialize client because the username was empty!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        bool isStringInRange{};
        string err = IsStringInRange(
            newUsername, 
            MIN_CLIENT_NAME_LENGTH, 
            MAX_CLIENT_NAME_LENGTH,
            isStringInRange);

        if (!err.empty())
        {
            Log::Print(
                "Failed to initialize client! Reason: " + err,
                "KS_CLIENT", 
                LogType::LOG_WARNING);

            return;
        }
        if (!isStringInRange)
        {
            Log::Print(
                "Failed to initialize client because the username length was out of range!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        username = newUsername;

        isInitialized = true;

        Log::Print(
            "Created new client '" + username + "'!",
            "KS_CLIENT",
            LogType::LOG_SUCCESS);
    }

    vector<u32> Client::GetLinkIDs() 
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to get client link IDs because KalaServer has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return {};
        }

        shared_lock lock(userMutex);

        if (!isInitialized)
        {
            Log::Print(
                "Failed to get client link IDs because it has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return {};
        }

        return linkIDs;
    }

    bool Client::IsVisible()
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to get client visibility state because KalaServer has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return {};
        }

        shared_lock lock(userMutex);

        if (!isInitialized)
        {
            Log::Print(
                "Failed to get client visibility state because it has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return false;
        }

        return isVisible;
    }
    void Client::SetVisibility(bool newValue)
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to set client visibility state because KalaServer has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        scoped_lock lock(userMutex);

        if (!isInitialized)
        {
            Log::Print(
                "Failed to set client visibility state because it has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        if (newValue
            && isVisible)
        {
            Log::Print(
                "Failed to set client '" + username + "' visible because it already was visible!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }
        if (!newValue
            && !isVisible)
        {
            Log::Print(
                "Failed to set client '" + username + "' invisible because it already was invisible!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        isVisible = newValue;

        Log::Print(
            "Changed client '" + username + "' visibility to '" + (isVisible ? "on" : "off") + "'.",
            "KS_CLIENT",
            LogType::LOG_SUCCESS);
    }

    string Client::GetUsername()
    {
        shared_lock lock(userMutex);
        return username;
    }

    void Client::Destroy()
    {
        if (!KalaServerCore::IsInitialized())
        {
            Log::Print(
                "Failed to destroy client because KalaServer has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        string usernameCopy{};
        vector<u32> linkIDsCopy{};

        {
            scoped_lock lock(userMutex);

            if (!isInitialized)
            {
                Log::Print(
                    "Failed to destroy client because it has not been initialized!",
                    "KS_CLIENT",
                    LogType::LOG_WARNING);

                return;
            }

            isInitialized = false;
            isVisible = false;

            usernameCopy = username;
            linkIDsCopy = linkIDs;
            
            linkIDs.clear();
        }

        for (const u32 lid : linkIDsCopy)
        {
            Link* l{};

            string err = Link::GetRegistry().GetContent(lid, l);
            if (!err.empty())
            {
                KalaServerCore::ForceClose(
                    "KalaServer Client error",
                    "Failed to destroy client '" + usernameCopy 
                    + "' because its link '" + to_string(lid) + "' was invalid!");
            }

            l->Destroy();
        }

		Log::Print(
			"Destroying client '" + usernameCopy + "'.",
			"KS_CLIENT",
			LogType::LOG_INFO);
    }
}