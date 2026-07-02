// ITfKeyEventSink implementation: mode toggle, composition editing, selection.
#include <algorithm>

#include "EngineBridge.h"
#include "Globals.h"
#include "TextService.h"

namespace {

constexpr size_t kMaxRaw = 72;

bool IsShiftDown() { return (GetKeyState(VK_SHIFT) & 0x8000) != 0; }
bool IsCtrlDown() { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }
bool IsAltDown() { return (GetKeyState(VK_MENU) & 0x8000) != 0; }
bool IsCapsOn() { return (GetKeyState(VK_CAPITAL) & 0x0001) != 0; }

}  // namespace

// ---- ITfKeyEventSink ----

STDMETHODIMP CZhiPinTextService::OnSetFocus(BOOL fForeground) {
    if (!fForeground) _shiftPending = false;
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnTestKeyDown(ITfContext*, WPARAM wParam, LPARAM,
                                               BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
        // Always claim Shift so OnKeyDown/OnKeyUp can track "Shift alone".
        // Modifier semantics for apps come from key state, not this message.
        *pfEaten = TRUE;
        return S_OK;
    }
    _shiftPending = false;  // any other key means Shift was not alone
    *pfEaten = _WouldEatKeyDown(wParam) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnTestKeyUp(ITfContext*, WPARAM wParam, LPARAM,
                                             BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten =
        (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM,
                                           BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
        _shiftPending = true;
        *pfEaten = TRUE;
        return S_OK;
    }
    _shiftPending = false;
    if (!_WouldEatKeyDown(wParam)) return S_OK;
    *pfEaten = TRUE;
    _HandleKeyDown(pic, wParam);
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM,
                                         BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    if (wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT) {
        *pfEaten = TRUE;
        if (_shiftPending) {
            _shiftPending = false;
            _ToggleMode(pic);
        }
    }
    return S_OK;
}

STDMETHODIMP CZhiPinTextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* pfEaten) {
    if (pfEaten) *pfEaten = FALSE;
    return S_OK;
}

// ---- eating decision (must be side-effect free) ----

bool CZhiPinTextService::_WouldEatKeyDown(WPARAM wParam) const {
    if (!_chineseMode) return false;
    if (IsAltDown()) return false;

    bool composing = _IsComposing();
    if (IsCtrlDown()) return composing && wParam == VK_DELETE;

    bool shift = IsShiftDown();
    if (composing) {
        // Keep the composition coherent: consume nearly everything, except
        // keys that must keep working globally.
        if (wParam >= VK_F1 && wParam <= VK_F24) return false;
        if (wParam == VK_LWIN || wParam == VK_RWIN || wParam == VK_APPS) return false;
        if (wParam == VK_CAPITAL || wParam == VK_NUMLOCK || wParam == VK_SCROLL) return false;
        if (wParam == VK_CONTROL || wParam == VK_MENU) return false;
        return true;
    }

    // Not composing:
    if (wParam >= 'A' && wParam <= 'Z') return !shift && !IsCapsOn();
    return _MapPunct(wParam, shift) != nullptr;
}

// ---- punctuation map (pure) ----

const wchar_t* CZhiPinTextService::_MapPunct(WPARAM wParam, bool shift) const {
    switch (wParam) {
        case VK_OEM_COMMA:  return shift ? L"《" : L"，";
        case VK_OEM_PERIOD: return shift ? L"》" : L"。";
        case VK_OEM_2:      return shift ? L"？" : nullptr;  // '/'
        case VK_OEM_1:      return shift ? L"：" : L"；";
        case VK_OEM_4:      return shift ? nullptr : L"【";  // '['
        case VK_OEM_6:      return shift ? nullptr : L"】";  // ']'
        case VK_OEM_5:      return shift ? nullptr : L"、";  // '\'
        case '1':           return shift ? L"！" : nullptr;
        case '9':           return shift ? L"（" : nullptr;
        case '0':           return shift ? L"）" : nullptr;
        case VK_OEM_7:  // '\'' — quote pairs; caller flips the toggle state
            if (shift) return _dquoteOpen ? L"”" : L"“";
            return _quoteOpen ? L"’" : L"‘";
    }
    return nullptr;
}

void CZhiPinTextService::_EmitPunct(ITfContext* pic, WPARAM wParam, bool shift) {
    const wchar_t* s = _MapPunct(wParam, shift);
    if (!s) return;
    if (wParam == VK_OEM_7) {
        if (shift)
            _dquoteOpen = !_dquoteOpen;
        else
            _quoteOpen = !_quoteOpen;
    }
    _InsertAtSelection(pic, s);
}

