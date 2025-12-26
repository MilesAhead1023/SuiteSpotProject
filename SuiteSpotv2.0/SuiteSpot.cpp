#include "pch.h"
#include "SuiteSpot.h"
#include "MapList.h"
#include "MapManager.h"
#include "SettingsSync.h"
#include "AutoLoadFeature.h"
#include "PrejumpPackManager.h"
#include "SettingsUI.h"
#include "PrejumpUI.h"
#include "LoadoutUI.h"
#include "OverlayRenderer.h"
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

// ===== SuiteSpot persistence helpers =====
std::filesystem::path SuiteSpot::GetDataRoot() const {
    return mapManager ? mapManager->GetDataRoot() : std::filesystem::path();
}
std::filesystem::path SuiteSpot::GetSuiteTrainingDir() const {
    return mapManager ? mapManager->GetSuiteTrainingDir() : std::filesystem::path();
}
std::filesystem::path SuiteSpot::GetTrainingFilePath() const {
    return mapManager ? mapManager->GetTrainingFilePath() : std::filesystem::path();
}
std::filesystem::path SuiteSpot::GetShuffleBagPath() const {
    return mapManager ? mapManager->GetShuffleBagPath() : std::filesystem::path();
}
void SuiteSpot::EnsureDataDirectories() const {
    if (mapManager) {
        mapManager->EnsureDataDirectories();
    }
}

int SuiteSpot::GetRandomTrainingIndex() const {
    return mapManager ? mapManager->GetRandomTrainingMap(RLTraining, trainingShuffleBag) : 0;
}

void SuiteSpot::LoadTrainingMaps() {
    if (mapManager) {
        int index = settingsSync ? settingsSync->GetCurrentTrainingIndex() : 0;
        mapManager->LoadTrainingMaps(RLTraining, index);
        if (settingsSync) {
            settingsSync->SetCurrentTrainingIndex(index);
        }
    }
}

void SuiteSpot::SaveTrainingMaps() const {
    if (mapManager) {
        mapManager->SaveTrainingMaps(RLTraining);
    }
}

void SuiteSpot::LoadShuffleBag() {
    if (mapManager) {
        mapManager->LoadShuffleBag(trainingShuffleBag, selectedTrainingIndices, RLTraining);
    }
}

void SuiteSpot::SaveShuffleBag() const {
    if (mapManager) {
        mapManager->SaveShuffleBag(trainingShuffleBag);
    }
}

void SuiteSpot::EnsureReadmeFiles() const {
    if (mapManager) {
        mapManager->EnsureReadmeFiles();
    }
}

std::filesystem::path SuiteSpot::GetWorkshopLoaderConfigPath() const {
    return mapManager ? mapManager->GetWorkshopLoaderConfigPath() : std::filesystem::path();
}

std::filesystem::path SuiteSpot::ResolveConfiguredWorkshopRoot() const {
    return mapManager ? mapManager->ResolveConfiguredWorkshopRoot() : std::filesystem::path();
}

void SuiteSpot::DiscoverWorkshopInDir(const std::filesystem::path& dir) {
    if (mapManager) {
        mapManager->DiscoverWorkshopInDir(dir, RLWorkshop);
    }
}

void SuiteSpot::LoadWorkshopMaps() {
    if (mapManager) {
        int index = settingsSync ? settingsSync->GetCurrentWorkshopIndex() : 0;
        mapManager->LoadWorkshopMaps(RLWorkshop, index);
        if (settingsSync) {
            settingsSync->SetCurrentWorkshopIndex(index);
        }
    }
}

void SuiteSpot::SaveWorkshopMaps() const {
    // No-op
}

// ===== PREJUMP SCRAPER INTEGRATION =====
bool SuiteSpot::IsEnabled() const {
    return settingsSync ? settingsSync->IsEnabled() : false;
}

bool SuiteSpot::IsAutoQueueEnabled() const {
    return settingsSync ? settingsSync->IsAutoQueue() : false;
}

bool SuiteSpot::IsTrainingShuffleEnabled() const {
    return settingsSync ? settingsSync->IsTrainingShuffleEnabled() : false;
}

int SuiteSpot::GetMapType() const {
    return settingsSync ? settingsSync->GetMapType() : 0;
}

