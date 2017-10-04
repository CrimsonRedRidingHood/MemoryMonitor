#include <windows.h>
#include <sddl.h>
#include <Psapi.h>

#include <unordered_set>
#include <string>
#include <iostream>
#include <fstream>

#include "MainDefs.h"
#include "MemMon.hpp"
#include "Service.hpp"

using namespace std;

int main( int argc, char ** argv ) // change to WinMain and SUBSYSTEM to WINDOWS when finished
{
	
	if (argc < 2)
	{
		cout << "Creating Service..." << endl;

		DWORD errorCode = CreateMemoryMonitorService( NULL );

		if (errorCode)
		{
			cout << "Error code: " << errorCode << endl;
		}
		else
		{
			cout << "Service has been successfully created." << endl;
		}
		cout << "Press enter to quit...";

		getchar();
 	}
	else
	{
		ServiceSetUp( MEMORY_MONITOR_SERVICE_NAME );
	}

	return ERROR_SUCCESS;
	
	/*
	MemoryMonitor * memMonitor = new MemoryMonitor(
		150000 * 1024,
		2.0,
		TEXT("whitelist.txt"),
		10000
	);

	string request = "";

	cin >> request;

	while (request != "exit")
	{
		if (request == "start")
		{
			if (memMonitor->Start() != ERROR_SUCCESS)
			{
				cout << "Already running" << endl;
			}
		}
		else
		{
			if (request == "stop")
			{
				if (memMonitor->Stop() != ERROR_SUCCESS)
				{
					cout << "Not running" << endl;
				}
			}
			else
			{
				cout << "Unknown request. Possible: start, stop, exit" << endl;
			}
		}

		cin >> request;
	}

	if (memMonitor->IsProcessing())
	{
		cout << "Waiting for return..." << endl;
		memMonitor->Stop();
	}

	delete memMonitor;

	cout << "Done" << endl;

	return ERROR_SUCCESS;
	*/	
}