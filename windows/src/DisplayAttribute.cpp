#include "DisplayAttribute.h"

#include <oleauto.h>

#include <new>

#include "Globals.h"

// ---- CZhiPinDisplayAttributeInfo ----

STDMETHODIMP CZhiPinDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
        *ppvObj = static_cast<ITfDisplayAttributeInfo*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CZhiPinDisplayAttributeInfo::AddRef() {
    return InterlockedIncrement(&_refCount);
}

STDMETHODIMP_(ULONG) CZhiPinDisplayAttributeInfo::Release() {
    ULONG c = InterlockedDecrement(&_refCount);
    if (c == 0) delete this;
    return c;
}

STDMETHODIMP CZhiPinDisplayAttributeInfo::GetGUID(GUID* pguid) {
    if (!pguid) return E_INVALIDARG;
    *pguid = GUID_ZhiPinDisplayAttributeInput;
    return S_OK;
}

STDMETHODIMP CZhiPinDisplayAttributeInfo::GetDescription(BSTR* pbstrDesc) {
    if (!pbstrDesc) return E_INVALIDARG;
    *pbstrDesc = SysAllocString(L"ZhiPin composition input");
    return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CZhiPinDisplayAttributeInfo::GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) {
    if (!pda) return E_INVALIDARG;
    ZeroMemory(pda, sizeof(*pda));
    pda->crText.type = TF_CT_NONE;
    pda->crBk.type = TF_CT_NONE;
    pda->crLine.type = TF_CT_NONE;
    pda->lsStyle = TF_LS_SOLID;
    pda->fBoldLine = FALSE;
    pda->bAttr = TF_ATTR_INPUT;
    return S_OK;
}

STDMETHODIMP CZhiPinDisplayAttributeInfo::SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) {
    return E_NOTIMPL;
}

STDMETHODIMP CZhiPinDisplayAttributeInfo::Reset() {
    return S_OK;
}

// ---- CZhiPinEnumDisplayAttributeInfo ----

STDMETHODIMP CZhiPinEnumDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
        *ppvObj = static_cast<IEnumTfDisplayAttributeInfo*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CZhiPinEnumDisplayAttributeInfo::AddRef() {
    return InterlockedIncrement(&_refCount);
}

STDMETHODIMP_(ULONG) CZhiPinEnumDisplayAttributeInfo::Release() {
    ULONG c = InterlockedDecrement(&_refCount);
    if (c == 0) delete this;
    return c;
}

STDMETHODIMP CZhiPinEnumDisplayAttributeInfo::Clone(IEnumTfDisplayAttributeInfo** ppEnum) {
    if (!ppEnum) return E_INVALIDARG;
    CZhiPinEnumDisplayAttributeInfo* clone =
        new (std::nothrow) CZhiPinEnumDisplayAttributeInfo();
    if (!clone) return E_OUTOFMEMORY;
    clone->_index = _index;
    *ppEnum = clone;
    return S_OK;
}

STDMETHODIMP CZhiPinEnumDisplayAttributeInfo::Next(ULONG ulCount,
                                                   ITfDisplayAttributeInfo** rgInfo,
                                                   ULONG* pcFetched) {
    if (pcFetched) *pcFetched = 0;
    if (!rgInfo) return E_INVALIDARG;
    ULONG fetched = 0;
    if (ulCount > 0 && _index == 0) {
        CZhiPinDisplayAttributeInfo* info = new (std::nothrow) CZhiPinDisplayAttributeInfo();
        if (!info) return E_OUTOFMEMORY;
        rgInfo[0] = info;
        fetched = 1;
        _index = 1;
    }
    if (pcFetched) *pcFetched = fetched;
    return fetched == ulCount ? S_OK : S_FALSE;
}

STDMETHODIMP CZhiPinEnumDisplayAttributeInfo::Reset() {
    _index = 0;
    return S_OK;
}

STDMETHODIMP CZhiPinEnumDisplayAttributeInfo::Skip(ULONG ulCount) {
    if (ulCount > 0 && _index == 0) {
        _index = 1;
        return S_OK;
    }
    return ulCount == 0 ? S_OK : S_FALSE;
}
