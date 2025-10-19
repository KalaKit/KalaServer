//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "KalaHeaders/core_utils.hpp"

namespace KalaServer::Core
{
	class KalaServerCore
	{
	public:
		static void Initialize();
		static inline bool IsInitialized() { return isInitialized; }

		static void SetRunningState(bool newState) { isRunning = newState; }
		static inline bool IsRunning() { return isRunning; }

		static void Run();

		static void Shutdown();
	private:
		static inline bool isInitialized{};
		static inline bool isRunning{};
	};
}