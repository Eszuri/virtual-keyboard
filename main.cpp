// Virtual Keyboard - Windows On-Screen Keyboard Clone
// Win32 C++ native application
// Build: cl /EHsc /O2 src\main.cpp /Fe:vkbd.exe user32.lib gdi32.lib

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define _CRT_SECURE_NO_WARNINGS   // suppress MSVC unsafe-function warnings

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cwctype>
#include <cstring>

// ── Constants ────────────────────────────────────────────────────────────────
constexpr int   KEY_ROWS          = 5;
constexpr int   MAX_KEYS_PER_ROW  = 15;
constexpr int   KEY_HEIGHT        = 56;   // taller keys like Windows 11 OSK
constexpr int   KEY_GAP           = 6;    // more gap between keys
constexpr int   TOP_MARGIN        = 10;
constexpr int   SIDE_MARGIN       = 10;

// Colors — Windows 11 OSK dark theme (matches screenshot)
constexpr COLORREF CLR_BG            = RGB(36, 36, 36);       // dark charcoal background
constexpr COLORREF CLR_KEY_BG        = RGB(62, 62, 62);       // medium-dark key face
constexpr COLORREF CLR_KEY_SPECIAL   = RGB(48, 48, 48);       // slightly darker for special keys (Esc, Shift, etc.)
constexpr COLORREF CLR_KEY_HOVER     = RGB(82, 82, 82);       // lighter on hover
constexpr COLORREF CLR_KEY_PRESS     = RGB(0, 120, 215);      // Windows blue accent on press
constexpr COLORREF CLR_KEY_ACTIVE    = RGB(0, 99, 177);       // active modifier — deeper blue
constexpr COLORREF CLR_KEY_BORDER    = RGB(80, 80, 80);       // border color
constexpr COLORREF CLR_KEY_BORDER_BOTTOM = RGB(30, 30, 30);   // bottom-shadow border
constexpr COLORREF CLR_TEXT          = RGB(255, 255, 255);    // pure white text
constexpr COLORREF CLR_TEXT_DIM      = RGB(170, 170, 175);    // dimmed text for hints/shift labels

// Accent colors for special keys
constexpr COLORREF CLR_ACCENT_RED    = RGB(196, 43, 28);      // close button red
constexpr COLORREF CLR_ACCENT_ORANGE = RGB(210, 105, 30);     // warning/orange
constexpr COLORREF CLR_ACCENT_GREEN  = RGB(16, 124, 16);      // success/green

// Timer IDs
constexpr UINT_PTR TIMER_CAPSLOCK   = 1;
constexpr UINT_PTR TIMER_REPEAT     = 2;
constexpr UINT     REPEAT_DELAY     = 400;
constexpr UINT     REPEAT_RATE      = 50;

// ── Global hotkey — toggle show/hide keyboard (runtime configurable) ──────────
// Defaults (used if no vkbd.cfg found):
constexpr UINT  TOGGLE_HOTKEY_DEF_MOD = MOD_CONTROL | MOD_SHIFT;
constexpr UINT  TOGGLE_HOTKEY_DEF_VK  = 'K';
constexpr int   TOGGLE_HOTKEY_ID      = 0x4000;

// Runtime state (may be changed via UI)
UINT  g_toggleHotkeyMod = TOGGLE_HOTKEY_DEF_MOD;
UINT  g_toggleHotkeyVk  = TOGGLE_HOTKEY_DEF_VK;

// ── Configurable sidebar toggle button label ──────────────────────────────────
constexpr const wchar_t* TOGGLE_BTN_LABEL = L"Hide";   // sidebar button label
constexpr const wchar_t* TOGGLE_BTN_HINT  = L"Show/Hide";  // hint text

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

constexpr int   SHORTCUT_COUNT   = 10;
constexpr int   SHORTCUT_COLS    = 2;
constexpr int   SHORTCUT_ROWS    = 5;
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
void     LoadConfig();
void     SaveConfig();
void     UpdateToggleHint();
void     ShowHotkeyPicker(HWND parent);
LRESULT CALLBACK HotkeyPickerProc(HWND, UINT, WPARAM, LPARAM);
bool     IsAutoStartEnabled();
void     SetAutoStart(bool enable);
void     RecreateFonts();
// ── Global state
HINSTANCE   g_hInst;
HFONT       g_hFont      = nullptr;
HFONT       g_hFontSmall = nullptr;
int         g_totalKeys  = 0;
int         g_rowCounts[KEY_ROWS] = {14, 14, 13, 12, 12};
int         g_hoveredKey = -1;
int         g_pressedKey = -1;
bool        g_shiftDown  = false;
bool        g_ctrlDown   = false;
bool        g_altDown    = false;
bool        g_winDown    = false;
bool        g_capsLock   = false;
BYTE        g_shiftLatchedVk = 0;   // Sticky Shift: VK_LSHIFT or VK_RSHIFT, 0=none
BYTE        g_ctrlLatchedVk  = 0;   // Sticky Ctrl: VK_LCONTROL or VK_RCONTROL, 0=none
BYTE        g_altLatchedVk   = 0;   // Sticky Alt: VK_LMENU or VK_RMENU, 0=none
BYTE        g_winLatchedVk   = 0;   // Sticky Win: VK_LWIN or VK_RWIN, 0=none
int         g_windowWidth  = 950;
int         g_windowHeight = 0;
int         g_baseWidth    = 950;  // keyboard width without sidebar
int         g_repeatPhase  = 0;    // 0=idle, 1=initial delay, 2=fast repeat
int         g_fontSize     = 20;   // base font size (persisted in config)
static wchar_t g_fontHintBuf[16] = {};  // font size hint buffer

