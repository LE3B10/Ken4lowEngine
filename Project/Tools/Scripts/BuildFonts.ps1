param(
    [string]$ProjectDir = ".",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

function Invoke-FontConverter {
    param(
        [string]$ExePath,
        [string]$FontPath,
        [string]$OutDir,
        [string]$CharsetFile,
        [string]$LogPath
    )

    $stdoutLog = [System.IO.Path]::ChangeExtension($LogPath, ".out.log")
    $stderrLog = [System.IO.Path]::ChangeExtension($LogPath, ".err.log")

    if (Test-Path $stdoutLog) { Remove-Item $stdoutLog -Force }
    if (Test-Path $stderrLog) { Remove-Item $stderrLog -Force }
    if (Test-Path $LogPath)   { Remove-Item $LogPath   -Force }

    $argList = @(
        $FontPath,
        "-out", $OutDir,
        "-size", "48",
        "-atlasWidth", "1024",
        "-atlasHeight", "1024",
        "-charsetFile", $CharsetFile
    )

    $process = Start-Process `
        -FilePath $ExePath `
        -ArgumentList $argList `
        -NoNewWindow `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog

    if (Test-Path $stdoutLog) {
        Get-Content $stdoutLog | Add-Content $LogPath
    }
    if (Test-Path $stderrLog) {
        Get-Content $stderrLog | Add-Content $LogPath
    }

    return $process.ExitCode
}

function Move-FontOutputs {
    param(
        [string]$SourceDir,
        [string]$TextureOutDir,
        [string]$MetaOutDir
    )

    New-Item -ItemType Directory -Force -Path $TextureOutDir | Out-Null
    New-Item -ItemType Directory -Force -Path $MetaOutDir | Out-Null

    Get-ChildItem -Path $SourceDir -File -Filter *.png -ErrorAction SilentlyContinue | ForEach-Object {
        Move-Item $_.FullName (Join-Path $TextureOutDir $_.Name) -Force
    }

    Get-ChildItem -Path $SourceDir -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Extension -in ".json", ".txt", ".pgm", ".log"
    } | ForEach-Object {
        Move-Item $_.FullName (Join-Path $MetaOutDir $_.Name) -Force
    }
}

function Remove-DirectoryIfExists {
    param(
        [string]$Path
    )

    if (Test-Path $Path) {
        Remove-Item $Path -Force -Recurse -ErrorAction SilentlyContinue
    }
}

Write-Host "[BuildFonts] ProjectDir    : $ProjectDir"
Write-Host "[BuildFonts] Configuration : $Configuration"
Write-Host "[BuildFonts] Platform      : $Platform"

try {
    $ProjectDir = (Resolve-Path $ProjectDir).Path
}
catch {
    throw "ProjectDir not found: $ProjectDir"
}

$rootDir = [System.IO.Path]::GetFullPath((Join-Path $ProjectDir ".."))
$generatedRoot = Join-Path $rootDir "Generated"

Write-Host "[BuildFonts] RootDir       : $rootDir"
Write-Host "[BuildFonts] GeneratedRoot : $generatedRoot"

$candidates = @(
    (Join-Path $generatedRoot "Bin\FontConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Platform\$Configuration\FontConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Configuration\FontConverter.exe"),

    (Join-Path $ProjectDir "Tools\Bin\FontConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Platform\$Configuration\FontConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Configuration\FontConverter.exe"),

    (Join-Path $ProjectDir "Tools\FontConverter\FontConverter.exe"),
    (Join-Path $ProjectDir "Tools\FontConverter\$Platform\$Configuration\FontConverter.exe")
)

$fontConverterExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $fontConverterExe) {
    throw "FontConverter.exe not found. Candidates:`n$($candidates -join "`n")"
}

$fontSourceDir = Join-Path $ProjectDir "Resources\Fonts\Sources"
$charsetDir    = Join-Path $ProjectDir "Resources\Fonts\Charsets"

$latinFontPath    = Join-Path $fontSourceDir "minecraft.ttf"
$jpFontPath       = Join-Path $fontSourceDir "PixelMplus12-Regular.ttf"
$latinCharsetFile = Join-Path $charsetDir "LatinCharset.txt"
$jpCharsetFile    = Join-Path $charsetDir "JPCharset.txt"

$tempRoot     = Join-Path $generatedRoot "Fonts"
$tempLatinDir = Join-Path $tempRoot "Latin"
$tempJPDir    = Join-Path $tempRoot "JP"

$textureFontRoot = Join-Path $ProjectDir "Resources\Textures\Sources\UI\Font"
$fontMetaRoot    = Join-Path $ProjectDir "Resources\Fonts\Compiled"

$latinTextureOutDir = Join-Path $textureFontRoot "Latin"
$jpTextureOutDir    = Join-Path $textureFontRoot "JP"

$latinMetaOutDir = Join-Path $fontMetaRoot "Latin"
$jpMetaOutDir    = Join-Path $fontMetaRoot "JP"

Write-Host "[BuildFonts] FontConverter    : $fontConverterExe"
Write-Host "[BuildFonts] LatinFont        : $latinFontPath"
Write-Host "[BuildFonts] JPFont           : $jpFontPath"
Write-Host "[BuildFonts] LatinCharsetFile : $latinCharsetFile"
Write-Host "[BuildFonts] JPCharsetFile    : $jpCharsetFile"

if (!(Test-Path $latinFontPath))    { throw "Latin font not found: $latinFontPath" }
if (!(Test-Path $jpFontPath))       { throw "Japanese font not found: $jpFontPath" }
if (!(Test-Path $latinCharsetFile)) { throw "Latin charset file not found: $latinCharsetFile" }
if (!(Test-Path $jpCharsetFile))    { throw "JP charset file not found: $jpCharsetFile" }

New-Item -ItemType Directory -Force -Path $tempLatinDir | Out-Null
New-Item -ItemType Directory -Force -Path $tempJPDir | Out-Null
New-Item -ItemType Directory -Force -Path $latinTextureOutDir | Out-Null
New-Item -ItemType Directory -Force -Path $jpTextureOutDir | Out-Null
New-Item -ItemType Directory -Force -Path $latinMetaOutDir | Out-Null
New-Item -ItemType Directory -Force -Path $jpMetaOutDir | Out-Null

$latinLogPath = Join-Path $latinMetaOutDir "latin_build.log"
$jpLogPath    = Join-Path $jpMetaOutDir "jp_build.log"

Get-ChildItem -Path $tempLatinDir -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $tempJPDir -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

Write-Host "[BuildFonts] Build Latin atlas..."
$latinExit = Invoke-FontConverter `
    -ExePath $fontConverterExe `
    -FontPath $latinFontPath `
    -OutDir $tempLatinDir `
    -CharsetFile $latinCharsetFile `
    -LogPath $latinLogPath

if ($latinExit -ne 0) {
    throw "Latin font conversion failed. ExitCode=$latinExit Log=$latinLogPath"
}

Move-FontOutputs `
    -SourceDir $tempLatinDir `
    -TextureOutDir $latinTextureOutDir `
    -MetaOutDir $latinMetaOutDir

Write-Host "[BuildFonts] Build Japanese atlas..."
$jpExit = Invoke-FontConverter `
    -ExePath $fontConverterExe `
    -FontPath $jpFontPath `
    -OutDir $tempJPDir `
    -CharsetFile $jpCharsetFile `
    -LogPath $jpLogPath

if ($jpExit -ne 0) {
    throw "Japanese font conversion failed. ExitCode=$jpExit Log=$jpLogPath"
}

Move-FontOutputs `
    -SourceDir $tempJPDir `
    -TextureOutDir $jpTextureOutDir `
    -MetaOutDir $jpMetaOutDir

Remove-DirectoryIfExists -Path $tempLatinDir
Remove-DirectoryIfExists -Path $tempJPDir

if (Test-Path $tempRoot) {
    $remain = Get-ChildItem -Path $tempRoot -Force -ErrorAction SilentlyContinue
    if ($null -eq $remain -or $remain.Count -eq 0) {
        Remove-Item $tempRoot -Force -Recurse -ErrorAction SilentlyContinue
    }
}

Write-Host "[BuildFonts] Font conversion completed."
exit 0