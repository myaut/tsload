
/*
    This file is part of TSLoad.
    Copyright 2012-2013, Sergey Klyaus, ITMO University

    TSLoad is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 3.

    TSLoad is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TSLoad.  If not, see <http://www.gnu.org/licenses/>.    
*/  



#include <tsload/defs.h>

#include <tsload/init.h>

#include <windows.h>


typedef NTSTATUS (NTAPI *NtDelayExecution_func)(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
NtDelayExecution_func NtDelayExecution;

int plat_read_ntdll(void) {
	HMODULE hModule = LoadLibrary("ntdll.dll");
	int ret = 0;

	if(!hModule) {
		return 1;
	}

	NtDelayExecution = (NtDelayExecution_func) GetProcAddress(hModule, "NtDelayExecution");

	if(!NtDelayExecution) {
		ret = 1;
	}

	FreeLibrary(hModule);

	return ret;
}

/* atexit doesn't works if user closes console window
 * or sends Ctrl+C, so handle it manually */
BOOL plat_console_handler(DWORD ctrl_type) {
	ts_finish();

	return TRUE;
}

PLATAPI int plat_init(void) {
	/* Suppress messageboxes with failures */
	SetErrorMode(SEM_FAILCRITICALERRORS);
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) plat_console_handler, TRUE);

	return plat_read_ntdll();
}

