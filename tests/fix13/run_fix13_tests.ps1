# tests/fix13/run_fix13_tests.ps1
# FIX-13 Null Safety regression suite.
# The negative oracle is intentionally strict: E033, codegen class and no artifact.

param(
    [string]$Compiler = "build/bootstrap/stage0.exe",
    [string]$OutputJson = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path (Split-Path $PSScriptRoot)
$compilerPath = if ([IO.Path]::IsPathRooted($Compiler)) { $Compiler } else { Join-Path $root $Compiler }
$compilerResolved = [IO.Path]::GetFullPath($compilerPath)

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

$compilerSha256 = Get-Sha256 $compilerResolved
$sourceRevisionSha256 = Get-CanonicalSourceRevision $root
$runId = "fix13-{0}-{1}" -f (Get-Date -Format "yyyyMMdd-HHmmss"), ([guid]::NewGuid().ToString("N").Substring(0, 8))
$artifactRoot = Join-Path $root (Join-Path "build/test-work/runs" $runId)
$reportPath = if ([string]::IsNullOrWhiteSpace($OutputJson)) {
    Join-Path $root (Join-Path "build/reports" ("fix13_results_{0}.json" -f $runId))
} elseif ([IO.Path]::IsPathRooted($OutputJson)) {
    $OutputJson
} else {
    Join-Path $root $OutputJson
}
New-Item -ItemType Directory -Force -Path $artifactRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $reportPath) | Out-Null

if (-not (Test-Path -LiteralPath $compilerPath)) {
    Write-Error "Derleyici bulunamadi: $compilerPath"
    exit 1
}

$results = @()
$passed = 0
$failed = 0

function Invoke-Fix13Case {
    param(
        [string]$Name,
        [string]$SourcePath,
        [bool]$ExpectSuccess,
        [bool]$RunBinary = $false
    )

    $artifact = Join-Path $artifactRoot ($Name + ".exe")
    if (Test-Path -LiteralPath $artifact) { Remove-Item -Force -LiteralPath $artifact }
    $output = @(& $compilerPath $SourcePath $artifact 2>&1)
    $exitCode = $LASTEXITCODE
    $text = ($output -join "`n")
    $artifactExists = Test-Path -LiteralPath $artifact
    $hasE033 = $text -match "\[E033\]"
    $hasDiagnosticName = $text -match "ERR_NULLABLE_ACCESS_WITHOUT_CHECK"
    $hasClass = $text -match "\[class=codegen\]"
    $binaryExit = $null
    $binaryOutput = ""

    if ($ExpectSuccess -and $RunBinary -and $exitCode -eq 0 -and $artifactExists) {
        $binaryOutput = ((& $artifact 2>&1) -join "`n")
        $binaryExit = $LASTEXITCODE
    }

    if ($ExpectSuccess) {
        $ok = ($exitCode -eq 0 -and $artifactExists)
        $reason = if ($ok) { "compile success + artifact" } else { "expected success, exit=$exitCode, artifact=$artifactExists" }
    } else {
        $ok = ($exitCode -ne 0 -and -not $artifactExists -and $hasE033 -and $hasDiagnosticName -and $hasClass)
        $reason = if ($ok) { "exact E033/codegen diagnostic + no artifact" } else {
            "expected strict E033 failure, exit=$exitCode, artifact=$artifactExists, E033=$hasE033, name=$hasDiagnosticName, class=$hasClass"
        }
    }

    if ($ok) {
        $script:passed++
        Write-Host "  [GECTI] $Name - $reason" -ForegroundColor Green
    } else {
        $script:failed++
        Write-Host "  [KALDI] $Name - $reason" -ForegroundColor Red
    }

    $script:results += [ordered]@{
        suite = "FIX13"
        file = $SourcePath
        status = if ($ok) { "PASS" } else { "FAIL" }
        name = $Name
        source = $SourcePath
        output = $text
        stderr = ""
        expected_success = $ExpectSuccess
        expect_success = $ExpectSuccess
        compiler_exit_code = $exitCode
        actual_exit_code = $exitCode
        artifact_required = $ExpectSuccess
        oracle_present = $true
        artifact_exists = $artifactExists
        diagnostic_code = if ($hasE033 -and $hasDiagnosticName) { "ERR_NULLABLE_ACCESS_WITHOUT_CHECK" } else { $null }
        error_class = if ($hasClass) { "codegen" } else { $null }
        strict_oracle_passed = $ok
        binary_exit_code = $binaryExit
        compiler_output = $text
    }
}

