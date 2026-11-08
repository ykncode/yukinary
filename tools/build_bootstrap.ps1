# ============================================================
# build_bootstrap.ps1 - Yukinary canonical bootstrap pipeline
#
# Builds and verifies:
#   app_full.cpp -> Stage-0 -> Stage-1 -> Stage-2 -> Stage-3
#
# All generated files stay under build/. The checked-in YKN sources are
# fingerprinted and every stage is exercised with the same smoke cases.
# ============================================================

[CmdletBinding()]
param(
    [string]$Cxx = "",
    [ValidateSet("O0", "O1", "O2", "O3", "Os")]
    [string]$Optimization = "O2",
    [switch]$SkipBootstrapBuild,
    [switch]$KeepArtifacts,
    [switch]$VerboseOutput
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$MANIFEST_PATH = Join-Path $PSScriptRoot "compiler-manifest.json"

$runStatus = "FAIL"
$failureMessage = $null
$reportPath = $null
$logRoot = $null
$verifyRoot = $null
$smokeRoot = $null
$stagePaths = [ordered]@{}
$stageArtifacts = @()
$stageBuildRecords = @()
$cppBuildRecords = @()
$programRunRecords = @()
$smokeRecords = @()
$sourceRecords = @()
$canonicalSourceHash = $null
$bootstrapSeedHash = $null
$launcherSourceHash = $null
$cppReproducible = $false
$stageReproducibility = [ordered]@{}
$smokeAllPassed = $false
$smokeCaseCount = 0
$cxxPath = $null
$cxxVersion = $null
$manifest = $null
$entryPath = $null
$sourceRoot = $null
$bootstrapSeedPath = $null
$launcherPath = $null
$artifactRoot = $null
$smokeManifestPath = $null

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

function Convert-ToReportPath([string]$Path) {
    if ([string]::IsNullOrEmpty($Path)) {
        return $Path
    }

    try {
        $candidate = $Path
        if (-not [System.IO.Path]::IsPathRooted($candidate)) {
            $candidate = [System.IO.Path]::GetFullPath((Join-Path $ROOT $candidate))
        } else {
            $candidate = [System.IO.Path]::GetFullPath($candidate)
        }
        $rootPrefix = $ROOT.TrimEnd("\", "/") + "\"
        if ($candidate.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $candidate.Substring($rootPrefix.Length).Replace("\", "/")
        }
    } catch {
        # Keep non-path compiler arguments unchanged.
    }
    return $Path.Replace("\", "/")
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-TextSha256([string]$Text) {
    $encoding = New-Object System.Text.UTF8Encoding($false)
    $bytes = $encoding.GetBytes($Text)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Read-Text([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }
    return [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path).Path, [System.Text.Encoding]::UTF8)
}

function Normalize-Newlines([string]$Text) {
    return ($Text -replace "`r`n", "`n" -replace "`r", "`n")
}

function Get-LogInfo([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return [ordered]@{
            path = Convert-ToReportPath $Path
            bytes = 0
            sha256 = $null
        }
    }
    return [ordered]@{
        path = Convert-ToReportPath $Path
        bytes = (Get-Item -LiteralPath $Path).Length
        sha256 = Get-Sha256 $Path
    }
}

function Invoke-Captured {
    param(
        [string]$File,
        [string[]]$Arguments = @(),
        [string]$Name,
        [string]$WorkingDirectory = $ROOT
    )

    Ensure-Directory $logRoot
    $stdoutPath = Join-Path $logRoot ($Name + ".stdout")
    $stderrPath = Join-Path $logRoot ($Name + ".stderr")
    Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

    $exitCode = -1
    $thrown = $null
    if ($VerboseOutput) {
        $displayArgs = ($Arguments | ForEach-Object {
            $text = [string]$_
            if ($text -match '[\s"]') { '"' + $text.Replace('"', '\"') + '"' } else { $text }
        }) -join " "
        Write-Host ("[>>] " + (Convert-ToReportPath $File) + " " + $displayArgs) -ForegroundColor Yellow
    }

    Push-Location $WorkingDirectory
    try {
        & $File @Arguments 1> $stdoutPath 2> $stderrPath
        $exitCode = $LASTEXITCODE
    } catch {
        $thrown = $_.Exception.Message
        [System.IO.File]::WriteAllText($stderrPath, $thrown, (New-Object System.Text.UTF8Encoding($false)))
        $exitCode = -1
    } finally {
        Pop-Location
    }

    $record = [ordered]@{
        name = $Name
        file = Convert-ToReportPath $File
        arguments = @($Arguments | ForEach-Object { Convert-ToReportPath ([string]$_) })
        working_directory = Convert-ToReportPath $WorkingDirectory
        exit_code = $exitCode
        exception = $thrown
        stdout = Get-LogInfo $stdoutPath
        stderr = Get-LogInfo $stderrPath
    }
    return $record
}

function Assert-Artifact([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw ($Label + ": artifact not found: " + (Convert-ToReportPath $Path))
    }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -le 0) {
        throw ($Label + ": artifact is empty: " + (Convert-ToReportPath $Path))
    }
    return $item
}

function Invoke-YknCompile {
    param(
        [string]$Compiler,
        [string]$SourceArgument,
        [string]$OutputPath,
        [string]$Name
    )

    Remove-Item -LiteralPath $OutputPath -Force -ErrorAction SilentlyContinue
    # Older generated stages use these names when an explicit output cannot
    # be honored. Quarantine any pre-existing files instead of deleting them.
    $rootFallbacks = @(
        (Join-Path $ROOT "out.exe"),
        (Join-Path $ROOT "cikti.exe")
    )
    $preservedFallbacks = @()
    Ensure-Directory $verifyRoot
    foreach ($fallback in $rootFallbacks) {
        if (Test-Path -LiteralPath $fallback) {
            $quarantine = Join-Path $verifyRoot ("preexisting-" + $Name + "-" + [System.IO.Path]::GetFileName($fallback))
            Remove-Item -LiteralPath $quarantine -Force -ErrorAction SilentlyContinue
            Move-Item -LiteralPath $fallback -Destination $quarantine -Force
            $preservedFallbacks += [ordered]@{
                original = $fallback
                quarantine = $quarantine
            }
        }
    }

    try {
        $record = Invoke-Captured -File $Compiler `
            -Arguments @($SourceArgument, $OutputPath) `
            -Name $Name

        if (-not (Test-Path -LiteralPath $OutputPath)) {
            foreach ($fallback in $rootFallbacks) {
                if (Test-Path -LiteralPath $fallback) {
                    Move-Item -LiteralPath $fallback -Destination $OutputPath -Force
                    break
                }
            }
        }

        $record["output"] = Convert-ToReportPath $OutputPath
        $record["output_exists"] = Test-Path -LiteralPath $OutputPath
        if ($record["output_exists"]) {
            $record["output_bytes"] = (Get-Item -LiteralPath $OutputPath).Length
            $record["output_sha256"] = Get-Sha256 $OutputPath
        } else {
            $record["output_bytes"] = 0
            $record["output_sha256"] = $null
        }

        Remove-Item -LiteralPath $rootFallbacks -Force -ErrorAction SilentlyContinue

        if ($record["exit_code"] -ne 0) {
            throw ($Name + ": compiler exited with " + $record["exit_code"] +
                " (see " + $record["stderr"]["path"] + ")")
        }
        Assert-Artifact $OutputPath $Name | Out-Null
        return $record
    } finally {
        # Restore user files after this invocation, even when compilation fails.
        foreach ($saved in $preservedFallbacks) {
            Remove-Item -LiteralPath $saved.original -Force -ErrorAction SilentlyContinue
            if (Test-Path -LiteralPath $saved.quarantine) {
                Move-Item -LiteralPath $saved.quarantine -Destination $saved.original -Force
            }
        }
    }
}

function Invoke-Program {
    param(
        [string]$Executable,
        [string]$Name
    )
    $record = Invoke-Captured -File $Executable -Arguments @() -Name $Name
    $record["stdout_text"] = Read-Text (Join-Path $logRoot ($Name + ".stdout"))
    $record["stderr_text"] = Read-Text (Join-Path $logRoot ($Name + ".stderr"))
    return $record
}

try {
    if (-not (Test-Path -LiteralPath $MANIFEST_PATH)) {
        throw ("compiler manifest not found: " + $MANIFEST_PATH)
    }
    $manifestText = [System.IO.File]::ReadAllText($MANIFEST_PATH, [System.Text.Encoding]::UTF8)
    $manifest = $manifestText | ConvertFrom-Json
    if ([string]$manifest.schema -ne "yukinary.compiler-manifest.v1") {
        throw "unsupported compiler manifest schema"
    }

    $entryPath = Get-RootPath ([string]$manifest.canonical.entry)
    $sourceRoot = Get-RootPath ([string]$manifest.canonical.source_root)
    $bootstrapSeedPath = Get-RootPath ([string]$manifest.bootstrap_seed.source)
    $launcherPath = Get-RootPath ([string]$manifest.launcher.source)
    $reportPath = Get-RootPath ([string]$manifest.build.report)
    $artifactRoot = Get-RootPath ([string]$manifest.build.artifact_root)
    $logRoot = Get-RootPath ([string]$manifest.build.logs)
    $verifyRoot = Join-Path $artifactRoot "verify"
    $smokeRoot = Join-Path $artifactRoot "smoke"
    $sourceExtension = [string]$manifest.canonical.source_extension

    Assert-Artifact $entryPath "canonical entry" | Out-Null
    if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
        throw ("canonical source root not found: " + $sourceRoot)
    }
    Assert-Artifact $bootstrapSeedPath "bootstrap seed" | Out-Null
    Assert-Artifact $launcherPath "launcher source" | Out-Null

    $sourceFiles = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File |
        Where-Object { $_.Extension -eq $sourceExtension } |
        Sort-Object FullName)
    if ($sourceFiles.Count -eq 0) {
        throw "canonical source set is empty"
    }

    $sourceRecords = @()
    foreach ($sourceFile in $sourceFiles) {
        $sourceRecords += [ordered]@{
            path = Convert-ToReportPath $sourceFile.FullName
            bytes = $sourceFile.Length
            sha256 = Get-Sha256 $sourceFile.FullName
        }
    }
    $canonicalMaterial = ($sourceRecords | ForEach-Object {
        ([string]$_.path) + "`n" + ([string]$_.sha256) + "`n"
    }) -join ""
    $canonicalSourceHash = Get-TextSha256 $canonicalMaterial
    $bootstrapSeedHash = Get-Sha256 $bootstrapSeedPath
    $launcherSourceHash = Get-Sha256 $launcherPath

    foreach ($stage in @($manifest.stages)) {
        $stagePaths[[string]$stage.name] = Get-RootPath ([string]$stage.artifact)
    }
    foreach ($requiredStage in @("Stage-0", "Stage-1", "Stage-2", "Stage-3")) {
        if (-not $stagePaths.Contains($requiredStage)) {
            throw ("compiler manifest is missing " + $requiredStage)
        }
    }

    if ((Test-Path -LiteralPath $artifactRoot -PathType Container) -and (-not $SkipBootstrapBuild)) {
        Remove-Item -LiteralPath $artifactRoot -Recurse -Force
    }
    Ensure-Directory $artifactRoot
    Ensure-Directory $logRoot
    Ensure-Directory $verifyRoot
    Ensure-Directory $smokeRoot
    Ensure-Directory ([System.IO.Path]::GetDirectoryName($reportPath))

    if ($SkipBootstrapBuild) {
        Assert-Artifact $stagePaths["Stage-0"] "existing Stage-0" | Out-Null
        $cppBuildRecords += [ordered]@{
            name = "bootstrap-skip"
            status = "SKIPPED"
            output = Convert-ToReportPath $stagePaths["Stage-0"]
            output_sha256 = Get-Sha256 $stagePaths["Stage-0"]
        }
    } else {
        $requestedCxx = if ([string]::IsNullOrWhiteSpace($Cxx)) {
            [string]$manifest.build.cxx
        } else {
            $Cxx
        }
        $cxxCommand = Get-Command $requestedCxx -ErrorAction Stop
        $cxxPath = $cxxCommand.Path
        if ([string]::IsNullOrEmpty($cxxPath)) {
            $cxxPath = $requestedCxx
        }
        $versionOutput = @(& $cxxPath --version 2>&1)
        if ($versionOutput.Count -gt 0) {
            $cxxVersion = [string]$versionOutput[0]
        } else {
            $cxxVersion = "unknown"
        }

        $cppFlags = @(("-" + $Optimization), ("-std=" + [string]$manifest.build.standard))
        foreach ($flag in @($manifest.build.reproducibility_flags)) {
            $cppFlags += [string]$flag
        }
        $cppLinkerFlags = @()
        if ($manifest.build.PSObject.Properties.Name -contains "linker_flags") {
            $cppLinkerFlags = @($manifest.build.linker_flags | ForEach-Object { [string]$_ })
        }
        $cppBaseArgs = $cppFlags + @($bootstrapSeedPath) + $cppLinkerFlags
        $cppAPath = Join-Path $verifyRoot "stage0-a.exe"
        $cppBPath = Join-Path $verifyRoot "stage0-b.exe"
        $cppBuildRecords += Invoke-Captured -File $cxxPath `
            -Arguments ($cppBaseArgs + @("-o", $cppAPath)) `
            -Name "cpp-bootstrap-a"
        $cppBuildRecords += Invoke-Captured -File $cxxPath `
            -Arguments ($cppBaseArgs + @("-o", $cppBPath)) `
            -Name "cpp-bootstrap-b"
        Assert-Artifact $cppAPath "C++ bootstrap build A" | Out-Null
        Assert-Artifact $cppBPath "C++ bootstrap build B" | Out-Null
        $cppHashA = Get-Sha256 $cppAPath
        $cppHashB = Get-Sha256 $cppBPath
        $cppReproducible = ($cppHashA -eq $cppHashB)
        if (-not $cppReproducible) {
            throw ("non-deterministic C++ bootstrap output: " + $cppHashA + " != " + $cppHashB)
        }
        Copy-Item -LiteralPath $cppAPath -Destination $stagePaths["Stage-0"] -Force
        Assert-Artifact $stagePaths["Stage-0"] "Stage-0" | Out-Null
    }

    # Stage-0 -> Stage-1 -> Stage-2 -> Stage-3. Every generated stage is
    # used as the next compiler; no checked-in root executable is consulted.
    $sourceArgument = ([string]$manifest.canonical.entry).Replace("/", "\")
    $stageBuildRecords += Invoke-YknCompile $stagePaths["Stage-0"] $sourceArgument $stagePaths["Stage-1"] "stage0-to-stage1"
    $stage1Repeat = Join-Path $verifyRoot "stage1-repeat.exe"
    $stageBuildRecords += Invoke-YknCompile $stagePaths["Stage-0"] $sourceArgument $stage1Repeat "stage0-to-stage1-repeat"
    $stage1Hash = Get-Sha256 $stagePaths["Stage-1"]
    $stage1RepeatHash = Get-Sha256 $stage1Repeat
    $stageReproducibility["Stage-1"] = ($stage1Hash -eq $stage1RepeatHash)

    $stageBuildRecords += Invoke-YknCompile $stagePaths["Stage-1"] $sourceArgument $stagePaths["Stage-2"] "stage1-to-stage2"
    $stage2Repeat = Join-Path $verifyRoot "stage2-repeat.exe"
    $stageBuildRecords += Invoke-YknCompile $stagePaths["Stage-1"] $sourceArgument $stage2Repeat "stage1-to-stage2-repeat"
    $stage2Hash = Get-Sha256 $stagePaths["Stage-2"]
    $stage2RepeatHash = Get-Sha256 $stage2Repeat
    $stageReproducibility["Stage-2"] = ($stage2Hash -eq $stage2RepeatHash)

    $stageBuildRecords += Invoke-YknCompile $stagePaths["Stage-2"] $sourceArgument $stagePaths["Stage-3"] "stage2-to-stage3"
    $stage3Repeat = Join-Path $verifyRoot "stage3-repeat.exe"
    $stageBuildRecords += Invoke-YknCompile $stagePaths["Stage-2"] $sourceArgument $stage3Repeat "stage2-to-stage3-repeat"
    $stage3Hash = Get-Sha256 $stagePaths["Stage-3"]
    $stage3RepeatHash = Get-Sha256 $stage3Repeat
    $stageReproducibility["Stage-3"] = ($stage3Hash -eq $stage3RepeatHash)

    foreach ($stageName in @("Stage-0", "Stage-1", "Stage-2", "Stage-3")) {
        $stageFile = Assert-Artifact $stagePaths[$stageName] $stageName
        $stageArtifacts += [ordered]@{
            name = $stageName
            path = Convert-ToReportPath $stageFile.FullName
            bytes = $stageFile.Length
            sha256 = Get-Sha256 $stageFile.FullName
        }
    }

    $smokeManifestPath = Get-RootPath ([string]$manifest.smoke_tests.manifest)
    Assert-Artifact $smokeManifestPath "smoke test manifest" | Out-Null
    $smokeManifest = (Read-Text $smokeManifestPath) | ConvertFrom-Json
    $smokeAllPassed = $true

    foreach ($stageName in @($manifest.smoke_tests.run_for_stages)) {
        if (-not $stagePaths.Contains([string]$stageName)) {
            throw ("smoke test references unknown stage: " + $stageName)
        }
        $stageShortName = ([string]$stageName).ToLowerInvariant().Replace("-", "")
        foreach ($case in @($smokeManifest.cases)) {
            $smokeCaseCount++
            $caseName = [string]$case.name
            $caseId = $stageShortName + "-" + $caseName
            $caseSource = Get-RootPath ([string]$case.source)
            $caseExpected = Get-RootPath ([string]$case.expected_stdout)
            $caseOutput = Join-Path $smokeRoot ($caseId + ".exe")
            $caseCompile = $null
            $caseRun = $null
            $expectedMatch = $false
            $caseFailure = $null

            try {
                Assert-Artifact $caseSource ($caseId + " source") | Out-Null
                Assert-Artifact $caseExpected ($caseId + " expected stdout") | Out-Null
                $caseCompile = Invoke-YknCompile $stagePaths[$stageName] `
                    (Convert-ToReportPath $caseSource).Replace("/", "\") `
                    $caseOutput ($caseId + "-compile")
                $caseRun = Invoke-Program $caseOutput ($caseId + "-run")
                $expectedText = Normalize-Newlines (Read-Text $caseExpected)
                $actualText = Normalize-Newlines ([string]$caseRun["stdout_text"])
                $expectedMatch = ($actualText -ceq $expectedText)
                if ([int]$caseRun["exit_code"] -ne [int]$case.exit_code) {
                    $caseFailure = "exit code mismatch"
                } elseif (-not $expectedMatch) {
                    $caseFailure = "stdout mismatch"
                }
            } catch {
                $caseFailure = $_.Exception.Message
            }

            $casePassed = ($null -eq $caseFailure)
            if (-not $casePassed) {
                $smokeAllPassed = $false
            }
            $smokeRecords += [ordered]@{
                stage = [string]$stageName
                case = $caseName
                status = if ($casePassed) { "PASS" } else { "FAIL" }
                failure = $caseFailure
                expected_stdout_match = $expectedMatch
                expected_exit_code = [int]$case.exit_code
                actual_exit_code = if ($null -ne $caseRun) { [int]$caseRun["exit_code"] } else { $null }
                compiler = if ($null -ne $caseCompile) { $caseCompile } else { $null }
                program = if ($null -ne $caseRun) {
                    [ordered]@{
                        name = $caseRun["name"]
                        exit_code = $caseRun["exit_code"]
                        stdout = $caseRun["stdout"]
                        stderr = $caseRun["stderr"]
                    }
                } else { $null }
            }

            if (-not $KeepArtifacts) {
                Remove-Item -LiteralPath $caseOutput -Force -ErrorAction SilentlyContinue
            }
        }
    }

    if (-not $smokeAllPassed) {
        throw ("semantic parity smoke tests failed (" +
            (($smokeRecords | Where-Object { $_.status -eq "FAIL" }).Count) + " failure(s))")
    }

    $runStatus = "PASS"
    Write-Host "BOOTSTRAP PASS - canonical source, deterministic seed build, stage chain and smoke parity verified." -ForegroundColor Green
} catch {
    $failureMessage = $_.Exception.Message
    Write-Host ("BOOTSTRAP FAIL - " + $failureMessage) -ForegroundColor Red
} finally {
    if (-not $KeepArtifacts) {
        if ($null -ne $verifyRoot -and (Test-Path -LiteralPath $verifyRoot)) {
            Remove-Item -LiteralPath $verifyRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
        if ($null -ne $smokeRoot -and (Test-Path -LiteralPath $smokeRoot)) {
            Remove-Item -LiteralPath $smokeRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    if ($null -ne $reportPath) {
        $manifestReport = [ordered]@{
            schema = "yukinary.bootstrap-report.v1"
            status = $runStatus
            failure = $failureMessage
            generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
            root = Convert-ToReportPath $ROOT
            manifest = Convert-ToReportPath $MANIFEST_PATH
            compiler = [ordered]@{
                executable = Convert-ToReportPath $cxxPath
                version = $cxxVersion
                optimization = $Optimization
                skip_bootstrap_build = [bool]$SkipBootstrapBuild
            }
            canonical = [ordered]@{
                entry = Convert-ToReportPath $entryPath
                source_root = Convert-ToReportPath $sourceRoot
                source_of_truth = [string]$manifest.canonical.source_of_truth
                source_sha256 = $canonicalSourceHash
                files = @($sourceRecords)
            }
            bootstrap_seed = [ordered]@{
                source = Convert-ToReportPath $bootstrapSeedPath
                role = [string]$manifest.bootstrap_seed.role
                sha256 = $bootstrapSeedHash
            }
            launcher = [ordered]@{
                source = Convert-ToReportPath $launcherPath
                sha256 = $launcherSourceHash
                ci_included = [bool]$manifest.launcher.ci_included
            }
            legacy = [ordered]@{
                source_root = [string]$manifest.legacy.source_root
                ci_included = [bool]$manifest.legacy.ci_included
                policy = [string]$manifest.legacy.policy
            }
            reproducibility = [ordered]@{
                cpp_bootstrap_reproducible = $cppReproducible
                cpp_builds = @($cppBuildRecords)
                stage_outputs_reproducible = $stageReproducibility
                stage_builds = @($stageBuildRecords)
            }
            artifacts = @($stageArtifacts)
            semantic_parity = [ordered]@{
                smoke_manifest = Convert-ToReportPath (Get-RootPath ([string]$manifest.smoke_tests.manifest))
                stages = @($manifest.smoke_tests.run_for_stages)
                cases = $smokeCaseCount
                all_passed = $smokeAllPassed
                results = @($smokeRecords)
            }
        }
        Ensure-Directory ([System.IO.Path]::GetDirectoryName($reportPath))
        $json = $manifestReport | ConvertTo-Json -Depth 20
        [System.IO.File]::WriteAllText($reportPath, $json, (New-Object System.Text.UTF8Encoding($false)))
        Write-Host ("Report: " + (Convert-ToReportPath $reportPath)) -ForegroundColor Cyan
    }
}

# ============================================================
# Seed Parity Check (informational — does NOT fail the build)
# ============================================================
if ($runStatus -eq "PASS") {
    $seedParityScript = Join-Path $ROOT "tests\seed_parity\run_seed_parity_check.ps1"
    if (Test-Path $seedParityScript) {
        Write-Host ""
        Write-Host "=== Seed Parity Check ===" -ForegroundColor Cyan
        $stage0Path = $stagePaths["stage0"]
        $stage3Path = $stagePaths["stage3"]
        if ($stage0Path -and $stage3Path -and (Test-Path $stage0Path) -and (Test-Path $stage3Path)) {
            & powershell.exe -NoProfile -ExecutionPolicy Bypass `
                -File $seedParityScript `
                -CompilerA $stage0Path `
                -CompilerB $stage3Path `
                -OutputJson "build/reports/seed_parity_report.json"
        } else {
            Write-Host "  Seed parity skipped: stage0/stage3 paths not resolved." -ForegroundColor Yellow
        }
    }
}

if ($runStatus -eq "PASS") {
    exit 0
}
exit 1
