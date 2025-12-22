#include "pch.h"
#include "SuiteSeeker.h"

// Plugin registration macro - tells BakkesMod about our plugin
BAKKESMOD_PLUGIN(SuiteSeeker, "Heatseeker training mode for freeplay", plugin_version, PLUGINTYPE_FREEPLAY)

// Global CVar manager for logging (required by BakkesMod SDK)
std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

// ============================================================================
// LIFECYCLE METHODS
// ============================================================================

void SuiteSeeker::onLoad()
{
	_globalCvarManager = cvarManager;
	LOG("SuiteSeeker: Plugin loaded!");
	DEBUGLOG("SuiteSeeker: Debug mode enabled");

	// Register CVars and hooks
	RegisterCVars();
	RegisterHooks();

	LOG("SuiteSeeker: Initialization complete");
}

void SuiteSeeker::onUnload()
{
	LOG("SuiteSeeker: Plugin unloaded");
}

std::string SuiteSeeker::GetPluginName()
{
	return "SuiteSeeker";
}

void SuiteSeeker::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

// ============================================================================
// CVAR REGISTRATION
// ============================================================================

void SuiteSeeker::RegisterCVars()
{
	// Main enable/disable toggle
	cvarManager->registerCvar("suiteseeker_enabled", "0",
		"Enable SuiteSeeker plugin", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			enabled = cvar.getBoolValue();
			LOG("SuiteSeeker: " + std::string(enabled ? "Enabled" : "Disabled"));

			// Apply state immediately if in freeplay
			if (enabled && isInFreeplay) {
				EnableHeatseeker();
			} else if (!enabled && heatseekerActive) {
				DisableHeatseeker();
			}
		});

	// Auto-enable in freeplay
	cvarManager->registerCvar("suiteseeker_auto_enable", "1",
		"Automatically enable Heatseeker when entering freeplay", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			autoEnableInFreeplay = cvar.getBoolValue();
		});

	// Speed multiplier
	cvarManager->registerCvar("suiteseeker_speed_multiplier", "1.0",
		"Speed multiplier for Heatseeker ball (0.5 = slow, 2.0 = fast)", true, true, 0.5, true, 3.0)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			speedMultiplier = cvar.getFloatValue();
			LOG("SuiteSeeker: Speed multiplier set to " + std::to_string(speedMultiplier));
		});

	// Horizontal speed
	cvarManager->registerCvar("suiteseeker_horizontal_speed", "1500.0",
		"Horizontal speed for Heatseeker ball", true, true, 500.0, true, 3000.0)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			horizontalSpeed = cvar.getFloatValue();
		});

	// Vertical speed
	cvarManager->registerCvar("suiteseeker_vertical_speed", "800.0",
		"Vertical speed for Heatseeker ball", true, true, 200.0, true, 2000.0)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			verticalSpeed = cvar.getFloatValue();
		});

	// Manual toggle notifier
	cvarManager->registerNotifier("suiteseeker_toggle",
		[this](std::vector<std::string> args) {
			if (heatseekerActive) {
				DisableHeatseeker();
				cvarManager->log("SuiteSeeker: Disabled");
			} else {
				EnableHeatseeker();
				cvarManager->log("SuiteSeeker: Enabled");
			}
		},
		"Toggle Heatseeker mode on/off",
		PERMISSION_ALL);
}

// ============================================================================
// EVENT HOOK REGISTRATION
// ============================================================================

void SuiteSeeker::RegisterHooks()
{
	// Hook freeplay load events
	// Note: Using generic event hooks since we're monitoring game state changes
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.InitGame",
		std::bind(&SuiteSeeker::OnFreeplayLoad, this, std::placeholders::_1));

	// Hook when leaving freeplay
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed",
		std::bind(&SuiteSeeker::OnFreeplayDestroy, this, std::placeholders::_1));

	// Hook ball spawned event
	gameWrapper->HookEvent("Function TAGame.Ball_TA.Spawned",
		std::bind(&SuiteSeeker::OnBallSpawned, this, std::placeholders::_1));

	// Register drawable for continuous physics application (called every frame)
	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		// Apply Heatseeker physics every frame when active
		if (enabled && heatseekerActive && isInFreeplay) {
			ApplyHeatseekerPhysics();
		}
	});

	LOG("SuiteSeeker: Event hooks registered");
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void SuiteSeeker::OnFreeplayLoad(std::string eventName)
{
	// CRITICAL: Use SetTimeout for thread-safe game state access
	gameWrapper->SetTimeout([this](GameWrapper* gw) {
		ServerWrapper server = gw->GetCurrentGameState();
		if (server.IsNull()) return;

		// Check if we're in freeplay (not a match)
		// Freeplay characteristics: single player, no online match
		auto cars = server.GetCars();
		if (cars.Count() <= 1) {
			isInFreeplay = true;
			LOG("SuiteSeeker: Freeplay detected");

			// Auto-enable if configured
			if (enabled && autoEnableInFreeplay) {
				EnableHeatseeker();
			}
		}
	}, 0.5f); // Small delay to ensure server is fully initialized
}

void SuiteSeeker::OnFreeplayDestroy(std::string eventName)
{
	isInFreeplay = false;
	heatseekerActive = false;
	LOG("SuiteSeeker: Left freeplay");
}

void SuiteSeeker::OnBallSpawned(std::string eventName)
{
	// If Heatseeker is active, apply physics when ball spawns
	if (heatseekerActive && enabled) {
		gameWrapper->SetTimeout([this](GameWrapper* gw) {
			ApplyHeatseekerPhysics();
		}, 0.1f);
	}
}

