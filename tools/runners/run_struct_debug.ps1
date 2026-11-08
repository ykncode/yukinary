Set-Location "d:\YukiLLM\yukinary_vanilla"
$code = @"
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
harici isv bellek_tahsis_et(n: tamsayi) -> *u8;

# Minimal struct offset testi
yapi TestMotoru {
    alan1: tamsayi;
    alan2: tamsayi;
    alan3: tamsayi;
}

isv ana() -> tamsayi {
    degisken m: *TestMotoru = bellek_tahsis_et(64);
    m.alan1 = 111;
    m.alan2 = 222;
    m.alan3 = 333;
    ekrana_yaz("alan1=".ptr); sayiyi_yazdir(m.alan1); ekrana_yaz("\n".ptr);
    ekrana_yaz("alan2=".ptr); sayiyi_yazdir(m.alan2); ekrana_yaz("\n".ptr);
    ekrana_yaz("alan3=".ptr); sayiyi_yazdir(m.alan3); ekrana_yaz("\n".ptr);
    # Artırma testi
    m.alan2 = m.alan2 + 1;
    ekrana_yaz("alan2+1=".ptr); sayiyi_yazdir(m.alan2); ekrana_yaz("\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tstruct.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tstruct.ykn tstruct.exe 2>&1 | Out-Null
if (Test-Path tstruct.exe) {
    $r = & .\tstruct.exe 2>&1
    Write-Host "Struct test: $r"
    Remove-Item tstruct.exe
}
Remove-Item tests\tstruct.ykn -EA SilentlyContinue
