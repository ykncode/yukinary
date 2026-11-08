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

# Tüm bağımlı struct'lar dahil, IrMotoru sona koy
yapi IrKomutu2 { opcode: tamsayi; hedef: tamsayi; }
yapi IrBloku2   { id: tamsayi;    kayac: tamsayi; }
yapi IrFonk2    { hash: tamsayi;  reg_sayisi: tamsayi; }

yapi IrMotoru2 {
    aktif_f:  *IrFonk2;
    aktif_b:  *IrBloku2;
    sayac:    tamsayi;    # offset +16
    blok_k:   tamsayi;    # offset +24
    arena_p:  *u8;        # offset +32
    arena_m:  tamsayi;    # offset +40
    arena_k:  tamsayi;    # offset +48
}

isv arttir2(m: *IrMotoru2) {
    m.sayac = m.sayac + 1;
}

isv ana() -> tamsayi {
    degisken m: *IrMotoru2 = bellek_tahsis_et(128);
    m.aktif_f = 0;
    m.aktif_b = 0;
    m.sayac   = 0;
    m.blok_k  = 0;
    m.arena_p = bellek_tahsis_et(1048576);
    m.arena_m = 0;
    m.arena_k = 1048576;
    
    arttir2(m);
    arttir2(m);
    
    ekrana_yaz("sayac=".ptr); sayiyi_yazdir(m.sayac); ekrana_yaz(" (beklenen 2)\n".ptr);
    ekrana_yaz("arena_k=".ptr); sayiyi_yazdir(m.arena_k); ekrana_yaz(" (beklenen 1048576)\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tirfull.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tirfull.ykn tirfull.exe 2>&1 | Out-Null
if (Test-Path tirfull.exe) {
    $r = & .\tirfull.exe 2>&1
    Write-Host "IrMotoru2: $r"
    Remove-Item tirfull.exe
} else { Write-Host "NO BIN" }
Remove-Item tests\tirfull.ykn -EA SilentlyContinue