int SuiteSpot::GetDelayQueueSec() const {
    return settingsSync ? settingsSync->GetDelayQueueSec() : 0;
}

int SuiteSpot::GetDelayFreeplaySec() const {
    return settingsSync ? settingsSync->GetDelayFreeplaySec() : 0;
}

int SuiteSpot::GetDelayTrainingSec() const {
    return settingsSync ? settingsSync->GetDelayTrainingSec() : 0;
}

int SuiteSpot::GetDelayWorkshopSec() const {
    return settingsSync ? settingsSync->GetDelayWorkshopSec() : 0;
}

int SuiteSpot::GetCurrentIndex() const {
    return settingsSync ? settingsSync->GetCurrentIndex() : 0;
}

int SuiteSpot::GetCurrentTrainingIndex() const {
    return settingsSync ? settingsSync->GetCurrentTrainingIndex() : 0;
}

int SuiteSpot::GetCurrentWorkshopIndex() const {
    return settingsSync ? settingsSync->GetCurrentWorkshopIndex() : 0;
}

int SuiteSpot::GetTrainingBagSize() const {
    return settingsSync ? settingsSync->GetTrainingBagSize() : 0;
}

float SuiteSpot::GetPostMatchDurationSec() const {
    return settingsSync ? settingsSync->GetPostMatchDurationSec() : 15.0f;
}

float SuiteSpot::GetOverlayWidth() const {
    return settingsSync ? settingsSync->GetOverlayWidth() : 880.0f;
}

float SuiteSpot::GetOverlayHeight() const {
    return settingsSync ? settingsSync->GetOverlayHeight() : 400.0f;
}

float SuiteSpot::GetOverlayAlpha() const {
    return settingsSync ? settingsSync->GetOverlayAlpha() : 0.85f;
}

float SuiteSpot::GetBlueTeamHue() const {
    return settingsSync ? settingsSync->GetBlueTeamHue() : 240.0f;
}

float SuiteSpot::GetOrangeTeamHue() const {
    return settingsSync ? settingsSync->GetOrangeTeamHue() : 25.0f;
}

PostMatchOverlayWindow::PostMatchOverlayWindow(SuiteSpot* plugin) : plugin_(plugin) {
    isWindowOpen_ = false;
}

void PostMatchOverlayWindow::Open() {
    isWindowOpen_ = true;
}

void PostMatchOverlayWindow::Close() {
    isWindowOpen_ = false;
}

void PostMatchOverlayWindow::Render() {
    if (!isWindowOpen_) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoNavFocus |
                             ImGuiWindowFlags_NoBackground;

    float offsetX = 0.0f;
    float offsetY = 0.0f;
    if (plugin_->GetOverlayRenderer()) {
        offsetX = plugin_->GetOverlayRenderer()->GetOverlayOffsetX();
        offsetY = plugin_->GetOverlayRenderer()->GetOverlayOffsetY();
    }
    ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 overlaySize = ImVec2(std::max(400.0f, plugin_->GetOverlayWidth()),
                                      std::max(180.0f, plugin_->GetOverlayHeight()));
    ImVec2 pos = ImVec2((display.x - overlaySize.x) * 0.5f + offsetX, display.y * 0.08f + offsetY);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(overlaySize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    
    if (!ImGui::Begin(GetMenuTitle().c_str(), &isWindowOpen_, flags)) {
        ImGui::End();
        return;
    }

    RenderWindow();
    ImGui::End();
}

void PostMatchOverlayWindow::RenderWindow() {
    if (plugin_->GetOverlayRenderer()) {
        plugin_->GetOverlayRenderer()->RenderPostMatchOverlay();
    }
}

std::filesystem::path SuiteSpot::GetPrejumpPacksPath() const {
    return GetSuiteTrainingDir() / "prejump_packs.json";
}

bool SuiteSpot::IsPrejumpCacheStale() const {
    return prejumpMgr ? prejumpMgr->IsCacheStale(GetPrejumpPacksPath()) : true;
}

std::string SuiteSpot::FormatLastUpdatedTime() const {
    return prejumpMgr ? prejumpMgr->GetLastUpdatedTime(GetPrejumpPacksPath()) : "Unknown";
}

void SuiteSpot::LoadPrejumpPacksFromFile(const std::filesystem::path& filePath) {
    if (prejumpMgr) {
        prejumpMgr->LoadPacksFromFile(filePath);
    }
}

// #detailed comments: ScrapeAndLoadPrejumpPacks
// Purpose: Launches an external PowerShell script to scrape Prejump.com
// and write a JSON cache to disk. This is intentionally performed in a
// background task (scheduled via gameWrapper->SetTimeout) to avoid any
// blocking on the UI/game thread.
//
// Safety and behavior notes:
//  - prejumpScrapingInProgress is a guard flag ensuring only one scrape
//    runs at a time. It is set before scheduling and cleared when the
//    background process finishes.
//  - The implementation uses system() and relies on the platform's
//    default process creation semantics; this must remain as-is for
//    portability with existing deployments. If this is changed to a
//    more advanced process API, ensure identical detach/exit semantics.
//  - The script path is hard-coded to the repo dev path; callers should
//    ensure that the script is present when invoking this routine.
//
// DO NOT CHANGE: Modifying timing (the 0.1f scheduling) or the way the
// result is checked could resurface race conditions that previously
// required this exact coordination.
void SuiteSpot::ScrapeAndLoadPrejumpPacks() {
    if (prejumpMgr) {
        prejumpMgr->ScrapeAndLoadPrejumpPacks(GetPrejumpPacksPath(), gameWrapper);
    }
}


using namespace std;
using namespace std::chrono_literals;

BAKKESMOD_PLUGIN(SuiteSpot, "SuiteSpot", plugin_version, PLUGINTYPE_FREEPLAY)

shared_ptr<CVarManagerWrapper> _globalCvarManager;

void SuiteSpot::LoadHooks() {
    // Re-queue/transition at match end or when main menu appears after a match
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", bind(&SuiteSpot::GameEndedEvent, this, placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.AchievementManager_TA.HandleMatchEnded", bind(&SuiteSpot::GameEndedEvent, this, placeholders::_1));
}

