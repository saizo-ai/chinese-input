// Minimal ITfEditSession wrapper around a callable.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msctf.h>

#include <functional>
#include <new>

class CFunctionalEditSession : public ITfEditSession {
public:
    explicit CFunctionalEditSession(std::function<HRESULT(TfEditCookie)> fn)
        : _fn(std::move(fn)) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppvObj = static_cast<ITfEditSession*>(this);
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

    STDMETHODIMP DoEditSession(TfEditCookie ec) override { return _fn(ec); }

private:
    virtual ~CFunctionalEditSession() = default;
    std::function<HRESULT(TfEditCookie)> _fn;
    LONG _refCount = 1;
};

// Runs fn in a synchronous read/write edit session on pic. Returns the edit
// session result, or the RequestEditSession failure code.
inline HRESULT RunSyncEditSession(ITfContext* pic, TfClientId tid,
                                  std::function<HRESULT(TfEditCookie)> fn) {
    if (!pic) return E_INVALIDARG;
    CFunctionalEditSession* session = new (std::nothrow) CFunctionalEditSession(std::move(fn));
    if (!session) return E_OUTOFMEMORY;
    HRESULT hrSession = E_FAIL;
    HRESULT hr = pic->RequestEditSession(tid, session, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
    session->Release();
    return FAILED(hr) ? hr : hrSession;
}

inline HRESULT RunAsyncEditSession(ITfContext* pic, TfClientId tid,
                                   std::function<HRESULT(TfEditCookie)> fn) {
    if (!pic) return E_INVALIDARG;
    CFunctionalEditSession* session = new (std::nothrow) CFunctionalEditSession(std::move(fn));
    if (!session) return E_OUTOFMEMORY;
    HRESULT hrSession = E_FAIL;
    HRESULT hr = pic->RequestEditSession(tid, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE,
                                         &hrSession);
    session->Release();
    return hr;
}
