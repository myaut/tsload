/*
 * tsinit.c
 *
 *  Created on: 30.12.2012
 *      Author: myaut
 */

#include <tsinit.h>

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