// Quick-panel state
bool        g_showQuickPanel = true;
RECT        g_shortcutRects[SHORTCUT_COUNT] = {};
int         g_hoveredShort   = -1;
int         g_pressedShort   = -1;
bool        g_autoStart      = false;  // registry Run key state

// ── Keyboard Layout
// QWERTY standard US layout — 61 keys
KeyDef g_keys[] = {
    // Row 0: Esc, 1-0, -, =, Backspace (14 keys)
    {L"Esc",   nullptr,   VK_ESCAPE,    0.75f, false, false, false},
    {L"1",     L"!",      '1',          0.85f, false, false, false},
    {L"2",     L"@",      '2',          0.85f, false, false, false},
    {L"3",     L"#",      '3',          0.85f, false, false, false},
    {L"4",     L"$",      '4',          0.85f, false, false, false},
    {L"5",     L"%",      '5',          0.85f, false, false, false},
    {L"6",     L"^",      '6',          0.85f, false, false, false},
    {L"7",     L"&",      '7',          0.85f, false, false, false},
    {L"8",     L"*",      '8',          0.85f, false, false, false},
    {L"9",     L"(",      '9',          0.85f, false, false, false},
    {L"0",     L")",      '0',          0.85f, false, false, false},
    {L"-",     L"_",      VK_OEM_MINUS, 0.85f, false, false, false},
    {L"=",     L"+",      VK_OEM_PLUS,  0.85f, false, false, false},
    {L"Bcksp", nullptr,   VK_BACK,      1.50f, false, false, true},

    // Row 1: Tab, Q-P, [, ], \ (14 keys) — QWERTY order
    {L"Tab",   nullptr,   VK_TAB,       1.25f, false, false, false},
    {L"q",     L"Q",      'Q',          1.00f, false, false, false},
    {L"w",     L"W",      'W',          1.00f, false, false, false},
    {L"e",     L"E",      'E',          1.00f, false, false, false},
    {L"r",     L"R",      'R',          1.00f, false, false, false},
    {L"t",     L"T",      'T',          1.00f, false, false, false},
    {L"y",     L"Y",      'Y',          1.00f, false, false, false},
    {L"u",     L"U",      'U',          1.00f, false, false, false},
    {L"i",     L"I",      'I',          1.00f, false, false, false},
    {L"o",     L"O",      'O',          1.00f, false, false, false},
    {L"p",     L"P",      'P',          1.00f, false, false, false},
    {L"[",     L"{",      VK_OEM_4,     1.00f, false, false, false},
    {L"]",     L"}",      VK_OEM_6,     1.00f, false, false, false},
    {L"\\",    L"|",      VK_OEM_5,     1.25f, false, false, false},

    // Row 2: Caps, A-L, ;, ', Enter (13 keys)
    {L"Caps",  nullptr,   VK_CAPITAL,   1.50f, false, true,  false},
    {L"a",     L"A",      'A',          1.00f, false, false, false},
    {L"s",     L"S",      'S',          1.00f, false, false, false},
    {L"d",     L"D",      'D',          1.00f, false, false, false},
    {L"f",     L"F",      'F',          1.00f, false, false, false},
    {L"g",     L"G",      'G',          1.00f, false, false, false},
    {L"h",     L"H",      'H',          1.00f, false, false, false},
    {L"j",     L"J",      'J',          1.00f, false, false, false},
    {L"k",     L"K",      'K',          1.00f, false, false, false},
    {L"l",     L"L",      'L',          1.00f, false, false, false},
    {L";",     L":",      VK_OEM_1,     1.00f, false, false, false},
    {L"'",     L"\"",     VK_OEM_7,     1.00f, false, false, false},
    {L"Enter", nullptr,   VK_RETURN,    1.75f, false, false, true},

    // Row 3: Shift, Z-M, ,, ., /, Shift (12 keys) — QWERTY: z first
    {L"Shift", nullptr,   VK_LSHIFT,    1.75f, true,  false, false},
    {L"z",     L"Z",      'Z',          1.00f, false, false, false},
    {L"x",     L"X",      'X',          1.00f, false, false, false},
    {L"c",     L"C",      'C',          1.00f, false, false, false},
    {L"v",     L"V",      'V',          1.00f, false, false, false},
    {L"b",     L"B",      'B',          1.00f, false, false, false},
    {L"n",     L"N",      'N',          1.00f, false, false, false},
    {L"m",     L"M",      'M',          1.00f, false, false, false},
    {L",",     L"<",      VK_OEM_COMMA, 1.00f, false, false, false},
    {L".",     L">",      VK_OEM_PERIOD,1.00f, false, false, false},
    {L"/",     L"?",      VK_OEM_2,     1.00f, false, false, false},
    {L"Shift", nullptr,   VK_RSHIFT,    2.00f, true,  false, false},

    // Row 4: Ctrl, Win, Alt, Space, Alt, Ctrl, ←, ↑, ↓, →, Home, End (12 keys)
    {L"Ctrl",  nullptr,   VK_LCONTROL,  1.25f, true,  false, false},
    {L"Win",   nullptr,   VK_LWIN,      1.00f, true,  false, false},
    {L"Alt",   nullptr,   VK_LMENU,     1.00f, true,  false, false},
    {L"",      nullptr,   VK_SPACE,     5.00f, false, false, true},
    {L"Alt",   nullptr,   VK_RMENU,     1.00f, true,  false, false},
    {L"Ctrl",  nullptr,   VK_RCONTROL,  1.00f, true,  false, false},
    {L"\u2190",nullptr,   VK_LEFT,      0.85f, false, false, false},
    {L"\u2191",nullptr,   VK_UP,        0.85f, false, false, false},
    {L"\u2193",nullptr,   VK_DOWN,      0.85f, false, false, false},
    {L"\u2192",nullptr,   VK_RIGHT,     0.85f, false, false, false},
    {L"Home",  nullptr,   VK_HOME,      0.85f, false, false, false},
    {L"End",   nullptr,   VK_END,       0.85f, false, false, false},
};

