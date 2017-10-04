#pragma once

#ifndef __MEMORY_MONITOR_H
#define __MEMORY_MONITOR_H

#include <windows.h>
#include <string>
#include <unordered_set>
#include <fstream>

#include "MainDefs.h"

using namespace std;

string toLowerCaseA(string str)
{
	string out;

	out.resize(str.length());

	for (int i = 0; i < str.length(); i++)
	{
		if ((str[i] >= 'A') && (str[i] <= 'Z'))
		{
			out[i] = str[i] - 'A' + 'a';
		}
		else
		{
			out[i] = str[i];
		}
	}

	return out;
}

#define ft2u64(ft) ((((uint64_t)((ft).dwHighDateTime))<<32)|((uint64_t)((ft).dwLowDateTime)))

thread_routine MonitorStart(void * memoryMonitor);

class MemoryMonitor
{
private:
	unordered_set<string> whiteList;

	size_t memoryUsageThreshold;
	long double cpuUsageThreshold;

	volatile short isProcessing;

	handle workerThread;

	dword millisecondsOperationInterval;

	class ProcessWorker
	{
	public:
		static size_t GetProcessMemoryUsage(handle process)
		{
			PROCESS_MEMORY_COUNTERS data;
			GetProcessMemoryInfo(process, &data, sizeof(data));

			return data.WorkingSetSize;
		}

		static long double GetProcessCPUUsage(handle process)
		{
			const int zeroUsageFixAttempts = 10; // the number of repetitions that we undertake to fix getting 0 CPU usage

			FILETIME ftcreation, ftexitting, ftumode, ftkmode, ftsysidle, ftsysumode, ftsyskmode;

			uint64_t kmode, umode, sysidle, syskmode, sysumode;
			uint64_t proctotal1, systotal1, proctotal2, systotal2;

			long double cpupercentage = 0.0;

			GetProcessTimes(process, &ftcreation, &ftexitting, &ftkmode, &ftumode);
			GetSystemTimes(&ftsysidle, &ftsyskmode, &ftsysumode);

			kmode = ft2u64(ftkmode);
			umode = ft2u64(ftumode);
			sysidle = ft2u64(ftsysidle);
			syskmode = ft2u64(ftsyskmode);
			sysumode = ft2u64(ftsysumode);

			proctotal1 = umode + kmode;
			systotal1 = syskmode + sysumode;

			int attemptsCounter = 0;

			// sometimes, despite the Sleep(100) right below, GetProcess/SystemTimes
			// keeps returning same shit. So, as we loop the next code, the shit
			// gets a few chances to be updated
			do
			{
				Sleep(100);

				GetProcessTimes(process, &ftcreation, &ftexitting, &ftkmode, &ftumode);
				GetSystemTimes(&ftsysidle, &ftsyskmode, &ftsysumode);

				kmode = ft2u64(ftkmode);
				umode = ft2u64(ftumode);
				sysidle = ft2u64(ftsysidle);
				syskmode = ft2u64(ftsyskmode);
				sysumode = ft2u64(ftsysumode);

				proctotal2 = umode + kmode;
				systotal2 = syskmode + sysumode;

				cpupercentage = ((long double)(proctotal2 - proctotal1)) / ((long double)(systotal2 - systotal1));
			} while ((fabsl(cpupercentage) < LONG_DOUBLE_COMPARISION_IMPRECISION)
				&& (++attemptsCounter < zeroUsageFixAttempts));

			return cpupercentage;
		}

		// Process name always returned lower-cased
		static string GetProcessName(handle process)
		{
			char processName[MAX_PATH];

			// Get the process name.

			dword lastError = ERROR_SUCCESS;

			HMODULE hMod[32];
			dword bytesNeeded;

			if (EnumProcessModulesEx(process, hMod, sizeof(hMod),
				&bytesNeeded, LIST_MODULES_ALL))
			{
				if (!GetModuleBaseNameA(process, hMod[0], processName,
					sizeof(processName) / sizeof(char)) != ERROR_SUCCESS)
				{
					return "<GetModuleBaseNameA() failed>";
				}
			}
			else
			{
				return "<EnumProcessModules() failed>";
			}

			return processName;
		}

