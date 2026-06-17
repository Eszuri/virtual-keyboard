// Virtual Keyboard - Windows On-Screen Keyboard Clone
// Win32 C++ native application
// Build: cl /EHsc /O2 src\main.cpp /Fe:vkbd.exe user32.lib gdi32.lib

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// ── Constants ────────────────────────────────────────────────────────────────
constexpr int   KEY_ROWS          = 5;
constexpr int   MAX_KEYS_PER_ROW  = 15;
constexpr int   KEY_HEIGHT        = 48;
constexpr int   KEY_GAP           = 4;
constexpr int   TOP_MARGIN        = 8;
constexpr int   SIDE_MARGIN       = 8;

// Colors (dark theme, like Windows 11 on-screen keyboard)
constexpr COLORREF CLR_BG         = RGB(30, 30, 30);
constexpr COLORREF CLR_KEY_BG     = RGB(50, 50, 50);
constexpr COLORREF CLR_KEY_HOVER  = RGB(70, 70, 70);
constexpr COLORREF CLR_KEY_PRESS  = RGB(100, 140, 200);
constexpr COLORREF CLR_KEY_ACTIVE = RGB(60, 100, 160); // Caps Lock ON, Shift held
constexpr COLORREF CLR_KEY_BORDER = RGB(90, 90, 90);
constexpr COLORREF CLR_TEXT       = RGB(230, 230, 230);
constexpr COLORREF CLR_TEXT_DIM   = RGB(160, 160, 160);

// Timer IDs
constexpr UINT_PTR TIMER_CAPSLOCK   = 1;
constexpr UINT_PTR TIMER_REPEAT     = 2;
constexpr UINT     REPEAT_DELAY     = 400;
constexpr UINT     REPEAT_RATE      = 50;

// ── Key Definition ───────────────────────────────────────────────────────────
struct KeyDef {
    const wchar_t* label;       // Display label (lowercase / normal)
    const wchar_t* shiftLabel;  // Display label when Shift is active
    BYTE           vk;          // Virtual-Key code
    float          widthMul;    // Relative width multiplier (1.0 = standard)
    bool           isModifier;  // Shift, Ctrl, Alt, Win
    bool           isToggle;    // Caps Lock (stays pressed)
    bool           isSpecial;   // Backspace, Tab, Enter, Space (rendered wider label)
};

// ── Per-key runtime state ────────────────────────────────────────────────────
struct KeyState {
    RECT   rect;        // Pixel rect (computed on paint/layout)
    bool   hovered;     // Mouse is over this key
    bool   pressed;     // Mouse button down on this key
    bool   active;      // Toggle is on (Caps Lock) or modifier is held
};

// ── Quick-panel shortcut definition ──────────────────────────────────────────
struct ShortcutDef {
    const wchar_t* label;
    const wchar_t* hint;    // e.g. "Ctrl+C"
    BYTE  mod;              // 0=none, 1=Ctrl, 2=Shift, 3=Alt, 4=Ctrl+Shift
    BYTE  vk;
};

constexpr int   SHORTCUT_COUNT   = 6;
constexpr int   SHORTCUT_COLS    = 2;     // 2 columns in sidebar
constexpr int   SHORTCUT_ROWS    = 3;     // 3 rows
constexpr int   SIDEBAR_WIDTH    = 280;   // wider for 2 columns

// ── Forward declarations ─────────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void     LayoutKeys(HWND hwnd);
void     PaintKeyboard(HWND hwnd);
int      HitTestKey(POINT pt);
void     SendKeyPress(const KeyDef& key, bool down);
void     UpdateModifierState();
void     ReleaseLatchedModifiers(HWND hwnd);
void     LatchModifier(BYTE vk, HWND hwnd);
void     UnlatchModifier(BYTE vk, HWND hwnd);
bool     IsLatched(BYTE vk);
void     StartKeyRepeat(HWND hwnd, int keyIndex);
void     StopKeyRepeat(HWND hwnd);
void     SendShortcut(const ShortcutDef& sc);
int      HitTestShortcut(POINT pt);
void     ResizeWindowForPanel(HWND hwnd);
// ── Global state ─────────────────────────────────────────────────────────────
HINSTANCE   g_hInst;
HFONT       g_hFont      = nullptr;
HFONT       g_hFontSmall = nullptr;
int         g_totalKeys  = 0;
int         g_rowCounts[KEY_ROWS] = {14, 14, 13, 12, 8};
int         g_hoveredKey = -1;
int         g_pressedKey = -1;
bool        g_shiftDown  = false;
bool        g_ctrlDown   = false;
bool        g_altDown    = false;
bool        g_winDown    = false;
bool        g_capsLock   = false;
bool        g_shiftLatched = false;  // Sticky Shift
bool        g_ctrlLatched  = false;  // Sticky Ctrl
bool        g_altLatched   = false;  // Sticky Alt
bool        g_winLatched   = false;  // Sticky Win
int         g_windowWidth  = 900;
int         g_windowHeight = 0;
int         g_baseWidth    = 900;  // keyboard width without sidebar
int         g_repeatPhase  = 0;    // 0=idle, 1=initial delay, 2=fast repeat

