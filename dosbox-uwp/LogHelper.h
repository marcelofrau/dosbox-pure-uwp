#pragma once

// After Windows headers — replaces OutputDebugStringA with file+debug logger
#define OutputDebugStringA(msg) LogPrint(msg)

void LogInit();
void LogPrint(const char* msg);
void LogShutdown();
