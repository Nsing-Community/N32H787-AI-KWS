@echo off
:: Copyright (c) 2025 Nations Technologies Inc.
:: SPDX-License-Identifier: Apache-2.0

rem ===========================================================================
rem N32H787 KWS demo one-click build entry for Windows.
rem All path changes are local to this process; no system/global PATH edits.
rem
rem Usage:
rem   build.cmd                 Build n32h787_kws_demo.elf/.hex/.bin
rem   build.cmd release=y       Release build without debug info
rem   build.cmd clean           Remove the build directory
rem   build.cmd flash           Build and flash the image at 0x15000000
rem                             via CMSIS-DAP (standalone, power-on direct run)
rem
rem Shell note:
rem   Make recipes need a POSIX sh plus a few coreutils (sh/rm/mkdir/awk).
rem   Git itself is NOT used by the build and does NOT have to be installed:
rem   a BusyBox binary is shipped under tools\host and a shim is created
rem   automatically on first run.  An explicitly installed Git for Windows
rem   (or any sh.exe on PATH / SH_DIR) also works and takes precedence only
rem   when the shim is unavailable.  Set SH_DIR to override shell location.
rem ===========================================================================

setlocal EnableExtensions

rem --- Repository layout (this script lives in firmware\Makefile)
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

rem --- Python 3 (user-scope winget install; needed by tools/embed_tflite.py)
if not defined PYTHON_DIR if exist "%LOCALAPPDATA%\Programs\Python\Python312" set "PYTHON_DIR=%LOCALAPPDATA%\Programs\Python\Python312"

rem --- N32Studio toolchain (absolute GCC_PATH avoids the native-make PATH
rem     truncation that otherwise loses the compiler inside sh recipes)
if not defined GCC_PATH set "GCC_PATH=%USERPROFILE:\=/%/.n32studio/Toolchain/gcc-arm-none-eabi/bin/"
if not defined PYTHON set "PYTHON=python"
if not defined OPENOCD set "OPENOCD=%USERPROFILE:\=/%/.n32studio/Toolchain/openocd/bin/openocd.exe"

rem ---------------------------------------------------------------------------
rem Locate a POSIX sh (sh.exe) with companion coreutils.
rem Search order:
rem   1. SH_DIR  - directory containing sh.exe, or a Git-for-Windows root
rem      (GIT_DIR is accepted as a legacy alias for SH_DIR)
rem   2. Bundled BusyBox shim at tools\host\.shim (auto-created, fully offline)
rem   3. sh.exe already found on PATH
rem   4. Common Git for Windows install locations
rem ---------------------------------------------------------------------------
set "SH_BIN="
if defined SH_DIR  call :resolve_sh "%SH_DIR%"
if not defined SH_BIN if defined GIT_DIR call :resolve_sh "%GIT_DIR%"
if not defined SH_BIN call :ensure_busybox_shim
if not defined SH_BIN (
    for %%I in (sh.exe) do if not "%%~$PATH:I"=="" set "SH_BIN=%%~$PATH:I"
)
if not defined SH_BIN (
    for %%P in (
        "C:\Program Files\Git"
        "C:\Program Files (x86)\Git"
        "%LOCALAPPDATA%\Programs\Git"
        "D:\Program Files\Git"
        "D:\Program\Git"
        "D:\Program\git\Git"
    ) do call :resolve_sh %%P
)
if not defined SH_BIN goto :no_shell

rem Locate the REAL GNU make before the shim is added to PATH: the BusyBox
rem shim ships its own "make" applet, which would shadow GNU make and fail
rem to parse this Makefile ("expected separator").
if not defined MAKE_BIN for %%I in (make.exe) do if not "%%~$PATH:I"=="" set "MAKE_BIN=%%~$PATH:I"
if not defined MAKE_BIN if exist "%USERPROFILE%\.n32studio\Toolchain\make\make.exe" set "MAKE_BIN=%USERPROFILE%\.n32studio\Toolchain\make\make.exe"
if not defined MAKE_BIN (
    echo [build] ERROR: GNU make not found on PATH or in the N32Studio toolchain.
    echo [build] Install N32Studio, or add its make.exe directory to PATH.
    endlocal
    exit /b 1
)
set "MAKE_HOME="
if defined MAKE_BIN for %%I in ("%MAKE_BIN%") do set "MAKE_HOME=%%~dpI"
if defined MAKE_HOME set "MAKE_HOME=%MAKE_HOME:~0,-1%"