// Quick-panel state
bool        g_showQuickPanel = false;
RECT        g_shortcutRects[SHORTCUT_COUNT] = {};
int         g_hoveredShort   = -1;
int         g_pressedShort   = -1;

// ── Keyboard Layout ──────────────────────────────────────────────────────────
// QWERTZ (Y↔Z swapped) with standard US symbols — 61 keys
KeyDef g_keys[] = {
    // Row 0: Esc, 1-0, -, =, Backspace (14 keys)
    {L"Esc", nullptr,     VK_ESCAPE,    0.75f, false, false, false},
    {L"1",   L"!",        '1',          0.85f, false, false, false},
    {L"2",   L"@",        '2',          0.85f, false, false, false},
    {L"3",   L"#",        '3',          0.85f, false, false, false},
    {L"4",   L"$",        '4',          0.85f, false, false, false},
    {L"5",   L"%",        '5',          0.85f, false, false, false},
    {L"6",   L"^",        '6',          0.85f, false, false, false},
    {L"7",   L"&",        '7',          0.85f, false, false, false},
    {L"8",   L"*",        '8',          0.85f, false, false, false},
    {L"9",   L"(",        '9',          0.85f, false, false, false},
    {L"0",   L")",        '0',          0.85f, false, false, false},
    {L"-",   L"_",        VK_OEM_MINUS, 0.85f, false, false, false},
    {L"=",   L"+",        VK_OEM_PLUS,  0.85f, false, false, false},
    {L"Bcksp", nullptr,  VK_BACK,      1.50f, false, false, true},   // Backspace

    // Row 1: Tab, Q-P, [, ], \ — Z replaces Y (14 keys)
    {L"Tab", nullptr,     VK_TAB,       1.25f, false, false, false},
    {L"q",   L"Q",        'Q',          1.00f, false, false, false},
    {L"w",   L"W",        'W',          1.00f, false, false, false},
    {L"e",   L"E",        'E',          1.00f, false, false, false},
    {L"r",   L"R",        'R',          1.00f, false, false, false},
    {L"t",   L"T",        'T',          1.00f, false, false, false},
    {L"z",   L"Z",        'Z',          1.00f, false, false, false},   // QWERTZ swap
    {L"u",   L"U",        'U',          1.00f, false, false, false},
    {L"i",   L"I",        'I',          1.00f, false, false, false},
    {L"o",   L"O",        'O',          1.00f, false, false, false},
    {L"p",   L"P",        'P',          1.00f, false, false, false},
    {L"[",   L"{",        VK_OEM_4,     1.00f, false, false, false},
    {L"]",   L"}",        VK_OEM_6,     1.00f, false, false, false},
    {L"\\",  L"|",        VK_OEM_5,     1.25f, false, false, false},

    // Row 2: Caps, A-L, ;, ', Enter (13 keys)
    {L"Caps", nullptr,    VK_CAPITAL,   1.50f, false, true,  false},
    {L"a",   L"A",        'A',          1.00f, false, false, false},
    {L"s",   L"S",        'S',          1.00f, false, false, false},
    {L"d",   L"D",        'D',          1.00f, false, false, false},
    {L"f",   L"F",        'F',          1.00f, false, false, false},
    {L"g",   L"G",        'G',          1.00f, false, false, false},
    {L"h",   L"H",        'H',          1.00f, false, false, false},
    {L"j",   L"J",        'J',          1.00f, false, false, false},
    {L"k",   L"K",        'K',          1.00f, false, false, false},
    {L"l",   L"L",        'L',          1.00f, false, false, false},
    {L";",   L":",        VK_OEM_1,     1.00f, false, false, false},
    {L"'",   L"\"",       VK_OEM_7,     1.00f, false, false, false},
    {L"Enter", nullptr,   VK_RETURN,    1.75f, false, false, true},

    // Row 3: Shift, Y-M, ,, ., /, Shift — Y replaces Z (12 keys)
    {L"Shift", nullptr,   VK_LSHIFT,    1.75f, true,  false, false},
    {L"y",   L"Y",        'Y',          1.00f, false, false, false},   // QWERTZ swap
    {L"x",   L"X",        'X',          1.00f, false, false, false},
    {L"c",   L"C",        'C',          1.00f, false, false, false},
    {L"v",   L"V",        'V',          1.00f, false, false, false},
    {L"b",   L"B",        'B',          1.00f, false, false, false},
    {L"n",   L"N",        'N',          1.00f, false, false, false},
    {L"m",   L"M",        'M',          1.00f, false, false, false},
    {L",",   L"<",        VK_OEM_COMMA, 1.00f, false, false, false},
    {L".",   L">",        VK_OEM_PERIOD,1.00f, false, false, false},
    {L"/",   L"?",        VK_OEM_2,     1.00f, false, false, false},
    {L"Shift", nullptr,   VK_RSHIFT,    2.00f, true,  false, false},

    // Row 4: Ctrl, Win, Alt, Space, Alt, Win, Menu, Ctrl (8 keys)
    {L"Ctrl", nullptr,    VK_LCONTROL,  1.25f, true,  false, false},
    {L"Win", nullptr,     VK_LWIN,      1.00f, true,  false, false},
    {L"Alt", nullptr,     VK_LMENU,     1.00f, true,  false, false},
    {L"",   nullptr,      VK_SPACE,     5.00f, false, false, true},
    {L"Alt", nullptr,     VK_RMENU,     1.00f, true,  false, false},
    {L"Win", nullptr,     VK_RWIN,      1.00f, true,  false, false},
    {L"Menu", nullptr,    VK_APPS,      1.00f, false, false, false},
    {L"Ctrl", nullptr,    VK_RCONTROL,  1.25f, true,  false, false},
};

