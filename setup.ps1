# CifraSync Setup — run this ONCE to enable "wake up cifra"
# Run: powershell -ExecutionPolicy Bypass -File setup.ps1

$projectRoot = $PSScriptRoot
$binDir = Join-Path $projectRoot "bin"

# 1. Add bin\ to PATH
$userPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($userPath -notlike "*$binDir*") {
    [Environment]::SetEnvironmentVariable("PATH", "$userPath;$binDir", "User")
    Write-Host "  [+] Added bin\ to PATH (user-level)"
} else {
    Write-Host "  [=] bin\ already in PATH"
}

# 2. Add 'wake' function to PowerShell profile
$profileContent = @"

# CifraSync: "wake up cifra" command
function wake { & "$binDir\cifrasync.exe" }
"@
if (-not (Test-Path $PROFILE)) {
    New-Item -Path $PROFILE -ItemType File -Force | Out-Null
}
$currentProfile = Get-Content $PROFILE -Raw -ErrorAction SilentlyContinue
if ($currentProfile -notlike "*CifraSync*") {
    Add-Content -Path $PROFILE -Value $profileContent
    Write-Host "  [+] Added 'wake' function to PowerShell profile"
} else {
    Write-Host "  [=] 'wake' function already in profile"
}

Write-Host ""
Write-Host "  Setup complete! Restart your terminal, then type: wake up cifra"
Write-Host ""
