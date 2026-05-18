param(
    [string]$ProjectDir = ".",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [int]$MipLevel = 0
)

$ErrorActionPreference = "Stop"

function Ensure-Directory {
    param(
        [string]$Path
    )

    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Get-RelativePathSafe {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $baseFull = [System.IO.Path]::GetFullPath($BasePath)
    $targetFull = [System.IO.Path]::GetFullPath($TargetPath)

    $baseUri = New-Object System.Uri(($baseFull.TrimEnd('\') + '\'))
    $targetUri = New-Object System.Uri($targetFull)

    $relativeUri = $baseUri.MakeRelativeUri($targetUri)
    $relativePath = [System.Uri]::UnescapeDataString($relativeUri.ToString())
    return $relativePath.Replace('/', '\')
}

function Invoke-TextureConverter {
    param(
        [string]$ExePath,
        [string]$InputPath,
        [int]$MipLevel
    )

    # TextureConverter の実装仕様:
    #   TextureConverter.exe <input> [-ml <level>]
    $argList = @(
        $InputPath
    )

    if ($MipLevel -ge 0) {
        $argList += "-ml"
        $argList += "$MipLevel"
    }

    Write-Host "[BuildTextures] EXE  : $ExePath"
    Write-Host "[BuildTextures] INPUT: $InputPath"
    Write-Host "[BuildTextures] ARGS : $($argList -join ' ')"

    $process = Start-Process `
        -FilePath $ExePath `
        -ArgumentList $argList `
        -NoNewWindow `
        -Wait `
        -PassThru

    Write-Host "[BuildTextures] ExitCode: $($process.ExitCode)"
    return $process.ExitCode
}

Write-Host "[BuildTextures] ProjectDir    : $ProjectDir"
Write-Host "[BuildTextures] Configuration : $Configuration"
Write-Host "[BuildTextures] Platform      : $Platform"
Write-Host "[BuildTextures] MipLevel      : $MipLevel"

try {
    $ProjectDir = (Resolve-Path $ProjectDir).Path
}
catch {
    throw "ProjectDir not found: $ProjectDir"
}

$rootDir = [System.IO.Path]::GetFullPath((Join-Path $ProjectDir ".."))
$generatedRoot = Join-Path $rootDir "Generated"

Write-Host "[BuildTextures] RootDir       : $rootDir"
Write-Host "[BuildTextures] GeneratedRoot : $generatedRoot"

# ------------------------------------------------------------
# TextureConverter.exe の候補
# ------------------------------------------------------------
$candidates = @(
    (Join-Path $generatedRoot "Bin\TextureConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Platform\$Configuration\TextureConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Configuration\TextureConverter.exe"),

    (Join-Path $ProjectDir "Tools\Bin\TextureConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Platform\$Configuration\TextureConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Configuration\TextureConverter.exe"),

    (Join-Path $ProjectDir "Tools\TextureConverter\TextureConverter.exe"),
    (Join-Path $ProjectDir "Tools\TextureConverter\$Platform\$Configuration\TextureConverter.exe")
)

$textureConverterExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $textureConverterExe) {
    throw "TextureConverter.exe not found. Candidates:`n$($candidates -join "`n")"
}

# ------------------------------------------------------------
# 入力 / 出力
# ------------------------------------------------------------
$textureSourceRoot   = Join-Path $ProjectDir "Resources\Textures\Sources"
$textureCompiledRoot = Join-Path $ProjectDir "Resources\Textures\Compiled"

Write-Host "[BuildTextures] TextureConverter : $textureConverterExe"
Write-Host "[BuildTextures] TextureSourceRoot: $textureSourceRoot"
Write-Host "[BuildTextures] TextureOutputRoot: $textureCompiledRoot"

if (!(Test-Path $textureSourceRoot)) {
    throw "Texture source root not found: $textureSourceRoot"
}

Ensure-Directory -Path $textureCompiledRoot

# ------------------------------------------------------------
# 変換対象:
#   WIC系 -> TextureConverter で DDS 化
#   DDS   -> そのまま Compiled にコピー
# ------------------------------------------------------------
$convertExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr")
$copyExtensions    = @(".dds")

$sourceFiles = Get-ChildItem -Path $textureSourceRoot -Recurse -File | Where-Object {
    ($convertExtensions -contains $_.Extension.ToLower()) -or
    ($copyExtensions -contains $_.Extension.ToLower())
}

Write-Host "[BuildTextures] Source file count: $($sourceFiles.Count)"

$successCount = 0
$skipCount = 0
$copyCount = 0

foreach ($file in $sourceFiles) {
    $ext = $_ = $null
    $ext = $file.Extension.ToLower()

    $relative = Get-RelativePathSafe -BasePath $textureSourceRoot -TargetPath $file.FullName
    $relativeWithoutExt = [System.IO.Path]::ChangeExtension($relative, $null)
    $relativeWithoutExt = $relativeWithoutExt.TrimEnd('.')

    # 最終保存先
    if ($copyExtensions -contains $ext) {
        $finalOutputPath = Join-Path $textureCompiledRoot $relative
    }
    else {
        $finalOutputPath = Join-Path $textureCompiledRoot ($relativeWithoutExt + ".dds")
    }

    $finalOutDir = Split-Path $finalOutputPath -Parent
    Ensure-Directory -Path $finalOutDir

    # --------------------------------------------------------
    # DDS はそのままコピー
    # --------------------------------------------------------
    if ($copyExtensions -contains $ext) {
        Write-Host "[BuildTextures] Copy DDS: $relative"
        Copy-Item -Path $file.FullName -Destination $finalOutputPath -Force
        $copyCount++
        continue
    }

    # --------------------------------------------------------
    # WIC系は TextureConverter で変換
    # --------------------------------------------------------
    $sourceDir = Split-Path $file.FullName -Parent
    $sourceBaseName = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
    $generatedDdsPath = Join-Path $sourceDir ($sourceBaseName + ".dds")

    if (Test-Path $generatedDdsPath) {
        Remove-Item $generatedDdsPath -Force
    }

    # PNG→DDS の最終配置先も出し、Sources と Compiled の対応を追跡できるようにする。
    Write-Host "[BuildTextures] Convert: $relative"
    Write-Host "[BuildTextures] Output : $finalOutputPath"

    $exitCode = Invoke-TextureConverter `
        -ExePath $textureConverterExe `
        -InputPath $file.FullName `
        -MipLevel $MipLevel

    if ($exitCode -ne 0) {
        Write-Warning "Skip texture conversion failed file: $($file.FullName) ExitCode=$exitCode"
        $skipCount++
        continue
    }

    if (!(Test-Path $generatedDdsPath)) {
        Write-Warning "Output file was not created, skipped: $generatedDdsPath"
        $skipCount++
        continue
    }

    Move-Item -Path $generatedDdsPath -Destination $finalOutputPath -Force
    $successCount++
}

Write-Host "[BuildTextures] Texture conversion completed. Converted=$successCount CopiedDDS=$copyCount Skip=$skipCount"
exit 0