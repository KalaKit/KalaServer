//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include "log_utils.hpp"

#include "core/ks_core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaServer::Core::KalaServerCore;

int main()
{
    Log::Print("start tests", "website_test", LogType::LOG_INFO);

    KalaServerCore::Initialize("192.168.1.1");

    Log::Print("end tests", "website_test", LogType::LOG_INFO);

    return 0;
}