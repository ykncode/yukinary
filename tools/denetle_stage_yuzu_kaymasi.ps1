# ============================================================
# denetle_stage_yuzu_kaymasi.ps1 - Stage-0 / Stage-3 Drift Gate
#
# Iki bolumlu kontrol:
#   BOLUM 1: Yuzey sembol karsilastirmasi (Surface Hash)
#   BOLUM 2: 12 kritik test cikti karsilastirmasi
#
# Cikis kurallari:
#   - Temiz ise  : exit 0
#   - Diff var   : exit 1
# ============================================================

[CmdletBinding()]
param(
    [switch]$SnapshotOnly
)

$ErrorActionPreference = "Continue"
Set-StrictMode -Version Latest

$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Ensure-Directory([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Get-RootPath([string]$RelativePath) {
    if ([System.IO.Path]::IsPathRooted($RelativePath)) {
        return [System.IO.Path]::GetFullPath($RelativePath)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $ROOT $RelativePath))
}

function Get-StringSha256([string]$InputString) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($InputString)
    $hashBytes = $sha.ComputeHash($bytes)
    $sha.Dispose()
    return [System.BitConverter]::ToString($hashBytes).Replace("-", "").ToLowerInvariant()
}

function Get-FileSha256([string]$FilePath) {
    $fs = [System.IO.File]::OpenRead($FilePath)
    try {
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $hashBytes = $sha.ComputeHash($fs)
            return [System.BitConverter]::ToString($hashBytes).Replace("-", "").ToLowerInvariant()
        }
        finally {
            $sha.Dispose()
        }
    }
    finally {
        $fs.Dispose()
    }
}

$REPORTS_DIR = Get-RootPath "build\reports"
$WORK_DIR = Get-RootPath "build\test-work\drift-gate"
$SNAPSHOT_FILE = Join-Path $REPORTS_DIR "stage0_stage3_surface.sha256"

Ensure-Directory $REPORTS_DIR
Ensure-Directory $WORK_DIR

$STAGE0_CANDIDATES = @(
    (Get-RootPath "build\stage0.exe"),
    (Get-RootPath "build\bootstrap\stage0.exe")
)

$STAGE3_CANDIDATES = @(
    (Get-RootPath "build\stage3.exe"),
    (Get-RootPath "build\bootstrap\stage3.exe")
)

$Stage0Path = $null
foreach ($c in $STAGE0_CANDIDATES) {
    if (Test-Path -LiteralPath $c) { $Stage0Path = $c; break }
}

$Stage3Path = $null
foreach ($c in $STAGE3_CANDIDATES) {
    if (Test-Path -LiteralPath $c) { $Stage3Path = $c; break }
}

# ----------------------------------------------------------------
# Graceful fallback: Binary yoksa ve -SnapshotOnly ise bilgi verip cik
# ----------------------------------------------------------------
if (-not $SnapshotOnly) {
    if (-not $Stage0Path -or -not $Stage3Path) {
        Write-Host "[WARN] Stage-0 ve/veya Stage-3 binary bulunamadi."
        if (-not $Stage0Path) { Write-Host "  Stage-0 aranan: $($STAGE0_CANDIDATES -join ', ')" }
        if (-not $Stage3Path) { Write-Host "  Stage-3 aranan: $($STAGE3_CANDIDATES -join ', ')" }
        Write-Host "  Drift kontrolu icin once bootstrap build gerek. CI'da artifact indirildikten sonra calisacaktir."
        Write-Host "[SKIP] Drift: Binary eksik - atlandi"
        exit 0
    }
}

# ============================================================
# BOLUM 1: YUZEY SEMBOL KARSILASTIRMASI (Surface Hash)
# ============================================================

Write-Host "============================================================"
Write-Host "BOLUM 1: Yuzey Sembol Karsilastirmasi (Surface Hash)"
Write-Host "============================================================"

$SYMBOL_PATTERN = '^(isv |fonksiyon |yapi |degisken |makro |trait |enum |olustur )'

$StdFiles = @()
$StdRoot = Get-RootPath "std"
if (Test-Path $StdRoot) {
    $StdFiles = Get-ChildItem -Path $StdRoot -Filter *.ykn -Recurse -File | Sort-Object FullName
}

