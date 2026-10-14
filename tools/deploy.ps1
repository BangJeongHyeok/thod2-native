# Copies the built thod2-native binaries into a game folder (default: the test copy).
#
#   .\tools\deploy.ps1                      # Release build -> ..\test-game\THOTD2
#   .\tools\deploy.ps1 -Config Debug
#   .\tools\deploy.ps1 -Remove              # delete deployed files again
#
# thod2-native.ini is copied only when the game folder has none, so local edits survive.
param(
  [ValidateSet('Debug', 'Release')] [string]$Config = 'Release',
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\test-game\THOTD2'),
  [switch]$Remove
)
$ErrorActionPreference = 'Stop'

$repo = Resolve-Path (Join-Path $PSScriptRoot '..')
$GameDir = (Resolve-Path $GameDir).Path
if (-not (Test-Path (Join-Path $GameDir 'Hod2.exe'))) {
  throw "Hod2.exe not found in $GameDir"
}

$binaries = @('thod2_core.dll')

if ($Remove) {
  foreach ($name in $binaries + @('thod2-native.log')) {
    $path = Join-Path $GameDir $name
    if (Test-Path $path) { Remove-Item $path; "removed $name" }
  }
  return
}

$binDir = Join-Path $repo "build\x86\bin\$Config"
foreach ($name in $binaries) {
  $src = Join-Path $binDir $name
  if (-not (Test-Path $src)) { throw "$src not found. Build first: cmake --build --preset x86-$($Config.ToLower())" }
  Copy-Item $src $GameDir -Force
  "copied $name ($Config)"
}

$ini = Join-Path $GameDir 'thod2-native.ini'
if (-not (Test-Path $ini)) {
  Copy-Item (Join-Path $repo 'config\thod2-native.ini') $ini
  "copied thod2-native.ini"
} else {
  "kept existing thod2-native.ini"
}