Write-Host ""
Write-Host "=== FIX-13 Null Guvenligi Test Suiti ===" -ForegroundColor Magenta
Write-Host "Compiler: $compilerPath"
Write-Host "Run: $runId"
Write-Host ""

# Lexer/parser and declaration smoke cases.
$t1 = Join-Path $artifactRoot "bos_literal.ykn"
@'
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {
    degisken p: tamsayi = 0;
    eger p == bos ise { ekrana_yaz("bos calisio\n".ptr); }
    dondu 0;
}
'@ | Set-Content -LiteralPath $t1 -Encoding UTF8
Invoke-Fix13Case "01_bos_literal" $t1 $true

$t2 = Join-Path $artifactRoot "nil_literal.ykn"
@'
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {
    degisken q: tamsayi = 0;
    eger q == nil ise { ekrana_yaz("nil calisio\n".ptr); }
    dondu 0;
}
'@ | Set-Content -LiteralPath $t2 -Encoding UTF8
Invoke-Fix13Case "02_nil_alias" $t2 $true

$t3 = Join-Path $artifactRoot "nullable_decl.ykn"
@'
isv ana(argc: tamsayi, argv: **u8) -> tamsayi {
    degisken p: ?*u8 = bos;
    eger p != bos ise { ekrana_yaz("gecerli\n".ptr); }
    yoksa { ekrana_yaz("bos\n".ptr); }
    dondu 0;
}
'@ | Set-Content -LiteralPath $t3 -Encoding UTF8
Invoke-Fix13Case "03_nullable_declaration" $t3 $true $true

