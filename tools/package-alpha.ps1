<#
Packages a 4Stim test build for private testers: two mod-manager archives

  4Stim-<version>.zip            the framework (plugin, settings, data, HUD,
                                 icons, sounds, scripts)
  4Stim-TestAnims-<version>.zip  the test animation pack

plus the testers' read-me (docs\ALPHA.md) beside them, in .\release\.

Run from anywhere, after building everything you want in it:
  xmake build -r                 (the plugin)
  Interface-src\build.bat        (the HUD and picker, if they changed)
  the Papyrus compiler           (the FourStim scripts)

  powershell -ExecutionPolicy Bypass -File tools\package-alpha.ps1

The plugin, settings, data files and script sources come from this repository
(so testers get the default 4Stim.ini, not yours); the HUD movies, icons and
sounds from your 4Stim Core mod folder; the compiled scripts from the first of
-ScriptDirs that has them; the test animations from your 4Stim Test Anims mod
folder (with this repository's scene file). It stops if anything is missing
and warns about anything that looks older than its source.
#>
param(
    [string]   $CoreMod      = "D:\Naked Commonwealth\mods\4Stim Core",
    [string]   $TestAnimsMod = "D:\Naked Commonwealth\mods\4Stim Test Anims",
    [string[]] $ScriptDirs   = @(
        "D:\Naked Commonwealth\overwrite\Scripts",
        "G:\SteamLibrary\steamapps\common\Fallout 4\Data\Scripts",
        "D:\Naked Commonwealth\mods\4Stim Core\Scripts"),
    [string]   $Out = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$Repo = Split-Path -Parent $PSScriptRoot
if (-not $Out) { $Out = Join-Path $Repo "release" }

$version = (Select-String -Path (Join-Path $Repo "xmake.lua") -Pattern 'set_version\("([^"]+)"\)').Matches[0].Groups[1].Value
$tag = "$version-alpha"
$Scripts = @("FourStim", "FourStimScene", "FourStimMenu", "FourStimUndress", "FourStimPhysics")
$problems = @()
$warnings = @()

function Need([string] $path, [string] $what) {
    if (-not (Test-Path -LiteralPath $path)) { $script:problems += "$what not found: $path" }
    return $path
}

# Entries are (source path, path in the archive) pairs, in typed lists (a
# plain PowerShell array would unroll a single pair into two strings).
function New-List { return , (New-Object 'System.Collections.Generic.List[object]') }

function Add-File($list, [string] $src, [string] $dst) {
    $list.Add(@($src, $dst))
}

# Every file under $dir (recursively), under $prefix in the archive.
function Add-Tree($list, [string] $dir, [string] $prefix, [string[]] $skip = @()) {
    if (-not (Test-Path -LiteralPath $dir)) { $script:problems += "folder not found: $dir"; return }
    $root = (Resolve-Path -LiteralPath $dir).Path.TrimEnd('\')
    foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File) {
        $name = $f.Name
        if ($skip | Where-Object { $name -like $_ }) { continue }
        $rel = $f.FullName.Substring($root.Length + 1).Replace('\', '/')
        $list.Add(@($f.FullName, ($prefix.TrimEnd('/') + '/' + $rel).TrimStart('/')))
    }
}

function Write-Zip([string] $zipPath, $entries) {
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath }
    $zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $seen = @{}
        foreach ($e in $entries) {
            if ($seen.ContainsKey($e[1])) { continue }  # first source wins
            $seen[$e[1]] = $true
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $e[0], $e[1], [System.IO.Compression.CompressionLevel]::Optimal)
        }
    } finally {
        $zip.Dispose()
    }
    "{0}: {1} files, {2:N1} MB" -f (Split-Path -Leaf $zipPath), $seen.Count, ((Get-Item -LiteralPath $zipPath).Length / 1MB)
}

# ---- 4Stim (the framework) ----
$core = New-List

