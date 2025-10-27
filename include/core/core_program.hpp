//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "KalaHeaders/math_utils.hpp"

namespace KalaServer::Core
{
	class KalaServerCore
	{
	public:
		static void Initialize();
		static inline bool IsInitialized() { return isInitialized; }

		static void SetRunningState(bool newState) { isRunning = newState; }
		static inline bool IsRunning() { return isRunning; }

		static void UpdateDeltaTime();
		static inline f64 GetDeltaTime() { return deltaTime; }

		static void Run();

		static void Shutdown();
	private:
		static inline f64 deltaTime{};

		static inline bool isInitialized{};
		static inline bool isRunning{};
	};
}