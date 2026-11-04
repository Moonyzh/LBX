param(
    [string]$CvmHost = "119.45.136.135",
    [string]$CvmUser = "ubuntu",
    [string]$RemoteRoot = "/home/ubuntu/lab1-mini-dht",
    [string]$KeyFile = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

$files = @(
    @{ Local = "config/dht-node-b.cvm.yaml"; Remote = "config/" },
    @{ Local = "config/dht-smoke-client.cvm.yaml"; Remote = "config/" },
    @{ Local = "scripts/start-node-b.sh"; Remote = "scripts/" }
)

$commonArgs = @()
if ($KeyFile -ne "") {
    if (-not (Test-Path -LiteralPath $KeyFile)) {
        throw "SSH key not found: $KeyFile"
    }
    $commonArgs += @("-i", $KeyFile)
}

foreach ($entry in $files) {
    $localPath = Join-Path $RepoRoot $entry.Local
    if (-not (Test-Path -LiteralPath $localPath)) {
        throw "Local file not found: $localPath"
    }

    $remotePath = "${CvmUser}@${CvmHost}:${RemoteRoot}/$($entry.Remote)"
    Write-Host "Uploading $localPath -> $remotePath"

    $scpArgs = $commonArgs + @($localPath, $remotePath)
    & scp @scpArgs
    if ($LASTEXITCODE -ne 0) {
        throw "scp failed for $localPath"
    }
}

Write-Host "Upload completed."
