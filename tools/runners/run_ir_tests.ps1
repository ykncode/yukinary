Set-Location "d:\YukiLLM\yukinary_vanilla"
$pass=0; $fail=0
foreach ($f in (Get-ChildItem "tests/ir/*.ykn" | Sort-Object Name)) {
    $out = & .\app.exe $f.FullName "tmp_ir.exe" 2>&1
    if (Test-Path tmp_ir.exe) {
        $r = (& .\tmp_ir.exe 2>&1) -join " "
        Remove-Item tmp_ir.exe -EA SilentlyContinue
        if ($r -match "PASS") { $pass++; Write-Host "  PASS: $($f.Name)" }
        else { $fail++; Write-Host "  FAIL: $($f.Name) -> $($r.Substring(0,[Math]::Min(60,$r.Length)))" }
    } else {
        $fail++
        $e = ($out | Where-Object { $_ -match "HATA" } | Select-Object -First 1)
        Write-Host "  FAIL(no bin): $($f.Name) -> $($e.Substring(0,[Math]::Min(80,$e.Length)))"
    }
}
Write-Host ""
Write-Host "IR testleri: PASS=$pass FAIL=$fail"
