<#
.SYNOPSIS
    run_all_suites.ps1 - Yukinary Tam Test Suite Kosucusu (Saf PowerShell, JSON Ciktili)
.DESCRIPTION
    Tum test suitelerini Python bagimliligi olmadan calistirir.
    Ozellikler:
      - Timeout: Her test icin 10 saniye (sonsuz dongu korumasi)
      - .expected stdout karsilastirmasi
      - JSON ciktisi: build\reports\test_results.json
      - Coverage matrisi: build\reports\coverage_matrix.json
      - Regresyon esigi korumasi
.PARAMETER Compiler
    Derleyici yolu (varsayilan: .\build\bootstrap\stage0.exe)
.PARAMETER MinPassThreshold
    Minimum gecen test sayisi (varsayilan: 91)
.PARAMETER TimeoutMs
    Test basina timeout milisaniye (varsayilan: 10000)
.PARAMETER OutputJson
    JSON sonuc ciktisi dosyasi (varsayilan: build\reports\test_results.json)
.PARAMETER NoCoverage
    Coverage matrisi hesaplamasini atla
#>
param(
    [string]$Compiler         = "",
    [int]$MinPassThreshold    = 91,
    [int]$TimeoutMs           = 10000,
    [string]$OutputJson       = "build\reports\test_results.json",
    [switch]$NoCoverage,
    [switch]$KeepArtifacts,
    [ValidateSet("Absolute", "Percent")]
    [string]$ThresholdMode     = "Absolute"
)

if (-not $Compiler) {
    if (Test-Path ".\build\yukinary.exe") {
        $Compiler = ".\build\yukinary.exe"
    } else {
        $Compiler = ".\build\bootstrap\stage0.exe"
    }
}