# Data-flow narrowing and rejection cases.
Invoke-Fix13Case "04_no_check" (Join-Path $PSScriptRoot "..\negative\07_nullable_index_without_check.ykn") $false
Invoke-Fix13Case "05_index_guard" (Join-Path $PSScriptRoot "nullable_index_guard.ykn") $true
Invoke-Fix13Case "06_truthy_guard" (Join-Path $PSScriptRoot "nullable_index_truthy.ykn") $true
Invoke-Fix13Case "07_null_branch" (Join-Path $PSScriptRoot "nullable_index_null_branch.ykn") $false
Invoke-Fix13Case "08_scope_restore" (Join-Path $PSScriptRoot "nullable_index_scope_restore.ykn") $false
Invoke-Fix13Case "09_field_no_check" (Join-Path $PSScriptRoot "nullable_field_no_check.ykn") $false
Invoke-Fix13Case "10_field_guard" (Join-Path $PSScriptRoot "nullable_field_guard.ykn") $true
Invoke-Fix13Case "11_unrelated_guard" (Join-Path $PSScriptRoot "nullable_unrelated_guard.ykn") $false
Invoke-Fix13Case "12_reassigned_inside_guard" (Join-Path $PSScriptRoot "nullable_reassigned_inside_guard.ykn") $false
Invoke-Fix13Case "13_null_aliases_positive" (Join-Path $PSScriptRoot "null_aliases_positive.ykn") $true
Invoke-Fix13Case "14_nested_guard" (Join-Path $PSScriptRoot "nullable_nested_guard.ykn") $true
Invoke-Fix13Case "15_two_pointer_guards" (Join-Path $PSScriptRoot "nullable_two_pointer_guards.ykn") $true
Invoke-Fix13Case "16_else_narrowing" (Join-Path $PSScriptRoot "nullable_else_narrowing.ykn") $true
Invoke-Fix13Case "17_non_nullable_access" (Join-Path $PSScriptRoot "non_nullable_pointer_access.ykn") $true
Invoke-Fix13Case "18_nul_initializer" (Join-Path $PSScriptRoot "nullable_nul_initializer.ykn") $true
Invoke-Fix13Case "19_null_initializer" (Join-Path $PSScriptRoot "nullable_null_initializer.ykn") $true
Invoke-Fix13Case "20_second_pointer_no_check" (Join-Path $PSScriptRoot "nullable_second_pointer_no_check.ykn") $false
Invoke-Fix13Case "21_and_narrowing" (Join-Path $PSScriptRoot "nullable_and_narrowing.ykn") $true
Invoke-Fix13Case "22_or_same_pointer" (Join-Path $PSScriptRoot "nullable_or_same_pointer.ykn") $true
Invoke-Fix13Case "23_or_two_pointers" (Join-Path $PSScriptRoot "nullable_or_two_pointers.ykn") $false
Invoke-Fix13Case "24_nullable_parameter_guard" (Join-Path $PSScriptRoot "nullable_parameter_guard.ykn") $true
Invoke-Fix13Case "25_nullable_parameter_no_check" (Join-Path $PSScriptRoot "nullable_parameter_no_check.ykn") $false
Invoke-Fix13Case "26_nullable_return_guard" (Join-Path $PSScriptRoot "nullable_return_guard.ykn") $true
Invoke-Fix13Case "27_nullable_return_no_check" (Join-Path $PSScriptRoot "nullable_return_no_check.ykn") $false
# Pending: call invalidation requires branch-local FlowState and a shared
# Stage-0/Stage-3 argument identity representation.
Invoke-Fix13Case "29_nullable_pointer_to_pointer_guard" (Join-Path $PSScriptRoot "nullable_pointer_to_pointer_guard.ykn") $true
Invoke-Fix13Case "30_nullable_pointer_to_pointer_no_check" (Join-Path $PSScriptRoot "nullable_pointer_to_pointer_no_check.ykn") $false
Invoke-Fix13Case "31_loop_guard" (Join-Path $PSScriptRoot "31_loop_guard.ykn") $true
Invoke-Fix13Case "32_loop_no_check" (Join-Path $PSScriptRoot "32_loop_no_check.ykn") $false
Invoke-Fix13Case "33_loop_scope_restore" (Join-Path $PSScriptRoot "33_loop_scope_restore.ykn") $false
Invoke-Fix13Case "34_struct_field_guard" (Join-Path $PSScriptRoot "nullable_struct_field_guard.ykn") $true
Invoke-Fix13Case "35_struct_field_no_check" (Join-Path $PSScriptRoot "nullable_struct_field_no_check.ykn") $false

$report = [ordered]@{
    schema = "yukinary.fix13-results.v2"
    oracle_contract = "yukinary.test-oracle.v2"
    diagnostic_oracle = [ordered]@{
        schema = "yukinary.diagnostic-oracle.v1"
        negative_requires = @("diagnostic_code", "error_class", "no_artifact")
        positive_requires = @("actual_exit_code", "artifact_exists")
    }
    run_id = $runId
    compiler = $compilerPath
    compiler_path_resolved = $compilerResolved
    compiler_sha256 = $compilerSha256
    source_revision_sha256 = $sourceRevisionSha256
    artifact_root = $artifactRoot
    passed = $passed
    failed = $failed
    total = $results.Count
    results = $results
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8

Write-Host ""
Write-Host "Report: $reportPath"
if ($failed -eq 0) {
    Write-Host "=== FIX-13 GECTI: $passed/$($results.Count) ===" -ForegroundColor Green
    exit 0
}
Write-Host "=== FIX-13 KALDI: $passed/$($results.Count) ===" -ForegroundColor Red
exit 1
