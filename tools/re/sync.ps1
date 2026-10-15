# Syncs the re/ tables with a Ghidra project (headless).
#
#   .\tools\re\sync.ps1 -Import <path to Hod2.exe>   # new project: import, auto-analyze, apply re/
#   .\tools\re\sync.ps1 -Apply                       # re/*.csv -> Ghidra project
#   .\tools\re\sync.ps1 -Export                      # names given in Ghidra -> re/functions.csv, re/globals.csv
#
# Close the project in the Ghidra GUI first: headless mode cannot open a locked project.
# After -Export, fill in notes/doc of new rows and run: python tools/re/check_re.py
param(
  [Parameter(ParameterSetName = 'Import', Mandatory = $true)] [string]$Import,
  [Parameter(ParameterSetName = 'Apply', Mandatory = $true)] [switch]$Apply,
  [Parameter(ParameterSetName = 'Export', Mandatory = $true)] [switch]$Export,
  [string]$ProjectDir = (Join-Path $PSScriptRoot '..\..\..\ghidra-projects'),
  [string]$ProjectName = 'hod2'
)
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$scripts = Join-Path $repo 're\ghidra_scripts'
$reDir = Join-Path $repo 're'
$ghidra = Join-Path $PSScriptRoot 'ghidra.ps1'

# Ghidra rejects project paths with '..' elements.
New-Item -ItemType Directory -Force $ProjectDir | Out-Null
$ProjectDir = (Resolve-Path $ProjectDir).Path

if ($Import) {
  $exe = (Resolve-Path $Import).Path
  $ghidraArgs = @($ProjectDir, $ProjectName, '-import', $exe,
    '-scriptPath', $scripts, '-postScript', 'ApplyThod2Re.java', $reDir)
} else {
  $script = if ($Apply) { 'ApplyThod2Re.java' } else { 'ExportThod2Re.java' }
  $ghidraArgs = @($ProjectDir, $ProjectName, '-process', 'Hod2.exe', '-noanalysis',
    '-scriptPath', $scripts, '-postScript', $script, $reDir)
}

# Keep the full Ghidra log out of the console; show the script output and errors.
$log = Join-Path ([IO.Path]::GetTempPath()) 'thod2-re-sync.log'
& $ghidra -Headless @ghidraArgs *> $log
$code = $LASTEXITCODE
Select-String -Path $log -Pattern '(ApplyThod2Re|ExportThod2Re)\.java>|ERROR|REPORT: .*(failed|Import)' |
  ForEach-Object { $_.Line -replace '^INFO\s+', '' -replace '\s+\(GhidraScript\)\s*$', '' }
"Ghidra log: $log (exit $code)"
exit $code
