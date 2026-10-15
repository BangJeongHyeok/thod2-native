# Runs Ghidra (GUI or headless) with the JDK from the external tools folder.
#
#   .\tools\re\ghidra.ps1                       # Ghidra GUI
#   .\tools\re\ghidra.ps1 -Headless <args...>   # analyzeHeadless.bat <args...>
#
# Ghidra's launch.bat looks for java on PATH or JAVA_HOME before it reads
# launch.properties, so JAVA_HOME is set for this process only.
# Override the locations with THOD2_GHIDRA_DIR / THOD2_JDK_DIR.
param(
  [switch]$Headless,
  [Parameter(ValueFromRemainingArguments = $true)] [string[]]$Rest
)
$ErrorActionPreference = 'Stop'

$external = Join-Path $PSScriptRoot '..\..\..\external'

function Find-ToolDir([string]$envName, [string]$pattern, [string]$probe) {
  $dir = [Environment]::GetEnvironmentVariable($envName)
  if (-not $dir) {
    $found = Get-ChildItem -Directory -Path $external -Filter $pattern -Recurse -Depth 1 -ErrorAction SilentlyContinue |
      Where-Object { Test-Path (Join-Path $_.FullName $probe) } |
      Sort-Object Name -Descending | Select-Object -First 1
    if ($found) { $dir = $found.FullName }
  }
  if (-not $dir -or -not (Test-Path (Join-Path $dir $probe))) {
    throw "$probe not found. Install it under $external or set $envName."
  }
  return (Resolve-Path $dir).Path
}

$ghidraDir = Find-ToolDir 'THOD2_GHIDRA_DIR' 'ghidra_*_PUBLIC' 'ghidraRun.bat'
$env:JAVA_HOME = Find-ToolDir 'THOD2_JDK_DIR' 'jdk-21*' 'bin\java.exe'

if ($Headless) {
  & (Join-Path $ghidraDir 'support\analyzeHeadless.bat') @Rest
} else {
  & (Join-Path $ghidraDir 'ghidraRun.bat') @Rest
}
exit $LASTEXITCODE
