Set-Location "d:\YukiLLM\yukinary_vanilla"
# format() minimal — sadece ilk {} değiştir
$code = @"
harici isv bellek_tahsis_et(n: tamsayi) -> *u8;
isv ekrana_yaz(m: *u8) {}
isv ml(s: *u8) -> tamsayi { degisken n: tamsayi = 0; dongu s[n] != 0 { n = n + 1; } dondu n; }
isv cat(a: *u8, b: *u8) -> *u8 {
    degisken la: tamsayi = ml(a);
    degisken lb: tamsayi = ml(b);
    degisken r: *u8 = bellek_tahsis_et(la + lb + 1);
    degisken i: tamsayi = 0;
    dongu i < la { r[i] = a[i]; i = i + 1; }
    dongu i < la + lb { r[i] = b[i - la]; i = i + 1; }
    r[la + lb] = 0;
    dondu r;
}
isv tm(n: tamsayi) -> *u8 {
    eger n == 0 ise { dondu "0".ptr; }
    degisken buf: *u8 = bellek_tahsis_et(24);
    degisken i: tamsayi = 22;
    buf[23] = 0;
    dongu n > 0 { buf[i] = (n % 10) + 48; n = n / 10; i = i - 1; }
    dondu buf + i + 1;
}
isv fmt1(sbl: *u8, a: tamsayi) -> *u8 {
    degisken len: tamsayi = ml(sbl);
    degisken p: tamsayi = 0;
    degisken found: tamsayi = 0;
    dongu p < len {
        eger sbl[p] == 123 ve sbl[p + 1] == 125 ise { found = 1; durdur; }
        p = p + 1;
    }
    eger found == 0 ise { dondu sbl; }
    degisken part1: *u8 = bellek_tahsis_et(p + 1);
    degisken x: tamsayi = 0;
    dongu x < p { part1[x] = sbl[x]; x = x + 1; }
    part1[p] = 0;
    dondu cat(cat(part1, tm(a)), sbl + p + 2);
}
isv ana() -> tamsayi {
    degisken s: *u8 = fmt1("val={}!".ptr, 42);
    ekrana_yaz(s);
    ekrana_yaz("\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tfa2.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tfa2.ykn tfa2.exe 2>&1 | Out-Null
if (Test-Path tfa2.exe) {
    $r = & .\tfa2.exe 2>&1
    Write-Host "fmt1: '$r'"
    Remove-Item tfa2.exe
} else {
    Write-Host "NO BINARY"
}
Remove-Item tests\tfa2.ykn -EA SilentlyContinue
