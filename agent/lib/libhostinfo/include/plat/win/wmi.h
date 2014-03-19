/*
 * wmi.h
 *
 *  Created on: Mar 19, 2014
 *      Author: myaut
 */

#ifndef WMI_H_
#define WMI_H_

#define _WIN32_DCOM

#include <defs.h>

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

#define HI_WMI_OK					0
#define HI_WMI_ERROR_INIT_COM		-1
#define HI_WMI_ERROR_SECURITY		-2
#define HI_WMI_ERROR_CREATE_LOCATOR	-3
#define HI_WMI_ERROR_CONNECT		-4
#define HI_WMI_ERROR_QUERY			-5

#define HI_WMI_ROOT_CIMV2			"ROOT\\CIMV2"

#ifdef __cplusplus
extern "C" {
#endif

LIBEXPORT int hi_wmi_connect(hi_wmi_t* wmi, const char* ns);
LIBEXPORT void hi_wmi_disconnect(hi_wmi_t* wmi);

LIBEXPORT int hi_wmi_query(hi_wmi_t* wmi, hi_wmi_iter_t* iter, const char* dialect, const char* query);
LIBEXPORT int hi_wmi_get_string(hi_wmi_iter_t* iter, const char* name, char* str, size_t len);
LIBEXPORT boolean_t hi_wmi_next(hi_wmi_iter_t* iter);

#ifdef __cplusplus
}
#endif

#endif /* WMI_H_ */
