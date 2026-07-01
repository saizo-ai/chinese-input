#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msctf.h>

#include <string>
#include <vector>

#include "CandidateWindow.h"

class CZhiPinTextService : public ITfTextInputProcessorEx,
                           public ITfThreadMgrEventSink,
                           public ITfKeyEventSink,
                           public ITfCompositionSink,
                           public ITfDisplayAttributeProvider {
public:
    CZhiPinTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor / Ex
    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
    STDMETHODIMP ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD dwFlags) override;
    STDMETHODIMP Deactivate() override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* pdimPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pic) override;
    STDMETHODIMP OnPopContext(ITfContext* pic) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                               BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                             BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                           BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                         BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite,
                                         ITfComposition* pComposition) override;

    // ITfDisplayAttributeProvider
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid,
                                         ITfDisplayAttributeInfo** ppInfo) override;

    // Called by edit sessions / composition code.
    ITfComposition* _pComposition = nullptr;   // active composition (owned)
    ITfContext* _pComposingContext = nullptr;  // context of the composition (owned)
    TfClientId _tfClientId = TF_CLIENTID_NULL;
    TfGuidAtom _inputAttrAtom = TF_INVALID_GUIDATOM;

private:
    ~CZhiPinTextService();

    // --- setup helpers ---
    bool _InitThreadMgrEventSink();
    void _UninitThreadMgrEventSink();
    bool _InitKeyEventSink();
    void _UninitKeyEventSink();
    void _InitDisplayAttributeAtom();

    // --- key handling ---
    bool _IsComposing() const { return _pComposition != nullptr; }
    bool _WouldEatKeyDown(WPARAM wParam) const;
    // Returns the fullwidth string for a mapped punctuation key, or nullptr.
    // Pure query; quote-pair toggling happens in _EmitPunct.
    const wchar_t* _MapPunct(WPARAM wParam, bool shift) const;
    void _EmitPunct(ITfContext* pic, WPARAM wParam, bool shift);
    void _HandleKeyDown(ITfContext* pic, WPARAM wParam);
    void _ToggleMode(ITfContext* pic);

    // --- composition state machine ---
    void _AppendChar(ITfContext* pic, char c);
    void _Backspace(ITfContext* pic);
    void _CancelComposition(ITfContext* pic);
    void _CommitRaw(ITfContext* pic);                 // pending + raw ascii, no learn
    void _SelectCandidate(ITfContext* pic, int index);  // digit/space selection
    void _ForgetHighlighted(ITfContext* pic);
    void _Requery();
    void _ResetState();

    // --- edit-session-backed operations ---
    void _UpdateComposition(ITfContext* pic);  // show pending+segmented, place window
    void _EndCompositionCleanly(ITfContext* pic, const std::wstring& finalText);
    void _InsertAtSelection(ITfContext* pic, const std::wstring& text);
    void _AbortCompositionAsync();  // focus loss: finalize whatever is on screen

    void _UpdateCandidateUI();
    std::wstring _DisplayText() const;

    // --- state ---
    LONG _refCount = 1;
    ITfThreadMgr* _pThreadMgr = nullptr;
    DWORD _threadMgrEventSinkCookie = TF_INVALID_COOKIE;

    bool _chineseMode = true;
    bool _shiftPending = false;
    bool _quoteOpen = false;    // ‘ ’ pairing
    bool _dquoteOpen = false;   // “ ” pairing

    std::string _buffer;       // remaining raw pinyin (ascii)
    std::string _originalRaw;  // everything typed for this composition
    std::wstring _pending;     // hanzi already chosen within this composition

    bool _valid = false;
    std::wstring _segmented;
    std::vector<CandItem> _cands;
    int _highlight = 0;
    int _pageStart = 0;

    POINT _caretPt = {0, 0};
    bool _haveCaretPt = false;

    CCandidateWindow _candWindow;
};
