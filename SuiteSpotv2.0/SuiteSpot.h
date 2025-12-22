#pragma once
#include "GuiBase.h" // defines SettingsWindowBase
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "MapList.h"
#include "LoadoutManager.h"
#include "version.h"
#include <filesystem>
#include <set>
#include <memory>

// Forward declarations for additional windows
class SuiteSpotSettingsWindow2;
class SuiteSpotTestWindow;
class SuiteSpotPostMatchWindow;

// External helpers
void SaveTrainingMaps(std::shared_ptr<CVarManagerWrapper> cvarManager, const std::vector<TrainingEntry>& RLTraining);
void LoadTrainingMaps(std::shared_ptr<CVarManagerWrapper> cvarManager, std::vector<TrainingEntry>& RLTraining);

// Version macro carried over from the master template
constexpr auto plugin_version =
    stringify(VERSION_MAJOR) "."
    stringify(VERSION_MINOR) "."
    stringify(VERSION_PATCH) "."
    stringify(VERSION_BUILD);

struct PostMatchPlayerRow {
    int teamIndex = -1;
    bool isLocal = false;
    std::string name;
    int score = 0;
    int goals = 0;
    int assists = 0;
    int saves = 0;
    int shots = 0;
    int ping = 0;
    bool isMVP = false;
};

struct PostMatchInfo {
    bool active = false;
    std::chrono::steady_clock::time_point start;
    int myScore = 0;
    int oppScore = 0;
    std::string myTeamName;
    std::string oppTeamName;
    std::string playlist;
    bool overtime = false;
    LinearColor myColor{};
    LinearColor oppColor{};
    std::vector<PostMatchPlayerRow> players;
};

// NOTE: inherit from SettingsWindowBase (not “GuiBase”)
class SuiteSpot final : public BakkesMod::Plugin::BakkesModPlugin,
                        public SettingsWindowBase,
                        public PluginWindowBase
{
public:
    // Persistence folders and files under %APPDATA%\bakkesmod\bakkesmod\data
    void EnsureDataDirectories() const;
    // ... (rest of persistence methods)
    std::filesystem::path GetDataRoot() const;
    std::filesystem::path GetSuiteTrainingDir() const;
    std::filesystem::path GetTrainingFilePath() const;   // SuiteTraining\SuiteSpotTrainingMaps.txt

    // Persistence API
    void LoadTrainingMaps();
    void SaveTrainingMaps() const;
    void LoadWorkshopMaps();
    void SaveWorkshopMaps() const; // no-op (legacy)
    void DiscoverWorkshopInDir(const std::filesystem::path& dir);
    std::filesystem::path GetWorkshopLoaderConfigPath() const;
    std::filesystem::path ResolveConfiguredWorkshopRoot() const;


    // Shuffle bag persistence
    std::filesystem::path GetShuffleBagPath() const;
    void LoadShuffleBag();
    void SaveShuffleBag() const;

    // File/dir utilities
    void MirrorDirectory(const std::filesystem::path& src, const std::filesystem::path& dst) const;
    void EnsureReadmeFiles() const;

    // lifecycle
    void onLoad() override;
    void onUnload() override;

    // settings UI
    void RenderSettings() override;
    void SetImGuiContext(uintptr_t ctx) override;

    // Standalone Window UI (PluginWindowBase interface)
    void Render() override;
    void RenderWindow() override;

    // hooks
    void LoadHooks();
    void GameEndedEvent(std::string name);

    // Prejump scraper integration
    std::filesystem::path GetPrejumpPacksPath() const;
    void ScrapeAndLoadPrejumpPacks();
    void LoadPrejumpPacksFromFile(const std::filesystem::path& filePath);
    bool IsPrejumpCacheStale() const;
    std::string FormatLastUpdatedTime() const;
    
    // Prejump UI rendering
    void RenderPrejumpPacksTab();

    // Post-match overlay rendering
    void RenderPostMatchOverlay();

private:
    // state (one definition only)
    bool enabled = false;
    bool showWindow = false; // Internal toggle for the standalone control window

    bool autoQueue = false;   // (renamed from “Requeue”)
    int  mapType = 0;         // 0=Freeplay, 1=Training, 2=Workshop

    int  delayQueueSec    = 0;
    int  delayFreeplaySec = 0;
    int  delayTrainingSec = 0;
    int  delayWorkshopSec = 0;

    int  currentIndex = 0;           // freeplay
    int  currentTrainingIndex = 0;   // training
    int  currentWorkshopIndex = 0;   // workshop

    bool trainingShuffleEnabled = false;
    int  trainingBagSize = 1;
    std::vector<TrainingEntry> trainingShuffleBag;
    std::set<int> selectedTrainingIndices;

    std::string lastGameMode = "";

    // Prejump scraper state
    bool prejumpScrapingInProgress = false;
    std::string prejumpLastUpdated = "";
    int prejumpPackCount = 0;
    std::vector<TrainingEntry> prejumpPacks;  // Loaded prejump packs
    
    // Prejump UI state
    char prejumpSearchText[256] = {0};
    std::string prejumpDifficultyFilter = "All";
    std::string prejumpTagFilter = "";
    int prejumpMinShots = 0;
    int prejumpMaxShots = 100;
    int prejumpSortColumn = 0;  // 0=Name, 1=Creator, 2=Difficulty, 3=Shots, 4=Likes, 5=Plays
    bool prejumpSortAscending = true;

    // Shuffle helpers
    int GetRandomTrainingIndex() const;

    ImGuiContext* imguiCtx = nullptr;
    
    // Loadout management
    std::unique_ptr<LoadoutManager> loadoutManager;

    PostMatchInfo postMatch;
    float postMatchDurationSec = 15.0f;

    // Window positioning and size
    float overlayWidth = 880.0f;
    float overlayHeight = 400.0f;
    float overlayOffsetX = 0.0f;
    float overlayOffsetY = 0.0f;

    // Team section layout
    float teamHeaderHeight = 28.0f;
    float playerRowHeight = 24.0f;
    float teamSectionSpacing = 12.0f;
    float sectionPadding = 8.0f;

    // Column positions
    float nameColumnX = 50.0f;
    float scoreColumnX = 230.0f;
    float goalsColumnX = 290.0f;
    float assistsColumnX = 350.0f;
    float savesColumnX = 410.0f;
    float shotsColumnX = 470.0f;
    float pingColumnX = 530.0f;

    // Text and styling
    float mainFontSize = 14.0f;
    float headerFontSize = 12.0f;
    float teamHeaderFontSize = 16.0f;

    // Colors and transparency
    float overlayAlpha = 0.85f;
    float blueTeamHue = 240.0f;
    float blueTeamSat = 0.8f;
    float blueTeamVal = 0.6f;
    float orangeTeamHue = 25.0f;
    float orangeTeamSat = 0.9f;
    float orangeTeamVal = 0.7f;
    float backgroundAlpha = 0.4f;
    float headerAlpha = 0.8f;

    // MVP and special effects
    float mvpCheckmarkSize = 1.2f;
    bool showMvpGlow = true;
    bool showTeamScores = true;
    bool showColumnHeaders = true;

    // Animation and effects
    float fadeInDuration = 0.5f;
    float fadeOutDuration = 2.0f;
    bool enableFadeEffects = true;
};
