@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

rem Build CrypTools.exe from a normal PowerShell or Command Prompt.
rem If cl.exe is not already available, initialize the latest Visual Studio
rem installation that contains the C++ build tools.
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
    echo ERROR: Microsoft C++ Build Tools are not installed or could not be initialized.
    echo Install "Desktop development with C++" from Visual Studio Installer, then run this file again.
    exit /b 1
)

if not exist "CrypTools.rc" (
    echo ERROR: CrypTools.rc is missing; the executable cannot include its icon.
    exit /b 1
)
if not exist "CrypTools.ico" (
    echo ERROR: CrypTools.ico is missing; the executable cannot include its icon.
    exit /b 1
)

rc.exe /nologo "CrypTools.rc"
if errorlevel 1 (
    echo ERROR: Could not compile CrypTools.rc.
    exit /b 1
)

cl.exe /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE ^
    "CrypTools.cpp" CrypTools.res ^
    user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib ole32.lib ^
    /link /SUBSYSTEM:WINDOWS
if errorlevel 1 exit /b %errorlevel%

echo Built "%SCRIPT_DIR%CrypTools.exe"
exit /b 0
