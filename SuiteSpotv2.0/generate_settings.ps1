# Generate suitespot.set file for BakkesMod settings UI
# Format: type|label|cvar_name|min|max|description
# Types: 1=Checkbox, 3=Number slider, 8=Separator, 9=Comment

$settingsDir = "$env:APPDATA\bakkesmod\bakkesmod\plugins\settings"
$setFile = "$settingsDir\suitespot.set"

# Create directory if it doesn't exist
if (-not (Test-Path $settingsDir)) {
    New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null
    Write-Host "[PostBuild] Created settings directory: $settingsDir"
}

# Generate the .set file content
$content = @"
SuiteSpot
1|Enable SuiteSpot|suitespot_enabled
8|
3|Map Type (0=Freeplay, 1=Training, 2=Workshop)|suitespot_map_type|0|2
1|Auto-Queue After Map Load|suitespot_auto_queue
8|
3|Queue Delay (seconds)|suitespot_delay_queue_sec|0|300
3|Freeplay Delay (seconds)|suitespot_delay_freeplay_sec|0|300
3|Training Delay (seconds)|suitespot_delay_training_sec|0|300
3|Workshop Delay (seconds)|suitespot_delay_workshop_sec|0|300
8|
3|Freeplay Map Index|suitespot_current_freeplay_index|0|1000
3|Training Map Index|suitespot_current_training_index|0|1000
3|Workshop Map Index|suitespot_current_workshop_index|0|1000
8|
9|Settings are automatically saved when changed
9|Use console commands: suitespot_enabled 1, suitespot_map_type 2, etc.
"@

# Write the file
try {
    $content | Out-File -FilePath $setFile -Encoding ASCII -Force
    Write-Host "[PostBuild] Generated settings file: $setFile"
    exit 0
} catch {
    Write-Host "[PostBuild] ERROR: Failed to generate settings file: $_"
    exit 1
}
