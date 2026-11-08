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
isv ml(s: *u8) -> tamsayi { degisken n: tamsayi = 0; dongu s[n] != 0 { n = n + 1; } dondu n; }

# Basit: {} konumunu bul, p_out'a yaz
isv bul_brace(sbl: *u8, slen: tamsayi, p_out: *tamsayi) {
    degisken p: tamsayi = 0;
    dongu p < slen {
        eger sbl[p] == 123 ve sbl[p + 1] == 125 ise { p_out[0] = p; dondu; }
        p = p + 1;
    }
    p_out[0] = slen; # bulunamadi
}

isv ana() -> tamsayi {
    degisken sbl: *u8 = "x={} y={}".ptr;
    degisken slen: tamsayi = ml(sbl);
    degisken pa: [1]tamsayi;
    bul_brace(sbl, slen, pa);
    sayiyi_yazdir(pa[0]);  # beklenen: 2
    ekrana_yaz("\n".ptr);
    dondu 0;
}
"@
[System.IO.File]::WriteAllText("tests\tfd.ykn", $code, [System.Text.Encoding]::UTF8)
& .\app.exe tests\tfd.ykn tfd.exe 2>&1 | Out-Null
if (Test-Path tfd.exe) {
    $r = & .\tfd.exe 2>&1
    Write-Host "brace pos: '$r' (beklenen 2)"
    Remove-Item tfd.exe
} else { Write-Host "NO BIN" }
Remove-Item tests\tfd.ykn -EA SilentlyContinue
