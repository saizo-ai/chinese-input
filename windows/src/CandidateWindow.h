// Borderless topmost popup listing up to 9 candidates per page, GDI-drawn.
#pragma once

#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>

struct CandItem {
    std::wstring text;     // hanzi, UTF-16 for display
    std::string textUtf8;  // same text, UTF-8 for engine calls
    int consumed = 0;      // raw input chars covered
    bool user = false;     // learned phrase (deletable)
};

class CCandidateWindow {
public:
    static constexpr int kPageSize = 9;

    ~CCandidateWindow() { Destroy(); }

    bool Create();
    void Destroy();

    // items may be nullptr to clear. The vector must stay alive while shown.
    void SetContent(const std::vector<CandItem>* items, int pageStart, int highlight);
    // pt: screen coordinates of the text baseline area (window opens below it).
    void Show(POINT pt);
    void Hide();
    bool IsVisible() const { return _hwnd && IsWindowVisible(_hwnd); }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void _EnsureFonts(HDC hdc);
    SIZE _Measure(HDC hdc, std::vector<std::wstring>& lines) const;
    void _Paint(HDC hdc);

    HWND _hwnd = nullptr;
    HFONT _font = nullptr;
    HFONT _footerFont = nullptr;
    const std::vector<CandItem>* _items = nullptr;
    int _pageStart = 0;
    int _highlight = 0;
};
