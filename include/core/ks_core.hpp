//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <string>

#include "KalaHeaders/core_utils.hpp"

namespace KalaServer::Core
{
	using std::string;

	class LIB_API KalaServerCore
	{
	public:
		//Force-close the program right this very moment with no cleanups
		[[noreturn]] static void ForceClose(
			const string& target,
			const string& reason);
	};
}