KeyState g_keyStates[_countof(g_keys)] = {};

// Total: 14 + 14 + 13 + 12 + 8 = 61 keys (QWERTZ-US layout)
// Row 0: 14 keys (indices 0-13)
// Row 1: 14 keys (indices 14-27)
// Row 2: 13 keys (indices 28-40)
// Row 3: 12 keys (indices 41-52)
// Row 4: 8 keys  (indices 53-60)

int g_rowDefs[KEY_ROWS][MAX_KEYS_PER_ROW] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13},                        // 14
    {14,15,16,17,18,19,20,21,22,23,24,25,26,27},                         // 14
    {28,29,30,31,32,33,34,35,36,37,38,39,40},                            // 13
    {41,42,43,44,45,46,47,48,49,50,51,52},                               // 12
    {53,54,55,56,57,58,59,60}                                             // 8
};

// ── Shortcut panel definitions ───────────────────────────────────────────────
ShortcutDef g_shortcuts[SHORTCUT_COUNT] = {
    {L"Copy",       L"Ctrl+C",   1, 'C'},
    {L"Paste",      L"Ctrl+V",   1, 'V'},
    {L"Cut",        L"Ctrl+X",   1, 'X'},
    {L"Undo",       L"Ctrl+Z",   1, 'Z'},
    {L"Redo",       L"Ctrl+Y",   1, 'Y'},
    {L"Select All", L"Ctrl+A",   1, 'A'},
};
// ── Entry Point ──────────────────────────────────────────────────────────────
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    g_hInst = hInstance;

    // Create fonts
    g_hFont = CreateFontW(
        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    g_hFontSmall = CreateFontW(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );

    // Register window class
    const wchar_t CLASS_NAME[] = L"VirtualKeyboardWnd";
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BG);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    g_totalKeys = _countof(g_keys);

    // Calculate default window size
    g_windowWidth  = g_baseWidth;
    g_windowHeight = KEY_ROWS * (KEY_HEIGHT + KEY_GAP) + TOP_MARGIN * 2 + KEY_GAP;

    // Create window — always on top, no activation, tool window
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        L"Virtual Keyboard",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        g_windowWidth, g_windowHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) return 1;

    // Read Caps Lock initial state
    g_capsLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Set timer for periodic Caps Lock sync
    SetTimer(hwnd, TIMER_CAPSLOCK, 500, nullptr);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(g_hFont);
    DeleteObject(g_hFontSmall);
    return (int)msg.wParam;
}