rem Put GNU make first, then the shell's companion utilities
rem (rm/mkdir/awk/...), then Python.
rem Git layout: <root>\bin\sh.exe + <root>\usr\bin\<applets>
rem shim layout: <dir>\sh.exe      + <dir>\<applets>
for %%I in ("%SH_BIN%") do set "SH_HOME=%%~dpI"
set "SH_HOME=%SH_HOME:~0,-1%"
set "SHELL_PATH="
rem shim layout: applets sit right next to sh.exe.
if exist "%SH_HOME%\mkdir.exe" set "SHELL_PATH=%SH_HOME%"
rem Git-for-Windows layout: sh.exe is in <root>\bin, applets in <root>\usr\bin.
if exist "%SH_HOME%\..\usr\bin\mkdir.exe" set "SHELL_PATH=%SH_HOME%;%SH_HOME%\..\usr\bin"
if defined MAKE_HOME set "PATH=%MAKE_HOME%;%SHELL_PATH%;%PATH%"
if not defined MAKE_HOME set "PATH=%SHELL_PATH%;%PATH%"
if defined PYTHON_DIR set "PATH=%PYTHON_DIR%;%PYTHON_DIR%\Scripts;%PATH%"

set "SH_BIN_FWD=%SH_BIN:\=/%"

if "%~1"=="" (
    make SH_BIN="%SH_BIN_FWD%"
) else (
    make SH_BIN="%SH_BIN_FWD%" %*
)
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%

rem ---------------------------------------------------------------------------
rem :resolve_sh <candidate>
rem   Accept either a directory that directly contains sh.exe (shim layout)
rem   or a Git-for-Windows install root whose bin\sh.exe exists.
rem ---------------------------------------------------------------------------
:resolve_sh
if defined SH_BIN exit /b 0
if exist "%~1\bin\sh.exe" set "SH_BIN=%~1\bin\sh.exe"
if exist "%~1\sh.exe"     set "SH_BIN=%~1\sh.exe"
exit /b 0

rem ---------------------------------------------------------------------------
rem :ensure_busybox_shim
rem   First-run setup: copy the shipped BusyBox into tools\host\.shim and
rem   hard-link every applet (sh/rm/mkdir/awk/...) next to it.  Copying first
rem   is required because BusyBox --install uses hard links, which cannot
rem   cross drive boundaries.
rem ---------------------------------------------------------------------------
:ensure_busybox_shim
if defined SH_BIN exit /b 0
set "BUSYBOX_EXE=%REPO_ROOT%\tools\host\busybox64.exe"
set "BUSYBOX_SHIM=%REPO_ROOT%\tools\host\.shim"
if not exist "%BUSYBOX_EXE%" exit /b 0
if not exist "%BUSYBOX_SHIM%\sh.exe" (
    echo [build] First run: creating the POSIX shell shim in tools\host\.shim
    if not exist "%BUSYBOX_SHIM%" mkdir "%BUSYBOX_SHIM%"
    copy /y "%BUSYBOX_EXE%" "%BUSYBOX_SHIM%\busybox.exe" >nul
    if errorlevel 1 (
        echo [build] WARNING: could not stage BusyBox into "%BUSYBOX_SHIM%".
        exit /b 0
    )
    "%BUSYBOX_SHIM%\busybox.exe" --install "%BUSYBOX_SHIM%" >nul 2>&1
)
if exist "%BUSYBOX_SHIM%\sh.exe" set "SH_BIN=%BUSYBOX_SHIM%\sh.exe"
exit /b 0

:no_shell
echo.
echo [build] ERROR: POSIX shell not found ^(sh.exe plus rm/mkdir/awk^).
echo.
echo The build does NOT need Git.  Normally the BusyBox binary shipped with
echo this repository ^(tools\host\busybox64.exe^) is used automatically -
echo please make sure that file exists in the unpacked project.
echo Alternatives:
echo   - install Git for Windows ^(provides sh.exe^):
echo       https://git-scm.com/download/win
echo   - or set SH_DIR to a folder that contains sh.exe and rerun build.cmd
echo.
endlocal
exit /b 1
