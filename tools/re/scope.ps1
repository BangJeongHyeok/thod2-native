# Measures the size of the reverse-engineering job (function counts, code coverage,
# decompiler quality per code region). Works on a temporary copy of the Ghidra
# project, so the real project is not changed.
#
#   .\tools\re\scope.ps1                 # prints the summary
#   .\tools\re\scope.ps1 -Keep           # also keeps the TSV files and the project copy
#
# Close the project in the Ghidra GUI first. Takes about 3 minutes.
# Results and how to read them: docs/work/reverse-engineering.md "리버싱 규모 산정".
param(
  [string]$ProjectDir = (Join-Path $PSScriptRoot '..\..\..\ghidra-projects'),
  [string]$ProjectName = 'hod2',
  [switch]$Keep
)
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$scripts = Join-Path $repo 're\ghidra_scripts'
$ghidra = Join-Path $PSScriptRoot 'ghidra.ps1'
$ProjectDir = (Resolve-Path $ProjectDir).Path

$work = Join-Path ([IO.Path]::GetTempPath()) ('thod2-re-scope-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Force $work | Out-Null
Copy-Item (Join-Path $ProjectDir "$ProjectName.gpr") $work
Copy-Item -Recurse (Join-Path $ProjectDir "$ProjectName.rep") $work

$ghidraArgs = @($work, $ProjectName, '-process', 'Hod2.exe', '-noanalysis', '-scriptPath', $scripts,
  '-postScript', 'ScopeCoverage.java', (Join-Path $work 'ptrs-before.tsv'),
  '-postScript', 'ScopeRecoverFunctions.java',
  '-postScript', 'ScopeCoverage.java', (Join-Path $work 'ptrs-after.tsv'),
  '-postScript', 'ScopeFuncStats.java', (Join-Path $work 'funcs.tsv'),
  '-postScript', 'ScopeDecompStats.java', (Join-Path $work 'decomp.tsv'))

$log = Join-Path $work 'ghidra.log'
& $ghidra -Headless @ghidraArgs *> $log
$code = $LASTEXITCODE
Select-String -Path $log -Pattern 'Scope\w+\.java>|ERROR' |
  ForEach-Object { $_.Line -replace '^INFO\s+', '' -replace '\s+\(GhidraScript\)\s*$', '' }
if ($code -ne 0) { "Ghidra failed (exit $code), log: $log"; exit $code }

python (Join-Path $PSScriptRoot 'scope_summary.py') (Join-Path $work 'funcs.tsv') (Join-Path $work 'decomp.tsv')
$code = $LASTEXITCODE

if ($Keep) { "Kept: $work" } else { Remove-Item -Recurse -Force $work }
exit $code