$ErrorActionPreference = "Continue"
$ScriptStart = Get-Date
$ROOT = Split-Path -Parent $PSScriptRoot

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-CanonicalSourceRevision([string]$ProjectRoot) {
    $files = @(
        Get-ChildItem -LiteralPath (Join-Path $ProjectRoot "src") -Recurse -File -Filter "*.ykn"
        Get-ChildItem -LiteralPath (Join-Path $ProjectRoot "std") -Recurse -File -Filter "*.ykn"
        Get-Item -LiteralPath (Join-Path $ProjectRoot "app_full.cpp")
    ) | Sort-Object FullName
    $manifest = (($files | ForEach-Object {
        $relative = $_.FullName.Substring($ProjectRoot.Length).TrimStart([char[]]@("\", "/")).Replace("\", "/")
        "{0} {1}" -f $relative, (Get-Sha256 $_.FullName)
    }) -join "`n")
    $bytes = [Text.Encoding]::UTF8.GetBytes($manifest)
    return ([BitConverter]::ToString(([Security.Cryptography.SHA256]::Create()).ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
}

$compilerResolved = if ([System.IO.Path]::IsPathRooted($Compiler)) {
    [System.IO.Path]::GetFullPath($Compiler)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $ROOT $Compiler))
}
$compilerSha256 = Get-Sha256 $compilerResolved
$sourceRevisionSha256 = Get-CanonicalSourceRevision $ROOT

# Her runner çağrısı kendine ait artifact kökü kullanır. Ortak single-test.exe
# kullanımı paralel suite/parity çağrılarında sahte sonuçlara yol açıyordu.
$RUN_ID = [Guid]::NewGuid().ToString("N")
$TEST_WORK_ROOT = Join-Path $ROOT ("build\test-work\runs\" + $RUN_ID)
New-Item -ItemType Directory -Path $TEST_WORK_ROOT -Force | Out-Null

$totalPassed = 0
$totalFailed = 0
$allResults  = [System.Collections.Generic.List[object]]::new()
$reportConsistency = $false

# ─────────────────────────────────────────────
# YARDIMCI: Timeout'lu surec calistirici
# ─────────────────────────────────────────────
function Invoke-WithTimeout {
    param(
        [string]$ExePath,
        [string[]]$Arguments = @(),
        [string]$WorkDir = $ROOT,
        [int]$Ms = $TimeoutMs
    )

    $resolvedExe = if ([System.IO.Path]::IsPathRooted($ExePath)) {
        $ExePath
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $WorkDir $ExePath))
    }

    if (-not (Test-Path $resolvedExe) -and ($ExePath -eq "powershell.exe" -or $ExePath -eq "powershell")) {
        $resolvedExe = "powershell.exe"
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName  = $resolvedExe
    $psi.Arguments = $Arguments -join " "
    $psi.WorkingDirectory = $WorkDir
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError  = $true
    # YKN programları UTF-8 byte çıktısı üretir; Windows ANSI code page
    # ile implicit decode edilmesi i18n oracle'ını bozmasın.
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    $psi.StandardOutputEncoding = $utf8
    $psi.StandardErrorEncoding = $utf8
    $psi.CreateNoWindow = $true

    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    
    $started = $false
    $startErr = $null
    for ($attempt = 0; $attempt -lt 4; $attempt++) {
        try {
            [void]$p.Start()
            $started = $true
            break
        } catch {
            $startErr = $_
            Start-Sleep -Milliseconds 200
        }
    }

    if (-not $started) {
        return [PSCustomObject]@{
            ExitCode = -1
            Stdout   = ""
            Stderr   = $startErr.Exception.Message
            TimedOut = $false
        }
    }

    $stdoutTask = $p.StandardOutput.ReadToEndAsync()
    $stderrTask = $p.StandardError.ReadToEndAsync()

    $timedOut = $false
    if (-not $p.WaitForExit($Ms)) {
        $timedOut = $true
        try { $p.Kill() } catch {}
    }
    $p.WaitForExit()

    try {
        [System.Threading.Tasks.Task]::WaitAll(@($stdoutTask, $stderrTask), 2000) | Out-Null
    } catch {}

    $stdoutStr = if ($stdoutTask.IsCompleted) { $stdoutTask.Result } else { "" }
    $stderrStr = if ($stderrTask.IsCompleted) { $stderrTask.Result } else { "" }

    return [PSCustomObject]@{
        ExitCode = if ($timedOut) { -99 } else { $p.ExitCode }
        Stdout   = $stdoutStr
        Stderr   = $stderrStr
        TimedOut = $timedOut
    }
}

# ─────────────────────────────────────────────
# YARDIMCI: Oracle ve diagnostic sözleşmesi
# ─────────────────────────────────────────────
function Normalize-OracleText {
    param([AllowNull()][string]$Text)
    if ($null -eq $Text) { return "" }
    # Platform satır sonlarını eşitle; yalnızca son satır sonunu göz ardı et.
    return (($Text -replace "`r`n", "`n" -replace "`r", "`n").TrimEnd("`n"))
}

function Read-OracleText {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    return [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path).Path, [System.Text.Encoding]::UTF8)
}

function Get-TextSha256 {
    param([AllowNull()][string]$Text)
    $encoding = New-Object System.Text.UTF8Encoding($false)
    $bytes = $encoding.GetBytes((Normalize-OracleText $Text))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Get-PositiveOracle {
    param([string]$File)

    $stdoutFile = [System.IO.Path]::ChangeExtension($File, ".expected")
    $stderrFile = $File + ".stderr.expected"
    $exitFile = $File + ".exitcode"
    $manifestFile = $File + ".oracle.json"
    $hasStdout = $false
    $hasStderr = $false
    $stdout = $null
    $stderr = ""
    $expectedExit = 0
    $oracleName = $null
    $artifactRequired = $true

    if (Test-Path -LiteralPath $manifestFile) {
        try {
            $manifest = (Read-OracleText $manifestFile) | ConvertFrom-Json
            $oracleName = $manifestFile
            if ($manifest.PSObject.Properties.Name -contains "stdout") {
                $stdout = [string]$manifest.stdout
                $hasStdout = $true
            } elseif ($manifest.PSObject.Properties.Name -contains "stdout_file") {
                $path = [string]$manifest.stdout_file
                if (-not [System.IO.Path]::IsPathRooted($path)) { $path = Join-Path (Split-Path -Parent $File) $path }
                $stdout = Read-OracleText $path
                $hasStdout = ($null -ne $stdout)
            }
            if ($manifest.PSObject.Properties.Name -contains "stderr") {
                $stderr = [string]$manifest.stderr
                $hasStderr = $true
            } elseif ($manifest.PSObject.Properties.Name -contains "stderr_file") {
                $path = [string]$manifest.stderr_file
                if (-not [System.IO.Path]::IsPathRooted($path)) { $path = Join-Path (Split-Path -Parent $File) $path }
                $stderr = Read-OracleText $path
                $hasStderr = ($null -ne $stderr)
            }
            if ($manifest.PSObject.Properties.Name -contains "exit_code") {
                $expectedExit = [int]$manifest.exit_code
            }
            if ($manifest.PSObject.Properties.Name -contains "artifact_required") {
                $artifactRequired = [bool]$manifest.artifact_required
            }
        } catch {
            return [PSCustomObject]@{
                Present = $false; Invalid = $true; File = $manifestFile; Stdout = $null
                Stderr = ""; ExpectedExit = 0; ArtifactRequired = $true
            }
        }
    } else {
        if (Test-Path -LiteralPath $stdoutFile) {
            $stdout = Read-OracleText $stdoutFile
            $hasStdout = $true
            $oracleName = $stdoutFile
        }
        if (Test-Path -LiteralPath $stderrFile) {
            $stderr = Read-OracleText $stderrFile
            $hasStderr = $true
            if ($null -eq $oracleName) { $oracleName = $stderrFile }
        }
        if (Test-Path -LiteralPath $exitFile) {
            try { $expectedExit = [int]((Read-OracleText $exitFile).Trim()) } catch { $expectedExit = 0 }
        }
    }

    return [PSCustomObject]@{
        Present = ($hasStdout -or $hasStderr)
        Invalid = $false
        File = $oracleName
        Stdout = if ($hasStdout) { $stdout } else { "" }
        Stderr = if ($hasStderr) { $stderr } else { "" }
        StdoutDefined = $hasStdout
        StderrDefined = $hasStderr
        ExpectedExit = $expectedExit
        ArtifactRequired = $artifactRequired
    }
}

function Get-RegexMatchCount {
    param([string]$Text, [string]$Pattern)
    if ([string]::IsNullOrEmpty($Pattern)) { return 0 }
    try { return [regex]::Matches($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase).Count }
    catch { return 0 }
}

function Test-DiagnosticLocation {
    param([string]$Text, $Location)
    if ($null -eq $Location) { return $false }
    if (-not ($Location.PSObject.Properties.Name -contains "line") -or
        -not ($Location.PSObject.Properties.Name -contains "column")) { return $false }
    $line = [int]$Location.line
    $column = [int]$Location.column
    $pattern = "(?m)(^|[\r\n])[^\r\n]*:{0}:{1}(?:[^0-9]|$)" -f $line, $column
    return ($Text -match $pattern)
}

# ─────────────────────────────────────────────
# YARDIMCI: Tek test calistirici (derleme + yurutme)
# ─────────────────────────────────────────────
function Get-TestArtifactPath {
    param(
        [string]$File,
        [string]$SuiteName
    )
    # Windows PowerShell 5.1'de Path.GetRelativePath yoktur; URI API'si
    # hem 5.1 hem PowerShell 7 üzerinde kararlı göreli yol üretir.
    $rootFull = [System.IO.Path]::GetFullPath($ROOT)
    $fileFull = [System.IO.Path]::GetFullPath($File)
    $rootUri = New-Object System.Uri(($rootFull.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar))
    $fileUri = New-Object System.Uri($fileFull)
    $relative = [System.Uri]::UnescapeDataString($rootUri.MakeRelativeUri($fileUri).ToString())
    $identity = $SuiteName + "|" + $relative
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($identity)
        $digest = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
    $safeSuite = ($SuiteName -replace "[^a-zA-Z0-9_-]", "_")
    $safeName = ([System.IO.Path]::GetFileNameWithoutExtension($File) -replace "[^a-zA-Z0-9_-]", "_")
    return (Join-Path $TEST_WORK_ROOT ("{0}_{1}_{2}.exe" -f $safeSuite, $safeName, $digest.Substring(0, 16)))
}

function Invoke-SingleTest {
    param(
        [string]$CompilerPath,
        [string]$File,
        [bool]$ShouldFail = $false,
        [string]$SuiteName = ""
    )

    $name = Split-Path $File -Leaf
    $tmpExe = Get-TestArtifactPath -File $File -SuiteName $SuiteName
    Remove-Item -LiteralPath $tmpExe -Force -ErrorAction SilentlyContinue
    $t0 = Get-Date

    # Negative testler için ayrı ve zorunlu diagnostic sözleşmesi.
    if ($ShouldFail) {
        $diagPath = $File + ".diagnostic.json"
        $elapsed = 0
        if (-not (Test-Path -LiteralPath $diagPath)) {
            $script:totalFailed++
            $r = [PSCustomObject]@{
                Suite=$SuiteName; File=$name; Status="FAIL (Missing Diagnostic Oracle)"
                Output="Missing diagnostic oracle: $diagPath"; ExpectedMatch=$false
                DiagnosticContract=$false; OraclePresent=$false; DurationMs=0; TimedOut=$false
            }
            [void]$script:allResults.Add($r)
            return $r
        }

        try {
            $diag = (Read-OracleText $diagPath) | ConvertFrom-Json
        } catch {
            $script:totalFailed++
            $r = [PSCustomObject]@{
                Suite=$SuiteName; File=$name; Status="FAIL (Invalid Diagnostic Oracle)"
                Output=$_.Exception.Message; ExpectedMatch=$false
                DiagnosticContract=$false; OraclePresent=$true; DurationMs=0; TimedOut=$false
            }
            [void]$script:allResults.Add($r)
            return $r
        }

        $cr = Invoke-WithTimeout -ExePath $CompilerPath -Arguments @($File, $tmpExe) -WorkDir $ROOT -Ms ($TimeoutMs * 2)
        $elapsed = ((Get-Date) - $t0).TotalMilliseconds
        $compileText = Normalize-OracleText ([string]($cr.Stdout + $cr.Stderr))
        $artifactExists = Test-Path -LiteralPath $tmpExe
        $expectedExit = if ($diag.PSObject.Properties.Name -contains "exit_code") { [int]$diag.exit_code } else { 1 }
        $actualExit = [int]$cr.ExitCode

        $schemaMatch = (($diag.PSObject.Properties.Name -contains "schema") -and
            ([string]$diag.schema -eq "yukinary.diagnostic-oracle.v1"))
        $codePattern = if ($diag.PSObject.Properties.Name -contains "diagnostic_code") { [string]$diag.diagnostic_code } else { "" }
        $classPattern = if ($diag.PSObject.Properties.Name -contains "error_class") { [string]$diag.error_class } else { "" }
        $messagePattern = if ($diag.PSObject.Properties.Name -contains "message_pattern") { [string]$diag.message_pattern } else { "" }
        $codeMatch = ([string]::IsNullOrEmpty($codePattern) -or $compileText -match [regex]::Escape($codePattern))
        $classMatch = ([string]::IsNullOrEmpty($classPattern) -or $compileText -match $classPattern)
        $messageCount = Get-RegexMatchCount $compileText $messagePattern
        $messageMatch = ($messageCount -gt 0)
        if ($diag.PSObject.Properties.Name -contains "error_count") {
            $messageMatch = $messageMatch -and ($messageCount -eq [int]$diag.error_count)
        }
        $locationMatch = Test-DiagnosticLocation $compileText $diag.location
        $locationRequired = ($null -ne $diag.location)
        $hintPattern = if ($diag.PSObject.Properties.Name -contains "hint_pattern") { [string]$diag.hint_pattern } else { "" }
        $hintMatch = ([string]::IsNullOrEmpty($hintPattern) -or $compileText -match $hintPattern)
        $exitMatch = ($actualExit -eq $expectedExit)
        $artifactRequired = if ($diag.PSObject.Properties.Name -contains "artifact_expected") { [bool]$diag.artifact_expected } else { $false }
        $artifactMatch = ($artifactRequired -and $artifactExists) -or (-not $artifactRequired -and (-not $artifactExists -or $expectedExit -eq 0))
        $contract = ($schemaMatch -and $exitMatch -and $artifactMatch -and $codeMatch -and $classMatch -and $messageMatch -and $hintMatch -and
            ((-not $locationRequired) -or $locationMatch))

        if (-not $KeepArtifacts) {
            Remove-Item -LiteralPath $tmpExe -Force -ErrorAction SilentlyContinue
        }
        if ($contract) { $script:totalPassed++; $stat = "PASS" } else { $script:totalFailed++; $stat = "FAIL (Diagnostic Contract)" }
        $r = [PSCustomObject]@{
            Suite=$SuiteName; File=$name; Status=$stat
            Output=$compileText.Substring(0, [Math]::Min(300, $compileText.Length))
            ExpectedMatch=$contract; OraclePresent=$true; DiagnosticContract=$contract
            DiagnosticOracleSchemaMatch=$schemaMatch; ExpectedExitCode=$expectedExit; ActualExitCode=$actualExit; ExitCodeMatch=$exitMatch
            ArtifactExists=$artifactExists; ArtifactMatch=$artifactMatch
            DiagnosticCode=$codePattern; DiagnosticCodeMatch=$codeMatch
            ErrorClass=$classPattern; ErrorClassMatch=$classMatch
            MessagePattern=$messagePattern; MessageMatch=$messageMatch; MessageCount=$messageCount
            HintPattern=$hintPattern; HintMatch=$hintMatch
            LocationRequired=$locationRequired; LocationMatch=$locationMatch
            DurationMs=[int]$elapsed; TimedOut=$cr.TimedOut
        }
        [void]$script:allResults.Add($r)
        return $r
    }

    $oracle = Get-PositiveOracle $File
    if ($oracle.Invalid -or -not $oracle.Present) {
        $script:totalFailed++
        $reason = if ($oracle.Invalid) { "Invalid oracle" } else { "Missing stdout/stderr oracle" }
        $r = [PSCustomObject]@{
            Suite=$SuiteName; File=$name; Status="FAIL (Missing Oracle)"
            Output=($reason + ": " + [string]$oracle.File); ExpectedMatch=$false
            OraclePresent=$false; DurationMs=0; TimedOut=$false
        }
        [void]$script:allResults.Add($r)
        return $r
    }

    # Derleme: başarılı testte açık artifact üretilmesi zorunludur.
    $cr = Invoke-WithTimeout -ExePath $CompilerPath -Arguments @($File, $tmpExe) -WorkDir $ROOT -Ms ($TimeoutMs * 2)
    $cCode = [int]$cr.ExitCode
    $cOut = Normalize-OracleText ([string]($cr.Stdout + $cr.Stderr))
    $exe = if (Test-Path -LiteralPath $tmpExe) { $tmpExe } else { "" }
    if ($cCode -ne 0 -or $exe -eq "") {
        $elapsed = ((Get-Date) - $t0).TotalMilliseconds
        $script:totalFailed++
        if (-not $KeepArtifacts) {
            Remove-Item -LiteralPath $tmpExe -Force -ErrorAction SilentlyContinue
        }
        $r = [PSCustomObject]@{
            Suite=$SuiteName; File=$name
            Status=if ($cr.TimedOut) { "FAIL (Compile Timeout)" } else { "FAIL (Compile Error)" }
            Output=$cOut.Substring(0, [Math]::Min(300, $cOut.Length)); ExpectedMatch=$false
            OraclePresent=$true; ArtifactExists=($exe -ne ""); CompilerExitCode=$cCode
            DurationMs=[int]$elapsed; TimedOut=$cr.TimedOut
        }
        [void]$script:allResults.Add($r)
        return $r
    }

    $artifactItem = Get-Item -LiteralPath $exe
    $artifactHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    $xr = Invoke-WithTimeout -ExePath $exe -Arguments @() -WorkDir $ROOT -Ms $TimeoutMs
    $elapsed = ((Get-Date) - $t0).TotalMilliseconds
    $actualStdout = Normalize-OracleText ([string]$xr.Stdout)
    $actualStderr = Normalize-OracleText ([string]$xr.Stderr)
    $expectedStdout = Normalize-OracleText ([string]$oracle.Stdout)
    $expectedStderr = Normalize-OracleText ([string]$oracle.Stderr)
    $stdoutMatch = ($actualStdout -ceq $expectedStdout)
    $stderrMatch = ($actualStderr -ceq $expectedStderr)
    $exitMatch = ([int]$xr.ExitCode -eq [int]$oracle.ExpectedExit)
    $expectedMatch = ($stdoutMatch -and $stderrMatch -and $exitMatch -and (-not $xr.TimedOut))
    if (-not $KeepArtifacts) {
        Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue
    }

    if ($xr.TimedOut) { $script:totalFailed++; $stat = "FAIL (Timeout ${TimeoutMs}ms)" }
    elseif (-not $exitMatch) { $script:totalFailed++; $stat = "FAIL (Exit $($xr.ExitCode))" }
    elseif (-not $stdoutMatch -or -not $stderrMatch) { $script:totalFailed++; $stat = "FAIL (Output Mismatch)" }
    else { $script:totalPassed++; $stat = "PASS" }

    $r = [PSCustomObject]@{
        Suite=$SuiteName; File=$name; Status=$stat
        Output=$actualStdout.Substring(0, [Math]::Min(300, $actualStdout.Length))
        Stderr=$actualStderr.Substring(0, [Math]::Min(300, $actualStderr.Length))
        ExpectedOut=$expectedStdout.Substring(0, [Math]::Min(300, $expectedStdout.Length))
        ExpectedStderr=$expectedStderr.Substring(0, [Math]::Min(300, $expectedStderr.Length))
        ExpectedMatch=$expectedMatch; StdoutMatch=$stdoutMatch; StderrMatch=$stderrMatch
        OraclePresent=$true; OracleFile=$oracle.File; ExpectedExitCode=[int]$oracle.ExpectedExit
        ActualExitCode=[int]$xr.ExitCode; ExitCodeMatch=$exitMatch; TimedOut=$xr.TimedOut
        ArtifactRequired=$oracle.ArtifactRequired; ArtifactExists=$true
        ArtifactBytes=[int64]$artifactItem.Length; ArtifactSha256=$artifactHash
        ActualStdoutSha256=(Get-TextSha256 $actualStdout); ActualStderrSha256=(Get-TextSha256 $actualStderr)
        DurationMs=[int]$elapsed
    }
    [void]$script:allResults.Add($r)
    return $r
}

# ─────────────────────────────────────────────
# YARDIMCI: Harici PS1 suite calistirici
# ─────────────────────────────────────────────
function Get-ExternalCaseMarkers {
    param([string]$Text)
    $markers = [System.Collections.Generic.List[object]]::new()
    foreach ($line in (([string]$Text) -split "`r?`n")) {
        # Tüm mevcut suite'ler per-case sonucu bu iki biçimden biriyle basar:
        # [PASS] label veya PASS: label. Özet satırları bilerek sayılmaz.
        if ($line -match '^\s*\[(PASS|FAIL)\]\s*(.*)$') {
            $status = $matches[1].ToUpperInvariant()
            $label = $matches[2].Trim()
        } elseif ($line -match '^\s*(PASS|FAIL):\s*(.*)$') {
            $status = $matches[1].ToUpperInvariant()
            $label = $matches[2].Trim()
        } else {
            continue
        }

        # Mesaj gövdesi compiler'a göre değişse de fixture kimliği değişmez.
        # Harici script'lerdeki "--", "=>" ve "title: açıklama" biçimlerini
        # tek bir kararlı anahtara indiriyoruz.
        if ($label -match '^(.+?)\s+--\s+') { $label = $matches[1].Trim() }
        elseif ($label -match '^(.+?)\s+=>\s+') { $label = $matches[1].Trim() }
        elseif ($label -match '^(.+?):\s+') { $label = $matches[1].Trim() }

        # Self-healing suite'i aynı vakayı compiler sonucuna göre farklı
        # metinlerle yazabiliyor: "compilation failed" / "failed to compile"
        # / "should have aborted" bir fixture kimliği değildir. Ayrıntı
        # noktalaması olsa da bu suffix'leri kaldır.
        $label = $label -replace '(?i)\s+(?:failed\s+to\s+compile|compilation\s+failed|should\s+have\s+aborted)\s*[.!?]*$', ''

        # Self-heal çalışma modu (`with --self-heal` veya
        # `with --kendini-onar`) aynı fixture'ın sonucu değil, invocation
        # ayrıntısıdır. `without --self-heal` senaryosunu özellikle koru;
        # böylece heal açık/kapalı vakaları birbirine karışmaz.
        $label = $label -replace '(?i)\s+with\s+--(?:self-heal|kendini-onar)$', ''
        $label = $label.Trim()
        [void]$markers.Add([PSCustomObject]@{ Status=$status; Label=$label })
    }
    return @($markers)
}

function Invoke-ExternalPS1Suite {
    param(
        [string]$Title,
        [string]$ScriptPath,
        [string[]]$ExtraArgs = @(),
        [switch]$SkipCompiler
    )
    Write-Host "`n--- $Title ---" -ForegroundColor Yellow

    if (-not (Test-Path -LiteralPath $ScriptPath)) {
        Write-Host "  [FAIL] Script bulunamadi: $ScriptPath" -ForegroundColor Red
        $script:totalFailed++
        [void]$script:allResults.Add([PSCustomObject]@{
            Suite=$Title; File="(suite-contract)"; Status="FAIL (Missing Suite)"
            Output="Script bulunamadi: $ScriptPath"; ExpectedMatch=$false
            External=$true; DurationMs=0; TimedOut=$false
        })
        return
    }

    $allArgs = @("-ExecutionPolicy", "Bypass", "-File", $ScriptPath)
    if (-not $SkipCompiler) { $allArgs += @("-Compiler", $Compiler) }
    $allArgs += $ExtraArgs
    $xr = Invoke-WithTimeout -ExePath "powershell.exe" `
        -Arguments $allArgs `
        -WorkDir $ROOT -Ms ([Math]::Max(60000, $TimeoutMs * 20))
    $combined = Normalize-OracleText ([string]($xr.Stdout + $xr.Stderr))
    if ($xr.Stdout) { Write-Host $xr.Stdout }
    if ($xr.Stderr) { Write-Host $xr.Stderr -ForegroundColor DarkGray }

    $markers = @(Get-ExternalCaseMarkers $combined)
    if ($markers.Count -eq 0) {
        $script:totalFailed++
        $reason = if ($xr.TimedOut) { "suite timeout" } elseif ($xr.ExitCode -eq 0) { "suite per-case PASS/FAIL kaydi uretmedi" } else { "suite erken sonlandi (exit $($xr.ExitCode))" }
        Write-Host "  [FAIL] Harici suite oracle sözleşmesi: $reason" -ForegroundColor Red
        [void]$script:allResults.Add([PSCustomObject]@{
            Suite=$Title; File="(suite-contract)"; Status="FAIL (External Contract)"
            Output=$combined.Substring(0, [Math]::Min(300, $combined.Length)); ExpectedMatch=$false
            External=$true; ExternalExitCode=[int]$xr.ExitCode; TimedOut=$xr.TimedOut; DurationMs=0
        })
        return
    }

    $passCount = @($markers | Where-Object { $_.Status -eq "PASS" }).Count
    $failCount = @($markers | Where-Object { $_.Status -eq "FAIL" }).Count
    # Exit code ile marker'lar çelişirse suite seviyesinde ek fail kaydı üret.
    if ($xr.ExitCode -ne 0 -and $failCount -eq 0) {
        [void]$markers.Add([PSCustomObject]@{ Status="FAIL"; Label="suite exit $($xr.ExitCode)" })
        $failCount++
    }

    foreach ($marker in $markers) {
        $status = if ($marker.Status -eq "PASS") { "PASS" } else { "FAIL (External)" }
        if ($marker.Status -eq "PASS") { $script:totalPassed++ } else { $script:totalFailed++ }
        [void]$script:allResults.Add([PSCustomObject]@{
            Suite=$Title; File=($Title + " :: " + $marker.Label); Status=$status
            Output=$marker.Label; ExpectedMatch=($marker.Status -eq "PASS")
            External=$true; ExternalExitCode=[int]$xr.ExitCode; TimedOut=$xr.TimedOut; DurationMs=0
        })
        $color = if ($marker.Status -eq "PASS") { "Green" } else { "Red" }
        Write-Host ("  [{0}] {1}" -f $marker.Status, $marker.Label) -ForegroundColor $color
    }
    Write-Host ("  Harici oracle: {0} PASS / {1} FAIL" -f $passCount, $failCount) -ForegroundColor Cyan
}

# ─────────────────────────────────────────────
# BASLANGIC
# ─────────────────────────────────────────────
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  YUKINARY FULL TEST SUITE -- $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "  Compiler : $Compiler" -ForegroundColor Cyan
Write-Host "  Timeout  : ${TimeoutMs}ms per test" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# ─────────────────────────────────────────────
# INLINE SUITELER (.ykn dosyalari dogrudan)
# ─────────────────────────────────────────────
$sections = @(
    @{ Name = "GENERICS";                          Path = "tests\generics\*.ykn";   Negative = $false },
    @{ Name = "I18N (TR, EN, FR, RU, JA, ZH, KO)"; Path = "tests\i18n\*.ykn";      Negative = $false },
    @{ Name = "NEGATIVE (STRICT TYPECHECKER)";     Path = "tests\negative\*.ykn";   Negative = $true  },
    @{ Name = "OPTIMIZER (CFG/DCE/FOLDING)";       Path = "tests\optimizer\*.ykn";  Negative = $false },
    @{ Name = "STDLIB (MATH, MEM, STR, FILE)";     Path = "tests\stdlib\*.ykn";     Negative = $false },
    @{ Name = "CONFIG (#config / #yapilandir)";     Path = "tests\config\*.ykn";     Negative = $false },
    @{ Name = "LLM & BITNET (GGUF, BPE, KERNELS)"; Path = "tests\llm\*.ykn";       Negative = $false }
)

foreach ($sec in $sections) {
    Write-Host "`n--- $($sec.Name) ---" -ForegroundColor Yellow
    $files = Get-ChildItem (Join-Path $ROOT $sec.Path) -ErrorAction SilentlyContinue
    # Native-DLL PE fixtures have a separate runner because the generic suite
    # intentionally executes each artifact from one shared work directory and
    # does not provision external DLL imports.
    if ($sec.Name -like "LLM*") {
        # These fixtures assert the removed keyword-based prompt generator. They
        # are retained as historical evidence; the explicit rejection contract
        # is exercised by tests/llm/test_stage0_prompt_guard.ps1 below.
        $files = $files | Where-Object { $_.BaseName -notin @(
            "01_prompt_fib", "11_prompt_cli_integration",
            "20_simd_pe", "21_iq2s_simd_pe", "22_q1_simd_pe", "23_f16_simd_pe",
            # These are exercised by dedicated real-model/trainer/sandbox
            # acceptance drivers and intentionally have no stdout oracle.
            "28_reference_ops", "29_tensor_reference", "30_model_llama_facade",
            "34_trainer_real_math", "35_trainer_checkpoint", "36_dataset_batch_tokenizer",
            "37_ykds_trainer_pipeline", "38_ykds_sequence_pack", "39_dataset_manifest",
            "33_yknllm_audit_reader",
            "40_gguf_writer_roundtrip", "41_dataset_generator_acceptance",
            "42_yknllm_sandbox_arena", "43_yknllm_sandbox_boundary",
            "44_yknllm_ast_capability_gate"
        ) }
    }
    if ($sec.Name -like "STDLIB*") {
        # Native socket/HTTP/DNS fixtures are integration programs, not
        # stdout-oracle unit tests. They require real peer provisioning and are
        # executed by their dedicated acceptance drivers. Keeping them in this
        # runner produces false "Missing Oracle" failures and makes parity
        # compare a test-harness classification error rather than compiler
        # behavior.
        $nativeIntegrationFixtures = @(
            "dns_generated_linux_probe", "dns_generated_negative_probe", "dns_generated_probe",
            "framebuffer_reference",
            "http_chunk_parser", "http_chunk_parser_negative",
            "http_native_chunk_client", "http_native_chunk_raw_client", "http_native_chunk_server",
            "http_native_client", "http_native_post_client", "http_native_post_server",
            "http_native_raw_client", "http_native_server", "http_native_split_client",
            "http_native_split_server", "http_reference", "http_response_parser",
            "network_syscall_probe", "network_syscall_raw_probe",
            "socket_nonblocking_probe_client", "socket_nonblocking_probe_server",
            "socket_timeout_probe_client", "socket_timeout_probe_server",
            "windows_http_debug_client", "windows_socket_client_debug", "windows_socket_debug",
            # Arc has no .expected oracle; its PE/ELF/hash/exit gate is dedicated.
            "arc_smart_pointer",
            # GLFW GUI window fixture requires glfw3.dll runtime
            "test_native_std_faz16"
        )
        $files = $files | Where-Object { $_.BaseName -notin $nativeIntegrationFixtures }
    }
    if (-not $files) { Write-Host "  (dosya bulunamadi)" -ForegroundColor DarkGray; continue }
    foreach ($f in $files) {
        $r = Invoke-SingleTest -CompilerPath $Compiler -File $f.FullName -ShouldFail $sec.Negative -SuiteName $sec.Name
        $color = if ($r.Status -like "PASS*") { "Green" } elseif ($r.Status -like "FAIL*") { "Red" } else { "Yellow" }
        Write-Host ("  [{0,-26}] {1}" -f $r.Status, $r.File) -ForegroundColor $color
        if ($r.Status -like "FAIL*" -and $null -ne $r.ExpectedOut) {
            $expShort = $r.ExpectedOut; if ($expShort.Length -gt 80) { $expShort = $expShort.Substring(0,80) }
            $outShort = $r.Output;      if ($outShort.Length -gt 80) { $outShort = $outShort.Substring(0,80) }
            Write-Host ("    Beklenen : {0}" -f $expShort) -ForegroundColor DarkGray
            Write-Host ("    Alinan   : {0}" -f $outShort) -ForegroundColor DarkGray
        }
    }
}

# ─────────────────────────────────────────────
# HARICI PS1 SUITELER (Python YOK)
# ─────────────────────────────────────────────
Invoke-ExternalPS1Suite "SELF-HEALING (TYPECHECKER TUNNEL)" `
    (Join-Path $ROOT "tests\negative\test_self_heal.ps1")

Invoke-ExternalPS1Suite "CROSS-PLATFORM (LINUX ELF64)" `
    (Join-Path $ROOT "tests\crossplatform\test_linux_elf.ps1") `
    @("-WorkRoot", $TEST_WORK_ROOT)

Invoke-ExternalPS1Suite "CROSS-PLATFORM (MACOS MACH-O 64)" `
    (Join-Path $ROOT "tests\crossplatform\test_macos_macho.ps1") `
    @("-WorkRoot", $TEST_WORK_ROOT)

Invoke-ExternalPS1Suite "CROSS-PLATFORM (ARM64 ELF & MACH-O)" `
    (Join-Path $ROOT "tests\crossplatform\test_arm64_elf.ps1") `
    @("-WorkRoot", $TEST_WORK_ROOT)

Invoke-ExternalPS1Suite "FAZ 9 FFI (#c-lib, extern isv, IAT)" `
    (Join-Path $ROOT "tests\ffi\test_ffi.ps1")

Invoke-ExternalPS1Suite "STAGE-1 SELF-HOST ENTRY POINT (ana)" `
    (Join-Path $ROOT "tests\bootstrap\test_stage1_selfhost.ps1")

Invoke-ExternalPS1Suite "FAZ 14 BELLEK GUVENLIGI (Bolge/Affine/Sinir)" `
    (Join-Path $ROOT "tests\guvenlik\run_guvenlik.ps1")

Invoke-ExternalPS1Suite "LLM STAGE-0 SAHTE PROMPT GUARD" `
    (Join-Path $ROOT "tests\llm\test_stage0_prompt_guard.ps1") `
    @() -SkipCompiler

Invoke-ExternalPS1Suite "LLM YKNA TRUNCATION RECOVERY" `
    (Join-Path $ROOT "tests\llm\test_yknllm_audit_recovery.ps1")

# ─────────────────────────────────────────────
# JSON CIKTISI
# ─────────────────────────────────────────────
$elapsed = ((Get-Date) - $ScriptStart).TotalSeconds
$totalTests = $totalPassed + $totalFailed
$thresholdTarget = if ($ThresholdMode -eq "Percent") {
    [int][math]::Ceiling($totalTests * ($MinPassThreshold / 100.0))
} else {
    [int][math]::Ceiling($MinPassThreshold)
}
$thresholdOk = ($totalPassed -ge $thresholdTarget)
$resultCount = @($allResults).Count
$reportConsistency = ($resultCount -eq $totalTests)

$report = [PSCustomObject]@{
    schema         = "yukinary.test-report.v2"
    timestamp      = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
    compiler       = $Compiler
    compiler_path_resolved = $compilerResolved
    compiler_sha256 = $compilerSha256
    source_revision_sha256 = $sourceRevisionSha256
    run_id         = $RUN_ID
    artifact_root  = $TEST_WORK_ROOT
    keep_artifacts = [bool]$KeepArtifacts
    timeout_ms     = $TimeoutMs
    total_passed   = $totalPassed
    total_failed   = $totalFailed
    total_tests    = $totalTests
    result_count   = $resultCount
    pass_rate      = if ($totalTests -gt 0) { [math]::Round($totalPassed / $totalTests * 100, 1) } else { 0 }
    duration_sec   = [math]::Round($elapsed, 2)
    threshold      = $MinPassThreshold
    threshold_mode = $ThresholdMode
    threshold_target = $thresholdTarget
    threshold_ok   = $thresholdOk
    report_consistent = $reportConsistency
    oracle_contract = "yukinary.test-oracle.v2"
    diagnostic_oracle = [ordered]@{
        schema = "yukinary.diagnostic-oracle.v1"
        negative_requires = @("actual_exit_code", "status", "no_artifact_when_required")
        positive_requires = @("actual_exit_code", "status", "artifact_exists_when_required")
    }
    results        = @($allResults)
}

$jsonOut = if ([System.IO.Path]::IsPathRooted($OutputJson)) {
    $OutputJson
} else {
    Join-Path $ROOT $OutputJson
}
$jsonParent = Split-Path -Parent $jsonOut
New-Item -ItemType Directory -Path $jsonParent -Force | Out-Null
$jsonText = $report | ConvertTo-Json -Depth 10
[System.IO.File]::WriteAllText($jsonOut, $jsonText, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "`n[JSON] Sonuclar kaydedildi: $jsonOut" -ForegroundColor DarkCyan

# ─────────────────────────────────────────────
# COVERAGE MATRISI
# ─────────────────────────────────────────────
if (-not $NoCoverage) {
    Write-Host "`n--- COVERAGE MATRISI (AST Node Tipleri) ---" -ForegroundColor Yellow

    # AST dugum tipi tanimlari
    $astNodes = [ordered]@{
        0  = "AST_BOS"
        1  = "AST_SAYI"
        2  = "AST_GERCEK_SAYI"
        3  = "AST_METIN"
        4  = "AST_KIMLIK"
        5  = "AST_IKILI_OP"
        6  = "AST_ATAMA"
        7  = "AST_DEGISKEN_TANIMI"
        8  = "AST_EGER"
        9  = "AST_DONGU"
        10 = "AST_ISLEV"
        11 = "AST_ISLEV_CAGRISI"
        12 = "AST_DONDUR"
        13 = "AST_BLOK"
        14 = "AST_DURDUR"
        15 = "AST_DEVAM_ET"
        16 = "AST_YAPI_TANIMI"
        17 = "AST_ALAN_ERISIMI"
        18 = "AST_INDEKS_ERISIMI"
        19 = "AST_UNARY"
        20 = "AST_REFERANS"
        21 = "AST_ICE_AKTAR"
        22 = "AST_MAKRO_TANIMI"
        23 = "AST_MAKRO_CAGRISI"
        24 = "AST_GENERIC_TANIM"
        25 = "AST_TRAIT_TANIM"
        26 = "AST_MANTIK"
        27 = "AST_DIREKTIF_PROMPT"
        28 = "AST_DIREKTIF_AI"
        29 = "AST_DIREKTIF_C_LIB"
        30 = "AST_DIREKTIF_CPP_LIB"
        31 = "AST_DIREKTIF_PY_LIB"
        32 = "AST_DIREKTIF_RS_LIB"
        33 = "AST_SORU_ISLEMI"
        34 = "AST_SONUC_TANIM"
    }

    # YKN syntax -> AST node pattern haritasi
    $yknSyntaxMap = [ordered]@{
        0  = @()
        1  = @("[0-9]+")
        2  = @("[0-9]+\.[0-9]+")
        3  = @('"')
        4  = @("[a-zA-Z_][a-zA-Z0-9_]+")
        5  = @("\+","\-","==","!=","<",">","ve ","veya ")
        6  = @(" = ")
        7  = @("degisken ")
        8  = @("eger ")
        9  = @("dongu ")
        10 = @("isv ")
        11 = @("[a-z_]\w*\(")
        12 = @("dondu ")
        13 = @("\{")
        14 = @("durdur")
        15 = @("devam")
        16 = @("yapi ")
        17 = @("\.\w+")
        18 = @("\[\w")
        19 = @("!\w+","\*\w+")
        20 = @("&\w+")
        21 = @("ice_aktar")
        22 = @("makro ")
        23 = @("@[a-z]")
        24 = @("<[A-Z]")
        25 = @("trait ")
        26 = @("dogru","yanlis")
        27 = @("#prompt","#ai")
        28 = @("#yapay_zeka")
        29 = @("#c-lib")
        30 = @("#cpp-lib")
        31 = @("#py-lib")
        32 = @("#rs-lib")
        33 = @("\?")
        34 = @("Sonuc<","sonuc<")
    }

    # Codegen dosyalari
    $codegenFiles = @()
    $codegenFiles += Get-ChildItem (Join-Path $ROOT "src\codegen*.ykn") -ErrorAction SilentlyContinue
    $codegenDir = Join-Path $ROOT "src\codegen"
    if (Test-Path $codegenDir) {
        $codegenFiles += Get-ChildItem $codegenDir -Recurse -Filter "*.ykn" -ErrorAction SilentlyContinue
    }
    $codegenContent = ""
    foreach ($f in $codegenFiles) {
        $codegenContent += (Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue)
    }

    # Test .ykn dosyalari
    $testYknFiles = Get-ChildItem (Join-Path $ROOT "tests") -Recurse -Filter "*.ykn" -ErrorAction SilentlyContinue
    $testContent = ""
    foreach ($f in $testYknFiles) {
        $testContent += (Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue)
    }

    $coverageItems = [System.Collections.Generic.List[object]]::new()
    $bothCovered = 0
    $codegenOnly = 0
    $testOnly    = 0
    $neitherCov  = 0

    foreach ($id in $astNodes.Keys) {
        $nodeName = $astNodes[$id]
        $patterns = $yknSyntaxMap[$id]

        $inCodegen = $codegenContent -match $nodeName

        $inTests = $false
        foreach ($pat in $patterns) {
            if ($testContent -match $pat) { $inTests = $true; break }
        }

        if ($inCodegen -and $inTests)    { $cov = "[OK] Codegen+Test"; $bothCovered++; $color = "Green" }
        elseif ($inCodegen)              { $cov = "[WARN] Codegen var, test yok"; $codegenOnly++; $color = "Yellow" }
        elseif ($inTests)                { $cov = "[WARN] Test var, codegen eksik"; $testOnly++; $color = "Red" }
        else                             { $cov = "[INFO] Henuz kullanilmamis"; $neitherCov++; $color = "DarkGray" }

        $item = [PSCustomObject]@{
            id         = $id
            name       = $nodeName
            in_codegen = $inCodegen
            in_tests   = $inTests
            coverage   = $cov
        }
        $coverageItems.Add($item)

        Write-Host ("  [{0,2}] {1,-32} {2}" -f $id, $nodeName, $cov) -ForegroundColor $color
    }

    Write-Host ""
    Write-Host ("  Codegen + Test : {0}/{1}" -f $bothCovered, $astNodes.Count) -ForegroundColor Green
    Write-Host ("  Sadece Codegen : {0}/{1}" -f $codegenOnly, $astNodes.Count) -ForegroundColor Yellow
    Write-Host ("  Sadece Test    : {0}/{1}" -f $testOnly,    $astNodes.Count) -ForegroundColor Red
    Write-Host ("  Hicbiri        : {0}/{1}" -f $neitherCov,  $astNodes.Count) -ForegroundColor DarkGray

    # Coverage JSON kaydet
    $coverageJson = Join-Path $ROOT "build\reports\coverage_matrix.json"
    New-Item -ItemType Directory -Path (Split-Path -Parent $coverageJson) -Force | Out-Null
    [PSCustomObject]@{
        timestamp       = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
        total_nodes     = $astNodes.Count
        codegen_covered = ($coverageItems | Where-Object { $_.in_codegen } | Measure-Object).Count
        test_covered    = ($coverageItems | Where-Object { $_.in_tests }   | Measure-Object).Count
        both_covered    = $bothCovered
        codegen_only    = $codegenOnly
        test_only       = $testOnly
        neither         = $neitherCov
        nodes           = @($coverageItems)
    } | ConvertTo-Json -Depth 10 | Set-Content $coverageJson -Encoding UTF8
    Write-Host "`n[JSON] Coverage matrisi kaydedildi: $coverageJson" -ForegroundColor DarkCyan
}

# ─────────────────────────────────────────────
# OZET
# ─────────────────────────────────────────────
Write-Host "`n==========================================" -ForegroundColor Cyan
Write-Host "  TEST SONUCLARI & OZET" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Gecen Testler   : $totalPassed" -ForegroundColor Green
Write-Host "Basarisiz       : $totalFailed" -ForegroundColor $(if ($totalFailed -eq 0) { "Green" } else { "Red" })
Write-Host "Toplam/Report   : $totalTests / $resultCount" -ForegroundColor Cyan
Write-Host "Esik            : $MinPassThreshold ($ThresholdMode => hedef $thresholdTarget)" -ForegroundColor Yellow
Write-Host ("Sure            : {0:F1} saniye" -f $elapsed)
Write-Host "JSON Raporu     : $OutputJson"

if (-not $reportConsistency) {
    Write-Host "`n[REGRESYON HATASI] JSON result_count ($resultCount) terminal sayaciyla ($totalTests) esit degil!" -ForegroundColor Red
    exit 1
} elseif ($totalFailed -gt 0) {
    Write-Host "`n[REGRESYON HATASI] $totalFailed test basarisiz; esik modu diagnostic olsa bile CI yesil ilan edilemez." -ForegroundColor Red
    exit 1
} elseif (-not $thresholdOk) {
    Write-Host "`n[REGRESYON HATASI] Gecen test ($totalPassed) < esik hedefi ($thresholdTarget)!" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`n[CI BASARILI] Test suite esigi gecti ($totalPassed >= $thresholdTarget) ve rapor tutarli." -ForegroundColor Green
    exit 0
}