KeyState g_keyStates[_countof(g_keys)] = {};

// Total: 14 + 14 + 13 + 12 + 12 = 65 keys
// Row 0: 14 keys (indices 0-13)
// Row 1: 14 keys (indices 14-27)
// Row 2: 13 keys (indices 28-40)
// Row 3: 12 keys (indices 41-52)
// Row 4: 12 keys (indices 53-64)

int g_rowDefs[KEY_ROWS][MAX_KEYS_PER_ROW] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13},                        // 14
    {14,15,16,17,18,19,20,21,22,23,24,25,26,27},                         // 14
    {28,29,30,31,32,33,34,35,36,37,38,39,40},                            // 13
    {41,42,43,44,45,46,47,48,49,50,51,52},                               // 12
    {53,54,55,56,57,58,59,60,61,62,63,64}                                // 12
};

// ── Shortcut panel definitions ───────────────────────────────────────────────
ShortcutDef g_shortcuts[SHORTCUT_COUNT] = {
    {L"Copy",       L"Ctrl+C",   1, 'C'},
    {L"Paste",      L"Ctrl+V",   1, 'V'},
    {L"Cut",        L"Ctrl+X",   1, 'X'},
    {L"Undo",       L"Ctrl+Z",   1, 'Z'},
    {L"Redo",       L"Ctrl+Y",   1, 'Y'},
    {L"Select All", L"Ctrl+A",   1, 'A'},
    {TOGGLE_BTN_LABEL, TOGGLE_BTN_HINT, 0, 0},   // toggle show/hide (index 6)
    {L"AutoStart", L"OFF",    0, 0},              // auto-start toggle (index 7)
    {L"Font +",    L"20",    0, 0},               // increase font (index 8)
    {L"Font \u2212",L"20",    0, 0},              // decrease font (index 9)
};
// ── Entry Point ──────────────────────────────────────────────────────────────
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // ── Single-instance guard ────────────────────────────────────────────
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\VirtualKeyboardApp_SingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is already running — notify user then exit
        MessageBoxW(nullptr, L"Virtual Keyboard sudah berjalan.\nGunakan hotkey atau cek system tray untuk memunculkan.",
                    L"Virtual Keyboard", MB_OK | MB_ICONINFORMATION);
        CloseHandle(hMutex);
        return 0;
    }
    // hMutex stays alive; OS releases it on process exit

    g_hInst = hInstance;

    // Create fonts — crisp Segoe UI like Windows 11 OSK
    RecreateFonts();

    // Register window class
    const wchar_t CLASS_NAME[] = L"VirtualKeyboardWnd";
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BG);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    g_totalKeys = _countof(g_keys);

    // Load saved config BEFORE window creation (includes window size + font size)
    LoadConfig();
    RecreateFonts();  // apply font size from config

    // Apply defaults if config didn't set them (first run)
    if (g_windowWidth == 0)  g_windowWidth  = g_baseWidth + SIDEBAR_WIDTH + SIDE_MARGIN;
    if (g_windowHeight == 0) g_windowHeight = KEY_ROWS * (KEY_HEIGHT + KEY_GAP) + TOP_MARGIN * 2 + KEY_GAP + 30; // +30 for title bar

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

    // Register global hotkey for show/hide toggle
    if (!RegisterHotKey(hwnd, TOGGLE_HOTKEY_ID,
                        g_toggleHotkeyMod, g_toggleHotkeyVk)) {
        // Hotkey may be taken by another app — non-fatal, sidebar button still works
    }

    // Build hotkey hint string from current runtime settings
    UpdateToggleHint();

    // Check auto-start registry state
    g_autoStart = IsAutoStartEnabled();
    g_shortcuts[7].hint = g_autoStart ? L"ON" : L"OFF";

    // Initialize font size hints
    swprintf(g_fontHintBuf, 16, L"%d", g_fontSize);
    g_shortcuts[8].hint = g_fontHintBuf;
    g_shortcuts[9].hint = g_fontHintBuf;

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

    case WM_EXITSIZEMOVE: {
        // Save window size after user finishes resizing
        RECT wr;
        GetWindowRect(hwnd, &wr);
        g_windowWidth  = wr.right - wr.left;
        g_windowHeight = wr.bottom - wr.top;
        g_baseWidth    = g_windowWidth - (g_showQuickPanel ? (SIDEBAR_WIDTH + SIDE_MARGIN) : 0);
        SaveConfig();
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
            if (si == 6) {
                // Hide keyboard (toggle button)
                ShowWindow(hwnd, SW_HIDE);
            } else if (si == 7) {
                // Toggle auto-start
                g_autoStart = !g_autoStart;
                SetAutoStart(g_autoStart);
                g_shortcuts[7].hint = g_autoStart ? L"ON" : L"OFF";
                InvalidateRect(hwnd, &g_shortcutRects[7], FALSE);
            } else if (si == 8) {
                // Increase font size
                if (g_fontSize < 40) {
                    g_fontSize += 2;
                    RecreateFonts();
                    swprintf(g_fontHintBuf, 16, L"%d", g_fontSize);
                    g_shortcuts[8].hint = g_fontHintBuf;
                    g_shortcuts[9].hint = g_fontHintBuf;
                    SaveConfig();
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (si == 9) {
                // Decrease font size
                if (g_fontSize > 8) {
                    g_fontSize -= 2;
                    RecreateFonts();
                    swprintf(g_fontHintBuf, 16, L"%d", g_fontSize);
                    g_shortcuts[8].hint = g_fontHintBuf;
                    g_shortcuts[9].hint = g_fontHintBuf;
                    SaveConfig();
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                SendShortcut(g_shortcuts[si]);
            }
            return 0;
        }

        // Normal keyboard keys
        int idx = HitTestKey(pt);
        if (idx >= 0) {
            KeyDef& kd = g_keys[idx];

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

    case WM_RBUTTONDOWN: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        int si = HitTestShortcut(pt);
        if (si == 6) {
            // Right-click on Hide button → reconfigure hotkey
            ShowHotkeyPicker(hwnd);
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

    case WM_HOTKEY: {
        if (wParam == TOGGLE_HOTKEY_ID) {
            // Toggle show/hide
            if (IsWindowVisible(hwnd))
                ShowWindow(hwnd, SW_HIDE);
            else
                ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        }
        return 0;
    }

    case WM_CLOSE:
        // Hide instead of closing — app stays alive for hotkey toggle
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        UnregisterHotKey(hwnd, TOGGLE_HOTKEY_ID);
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

        bool isSpecialKey = kd.isModifier || kd.isToggle || kd.isSpecial
                         || kd.vk == VK_ESCAPE || kd.vk == VK_TAB
                         || kd.vk == VK_APPS;

        // Determine fill color
        COLORREF fill;
        if (ks.pressed) {
            fill = CLR_KEY_PRESS;
        } else if (ks.active) {
            fill = CLR_KEY_ACTIVE;
        } else if (ks.hovered) {
            fill = CLR_KEY_HOVER;
        } else if (isSpecialKey) {
            fill = CLR_KEY_SPECIAL;
        } else {
            fill = CLR_KEY_BG;
        }

        // Bottom shadow border (1px darker line at bottom for 3D feel)
        if (!ks.pressed) {
            HBRUSH shadowBrush = CreateSolidBrush(CLR_KEY_BORDER_BOTTOM);
            HPEN nullPen = (HPEN)GetStockObject(NULL_PEN);
            HBRUSH oldBr2 = (HBRUSH)SelectObject(memDC, shadowBrush);
            HPEN oldPn2 = (HPEN)SelectObject(memDC, nullPen);
            RoundRect(memDC, r.left, r.top + 2, r.right, r.bottom + 2, 10, 10);
            SelectObject(memDC, oldBr2);
            SelectObject(memDC, oldPn2);
            DeleteObject(shadowBrush);
        }

        // Key face background (rounded rect, larger radius like Win11 OSK)
        HBRUSH keyBrush = CreateSolidBrush(fill);
        HPEN borderPen;
        if (ks.pressed || ks.active) {
            borderPen = CreatePen(PS_SOLID, 1, CLR_KEY_PRESS);
        } else if (ks.hovered) {
            borderPen = CreatePen(PS_SOLID, 1, RGB(110, 110, 110));
        } else {
            borderPen = CreatePen(PS_SOLID, 1, CLR_KEY_BORDER);
        }
        HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, keyBrush);
        HPEN oldPen     = (HPEN)SelectObject(memDC, borderPen);

        // Large corner radius like Windows 11 OSK (8px)
        RoundRect(memDC, r.left, r.top, r.right, r.bottom, 10, 10);

        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(keyBrush);
        DeleteObject(borderPen);

        // Determine main label and hint label (dual-label keys swap on shift)
        bool shiftActive = g_shiftDown || g_shiftLatchedVk != 0;
        bool hasDualLabel = !kd.isModifier && !kd.isToggle && !kd.isSpecial
                          && kd.shiftLabel != nullptr && kd.shiftLabel[0] != 0;

        const wchar_t* mainLabel = kd.label;
        const wchar_t* hintLabel = nullptr;

        if (hasDualLabel) {
            if (iswalpha(kd.label[0])) {
                // Letter key: shift/caps XOR determines case, hint is the opposite
                bool upper = shiftActive ^ g_capsLock;
                if (upper) {
                    mainLabel = kd.shiftLabel;
                    hintLabel = kd.label;
                } else {
                    mainLabel = kd.label;
                    hintLabel = kd.shiftLabel;
                }
            } else {
                // Number/symbol key: shift swaps which label is primary
                if (shiftActive) {
                    mainLabel = kd.shiftLabel;
                    hintLabel = kd.label;
                } else {
                    mainLabel = kd.label;
                    hintLabel = kd.shiftLabel;
                }
            }
        }

        // Select font — use larger font for letter/number keys, smaller for special
        HFONT useFont = isSpecialKey ? g_hFontSmall : g_hFont;
        HFONT oldFont = (HFONT)SelectObject(memDC, useFont);

        // Main label: bottom-left for dual-label keys, centered otherwise
        SIZE textSize;
        GetTextExtentPoint32W(memDC, mainLabel, (int)wcslen(mainLabel), &textSize);

        int keyW = r.right - r.left;
        int keyH = r.bottom - r.top;

        int tx, ty;
        if (hasDualLabel) {
            tx = r.left + 7;
            ty = r.bottom - textSize.cy - 6;
        } else {
            tx = r.left + (keyW - textSize.cx) / 2;
            ty = r.top  + (keyH - textSize.cy) / 2;
        }

        SetTextColor(memDC, CLR_TEXT);
        TextOutW(memDC, tx, ty, mainLabel, (int)wcslen(mainLabel));

        // Draw the opposite label as a small hint at top-right
        if (hintLabel) {
            HFONT oldF2 = (HFONT)SelectObject(memDC, g_hFontSmall);
            SetTextColor(memDC, CLR_TEXT_DIM);
            SIZE shSize;
            GetTextExtentPoint32W(memDC, hintLabel, (int)wcslen(hintLabel), &shSize);
            int shx = r.right - shSize.cx - 6;
            int shy = r.top + 4;
            TextOutW(memDC, shx, shy, hintLabel, (int)wcslen(hintLabel));
            SelectObject(memDC, oldF2);
            SetTextColor(memDC, CLR_TEXT);
        }

        SelectObject(memDC, oldFont);
    }

    // ── Draw shortcut grid (right sidebar) ────────────────────────────────
    if (g_showQuickPanel) {
        for (int i = 0; i < SHORTCUT_COUNT; i++) {
            RECT& r = g_shortcutRects[i];
            const ShortcutDef& sc = g_shortcuts[i];

            COLORREF fill;
            if (i == g_pressedShort)               fill = CLR_KEY_PRESS;
            else if (i == g_hoveredShort)           fill = CLR_KEY_HOVER;
            else if (i == 7 && g_autoStart)         fill = CLR_KEY_ACTIVE;
            else                                    fill = CLR_KEY_SPECIAL;

            // Shadow
            HBRUSH shadowBr = CreateSolidBrush(CLR_KEY_BORDER_BOTTOM);
            HPEN nullPen2 = (HPEN)GetStockObject(NULL_PEN);
            HBRUSH oldBr3 = (HBRUSH)SelectObject(memDC, shadowBr);
            HPEN oldPn3 = (HPEN)SelectObject(memDC, nullPen2);
            RoundRect(memDC, r.left, r.top + 2, r.right, r.bottom + 2, 10, 10);
            SelectObject(memDC, oldBr3);
            SelectObject(memDC, oldPn3);
            DeleteObject(shadowBr);

            HBRUSH br = CreateSolidBrush(fill);
            HPEN pn = CreatePen(PS_SOLID, 1,
                (i == g_hoveredShort || i == g_pressedShort) ? CLR_KEY_PRESS : CLR_KEY_BORDER);
            HBRUSH oldBr = (HBRUSH)SelectObject(memDC, br);
            HPEN oldPn = (HPEN)SelectObject(memDC, pn);
            RoundRect(memDC, r.left, r.top, r.right, r.bottom, 10, 10);
            SelectObject(memDC, oldBr);
            SelectObject(memDC, oldPn);
            DeleteObject(br);
            DeleteObject(pn);

            // Label (centered vertically if no hint, otherwise top-centered)
            HFONT oldF = (HFONT)SelectObject(memDC, g_hFontSmall);
            SetTextColor(memDC, CLR_TEXT);
            SetBkMode(memDC, TRANSPARENT);

            SIZE tsz;
            GetTextExtentPoint32W(memDC, sc.label, (int)wcslen(sc.label), &tsz);
            int bh = r.bottom - r.top;
            int tx2 = r.left + (r.right - r.left - tsz.cx) / 2;
            int ty2 = r.top + (bh / 2) - tsz.cy - 2;
            TextOutW(memDC, tx2, ty2, sc.label, (int)wcslen(sc.label));

            // Hint (bottom, dimmed)
            SetTextColor(memDC, CLR_TEXT_DIM);
            SIZE hsz;
            GetTextExtentPoint32W(memDC, sc.hint, (int)wcslen(sc.hint), &hsz);
            int hx = r.left + (r.right - r.left - hsz.cx) / 2;
            int hy = r.top + (bh / 2) + 2;
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
        BYTE vk = kd.vk;
        if (kd.isToggle && vk == VK_CAPITAL) {
            g_keyStates[i].active = g_capsLock;
        } else if (vk == VK_LSHIFT) {
            g_keyStates[i].active = g_shiftDown || g_shiftLatchedVk == VK_LSHIFT;
        } else if (vk == VK_RSHIFT) {
            g_keyStates[i].active = g_shiftDown || g_shiftLatchedVk == VK_RSHIFT;
        } else if (vk == VK_LCONTROL) {
            g_keyStates[i].active = g_ctrlDown || g_ctrlLatchedVk == VK_LCONTROL;
        } else if (vk == VK_RCONTROL) {
            g_keyStates[i].active = g_ctrlDown || g_ctrlLatchedVk == VK_RCONTROL;
        } else if (vk == VK_LMENU) {
            g_keyStates[i].active = g_altDown || g_altLatchedVk == VK_LMENU;
        } else if (vk == VK_RMENU) {
            g_keyStates[i].active = g_altDown || g_altLatchedVk == VK_RMENU;
        } else if (vk == VK_LWIN) {
            g_keyStates[i].active = g_winDown || g_winLatchedVk == VK_LWIN;
        } else if (vk == VK_RWIN) {
            g_keyStates[i].active = g_winDown || g_winLatchedVk == VK_RWIN;
        }
    }
}

// ── Release all latched modifiers ──────────────────────────────────────────
void ReleaseLatchedModifiers(HWND hwnd) {
    if (g_shiftLatchedVk) {
        KeyDef shiftKey = {L"", nullptr, g_shiftLatchedVk, 0, true, false, false};
        SendKeyPress(shiftKey, false);
        g_shiftLatchedVk = 0;
    }
    if (g_ctrlLatchedVk) {
        KeyDef ctrlKey = {L"", nullptr, g_ctrlLatchedVk, 0, true, false, false};
        SendKeyPress(ctrlKey, false);
        g_ctrlLatchedVk = 0;
    }
    if (g_altLatchedVk) {
        KeyDef altKey = {L"", nullptr, g_altLatchedVk, 0, true, false, false};
        SendKeyPress(altKey, false);
        g_altLatchedVk = 0;
    }
    if (g_winLatchedVk) {
        KeyDef winKey = {L"", nullptr, g_winLatchedVk, 0, true, false, false};
        SendKeyPress(winKey, false);
        g_winLatchedVk = 0;
    }
    UpdateModifierState();
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ── Latch a modifier key (hold down until released by regular key) ────────
void LatchModifier(BYTE vk, HWND hwnd) {
    KeyDef modKey = {L"", nullptr, vk, 0, true, false, false};
    SendKeyPress(modKey, true);  // press and hold in the system
    if (vk == VK_LSHIFT || vk == VK_RSHIFT)      g_shiftLatchedVk = vk;
    else if (vk == VK_LCONTROL || vk == VK_RCONTROL) g_ctrlLatchedVk = vk;
    else if (vk == VK_LMENU || vk == VK_RMENU)   g_altLatchedVk = vk;
    else if (vk == VK_LWIN || vk == VK_RWIN)     g_winLatchedVk = vk;
    UpdateModifierState();
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ── Unlatch a modifier ────────────────────────────────────────────────────
void UnlatchModifier(BYTE vk, HWND hwnd) {
    KeyDef modKey = {L"", nullptr, vk, 0, true, false, false};
    SendKeyPress(modKey, false);  // release
    if (vk == VK_LSHIFT || vk == VK_RSHIFT)      g_shiftLatchedVk = 0;
    else if (vk == VK_LCONTROL || vk == VK_RCONTROL) g_ctrlLatchedVk = 0;
    else if (vk == VK_LMENU || vk == VK_RMENU)   g_altLatchedVk = 0;
    else if (vk == VK_LWIN || vk == VK_RWIN)     g_winLatchedVk = 0;
    UpdateModifierState();
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ── Check if a key is currently latched ────────────────────────────────────
bool IsLatched(BYTE vk) {
    if (vk == VK_LSHIFT || vk == VK_RSHIFT)      return g_shiftLatchedVk == vk;
    if (vk == VK_LCONTROL || vk == VK_RCONTROL)   return g_ctrlLatchedVk == vk;
    if (vk == VK_LMENU || vk == VK_RMENU)          return g_altLatchedVk == vk;
    if (vk == VK_LWIN || vk == VK_RWIN)            return g_winLatchedVk == vk;
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

// ── Recreate fonts at current g_fontSize ────────────────────────────────────
void RecreateFonts() {
    if (g_hFont) DeleteObject(g_hFont);
    if (g_hFontSmall) DeleteObject(g_hFontSmall);
    g_hFont = CreateFontW(
        g_fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
    int smallSz = g_fontSize - 6;
    if (smallSz < 8) smallSz = 8;
    g_hFontSmall = CreateFontW(
        smallSz, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );
}

// ── Config file path helper ────────────────────────────────────────────────
static wchar_t g_hotkeyHintStr[32] = {};  // shared hint buffer

void GetConfigPath(wchar_t* buf, int bufSize) {
    GetModuleFileNameW(nullptr, buf, bufSize);
    wchar_t* lastSlash = wcsrchr(buf, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(buf, bufSize, L"vkbd.cfg");
}

// ── Load hotkey config from vkbd.cfg ───────────────────────────────────────
void LoadConfig() {
    wchar_t path[MAX_PATH];
    GetConfigPath(path, MAX_PATH);
    FILE* f = _wfopen(path, L"r, ccs=UTF-8");
    if (!f) return;
    wchar_t line[128];
    while (fgetws(line, 128, f)) {
        UINT mod = 0, vk = 0;
        int w = 0, h = 0, fs = 0;
        if (swscanf(line, L"hotkey_mod=%u", &mod) == 1)
            g_toggleHotkeyMod = mod;
        if (swscanf(line, L"hotkey_vk=%u", &vk) == 1)
            g_toggleHotkeyVk = vk;
        if (swscanf(line, L"window_w=%d", &w) == 1)
            g_windowWidth = w;
        if (swscanf(line, L"window_h=%d", &h) == 1)
            g_windowHeight = h;
        if (swscanf(line, L"font_size=%d", &fs) == 1 && fs >= 8 && fs <= 40)
            g_fontSize = fs;
    }
    fclose(f);
}

// ── Save hotkey config to vkbd.cfg ─────────────────────────────────────────
void SaveConfig() {
    wchar_t path[MAX_PATH];
    GetConfigPath(path, MAX_PATH);
    FILE* f = _wfopen(path, L"w, ccs=UTF-8");
    if (!f) return;
    fwprintf(f, L"hotkey_mod=%u\nhotkey_vk=%u\nwindow_w=%d\nwindow_h=%d\nfont_size=%d\n",
             g_toggleHotkeyMod, g_toggleHotkeyVk,
             g_windowWidth, g_windowHeight, g_fontSize);
    fclose(f);
}

// ── Rebuild the sidebar hint string from current hotkey settings ───────────
void UpdateToggleHint() {
    int pos = 0;
    g_hotkeyHintStr[0] = L'\0';
    if (g_toggleHotkeyMod & MOD_CONTROL) { wcscpy(g_hotkeyHintStr, L"Ctrl+"); pos = 5; }
    if (g_toggleHotkeyMod & MOD_SHIFT)   { wcscpy(g_hotkeyHintStr + pos, L"Shift+"); pos += 6; }
    if (g_toggleHotkeyMod & MOD_ALT)     { wcscpy(g_hotkeyHintStr + pos, L"Alt+");  pos += 4; }
    if (g_toggleHotkeyMod & MOD_WIN)     { wcscpy(g_hotkeyHintStr + pos, L"Win+");  pos += 4; }

    // Append key name
    UINT vk = g_toggleHotkeyVk;
    if (vk == 0 && pos == 0) {
        wcscpy(g_hotkeyHintStr, L"(none)");
    } else if (vk >= 'A' && vk <= 'Z') {
        g_hotkeyHintStr[pos++] = (wchar_t)vk;
        g_hotkeyHintStr[pos] = L'\0';
    } else if (vk >= '0' && vk <= '9') {
        g_hotkeyHintStr[pos++] = (wchar_t)vk;
        g_hotkeyHintStr[pos] = L'\0';
    } else if (vk >= VK_F1 && vk <= VK_F12) {
        swprintf(g_hotkeyHintStr + pos, 8, L"F%d", vk - VK_F1 + 1);
    } else if (vk != 0) {
        swprintf(g_hotkeyHintStr + pos, 10, L"0x%X", vk);
    }
    g_shortcuts[6].hint = g_hotkeyHintStr;
}

// ── Hotkey Picker Dialog ───────────────────────────────────────────────────
LRESULT CALLBACK HotkeyPickerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Center on parent
        HWND parent = GetWindow(hwnd, GW_OWNER);
        if (parent) {
            RECT pr, dr;
            GetWindowRect(parent, &pr);
            GetWindowRect(hwnd, &dr);
            int dw = dr.right - dr.left, dh = dr.bottom - dr.top;
            SetWindowPos(hwnd, nullptr,
                pr.left + ((pr.right - pr.left) - dw) / 2,
                pr.top  + ((pr.bottom - pr.top) - dh) / 2,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        UINT vk = (UINT)wParam;
        // Ignore pure modifier releases/presses
        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU ||
            vk == VK_LWIN || vk == VK_RWIN ||
            vk == VK_LCONTROL || vk == VK_RCONTROL ||
            vk == VK_LSHIFT || vk == VK_RSHIFT ||
            vk == VK_LMENU || vk == VK_RMENU)
            return 0;
        // Escape cancels
        if (vk == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }

        // Read modifier states
        UINT newMod = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) newMod |= MOD_CONTROL;
        if (GetKeyState(VK_SHIFT)   & 0x8000) newMod |= MOD_SHIFT;
        if (GetKeyState(VK_MENU)    & 0x8000) newMod |= MOD_ALT;
        if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) newMod |= MOD_WIN;

        // Apply to owner
        HWND owner = GetWindow(hwnd, GW_OWNER);
        if (owner) {
            UnregisterHotKey(owner, TOGGLE_HOTKEY_ID);
            g_toggleHotkeyMod = newMod;
            g_toggleHotkeyVk  = vk;
            RegisterHotKey(owner, TOGGLE_HOTKEY_ID,
                          g_toggleHotkeyMod, g_toggleHotkeyVk);
            UpdateToggleHint();
            SaveConfig();
            // Force sidebar to repaint with new hint text
            InvalidateRect(owner, &g_shortcutRects[6], FALSE);
            InvalidateRect(owner, nullptr, FALSE);
            UpdateWindow(owner);
        }
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        HBRUSH bg = CreateSolidBrush(CLR_BG);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        HFONT fnt = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldF = (HFONT)SelectObject(hdc, fnt);
        SetBkMode(hdc, TRANSPARENT);

        // Current hotkey
        SetTextColor(hdc, CLR_TEXT_DIM);
        wchar_t cur[64];
        swprintf(cur, 64, L"Current: %s", g_hotkeyHintStr);
        SIZE sz;
        GetTextExtentPoint32W(hdc, cur, (int)wcslen(cur), &sz);
        TextOutW(hdc, (rc.right - sz.cx) / 2, 20, cur, (int)wcslen(cur));

        // Instruction
        SetTextColor(hdc, CLR_TEXT);
        const wchar_t* instr = L"Press new key combination...";
        GetTextExtentPoint32W(hdc, instr, (int)wcslen(instr), &sz);
        TextOutW(hdc, (rc.right - sz.cx) / 2, 52, instr, (int)wcslen(instr));

        // Cancel hint
        SetTextColor(hdc, CLR_TEXT_DIM);
        const wchar_t* esc = L"(Esc to cancel)";
        GetTextExtentPoint32W(hdc, esc, (int)wcslen(esc), &sz);
        TextOutW(hdc, (rc.right - sz.cx) / 2, 82, esc, (int)wcslen(esc));

        SelectObject(hdc, oldF);
        DeleteObject(fnt);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ── Show hotkey picker popup ───────────────────────────────────────────────
void ShowHotkeyPicker(HWND parent) {
    const wchar_t PICKER_CLASS[] = L"HotkeyPickerWnd";
    // Register once (check if already registered)
    WNDCLASSW wcTest = {};
    if (!GetClassInfoW(g_hInst, PICKER_CLASS, &wcTest)) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc   = HotkeyPickerProc;
        wc.hInstance     = g_hInst;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(CLR_BG);
        wc.lpszClassName = PICKER_CLASS;
        RegisterClassW(&wc);
    }

    HWND dlg = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        PICKER_CLASS, L"Configure Hotkey",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, 360, 170,
        parent, nullptr, g_hInst, nullptr);

    if (dlg) {
        ShowWindow(dlg, SW_SHOW);
        UpdateWindow(dlg);
        SetForegroundWindow(dlg);
    }
}

// ── Auto-start: check if registered in Windows Run key ───────────────────
bool IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    wchar_t path[MAX_PATH];
    DWORD size = sizeof(path);
    DWORD type = 0;
    LSTATUS result = RegQueryValueExW(hKey, L"VirtualKeyboard", nullptr,
                                      &type, (BYTE*)path, &size);
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

// ── Auto-start: enable/disable via Windows Run registry key ──────────────
void SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;

    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        RegSetValueExW(hKey, L"VirtualKeyboard", 0, REG_SZ,
                       (BYTE*)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, L"VirtualKeyboard");
    }
    RegCloseKey(hKey);
}
