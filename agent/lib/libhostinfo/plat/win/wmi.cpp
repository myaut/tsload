
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



#include <tsload/defs.h>

#include <hostinfo/plat/wmi.h>

#include <hitrace.h>

#include <string.h>

#include <windows.h>

/**
 * Connect to WMI and initialize handle
 * 
 * @param wmi   pointer to unitialized wmi handle
 * @param ns    WMI namespace
 * 
 * @return HI_WMI_OK if everything was fine or an error code
 */
int hi_wmi_connect(hi_wmi_t* wmi, const char* ns) {
	HRESULT hres;
	int err = HI_WMI_OK;

	wmi->loc = NULL;
	wmi->svc = NULL;

	hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres))
	{
		hi_wmi_dprintf("Failed to initialize COM library. Error code = 0x%x\n", hres);
		return HI_WMI_ERROR_INIT_COM;
	}

	hres = CoInitializeSecurity(NULL, -1, NULL, NULL,
								RPC_C_AUTHN_LEVEL_DEFAULT,
								RPC_C_IMP_LEVEL_IMPERSONATE,
								NULL, EOAC_NONE, NULL);
	if (FAILED(hres))
	{
		hi_wmi_dprintf("Failed to initialize security. Error code = 0x%x\n", hres);

		err = HI_WMI_ERROR_SECURITY;
		goto end;
	}

	hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
	        			    IID_IWbemLocator, (LPVOID *) &wmi->loc);
	if (FAILED(hres))
	{
		hi_wmi_dprintf("Failed to initialize create IWbemLocator object. Error code = 0x%x\n", hres);

		err = HI_WMI_ERROR_CREATE_LOCATOR;
		goto end;
	}

	hres = wmi->loc->ConnectServer(_bstr_t(ns), NULL, NULL, 0, NULL, 0, 0,
								   &wmi->svc);
	if (FAILED(hres))
	{
		hi_wmi_dprintf("Could not connect. Error code = 0x%x\n", hres);

		err = HI_WMI_ERROR_CREATE_LOCATOR;
		goto end;
	}

	hi_wmi_dprintf("Connected to namespace '%s'\n", ns);

	hres = CoSetProxyBlanket(wmi->svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
							 NULL, RPC_C_AUTHN_LEVEL_CALL,
							 RPC_C_IMP_LEVEL_IMPERSONATE,
							 NULL, EOAC_NONE);
	if (FAILED(hres))
	{
		hi_wmi_dprintf("Could not set proxy blanket. Error code = 0x%x\n", hres);

		err = HI_WMI_ERROR_CREATE_LOCATOR;
		goto end;
	}

end:
	if(err != HI_WMI_OK)
		hi_wmi_disconnect(wmi);

	return err;
}

/**
 * Disconnect to WMI and reset handle
 * 
 * @param wmi   pointer to connected wmi handle
 */
void hi_wmi_disconnect(hi_wmi_t* wmi) {
	if(wmi->svc) {
		wmi->svc->Release();
	}

	if(wmi->loc) {
		wmi->loc->Release();
	}

	wmi->loc = NULL;
	wmi->svc = NULL;

	CoUninitialize();
}

/**
 * Run a query
 * 
 * Data is returned as several rows in iterator `iter`. Initially 
 * iterator contains no row, so you should call `hi_wmi_next()` to
 * fetch first row.
 * 
 * @param wmi   pointer to unitialized wmi handle
 * @param iter  unitialized iterator to walk over results
 * @param dialect wide string containing dialect of query, usually `L"WQL"`
 * @param query wide string containing query string
 * 
 * @return HI_WMI_OK if everything was fine or an error code
 */
int hi_wmi_query(hi_wmi_t* wmi, hi_wmi_iter_t* iter, unsigned short* dialect, unsigned short* query) {
	HRESULT hres;

	iter->enumerator = NULL;

	hres = wmi->svc->ExecQuery(bstr_t((LPCWSTR) dialect), bstr_t((LPCWSTR) query),
							   WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
							   NULL, &iter->enumerator);
	if(FAILED(hres))
		return HI_WMI_ERROR_QUERY;

	return HI_WMI_OK;
}

