[CmdletBinding()]
param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [int]$TimeoutMs = 10000,
    [string]$OutputJson = "build/reports/fallback_dynamic_load_report.json"
)

$ErrorActionPreference = "Stop"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$workDir = Join-Path $ROOT "build\test-work\fallback-dynamic"
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

$isWindows = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
if (-not $isWindows) { throw "This native fixture test currently targets the Windows PE loader; POSIX is exercised in Docker separately." }

$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($null -eq $gpp) {
    $knownGpp = "C:\Users\naitr\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe"
    if (Test-Path -LiteralPath $knownGpp) { $gppPath = $knownGpp } else { throw "g++ not found" }
} else { $gppPath = $gpp.Source }

$fixtureDll = Join-Path $workDir "dynamic_fixture.dll"
$fixtureC = Join-Path $ROOT "tests\fallback\dynamic_fixture.c"
& $gppPath -shared -O2 -o $fixtureDll $fixtureC 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $fixtureDll)) { throw "Dynamic fixture DLL build failed" }
$libraryName = ($fixtureDll -replace "\\", "/")
$symbolName = "dyn_add"

Write-Host "=== Dynamic Load Test ===" -ForegroundColor Cyan
$srcPath = Join-Path $workDir "test_dynload.ykn"
$source = @"
ice_aktar "src/fallback_dinamik.ykn";

isv ana() -> tamsayi {
    degisken h: tamsayi = dyn_lib_yukle("$libraryName".ptr);
    eger h != 0 ise {
        ekrana_yaz("DYNLOAD: load OK\n".ptr);
        degisken fn_ptr: *u8 = dyn_sym_al("$symbolName".ptr);
        eger fn_ptr != 0 ise {
            degisken sonuc: tamsayi = dyn_sym_cagir_i64("$symbolName".ptr, 19, 23);
            eger sonuc == 42 ise {
                ekrana_yaz("DYNLOAD: symbol call OK\n".ptr);
            } yoksa {
                ekrana_yaz("DYNLOAD: symbol call FAIL\n".ptr);
                dyn_lib_kapat();
                dondu 3;
            }
            eger dyn_sym_cagir_i64("missing_symbol".ptr, 1, 2) != 0 - 1 ise {
                ekrana_yaz("DYNLOAD: missing symbol accepted\n".ptr);
                dyn_lib_kapat();
                dondu 4;
            }
        } yoksa {
            ekrana_yaz("DYNLOAD: symbol FAIL\n".ptr);
            dyn_lib_kapat();
            dondu 2;
        }
        dyn_lib_kapat();
        dondu 0;
    }
    ekrana_yaz("DYNLOAD: load FAIL\n".ptr);
    dondu 1;
}
"@
Set-Content -LiteralPath $srcPath -Value $source -Encoding UTF8

$compilerPath = if ([IO.Path]::IsPathRooted($Compiler)) { $Compiler } else { Join-Path $ROOT $Compiler }
if (-not (Test-Path -LiteralPath $compilerPath)) { throw "Compiler not found: $compilerPath" }

$dynSrc = Join-Path $ROOT "src\fallback_dinamik.ykn"
$dynText = Get-Content -LiteralPath $dynSrc -Raw
$noMockHandles = ($dynText -notmatch "g_dyn_lib_handle\s*=\s*1\s*;")
if (-not $noMockHandles) { throw "Dynamic fallback contains mock handle assignment" }

$outPath = Join-Path $workDir "test_dynload.exe"
Remove-Item -LiteralPath $outPath -Force -ErrorAction SilentlyContinue
& $compilerPath $srcPath $outPath 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $outPath)) { throw "Dynamic fixture compilation failed" }

$runOutput = & $outPath 2>&1 | Out-String
$runCode = $LASTEXITCODE
if ($runCode -ne 0 -or $runOutput -notmatch "DYNLOAD: load OK" -or $runOutput -notmatch "DYNLOAD: symbol call OK") {
    throw "Dynamic loader execution failed: exit=$runCode output=$runOutput"
}

$reportPath = Join-Path $ROOT $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $reportPath) -Force | Out-Null
[ordered]@{
    schema = "yukinary.fallback-dynamic.v2"
    generated_at = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    platform = if ($isWindows) { "windows" } else { "posix" }
    library = $libraryName
    symbol = $symbolName
    no_mock_handles = $noMockHandles
    compiled = $true
    native_exit_code = $runCode
    load_ok = $true
    symbol_ok = $true
} | ConvertTo-Json | Set-Content -LiteralPath $reportPath -Encoding UTF8

Write-Host "PASS: dynamic library and symbol resolved natively" -ForegroundColor Green
exit 0
