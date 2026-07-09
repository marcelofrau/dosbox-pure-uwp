#pragma once

#ifdef XB_INSPECTOR_ENABLED

#include <spdlog/spdlog.h>
#include <string>

inline void LogPrint(const char* msg)
{
    std::string s(msg);
    if (!s.empty() && s.back() == '\n')
        s.pop_back();
    spdlog::info("{}", s);
}

#define OutputDebugStringA(msg) LogPrint(msg)

#else

__declspec(dllimport) void __stdcall OutputDebugStringA(const char*);

#endif

inline void LogInit() {}
inline void LogShutdown() {}
