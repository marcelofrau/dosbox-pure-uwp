#include "pch.h"

// Undef macro so this file uses real OutputDebugStringA directly
#ifdef OutputDebugStringA
#undef OutputDebugStringA
#endif

#include "LogHelper.h"
#include <cstdio>
#include <ctime>

static FILE* g_logFile = nullptr;

void LogInit()
{
    ::OutputDebugStringA("[dosbox-uwp] LogInit\n");

    // Try creating directories. Silent fail if access denied (desktop UWP).
    CreateDirectoryA("D:\\dosbox", NULL);
    CreateDirectoryA("D:\\dosbox\\logs", NULL);
    CreateDirectoryA("D:\\dosbox\\config", NULL);

    // Session log with timestamp
    time_t now = time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);
    char filename[MAX_PATH];
    strftime(filename, sizeof(filename), "D:\\dosbox\\logs\\dosbox-pure-%Y%m%d-%H%M%S.log", &tm);

    if (fopen_s(&g_logFile, filename, "w") == 0 && g_logFile)
    {
        fprintf(g_logFile, "[dosbox-uwp] Log initialized\n");
        fflush(g_logFile);
    }
}

void LogPrint(const char* msg)
{
    ::OutputDebugStringA(msg);
    if (g_logFile)
    {
        fputs(msg, g_logFile);
        fflush(g_logFile);
    }
}

void LogShutdown()
{
    if (g_logFile)
    {
        fprintf(g_logFile, "[dosbox-uwp] Log shutdown\n");
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}
