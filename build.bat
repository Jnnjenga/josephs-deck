@echo off
setlocal
set GPP=C:\msys64\ucrt64\bin\g++.exe
if not exist "%GPP%" (
    echo ERROR: g++ not found at %GPP%
    pause & exit /b 1
)
echo Building JosephsDeck...
"%GPP%" -std=c++17 -mwindows -O2 ^
    main.cpp LauncherWindow.cpp ShortcutManager.cpp ActionExecutor.cpp ^
    EditDialog.cpp SettingsDialog.cpp ^
    -ldwmapi -luxtheme -lshell32 -luser32 -lgdi32 -lcomctl32 ^
    -ladvapi32 -lcomdlg32 -lole32 ^
    -o JosephsDeck.exe
if %ERRORLEVEL% EQU 0 (
    echo BUILD SUCCESS
    copy /Y "C:\msys64\ucrt64\bin\libgcc_s_seh-1.dll" . >nul 2>&1
    copy /Y "C:\msys64\ucrt64\bin\libstdc++-6.dll"    . >nul 2>&1
    echo Done.
) else (
    echo BUILD FAILED
)
pause
