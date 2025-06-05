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

namespace KalaKit::KalaCrash
{
	using std::string;
	using std::to_string;
	using std::ofstream;
	using std::ios;
	using std::terminate;
	using std::cerr;
	using std::filesystem::path;
	using std::filesystem::current_path;
	using std::filesystem::directory_iterator;
	
	// ======================================================================
	// Creates a crash log to executable path and terminates immediately after being called.
	// 
	// Usage:
	//   Crash("crash reason");
	// ======================================================================
	
	static void Crash(const string& reason)
	{
		int fileCount = 0;
		
		for (const auto& file : directory_iterator(current_path()))
		{
			path thisFile = file.path();
			if (is_regular_file(thisFile)
				&& thisFile.extension() == ".txt"
				&& thisFile.filename().string().starts_with("kalacrash_"))
			{
				++fileCount;
			}
		}
		
		int index = ++fileCount;
		string newCrashName = "kalacrash_" + to_string(index) + ".txt";
		path logPath = current_path() / newCrashName;
		
		ofstream logFile(logPath, ios::binary);
		if (logFile.is_open())
		{
			logFile << "This crash file was created because this application crashed due to an error which was caught by KalaKit::KalaCrash::Crash.\n";
			logFile << "If you are not a developer of this program then please send this file to the developers.\n\n";
			
			logFile << "Crash reason: " << reason;
		}
		
		cerr << "[FATAL_ERROR]: " << reason << "\n";
		logFile.close();
		
		terminate();
	}
}