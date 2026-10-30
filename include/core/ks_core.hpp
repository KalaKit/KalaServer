//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <functional>
#include <source_location>

#include "core_utils.hpp"

namespace KalaServer::Core
{
    using std::string;
    using std::string_view;
    using std::filesystem::path;
    using std::vector;
    using std::function;
    using std::source_location;

    //wait this period until doing a full HasInternet check again,
    //if within this range then always return as true
    static constexpr u8 INTERNET_CHECK_WAIT_PERIOD_S = 10;

    class LIB_API KalaServerCore
    {
    public:
        static bool IsInitialized();
        static bool Initialize(string_view newValue);

        static string ErrorToString(int error);

        static const string& GetIP();

        static bool IsValidIP(string_view targetValue);

        static bool HasInternet();

		KNODISCARD
		static u32 GetGlobalID();
		static void SetGlobalID(u32 newID);
        
        KNODISCARD
        static path GetExePath();

        //Main once-per-frame update call, syncs server and client worker thread events with current stored data 
        static void Update();

        static void SetExternalHandler(function<void(string, string, const source_location&)>&& newExternalHandler);

        KNORETURN
        static void ForceClose(
            string_view title,
            string_view reason,
            const source_location& location = source_location::current());

        //Assign a shutdown callback to decide what happens besides just KalaServer shutdown when Shutdown is called
        static void SetShutdownCallback(function<void()> shutdownCallback);

        //Gracefully shut down KalaServer and all its servers and clients,
        //set closeExecutableAfterShutdown if you also want to close the executable once KalaServer shutdown is complete,
        //assign a function via SetShutdownCallback to decide what happens after KalaServer resources are cleaned
        //and before KalaServer closes the executable if closeExecutableAfterShutdown is true
        static void Shutdown(bool closeExecutableAfterShutdown = false);
    };
}