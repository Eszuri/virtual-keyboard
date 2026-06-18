# Virtual Keyboard (Windows)

Virtual keyboard Windows native — dibuat dengan Win32 API dan C++17. Tanpa dependency eksternal, hanya `user32.lib` + `gdi32.lib`.

![screenshot](https://img.shields.io/badge/build-passing-brightgreen) ![lang](https://img.shields.io/badge/C%2B%2B-17-blue) ![platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)

---

## Fitur

- **65 tombol** — QWERTY layout (baris 4: Ctrl, Win, Alt, Spasi, Alt, Ctrl, ←, ↑, ↓, →, Home, End)
- **Dual-label keys** — setiap tombol dengan dua label (huruf besar/kecil, simbol) menampilkan keduanya secara berpasangan; posisi bertukar saat Shift aktif
- **Sidebar permanen** — panel pintasan selalu tampil di kanan (10 tombol, 5 baris × 2 kolom)
- **Font size control** — tombol **Font + / Font −** di sidebar, ukuran tersimpan di `vkbd.cfg`
- **Dark theme** — ala Windows 11 on-screen keyboard
- **Sticky modifiers** — Shift, Ctrl, Alt, Win bisa di-latch (highlight biru hanya di tombol spesifik kiri/kanan)
- **Hold-to-repeat** — tahan Backspace/Space/huruf untuk auto-repeat (400ms delay awal, 50ms repeat)
- **Dynamic resize** — window bisa di-resize, key menyesuaikan
- **Always on top** — tidak mencuri fokus (`WS_EX_NOACTIVATE`)
- **Caps Lock sync** — membaca status Caps Lock sistem setiap 500ms
- **Config persistence** — `vkbd.cfg` menyimpan hotkey, ukuran window, dan ukuran font

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
├── main.cpp      # semua kode (single-file, ~1365 baris)
├── build.bat     # build script MSVC
├── app.manifest  # DPI manifest (PerMonitorV2)
├── resource.rc   # icon + manifest resource
├── vkbd.cfg      # konfigurasi runtime (git-ignored)
├── vkbd.exe      # output binary
├── AGENTS.md     # panduan untuk agent AI
└── README.md
```

---

## Tombol

| Baris | Tombol |
|-------|--------|
| 0 | Esc, 1–0, -, =, **Bcksp** |
| 1 | Tab, Q–P, [, ], \ |
| 2 | Caps, A–L, ;, ', Enter |
| 3 | Shift, Z–M, ,, ., /, Shift |
| 4 | Ctrl, Win, Alt, **Space**, Alt, Ctrl, **←**, **↑**, **↓**, **→**, **Home**, **End** |

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
├──────────┼──────────┤
│   Hide   │AutoStart │
│ Ctrl+Sh+K│ ON / OFF │
├──────────┼──────────┤
│  Font +  │  Font −  │
│   20pt   │   20pt   │
└──────────┴──────────┘
```

Sidebar selalu tampil di sisi kanan. **Hide** menyembunyikan window, **AutoStart** mendaftarkan ke registry Run key. **Font + / −** mengubah ukuran huruf (8–40pt) dan tersimpan otomatis.

---

## License

MIT
