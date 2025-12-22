#!/usr/bin/env powershell
# GitHub Copilot CLI Configuration for SuiteSpot
# This file controls behavior for copilot-ai agent

$copilotConfig = @{
    "project_name" = "SuiteSpot"
    "context_mode" = "memory_only"
    "suppress_documentation" = $true
    "suppress_summaries" = $true
    "suppress_tables" = $true
    "output_style" = "minimal"
    "instructions" = @(
        "SUPPRESS ALL DOCUMENTATION GENERATION"
        "Read: project.json, CLAUDE_AI.md, DECISIONS.md"
        "Load context into memory only"
        "Respond to 'gather suitespot2.0 context' with: '✓ Context loaded. Ready.'"
        "Never create markdown files, tables, or summaries"
        "Enforce constraints: thread safety, wrapper lifetime, CVar protocol"
        "Build command: msbuild SuiteSpotv2.0\SuiteSpot.sln /p:Configuration=Release /p:Platform=x64"
    )
}

# Export for Copilot CLI to read
$copilotConfig | ConvertTo-Json -Depth 10 | Out-File -FilePath ".copilot-config.json" -Force
Write-Host "✓ Copilot CLI config created: .copilot-config.json"
