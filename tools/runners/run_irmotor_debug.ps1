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

yapi TestMotorX {
    ptr1: *u8;      # +0
    ptr2: *u8;      # +8
    sayac: tamsayi; # +16  ← 3. alan
    kap:   tamsayi; # +24  ← 4. alan
}

isv ana() -> tamsayi {
    degisken m: *TestMotorX = bellek_tahsis_et(64);
    m.ptr1 = 0;
    m.ptr2 = 0;
    m.sayac = 0;
    m.kap = 9999;
    
    m.sayac = m.sayac + 1;
    ekrana_yaz("sayac=".ptr); sayiyi_yazdir(m.sayac); ekrana_yaz(" (beklenen 1)\n".ptr);
    ekrana_yaz("kap=".ptr);   sayiyi_yazdir(m.kap);   ekrana_yaz(" (beklenen 9999)\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tirmotor.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tirmotor.ykn tirmotor.exe 2>&1 | Out-Null
if (Test-Path tirmotor.exe) {
    $r = & .\tirmotor.exe 2>&1
    Write-Host "TestMotorX: $r"
    Remove-Item tirmotor.exe
}
Remove-Item tests\tirmotor.ykn -EA SilentlyContinue
