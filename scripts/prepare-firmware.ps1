$ErrorActionPreference = "Stop"

foreach ($target in @("rover", "gateway")) {
    $base = "firmware/$target/include"
    $example = "$base/local_config.h.example"
    $dest = "$base/local_config.h"
    if ((Test-Path $example) -and -not (Test-Path $dest)) {
        Copy-Item $example $dest
        Write-Host "Created $dest"
    }
}

$secrets = "firmware/gateway/include/secrets.h"
if (-not (Test-Path $secrets)) {
    Copy-Item "firmware/gateway/include/secrets.example.h" $secrets
    Write-Host "Created $secrets"
}

Write-Host "Edit local_config.h and secrets.h before flashing. These files are gitignored."
