[CmdletBinding()]
param(
    [string]$GeneratorElf = "build/test-work/dataset-generator-linux.elf",
    [string]$AcceptanceElf = "build/test-work/dataset-generator-acceptance-linux.elf",
    [string]$DockerImage = "ubuntu:24.04",
    [string]$OutputJson = "build/reports/dataset-native-execution.json"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$generatorPath = Join-Path $Root $GeneratorElf
$acceptancePath = Join-Path $Root $AcceptanceElf
if (-not (Test-Path $generatorPath -PathType Leaf)) { throw "Generator ELF not found: $GeneratorElf" }
if (-not (Test-Path $acceptancePath -PathType Leaf)) { throw "Acceptance ELF not found: $AcceptanceElf" }

$dockerCommand = Get-Command docker.exe -ErrorAction SilentlyContinue
if ($null -eq $dockerCommand) { $dockerCommand = Get-Command docker -ErrorAction SilentlyContinue }
if ($null -eq $dockerCommand) { throw "Docker executable not found" }

$generatorContainerPath = "/workspace/" + ($GeneratorElf -replace "\\", "/")
$acceptanceContainerPath = "/workspace/" + ($AcceptanceElf -replace "\\", "/")
$arguments = @(
    "run", "--rm",
    "-v", "$($Root):/workspace",
    "-w", "/workspace",
    $DockerImage,
    "/bin/sh", "-c",
    "$generatorContainerPath && $acceptanceContainerPath"
)

$output = @(& $dockerCommand.Source @arguments 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
$exitCode = $LASTEXITCODE
$reportDirectory = Join-Path $Root (Split-Path $OutputJson -Parent)
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$outputHash = [System.BitConverter]::ToString(
    ([System.Security.Cryptography.SHA256]::Create()).ComputeHash(
        [System.Text.Encoding]::UTF8.GetBytes($output)
    )
).Replace("-", "").ToLowerInvariant()
$generatorHash = (Get-FileHash $generatorPath -Algorithm SHA256).Hash.ToLowerInvariant()
$acceptanceHash = (Get-FileHash $acceptancePath -Algorithm SHA256).Hash.ToLowerInvariant()
$generatorPass = $output -match "YKN_DATASET_GENERATOR_NATIVE_YKDS_YKLM_PASS"
$acceptancePass = $output -match "YKN_DATASET_GENERATOR_ACCEPTANCE_PASS"
$verified = ($exitCode -eq 0 -and $generatorPass -and $acceptancePass)

[ordered]@{
    schema = "yukinary.native-execution-evidence.v1"
    generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    target = "linux-x86_64-docker"
    image = $DockerImage
    command = @("docker", "run", "--rm", "-v", "<root>:/workspace", "-w", "/workspace", $DockerImage, "/bin/sh", "-c", "$generatorContainerPath && $acceptanceContainerPath")
    exit_code = $exitCode
    generator_elf = [ordered]@{ path = $GeneratorElf; sha256 = $generatorHash }
    acceptance_elf = [ordered]@{ path = $AcceptanceElf; sha256 = $acceptanceHash }
    stdout_sha256 = $outputHash
    generator_pass_marker = $generatorPass
    acceptance_pass_marker = $acceptancePass
    verified = $verified
    output = $output
} | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $Root $OutputJson) -Encoding UTF8

Write-Host "Docker native execution evidence: $(Join-Path $Root $OutputJson)"
Write-Host "Exit=$exitCode Generator=$generatorPass Acceptance=$acceptancePass Verified=$verified"
if (-not $verified) { exit 1 }
exit 0