// #detailed comments: GameEndedEvent
// Purpose: Called by hooked game events when a match ends. The function
// must capture final match state (team scores, PRIs) as quickly as
// possible and prepare the post-match overlay and optional subsequent
// automation (map load, queueing). This must be robust: exceptions
// are caught and logged to avoid destabilizing the host.
//
// Timing and ordering notes:
//  - Capture scores and PRIs immediately; the game may transition state
//    between frames so delaying capture risks losing final values.
//  - The postMatch.start timestamp is recorded with steady_clock so
//    overlay lifetime calculations are not affected by system clock
//    adjustments.
//
// DO NOT CHANGE: The safeExecute lambda intentionally accepts a delay
// (in seconds) and either executes immediately or schedules via
// gameWrapper->SetTimeout. Changing its semantics will alter when
// external commands (load_freeplay, queue, etc.) are run relative to
// overlay presentation.
void SuiteSpot::GameEndedEvent(std::string name) {
    if (!IsEnabled()) return;

    // Capture final scores before any transitions
    try {
        auto server = gameWrapper->GetGameEventAsServer();
        if (!server.IsNull()) {
            auto teams = server.GetTeams();
            TeamWrapper myTeam(static_cast<uintptr_t>(0));
            TeamWrapper oppTeam(static_cast<uintptr_t>(0));

            int myTeamIndex = -1;
            auto pc = gameWrapper->GetPlayerController();
            if (!pc.IsNull()) {
                auto pri = pc.GetPRI();
                if (!pri.IsNull()) {
                    auto team = pri.GetTeam();
                    if (!team.IsNull()) {
                        myTeamIndex = team.GetTeamIndex();
                    }
                }
            }

            for (int i = 0; i < teams.Count(); ++i) {
                TeamWrapper tw = teams.Get(i);
                if (tw.IsNull()) continue;
                if (tw.GetTeamIndex() == myTeamIndex) {
                    myTeam = tw;
                } else if (oppTeam.IsNull()) {
                    oppTeam = tw;
                }
            }

            // Fallback selection if we couldn't match by index
            if (myTeam.IsNull() && teams.Count() > 0) {
                myTeam = teams.Get(0);
            }
            if (oppTeam.IsNull() && teams.Count() > 1) {
                oppTeam = teams.Get(1);
            }

            if (!myTeam.IsNull() && !oppTeam.IsNull()) {
                auto nameFromTeam = [](TeamWrapper& t, const std::string& fallback) {
                    std::string result = fallback;
                    auto custom = t.GetCustomTeamName();
                    if (!custom.IsNull()) {
                        auto str = custom.ToString();
                        if (!str.empty()) result = str;
                    }
                    auto baseName = t.GetTeamName();
                    if (!baseName.IsNull()) {
                        auto str = baseName.ToString();
                        if (!str.empty()) result = str;
                    }
                    return result;
                };

                // Build player rows from PRIs
                postMatch.players.clear();
                try {
                    auto pris = server.GetPRIs();
                    for (int i = 0; i < pris.Count(); ++i) {
                        PriWrapper pri = pris.Get(i);
                        if (pri.IsNull()) continue;

                        PostMatchPlayerRow row;
                        row.teamIndex = pri.GetTeam().IsNull() ? -1 : pri.GetTeam().GetTeamIndex();
                        row.isLocal = pri.IsLocalPlayerPRI();

                        auto name = pri.GetPlayerName();
                        row.name = name.IsNull() ? "" : name.ToString();
                        row.score = pri.GetMatchScore();
                        row.goals = pri.GetMatchGoals();
                        row.assists = pri.GetMatchAssists();
                        row.saves = pri.GetMatchSaves();
                        row.shots = pri.GetMatchShots();  // New: shots on goal
                        
                        // Network quality (0-255, convert to ping-like display)
                        unsigned char netQuality = pri.GetReplicatedWorstNetQualityBeyondLatency();
                        row.ping = static_cast<int>(netQuality * 2); // Rough conversion for display
                        
                        // Simple MVP detection (highest score on team for now)
                        row.isMVP = false; // Will be calculated after all players are collected

                        postMatch.players.push_back(row);
                    }
                } catch (...) {
                    // ignore; overlay still shows team scores
                }

                // Sort rows by team, score desc then name
                std::sort(postMatch.players.begin(), postMatch.players.end(), [](const PostMatchPlayerRow& a, const PostMatchPlayerRow& b) {
                    if (a.teamIndex != b.teamIndex) return a.teamIndex < b.teamIndex;
                    if (a.score != b.score) return a.score > b.score;
                    return a.name < b.name;
                });

                // Mark MVPs (highest scoring player on each team)
                std::map<int, int> teamHighScores;
                for (auto& row : postMatch.players) {
                    if (teamHighScores.find(row.teamIndex) == teamHighScores.end() || 
                        row.score > teamHighScores[row.teamIndex]) {
                        teamHighScores[row.teamIndex] = row.score;
                    }
                }
                for (auto& row : postMatch.players) {
                    row.isMVP = (row.score == teamHighScores[row.teamIndex] && row.score > 0);
                }

                postMatch.myScore = myTeam.GetScore();
                postMatch.oppScore = oppTeam.GetScore();
                postMatch.myTeamName = nameFromTeam(myTeam, "My Team");
                postMatch.oppTeamName = nameFromTeam(oppTeam, "Opponents");
                postMatch.playlist = server.GetMatchTypeName();
                postMatch.overtime = !!server.GetbOverTime();
                postMatch.myColor = myTeam.GetFontColor();
                postMatch.oppColor = oppTeam.GetFontColor();

                postMatch.start = std::chrono::steady_clock::now();
                postMatch.active = true;
                if (postMatchOverlayWindow) {
                    postMatchOverlayWindow->Open();
                }
                LOG("SuiteSpot: Post-match overlay activated - {} vs {}, Score: {}-{}",
                    postMatch.myTeamName, postMatch.oppTeamName, postMatch.myScore, postMatch.oppScore);
                // Overlay is now independent from settings window - no menu toggle needed
            }
        }
    } catch (const std::exception& e) {
        LOG("SuiteSpot: Failed to capture post-match overlay data: {}", e.what());
    }

    if (autoLoadFeature && settingsSync) {
        autoLoadFeature->OnMatchEnded(gameWrapper, cvarManager, RLMaps, RLTraining, RLWorkshop,
            trainingShuffleBag, *settingsSync);
    }
}