// ============================================================================
// CORE FUNCTIONALITY
// ============================================================================

void SuiteSeeker::StartHeatseekerMatch()
{
	gameWrapper->Execute([this](GameWrapper* gw) {
		// Get matchmaking wrapper
		MatchmakingWrapper mm = gw->GetMatchmakingWrapper();
		if (mm.IsNull()) {
			LOG("SuiteSeeker: Matchmaking wrapper is null");
			cvarManager->log("ERROR: Could not access matchmaking");
			return;
		}

		// Configure custom match settings for Heatseeker
		CustomMatchSettings settings;
		settings.GameMode = 6;  // Heatseeker mode
		settings.ServerName = "SuiteSeeker Training";
		settings.MaxPlayerCount = 2;  // Just the player, no bots
		settings.bPartyMembersOnly = true;  // Solo practice

		// GameTags = mutators (comma separated)
		// Goal blocker for opponent's goal - we need to determine which team player is on
		// For now, enable goal blocker - player will be on blue team by default
		settings.GameTags = "BoomerMode";  // Start with standard heatseeker mutator

		// Default map
		settings.MapName = "Stadium_P";  // DFH Stadium

		// Team settings (empty, no bots)
		settings.BlueTeamSettings.Name = "Player";
		settings.OrangeTeamSettings.Name = "Opponent";

		// Create the private match
		mm.CreatePrivateMatch(Region::USE, 0, settings);

		LOG("SuiteSeeker: Created Heatseeker private match");
		cvarManager->log("Starting Heatseeker training match...");
	});
}

void SuiteSeeker::EnableHeatseeker()
{
	if (heatseekerActive) {
		LOG("SuiteSeeker: Already active");
		return;
	}

	// Manual physics approach - no server commands needed
	// The RegisterDrawable hook will apply forces every frame
	heatseekerActive = true;
	LOG("SuiteSeeker: Heatseeker mode enabled - applying manual physics");
	cvarManager->log("Heatseeker mode ACTIVE! Ball will seek goals.");
}

void SuiteSeeker::DisableHeatseeker()
{
	if (!heatseekerActive) {
		return;
	}

	// Simply stop applying forces - RegisterDrawable will check heatseekerActive
	heatseekerActive = false;
	LOG("SuiteSeeker: Heatseeker mode disabled");
	cvarManager->log("Heatseeker mode disabled. Normal ball physics restored.");
}

void SuiteSeeker::ApplyHeatseekerPhysics()
{
	// Manually apply Heatseeker-like physics by adding force toward nearest goal
	// NOTE: This is called from RegisterDrawable which runs on game thread,
	// so we don't need Execute/SetTimeout here (would cause nested thread sync)

	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (server.IsNull()) return;

	BallWrapper ball = server.GetBall();
	if (ball.IsNull()) return;

	// Get ball position
	Vector ballPos = ball.GetLocation();

	// Find nearest goal
	ArrayWrapper<GoalWrapper> goals = server.GetGoals();
	if (goals.IsNull()) return;
	if (goals.Count() < 2) return;

	GoalWrapper goal1 = goals.Get(0);
	GoalWrapper goal2 = goals.Get(1);
	if (goal1.IsNull() || goal2.IsNull()) return;

	Vector goal1Pos = goal1.GetLocation();
	Vector goal2Pos = goal2.GetLocation();

	// Calculate distances to both goals
	Vector toGoal1 = goal1Pos - ballPos;
	Vector toGoal2 = goal2Pos - ballPos;

	float dist1 = toGoal1.magnitude();
	float dist2 = toGoal2.magnitude();

	// Choose nearest goal
	Vector targetDir = (dist1 < dist2) ? toGoal1 : toGoal2;
	targetDir = targetDir.getNormalized();

	// Calculate force magnitude based on speed multiplier
	float forceMagnitude = horizontalSpeed * speedMultiplier * 10.0f;  // Scale factor

	Vector force = targetDir * forceMagnitude;

	// Apply force to ball (ForceMode: 0 = Force, 1 = Impulse, 2 = VelocityChange, 3 = Acceleration)
	ball.AddForce(force, 0);  // Continuous force application
}

// ============================================================================
// SETTINGS UI
// ============================================================================

void SuiteSeeker::RenderSettings()
{
	ImGui::TextUnformatted("SuiteSeeker - Heatseeker Training Mode");
	ImGui::Separator();
	ImGui::Spacing();

	// Main action button
	ImGui::TextWrapped("Click the button below to start a Heatseeker training match.\n"
		"The match will have no bots and goal blocker for opponent's goal.");
	ImGui::Spacing();

	if (ImGui::Button("Start Heatseeker Match", ImVec2(200, 40))) {
		StartHeatseekerMatch();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Creates a private Heatseeker match for solo training");
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Info text
	ImGui::TextWrapped("Match Settings:");
	ImGui::BulletText("Game Mode: Heatseeker");
	ImGui::BulletText("Map: DFH Stadium");
	ImGui::BulletText("Bots: None");
	ImGui::BulletText("Max Players: 1 (Solo)");
	ImGui::BulletText("Mutators: BoomerMode (standard heatseeker)");

	ImGui::Spacing();
	ImGui::TextWrapped("Note: The match will be created as a private match. "
		"You'll be placed on the Blue team by default.");
}