/**
 * Fetch next row from iterator. If query failed or no more rows are available,
 * returns B\_FALSE. If next row was fetched, returns B\_TRUE.
 */
boolean_t hi_wmi_next(hi_wmi_iter_t* iter) {
	ULONG ret;
	HRESULT hres;

	if(iter->enumerator == NULL)
		return B_FALSE;

	hres = iter->enumerator->Next(WBEM_INFINITE, 1,
            					  &iter->cls_obj, &ret);

	if(ret != 0)
		return B_TRUE;

	return B_FALSE;
}

/**
 * Get a raw (UNICODE) string value from row
 * 
 * @param iter		WMI iterator
 * @param name		wide string -- name of attribute
 * @param str 		destination string buffer (statically allocated)
 * @param len 		length of string buffer
 * 
 * @return HI_WMI_OK if everything went fine, or HI_WMI_ERROR_FETCH_PROPERTY
 */
int hi_wmi_get_string_raw(hi_wmi_iter_t* iter, unsigned short* name, char* str, size_t len) {
	VARIANT vtProp;
	HRESULT hres;
	char* tmp;

	hres = iter->cls_obj->Get((LPCWSTR) name, 0, &vtProp, 0, 0);
	if(FAILED(hres) || vtProp.vt == VT_EMPTY || vtProp.vt == VT_NULL) {
		return HI_WMI_ERROR_FETCH_PROPERTY;
	}

	strncpy(str, (const char*) vtProp.bstrVal, len);

	VariantClear(&vtProp);

	return HI_WMI_OK;
}

/**
 * Same as `hi_wmi_get_string_raw()`, but converts string to ASCII
 */
int hi_wmi_get_string(hi_wmi_iter_t* iter, unsigned short* name, char* str, size_t len) {
	VARIANT vtProp;
	HRESULT hres;
	char* tmp;

	hres = iter->cls_obj->Get((LPCWSTR) name, 0, &vtProp, 0, 0);
	if(FAILED(hres) || vtProp.vt == VT_EMPTY || vtProp.vt == VT_NULL) {
		return HI_WMI_ERROR_FETCH_PROPERTY;
	}

	tmp = _com_util::ConvertBSTRToString(vtProp.bstrVal);

	strncpy(str, (const char*) tmp, len);

	delete[] tmp;
	VariantClear(&vtProp);

	return HI_WMI_OK;
}

/**
 * Same as `hi_wmi_get_string_raw()`, but gets integer
 */
int hi_wmi_get_integer(hi_wmi_iter_t* iter, unsigned short* name, int64_t* pi) {
	VARIANT vtProp;
	VARIANT vtProp2;
	HRESULT hres;

	hres = iter->cls_obj->Get((LPCWSTR) name, 0, &vtProp, 0, 0);
	if(FAILED(hres) || vtProp.vt == VT_EMPTY || vtProp.vt == VT_NULL) {
		return HI_WMI_ERROR_FETCH_PROPERTY;
	}

	hres = VariantChangeType (&vtProp2, &vtProp, 0, VT_I8);
	if(FAILED(hres)) {
		return HI_WMI_ERROR_CONVERT_PROPERTY;
	}

	*pi = vtProp2.llVal;

	VariantClear(&vtProp);

	return HI_WMI_OK;
}

/**
 * Same as `hi_wmi_get_string_raw()`, but gets boolean
 */
int hi_wmi_get_boolean(hi_wmi_iter_t* iter, unsigned short* name, boolean_t* pb) {
	VARIANT vtProp;
	HRESULT hres;
	int ret = HI_WMI_OK;

	hres = iter->cls_obj->Get((LPCWSTR) name, 0, &vtProp, 0, 0);
	if(FAILED(hres) || vtProp.vt == VT_EMPTY || vtProp.vt == VT_NULL) {
		return HI_WMI_ERROR_FETCH_PROPERTY;
	}

	switch(vtProp.boolVal) {
		case 0:
			*pb = B_FALSE;
			break;
		default:
			*pb = B_TRUE;
			break;
	}

	VariantClear(&vtProp);

	return ret;
}

