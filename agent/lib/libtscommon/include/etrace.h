
/*
    This file is part of TSLoad.
    Copyright 2013-2014, Sergey Klyaus, ITMO University

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



#ifndef ETRACE_H_
#define ETRACE_H_

#include <defs.h>

/**
 * @module Event tracing
 *
 * Cross-platform event tracing subsystem.
 * Supports DTrace or SystemTap USDT and ETW (Event Tracing for Windows)
 *
 * TODO: Type translation
 */

#if defined(ETRC_USE_USDT)

#include <sys/sdt.h>

#define ETRC_DEFINE_PROVIDER(provider, guid)
#define ETRC_DEFINE_EVENT(provider, event, id)

#define etrc_provider_init(provider) 		do { } while(0)
#define etrc_provider_destroy(provider) 	do { } while(0)

#define ETRC_PROBE0(provider, name)			DTRACE_PROBE(provider, name)
#define ETRC_PROBE1(provider, name, type1, arg1)							\
		DTRACE_PROBE1(provider, name, arg1)

#define ETRC_PROBE2(provider, name, type1, arg1, type2, arg2)				\
		DTRACE_PROBE2(provider, name, arg1, arg2)

#define ETRC_PROBE3(provider, name, type1, arg1, type2, arg2, type3, arg3)	\
		DTRACE_PROBE3(provider, name, arg1, arg2, arg3)

#define ETRC_PROBE4(provider, name, type1, arg1, type2, arg2, type3, arg3,	\
		type4, arg4)														\
			DTRACE_PROBE4(provider, name, arg1, arg2, arg3, arg4)

#define ETRC_PROBE5(provider, name, type1, arg1, type2, arg2, type3, arg3,	\
		type4, arg4, type5, arg5)											\
			DTRACE_PROBE5(provider, name, arg1, arg2, arg3, arg4, arg5)

#elif defined(ETRC_USE_ETW)

#include <windows.h>
#include <evntprov.h>

/* Should be linked with Advapi32.lib */

typedef struct {
	REGHANDLE etp_reghandle;
	GUID	  etp_guid;
} etrc_provider_t;

#define ETRC_DEFINE_PROVIDER(provider, guid)							\
	etrc_provider_t provider = 											\
		{ (REGHANDLE) INVALID_HANDLE_VALUE, guid }

#define ETRC_DEFINE_EVENT(provider, event, id)							\
	const EVENT_DESCRIPTOR provider ## _ ## event = 					\
			{ id, 1, 0x10, 0x4, 10 + id, 0, 0x1 }

static void etrc_provider_init(etrc_provider_t* provider) {
	EventRegister(&provider->etp_guid, NULL, NULL, &provider->etp_reghandle);
}

static void etrc_provider_destroy(etrc_provider_t* provider) {
	EventUnregister(provider->etp_reghandle);
}

TSDOC_HIDDEN static ULONG etrc_probe_n(etrc_provider_t* provider, const EVENT_DESCRIPTOR* event, int numargs,
	void* arg1, size_t size1, void* arg2, size_t size2, void* arg3, size_t size3, void* arg4, size_t size4,
	void* arg5, size_t size5, void* arg6, size_t size6) {
	EVENT_DATA_DESCRIPTOR EventData[6];

	switch (numargs) {
	case 6:
		EventDataDescCreate(&EventData[5], arg6, size6);
	case 5:
		EventDataDescCreate(&EventData[4], arg5, size5);
	case 4:
		EventDataDescCreate(&EventData[3], arg4, size4);
	case 3:
		EventDataDescCreate(&EventData[2], arg3, size3);
	case 2:
		EventDataDescCreate(&EventData[1], arg2, size2);
	case 1:
		EventDataDescCreate(&EventData[0], arg1, size1);
	}

	return EventWrite(provider->etp_reghandle, event, numargs, EventData);
}

#define ETRC_PROBE0(provider, name)		etrc_probe_n(&provider, &provider ## _ ## name, 0,	\
		NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0)

#define ETRC_PROBE1(provider, name, type1, arg1)							\
		etrc_probe_n(&provider, &provider ## _ ## name, 1,					\
				(void*) &(arg1), sizeof(type1), NULL, 0, NULL, 0, NULL, 0,  \
				NULL, 0, NULL, 0)

#define ETRC_PROBE2(provider, name, type1, arg1, type2, arg2)				\
		etrc_probe_n(&provider, &provider ## _ ## name, 2,					\
				(void*) &(arg1), sizeof(type1), (void*) &(arg2),			\
				sizeof(type2), NULL, 0, NULL, 0, NULL, 0, NULL, 0)

#define ETRC_PROBE3(provider, name, type1, arg1, type2, arg2, type3, arg3)	\
		etrc_probe_n(&provider, &provider ## _ ## name, 3,					\
				(void*) &(arg1), sizeof(type1), (void*) &(arg2),			\
				sizeof(type2), (void*) &(arg3), sizeof(type3), NULL, 0, 	\
				NULL, 0, NULL, 0)

#define ETRC_PROBE4(provider, name, type1, arg1, type2, arg2, type3, arg3,	\
		type4, arg4)														\
		etrc_probe_n(&provider, &provider ## _ ## name, 4,					\
				(void*) &(arg1), sizeof(type1), (void*) &(arg2),			\
				sizeof(type2), (void*) &(arg3), sizeof(type3),				\
				(void*) &(arg4), sizeof(type4), NULL, 0, NULL, 0)

#define ETRC_PROBE5(provider, name, type1, arg1, type2, arg2, type3, arg3,	\
		type4, arg4, type5, arg5)											\
		etrc_probe_n(&provider, &provider ## _ ## name, 5,					\
				(void*) &(arg1), sizeof(type1), (void*) &(arg2),			\
				sizeof(type2), (void*) &(arg3), sizeof(type3),				\
				(void*) &(arg4), sizeof(type4), (void*) &(arg5), 			\
				sizeof(type5), 0, NULL, 0)

#else

/**
 * Define profiler and events.
 *
 * @note you need manually assign event id's
 */
#define ETRC_DEFINE_PROVIDER(provider, guid)
#define ETRC_DEFINE_EVENT(provider, event, id)

/**
 * Initialize/destroy provider object
 */
#define etrc_provider_init(provider) 		do { } while(0)
#define etrc_provider_destroy(provider) 	do { } while(0)

/**
 * Probe functions
 */
#define ETRC_PROBE0(provider, name)			do { } while(0)
#define ETRC_PROBE1(provider, name, type1, arg1)							\
		do { } while(0)
#define ETRC_PROBE2(provider, name, type1, arg1, type2, arg2)				\
		do { } while(0)
#define ETRC_PROBE3(provider, name, type1, arg1, type2, arg2, type3, arg3)	\
		do { } while(0)
#define ETRC_PROBE4(provider, name, type1, arg1, type2, arg2, type3, arg3,	\
		type4, arg4)														\
				do { } while(0)
#define ETRC_PROBE5(provider, name, type1, arg1, type2, arg2, type3, arg3,	\
		type4, arg4, type5, arg5)											\
				do { } while(0)

#endif

#endif /* ETRACE_H_ */

