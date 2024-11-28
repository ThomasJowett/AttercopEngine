#ifndef PROFILER_HPP
#define PROFILER_HPP

#ifdef ATCP_ENABLE_TRACY
#ifndef TRACY_ENABLE
#define TRACY_ENABLE
#endif

#include <Tracy.hpp>

#if defined(__FUNCSIG__)
#define FUNC_SIG __FUNCSIG__
#elif defined(__PRETTY_FUNCTION__)
#define FUNC_SIG __PRETTY_FUNCTION__
#else
#define FUNC_SIG __func__
#endif

#define PROFILE_FRAME() FrameMark
#define PROFILE_SCOPE(name) ZoneScopedN(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(FUNC_SIG)
#define PROFILE_TAG(x, y) ZoneText(x, strlen(x))
#define PROFILE_LOG(text, size) TracyMessage(text, size)
#define PROFILE_VALUE(text, value) TracyPlot(text, value)
#define PROFILE_ALLOC(ptr, size) TracyAlloc(ptr, size)
#define ROFILE_FREE(ptr) TracyFree(ptr)

#else
#define PROFILE_FRAME()
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#define PROFILE_TAG(x, y)
#define PROFILE_LOG(text, size)
#define PROFILE_VALUE(text, value)
#define PROFILE_ALLOC(ptr, size)
#define ROFILE_FREE(ptr)
#endif

#endif // PROFILER_HPP
