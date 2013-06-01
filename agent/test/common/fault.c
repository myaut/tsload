/* Makes Windows fault handling UNIX-friendly:
 * - Emulate "signal exit" when exception caught
 * - Dumps a core of process
 *
 * Also enables corefiles for UNIX*/

#include <defs.h>

#include <stdio.h>
#include <stdlib.h>

#if defined(PLAT_WIN)
#include <windows.h>
#include <dbghelp.h>
#include <signal.h>

/* Defined to be 22 in CRT */
#undef SIGABRT

#define SIGTRAP		5
#define SIGBUS		10
#define SIGABRT		6

int fault_dump(PEXCEPTION_POINTERS exc_info) {
	MINIDUMP_EXCEPTION_INFORMATION md_exc_info;
	PMINIDUMP_EXCEPTION_INFORMATION p_md_exc_info = NULL;
	HANDLE core;
	BOOL ret;

	core = CreateFile(
		"core",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if(core == INVALID_HANDLE_VALUE)
		return 1;

	if(exc_info) {
		md_exc_info.ThreadId = GetCurrentThreadId();
		md_exc_info.ExceptionPointers = exc_info;
		md_exc_info.ClientPointers = TRUE;

		p_md_exc_info = &md_exc_info;
	}

	ret = MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		core,
		MiniDumpWithFullMemory,
		p_md_exc_info,
		NULL,
		NULL
	);

	CloseHandle(core);

	return (int) ret;
}

LONG WINAPI fault_handler(PEXCEPTION_POINTERS exc_info) {
	int sig = 255;

	switch(exc_info->ExceptionRecord->ExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	case EXCEPTION_STACK_OVERFLOW:
		sig = SIGSEGV;
		break;
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_STACK_CHECK:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_INT_OVERFLOW:
		sig = SIGFPE;
		break;
	case EXCEPTION_ILLEGAL_INSTRUCTION:
	case EXCEPTION_PRIV_INSTRUCTION:
		sig = SIGILL;
		break;
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		sig = SIGBUS;
		break;
	case EXCEPTION_BREAKPOINT:
	case EXCEPTION_SINGLE_STEP:
		sig = SIGTRAP;
		break;
	default:
		_exit(255);
		break;
	}

	fault_dump(exc_info);

	/* Should call raise() here.
	 *
	 * However raise() will cause abort() which confuses
	 * run-suite.py by returning code 3 (+SIGABRT)
	 *
	 * So call _exit directly. */
	_exit(-sig);

	/* NOTREACHED */
	return 0;
}

void fault_abort_handler(int signum) {
	fault_dump(NULL);

	_exit(-SIGABRT);
}

int fault_init(void) {
	/* For DEBUG builds, MSVC CRT will report faults with pretty MessageBox
	 * instead of silently killing application. Override this behavior.
	 * */
	_set_abort_behavior(0, _WRITE_ABORT_MSG);

	signal(SIGABRT, fault_abort_handler);

	SetUnhandledExceptionFilter(fault_handler);

	return 0;
}

#elif defined(PLAT_POSIX)

#include <sys/time.h>
#include <sys/resource.h>

int fault_init(void) {
	struct rlimit rl;

	getrlimit(RLIMIT_CORE, &rl);
	rl.rlim_cur = rl.rlim_max = 16 * SZ_MB;
	setrlimit(RLIMIT_CORE, &rl);

	return 0;
}

#else

int fault_init(void) {
	return 0;
}

#endif
