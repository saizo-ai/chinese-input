// Edit-session-backed composition operations.
#include "EditSession.h"
#include "EngineBridge.h"
#include "Globals.h"
#include "TextService.h"

namespace {

void ApplyAttribute(ITfContext* pic, TfEditCookie ec, ITfRange* range, TfGuidAtom atom) {
    if (atom == TF_INVALID_GUIDATOM || !range) return;
    ITfProperty* pProp = nullptr;
    if (SUCCEEDED(pic->GetProperty(GUID_PROP_ATTRIBUTE, &pProp)) && pProp) {
        VARIANT var;
        VariantInit(&var);
        var.vt = VT_I4;
        var.lVal = (LONG)atom;
        pProp->SetValue(ec, range, &var);
        pProp->Release();
    }
}

void ClearAttribute(ITfContext* pic, TfEditCookie ec, ITfRange* range) {
    if (!range) return;
    ITfProperty* pProp = nullptr;
    if (SUCCEEDED(pic->GetProperty(GUID_PROP_ATTRIBUTE, &pProp)) && pProp) {
        pProp->Clear(ec, range);
        pProp->Release();
    }
}

void CollapseSelectionAfter(ITfContext* pic, TfEditCookie ec, ITfRange* range) {
    if (!range) return;
    ITfRange* pEnd = nullptr;
    if (FAILED(range->Clone(&pEnd)) || !pEnd) return;
    pEnd->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION sel;
    sel.range = pEnd;
    sel.style.ase = TF_AE_NONE;
    sel.style.fInterimChar = FALSE;
    pic->SetSelection(ec, 1, &sel);
    pEnd->Release();
}

}  // namespace

void CZhiPinTextService::_UpdateComposition(ITfContext* pic) {
    if (!pic) return;
    std::wstring display = _DisplayText();
    CZhiPinTextService* self = this;

    RunSyncEditSession(pic, _tfClientId, [self, pic, display](TfEditCookie ec) -> HRESULT {
        if (!self->_pComposition) {
            ITfInsertAtSelection* pias = nullptr;
            if (FAILED(pic->QueryInterface(IID_ITfInsertAtSelection, (void**)&pias)) || !pias)
                return E_FAIL;
            ITfRange* pRange = nullptr;
            HRESULT hr =
                pias->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, &pRange);
            pias->Release();
            if (FAILED(hr) || !pRange) return FAILED(hr) ? hr : E_FAIL;

            ITfContextComposition* pCC = nullptr;
            hr = pic->QueryInterface(IID_ITfContextComposition, (void**)&pCC);
            if (SUCCEEDED(hr) && pCC) {
                ITfComposition* pComposition = nullptr;
                hr = pCC->StartComposition(ec, pRange,
                                           static_cast<ITfCompositionSink*>(self),
                                           &pComposition);
                pCC->Release();
                if (SUCCEEDED(hr) && pComposition) {
                    self->_pComposition = pComposition;  // owned
                    self->_pComposingContext = pic;
                    pic->AddRef();
                }
            }
            pRange->Release();
            if (!self->_pComposition) return E_FAIL;
        }

        ITfRange* pRange = nullptr;
        if (FAILED(self->_pComposition->GetRange(&pRange)) || !pRange) return E_FAIL;
        HRESULT hr = pRange->SetText(ec, 0, display.c_str(), (LONG)display.size());
        if (SUCCEEDED(hr)) {
            ApplyAttribute(pic, ec, pRange, self->_inputAttrAtom);
            CollapseSelectionAfter(pic, ec, pRange);
        }

        // Caret rectangle for the candidate window.
        self->_haveCaretPt = false;
        ITfContextView* pView = nullptr;
        if (SUCCEEDED(pic->GetActiveView(&pView)) && pView) {
            RECT rc = {};
            BOOL clipped = FALSE;
            if (SUCCEEDED(pView->GetTextExt(ec, pRange, &rc, &clipped))) {
                self->_caretPt.x = rc.left;
                self->_caretPt.y = rc.bottom;
                self->_haveCaretPt = true;
            }
            pView->Release();
        }
        pRange->Release();
        return hr;
    });
}

void CZhiPinTextService::_EndCompositionCleanly(ITfContext* pic, const std::wstring& finalText) {
    if (_pComposition) {
        ITfContext* ctx = _pComposingContext ? _pComposingContext : pic;
        CZhiPinTextService* self = this;
        RunSyncEditSession(ctx, _tfClientId, [self, ctx, finalText](TfEditCookie ec) -> HRESULT {
            if (!self->_pComposition) return S_OK;
            ITfRange* pRange = nullptr;
            if (SUCCEEDED(self->_pComposition->GetRange(&pRange)) && pRange) {
                pRange->SetText(ec, 0, finalText.c_str(), (LONG)finalText.size());
                ClearAttribute(ctx, ec, pRange);
                CollapseSelectionAfter(ctx, ec, pRange);
                pRange->Release();
            }
            self->_pComposition->EndComposition(ec);
            return S_OK;
        });
        if (_pComposition) {
            _pComposition->Release();
            _pComposition = nullptr;
        }
        if (_pComposingContext) {
            _pComposingContext->Release();
            _pComposingContext = nullptr;
        }
    } else if (!finalText.empty()) {
        _InsertAtSelection(pic, finalText);
    }
}

void CZhiPinTextService::_InsertAtSelection(ITfContext* pic, const std::wstring& text) {
    if (!pic || text.empty()) return;
    TfClientId tid = _tfClientId;
    RunSyncEditSession(pic, tid, [pic, text](TfEditCookie ec) -> HRESULT {
        ITfInsertAtSelection* pias = nullptr;
        if (FAILED(pic->QueryInterface(IID_ITfInsertAtSelection, (void**)&pias)) || !pias)
            return E_FAIL;
        ITfRange* pRange = nullptr;
        HRESULT hr = pias->InsertTextAtSelection(ec, 0, text.c_str(), (LONG)text.size(),
                                                 &pRange);
        pias->Release();
        if (SUCCEEDED(hr) && pRange) {
            CollapseSelectionAfter(pic, ec, pRange);
            pRange->Release();
        }
        return hr;
    });
}

// Focus is moving away: finalize the on-screen composition text asynchronously
// (a sync session can deadlock during focus changes). State is reset by the
// caller; the lambda owns the interface references.
void CZhiPinTextService::_AbortCompositionAsync() {
    if (!_pComposition) return;
    ITfComposition* comp = _pComposition;
    ITfContext* ctx = _pComposingContext;
    _pComposition = nullptr;
    _pComposingContext = nullptr;
    if (!ctx) {
        comp->Release();
        return;
    }
    HRESULT hr = RunAsyncEditSession(ctx, _tfClientId, [comp, ctx](TfEditCookie ec) -> HRESULT {
        ITfRange* pRange = nullptr;
        if (SUCCEEDED(comp->GetRange(&pRange)) && pRange) {
            ClearAttribute(ctx, ec, pRange);
            pRange->Release();
        }
        comp->EndComposition(ec);
        comp->Release();
        ctx->Release();
        return S_OK;
    });
    if (FAILED(hr)) {
        comp->Release();
        ctx->Release();
    }
}
