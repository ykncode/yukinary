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

yapi MinMotor {
    ptr1:   *u8;
    sayac:  tamsayi;
    kap:    tamsayi;
}

# PARAMETRE OLARAK alan test fonksiyonu
isv arttir(m: *MinMotor) {
    m.sayac = m.sayac + 1;
}

isv oku(m: *MinMotor) -> tamsayi {
    dondu m.sayac;
}

isv ana() -> tamsayi {
    degisken m: *MinMotor = bellek_tahsis_et(64);
    m.ptr1 = 0; m.sayac = 0; m.kap = 9999;
    
    arttir(m);
    arttir(m);
    
    degisken v: tamsayi = oku(m);
    ekrana_yaz("sayac param=".ptr); sayiyi_yazdir(v); ekrana_yaz(" (beklenen 2)\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tirparam.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tirparam.ykn tirparam.exe 2>&1 | Out-Null
if (Test-Path tirparam.exe) {
    $r = & .\tirparam.exe 2>&1
    Write-Host "Param test: $r"
    Remove-Item tirparam.exe
} else { Write-Host "NO BIN" }
Remove-Item tests\tirparam.ykn -EA SilentlyContinue
