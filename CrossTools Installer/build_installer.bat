@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "APP_DIR=%SCRIPT_DIR%..\CrossTools"
set "UNINST_DIR=%SCRIPT_DIR%..\CrossTools Uninstaller"
cd /d "%SCRIPT_DIR%"

rem ─────────────────────────────────────────────────────────────────────────
rem  Initialize MSVC environment if cl.exe is not already available.
rem ─────────────────────────────────────────────────────────────────────────
if defined VCToolsInstallDir goto tools_ready

set "VSDEV=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2019\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=%ProgramFiles%\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"

if exist "%VSDEV%" call "%VSDEV%" -arch=x64
:tools_ready

where cl.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: Microsoft C++ Build Tools not found.
    echo Install "Desktop development with C++" from the Visual Studio Installer.
    exit /b 1
)

rem ─────────────────────────────────────────────────────────────────────────
rem  Step 1: Check if CrossTools.exe needs rebuilding
rem ─────────────────────────────────────────────────────────────────────────
if not exist "%APP_DIR%\CrossTools.exe" goto build_app

powershell -NoProfile -Command "if ((Get-Item '%APP_DIR%\CrossTools.cpp').LastWriteTime -gt (Get-Item '%APP_DIR%\CrossTools.exe').LastWriteTime -or (Get-Item '%APP_DIR%\CrossTools.rc').LastWriteTime -gt (Get-Item '%APP_DIR%\CrossTools.exe').LastWriteTime) { exit 1 } else { exit 0 }"
if errorlevel 1 goto build_app

echo [1/3] CrossTools.exe is up to date (skipped compilation).
goto check_uninstaller

:build_app
echo [1/3] Building CrossTools.exe...
cd /d "%APP_DIR%"
rc.exe /nologo "CrossTools.rc"
if errorlevel 1 (
    echo ERROR: Failed to compile CrossTools.rc
    exit /b 1
)
cl.exe /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE ^
    "CrossTools.cpp" CrossTools.res ^
    user32.lib gdi32.lib comctl32.lib advapi32.lib ^
    /link /SUBSYSTEM:WINDOWS
if errorlevel 1 (
    echo ERROR: Failed to compile CrossTools.cpp
    exit /b 1
)
cd /d "%SCRIPT_DIR%"

:check_uninstaller
rem ─────────────────────────────────────────────────────────────────────────
rem  Step 2: Check if Uninstall CrossTools.exe needs rebuilding
rem ─────────────────────────────────────────────────────────────────────────
if not exist "%UNINST_DIR%\Uninstall CrossTools.exe" goto build_uninst

powershell -NoProfile -Command "if ((Get-Item '%UNINST_DIR%\Uninstaller.cpp').LastWriteTime -gt (Get-Item '%UNINST_DIR%\Uninstall CrossTools.exe').LastWriteTime) { exit 1 } else { exit 0 }"
if errorlevel 1 goto build_uninst

echo [2/3] Uninstall CrossTools.exe is up to date (skipped compilation).
goto package_installer

:build_uninst
echo [2/3] Building Uninstall CrossTools.exe...
cd /d "%UNINST_DIR%"
cl.exe /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE ^
    "Uninstaller.cpp" ^
    user32.lib shell32.lib advapi32.lib ^
    /link /SUBSYSTEM:WINDOWS /OUT:"Uninstall CrossTools.exe"
if errorlevel 1 (
    echo ERROR: Failed to compile Uninstaller.cpp
    exit /b 1
)
cd /d "%SCRIPT_DIR%"

:package_installer
rem ─────────────────────────────────────────────────────────────────────────
rem  Step 3: Package pre-compiled binaries into CrossTools Setup.exe
rem ─────────────────────────────────────────────────────────────────────────
echo [3/3] Packaging CrossTools Setup.exe...

copy /Y "%APP_DIR%\CrossTools.ico"                "%SCRIPT_DIR%CrossTools.ico" >nul
copy /Y "%APP_DIR%\CrossTools.exe"                "%SCRIPT_DIR%CrossTools.exe" >nul
copy /Y "%UNINST_DIR%\Uninstall CrossTools.exe"  "%SCRIPT_DIR%Uninstall CrossTools.exe" >nul

rc.exe /nologo "Installer.rc"
if errorlevel 1 (
    echo ERROR: rc.exe failed on Installer.rc
    goto cleanup
)

cl.exe /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE ^
    "Installer.cpp" "Installer.res" ^
    shell32.lib shlwapi.lib user32.lib gdi32.lib ole32.lib comctl32.lib advapi32.lib ^
    /link /SUBSYSTEM:WINDOWS /OUT:"CrossTools Setup.exe"
if errorlevel 1 (
    echo ERROR: cl.exe failed on Installer.cpp
    goto cleanup
)

echo.
echo Success! "%SCRIPT_DIR%CrossTools Setup.exe" is ready.

rem ─────────────────────────────────────────────────────────────────────────
rem  Cleanup temporary copies
rem ─────────────────────────────────────────────────────────────────────────
:cleanup
echo Cleaning temporary packaging files...
if exist "%SCRIPT_DIR%CrossTools.ico"            del /f /q "%SCRIPT_DIR%CrossTools.ico"
if exist "%SCRIPT_DIR%CrossTools.exe"            del /f /q "%SCRIPT_DIR%CrossTools.exe"
if exist "%SCRIPT_DIR%Uninstall CrossTools.exe"  del /f /q "%SCRIPT_DIR%Uninstall CrossTools.exe"
if exist "%SCRIPT_DIR%Installer.res"             del /f /q "%SCRIPT_DIR%Installer.res"
if exist "%SCRIPT_DIR%Installer.obj"             del /f /q "%SCRIPT_DIR%Installer.obj"

exit /b 0
