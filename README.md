# Virtual Keyboard (Windows)

Virtual keyboard Windows native — dibuat dengan Win32 API dan C++17. Tanpa dependency eksternal, hanya `user32.lib` + `gdi32.lib`.

![screenshot](https://img.shields.io/badge/build-passing-brightgreen) ![lang](https://img.shields.io/badge/C%2B%2B-17-blue) ![platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)

---

## Fitur

- **QWERTZ layout** (Y↔Z swap) dengan 61 tombol
- **Dark theme** — ala Windows 11 on-screen keyboard
- **Sticky modifiers** — Shift, Ctrl, Alt, Win bisa di-latch (klik sekali = tahan)
- **Hold-to-repeat** — tahan Backspace/Space/huruf untuk auto-repeat (400ms delay awal, 50ms repeat)
- **Sidebar shortcut** — klik tombol **Menu** untuk toggle panel pintasan di kanan (6 tombol, 2 kolom)
  - Copy `Ctrl+C` — Paste `Ctrl+V`
  - Cut `Ctrl+X` — Undo `Ctrl+Z`
  - Redo `Ctrl+Y` — Select All `Ctrl+A`
- **Dynamic resize** — window bisa di-resize, key menyesuaikan
- **Always on top** — tidak mencuri fokus (`WS_EX_NOACTIVATE`)
- **Caps Lock sync** — membaca status Caps Lock sistem setiap 500ms

---

## Build

### MSVC (Visual Studio 2022)

```bat
build.bat
```

Atau manual dari **Developer Command Prompt**:

```bat
rc resource.rc
cl /EHsc /O2 /std:c++17 /W4 main.cpp resource.res /Fe:vkbd.exe user32.lib gdi32.lib advapi32.lib
```

### g++ (MinGW-w64)

```bash
windres resource.rc -o resource.o
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o vkbd.exe main.cpp resource.o \
    -luser32 -lgdi32 -ladvapi32 -municode
```

Flag `-municode` **wajib** karena entry point `wWinMain`.

---

## Struktur

```
D:\Codingan\C++\virtual keyboard\
├── main.cpp      # semua kode (single-file)
├── build.bat     # build script MSVC
├── vkbd.exe      # output binary
└── README.md
```

| File | Lines | Deskripsi |
|------|-------|-----------|
| `main.cpp` | ~900 | Entry point, window proc, layout, paint, input, shortcut panel |

---

## Tombol

| Baris | Tombol |
|-------|--------|
| 0 | Esc, 1–0, -, =, **Bcksp** |
| 1 | Tab, Q–P (dengan Z↔Y swap), [, ], \ |
| 2 | Caps, A–L, ;, ', Enter |
| 3 | Shift, Y–M, ,, ., /, Shift |
| 4 | Ctrl, Win, Alt, **Space**, Alt, Win, **Menu**, Ctrl |

**Menu** = toggle sidebar shortcut (highlight biru saat aktif)

---

## Shortcut panel (sidebar kanan)

```
┌──────────┬──────────┐
│   Copy   │  Paste   │
│  Ctrl+C  │  Ctrl+V  │
├──────────┼──────────┤
│   Cut    │  Undo    │
│  Ctrl+X  │  Ctrl+Z  │
├──────────┼──────────┤
│   Redo   │ Sel.All  │
│  Ctrl+Y  │  Ctrl+A  │
└──────────┴──────────┘
```

Window melebar otomatis saat panel dibuka (main keys tetap fixed).

---

## License

MIT
