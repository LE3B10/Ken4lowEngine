param(
  [Parameter(Mandatory=$true)]
  [string]$ProjectDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$cfg = $env:Configuration
if ($cfg -and ($cfg -ne "Debug")) {
  Write-Host "[TextureBuild] Skip (Configuration=$cfg)"
  exit 0
}

function Get-RelPath([string]$baseDir, [string]$fullPath) {
  $b = (Resolve-Path -LiteralPath $baseDir).Path.TrimEnd('\','/') + '\'
  $p = $fullPath
  try { $p = (Resolve-Path -LiteralPath $fullPath).Path } catch { }

  if ($p.StartsWith($b, [System.StringComparison]::OrdinalIgnoreCase)) {
    return $p.Substring($b.Length)
  }

  $baseUri = New-Object System.Uri($b)
  $pathUri = New-Object System.Uri($p)
  $relUri  = $baseUri.MakeRelativeUri($pathUri)
  return [System.Uri]::UnescapeDataString($relUri.ToString()).Replace('/','\')
}

try {
  $ProjectDir = (Resolve-Path -LiteralPath $ProjectDir).Path
} catch {
}

$srcDir = Join-Path $ProjectDir "Resources\Textures\Sources"
$dstDir = Join-Path $ProjectDir "Resources\Textures\Compiled"
$conv   = Join-Path $ProjectDir "Tools\Bin\TextureConverter.exe"
$log    = Join-Path $dstDir "TextureBuild.log"

New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
Remove-Item -LiteralPath $log -ErrorAction SilentlyContinue

function Log([string]$msg) {
  Add-Content -LiteralPath $log -Value $msg
}

try {
  Log "[TextureBuild] ProjectDir=$ProjectDir"
  Log "[TextureBuild] SRC=$srcDir"
  Log "[TextureBuild] DST=$dstDir"
  Log "[TextureBuild] CONV=$conv"
  Log "[TextureBuild] CWD=$(Get-Location)"

  if (!(Test-Path -LiteralPath $srcDir)) { throw "Sources folder not found: $srcDir" }
  if (!(Test-Path -LiteralPath $conv))   { throw "Converter not found: $conv" }

  $convTime = (Get-Item -LiteralPath $conv).LastWriteTimeUtc

  $total = 0
  $converted = 0
  $skipped = 0

  Get-ChildItem -LiteralPath $srcDir -Recurse -File |
    Where-Object {
      @(".png", ".jpg", ".jpeg", ".tga") -contains $_.Extension.ToLowerInvariant()
    } |
    ForEach-Object {

      $total++
      $srcPath = $_.FullName
      $srcTime = $_.LastWriteTimeUtc

      $rel = Get-RelPath $srcDir $srcPath
      $ddsRel  = [System.IO.Path]::ChangeExtension($rel, ".dds")
      $ddsPath = Join-Path $dstDir $ddsRel

      $outDir = Split-Path -Parent $ddsPath
      New-Item -ItemType Directory -Force -Path $outDir | Out-Null

      if (Test-Path -LiteralPath $ddsPath) {
        $ddsTime = (Get-Item -LiteralPath $ddsPath).LastWriteTimeUtc
        if ($ddsTime -ge $srcTime -and $ddsTime -ge $convTime) {
          $skipped++
          Log "[Skip] $rel"
          return
        }
      }

      Log "[Convert] $rel"

      # 現在の TextureConverter は入力ファイルと同じ場所に .dds を出す前提
      $out = & $conv $srcPath -ml 0 2>&1
      foreach ($line in $out) { Log "  $line" }

      if ($LASTEXITCODE -ne 0) {
        throw "Converter failed (ExitCode=$LASTEXITCODE): $srcPath"
      }

      $tmpDds = [System.IO.Path]::ChangeExtension($srcPath, ".dds")
      if (!(Test-Path -LiteralPath $tmpDds)) {
        throw "DDS not generated: $tmpDds"
      }

      Move-Item -LiteralPath $tmpDds -Destination $ddsPath -Force
      $converted++
    }

  Log "[TextureBuild] Done. total=$total converted=$converted skipped=$skipped"
  Write-Host "[TextureBuild] Done. total=$total converted=$converted skipped=$skipped"
  exit 0
}
catch {
  Log "[TextureBuild] ERROR: $($_.Exception.Message)"
  Log $_.ScriptStackTrace
  Write-Error $_
  exit 1
}