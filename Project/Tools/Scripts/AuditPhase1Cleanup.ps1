param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

$vcxprojPath = Join-Path $ProjectRoot "Ken4lowEngine.vcxproj"
if (-not (Test-Path $vcxprojPath)) {
    throw "Ken4lowEngine.vcxproj was not found: $vcxprojPath"
}

# Phase 1監査は削除を行わず、消えたファイル参照と責務混在を再現可能な形で報告する。
[xml]$projectXml = Get-Content -LiteralPath $vcxprojPath -Raw
$namespace = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable)
$namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")

function Normalize-ProjectPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    return $Path.Replace("/", "\").Trim()
}

function Get-Classification([string]$RelativePath) {
    $normalized = (Normalize-ProjectPath $RelativePath).ToLowerInvariant()
    if ($normalized.StartsWith("engine\editor\")) { return "Editor" }
    if ($normalized.StartsWith("applicationlayer\")) { return "Gameplay/Application" }
    if ($normalized.StartsWith("engine\scene\actor\character\")) { return "Gameplay Framework Candidate" }
    if ($normalized.StartsWith("engine\")) { return "Engine Runtime" }
    if ($normalized.StartsWith("tools\")) { return "Tools" }
    if ($normalized.StartsWith("resources\")) { return "Resources" }
    return "Other"
}

$sourceEntries = @()
foreach ($nodeName in @("ClCompile", "ClInclude")) {
    $nodes = $projectXml.SelectNodes("//*[local-name()='$nodeName']")
    foreach ($node in $nodes) {
        $include = Normalize-ProjectPath $node.Include
        if ([string]::IsNullOrWhiteSpace($include) -or $include.Contains("$(")) { continue }
        $fullPath = Join-Path $ProjectRoot $include
        $sourceEntries += [pscustomobject]@{
            Kind = $nodeName
            Path = $include
            Exists = Test-Path -LiteralPath $fullPath
            Classification = Get-Classification $include
        }
    }
}

$missingProjectEntries = @($sourceEntries | Where-Object { -not $_.Exists } | Sort-Object Path -Unique)

$legacyPattern = '(?i)(legacy|fps|player|enemy|boss|bullet|weapon|crosshair|noammo|reload)'
$legacyCandidates = @(
    Get-ChildItem -LiteralPath $ProjectRoot -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -notmatch '\\Externals\\' -and
            $_.FullName -notmatch '\\Resources\\' -and
            $_.FullName -match $legacyPattern
        } |
        ForEach-Object {
            $relative = $_.FullName.Substring($ProjectRoot.Length).TrimStart('\')
            [pscustomobject]@{
                Path = $relative
                Classification = Get-Classification $relative
            }
        } |
        Sort-Object Path -Unique
)

$classificationSummary = @(
    $sourceEntries |
        Where-Object { $_.Exists } |
        Group-Object Classification |
        Sort-Object Name |
        ForEach-Object {
            [pscustomobject]@{
                Classification = $_.Name
                ProjectEntries = $_.Count
            }
        }
)

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Phase 1 Cleanup Audit")
$lines.Add("")
$lines.Add("Project: $vcxprojPath")
$lines.Add("Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
$lines.Add("")
$lines.Add("## Classification summary")
$lines.Add("")
$lines.Add("| Classification | Project entries |")
$lines.Add("|---|---:|")
foreach ($item in $classificationSummary) {
    $lines.Add("| $($item.Classification) | $($item.ProjectEntries) |")
}

$lines.Add("")
$lines.Add("## Missing .vcxproj file references")
$lines.Add("")
if ($missingProjectEntries.Count -eq 0) {
    $lines.Add("None")
} else {
    foreach ($item in $missingProjectEntries) {
        $lines.Add("- [$($item.Kind)] $($item.Path)")
    }
}

$lines.Add("")
$lines.Add("## Legacy / FPS / gameplay-name candidates")
$lines.Add("")
$lines.Add("These are review candidates, not automatic deletion targets.")
$lines.Add("")
foreach ($item in $legacyCandidates) {
    $lines.Add("- [$($item.Classification)] $($item.Path)")
}

$report = $lines -join [Environment]::NewLine
Write-Output $report

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $resolvedOutput = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
        $OutputPath
    } else {
        Join-Path $ProjectRoot $OutputPath
    }
    $outputDirectory = Split-Path -Parent $resolvedOutput
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    Set-Content -LiteralPath $resolvedOutput -Value $report -Encoding UTF8
    Write-Host "Audit report written: $resolvedOutput"
}
