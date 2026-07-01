#include "CandidateWindow.h"

#include <algorithm>

#include "Globals.h"

namespace {

const WCHAR kClassName[] = L"ZhiPinCandidateWindow";
const WCHAR kFooter[] = L"空格:选定  -/=:翻页  回车:原文  Ctrl+Del:删除自造词";

constexpr int kPadX = 10;
constexpr int kPadY = 6;
constexpr int kLineGap = 4;

const COLORREF kBg = RGB(255, 255, 255);
const COLORREF kBorder = RGB(160, 160, 160);
const COLORREF kTextColor = RGB(20, 20, 20);
const COLORREF kHighlightBg = RGB(214, 231, 250);
const COLORREF kFooterColor = RGB(130, 130, 130);
const COLORREF kStarColor = RGB(20, 20, 20);

bool g_classRegistered = false;

}  // namespace

LRESULT CALLBACK CCandidateWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CCandidateWindow* self =
        reinterpret_cast<CCandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (self) self->_Paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool CCandidateWindow::Create() {
    if (_hwnd) return true;
    if (!g_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClassName;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        g_classRegistered = true;
    }
    _hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kClassName,
                            L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, g_hInst, this);
    return _hwnd != nullptr;
}

void CCandidateWindow::Destroy() {
    if (_hwnd) {
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    if (_font) {
        DeleteObject(_font);
        _font = nullptr;
    }
    if (_footerFont) {
        DeleteObject(_footerFont);
        _footerFont = nullptr;
    }
    _items = nullptr;
}

void CCandidateWindow::_EnsureFonts(HDC hdc) {
    if (_font) return;
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    LOGFONTW lf = {};
    lf.lfHeight = -MulDiv(16, dpi, 72);  // 16pt
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    _font = CreateFontIndirectW(&lf);
    lf.lfHeight = -MulDiv(9, dpi, 72);  // 9pt footer
    _footerFont = CreateFontIndirectW(&lf);
}

void CCandidateWindow::SetContent(const std::vector<CandItem>* items, int pageStart,
                                  int highlight) {
    _items = items;
    _pageStart = pageStart;
    _highlight = highlight;
    if (_hwnd && IsWindowVisible(_hwnd)) InvalidateRect(_hwnd, nullptr, TRUE);
}

SIZE CCandidateWindow::_Measure(HDC hdc, std::vector<std::wstring>& lines) const {
    lines.clear();
    if (_items) {
        int end = std::min<int>((int)_items->size(), _pageStart + kPageSize);
        for (int i = _pageStart; i < end; ++i) {
            const CandItem& it = (*_items)[i];
            std::wstring line = std::to_wstring(i - _pageStart + 1) + L". ";
            if (it.user) line += L"\x2605";  // ★
            line += it.text;
            lines.push_back(std::move(line));
        }
    }
    LONG width = 0;
    LONG lineH = 0;
    HGDIOBJ old = SelectObject(hdc, _font);
    for (const auto& l : lines) {
        SIZE sz = {};
        GetTextExtentPoint32W(hdc, l.c_str(), (int)l.size(), &sz);
        width = std::max(width, sz.cx);
        lineH = std::max(lineH, sz.cy);
    }
    SelectObject(hdc, _footerFont);
    SIZE fsz = {};
    GetTextExtentPoint32W(hdc, kFooter, (int)wcslen(kFooter), &fsz);
    width = std::max(width, fsz.cx);
    SelectObject(hdc, old);

    if (lineH == 0) lineH = 24;
    SIZE total;
    total.cx = width + kPadX * 2;
    total.cy = kPadY * 2 + (LONG)lines.size() * (lineH + kLineGap) + fsz.cy + kLineGap;
    return total;
}

void CCandidateWindow::_Paint(HDC hdc) {
    RECT rc;
    GetClientRect(_hwnd, &rc);
    HBRUSH bg = CreateSolidBrush(kBg);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    HBRUSH border = CreateSolidBrush(kBorder);
    FrameRect(hdc, &rc, border);
    DeleteObject(border);

    _EnsureFonts(hdc);
    SetBkMode(hdc, TRANSPARENT);

    std::vector<std::wstring> lines;
    if (_items) {
        int end = std::min<int>((int)_items->size(), _pageStart + kPageSize);
        HGDIOBJ old = SelectObject(hdc, _font);
        TEXTMETRICW tm = {};
        GetTextMetricsW(hdc, &tm);
        int lineH = tm.tmHeight;
        int y = kPadY;
        for (int i = _pageStart; i < end; ++i) {
            const CandItem& it = (*_items)[i];
            std::wstring line = std::to_wstring(i - _pageStart + 1) + L". ";
            if (it.user) line += L"\x2605";
            line += it.text;
            if (i == _highlight) {
                RECT hl = {2, y - 2, rc.right - 2, y + lineH + 2};
                HBRUSH hb = CreateSolidBrush(kHighlightBg);
                FillRect(hdc, &hl, hb);
                DeleteObject(hb);
            }
            SetTextColor(hdc, it.user ? kStarColor : kTextColor);
            TextOutW(hdc, kPadX, y, line.c_str(), (int)line.size());
            y += lineH + kLineGap;
        }
        SelectObject(hdc, _footerFont);
        SetTextColor(hdc, kFooterColor);
        TextOutW(hdc, kPadX, y, kFooter, (int)wcslen(kFooter));
        SelectObject(hdc, old);
    }
}

void CCandidateWindow::Show(POINT pt) {
    if (!_hwnd && !Create()) return;
    HDC hdc = GetDC(_hwnd);
    _EnsureFonts(hdc);
    std::vector<std::wstring> lines;
    SIZE sz = _Measure(hdc, lines);
    ReleaseDC(_hwnd, hdc);

    // Clamp to the monitor work area; flip above the caret if no room below.
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    int x = pt.x, y = pt.y + 4;
    if (GetMonitorInfoW(mon, &mi)) {
        if (x + sz.cx > mi.rcWork.right) x = mi.rcWork.right - sz.cx;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y + sz.cy > mi.rcWork.bottom) y = pt.y - sz.cy - 28;
        if (y < mi.rcWork.top) y = mi.rcWork.top;
    }
    SetWindowPos(_hwnd, HWND_TOPMOST, x, y, sz.cx, sz.cy,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(_hwnd, nullptr, TRUE);
}

void CCandidateWindow::Hide() {
    if (_hwnd) ShowWindow(_hwnd, SW_HIDE);
}
