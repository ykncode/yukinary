Set-Location "d:\YukiLLM\yukinary_vanilla"
# Sadece IrMotoru struct — ice_aktar olmadan inline tanım
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

# Minimal -- sadece 3 alan, pointer + 2 tamsayi
yapi MinMotor {
    ptr1:   *u8;
    sayac:  tamsayi;
    kap:    tamsayi;
}

isv ana() -> tamsayi {
    degisken m: *MinMotor = bellek_tahsis_et(64);
    m.ptr1  = 0;
    m.sayac = 0;
    m.kap   = 9999;
    m.sayac = m.sayac + 1;
    ekrana_yaz("ptr1=".ptr);  sayiyi_yazdir(m.ptr1);  ekrana_yaz("\n".ptr);
    ekrana_yaz("sayac=".ptr); sayiyi_yazdir(m.sayac); ekrana_yaz("\n".ptr);
    ekrana_yaz("kap=".ptr);   sayiyi_yazdir(m.kap);   ekrana_yaz("\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tirmin.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tirmin.ykn tirmin.exe 2>&1 | Out-Null
if (Test-Path tirmin.exe) {
    $r = & .\tirmin.exe 2>&1
    Write-Host "MinMotor: $r"
    Remove-Item tirmin.exe
} else { Write-Host "NO BIN" }
Remove-Item tests\tirmin.ykn -EA SilentlyContinue