void SuiteSpot::onLoad() {
    _globalCvarManager = cvarManager;
    LOG("SuiteSpot loaded");
    mapManager = new MapManager();
    settingsSync = new SettingsSync();
    autoLoadFeature = new AutoLoadFeature();
    prejumpMgr = new PrejumpPackManager();
    settingsUI = new SettingsUI(this);
    prejumpUI = new PrejumpUI(this);
    loadoutUI = new LoadoutUI(this);
    overlayRenderer = new OverlayRenderer(this);
    EnsureDataDirectories();
    EnsureReadmeFiles();
    LoadTrainingMaps();
    LoadWorkshopMaps();
    LoadShuffleBag();
    
    // Initialize LoadoutManager
    loadoutManager = std::make_unique<LoadoutManager>(gameWrapper);
    LOG("SuiteSpot: LoadoutManager initialized");
    
    // Check Prejump cache and load if available
    if (prejumpMgr) {
        if (!std::filesystem::exists(GetPrejumpPacksPath())) {
            LOG("SuiteSpot: No Prejump cache found. Schedule scraping on next opportunity.");
            // Will be scraped on first Settings render or user request
        } else {
            // Load existing Prejump cache
            prejumpMgr->LoadPacksFromFile(GetPrejumpPacksPath());
            LOG("SuiteSpot: Prejump cache loaded");
        }
    }
    
    LoadHooks();

    // Initialize windows
    postMatchOverlayWindow = std::make_shared<PostMatchOverlayWindow>(this);

    // Register test overlay toggle
    cvarManager->registerNotifier("ss_testoverlay", [this](std::vector<std::string> args) {
        // Mock data if empty for testing
        if (postMatch.players.empty()) {
            postMatch.myTeamName = "Blue Team";
            postMatch.oppTeamName = "Orange Team";
            postMatch.myScore = 3;
            postMatch.oppScore = 2;
            postMatch.playlist = "Competitive Doubles";
            postMatch.overtime = false;
            
            PostMatchPlayerRow p1; p1.name = "LocalPlayer"; p1.score = 650; p1.goals = 2; p1.isLocal = true; p1.teamIndex = 0; p1.isMVP = true;
            PostMatchPlayerRow p2; p2.name = "Teammate"; p2.score = 400; p2.goals = 1; p2.isLocal = false; p2.teamIndex = 0;
            PostMatchPlayerRow p3; p3.name = "Opponent 1"; p3.score = 500; p3.goals = 1; p3.isLocal = false; p3.teamIndex = 1; p3.isMVP = true;
            PostMatchPlayerRow p4; p4.name = "Opponent 2"; p4.score = 300; p4.goals = 1; p4.isLocal = false; p4.teamIndex = 1;
            
            postMatch.players = { p1, p2, p3, p4 };
        }
        
        if (!postMatch.active) {
            postMatch.start = std::chrono::steady_clock::now();
            postMatch.active = true;
            postMatchOverlayWindow->Open();
            LOG("SuiteSpot: Test overlay ACTIVATED via ss_testoverlay");
        } else {
            postMatch.active = false;
            postMatchOverlayWindow->Close();
            LOG("SuiteSpot: Test overlay DEACTIVATED via ss_testoverlay");
        }
    }, "Toggle the SuiteSpot test overlay", PERMISSION_ALL);

    if (settingsSync) {
        settingsSync->RegisterAllCVars(cvarManager);
        settingsSync->UpdateTrainingBagSize(static_cast<int>(trainingShuffleBag.size()), cvarManager);
    }
    
    LOG("SuiteSpot: Plugin initialization complete");
}



void SuiteSpot::RenderPostMatchOverlay() {
    if (overlayRenderer) {
        overlayRenderer->RenderPostMatchOverlay();
    }
}

void SuiteSpot::onUnload() {
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded");
    gameWrapper->UnhookEvent("Function TAGame.AchievementManager_TA.HandleMatchEnded");
    delete overlayRenderer;
    overlayRenderer = nullptr;
    delete settingsUI;
    settingsUI = nullptr;
    delete prejumpUI;
    prejumpUI = nullptr;
    delete loadoutUI;
    loadoutUI = nullptr;
    delete prejumpMgr;
    prejumpMgr = nullptr;
    delete autoLoadFeature;
    autoLoadFeature = nullptr;
    delete settingsSync;
    settingsSync = nullptr;
    delete mapManager;
    mapManager = nullptr;
    LOG("SuiteSpot unloaded");
}
