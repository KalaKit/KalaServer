#pragma once

#include <string>
#include <functional>

namespace MiniWeb
{
	using std::function;
	using std::string;
	using RouteHandler = function<string(const string& request)>;

	class Server
	{
	public:
		Server(int port = 8080);
		void Route(const string& path, RouteHandler handler);
		void Run() const;
	private:
		int port = -1;
	};
}