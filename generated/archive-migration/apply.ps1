$ErrorActionPreference = 'Stop'
$migrationRoot = 'C:\project\GJ3\generated\archive-migration'
$destinationRoot = 'C:\project\MyEngine\project'
$manifest = Get-Content -LiteralPath "$migrationRoot\manifest.json" -Raw | ConvertFrom-Json
foreach ($entry in $manifest.PSObject.Properties) {
    $target = [IO.Path]::GetFullPath((Join-Path $destinationRoot $entry.Name))
    if (-not $target.StartsWith($destinationRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Invalid target: $target" }
    if ($entry.Name -match 'TitleScene') { throw 'TitleScene must not change' }
    if ($null -ne $entry.Value) {
        if (-not (Test-Path -LiteralPath $target)) { throw "Missing original: $target" }
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value) { throw "Original changed: $target" }
    } elseif (Test-Path -LiteralPath $target) { throw "New file already exists: $target" }
}
foreach ($entry in $manifest.PSObject.Properties) {
    $target = Join-Path $destinationRoot $entry.Name
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    Copy-Item -LiteralPath (Join-Path "$migrationRoot\files" $entry.Name) -Destination $target
}
Write-Output "Applied $($manifest.PSObject.Properties.Count) migration entries."
