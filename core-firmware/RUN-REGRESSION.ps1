# Host-only tests; this script never flashes or accesses COM ports.
Set-Location $PSScriptRoot
python .\qa\run-regression.py
if ($LASTEXITCODE -ne 0) { throw 'OpenTail host regression checks failed' }