// ── Window Procedure ─────────────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE: {
        // Enable mouse tracking for hover
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_HOVER | TME_LEAVE, hwnd, 1};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_SIZE: {
        g_windowWidth  = LOWORD(lParam);
        g_windowHeight = HIWORD(lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_TIMER: {
        if (wParam == TIMER_CAPSLOCK) {
            // Sync Caps Lock state
            bool capsNow = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            if (capsNow != g_capsLock) {
                g_capsLock = capsNow;
                UpdateModifierState();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        } else if (wParam == TIMER_REPEAT && g_pressedKey >= 0) {
            // Key repeat — send key-up + key-down for each repeat tick
            KeyDef& kd = g_keys[g_pressedKey];
            SendKeyPress(kd, false);  // release
            SendKeyPress(kd, true);   // press again
            if (g_repeatPhase == 1) {
                // Switch from initial delay to fast repeat
                KillTimer(hwnd, TIMER_REPEAT);
                SetTimer(hwnd, TIMER_REPEAT, REPEAT_RATE, nullptr);
                g_repeatPhase = 2;
            }
        }
        return 0;
    }

    case WM_PAINT: {
        PaintKeyboard(hwnd);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // We handle background in WM_PAINT

    case WM_MOUSEMOVE: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};

        // ── Shortcut button hover ──────────────────────────────────────────
        int sIdx = HitTestShortcut(pt);
        if (g_hoveredShort >= 0 && g_hoveredShort != sIdx) {
            InvalidateRect(hwnd, &g_shortcutRects[g_hoveredShort], FALSE);
            g_hoveredShort = -1;
        }
        if (sIdx >= 0 && sIdx != g_hoveredShort) {
            g_hoveredShort = sIdx;
            InvalidateRect(hwnd, &g_shortcutRects[sIdx], FALSE);
        }
        if (sIdx < 0) g_hoveredShort = -1;

        // ── Normal key hover ───────────────────────────────────────────────
        int idx = HitTestKey(pt);

        // Clear old hover
        if (g_hoveredKey >= 0 && g_hoveredKey != idx) {
            g_keyStates[g_hoveredKey].hovered = false;
            InvalidateRect(hwnd, &g_keyStates[g_hoveredKey].rect, FALSE);
        }
        // Set new hover
        if (idx >= 0 && idx != g_hoveredKey) {
            g_keyStates[idx].hovered = true;
            InvalidateRect(hwnd, &g_keyStates[idx].rect, FALSE);
        }
        g_hoveredKey = idx;

        // Track leave for next time
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_HOVER | TME_LEAVE, hwnd, 1};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSEHOVER:
        return 0; // Handled via WM_MOUSEMOVE

    case WM_MOUSELEAVE: {
        if (g_hoveredKey >= 0) {
            g_keyStates[g_hoveredKey].hovered = false;
            InvalidateRect(hwnd, &g_keyStates[g_hoveredKey].rect, FALSE);
            g_hoveredKey = -1;
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};

        // Shortcut button?
        int si = HitTestShortcut(pt);
        if (si >= 0) {
            g_pressedShort = si;
            InvalidateRect(hwnd, &g_shortcutRects[si], FALSE);
            SetCapture(hwnd);
            SendShortcut(g_shortcuts[si]);
            return 0;
        }

        // Normal keyboard keys
        int idx = HitTestKey(pt);
        if (idx >= 0) {
            KeyDef& kd = g_keys[idx];

            // Menu key → toggle quick panel
            if (kd.vk == VK_APPS) {
                g_showQuickPanel = !g_showQuickPanel;
                ResizeWindowForPanel(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (kd.isModifier) {
                // Modifier key: toggle latch
                if (IsLatched(kd.vk)) {
                    UnlatchModifier(kd.vk, hwnd);
                } else {
                    LatchModifier(kd.vk, hwnd);
                }
                // Visual flash feedback
                g_pressedKey = idx;
                g_keyStates[idx].pressed = true;
                InvalidateRect(hwnd, &g_keyStates[idx].rect, FALSE);
                SetCapture(hwnd);
            } else if (kd.isToggle && kd.vk == VK_CAPITAL) {
                // Caps Lock: toggle
                g_pressedKey = idx;
                g_keyStates[idx].pressed = true;
                InvalidateRect(hwnd, &g_keyStates[idx].rect, FALSE);
                SetCapture(hwnd);
                SendKeyPress(kd, true);
            } else {
                // Regular key
                g_pressedKey = idx;
                g_keyStates[idx].pressed = true;
                InvalidateRect(hwnd, &g_keyStates[idx].rect, FALSE);
                SetCapture(hwnd);
                SendKeyPress(kd, true);
                // Start key repeat for hold-to-repeat
                StartKeyRepeat(hwnd, idx);
            }
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        StopKeyRepeat(hwnd);

        // Shortcut button release
        if (g_pressedShort >= 0) {
            g_pressedShort = -1;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (g_pressedKey >= 0) {
            int idx = g_pressedKey;
            KeyDef& kd = g_keys[idx];
            g_keyStates[idx].pressed = false;

            if (kd.isModifier) {
                // Visual flash done only (latch already handled on down)
            } else if (kd.isToggle && kd.vk == VK_CAPITAL) {
                SendKeyPress(kd, false);
                g_capsLock = !g_capsLock;
                UpdateModifierState();
                // Release latched modifiers when pressing Caps Lock
                ReleaseLatchedModifiers(hwnd);
            } else {
                // Regular key: send key up
                SendKeyPress(kd, false);
                // Release all latched modifiers after the regular key
                ReleaseLatchedModifiers(hwnd);
            }

            g_pressedKey = -1;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_CAPTURECHANGED: {
        StopKeyRepeat(hwnd);
        if (g_pressedShort >= 0) {
            g_pressedShort = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (g_pressedKey >= 0) {
            // Only send key up for non-modifier, non-toggle keys
            KeyDef& kd = g_keys[g_pressedKey];
            if (!kd.isModifier && !(kd.isToggle && kd.vk == VK_CAPITAL)) {
                SendKeyPress(kd, false);
            }
            g_keyStates[g_pressedKey].pressed = false;
            g_pressedKey = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_CAPSLOCK);
        KillTimer(hwnd, TIMER_REPEAT);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ── Layout: compute pixel rects for every key + sidebar shortcuts ────────────
void LayoutKeys(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    int cw = client.right - client.left;
    int ch = client.bottom - client.top;

    // Sidebar takes space on the right when visible
    int sidebarW = g_showQuickPanel ? (SIDEBAR_WIDTH + SIDE_MARGIN) : 0;
    int availW = cw - SIDE_MARGIN * 2 - sidebarW;

    // Main keys — fixed height, width adapts to available space
    int keyH = (ch - TOP_MARGIN * 2 - (KEY_ROWS - 1) * KEY_GAP) / KEY_ROWS;
    if (keyH < 20) keyH = 20;

    int y = TOP_MARGIN;

    for (int row = 0; row < KEY_ROWS; row++) {
        float totalMul = 0.0f;
        for (int c = 0; c < g_rowCounts[row]; c++) {
            int ki = g_rowDefs[row][c];
            totalMul += g_keys[ki].widthMul;
        }

        float unitW = (float)(availW - (g_rowCounts[row] - 1) * KEY_GAP) / totalMul;
        if (unitW < 10.0f) unitW = 10.0f;

        int x = SIDE_MARGIN;
        for (int c = 0; c < g_rowCounts[row]; c++) {
            int ki = g_rowDefs[row][c];
            int kw = (int)(g_keys[ki].widthMul * unitW);

            g_keyStates[ki].rect.left   = x;
            g_keyStates[ki].rect.top    = y;
            g_keyStates[ki].rect.right  = x + kw;
            g_keyStates[ki].rect.bottom = y + keyH;

            x += kw + KEY_GAP;
        }
        y += keyH + KEY_GAP;
    }

    // ── Sidebar shortcut buttons (right side, 2-column grid) ──────────────
    if (g_showQuickPanel) {
        int sx = cw - SIDE_MARGIN - SIDEBAR_WIDTH;
        int sy = TOP_MARGIN;
        int colW = (SIDEBAR_WIDTH - KEY_GAP) / SHORTCUT_COLS;
        int sbtnH = (ch - TOP_MARGIN * 2 - (SHORTCUT_ROWS - 1) * KEY_GAP) / SHORTCUT_ROWS;
        if (sbtnH < 20) sbtnH = 20;

        for (int i = 0; i < SHORTCUT_COUNT; i++) {
            int col = i % SHORTCUT_COLS;
            int row = i / SHORTCUT_COLS;

            g_shortcutRects[i].left   = sx + col * (colW + KEY_GAP);
            g_shortcutRects[i].top    = sy + row * (sbtnH + KEY_GAP);
            g_shortcutRects[i].right  = g_shortcutRects[i].left + colW;
            g_shortcutRects[i].bottom = g_shortcutRects[i].top + sbtnH;
        }
    }
}

// ── Hit-test: which key is under point pt (client coords) ────────────────────
int HitTestKey(POINT pt) {
    for (int i = 0; i < g_totalKeys; i++) {
        if (PtInRect(&g_keyStates[i].rect, pt))
            return i;
    }
    return -1;
}

// ── Hit-test: which shortcut button is under point? ──────────────────────────
int HitTestShortcut(POINT pt) {
    if (!g_showQuickPanel) return -1;
    for (int i = 0; i < SHORTCUT_COUNT; i++) {
        if (PtInRect(&g_shortcutRects[i], pt))
            return i;
    }
    return -1;
}

// ── Resize window to fit or hide the sidebar ─────────────────────────────────
void ResizeWindowForPanel(HWND hwnd) {
    int newW = g_baseWidth + (g_showQuickPanel ? (SIDEBAR_WIDTH + SIDE_MARGIN) : 0);
    RECT wr;
    GetWindowRect(hwnd, &wr);
    SetWindowPos(hwnd, nullptr, 0, 0, newW, wr.bottom - wr.top,
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
}

// ── Send a shortcut key combination ──────────────────────────────────────────
void SendShortcut(const ShortcutDef& sc) {
    INPUT inputs[4] = {};
    int count = 0;

    // Press modifiers
    if (sc.mod == 1 || sc.mod == 4) {  // Ctrl
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_CONTROL;
        count++;
    }
    if (sc.mod == 2 || sc.mod == 4) {  // Shift
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_SHIFT;
        count++;
    }
    if (sc.mod == 3) {  // Alt
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_MENU;
        count++;
    }

    // Press the main key
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = sc.vk;
    int mainIdx = count;
    count++;

    // Release main key
    inputs[count] = inputs[mainIdx];
    inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
    count++;

    // Release modifiers in reverse order
    for (int i = mainIdx - 1; i >= 0; i--) {
        inputs[count] = inputs[i];
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    }

    SendInput(count, inputs, sizeof(INPUT));
}

// ── Paint the entire keyboard ────────────────────────────────────────────────
void PaintKeyboard(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT client;
    GetClientRect(hwnd, &client);

    // Double-buffer
    int cw = client.right - client.left;
    int ch = client.bottom - client.top;
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, cw, ch);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // Background
    HBRUSH bgBrush = CreateSolidBrush(CLR_BG);
    FillRect(memDC, &client, bgBrush);
    DeleteObject(bgBrush);

    // Re-layout and draw keys
    LayoutKeys(hwnd);

    SetBkMode(memDC, TRANSPARENT);

    for (int i = 0; i < g_totalKeys; i++) {
        RECT& r = g_keyStates[i].rect;
        KeyDef& kd = g_keys[i];
        KeyState& ks = g_keyStates[i];

        // Determine fill color
        COLORREF fill = CLR_KEY_BG;
        if (ks.pressed) {
            fill = CLR_KEY_PRESS;
        } else if (ks.active) {
            fill = CLR_KEY_ACTIVE;
        } else if (ks.hovered) {
            fill = CLR_KEY_HOVER;
        }

        // Draw key background (rounded rect via regions)
        HBRUSH keyBrush = CreateSolidBrush(fill);
        HPEN borderPen  = CreatePen(PS_SOLID, 1, (ks.hovered || ks.pressed || ks.active) ? CLR_KEY_PRESS : CLR_KEY_BORDER);
        HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, keyBrush);
        HPEN oldPen     = (HPEN)SelectObject(memDC, borderPen);

        // Rounded rectangle (small radius)
        int radius = 4;
        RoundRect(memDC, r.left, r.top, r.right, r.bottom, radius * 2, radius * 2);

        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(keyBrush);
        DeleteObject(borderPen);

        // Determine label
        const wchar_t* label = kd.label;
        bool shiftActive = g_shiftDown || g_shiftLatched;
        if (shiftActive && kd.shiftLabel != nullptr && kd.shiftLabel[0] != 0) {
            label = kd.shiftLabel;
        }
        // For letter keys: uppercase if shift or caps (but not both)
        if (!kd.isModifier && !kd.isToggle && !kd.isSpecial) {
            bool upper = shiftActive ^ g_capsLock;
            if (upper && kd.shiftLabel && wcslen(kd.shiftLabel) == 1 && iswalpha(kd.shiftLabel[0])) {
                label = kd.shiftLabel;
            } else if (!upper && iswalpha(kd.label[0])) {
                label = kd.label;
            }
        }

        // Select font
        HFONT useFont = (kd.isSpecial || kd.isModifier || kd.isToggle) ? g_hFontSmall : g_hFont;
        HFONT oldFont = (HFONT)SelectObject(memDC, useFont);

        // Measure text
        SIZE textSize;
        GetTextExtentPoint32W(memDC, label, (int)wcslen(label), &textSize);

        int tx = r.left + (r.right - r.left - textSize.cx) / 2;
        int ty = r.top  + (r.bottom - r.top - textSize.cy) / 2;

        // Text color
        SetTextColor(memDC, CLR_TEXT);
        TextOutW(memDC, tx, ty, label, (int)wcslen(label));

        SelectObject(memDC, oldFont);
    }

    // ── Draw shortcut grid (same key style as main keyboard) ───────────────
    if (g_showQuickPanel) {
        for (int i = 0; i < SHORTCUT_COUNT; i++) {
            RECT& r = g_shortcutRects[i];
            const ShortcutDef& sc = g_shortcuts[i];

            COLORREF fill = (i == g_pressedShort) ? CLR_KEY_PRESS
                          : (i == g_hoveredShort) ? CLR_KEY_HOVER
                          : CLR_KEY_BG;

            HBRUSH br = CreateSolidBrush(fill);
            HPEN pn = CreatePen(PS_SOLID, 1, (i == g_hoveredShort || i == g_pressedShort) ? CLR_KEY_PRESS : CLR_KEY_BORDER);
            HBRUSH oldBr = (HBRUSH)SelectObject(memDC, br);
            HPEN oldPn = (HPEN)SelectObject(memDC, pn);
            RoundRect(memDC, r.left, r.top, r.right, r.bottom, 8, 8);
            SelectObject(memDC, oldBr);
            SelectObject(memDC, oldPn);
            DeleteObject(br);
            DeleteObject(pn);

            // Label (top-centered)
            HFONT oldF = (HFONT)SelectObject(memDC, g_hFontSmall);
            SetTextColor(memDC, CLR_TEXT);
            SetBkMode(memDC, TRANSPARENT);

            SIZE tsz;
            GetTextExtentPoint32W(memDC, sc.label, (int)wcslen(sc.label), &tsz);
            int tx = r.left + (r.right - r.left - tsz.cx) / 2;
            int ty = r.top + 4;
            TextOutW(memDC, tx, ty, sc.label, (int)wcslen(sc.label));

            // Hint (bottom, dimmed)
            SetTextColor(memDC, CLR_TEXT_DIM);
            SIZE hsz;
            GetTextExtentPoint32W(memDC, sc.hint, (int)wcslen(sc.hint), &hsz);
            int hx = r.left + (r.right - r.left - hsz.cx) / 2;
            int hy = r.bottom - 4 - hsz.cy;
            TextOutW(memDC, hx, hy, sc.hint, (int)wcslen(sc.hint));

            SelectObject(memDC, oldF);
        }
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, cw, ch, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

// ── Send a key press or release using SendInput ──────────────────────────────
void SendKeyPress(const KeyDef& key, bool down) {
    INPUT input = {};
    input.type       = INPUT_KEYBOARD;
    input.ki.wVk     = key.vk;
    input.ki.wScan   = (WORD)MapVirtualKey(key.vk, MAPVK_VK_TO_VSC);
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;

    // For extended keys (Right Alt, Right Ctrl, arrows, etc.)
    UINT extendedKeys[] = {VK_RMENU, VK_RCONTROL, VK_INSERT, VK_DELETE,
                           VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
                           VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,
                           VK_NUMLOCK, VK_DIVIDE, VK_APPS,
                           VK_LWIN, VK_RWIN};
    for (UINT ek : extendedKeys) {
        if (key.vk == ek) {
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            break;
        }
    }

    SendInput(1, &input, sizeof(INPUT));
}

// ── Update active flags on modifier/toggle keys ──────────────────────────────
void UpdateModifierState() {
    for (int i = 0; i < g_totalKeys; i++) {
        KeyDef& kd = g_keys[i];
        if (kd.isToggle && kd.vk == VK_CAPITAL) {
            g_keyStates[i].active = g_capsLock;
        } else if (kd.vk == VK_LSHIFT || kd.vk == VK_RSHIFT) {
            g_keyStates[i].active = g_shiftDown || g_shiftLatched;
        } else if (kd.vk == VK_LCONTROL || kd.vk == VK_RCONTROL) {
            g_keyStates[i].active = g_ctrlDown || g_ctrlLatched;
        } else if (kd.vk == VK_LMENU || kd.vk == VK_RMENU) {
            g_keyStates[i].active = g_altDown || g_altLatched;
        } else if (kd.vk == VK_LWIN || kd.vk == VK_RWIN) {
            g_keyStates[i].active = g_winDown || g_winLatched;
        } else if (kd.vk == VK_APPS) {
            g_keyStates[i].active = g_showQuickPanel;
        }
    }
}

// ── Release all latched modifiers ──────────────────────────────────────────
void ReleaseLatchedModifiers(HWND hwnd) {
    if (g_shiftLatched) {
        KeyDef shiftKey = {L"", nullptr, VK_LSHIFT, 0, true, false, false};
        SendKeyPress(shiftKey, false);
        g_shiftLatched = false;
    }
    if (g_ctrlLatched) {
        KeyDef ctrlKey = {L"", nullptr, VK_LCONTROL, 0, true, false, false};
        SendKeyPress(ctrlKey, false);
        g_ctrlLatched = false;
    }
    if (g_altLatched) {
        KeyDef altKey = {L"", nullptr, VK_LMENU, 0, true, false, false};
        SendKeyPress(altKey, false);
        g_altLatched = false;
    }
    if (g_winLatched) {
        KeyDef winKey = {L"", nullptr, VK_LWIN, 0, true, false, false};
        SendKeyPress(winKey, false);
        g_winLatched = false;
    }
    UpdateModifierState();
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ── Latch a modifier key (hold down until released by regular key) ────────
void LatchModifier(BYTE vk, HWND hwnd) {
    KeyDef modKey = {L"", nullptr, vk, 0, true, false, false};
    SendKeyPress(modKey, true);  // press and hold in the system
    if (vk == VK_LSHIFT || vk == VK_RSHIFT)      g_shiftLatched = true;
    else if (vk == VK_LCONTROL || vk == VK_RCONTROL) g_ctrlLatched = true;
    else if (vk == VK_LMENU || vk == VK_RMENU)   g_altLatched = true;
    else if (vk == VK_LWIN || vk == VK_RWIN)     g_winLatched = true;
    UpdateModifierState();
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ── Unlatch a modifier ────────────────────────────────────────────────────
void UnlatchModifier(BYTE vk, HWND hwnd) {
    KeyDef modKey = {L"", nullptr, vk, 0, true, false, false};
    SendKeyPress(modKey, false);  // release
    if (vk == VK_LSHIFT || vk == VK_RSHIFT)      g_shiftLatched = false;
    else if (vk == VK_LCONTROL || vk == VK_RCONTROL) g_ctrlLatched = false;
    else if (vk == VK_LMENU || vk == VK_RMENU)   g_altLatched = false;
    else if (vk == VK_LWIN || vk == VK_RWIN)     g_winLatched = false;
    UpdateModifierState();
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ── Check if a key is currently latched ────────────────────────────────────
bool IsLatched(BYTE vk) {
    if (vk == VK_LSHIFT || vk == VK_RSHIFT)      return g_shiftLatched;
    if (vk == VK_LCONTROL || vk == VK_RCONTROL)   return g_ctrlLatched;
    if (vk == VK_LMENU || vk == VK_RMENU)          return g_altLatched;
    if (vk == VK_LWIN || vk == VK_RWIN)            return g_winLatched;
    return false;
}

// ── Key repeat: start auto-repeat timer for hold-to-repeat ──────────────────
void StartKeyRepeat(HWND hwnd, int keyIndex) {
    (void)keyIndex;  // reserved for per-key repeat behavior
    g_repeatPhase = 1;
    SetTimer(hwnd, TIMER_REPEAT, REPEAT_DELAY, nullptr);
}

// ── Key repeat: stop auto-repeat timer ─────────────────────────────────────
void StopKeyRepeat(HWND hwnd) {
    if (g_repeatPhase > 0) {
        KillTimer(hwnd, TIMER_REPEAT);
        g_repeatPhase = 0;
    }
}
