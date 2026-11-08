# ============================================================
# lock_test_helper.ps1
# Cache kilitleme test yardimcisi. Belirli bir lock dosyasini
# belirli sure boyunca tutarak diger islemi test etmeyi saglar.
# ============================================================
param(
    [string]$LockFile,
    [int]$HoldMs = 2000
)

if (-not $LockFile) {
    Write-Host "Usage: lock_test_helper.ps1 -LockFile <path> [-HoldMs <ms>]" -ForegroundColor Red
    exit 1
}

# Create the lock file
$stream = $null
try {
    $stream = [System.IO.File]::Open($LockFile, 
        [System.IO.FileMode]::OpenOrCreate,
        [System.IO.FileAccess]::ReadWrite,
        [System.IO.FileShare]::None)  # Exclusive lock
    
    Write-Host "LOCK_HELPER: Kilit alindi: $LockFile" -ForegroundColor Green
    Write-Host "LOCK_HELPER: $HoldMs ms bekleniyor..."
    Start-Sleep -Milliseconds $HoldMs
    Write-Host "LOCK_HELPER: Kilit serbest birakiliyor"
} catch {
    Write-Host "LOCK_HELPER: Kilit alinamadi: $_" -ForegroundColor Yellow
    exit 1
} finally {
    if ($stream) { $stream.Close(); $stream.Dispose() }
}
exit 0
