# Generate compile_commands.json for LSP/clangd
# Run this after modifying project files or include paths

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

Write-Host "Generating compile_commands.json..." -ForegroundColor Cyan

# Path to the solution
$SolutionPath = Join-Path $PSScriptRoot "SuiteSpotv2.0\SuiteSpot.sln"
$ProjectPath = Join-Path $PSScriptRoot "SuiteSpotv2.0\SuiteSpot.vcxproj"
$OutputPath = Join-Path $PSScriptRoot "compile_commands.json"

# Get BakkesMod path from registry
$BakkesModPath = (Get-ItemProperty -Path "HKCU:\Software\BakkesMod\AppPath" -Name "BakkesModPath" -ErrorAction SilentlyContinue).BakkesModPath

if (-not $BakkesModPath) {
    Write-Warning "BakkesMod path not found in registry. Using default AppData location."
    $BakkesModPath = "$env:APPDATA\bakkesmod\bakkesmod"
}

$BakkesSDKInclude = Join-Path $BakkesModPath "bakkesmodsdk\include"
Write-Host "BakkesMod SDK: $BakkesSDKInclude" -ForegroundColor Yellow

# Fallback to local SDK if registry path doesn't exist
if (-not (Test-Path $BakkesSDKInclude)) {
    $LocalSDK = Join-Path $PSScriptRoot "BakkesModSDK\include"
    if (Test-Path $LocalSDK) {
        Write-Host "Using local BakkesMod SDK: $LocalSDK" -ForegroundColor Yellow
        $BakkesSDKInclude = $LocalSDK
    } else {
        Write-Error "BakkesMod SDK not found! Please install BakkesMod or place SDK in BakkesModSDK\include\"
        exit 1
    }
}

# Get project directory
$ProjectDir = Join-Path $PSScriptRoot "SuiteSpotv2.0"

# Get source files from project
[xml]$vcxproj = Get-Content $ProjectPath
$sourceFiles = $vcxproj.Project.ItemGroup.ClCompile.Include | Where-Object { $_ }

# Build compile commands JSON
$commands = @()

foreach ($sourceFile in $sourceFiles) {
    $fullPath = Join-Path $ProjectDir $sourceFile
    $fullPath = $fullPath -replace '\\', '/'  # Normalize path separators

    # Check if file exists
    if (-not (Test-Path $fullPath)) {
        Write-Warning "Skipping missing file: $sourceFile"
        continue
    }

    # Special handling for pch.cpp (creates precompiled header)
    $pchFlag = if ($sourceFile -like "*pch.cpp") { "/Yc`"pch.h`"" } else { "/Yu`"pch.h`"" }

    # Skip precompiled header usage for imgui files
    if ($sourceFile -like "*imgui*") {
        $pchFlag = ""
    }

    $command = @{
        directory = $ProjectDir
        command = "cl.exe /c /std:c++20 /EHsc /MT /W3 /DNDEBUG /D_CONSOLE /DUNICODE /D_UNICODE /I`"$ProjectDir`" /I`"$BakkesSDKInclude`" /I`"$ProjectDir\imgui`" /I`"C:\Users\bmile\vcpkg\installed\x64-windows\include`" $pchFlag `"$fullPath`""
        file = $fullPath
    }

    $commands += $command
}

# Write JSON
$json = ConvertTo-Json $commands -Depth 10
Set-Content -Path $OutputPath -Value $json -Encoding UTF8

Write-Host "`n✓ Success! Created: $OutputPath" -ForegroundColor Green
Write-Host "✓ Found $($commands.Count) source files" -ForegroundColor Green
Write-Host "`nNext steps:" -ForegroundColor Cyan
Write-Host "  1. Install clangd extension in VS Code (recommended)" -ForegroundColor White
Write-Host "  2. Reload VS Code window (Ctrl+Shift+P -> 'Reload Window')" -ForegroundColor White
Write-Host "  3. Enjoy IntelliSense for BakkesMod wrappers!" -ForegroundColor White
