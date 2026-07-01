#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msctf.h>

#include "ZhiPinGuids.h"

extern HINSTANCE g_hInst;   // DLL module handle
extern LONG g_cRefDll;      // COM server lock count

void DllAddRef();
void DllRelease();

#define ZHIPIN_LANGID ((LANGID)0x0804)  // zh-CN

extern const WCHAR ZHIPIN_DESC[];  // L"直拼 ZhiPin"
