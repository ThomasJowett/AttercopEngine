#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <spdlog/spdlog.h>

#include "Debug.hpp"

namespace atcp {

class Logger
{
public:
	static void Init(const std::string& name);

	inline static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

private:
	static std::shared_ptr<spdlog::logger> s_Logger;
};
}

// Fatal error only to be called when the application is about to crash
#define LOG_CRITICAL(...)	atcp::Logger::GetLogger()->critical(__VA_ARGS__);\
								DBG_OUTPUT("\n")\

// Serious issue and a failure of something important
#define LOG_ERROR(...)		atcp::Logger::GetLogger()->error(__VA_ARGS__);\
								DBG_OUTPUT("\n")\

// Indicates you may have a problem or unusual situation
#define LOG_WARN(...)		atcp::Logger::GetLogger()->warn(__VA_ARGS__);\
								DBG_OUTPUT("\n")\

// Normal application behaviour
#define LOG_INFO(...)		atcp::Logger::GetLogger()->info(__VA_ARGS__)

#ifdef DEBUG
// Diagnostic information to help the understand the flow of the engine
#define LOG_DEBUG(...)		atcp::Logger::GetLogger()->debug(__VA_ARGS__);\
								DBG_OUTPUT("\n")\

// Very fine detailed Diagnostic information
#define LOG_TRACE(...)		atcp::Logger::GetLogger()->trace(__VA_ARGS__)

#else
// Nothing logged unless in debug mode
#define LOG_DEBUG(...)
// Nothing logged unless in debug mode
#define LOG_TRACE(...)
#endif // DEBUG


#endif // LOGGER_HPP