$dll = Need (Join-Path $Repo "build\windows\x64\release\4Stim.dll") "the plugin (run xmake build -r)"
Add-File $core $dll "F4SE/Plugins/4Stim.dll"
if (Test-Path -LiteralPath $dll) {
    $built = (Get-Item -LiteralPath $dll).LastWriteTime
    $newer = @(Get-ChildItem -LiteralPath (Join-Path $Repo "src") -File) + @(Get-Item -LiteralPath (Join-Path $Repo "xmake.lua")) |
        Where-Object { $_.LastWriteTime -gt $built }
    if ($newer) { $warnings += "4Stim.dll is older than " + (($newer | ForEach-Object Name) -join ", ") + ": rebuild with xmake build -r?" }
}

Add-File $core (Need (Join-Path $Repo "Data\F4SE\Plugins\4Stim.ini") "the default 4Stim.ini") "F4SE/Plugins/4Stim.ini"
foreach ($sub in "Actions", "Furniture", "Physics") {
    Add-Tree $core (Join-Path $Repo "Data\F4SE\Plugins\4Stim\$sub") "F4SE/Plugins/4Stim/$sub"
}
Add-Tree $core (Join-Path $Repo "Data\Interface\4Stim\Themes") "Interface/4Stim/Themes"

foreach ($movie in "FourStimHUDMenu.swf", "FourStimPickerMenu.swf") {
    Add-File $core (Need (Join-Path $CoreMod "Interface\$movie") "the HUD movie $movie") "Interface/$movie"
}
Add-File $core (Need (Join-Path $CoreMod "Interface\4Stim\HUDLogo.swf") "the HUD logo") "Interface/4Stim/HUDLogo.swf"
Add-Tree $core (Join-Path $CoreMod "Interface\4Stim\Icons") "Interface/4Stim/Icons"
Add-Tree $core (Join-Path $CoreMod "Sound") "Sound"

foreach ($s in $Scripts) {
    $psc = Need (Join-Path $Repo "Data\Scripts\Source\User\$s.psc") "the script source $s.psc"
    Add-File $core $psc "Scripts/Source/User/$s.psc"
    $pex = $ScriptDirs | ForEach-Object { Join-Path $_ "$s.pex" } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $pex) {
        $problems += "compiled script $s.pex not found in: " + ($ScriptDirs -join "; ") + " (compile it, or pass -ScriptDirs)"
        continue
    }
    Add-File $core $pex "Scripts/$s.pex"
    if ((Get-Item -LiteralPath $pex).LastWriteTime -lt (Get-Item -LiteralPath $psc).LastWriteTime) {
        $warnings += "$s.pex ($pex) is older than its source: recompile it?"
    }
}

# ---- 4Stim Test Anims ----
$anims = New-List
Add-File $anims (Need (Join-Path $TestAnimsMod "4StimTestAnims.esp") "4StimTestAnims.esp") "4StimTestAnims.esp"
Add-File $anims (Need (Join-Path $Repo "Data\F4SE\Plugins\4Stim\Scenes\4StimTestAnims.json") "the test scene file") "F4SE/Plugins/4Stim/Scenes/4StimTestAnims.json"
Add-Tree $anims (Join-Path $TestAnimsMod "meshes") "meshes" @("*.bak")

if ($problems) {
    Write-Host "Not packaged, missing:" -ForegroundColor Red
    $problems | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null
Write-Host "4Stim $tag"
Write-Zip (Join-Path $Out "4Stim-$tag.zip") $core
Write-Zip (Join-Path $Out "4Stim-TestAnims-$tag.zip") $anims
Copy-Item -LiteralPath (Join-Path $Repo "docs\ALPHA.md") -Destination (Join-Path $Out "4Stim-$tag-ReadMe.md") -Force
Write-Host "Read-me: 4Stim-$tag-ReadMe.md"
Write-Host "In: $Out"
if ($warnings) {
    Write-Host ""
    $warnings | ForEach-Object { Write-Host "Warning: $_" -ForegroundColor Yellow }
}
