# ============================================================
# measure_global_state.ps1 - Audit global state across YKN compiler modules
# ============================================================
[CmdletBinding()]
param()

$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$srcDir = Join-Path $ROOT "src"

$files = Get-ChildItem -Path $srcDir -Filter *.ykn | Sort-Object Name
$total = 0
$registry = [ordered]@{}

foreach ($f in $files) {
    $lines = Get-Content -Path $f.FullName
    $vars = @()
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]
        if ($line -match '^\s*degisken\s+(g_[a-zA-Z0-9_]+)') {
            $vars += [PSCustomObject]@{
                Name = $Matches[1]
                Line = $i + 1
            }
        }
    }
    if ($vars.Count -gt 0) {
        $registry[$f.Name] = $vars
        $total += $vars.Count
    }
}

Write-Host "=== YKN COMPILER GLOBAL STATE AUDIT ===" -ForegroundColor Cyan
Write-Host "Total global variables (degisken g_*): $total`n" -ForegroundColor Yellow

$summary = @()
foreach ($k in $registry.Keys) {
    $summary += [PSCustomObject]@{
        Module = $k
        Count  = $registry[$k].Count
    }
}
$summary | Sort-Object Count -Descending | Format-Table -AutoSize

# Output JSON report
$reportDir = Join-Path $ROOT "build\reports"
if (-not (Test-Path $reportDir)) { New-Item -ItemType Directory -Path $reportDir -Force | Out-Null }
$reportPath = Join-Path $reportDir "global-state-audit.json"

$jsonObj = [ordered]@{
    schema = "yukinary.global-state-audit.v1"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    total_globals = $total
    modules = [ordered]@{}
}
foreach ($k in $registry.Keys) {
    $jsonObj.modules[$k] = [ordered]@{
        count = $registry[$k].Count
        variables = ($registry[$k] | ForEach-Object { [ordered]@{ name = $_.Name; line = $_.Line } })
    }
}

$jsonObj | ConvertTo-Json -Depth 5 | Set-Content -Path $reportPath -Encoding utf8
Write-Host "Registry saved to $reportPath" -ForegroundColor Green