// ---- dispatch ----

void CZhiPinTextService::_HandleKeyDown(ITfContext* pic, WPARAM wParam) {
    bool shift = IsShiftDown();
    bool composing = _IsComposing();

    if (IsCtrlDown()) {
        if (composing && wParam == VK_DELETE) _ForgetHighlighted(pic);
        return;
    }

    if (!composing) {
        if (wParam >= 'A' && wParam <= 'Z' && !shift) {
            _AppendChar(pic, static_cast<char>('a' + (wParam - 'A')));
        } else if (_MapPunct(wParam, shift)) {
            _EmitPunct(pic, wParam, shift);
        }
        return;
    }

    // Composing:
    if (wParam >= 'A' && wParam <= 'Z') {
        _AppendChar(pic, static_cast<char>('a' + (wParam - 'A')));
        return;
    }
    if (wParam == VK_OEM_7 && !shift) {  // apostrophe joins the buffer
        _AppendChar(pic, '\'');
        return;
    }
    switch (wParam) {
        case VK_BACK:
            _Backspace(pic);
            return;
        case VK_ESCAPE:
            _CancelComposition(pic);
            return;
        case VK_RETURN:
            _CommitRaw(pic);
            return;
        case VK_SPACE:
            if (!_cands.empty())
                _SelectCandidate(pic, _highlight);
            else
                _CommitRaw(pic);
            return;
        case VK_UP:
            if (_highlight > 0) {
                --_highlight;
                if (_highlight < _pageStart) _pageStart -= CCandidateWindow::kPageSize;
                if (_pageStart < 0) _pageStart = 0;
                _UpdateCandidateUI();
            }
            return;
        case VK_DOWN:
            if (_highlight + 1 < (int)_cands.size()) {
                ++_highlight;
                if (_highlight >= _pageStart + CCandidateWindow::kPageSize)
                    _pageStart += CCandidateWindow::kPageSize;
                _UpdateCandidateUI();
            }
            return;
        case VK_PRIOR:
        case VK_OEM_MINUS:
            if (_pageStart >= CCandidateWindow::kPageSize) {
                _pageStart -= CCandidateWindow::kPageSize;
                _highlight = _pageStart;
                _UpdateCandidateUI();
            }
            return;
        case VK_NEXT:
        case VK_OEM_PLUS:
            if (_pageStart + CCandidateWindow::kPageSize < (int)_cands.size()) {
                _pageStart += CCandidateWindow::kPageSize;
                _highlight = _pageStart;
                _UpdateCandidateUI();
            }
            return;
    }
    if (wParam >= '1' && wParam <= '9' && !shift) {
        int idx = _pageStart + (int)(wParam - '1');
        if (idx < (int)_cands.size())
            _SelectCandidate(pic, idx);
        else
            MessageBeep(MB_OK);
        return;
    }
    if (_MapPunct(wParam, shift)) {
        // Commit the highlighted candidate (plus any leftover raw pinyin),
        // then emit the fullwidth punctuation.
        if (!_cands.empty()) {
            const CandItem& c = _cands[_highlight];
            if (c.consumed >= (int)_buffer.size()) {
                std::wstring full = _pending + c.text;
                std::string origRaw = _originalRaw;
                _EndCompositionCleanly(pic, full);
                if (ime::Engine* eng = GetEngine())
                    eng->learn(origRaw, WideToUtf8(full));
                _ResetState();
                _UpdateCandidateUI();
            } else {
                std::wstring full =
                    _pending + c.text + Utf8ToWide(_buffer.substr(c.consumed));
                _EndCompositionCleanly(pic, full);
                _ResetState();
                _UpdateCandidateUI();
            }
        } else {
            _CommitRaw(pic);
        }
        _EmitPunct(pic, wParam, shift);
        return;
    }
    // Everything else eaten while composing (arrows left/right, tab, ...): ignore.
}

void CZhiPinTextService::_ToggleMode(ITfContext* pic) {
    if (_IsComposing()) _CommitRaw(pic);  // keep exactly what was typed
    _chineseMode = !_chineseMode;
}

// ---- composition state machine (edit-session ops live in Composition.cpp) ----

void CZhiPinTextService::_AppendChar(ITfContext* pic, char c) {
    if (_buffer.size() >= kMaxRaw) {
        MessageBeep(MB_OK);
        return;
    }
    _buffer.push_back(c);
    _originalRaw.push_back(c);
    _Requery();
    _UpdateComposition(pic);
    _UpdateCandidateUI();
}

