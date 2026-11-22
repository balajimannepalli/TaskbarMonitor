[CmdletBinding()]
param(
    [string]$BuildDirectory = "$PSScriptRoot\..\build\phase6",
    [Parameter(Mandatory)][string]$Compiler
)
$ErrorActionPreference='Stop'
$build=(Resolve-Path -LiteralPath $BuildDirectory).Path
$release=Join-Path $PSScriptRoot '..\release'
$installed=Get-Item -LiteralPath (Join-Path $build 'Release\TaskbarMonitor.exe')
$portable=Get-Item -LiteralPath (Join-Path $build 'portable\Release\TaskbarMonitor.exe')
if (!$installed.VersionInfo.ProductVersion -or $installed.VersionInfo.ProductVersion -ne $portable.VersionInfo.ProductVersion) {
    throw 'Build both Release targets with matching version resources first.'
}
& $Compiler "/DBinaryDir=$($installed.DirectoryName)" "$PSScriptRoot\TaskbarMonitor.iss"
if ($LASTEXITCODE) {throw "Installer compilation failed ($LASTEXITCODE)"}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath=Join-Path $release 'TaskbarMonitor-Portable-x64.zip'
$tempZip=Join-Path $build ([guid]::NewGuid().ToString()+'.zip')
try {
    $zip=[IO.Compression.ZipFile]::Open($tempZip,'Create')
    try {
        $entries=@{
            'TaskbarMonitor.exe'=$portable.FullName
            'README.txt'="$PSScriptRoot\Portable-README.txt"
            'nlohmann-JSON-LICENSE.txt'="$PSScriptRoot\..\third_party\nlohmann\LICENSE.MIT"
        }
        foreach ($name in $entries.Keys) {
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$entries[$name],$name)
        }
    } finally {$zip.Dispose()}
    Move-Item -LiteralPath $tempZip -Destination $zipPath -Force
} finally {if (Test-Path -LiteralPath $tempZip) {Remove-Item -LiteralPath $tempZip}}
$hashes=foreach ($name in 'TaskbarMonitor-Setup-x64.exe','TaskbarMonitor-Portable-x64.zip','Release-Notes.md') {
    (Get-FileHash -LiteralPath (Join-Path $release $name) -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$name
}
$hashes | Set-Content -LiteralPath (Join-Path $release 'SHA256SUMS.txt') -Encoding ascii
