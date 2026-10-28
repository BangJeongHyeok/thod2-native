# Packs the Release build into the player download (build\package\thod2-native.zip).
# Its files are extracted straight into the game folder (next to Hod2.exe).
#
#   cmake --build --preset x86-release
#   .\tools\package.ps1
#
# The GitHub Actions workflow (.github/workflows/build.yml) runs the same
# script and attaches the zip to the release of a v* tag.
param(
  [string]$OutDir = (Join-Path $PSScriptRoot '..\build\package')
)
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$bin = Join-Path $repo 'build\x86\bin\Release'
$files = [ordered]@{
  'dinput.dll'        = Join-Path $bin 'dinput.dll'
  'thod2_core.dll'    = Join-Path $bin 'thod2_core.dll'
  'thod2-native.ini'  = Join-Path $repo 'config\thod2-native.ini'
  'thod2-native-README.txt' = Join-Path $repo 'config\package-readme.txt'
  'thod2-native-LICENSE.txt' = Join-Path $repo 'LICENSE'
}
foreach ($source in $files.Values) {
  if (-not (Test-Path $source)) { throw "missing $source (build Release first)" }
}

$stage = Join-Path $OutDir 'thod2-native'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
foreach ($name in $files.Keys) {
  Copy-Item $files[$name] (Join-Path $stage $name)
}
$zip = Join-Path $OutDir 'thod2-native.zip'
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Get-ChildItem $stage | ForEach-Object { '{0,-26} {1,10:N0} bytes' -f $_.Name, $_.Length }
"-> $zip ({0:N0} bytes)" -f (Get-Item $zip).Length
