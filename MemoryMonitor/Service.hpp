#pragma once

#ifndef __SERVICE_H
#define __SERVICE_H

#include <windows.h>
#include <tchar.h>

#include "MainDefs.h"
#include "MemMon.hpp"

SERVICE_STATUS gServiceStatus = { 0 };
SERVICE_STATUS_HANDLE gServiceStatusHandle = NULL;

HANDLE gServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR * argv); // The main function, that does all the registering and other config stuff
VOID WINAPI ServiceCtrlHandler(DWORD msg);
DWORD CALLBACK ServiceEntry(LPVOID param); // Function, that does the work

BOOL UpdateServiceStatus(
	DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwServiceSpecificExitCode,
	DWORD dwWaitHint
);

TCHAR ServiceName[0x100];

DWORD ServiceSetUp( LPCTSTR serviceName )
{
	_tcscpy_s( ServiceName, 0x100, serviceName );
	SERVICE_TABLE_ENTRY entryTable[] =
	{
		{ ServiceName, (LPSERVICE_MAIN_FUNCTION) ServiceMain },
		{ NULL, NULL }
	};

	if (!StartServiceCtrlDispatcher(entryTable))
	{
		return GetLastError();
	}

	return ERROR_SUCCESS;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR * argv)
{
	TCHAR filePath[MAX_PATH];
	DWORD status = E_FAIL;

	if (argc < 2)
	{
		if (!UpdateServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR, ERROR_SERVICE_WRONG_USAGE, 0))
		{
			OutputDebugString( TEXT("UpdateServiceStatus() failed (before calling RegisterServiceCtrlHandler)") );
		}
		return;
	}

	gServiceStatusHandle = RegisterServiceCtrlHandler( ServiceName, ServiceCtrlHandler );

	_tcscpy_s( filePath, MAX_PATH, argv[1] );

	if (gServiceStatusHandle == NULL)
	{
		return;
	}

	ZeroMemory( &gServiceStatus, sizeof(gServiceStatus) );
	gServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

	if (!UpdateServiceStatus(SERVICE_START_PENDING, NO_ERROR, ERROR_SUCCESS, 0))
	{
		return;
	}

	gServiceStopEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	if (gServiceStopEvent == NULL)
	{
		UpdateServiceStatus( SERVICE_STOPPED, GetLastError(), IGNORED, 0 );

		return;
	}

	if (!UpdateServiceStatus(SERVICE_RUNNING, NO_ERROR, ERROR_SUCCESS, 0))
	{
		return;
	}

	HANDLE mainServiceThread = CreateThread(
		NULL,
		0,
		ServiceEntry,
		filePath,
		0,
		NULL
	);

	WaitForSingleObject(mainServiceThread, INFINITE);

	CloseHandle( mainServiceThread );

	if (!UpdateServiceStatus(SERVICE_STOPPED, NO_ERROR, ERROR_SUCCESS, 0))
	{
		return;
	}
}

DWORD CALLBACK ServiceEntry(LPVOID whiteListPath)
{
	try
	{
		MemoryMonitor * memMonitor = new MemoryMonitor(
			150000 * 1024,
			2.0,
			(LPTSTR)whiteListPath,
			0
		);

		memMonitor->Start();

		WaitForSingleObject(gServiceStopEvent, INFINITE);

		memMonitor->Stop();

		delete memMonitor;
	}
	catch (LPCTSTR exceptionMessage)
	{
		OutputDebugString( exceptionMessage );
		return ERROR_EXCEPTION_IN_SERVICE;
	}

	return ERROR_SUCCESS;
}

VOID WINAPI ServiceCtrlHandler(DWORD msg)
{
	switch (msg)
	{
	case SERVICE_CONTROL_STOP:

		if (gServiceStatus.dwCurrentState == SERVICE_STOPPED)
		{
			break;
		}

		UpdateServiceStatus( SERVICE_STOP_PENDING, NO_ERROR, ERROR_SUCCESS, 0 );

		SetEvent( gServiceStopEvent );
		UpdateServiceStatus( gServiceStatus.dwCurrentState, NO_ERROR, ERROR_SUCCESS, 0 );

		break;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}
}

