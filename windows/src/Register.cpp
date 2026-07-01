// COM class registration (HKLM) and TSF profile/category registration.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msctf.h>

#include <string>

#include "Globals.h"

namespace {

std::wstring ClsidString() {
    WCHAR buf[64] = {};
    StringFromGUID2(CLSID_ZhiPinTextService, buf, 64);
    return buf;
}

const GUID* kCategories[] = {
    &GUID_TFCAT_TIP_KEYBOARD,
    &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    &GUID_TFCAT_TIPCAP_SECUREMODE,
    &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    &GUID_TFCAT_TIPCAP_COMLESS,
};

}  // namespace

HRESULT RegisterServer() {
    std::wstring keyPath = L"SOFTWARE\\Classes\\CLSID\\" + ClsidString();

    HKEY hKey = nullptr;
    LONG rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, nullptr,
                              REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (rc != ERROR_SUCCESS) return HRESULT_FROM_WIN32(rc);
    const WCHAR desc[] = L"ZhiPin Text Service";
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)desc, sizeof(desc));

    HKEY hInproc = nullptr;
    rc = RegCreateKeyExW(hKey, L"InprocServer32", 0, nullptr, REG_OPTION_NON_VOLATILE,
                         KEY_WRITE, nullptr, &hInproc, nullptr);
    if (rc == ERROR_SUCCESS) {
        WCHAR path[MAX_PATH] = {};
        GetModuleFileNameW(g_hInst, path, MAX_PATH);
        RegSetValueExW(hInproc, nullptr, 0, REG_SZ, (const BYTE*)path,
                       (DWORD)((wcslen(path) + 1) * sizeof(WCHAR)));
        const WCHAR model[] = L"Apartment";
        RegSetValueExW(hInproc, L"ThreadingModel", 0, REG_SZ, (const BYTE*)model,
                       sizeof(model));
        RegCloseKey(hInproc);
    }
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(rc);
}

HRESULT UnregisterServer() {
    std::wstring keyPath = L"SOFTWARE\\Classes\\CLSID\\" + ClsidString();
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, keyPath.c_str());
    return S_OK;
}

HRESULT RegisterProfile() {
    ITfInputProcessorProfileMgr* pMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfileMgr,
                                  (void**)&pMgr);
    if (FAILED(hr) || !pMgr) return FAILED(hr) ? hr : E_FAIL;
    hr = pMgr->RegisterProfile(CLSID_ZhiPinTextService, ZHIPIN_LANGID, GUID_ZhiPinProfile,
                               ZHIPIN_DESC, (ULONG)wcslen(ZHIPIN_DESC), nullptr, 0, 0,
                               nullptr, 0, TRUE, 0);
    pMgr->Release();
    return hr;
}

HRESULT UnregisterProfile() {
    ITfInputProcessorProfileMgr* pMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfileMgr,
                                  (void**)&pMgr);
    if (FAILED(hr) || !pMgr) return FAILED(hr) ? hr : E_FAIL;
    hr = pMgr->UnregisterProfile(CLSID_ZhiPinTextService, ZHIPIN_LANGID, GUID_ZhiPinProfile,
                                 0);
    pMgr->Release();
    return hr;
}

HRESULT RegisterCategories() {
    ITfCategoryMgr* pCatMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfCategoryMgr, (void**)&pCatMgr);
    if (FAILED(hr) || !pCatMgr) return FAILED(hr) ? hr : E_FAIL;
    for (const GUID* cat : kCategories) {
        hr = pCatMgr->RegisterCategory(CLSID_ZhiPinTextService, *cat,
                                       CLSID_ZhiPinTextService);
        if (FAILED(hr)) break;
    }
    pCatMgr->Release();
    return hr;
}

HRESULT UnregisterCategories() {
    ITfCategoryMgr* pCatMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfCategoryMgr, (void**)&pCatMgr);
    if (FAILED(hr) || !pCatMgr) return FAILED(hr) ? hr : E_FAIL;
    for (const GUID* cat : kCategories)
        pCatMgr->UnregisterCategory(CLSID_ZhiPinTextService, *cat, CLSID_ZhiPinTextService);
    pCatMgr->Release();
    return S_OK;
}