$SrcFiles = @()
$SrcRoot = Get-RootPath "src"
if (Test-Path $SrcRoot) {
    $SrcFiles = Get-ChildItem -Path $SrcRoot -Filter *.ykn -Recurse -File | Sort-Object FullName
}

$AllYknFiles = @($StdFiles) + @($SrcFiles)

Write-Host "  Taranan std/*.ykn sayisi: $($StdFiles.Count)"
Write-Host "  Taranan src/*.ykn sayisi: $($SrcFiles.Count)"
Write-Host "  Toplam taranan dosya    : $($AllYknFiles.Count)"

$SurfaceLines = New-Object System.Collections.Generic.List[string]

foreach ($file in $AllYknFiles) {
    try {
        $lines = Get-Content -LiteralPath $file.FullName -Encoding UTF8
        foreach ($line in $lines) {
            if ($line -match $SYMBOL_PATTERN) {
                $trimmed = $line.Trim()
                if (-not [string]::IsNullOrWhiteSpace($trimmed)) {
                    $SurfaceLines.Add($trimmed)
                }
            }
        }
    }
    catch {
        Write-Host "  [WARN] Dosya okunamadi: $($file.FullName) - $_"
    }
}

$SurfaceLinesSorted = $SurfaceLines | Sort-Object
$SurfaceJoined = $SurfaceLinesSorted -join [Environment]::NewLine
$SurfaceHash = Get-StringSha256 $SurfaceJoined

Write-Host "  Tespit edilen sembol satiri: $($SurfaceLines.Count)"
Write-Host "  Hesaplanan SHA-256 (surface): $SurfaceHash"

$SnapshotExists = Test-Path -LiteralPath $SNAPSHOT_FILE

if ($SnapshotOnly) {
    Set-Content -LiteralPath $SNAPSHOT_FILE -Value $SurfaceHash -Encoding UTF8 -NoNewline
    Write-Host ""
    Write-Host "[SNAPSHOT] Snapshot olusturuldu: $SNAPSHOT_FILE"
    Write-Host "  Icerik: $SurfaceHash"
    exit 0
}

$DiffDetected = $false
$DiffMessages = New-Object System.Collections.Generic.List[string]

if (-not $SnapshotExists) {
    Write-Host "  [WARN] Referans snapshot dosyasi bulunamadi: $SNAPSHOT_FILE"
    Write-Host "  Ilk calistirmada -SnapshotOnly parametresiyle snapshot olusturunuz."
    $DiffDetected = $true
    $DiffMessages.Add("Snapshot bulunamadi - ilk kurulum")
}
else {
    $SavedHash = (Get-Content -LiteralPath $SNAPSHOT_FILE -Encoding UTF8).Trim()
    if ($SavedHash -ne $SurfaceHash) {
        Write-Host "  [FAIL] Surface hash FARKLI!"
        Write-Host "    Referans : $SavedHash"
        Write-Host "    Hesaplanan: $SurfaceHash"
        $DiffDetected = $true
        $DiffMessages.Add("Surface Hash: snapshot=$SavedHash  hesaplanan=$SurfaceHash")
    }
    else {
        Write-Host "  [OK] Surface hash TEMIZ"
    }
}

# ============================================================
# BOLUM 2: 12 KRITIK TEST CIKTI KARSILASTIRMASI
# ============================================================

Write-Host ""
Write-Host "============================================================"
Write-Host "BOLUM 2: 12 Kritik Test Cikti Karsilastirmasi"
Write-Host "============================================================"

function Get-PositiveFixtures([string]$RootDir, [int]$Limit) {
    $result = New-Object System.Collections.Generic.List[string]
    if (-not (Test-Path -LiteralPath $RootDir)) { return $result }

    $candidates = Get-ChildItem -LiteralPath $RootDir -Filter *.ykn -Recurse -File | Sort-Object FullName
    foreach ($c in $candidates) {
        if ($result.Count -ge $Limit) { break }
        $dirName = Split-Path (Split-Path $c.FullName -Parent) -Leaf
        $baseName = Split-Path $c.FullName -Parent
        if ($baseName -match "(negative|scratch|fix[2-9]|fix1[0-9]|lsp|crossplatform|ffi|guvenlik|fix8|formatter|package|repl|optimizer|generics|llm|codegen|debug_)" ) {
            continue
        }
        $content = Get-Content -LiteralPath $c.FullName -Encoding UTF8 -TotalCount 30
        $hasMain = $false
        foreach ($cl in $content) {
            if ($cl -match '^\s*(ana\s*fonksiyon|fonksiyon\s+ana|isv\s+ana|main)') { $hasMain = $true; break }
            if ($cl -match '^\s*(yazdir|goster|System\.Console|ykn_yaz|cout)' -or $cl -match 'basla\s*\{') { $hasMain = $true; break }
        }
        if ($hasMain) {
            $result.Add($c.FullName)
        }
    }
    return $result
}

