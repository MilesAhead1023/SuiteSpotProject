#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class SuiteSeeker: public BakkesMod::Plugin::BakkesModPlugin
	,public SettingsWindowBase // Render settings in BakkesMod settings menu
{
private:
	// Plugin state
	bool enabled = false;
	bool autoEnableInFreeplay = true;

	// Heatseeker physics settings
	float horizontalSpeed = 1500.0f;  // Default heatseeker horizontal speed
	float verticalSpeed = 800.0f;     // Default heatseeker vertical speed
	float speedMultiplier = 1.0f;     // User-adjustable speed multiplier

	// Internal state
	bool heatseekerActive = false;
	bool isInFreeplay = false;

	// Event handlers
	void OnFreeplayLoad(std::string eventName);
	void OnFreeplayDestroy(std::string eventName);
	void OnBallSpawned(std::string eventName);

	// Core functionality
	void StartHeatseekerMatch();
	void EnableHeatseeker();
	void DisableHeatseeker();
	void ApplyHeatseekerPhysics();

	// Helper methods
	void RegisterCVars();
	void RegisterHooks();

public:
	// BakkesMod lifecycle
	void onLoad() override;
	void onUnload() override;

	// Settings UI
	void RenderSettings() override;

	// Plugin info
	std::string GetPluginName() override;
	void SetImGuiContext(uintptr_t ctx) override;
};
