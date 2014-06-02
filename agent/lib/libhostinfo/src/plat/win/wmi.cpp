
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



#include <hitrace.h>

#include <plat/win/wmi.h>

#include <windows.h>
#include <string.h>

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

int hi_wmi_query(hi_wmi_t* wmi, hi_wmi_iter_t* iter, const char* dialect, const char* query) {
	HRESULT hres;

	iter->enumerator = NULL;

	hres = wmi->svc->ExecQuery(bstr_t((LPCWSTR) dialect), bstr_t((LPCWSTR) query),
							   WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
							   NULL, &iter->enumerator);
	if(FAILED(hres))
		return HI_WMI_ERROR_QUERY;

	return HI_WMI_OK;
}

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

int hi_wmi_get_string(hi_wmi_iter_t* iter, const char* name, char* str, size_t len) {
	VARIANT vtProp;
	HRESULT hres;
	char* tmp;

	hres = iter->cls_obj->Get((LPCWSTR) name, 0, &vtProp, 0, 0);
	tmp = _com_util::ConvertBSTRToString(vtProp.bstrVal);

	strncpy(str, (const char*) tmp, len);

	delete[] tmp;
	VariantClear(&vtProp);

	return HI_WMI_OK;
}