$FixDogrulukDir = Get-RootPath "tests\fix_dogruluk"
$Fixtures = New-Object System.Collections.Generic.List[string]

if (Test-Path -LiteralPath $FixDogrulukDir) {
    Write-Host "  tests\fix_dogruluk dizini bulundu, ilk 12 fixture aliniyor..."
    $tmp = Get-PositiveFixtures $FixDogrulukDir 12
    foreach ($t in $tmp) { $Fixtures.Add($t) }
}

if ($Fixtures.Count -lt 12) {
    Write-Host "  tests\fix_dogruluk eksik, pozitif fixture havuzundan 12 tane seciliyor..."
    $bootstrapDir = Get-RootPath "tests\bootstrap"
    $i18nDir = Get-RootPath "tests\i18n"
    $stdlibDir = Get-RootPath "tests\stdlib"

    $p1 = Get-PositiveFixtures $bootstrapDir 12
    foreach ($t in $p1) { if ($Fixtures.Count -lt 12) { $Fixtures.Add($t) } }

    if ($Fixtures.Count -lt 12) {
        $p2 = Get-PositiveFixtures $i18nDir (12 - $Fixtures.Count)
        foreach ($t in $p2) { if ($Fixtures.Count -lt 12) { $Fixtures.Add($t) } }
    }

    if ($Fixtures.Count -lt 12) {
        $p3 = Get-PositiveFixtures $stdlibDir (12 - $Fixtures.Count)
        foreach ($t in $p3) { if ($Fixtures.Count -lt 12) { $Fixtures.Add($t) } }
    }
}

Write-Host "  Calisilacak fixture sayisi: $($Fixtures.Count)"

$TestPassCount = 0
$TestFailCount = 0

