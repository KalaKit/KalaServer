//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <vector>
#include <algorithm>

namespace KalaKit::Core
{
	using std::vector;
	using std::find;

	class KalaServer
	{
	public:
		//Is the server currently running
		static inline bool isRunning = false;

		static bool IsRunningAsAdmin();

		template<typename T>
		static bool VectorContains(const vector<T>& vec, const T& value)
		{
			return find(vec.begin(), vec.end(), value) != vec.end();
		}

		/// <summary>
		/// Runs the server every frame.
		/// </summary>
		static void Run();
			
		static void Quit();
	};
}