<#
.SYNOPSIS
  Registers a Task Scheduler task that starts emberd (EmberMesh hub) automatically. UNTESTED.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File install-task.ps1 -EmberdPath C:\EmberMesh\emberd.exe
  powershell -ExecutionPolicy Bypass -File install-task.ps1 -EmberdPath C:\EmberMesh\emberd.exe -AtStartup -DataDir C:\EmberMesh\data
  powershell -ExecutionPolicy Bypass -File install-task.ps1 -Remove

.NOTES
  -AtStartup runs the task as SYSTEM at boot (requires an elevated prompt); otherwise it runs as the
  current user at logon. The task restarts emberd up to 999 times, one minute apart, if it fails.
#>
param(
  [string]$EmberdPath = (Join-Path $PSScriptRoot 'emberd.exe'),
  [string]$DataDir = '',
  [string]$WebRoot = '',
  [string]$ExtraArgs = '',
  [switch]$AtStartup,
  [switch]$Remove,
  [string]$TaskName = 'EmberMesh emberd'
)
$ErrorActionPreference = 'Stop'

if ($Remove) {
  Write-Host "==> removing task '$TaskName'"
  Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction SilentlyContinue
  return
}

if (-not (Test-Path $EmberdPath)) { throw "emberd.exe not found at $EmberdPath" }
$EmberdPath = (Resolve-Path $EmberdPath).Path

$argList = @()
if ($DataDir) { $argList += "--data-dir `"$DataDir`"" }
if ($WebRoot) { $argList += "--web-root `"$WebRoot`"" }
elseif (Test-Path (Join-Path (Split-Path $EmberdPath) 'ui')) {
  $argList += "--web-root `"$(Join-Path (Split-Path $EmberdPath) 'ui')`""
}
if ($ExtraArgs) { $argList += $ExtraArgs }
$arguments = $argList -join ' '

$action = New-ScheduledTaskAction -Execute $EmberdPath -Argument $arguments -WorkingDirectory (Split-Path $EmberdPath)
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
  -StartWhenAvailable -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1) `
  -ExecutionTimeLimit ([TimeSpan]::Zero)

if ($AtStartup) {
  $trigger = New-ScheduledTaskTrigger -AtStartup
  $principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
} else {
  $trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
  $principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive
}

Write-Host "==> registering task '$TaskName': $EmberdPath $arguments"
Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger -Settings $settings `
  -Principal $principal -Force | Out-Null
Write-Host "==> starting task"
Start-ScheduledTask -TaskName $TaskName

Write-Host "Done. Guests: http://<this-pc-ip>:8437/   Operator token: `"$EmberdPath`" --print-token"
Write-Host "Remember the firewall rules (see README.md): TCP 8437 and UDP 47474 inbound."
