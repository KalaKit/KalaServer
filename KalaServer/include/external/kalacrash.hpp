//Copyright(C) 2025 Lost Empire Entertainment
//This header comes with ABSOLUTELY NO WARRANTY.
//This is free code, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

//This header is a part of the KalaKit KalaHeaders repository: https://github.com/KalaKit/KalaHeaders

// ======================================================================
//  Terminates when Crash() is called, aborting all actions 
//  and creating a crash log at ./kalacrash_index.txt
// ======================================================================

#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <atomic>

namespace KalaKit::KalaCrash
{
	using std::string;
	using std::to_string;
	using std::ofstream;
	using std::ios;
	using std::terminate;
	using std::cerr;
	using std::atomic;
	using std::memory_order_relaxed;
	using std::filesystem::path;
	using std::filesystem::current_path;
	
	// ======================================================================
	// Creates a thread-safe crash log to executable path. 
	// The crash log always starts at 1, so old ones are overwritten.
	// Terminates executable immediately right after crash log is created.
	// 
	// Usage:
	//   Crash("crash reason");
	// ======================================================================
	
	static inline atomic<int> crashIndexCounter{0};
	static void Crash(const string& reason)
	{
		int index = crashIndexCounter.fetch_add(1, memory_order_relaxed) + 1;
		string newCrashName = "kalacrash_" + to_string(index) + ".txt";
		path logPath = current_path() / newCrashName;
		
		ofstream logFile(logPath, ios::binary);
		if (logFile.is_open())
		{
			logFile << "This crash file was created because this application crashed due to an error which was caught by KalaKit::KalaCrash::Crash.\n";
			logFile << "If you are not a developer of this program then please send this file to the developers.\n\n";
			
			logFile << "Crash reason: " << reason;
		}
		else
		{
			cerr << "[FATAL_ERROR]: " << reason << " (failed to write crash log)\n";
			terminate();
		}

		cerr << "[FATAL_ERROR]: " << reason << "\n";
		logFile.close();
		
		terminate();
	}
}