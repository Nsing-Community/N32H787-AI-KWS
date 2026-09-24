# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

param(
    [string]$InfPath = "C:\Users\tobby\Desktop\nslink-cmsis-dap-v2.inf",
    [string]$LogPath = "C:\Users\tobby\Desktop\nslink-winusb-install.log"
)

$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath,
        "-InfPath", $InfPath, "-LogPath", $LogPath
    )
    $elevated = Start-Process -FilePath powershell.exe -Verb RunAs -Wait -PassThru `
        -ArgumentList $arguments
    exit $elevated.ExitCode
}

$log = @()
$log += "Installing $InfPath"
$log += (pnputil /add-driver $InfPath /install /force 2>&1)
$addExit = $LASTEXITCODE
$log += "pnputil exit=$addExit"

$identity = "USB\VID_19F5&PID_3106&MI_02"
$device = Get-PnpDevice -PresentOnly |
    Where-Object { $_.InstanceId -like "$identity*" } |
    Select-Object -First 1
if ($null -ne $device) {
    $log += "Rescanning $($device.InstanceId)"
    pnputil /remove-device $device.InstanceId 2>&1 | ForEach-Object { $log += $_ }
    Start-Sleep -Milliseconds 750
    pnputil /scan-devices 2>&1 | ForEach-Object { $log += $_ }
    Start-Sleep -Seconds 2
    $result = Get-PnpDevice -PresentOnly |
        Where-Object { $_.InstanceId -like "$identity*" } |
        Select-Object Status, Class, FriendlyName, Problem, InstanceId
    $log += ($result | Format-List | Out-String)
}

$log | Set-Content -LiteralPath $LogPath -Encoding ascii
if ($addExit -ne 0) {
    exit $addExit
}
