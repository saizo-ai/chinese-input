// CZhiPinTextService: IUnknown, activation, sinks, display attribute provider.
#include "TextService.h"

#include "DisplayAttribute.h"
#include "Globals.h"

CZhiPinTextService::CZhiPinTextService() {
    DllAddRef();
}

CZhiPinTextService::~CZhiPinTextService() {
    DllRelease();
}

// ---- IUnknown ----

STDMETHODIMP CZhiPinTextService::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppvObj = static_cast<ITfTextInputProcessor*>(this);
    } else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
        *ppvObj = static_cast<ITfDisplayAttributeProvider*>(this);
    }
    if (*ppvObj) {
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CZhiPinTextService::AddRef() {
    return InterlockedIncrement(&_refCount);
}

STDMETHODIMP_(ULONG) CZhiPinTextService::Release() {
    ULONG c = InterlockedDecrement(&_refCount);
    if (c == 0) delete this;
    return c;
}

// ---- activation ----

STDMETHODIMP CZhiPinTextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
    return ActivateEx(ptim, tid, 0);
}

STDMETHODIMP CZhiPinTextService::ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD) {
    if (!ptim) return E_INVALIDARG;
    _pThreadMgr = ptim;
    _pThreadMgr->AddRef();
    _tfClientId = tid;

    _InitDisplayAttributeAtom();
    if (!_InitThreadMgrEventSink() || !_InitKeyEventSink()) {
        Deactivate();
        return E_FAIL;
    }
    _candWindow.Create();
    _candWindow.onDeleteUserEntry = [this](int index) { _ForgetAt(index); };
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::Deactivate() {
    _AbortCompositionAsync();
    _ResetState();
    _candWindow.Destroy();
    _UninitKeyEventSink();
    _UninitThreadMgrEventSink();
    if (_pThreadMgr) {
        _pThreadMgr->Release();
        _pThreadMgr = nullptr;
    }
    _tfClientId = TF_CLIENTID_NULL;
    return S_OK;
}

bool CZhiPinTextService::_InitThreadMgrEventSink() {
    ITfSource* pSource = nullptr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource)) || !pSource)
        return false;
    HRESULT hr = pSource->AdviseSink(IID_ITfThreadMgrEventSink,
                                     static_cast<ITfThreadMgrEventSink*>(this),
                                     &_threadMgrEventSinkCookie);
    pSource->Release();
    if (FAILED(hr)) {
        _threadMgrEventSinkCookie = TF_INVALID_COOKIE;
        return false;
    }
    return true;
}

void CZhiPinTextService::_UninitThreadMgrEventSink() {
    if (!_pThreadMgr || _threadMgrEventSinkCookie == TF_INVALID_COOKIE) return;
    ITfSource* pSource = nullptr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource)) && pSource) {
        pSource->UnadviseSink(_threadMgrEventSinkCookie);
        pSource->Release();
    }
    _threadMgrEventSinkCookie = TF_INVALID_COOKIE;
}

bool CZhiPinTextService::_InitKeyEventSink() {
    ITfKeystrokeMgr* pKeyMgr = nullptr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeyMgr)) || !pKeyMgr)
        return false;
    HRESULT hr = pKeyMgr->AdviseKeyEventSink(_tfClientId,
                                             static_cast<ITfKeyEventSink*>(this), TRUE);
    pKeyMgr->Release();
    return SUCCEEDED(hr);
}

void CZhiPinTextService::_UninitKeyEventSink() {
    if (!_pThreadMgr) return;
    ITfKeystrokeMgr* pKeyMgr = nullptr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeyMgr)) &&
        pKeyMgr) {
        pKeyMgr->UnadviseKeyEventSink(_tfClientId);
        pKeyMgr->Release();
    }
}

void CZhiPinTextService::_InitDisplayAttributeAtom() {
    ITfCategoryMgr* pCatMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfCategoryMgr, (void**)&pCatMgr)) &&
        pCatMgr) {
        pCatMgr->RegisterGUID(GUID_ZhiPinDisplayAttributeInput, &_inputAttrAtom);
        pCatMgr->Release();
    }
}

// ---- ITfThreadMgrEventSink ----

STDMETHODIMP CZhiPinTextService::OnInitDocumentMgr(ITfDocumentMgr*) {
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnUninitDocumentMgr(ITfDocumentMgr*) {
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) {
    // Focus moved to another document: finalize whatever composition text is
    // on screen and reset; a stale composition must not follow the new focus.
    if (_IsComposing()) {
        _AbortCompositionAsync();
        _ResetState();
        _UpdateCandidateUI();
    }
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnPushContext(ITfContext*) {
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnPopContext(ITfContext*) {
    return S_OK;
}

// ---- ITfCompositionSink ----

STDMETHODIMP CZhiPinTextService::OnCompositionTerminated(TfEditCookie,
                                                         ITfComposition* pComposition) {
    // The application (or another TIP) force-ended our composition.
    if (_pComposition == pComposition && _pComposition) {
        _pComposition->Release();
        _pComposition = nullptr;
    }
    if (_pComposingContext) {
        _pComposingContext->Release();
        _pComposingContext = nullptr;
    }
    _ResetState();
    _UpdateCandidateUI();
    return S_OK;
}

// ---- ITfDisplayAttributeProvider ----

STDMETHODIMP CZhiPinTextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) {
    if (!ppEnum) return E_INVALIDARG;
    *ppEnum = new (std::nothrow) CZhiPinEnumDisplayAttributeInfo();
    return *ppEnum ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CZhiPinTextService::GetDisplayAttributeInfo(REFGUID guid,
                                                         ITfDisplayAttributeInfo** ppInfo) {
    if (!ppInfo) return E_INVALIDARG;
    *ppInfo = nullptr;
    if (!IsEqualGUID(guid, GUID_ZhiPinDisplayAttributeInput)) return E_INVALIDARG;
    *ppInfo = new (std::nothrow) CZhiPinDisplayAttributeInfo();
    return *ppInfo ? S_OK : E_OUTOFMEMORY;
}
