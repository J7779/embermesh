<#
EmberMesh — Windows build entry point (MSVC). Mirrors the core / guides / desktop / test targets of
build.sh, which needs bash and a single-configuration generator; everything else (web, Android) is
built with build.sh (Git Bash or WSL) or on another OS.

  .\build.ps1 <target>... [-Config Release|Debug] [-Jobs N] [-NoGuides] [-Only native,privacy]

Targets
  core      build the shared C++ core + host tools + host library + emberd (build\host)
  guides    pack guides\content into build\guides.pack
  desktop   core + guides, staged into dist\desktop\ (emberd.exe, guides.pack, deploy\)
  test      ctest over the native build (core doctest, host and emberd suites); with -Only
            privacy also runs tools\check-publishable.sh through bash (Git for Windows)
  clean     remove build\ and dist\

Environment: EMBER_CMAKE (cmake to use), EMBER_TEST_RESULTS=DIR (test: also write ctest JUnit
XML there). Uses CMake's default generator (Visual Studio) with -A x64 unless CMAKE_GENERATOR is set.

  .\build.ps1 desktop test        what the Desktop workflow runs on Windows
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0, ValueFromRemainingArguments = $true)] [string[]] $Targets,
  [ValidateSet('Release', 'Debug')] [string] $Config = 'Release',
  [int] $Jobs = [Environment]::ProcessorCount,
  [switch] $NoGuides,
  [string] $Only = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3

$Root = $PSScriptRoot
Set-Location $Root
$HostBuild = Join-Path $Root ($(if ($Config -eq 'Debug') { 'build\host-debug' } else { 'build\host' }))
$GuidesPack = Join-Path $Root 'build\guides.pack'
$Cmake = if ($env:EMBER_CMAKE) { $env:EMBER_CMAKE } else { 'cmake' }
$OnWindows = ($env:OS -eq 'Windows_NT')
$Ctest = Join-Path (Split-Path (Get-Command $Cmake).Source) ($(if ($OnWindows) { 'ctest.exe' } else { 'ctest' }))
if (-not (Test-Path $Ctest)) { $Ctest = 'ctest' }

function Step([string] $m) { Write-Host "==> $m" -ForegroundColor White }
function Ok([string] $m) { Write-Host "ok  $m" -ForegroundColor Green }
function Die([string] $m) { Write-Host "error: $m" -ForegroundColor Red; exit 1 }
function Run([string] $exe, [string[]] $argv) {
  & $exe @argv
  if ($LASTEXITCODE -ne 0) { Die "$exe $($argv -join ' ') failed with exit code $LASTEXITCODE" }
}
function Want([string] $suite) { return ($Only -eq '') -or ((',' + $Only + ',') -like "*,$suite,*") }

$script:Configured = $false
function Configure-Host {
  if ($script:Configured) { return }
  Step "Configuring native build ($Config) in $($HostBuild.Substring($Root.Length + 1))"
  $gen = @()
  if ($OnWindows -and -not $env:CMAKE_GENERATOR -and -not (Test-Path (Join-Path $HostBuild 'CMakeCache.txt'))) { $gen = @('-A', 'x64') }
  Run $Cmake (@('-S', $Root, '-B', $HostBuild) + $gen + @("-DCMAKE_BUILD_TYPE=$Config", '-DEMBER_SANITIZE=OFF',
    '-DEMBER_BUILD_TESTS=ON', '-DEMBER_BUILD_TOOLS=ON', '-DEMBER_BUILD_DESKTOP=ON'))
  $script:Configured = $true
}

$script:DidCore = $false
function Do-Core {
  if ($script:DidCore) { return }
  Configure-Host
  Step 'Building core, tools, host library and desktop daemon'
  Run $Cmake @('--build', $HostBuild, '--config', $Config, '--parallel', "$Jobs")
  $script:DidCore = $true
  Ok "native build: $HostBuild"
}

