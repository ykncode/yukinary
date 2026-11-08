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
ice_aktar "../../src/ir.ykn";
isv ana() -> tamsayi {
    degisken motor: *IrMotoru = ir_motoru_olustur(1048576);
    degisken fonk: *IrFonksiyonu = ir_fonksiyon_baslat(motor, 12345);
    ekrana_yaz("motor.sanal_reg_sayaci after start: ".ptr);
    sayiyi_yazdir(motor.sanal_reg_sayaci);
    ekrana_yaz("\n".ptr);
    degisken r1: tamsayi = ir_yeni_reg(motor);
    ekrana_yaz("r1=".ptr); sayiyi_yazdir(r1); ekrana_yaz("\n".ptr);
    degisken r2: tamsayi = ir_yeni_reg(motor);
    ekrana_yaz("r2=".ptr); sayiyi_yazdir(r2); ekrana_yaz("\n".ptr);
    ekrana_yaz("fonk.sanal_reg_sayisi=".ptr); sayiyi_yazdir(fonk.sanal_reg_sayisi); ekrana_yaz("\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\ir_dbg.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\ir_dbg.ykn ir_dbg.exe 2>&1 | Out-Null
if (Test-Path ir_dbg.exe) {
    $r = & .\ir_dbg.exe 2>&1
    Write-Host $r
    Remove-Item ir_dbg.exe
}
Remove-Item tests\ir_dbg.ykn -EA SilentlyContinue
