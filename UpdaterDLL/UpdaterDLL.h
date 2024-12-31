// UpdaterDLL.h

#pragma once

#ifdef UPDATERDLL_EXPORTS
#define UPDATERDLL_API __declspec(dllexport)
#else
#define UPDATERDLL_API __declspec(dllimport)
#endif

extern "C" UPDATERDLL_API void __stdcall CheckUpdates(const char* appPath);
