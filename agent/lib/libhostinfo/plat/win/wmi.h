
/*
    This file is part of TSLoad.
    Copyright 2014, Sergey Klyaus, ITMO University

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



#ifndef WMI_H_
#define WMI_H_

#define _WIN32_DCOM

#include <tsload/defs.h>


#ifdef __cplusplus

#include <comdef.h>
#include <wbemidl.h>

typedef struct {
	IWbemLocator *loc;
	IWbemServices *svc;
} hi_wmi_t;

typedef struct {
	IEnumWbemClassObject* enumerator;
	IWbemClassObject *cls_obj;
} hi_wmi_iter_t;

#else

typedef struct {
	void *loc;
	void *svc;
} hi_wmi_t;

typedef struct {
	 void* enumerator;
	 void* cls_obj;
} hi_wmi_iter_t;

#endif

#define HI_WMI_OK						0
#define HI_WMI_ERROR_INIT_COM			-1
#define HI_WMI_ERROR_SECURITY			-2
#define HI_WMI_ERROR_CREATE_LOCATOR		-3
#define HI_WMI_ERROR_CONNECT			-4
#define HI_WMI_ERROR_QUERY				-5
#define HI_WMI_ERROR_FETCH_PROPERTY		-6
#define HI_WMI_ERROR_CONVERT_PROPERTY	-7

#define HI_WMI_ROOT_CIMV2			"ROOT\\CIMV2"

#ifdef __cplusplus
extern "C" {
#endif

LIBEXPORT int hi_wmi_connect(hi_wmi_t* wmi, const char* ns);
LIBEXPORT void hi_wmi_disconnect(hi_wmi_t* wmi);

LIBEXPORT int hi_wmi_query(hi_wmi_t* wmi, hi_wmi_iter_t* iter, unsigned short* dialect, unsigned short* query);
LIBEXPORT boolean_t hi_wmi_next(hi_wmi_iter_t* iter);

LIBEXPORT int hi_wmi_get_string_raw(hi_wmi_iter_t* iter, unsigned short* name, char* str, size_t len);
LIBEXPORT int hi_wmi_get_string(hi_wmi_iter_t* iter, unsigned short* name, char* str, size_t len);
LIBEXPORT int hi_wmi_get_integer(hi_wmi_iter_t* iter, unsigned short* name, int64_t* pi);
LIBEXPORT int hi_wmi_get_boolean(hi_wmi_iter_t* iter, unsigned short* name, boolean_t* pb);

#ifdef __cplusplus
}
#endif

#endif /* WMI_H_ */

