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
    degisken motor: *IrMotoru = bellek_tahsis_et(256);
    # Manuel sıfırlama
    motor.aktif_fonksiyon = 0;
    motor.aktif_blok      = 0;
    motor.sanal_reg_sayaci = 0;
    motor.blok_sayaci      = 0;
    motor.ilk_fonksiyon    = 0;
    motor.son_fonksiyon    = 0;
    motor.arena_ptr        = bellek_tahsis_et(65536);
    motor.arena_mevcut     = 0;
    motor.arena_kapasite   = 65536;
    motor.hata_sayisi      = 0;
    
    ekrana_yaz("sanal_reg_sayaci init=".ptr); sayiyi_yazdir(motor.sanal_reg_sayaci); ekrana_yaz("\n".ptr);
    motor.sanal_reg_sayaci = motor.sanal_reg_sayaci + 1;
    ekrana_yaz("sanal_reg_sayaci +1=".ptr);   sayiyi_yazdir(motor.sanal_reg_sayaci); ekrana_yaz("\n".ptr);
    ekrana_yaz("arena_kapasite=".ptr);         sayiyi_yazdir(motor.arena_kapasite);  ekrana_yaz("\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tirmotor2.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tirmotor2.ykn tirmotor2.exe 2>&1 | Out-Null
if (Test-Path tirmotor2.exe) {
    $r = & .\tirmotor2.exe 2>&1
    Write-Host "IrMotoru manuel: $r"
    Remove-Item tirmotor2.exe
} else {
    Write-Host "NO BIN"
}
Remove-Item tests\tirmotor2.ykn -EA SilentlyContinue
