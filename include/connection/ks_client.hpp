//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>

#include "core_utils.hpp"

#include "core/ks_registry.hpp"

namespace KalaServer::Core
{
    class KalaServerCore;
}

namespace KalaServer::Connection
{
    using KalaServer::Core::KalaServerRegistry;

    using std::string;
    using std::string_view;
    using std::vector;
    using std::atomic;
    using std::mutex;

    static constexpr u8 MIN_CLIENT_NAME_LENGTH = 3;
    static constexpr u8 MAX_CLIENT_NAME_LENGTH = 10;

    class LIB_API Client
    {
    friend class KalaServer::Core::KalaServerCore;
    friend class Server;
    friend class Link;
    public:
        static bool IsInitialized();
        //Create a new client with a username for identification
        static void Initialize(string_view username);

        static vector<u32> GetLinkIDs();

        static bool IsVisible();
        static void SetVisibility(bool newValue);

        static string GetUsername();

        //Disconnects the client from its target server if connected to one
        static void Destroy();
    };
}