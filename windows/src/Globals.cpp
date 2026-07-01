// This translation unit defines every GUID used by the DLL (initguid.h must
// come before the headers that declare them).
#include <initguid.h>

#include "Globals.h"

HINSTANCE g_hInst = nullptr;
LONG g_cRefDll = 0;

void DllAddRef() { InterlockedIncrement(&g_cRefDll); }
void DllRelease() { InterlockedDecrement(&g_cRefDll); }

const WCHAR ZHIPIN_DESC[] = L"直拼 ZhiPin";  // 直拼 ZhiPin
