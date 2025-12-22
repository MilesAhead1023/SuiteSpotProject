# SuiteSeeker Build Script
# Finds MSBuild and builds the plugin

Write-Host "=== SuiteSeeker Build Script ===" -ForegroundColor Cyan

# Method 1: Check for VS 2022 Professional first (user's preferred location)
$vs2022ProfessionalPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"

if (Test-Path $vs2022ProfessionalPath) {
    Write-Host "Found MSBuild (VS 2022 Professional): $vs2022ProfessionalPath" -ForegroundColor Green
    $msbuildPath = $vs2022ProfessionalPath
} else {
    Write-Host "VS 2022 Professional MSBuild not found. Checking other locations..." -ForegroundColor Yellow

    # Method 2: Try to find MSBuild in PATH
    $msbuild = Get-Command msbuild -ErrorAction SilentlyContinue

    if ($msbuild) {
        Write-Host "Found MSBuild in PATH: $($msbuild.Path)" -ForegroundColor Green
        $msbuildPath = $msbuild.Path
    } else {
        Write-Host "MSBuild not in PATH. Using vswhere to search..." -ForegroundColor Yellow

        # Method 3: Use vswhere to find Visual Studio
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

        if (Test-Path $vsWhere) {
            Write-Host "Using vswhere to locate Visual Studio..." -ForegroundColor Yellow
            $vsPath = & $vsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath

            if ($vsPath) {
                $msbuildPath = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"

                if (Test-Path $msbuildPath) {
                    Write-Host "Found MSBuild via vswhere: $msbuildPath" -ForegroundColor Green
                } else {
                    Write-Host "ERROR: MSBuild.exe not found at expected location!" -ForegroundColor Red
                    exit 1
                }
            } else {
                Write-Host "ERROR: Visual Studio installation not found!" -ForegroundColor Red
                exit 1
            }
        } else {
            Write-Host "ERROR: vswhere.exe not found. Is Visual Studio installed?" -ForegroundColor Red
            exit 1
        }
    }
}

# Build the solution
Write-Host "`nBuilding SuiteSeeker.sln..." -ForegroundColor Cyan
Write-Host "Configuration: Release | Platform: x64" -ForegroundColor Gray

& $msbuildPath "SuiteSeeker.sln" /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== BUILD SUCCESSFUL ===" -ForegroundColor Green

    # Check output
    $dllPath = "plugins\SuiteSeeker.dll"
    if (Test-Path $dllPath) {
        $fileInfo = Get-Item $dllPath
        Write-Host "`nOutput: $dllPath" -ForegroundColor Green
        Write-Host "Size: $($fileInfo.Length) bytes" -ForegroundColor Gray
        Write-Host "Modified: $($fileInfo.LastWriteTime)" -ForegroundColor Gray
    } else {
        Write-Host "`nWARNING: DLL not found at expected location!" -ForegroundColor Yellow
    }
} else {
    Write-Host "`n=== BUILD FAILED ===" -ForegroundColor Red
    Write-Host "Exit code: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
