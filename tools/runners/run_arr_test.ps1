Set-Location "d:\YukiLLM\yukinary_vanilla"
$code = @"
harici isv bellek_tahsis_et(n: tamsayi) -> *u8;
isv ekrana_yaz(m: *u8) {}
isv sayiyi_yazdir(n: tamsayi) {
    degisken buf: [32]u8; degisken i: tamsayi = 0;
    eger n == 0 ise { buf[0] = 48; buf[1] = 0; ekrana_yaz(buf); dondu; }
    eger n < 0 ise { buf[i] = 45; i = i + 1; n = 0 - n; }
    degisken start: tamsayi = i;
    dongu n > 0 { buf[i] = (n % 10) + 48; n = n / 10; i = i + 1; }
    buf[i] = 0;
    degisken j: tamsayi = start; degisken k: tamsayi = i - 1;
    dongu j < k { degisken tmp: u8 = buf[j]; buf[j] = buf[k]; buf[k] = tmp; j = j + 1; k = k - 1; }
    ekrana_yaz(buf);
}
isv yaz_arr(out: *tamsayi, val: tamsayi) { out[0] = val; }
isv ana() -> tamsayi {
    degisken p: [1]tamsayi;
    yaz_arr(p, 42);
    sayiyi_yazdir(p[0]);
    ekrana_yaz("\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tarr.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tarr.ykn tarr.exe 2>&1 | Out-Null
if (Test-Path tarr.exe) {
    $r = & .\tarr.exe 2>&1
    Write-Host "arr[1]tamsayi: '$r' (beklenen 42)"
    Remove-Item tarr.exe
} else { Write-Host "NO BIN" }
Remove-Item tests\tarr.ykn -EA SilentlyContinue
