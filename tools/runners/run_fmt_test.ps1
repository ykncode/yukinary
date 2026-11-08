Set-Location "d:\YukiLLM\yukinary_vanilla"
$code = @"
harici isv bellek_tahsis_et(n: tamsayi) -> *u8;
isv ekrana_yaz(m: *u8) {}
ice_aktar "../../std/metin.ykn";
isv ana()->tamsayi{
    degisken s: *u8 = format("x={} y={}".ptr, 42, 100);
    ekrana_yaz(s);
    ekrana_yaz("\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tfa.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tfa.ykn tfa.exe 2>&1 | Out-Null
if (Test-Path tfa.exe) {
    $r = & .\tfa.exe 2>&1
    Write-Host "format result: '$r'"
    Remove-Item tfa.exe
} else {
    Write-Host "NO BINARY"
}
Remove-Item tests\tfa.ykn -EA SilentlyContinue
