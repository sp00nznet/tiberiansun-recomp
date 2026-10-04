# One-click setup for the Tiberian Sun static recompilation.
# Run it by double-clicking Setup.cmd in the repo folder; the README's "Step by
# step" is the same thing by hand, command for command.
#
# It copies your install into game\, analyses Game.exe, builds the function
# catalog, lifts it to C, builds the 32-bit host and leaves a shortcut. Each
# step is skipped when its output already exists (-Force redoes them).
# Everything it does goes to setup.log.
param([switch]$Force, [string]$Game = "")

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Toolkit = Join-Path (Split-Path -Parent $Root) 'tools'
$T = Join-Path $Toolkit 'tools'
$Log = Join-Path $Root 'setup.log'
Set-Location $Root
# A log line that cannot be written (the log open in another program) is
# not a reason to stop the setup.
function Log($t) { try { Add-Content -Path $Log -Value $t -Encoding UTF8 -ErrorAction Stop } catch {} }
Log "==== setup $(Get-Date -Format s)"

function Say($t, $c = 'Gray') { Write-Host $t -ForegroundColor $c; Log $t }
function Step($n, $t) { Write-Host ""; Say "[$n/7] $t" 'Cyan' }
function Fail($t) {
  Say "" ; Say "Setup stopped: $t" 'Red'
  Say "The details are in $Log. Fix that and run Setup.cmd again; finished steps are skipped." 'Yellow'
  Read-Host "Press Enter to close" | Out-Null; exit 1
}
function Ask($q) { $a = Read-Host "$q [Y/n]"; return -not ($a -match '^[nN]') }   # Enter = yes
function Refresh-Path {
  $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}
# Run a program with its output in the log. Windows PowerShell turns a native
# program's stderr into errors, so 'Stop' is off while it runs.
function Exec([string[]]$cmd) {
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  $rest = @($cmd | Select-Object -Skip 1)   # @(): a one-element slice would splat as characters
  & $cmd[0] @rest 2>&1 | ForEach-Object { Log "$_" }
  $code = $LASTEXITCODE
  $ErrorActionPreference = $old
  return $code
}
function Run($what, [string[]]$cmd) {
  Say "  $what..."
  $code = Exec $cmd
  if ($code -ne 0) { Fail "$what failed (exit code $code)." }
}

Clear-Host
Say "Tiberian Sun - static recompilation setup" 'White'
Say "You need your installed copy of Tiberian Sun (the Steam build, with Firestorm) and about 6 GB free."
Say "The function catalog, the lift and the build take 30 to 60 minutes, once."

# ---------------------------------------------------------------- tools
Step 1 "Checking the tools the pipeline needs"
# A Python that answers "Python 3.x". The Store's placeholder "python" (the
# one that opens the Store) answers nothing, so asking is the reliable test.
function Find-Python {
  foreach ($c in @(@('py', '-3'), @('python'), @('python3'))) {
    if (-not (Get-Command $c[0] -ErrorAction SilentlyContinue)) { continue }
    $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $rest = @($c | Select-Object -Skip 1)
    $v = (& $c[0] @rest --version 2>&1 | Out-String).Trim()
    $ErrorActionPreference = $old
    if ($v -match '^Python 3\.(\d+)' -and [int]$Matches[1] -ge 10) { return ,$c }
  }
  return $null
}
$pyargs = Find-Python
if (-not $pyargs) {
  Say "  Python 3.10 or newer is not installed."
  if (-not (Ask "  Install Python 3.12 now (winget, for your user only, about 30 MB)?")) { Fail "Python 3 is required." }
  if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Fail "winget is missing. Install 'App Installer' from the Microsoft Store, or install Python yourself."
  }
  Exec @('winget', 'install', '-e', '--id', 'Python.Python.3.12', '--scope', 'user', '--accept-package-agreements', '--accept-source-agreements') | Out-Null
  Refresh-Path
  $pyargs = Find-Python
  if (-not $pyargs) { Fail "Python installed, but Windows has not picked it up yet: close this window and run Setup.cmd again." }
}
Say "  Python: $($pyargs -join ' ')"

if ((Exec ($pyargs + @('-c', 'import pefile, capstone'))) -ne 0) {
  Say "  The Python packages pefile and capstone are missing (about 15 MB)."
  if (-not (Ask "  Install them now (pip, for your user only)?")) { Fail "pefile and capstone are required." }
  Run "Installing pefile and capstone" ($pyargs + @('-m', 'pip', 'install', '--user', 'pefile', 'capstone'))
}

