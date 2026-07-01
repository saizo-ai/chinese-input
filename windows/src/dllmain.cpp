// DLL entry points and class factory.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msctf.h>
#include <olectl.h>

#include <new>

#include "Globals.h"
#include "TextService.h"

// Implemented in Register.cpp
HRESULT RegisterServer();
HRESULT UnregisterServer();
HRESULT RegisterProfile();
HRESULT UnregisterProfile();
HRESULT RegisterCategories();
HRESULT UnregisterCategories();

namespace {

class CClassFactory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppvObj = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG c = InterlockedDecrement(&_refCount);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        CZhiPinTextService* service = new (std::nothrow) CZhiPinTextService();
        if (!service) return E_OUTOFMEMORY;
        HRESULT hr = service->QueryInterface(riid, ppvObj);
        service->Release();  // QI holds the surviving reference
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock)
            DllAddRef();
        else
            DllRelease();
        return S_OK;
    }

private:
    virtual ~CClassFactory() = default;
    LONG _refCount = 1;
};

}  // namespace

STDAPI_(BOOL) DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            g_hInst = hInstance;
            DisableThreadLibraryCalls(hInstance);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (!IsEqualCLSID(rclsid, CLSID_ZhiPinTextService)) return CLASS_E_CLASSNOTAVAILABLE;
    CClassFactory* factory = new (std::nothrow) CClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppvObj);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return g_cRefDll <= 0 ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    // regsvr32 runs elevated; COM may or may not be initialized.
    HRESULT hrCo = CoInitialize(nullptr);
    HRESULT hr = RegisterServer();
    if (SUCCEEDED(hr)) hr = RegisterProfile();
    if (SUCCEEDED(hr)) hr = RegisterCategories();
    if (SUCCEEDED(hrCo)) CoUninitialize();
    if (FAILED(hr)) {
        DllUnregisterServer();
        return SELFREG_E_CLASS;
    }
    return S_OK;
}

STDAPI DllUnregisterServer() {
    HRESULT hrCo = CoInitialize(nullptr);
    UnregisterCategories();
    UnregisterProfile();
    UnregisterServer();
    if (SUCCEEDED(hrCo)) CoUninitialize();
    return S_OK;
}