// ripped from MSDN :D
BOOL UpdateServiceStatus(
	DWORD dwNewState,
	DWORD dwWin32ExitCode,
	DWORD dwServiceSpecificExitCode,
	DWORD dwWaitHint )
{
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.

	gServiceStatus.dwCurrentState = dwNewState;
	gServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
	gServiceStatus.dwWaitHint = dwWaitHint;
	gServiceStatus.dwServiceSpecificExitCode = dwServiceSpecificExitCode;

	if ((dwNewState == SERVICE_START_PENDING) ||
		(dwNewState == SERVICE_STOP_PENDING) ||
		(dwNewState == SERVICE_STOPPED))
	{
		gServiceStatus.dwControlsAccepted = 0;
	}
	else
	{
		gServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	}

	if ((dwNewState == SERVICE_RUNNING) || (dwNewState == SERVICE_STOPPED))
	{
		gServiceStatus.dwCheckPoint = 0;
	}
	else
	{
		gServiceStatus.dwCheckPoint = dwCheckPoint++;
	}

	// Report the status of the service to the SCM.
	if (!SetServiceStatus(gServiceStatusHandle, &gServiceStatus))
	{
		OutputDebugString( TEXT("SetServiceStatus() failed") );
		return FALSE;
	}

	return TRUE;
}

DWORD GetThisExePath(LPTSTR exePath, LPTSTR whiteListPath) // exePath and whiteListPath sizes must be MAX_PATH
{
	int strptr = 0;

	if (!GetModuleFileName( NULL, exePath, sizeof(TCHAR) * MAX_PATH ))
	{
		OutputDebugString(TEXT("GetModuleFileName() failed"));
		return GetLastError();
	}

	_tcscpy_s( whiteListPath, MAX_PATH, exePath );

	// Set strptr to the position of the very .exe file name
	while (whiteListPath[strptr++]);
	while (whiteListPath[--strptr] != '\\');
	strptr++;

	_tcscpy( whiteListPath + strptr, TEXT( "whitelist.txt" ) );

	return ERROR_SUCCESS;
}

DWORD CreateMemoryMonitorService(LPVOID params)
{
	SC_HANDLE scManager = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE );

	if (scManager == NULL)
	{
		return GetLastError();
	}

	TCHAR exePath[MAX_PATH];
	TCHAR whiteListPath[MAX_PATH];

	TCHAR binPathName[1024];

	GetThisExePath( exePath, whiteListPath );

	_tcscpy_s( binPathName, MAX_PATH, exePath );
	_tcscat( binPathName, TEXT( " " ) );
	_tcscat_s( binPathName, MAX_PATH, whiteListPath );

	SC_HANDLE service = CreateService(
		scManager,
		MEMORY_MONITOR_SERVICE_NAME,
		TEXT("Memory Monitor"),
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_IGNORE,
		binPathName,
		NULL,
		NULL,
		NULL,
		NULL,
		TEXT("")
	);

	if (service == NULL)
	{
		return GetLastError();
	}

	LPTSTR whiteListPathLptStr = new TCHAR[MAX_PATH];
	_tcscpy(whiteListPathLptStr, whiteListPath);

	// because we need to pass a pointer to LPCTSTR in StartService,
	// we must have an lvalue
	LPCTSTR whiteListConstLpt = (LPCTSTR)whiteListPathLptStr;
	
	if (!StartService(service, 1, &whiteListConstLpt))
	{
		OutputDebugString( TEXT("StartService() failed") );
		return GetLastError();
	}

	SERVICE_STATUS serviceStatus;

	do
	{
		Sleep(1);
		ControlService(service, SERVICE_CONTROL_INTERROGATE, &serviceStatus);		
	} while ( serviceStatus.dwCurrentState != SERVICE_RUNNING ); // by the time we get to SERVICE_RUNNING, the string has alerady been copied

	delete whiteListPathLptStr;

	CloseServiceHandle( service );
	CloseServiceHandle( scManager );

	return ERROR_SUCCESS;
}

#endif /* __SERVICE_H */