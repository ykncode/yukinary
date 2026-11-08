Set-Location "d:\YukiLLM\yukinary_vanilla"
$pass=0; $fail=0
$suites = @{
    "stdlib"   = "tests/stdlib/*.ykn"
    "guvenlik" = "tests/guvenlik/*.ykn"
    "generics" = "tests/generics/*.ykn"
}
foreach ($sname in $suites.Keys) {
    $sp=0; $sf=0
    foreach ($f in (Get-ChildItem $suites[$sname] | Sort-Object Name)) {
        & .\app.exe $f.FullName tmp_t.exe 2>&1 | Out-Null
        if (Test-Path tmp_t.exe) {
            $r = (& .\tmp_t.exe 2>&1) -join " "
            Remove-Item tmp_t.exe -EA SilentlyContinue
            if ($r -match "PASS|TAMAM|BASARILI|OK") { $sp++ } else { $sf++ }
        } else { $sf++ }
    }
    Write-Host "${sname}: PASS=$sp FAIL=$sf"
    $pass += $sp; $fail += $sf
}
Write-Host "TOPLAM: PASS=$pass FAIL=$fail"
