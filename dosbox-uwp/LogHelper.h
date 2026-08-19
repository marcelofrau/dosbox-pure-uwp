#pragma once

#include <spdlog/spdlog.h>

// Save real OutputDebugStringA before any macro override, so msvc_sink uses the Win32 API
#pragma push_macro("OutputDebugStringA")
#undef OutputDebugStringA
#include <spdlog/sinks/msvc_sink.h>
#pragma pop_macro("OutputDebugStringA")

#include <string>

inline void LogPrint(const char* msg)
{
    std::string s(msg);
    if (!s.empty() && s.back() == '\n')
        s.pop_back();
    spdlog::info("{}", s);
}

#define OutputDebugStringA(msg) LogPrint(msg)

inline void LogSetLevel(spdlog::level::level_enum level)
{
    spdlog::set_level(level);
}

inline void LogInit()
{
    auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("dbp", sink);
    spdlog::set_default_logger(logger);
#ifdef _DEBUG
    logger->set_level(spdlog::level::debug);
#else
    logger->set_level(spdlog::level::info);
#endif
    OutputDebugStringA("LogInit: msvc_sink registered");
}

inline void LogShutdown()
{
    spdlog::shutdown();
}
