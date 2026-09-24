@echo off
rem Copyright (c) 2025 Nations Technologies Inc.
rem SPDX-License-Identifier: Apache-2.0

rem ===========================================================================
rem Flash an already-built image with no make / sh / Git dependency.
rem
rem Usage:
rem   flash.cmd                  Flash build\n32h787_kws_demo.bin
rem   flash.cmd path\to.bin      Flash another raw binary
rem
rem Override environment:
rem   set OPENOCD=D:\path\to\openocd.exe
rem Requires only N32Studio (OpenOCD + CMSIS-DAP/NSLink driver) and a board
rem connected to DEBUG USB.
rem ===========================================================================

setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"

if not defined OPENOCD set "OPENOCD=%USERPROFILE%\.n32studio\Toolchain\openocd\bin\openocd.exe"
if not exist "%OPENOCD%" (
    echo [flash] ERROR: OpenOCD not found: "%OPENOCD%"
    echo [flash] Install N32Studio first, or set OPENOCD to a full openocd.exe path.
    endlocal
    exit /b 1
)

set "BIN=%~1"
if "%BIN%"=="" set "BIN=%SCRIPT_DIR%build\n32h787_kws_demo.bin"
for %%I in ("%BIN%") do set "BIN_ABS=%%~fI"
if not exist "%BIN_ABS%" (
    echo [flash] ERROR: image not found: "%BIN_ABS%"
    echo [flash] Build it first with build.cmd, or pass a .bin path explicitly.
    endlocal
    exit /b 1
)

for %%I in ("%SCRIPT_DIR%..\openocd\n32h7x_cmsisdap.tcl") do set "TCL_ABS=%%~fI"
rem Forward slashes: a backslash Windows path inside the Tcl command line is
rem mangled by Tcl escape parsing (\f \n ...), which breaks [info script] in
rem the config and its relative "flashos" source.
set "TCL_FWD=%TCL_ABS:\=/%"
set "BIN_FWD=%BIN_ABS:\=/%"

echo [flash] Programming "%BIN_ABS%" @ 0x15000000 via CMSIS-DAP ...
"%OPENOCD%" -d1 -c "gdb_port disabled" -f "%TCL_FWD%" -c "adapter speed 1000; n32h7x_flash {%BIN_FWD%} 6000 0x15000000; shutdown"
set "RC=%ERRORLEVEL%"
if "%RC%"=="0" echo [flash] Done.
endlocal & exit /b %RC%
