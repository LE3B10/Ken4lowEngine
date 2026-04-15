param(
    [string]$ProjectDir = ".",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

function Ensure-Directory {
    param(
        [string]$Path
    )

    if (!(Test-Path $Path)) {
        New-Item -ItemType Directory -Force -Path $Path | Out-Null
    }
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

function Invoke-MeshConverter {
    param(
        [string]$ExePath,
        [string]$InputPath
    )

    $argList = @(
        $InputPath
    )

    Write-Host "[BuildMeshes] EXE  : $ExePath"
    Write-Host "[BuildMeshes] INPUT: $InputPath"
    Write-Host "[BuildMeshes] ARGS : $($argList -join ' ')"

    $process = Start-Process `
        -FilePath $ExePath `
        -ArgumentList $argList `
        -NoNewWindow `
        -Wait `
        -PassThru

    Write-Host "[BuildMeshes] ExitCode: $($process.ExitCode)"
    return $process.ExitCode
}

Write-Host "[BuildMeshes] ProjectDir    : $ProjectDir"
Write-Host "[BuildMeshes] Configuration : $Configuration"
Write-Host "[BuildMeshes] Platform      : $Platform"

try {
    $ProjectDir = (Resolve-Path $ProjectDir).Path
}
catch {
    throw "ProjectDir not found: $ProjectDir"
}

$rootDir = [System.IO.Path]::GetFullPath((Join-Path $ProjectDir ".."))
$generatedRoot = Join-Path $rootDir "Generated"

$candidates = @(
    (Join-Path $generatedRoot "Bin\MeshConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Platform\$Configuration\MeshConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Configuration\MeshConverter.exe"),

    (Join-Path $ProjectDir "Tools\Bin\MeshConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Platform\$Configuration\MeshConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Configuration\MeshConverter.exe"),

    (Join-Path $ProjectDir "Tools\MeshConverter\MeshConverter.exe"),
    (Join-Path $ProjectDir "Tools\MeshConverter\$Platform\$Configuration\MeshConverter.exe")
)

$meshConverterExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $meshConverterExe) {
    throw "MeshConverter.exe not found. Candidates:`n$($candidates -join "`n")"
}

$modelSourceRoot   = Join-Path $ProjectDir "Resources\Models\Sources"
$modelCompiledRoot = Join-Path $ProjectDir "Resources\Models\Compiled"

Write-Host "[BuildMeshes] MeshConverter  : $meshConverterExe"
Write-Host "[BuildMeshes] ModelSourceRoot: $modelSourceRoot"
Write-Host "[BuildMeshes] ModelOutputRoot: $modelCompiledRoot"

if (!(Test-Path $modelSourceRoot)) {
    throw "Model source root not found: $modelSourceRoot"
}

Ensure-Directory -Path $modelCompiledRoot

# FBX ÇÕåªç›ÇÃ Assimp ç\ê¨Ç≈é∏îsÇµÇƒÇ¢ÇÈÇΩÇﬂèúäO
$extensions = @(".gltf", ".glb", ".obj")

$sourceFiles = Get-ChildItem -Path $modelSourceRoot -Recurse -File | Where-Object {
    $extensions -contains $_.Extension.ToLower()
}

Write-Host "[BuildMeshes] Source file count: $($sourceFiles.Count)"

$successCount = 0
$skipCount = 0

foreach ($file in $sourceFiles) {
    $relative = Get-RelativePathSafe -BasePath $modelSourceRoot -TargetPath $file.FullName
    $relativeWithoutExt = [System.IO.Path]::ChangeExtension($relative, $null)
    $relativeWithoutExt = $relativeWithoutExt.TrimEnd('.')

    $sourceDir = Split-Path $file.FullName -Parent
    $sourceBaseName = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
    $generatedKmeshPath = Join-Path $sourceDir ($sourceBaseName + ".kmesh")

    $finalOutputPath = Join-Path $modelCompiledRoot ($relativeWithoutExt + ".kmesh")
    $finalOutDir = Split-Path $finalOutputPath -Parent

    Ensure-Directory -Path $finalOutDir

    if (Test-Path $generatedKmeshPath) {
        Remove-Item $generatedKmeshPath -Force
    }

    Write-Host "[BuildMeshes] Convert: $relative"

    $exitCode = Invoke-MeshConverter `
        -ExePath $meshConverterExe `
        -InputPath $file.FullName

    if ($exitCode -ne 0) {
        Write-Warning "Skip mesh conversion failed file: $($file.FullName) ExitCode=$exitCode"
        $skipCount++
        continue
    }

    if (!(Test-Path $generatedKmeshPath)) {
        Write-Warning "Output file was not created, skipped: $generatedKmeshPath"
        $skipCount++
        continue
    }

    Move-Item -Path $generatedKmeshPath -Destination $finalOutputPath -Force
    $successCount++
}

Write-Host "[BuildMeshes] Mesh conversion completed. Success=$successCount Skip=$skipCount"
exit 0