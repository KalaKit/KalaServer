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

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::FromVar;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaString::IsStringInRange;

using KalaServer::Core::KalaServerCore;
using KalaServer::Core::KalaServerRegistry;
using KalaServer::Connection::Client;

using std::string;
using std::to_string;
using std::vector;
using std::atomic;
using std::mutex;
using std::scoped_lock;

#if defined(KWIN_ANY)
using ksocket = SOCKET;
constexpr ksocket invalid_socket = INVALID_SOCKET;
#define kclose(targetSocket) closesocket(targetSocket)
#else
using ksocket = int;
constexpr ksocket invalid_socket = -1;
#define kclose(targetSocket) close(targetSocket)
#endif

static bool isInitialized{};

static mutex linkIDsMutex{};
static vector<u32> linkIDs{};

static atomic<bool> isVisible{};

static string username{};

namespace KalaServer::Connection
{
    bool Client::IsInitialized() { return isInitialized; }
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
        if (!IsStringInRange(newUsername, MIN_CLIENT_NAME_LENGTH, MAX_CLIENT_NAME_LENGTH))
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

        if (!isInitialized)
        {
            Log::Print(
                "Failed to get client link IDs because it has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return {};
        }

        scoped_lock lock(linkIDsMutex);
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

        if (!isInitialized)
        {
            Log::Print(
                "Failed to get client visibility state because it has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return false;
        }

        return isVisible.load();
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

        if (!isInitialized)
        {
            Log::Print(
                "Failed to set client visibility state because it has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        if (newValue
            && isVisible.load())
        {
            Log::Print(
                "Failed to set client '" + username + "' visible because it already was visible!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }
        if (!newValue
            && !isVisible.load())
        {
            Log::Print(
                "Failed to set client '" + username + "' invisible because it already was invisible!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        isVisible.store(newValue);

        Log::Print(
            "Changed client '" + username + "' visibility to '" + (isVisible.load() ? "on" : "off") + "'.",
            "KS_CLIENT",
            LogType::LOG_SUCCESS);
    }

    const string& Client::GetUsername() { return username; }

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

        if (!isInitialized)
        {
            Log::Print(
                "Failed to destroy client because it has not been initialized!",
                "KS_CLIENT",
                LogType::LOG_WARNING);

            return;
        }

        isVisible = false;

        for (const u32 lid : GetLinkIDs())
        {
            Link* l{};

            string err = Link::GetRegistry().GetContent(lid, l);
            if (!err.empty())
            {
                KalaServerCore::ForceClose(
                    "KalaServer Client error",
                    "Failed to destroy client '" + username 
                    + "' because its link '" + to_string(lid) + "' was invalid!");
            }

            l->Destroy();
        }

        {
            scoped_lock lock(linkIDsMutex);
            linkIDs.clear();
        }

        isInitialized = false;

		Log::Print(
			"Destroying client '" + username + "'.",
			"KS_CLIENT",
			LogType::LOG_INFO);
    }
}