if (-not (Test-Path (Join-Path $T 'lift\lift32.py'))) {
  Say "  The pcrecomp toolkit is not beside this folder ($Toolkit)."
  if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail "git is needed to fetch pcrecomp. Install Git for Windows, then run Setup.cmd again." }
  if (-not (Ask "  Download it now (git, about 20 MB)?")) { Fail "pcrecomp is required at $Toolkit." }
  Run "Cloning pcrecomp" @('git', 'clone', '--depth', '1', 'https://github.com/sp00nznet/pcrecomp', $Toolkit)
}
if (-not (Test-Path (Join-Path $Toolkit 'runtime\native32\native32.c'))) {
  Fail "your pcrecomp at $Toolkit predates runtime\native32. Update it: git -C `"$Toolkit`" pull"
}
Say "  pcrecomp: $Toolkit"

# The compiler. build.cmd finds Visual Studio itself; this only checks that it
# is there, with CMake and Ninja. Visual Studio is several GB with an installer
# of its own, so this says what to install rather than installing it.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = if (Test-Path $vswhere) { (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) } else { $null }
if (-not $vs) { Fail "Visual Studio 2022 (or its Build Tools) with 'Desktop development with C++' is needed. Install it, then run Setup.cmd again." }
Say "  Visual Studio: $vs"
# $tool, not $t: PowerShell names are case-insensitive, and $T is the toolkit path.
foreach ($tool in @(@('cmake', 'Kitware.CMake', '30 MB'), @('ninja', 'Ninja-build.Ninja', '1 MB'))) {
  if (Get-Command $tool[0] -ErrorAction SilentlyContinue) { continue }
  Say "  $($tool[0]) is not installed."
  if (-not (Ask "  Install $($tool[0]) now (winget, about $($tool[2]))?")) { Fail "$($tool[0]) is required." }
  Exec @('winget', 'install', '-e', '--id', $tool[1], '--accept-package-agreements', '--accept-source-agreements') | Out-Null
  Refresh-Path
  if (-not (Get-Command $tool[0] -ErrorAction SilentlyContinue)) { Fail "$($tool[0]) installed, but Windows has not picked it up yet: close this window and run Setup.cmd again." }
}

# ---------------------------------------------------------------- the game
Step 2 "Copying your copy of Tiberian Sun into game\"
function Is-Install([string]$d) { return ($d -and (Test-Path (Join-Path $d 'Game.exe'))) }
function Find-Steam {
  $roots = @()
  foreach ($k in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam') {
    try { $roots += (Get-ItemProperty $k -ErrorAction Stop).SteamPath } catch {}
    try { $roots += (Get-ItemProperty $k -ErrorAction Stop).InstallPath } catch {}
  }
  foreach ($r in ($roots | Where-Object { $_ } | Select-Object -Unique)) {
    $libs = @($r)
    $vdf = Join-Path $r 'steamapps\libraryfolders.vdf'
    if (Test-Path $vdf) {
      $libs += (Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"' | ForEach-Object { $_.Matches[0].Groups[1].Value -replace '\\\\', '\' })
    }
    foreach ($l in $libs) {
      $d = Join-Path $l 'steamapps\common\Command & Conquer Tiberian Sun'
      if (Is-Install $d) { return $d }
    }
  }
  return ""
}

if ((Test-Path 'game\Game.exe') -and -not $Force) {
  Say "  Already in game\ (skipping)."
} else {
  $Game = $Game.Trim('"', ' ')
  if (-not (Is-Install $Game)) {
    $Game = Find-Steam
    if ($Game) { Say "  Found it in your Steam library: $Game" }
  }
  while (-not (Is-Install $Game)) {
    $Game = (Read-Host "  Paste the folder Tiberian Sun is installed in (the one with Game.exe)").Trim('"', ' ')
    if (-not (Is-Install $Game)) { Say "  No Game.exe in that folder." 'Yellow' }
  }
  Say "  Copying $Game (about 2.2 GB)..."
  $code = Exec @('robocopy', $Game, (Join-Path $Root 'game'), '/E', '/NFL', '/NDL', '/NJH', '/NP')
  if ($code -ge 8) { Fail "copying the game failed (robocopy exit code $code)." }   # robocopy: <8 is success
}

# ---------------------------------------------------------------- analyse
Step 3 "Analysing Game.exe (seconds)"
New-Item -ItemType Directory -Force work | Out-Null
if ((Test-Path 'work\rtti_seeds.json') -and -not $Force) { Say "  Already done (skipping)." }
else {
  Run "Headers and imports" ($pyargs + @("$T\pe\pe_analyze.py", 'game\Game.exe', '--json', 'work\pe_analysis.json'))
  Run "C++ classes from RTTI" ($pyargs + @("$T\cpp\rtti.py", 'game\Game.exe', '-o', 'work\rtti.json', '--seeds', 'work\rtti_seeds.json'))
}

# ---------------------------------------------------------------- catalog
Step 4 "Finding every function (about 15 minutes, once)"
if ((Test-Path 'work\functions.json') -and -not $Force) { Say "  Already done (skipping)." }
else {
  Run "Disassembling" ($pyargs + @("$T\disasm\disasm32.py", 'game\Game.exe', '-o', 'work\functions.json', '--seed-functions', 'work\rtti_seeds.json'))
}

# ---------------------------------------------------------------- lift
Step 5 "Lifting the game to C (5 to 15 minutes)"
if ((Test-Path 'src\recomp\gen\recomp_dispatch.c') -and -not $Force) { Say "  Already done (skipping)." }
else {
  $env:PCRECOMP = $Toolkit
  Run "Lifting" ($pyargs + @('run_lift.py', '--all'))
}

# ---------------------------------------------------------------- build
Step 6 "Building build\ts.exe (10 to 30 minutes)"
if ((Test-Path 'build\ts.exe') -and -not $Force) { Say "  Already done (skipping)." }
else {
  Run "Compiling" @('cmd', '/c', (Join-Path $Root 'build.cmd'))
}

# ---------------------------------------------------------------- shortcut
Step 7 "Making the shortcut"
$lnk = Join-Path $Root 'Tiberian Sun (recomp).cmd'
Set-Content -Path $lnk -Encoding ASCII -Value @(
  '@echo off',
  'rem Plays the recompiled game in its own window: settings F10, scaling F12, fullscreen F11.',
  'cd /d "%~dp0"',
  'start "" build\ts.exe --run')
Say "  $lnk"

Write-Host ""
Say "Done. Double-click 'Tiberian Sun (recomp).cmd' to play." 'Green'
Say "F10 opens the settings (scaling, fullscreen, HD vehicles, the game's resolution)."
Read-Host "Press Enter to close" | Out-Null