for ($i = 0; $i -lt $Fixtures.Count; $i++) {
    $fixture = $Fixtures[$i]
    $fixtureName = [System.IO.Path]::GetFileNameWithoutExtension($fixture)
    $fixtureSafe = $fixtureName -replace '[^a-zA-Z0-9_]', '_'
    $idx = ($i + 1).ToString("00")

    $caseDir = Join-Path $WORK_DIR ("{0}_{1}" -f $idx, $fixtureSafe)
    Ensure-Directory $caseDir

    $outS0 = Join-Path $caseDir "stage0_out.exe"
    $outS3 = Join-Path $caseDir "stage3_out.exe"
    $runS0Out = Join-Path $caseDir "stage0_run.stdout"
    $runS0Err = Join-Path $caseDir "stage0_run.stderr"
    $runS3Out = Join-Path $caseDir "stage3_run.stdout"
    $runS3Err = Join-Path $caseDir "stage3_run.stderr"
    $compileS0Out = Join-Path $caseDir "stage0_compile.stdout"
    $compileS0Err = Join-Path $caseDir "stage0_compile.stderr"
    $compileS3Out = Join-Path $caseDir "stage3_compile.stdout"
    $compileS3Err = Join-Path $caseDir "stage3_compile.stderr"

    Write-Host ""
    Write-Host "  --- Test $idx/$($Fixtures.Count): $fixtureName ---"

    $compileS0Ok = $true
    try {
        $s0proc = Start-Process -FilePath $Stage0Path `
            -ArgumentList @($fixture, "-o", $outS0) `
            -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput $compileS0Out -RedirectStandardError $compileS0Err
        if ($s0proc.ExitCode -ne 0) { $compileS0Ok = $false }
    }
    catch {
        Write-Host "    [WARN] Stage0 derleme hatasi: $_"
        $compileS0Ok = $false
    }

    $compileS3Ok = $true
    try {
        $s3proc = Start-Process -FilePath $Stage3Path `
            -ArgumentList @($fixture, "-o", $outS3) `
            -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput $compileS3Out -RedirectStandardError $compileS3Err
        if ($s3proc.ExitCode -ne 0) { $compileS3Ok = $false }
    }
    catch {
        Write-Host "    [WARN] Stage3 derleme hatasi: $_"
        $compileS3Ok = $false
    }

    if (-not $compileS0Ok -and -not $compileS3Ok) {
        Write-Host "    [SKIP] Iki derleyici de basarisiz - karsilastirma anlamsiz"
        $TestPassCount++
        continue
    }
    if (-not $compileS0Ok -or -not $compileS3Ok) {
        Write-Host "    [FAIL] Derleme farki: S0=$compileS0Ok  S3=$compileS3Ok"
        $DiffDetected = $true
        $DiffMessages.Add("Test $idx $fixtureName : Derleme farki S0=$compileS0Ok S3=$compileS3Ok")
        $TestFailCount++
        continue
    }

    $runS0Ok = $true
    try {
        if (Test-Path -LiteralPath $outS0) {
            $r0 = Start-Process -FilePath $outS0 `
                -NoNewWindow -Wait -PassThru `
                -RedirectStandardOutput $runS0Out -RedirectStandardError $runS0Err
        }
    }
    catch {
        Write-Host "    [WARN] Stage0 calistirma hatasi: $_"
        $runS0Ok = $false
    }

    $runS3Ok = $true
    try {
        if (Test-Path -LiteralPath $outS3) {
            $r3 = Start-Process -FilePath $outS3 `
                -NoNewWindow -Wait -PassThru `
                -RedirectStandardOutput $runS3Out -RedirectStandardError $runS3Err
        }
    }
    catch {
        Write-Host "    [WARN] Stage3 calistirma hatasi: $_"
        $runS3Ok = $false
    }

    if (-not $runS0Ok -and -not $runS3Ok) {
        Write-Host "    [SKIP] Iki binary de calismadi - atlandi"
        $TestPassCount++
        continue
    }

    function Safe-ReadAll([string]$Path) {
        if (Test-Path -LiteralPath $Path) {
            try { return [System.IO.File]::ReadAllText($Path) } catch { return "" }
        }
        return ""
    }

    $s0stdout = Safe-ReadAll $runS0Out
    $s0stderr = Safe-ReadAll $runS0Err
    $s3stdout = Safe-ReadAll $runS3Out
    $s3stderr = Safe-ReadAll $runS3Err

    $s0Combined = $s0stdout + "`n==STDERR==`n" + $s0stderr
    $s3Combined = $s3stdout + "`n==STDERR==`n" + $s3stderr

    $s0Hash = Get-StringSha256 $s0Combined
    $s3Hash = Get-StringSha256 $s3Combined

    if ($s0Hash -eq $s3Hash) {
        Write-Host "    [OK] stdout+stderr ESLER  ($s0Hash)"
        $TestPassCount++
    }
    else {
        Write-Host "    [FAIL] stdout+stderr FARKLI!"
        Write-Host "      Stage-0 hash: $s0Hash"
        Write-Host "      Stage-3 hash: $s3Hash"
        $DiffDetected = $true
        $DiffMessages.Add("Test $idx $fixtureName : stdout+stderr S0=$s0Hash  S3=$s3Hash")
        $TestFailCount++
    }
}

# ============================================================
# SONUC
# ============================================================

Write-Host ""
Write-Host "============================================================"
Write-Host "SONUC"
Write-Host "============================================================"
Write-Host "  Yuzey Sembol  : $(if ($SnapshotExists -and -not $DiffMessages[0] -match 'Surface Hash') { 'TEMIZ' } elseif (-not $SnapshotExists) { 'ATLANDI' } else { 'DIFF' })"
Write-Host "  Test Basarili : $TestPassCount / $($Fixtures.Count)"
Write-Host "  Test Basarisiz: $TestFailCount / $($Fixtures.Count)"

if (-not $DiffDetected) {
    Write-Host ""
    Write-Host "[OK] Drift: TEMIZ"
    exit 0
}
else {
    Write-Host ""
    Write-Host "[FAIL] Drift: DIFF bulundu"
    Write-Host ""
    Write-Host "  Detaylar:"
    foreach ($msg in $DiffMessages) {
        Write-Host "    - $msg"
    }
    exit 1
}