void CZhiPinTextService::_Backspace(ITfContext* pic) {
    if (_buffer.empty()) {
        _CancelComposition(pic);
        return;
    }
    _buffer.pop_back();
    if (!_originalRaw.empty()) _originalRaw.pop_back();
    if (_buffer.empty()) {
        if (!_pending.empty()) {
            // The user already chose these characters; keep them.
            std::wstring full = _pending;
            _EndCompositionCleanly(pic, full);
        } else {
            _EndCompositionCleanly(pic, L"");
        }
        _ResetState();
        _UpdateCandidateUI();
        return;
    }
    _Requery();
    _UpdateComposition(pic);
    _UpdateCandidateUI();
}

void CZhiPinTextService::_CancelComposition(ITfContext* pic) {
    _EndCompositionCleanly(pic, L"");
    _ResetState();
    _UpdateCandidateUI();
}

void CZhiPinTextService::_CommitRaw(ITfContext* pic) {
    std::wstring full = _pending + Utf8ToWide(_buffer);
    _EndCompositionCleanly(pic, full);
    _ResetState();
    _UpdateCandidateUI();
}

void CZhiPinTextService::_SelectCandidate(ITfContext* pic, int index) {
    if (index < 0 || index >= (int)_cands.size()) return;
    CandItem c = _cands[index];
    if (c.consumed >= (int)_buffer.size()) {
        std::wstring full = _pending + c.text;
        std::string origRaw = _originalRaw;
        _EndCompositionCleanly(pic, full);
        if (ime::Engine* eng = GetEngine()) eng->learn(origRaw, WideToUtf8(full));
        _ResetState();
        _UpdateCandidateUI();
        return;
    }
    _pending += c.text;
    _buffer.erase(0, (size_t)c.consumed);
    _Requery();
    _UpdateComposition(pic);
    _UpdateCandidateUI();
}

void CZhiPinTextService::_ForgetHighlighted(ITfContext*) {
    _ForgetAt(_highlight);
}

// Deleting a learned phrase never changes the composition text (pending +
// segmented pinyin stay as-is), so no edit session is needed here — safe to
// call from the candidate window's mouse handler too.
void CZhiPinTextService::_ForgetAt(int index) {
    if (!_IsComposing() || index < 0 || index >= (int)_cands.size()) return;
    const CandItem& c = _cands[index];
    if (!c.user) {
        MessageBeep(MB_OK);
        return;
    }
    if (ime::Engine* eng = GetEngine()) eng->forget(_buffer, c.textUtf8);
    _Requery();
    if (_highlight >= (int)_cands.size()) _highlight = _cands.empty() ? 0 : (int)_cands.size() - 1;
    _pageStart = (_highlight / CCandidateWindow::kPageSize) * CCandidateWindow::kPageSize;
    _UpdateCandidateUI();
}

void CZhiPinTextService::_Requery() {
    _cands.clear();
    _valid = false;
    _segmented.clear();
    _highlight = 0;
    _pageStart = 0;
    ime::Engine* eng = GetEngine();
    if (!eng || _buffer.empty()) return;
    ime::QueryResult r = eng->query(_buffer, 100);
    _valid = r.valid;
    _segmented = Utf8ToWide(r.segmented);
    _cands.reserve(r.candidates.size());
    for (const auto& c : r.candidates) {
        CandItem item;
        item.text = Utf8ToWide(c.text);
        item.textUtf8 = c.text;
        item.consumed = c.consumed;
        item.user = c.user;
        _cands.push_back(std::move(item));
    }
}

void CZhiPinTextService::_ResetState() {
    _buffer.clear();
    _originalRaw.clear();
    _pending.clear();
    _cands.clear();
    _segmented.clear();
    _valid = false;
    _highlight = 0;
    _pageStart = 0;
    _haveCaretPt = false;
}

std::wstring CZhiPinTextService::_DisplayText() const {
    if (_valid) return _pending + _segmented;
    return _pending + Utf8ToWide(_buffer);
}

void CZhiPinTextService::_UpdateCandidateUI() {
    if (!_IsComposing() || _cands.empty()) {
        _candWindow.Hide();
        return;
    }
    _candWindow.SetContent(&_cands, _pageStart, _highlight);
    POINT pt = _caretPt;
    if (!_haveCaretPt) GetCursorPos(&pt);
    _candWindow.Show(pt);
}
