@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul
cd /d "D:\Codingan\C++\virtual keyboard"
rc resource.rc
cl /EHsc /O2 /std:c++17 /W4 main.cpp resource.res /Fe:vkbd.exe user32.lib gdi32.lib 2>&1
if %ERRORLEVEL% EQU 0 (
    echo === Build sukses: vkbd.exe ===
)