		static dword KillProcess(dword pid)
		{
			handle process = OpenProcess(
				PROCESS_TERMINATE,
				NULL,
				pid
			);

			if (process == NULL)
			{
				return ERROR_OPEN_PROCESS_FAILED;
			}

			dword errorCode = ERROR_SUCCESS;

			if (!TerminateProcess(process, EXIT_CODE_KILLED_BY_MEMORY_MANAGER))
			{
				errorCode = ERROR_COULDNT_TERMINATE_PROCESS;
			}

			CloseHandle(process);

			return errorCode;
		}
	};

public:

	short IsProcessing()
	{
		return InterlockedAnd16(&isProcessing, 1);
	}

	dword Start()
	{
		if (InterlockedOr16(&isProcessing, 1))
		{
			return ERROR_ALREADY_RUNNING;
		}

		workerThread = CreateThread(
			NULL,
			0,
			MonitorStart,
			this,
			0,
			NULL
		);

		return ERROR_SUCCESS;
	}

	dword Stop()
	{
		if (!InterlockedAnd16(&isProcessing, 0))
		{
			return ERROR_NOT_RUNNING;
		}

		WaitForSingleObject(workerThread, INFINITE);

		CloseHandle(workerThread);

		return ERROR_SUCCESS;
	}

	dword GetInterval()
	{
		return millisecondsOperationInterval;
	}

	dword LoadTheWhiteList(_tcstdstring filename) // Works Fine
	{
		string currentName;

		ifstream file;
		file.open(filename, ios::in | ios::binary);

		if (file.fail() | file.bad())
		{
			return ERROR_OPEN_FAILED;
		}

		file >> currentName;

		do
		{
			whiteList.insert(toLowerCaseA(currentName));
			file >> currentName;

		} while (!file.eof());

		file.close();

		return ERROR_SUCCESS;
	}

	dword ProcessProcess(dword pid)
	{
		handle process = OpenProcess(
			PROCESS_QUERY_INFORMATION |
			PROCESS_VM_READ,
			FALSE, pid);

		if (process == NULL)
		{
			return ERROR_OPEN_PROCESS_FAILED;
		}

		long double cpuusage;

		if ((ProcessWorker::GetProcessMemoryUsage(process) > memoryUsageThreshold)
			&& (((cpuusage = ProcessWorker::GetProcessCPUUsage(process))) < cpuUsageThreshold))
		{
			if (whiteList.find(toLowerCaseA(ProcessWorker::GetProcessName(process))) == whiteList.end())
			{
				ProcessWorker::KillProcess( pid );
				//printf("pid: %d, cpu: %f\r\n", pid, cpuusage);
			}
		}

		CloseHandle(process);

		return ERROR_SUCCESS;
	}

	MemoryMonitor(
		size_t bytesMemoryUsageThreshold,
		long double percentageCpuUsagePercentageThreshold,
		_tcstdstring whiteListFileName,
		dword millisecondsInterval) :

		memoryUsageThreshold(bytesMemoryUsageThreshold),
		cpuUsageThreshold(percentageCpuUsagePercentageThreshold / 100.0),
		isProcessing(0),
		millisecondsOperationInterval(millisecondsInterval)
	{
		whiteList.clear();

		if ((percentageCpuUsagePercentageThreshold < 0.0) || (percentageCpuUsagePercentageThreshold - 100.0 > LONG_DOUBLE_COMPARISION_IMPRECISION))
		{
			throw TEXT("CPU Usage Percentage Threshold must a non-negative value between 0.0 and 100.0");
		}

		if (LoadTheWhiteList(whiteListFileName) != ERROR_SUCCESS)
		{
			throw TEXT("Couldn't read the whilelist");
		}
	}

	~MemoryMonitor()
	{
		Stop();

		whiteList.clear();
	}
};

thread_routine MonitorStart(void * memoryMonitor)
{
	MemoryMonitor * memMonitor = (MemoryMonitor *)memoryMonitor;

	dword pids[1024];
	dword processCount;

	dword sleepTime = memMonitor->GetInterval();

	dword i = 0;

	while (memMonitor->IsProcessing())
	{
		EnumProcesses(pids, sizeof(pids), &processCount);
		processCount /= sizeof(dword);

		for (i = 0; i < processCount; i++)
		{
			memMonitor->ProcessProcess(pids[i]);
		}

		Sleep(sleepTime);
	}

	return ERROR_SUCCESS;
}

#endif /* __MEMORY_MONITOR_H */