$script:DidGuides = $false
function Do-Guides {
  if ($script:DidGuides -or $NoGuides) { return }
  Configure-Host
  Step 'Packing the offline guide library'
  Run $Cmake @('--build', $HostBuild, '--config', $Config, '--target', 'guides_pack', '--parallel', "$Jobs")
  $built = Join-Path $HostBuild 'guides\guides.pack'
  if (-not (Test-Path $built)) {
    $found = Get-ChildItem -Path $HostBuild -Recurse -Filter guides.pack -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $found) { Die 'guides_pack target did not produce guides.pack' }
    $built = $found.FullName
  }
  Copy-Item $built $GuidesPack -Force
  $script:DidGuides = $true
  Ok ("guides.pack: {0:N1} MiB" -f ((Get-Item $GuidesPack).Length / 1MB))
}

function Do-Desktop {
  Do-Core
  Do-Guides
  $exe = if ($OnWindows) { 'emberd.exe' } else { 'emberd' }
  $all = @(Get-ChildItem -Path $HostBuild -Recurse -Filter $exe -File | Where-Object { $_.Name -eq $exe })
  $bin = $all | Where-Object { $_.DirectoryName -like "*$([IO.Path]::DirectorySeparatorChar)$Config" } | Select-Object -First 1
  if (-not $bin) { $bin = $all | Select-Object -First 1 }
  if (-not $bin) { Die "$exe was not built" }
  Step 'Staging dist\desktop'
  $dist = Join-Path $Root 'dist\desktop'
  New-Item -ItemType Directory -Force -Path $dist | Out-Null
  Copy-Item $bin.FullName $dist -Force
  if ((-not $NoGuides) -and (Test-Path $GuidesPack)) { Copy-Item $GuidesPack (Join-Path $dist 'guides.pack') -Force }
  $deploy = Join-Path $Root 'apps\desktop\deploy'
  if (Test-Path $deploy) {
    Remove-Item -Recurse -Force (Join-Path $dist 'deploy') -ErrorAction SilentlyContinue
    Copy-Item -Recurse $deploy (Join-Path $dist 'deploy')
  }
  Ok "dist\desktop\$exe"
}

function Do-Test {
  $failed = @()
  if (Want 'native') {
    Do-Core
    $argv = @('--test-dir', $HostBuild, '-C', $Config, '--output-on-failure', '-j', "$Jobs")
    if ($env:EMBER_TEST_RESULTS) {
      New-Item -ItemType Directory -Force -Path $env:EMBER_TEST_RESULTS | Out-Null
      $argv += @('--output-junit', (Join-Path (Resolve-Path $env:EMBER_TEST_RESULTS) 'ctest.xml'))
    }
    Step 'Tests: core + host (ctest)'
    & $Ctest @argv
    if ($LASTEXITCODE -ne 0) { $failed += 'core + host (ctest)' } else { Ok 'core + host (ctest)' }
  }
  if ($Only -ne '' -and (Want 'privacy')) {
    Step 'Tests: privacy (publishable files)'
    & bash (Join-Path $Root 'tools/check-publishable.sh')
    if ($LASTEXITCODE -ne 0) { $failed += 'privacy' } else { Ok 'privacy' }
  }
  if ($failed.Count -gt 0) { Die ("FAILED: " + ($failed -join ', ')) }
}

if (-not $Targets -or $Targets.Count -eq 0) { Get-Help $PSCommandPath; exit 2 }
foreach ($t in $Targets) {
  switch ($t) {
    'core'    { Do-Core }
    'guides'  { Do-Guides }
    'desktop' { Do-Desktop }
    'test'    { Do-Test }
    'clean'   { Step 'Removing build\ and dist\'; Remove-Item -Recurse -Force (Join-Path $Root 'build'), (Join-Path $Root 'dist') -ErrorAction SilentlyContinue; Ok 'clean' }
    default   { Die "unknown target: $t (core, guides, desktop, test, clean)" }
  }
}
