#include "Logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

namespace atcp {
std::shared_ptr<spdlog::logger> Logger::s_Logger;
void Logger::Init(const std::string& name)
{
	std::string logFilename = "Log.txt";

	std::vector<spdlog::sink_ptr>logSinks;
	//std::cout
	logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	logSinks.back()->set_pattern("%^[%T] %n: %v%$");
	//log file
	logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilename, true));
	logSinks.back()->set_pattern("[%d/%m/%Y] [%T] [%l] %n: %v");

	// Internal Console
	//logSinks.emplace_back(std::make_shared<InternalConsoleSink_mt>());
	//logSinks.back()->set_pattern("%^[%T] [%l] %n: %v%$");
#ifdef _MSC_VER
	// msvc output
	logSinks.emplace_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
	logSinks.back()->set_pattern("%n: %v");
#endif


	s_Logger = std::make_shared<spdlog::logger>(name, begin(logSinks), end(logSinks));
	spdlog::register_logger(s_Logger);
	s_Logger->set_level(spdlog::level::trace);
	s_Logger->flush_on(spdlog::level::trace);
}
}