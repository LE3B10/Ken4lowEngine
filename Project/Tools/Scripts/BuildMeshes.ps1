param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir,

    [string]$Config = $(if ($env:Configuration) { $env:Configuration } else { "Debug" }),
    [string]$Platform = $(if ($env:Platform) { $env:Platform } else { "x64" }),

    [switch]$Pause
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Info($message) {
    Write-Host "[BuildMeshes] $message"
}

# ------------------------------------------------------------
# パス解決
# ------------------------------------------------------------
$project = (Resolve-Path $ProjectDir).Path

$srcDir = Join-Path $project "Resources\Models\Sources"
$dstDir = Join-Path $project "Resources\Models\Compiled"

New-Item -ItemType Directory -Force -Path $dstDir | Out-Null

if (-not (Test-Path $srcDir)) {
    Write-Info "Sources フォルダーが見つかりません: $srcDir"
    if ($Pause) { pause }
    exit 0
}

# ------------------------------------------------------------
# MeshConverter.exe の候補
# ------------------------------------------------------------
$candidates = @(
    (Join-Path $project "Tools\MeshConverter\$Platform\$Config\MeshConverter.exe"),
    (Join-Path $project "Tools\Bin\$Platform\$Config\MeshConverter.exe"),
    (Join-Path $project "Tools\MeshConverter\MeshConverter.exe"),
    (Join-Path $project "$Platform\$Config\MeshConverter.exe"),
    (Join-Path $project "$Platform\$Config\MeshConverter\MeshConverter.exe")
)

$converter = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $converter) {
    throw "MeshConverter.exe が見つかりません。候補:`n$($candidates -join "`n")"
}

Write-Info "Converter: $converter"
Write-Info "Source   : $srcDir"
Write-Info "Compiled : $dstDir"

# ------------------------------------------------------------
# 対象ファイル列挙
# ------------------------------------------------------------
$patterns = @("*.gltf", "*.glb", "*.fbx", "*.obj")
$files = @()

foreach ($p in $patterns) {
    $files += Get-ChildItem -Path $srcDir -Recurse -File -Filter $p
}

if ($files.Count -eq 0) {
    Write-Info "変換対象モデルが見つかりません。"
    if ($Pause) { pause }
    exit 0
}

# ------------------------------------------------------------
# MeshConverter オプション
# エンジン流の左手座標へ寄せる前提
# ------------------------------------------------------------
$options = @("-lh")

# 必要なら使う
# $options += @("-scale", "0.01")
# $options += @("-flipuv")

# ------------------------------------------------------------
# 変換
# ------------------------------------------------------------
$convertedCount = 0
$skippedCount = 0

foreach ($f in $files) {

    $rel = $f.FullName.Substring($srcDir.Length).TrimStart('\', '/')
    $outRel = [System.IO.Path]::ChangeExtension($rel, ".kmesh")
    $outPath = Join-Path $dstDir $outRel
    $outDir = Split-Path $outPath -Parent

    New-Item -ItemType Directory -Force -Path $outDir | Out-Null

    # 差分変換
    if (Test-Path $outPath) {
        $srcTime = $f.LastWriteTimeUtc
        $dstTime = (Get-Item $outPath).LastWriteTimeUtc

        if ($dstTime -ge $srcTime) {
            Write-Info "Skip: $rel"
            $skippedCount++
            continue
        }
    }

    Write-Info "Convert: $rel"

    & $converter $f.FullName @options
    if ($LASTEXITCODE -ne 0) {
        throw "MeshConverter failed: $($f.FullName)"
    }

    # 現在の MeshConverter は入力ファイルの横に .kmesh を吐く仕様
    $generated = [System.IO.Path]::ChangeExtension($f.FullName, ".kmesh")

    if (-not (Test-Path $generated)) {
        throw "生成された .kmesh が見つかりません: $generated"
    }

    Copy-Item -Force $generated $outPath
    $convertedCount++
}

Write-Info "完了: converted=$convertedCount, skipped=$skippedCount"

if ($Pause) { pause }
exit 0