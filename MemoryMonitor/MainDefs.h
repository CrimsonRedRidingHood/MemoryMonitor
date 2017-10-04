#pragma once

#include <windows.h>
#include <sddl.h>
#include <Psapi.h>

#ifndef  __MAIN_DEFS_H
#define __MAIN_DEFS_H

#define MEMORY_MONITOR_SERVICE_NAME TEXT("MemoryMonitor")

#define ERROR_ENUM_PROCESS_MODULES_FAILED 0x00010001
#define ERROR_OPEN_PROCESS_FAILED 0x00010002
#define ERROR_COULDNT_TERMINATE_PROCESS 0x00010003
#define ERROR_ALREADY_RUNNING 0x00010004
#define ERROR_NOT_RUNNING 0x00010005
#define ERROR_REGISTER_SERVICE_CTRL_HANDLER_FAILED 0x00010006
#define ERROR_SET_SERVICE_STATUS_FAILED 0x00010007
#define ERROR_CREATE_EVENT_FAILED 0x00010008
#define ERROR_SERVICE_WRONG_USAGE 0x00010009

#define IGNORED 0xFFFF0000 // just a random non-zero value

#define EXIT_CODE_KILLED_BY_MEMORY_MANAGER 0x00FF02EE

#define LONG_DOUBLE_COMPARISION_IMPRECISION 1E-6

#define thread_routine DWORD CALLBACK

typedef DWORD dword;
typedef HANDLE handle;

#ifdef _UNICODE
typedef std::wstring _tcstdstring;
#else
typedef std::string _tcstdstring;
#endif /* _UNICODE */

#endif /* __MAIN_DEFS_H */