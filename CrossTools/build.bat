@echo off
setlocal EnableExtensions
cd /d "%~dp0"
rem Builds a standalone CrossTools.exe with Visual Studio's C++ tools (2017 or later, workload "Desktop development with C++").
rem Run it from anywhere, even by double-clicking: if this is not a Developer Command Prompt, the Visual Studio build
rem environment is found and loaded first. The compiler output is also saved in build.log.
rem The icon is added when CrossTools.ico sits beside this file; without it the exe is built without the icon.

set "PAUSE_AT_END="
echo %cmdcmdline% | find /i "%~nx0" >nul && set "PAUSE_AT_END=1"

where cl >nul 2>nul && goto :havecl
call :loadvs
where cl >nul 2>nul && goto :havecl
echo ERROR: Visual Studio's C++ build tools were not found.
echo Install "Desktop development with C++" (Visual Studio or Build Tools for Visual Studio, 2017 or later),
echo or run this file from a "Developer Command Prompt".
goto :fail

:havecl
if not exist CrossTools.ico if exist CrossTools_ico.txt copy /y CrossTools_ico.txt CrossTools.ico >nul
set "RES="
if exist CrossTools.ico rc /nologo CrossTools.rc && set "RES=CrossTools.res"
if not defined RES echo NOTE: building without the icon (CrossTools.ico not found or not valid).

cl /nologo /O2 /MT /EHsc /DUNICODE /D_UNICODE CrossTools.cpp %RES% user32.lib gdi32.lib comctl32.lib advapi32.lib /link /SUBSYSTEM:WINDOWS > build.log 2>&1
set "BUILD_ERR=%errorlevel%"
type build.log
if not "%BUILD_ERR%"=="0" goto :fail
echo.
echo Built CrossTools.exe
if defined PAUSE_AT_END pause
exit /b 0

:fail
echo.
echo BUILD FAILED. The messages above are also in build.log - send that file along if you need help.
if defined PAUSE_AT_END pause
exit /b 1

:loadvs
rem Finds the newest Visual Studio with the C++ tools (vswhere), else looks in the usual folders, then loads its x64 environment.
set "VSDIR="
set "VSW=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSW%" for /f "usebackq delims=" %%i in (`"%VSW%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if defined VSDIR if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" goto :loadvs_call
set "VSDIR="
for %%Y in (2022 2019 2017) do for %%E in (Enterprise Professional Community BuildTools Preview) do for %%P in ("%ProgramFiles%" "%ProgramFiles(x86)%") do if not defined VSDIR if exist "%%~P\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VSDIR=%%~P\Microsoft Visual Studio\%%Y\%%E"
if not defined VSDIR exit /b 1
:loadvs_call
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b 0
