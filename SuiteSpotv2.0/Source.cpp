#include "pch.h"
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include "SuiteSpot.h"
#include "MapList.h"
#include <fstream>
#include <sstream>
#include <algorithm>

// Helper function for sortable column headers with visual indicators
namespace {
    bool SortableColumnHeader(const char* label, int columnIndex, int& currentSortColumn, bool& sortAscending) {
        // Display label with sort indicator if this column is active
        char buffer[256];
        if (currentSortColumn == columnIndex) {
            snprintf(buffer, sizeof(buffer), "%s %s", label, sortAscending ? "▲" : "▼");
        } else {
            snprintf(buffer, sizeof(buffer), "%s", label);
        }

        bool clicked = ImGui::Selectable(buffer, currentSortColumn == columnIndex, ImGuiSelectableFlags_DontClosePopups);
        if (clicked) {
            if (currentSortColumn == columnIndex) {
                sortAscending = !sortAscending;
            } else {
                currentSortColumn = columnIndex;
                sortAscending = true;
            }
        }
        return clicked;
    }
}

// #detailed comments: SetImGuiContext
// Purpose: Hook the plugin's ImGui context into the Settings/UI code.
// The pointer is stored in imguiCtx (retrieved via GetCurrentContext())
// so that non-UI threads or deferred render paths can re-establish
// the ImGui context before drawing. This is necessary when the plugin
// must render outside the main ImGui entry point.
//
// DO NOT CHANGE: Incorrect context handling leads to hard-to-diagnose
// rendering issues or asserts inside ImGui when a null/incorrect
// context is active.
void SuiteSpot::SetImGuiContext(uintptr_t ctx) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
    imguiCtx = ImGui::GetCurrentContext();
}

// #detailed comments: RenderSettings
// Purpose: Build the Settings UI using ImGui. This method is called on
// the UI thread and must complete quickly — avoid heavy computation
// or blocking calls here. Instead, schedule background work with
// gameWrapper->SetTimeout if needed.
//
// UI & state invariants:
//  - Any changes to cvars must keep the CVar<->member variable sync
//    consistent. The code uses cvarManager to persist values and also
//    updates plugin members; changing the order of writes may create
//    transient inconsistencies visible to users.
//  - Many UI controls guard against empty lists (e.g., RLTraining) and
//    clamp indices to valid ranges to prevent out-of-bounds access.
//
// DO NOT CHANGE: Rendering flow, label strings, and cvar names are
// relied upon by external automation and saved settings; altering them
// will change user-visible state persistence and CLI integrations.
void SuiteSpot::RenderSettings() {
    // Header with status
    ImGui::TextUnformatted("SuiteSpot - Auto Map Loader");
    
    // Only show status if enabled
    if (enabled) {
        ImGui::SameLine(420);
        
        const char* modeNames[] = {"Freeplay", "Training", "Workshop"};
        std::string currentMap = "<none>";
        std::string queueDelayStr = std::to_string(delayQueueSec) + "s";
        std::string mapDelayStr = "0s";
        std::string shotProgressStr = "";
        const ImVec4 white = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        
        // Get current selection and appropriate delay
        if (mapType == 0) {
            if (!RLMaps.empty() && currentIndex >= 0 && currentIndex < (int)RLMaps.size()) {
                currentMap = RLMaps[currentIndex].name;
            }
            mapDelayStr = std::to_string(delayFreeplaySec) + "s";
        } else if (mapType == 1) {
            if (!RLTraining.empty() && currentTrainingIndex >= 0 && currentTrainingIndex < (int)RLTraining.size()) {
                currentMap = RLTraining[currentTrainingIndex].name + " (Shots:" + std::to_string(RLTraining[currentTrainingIndex].shotCount) + ")";
            }
            mapDelayStr = std::to_string(delayTrainingSec) + "s";

            if (trainingShuffleEnabled) {
                int shuffleCount = 0;
                if (!trainingShuffleBag.empty()) {
                    shuffleCount = static_cast<int>(trainingShuffleBag.size());
                } else if (!selectedTrainingIndices.empty()) {
                    shuffleCount = static_cast<int>(selectedTrainingIndices.size());
                }
                // Only show shuffle indicator when we have a non-empty selection/bag.
                if (shuffleCount > 0) {
                    if (shuffleCount == 1) {
                        // Single pack in shuffle – show its name/shots.
                        const TrainingEntry* entry = nullptr;
                        if (!trainingShuffleBag.empty()) {
                            entry = &trainingShuffleBag.front();
                        } else if (!selectedTrainingIndices.empty()) {
                            int idx = *selectedTrainingIndices.begin();
                            if (idx >= 0 && idx < static_cast<int>(RLTraining.size())) {
                                entry = &RLTraining[idx];
                            }
                        }
                        if (entry) {
                            currentMap = entry->name + " (Shots:" + std::to_string(entry->shotCount) + ")";
                        }
                    } else {
                        currentMap = "Shuffle (" + std::to_string(shuffleCount) + " packs)";
                    }
                } // else fall back to current dropdown selection
            }
            
            // Real-time shot progress requires APIs not exposed by BakkesMod SDK
            // This will be added in Phase 2b when we have better access to training state
        } else if (mapType == 2) {
            if (!RLWorkshop.empty() && currentWorkshopIndex >= 0 && currentWorkshopIndex < (int)RLWorkshop.size()) {
                currentMap = RLWorkshop[currentWorkshopIndex].name;
            }
            mapDelayStr = std::to_string(delayWorkshopSec) + "s";
        }
        
        // Map mode status (always green when enabled)
        const ImVec4 green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 red   = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        std::string modeText = "Mode: " + std::string(modeNames[mapType]);
        if (delayFreeplaySec > 0 || delayTrainingSec > 0 || delayWorkshopSec > 0) {
            modeText += " Delayed: " + mapDelayStr;
        }
        ImGui::TextColored(green, "%s", modeText.c_str());
        ImGui::SameLine();
        ImGui::TextColored(white, "|");
        ImGui::SameLine();
        ImGui::TextColored(green, "Map: %s", currentMap.c_str());
        ImGui::SameLine();
        ImGui::TextColored(white, "|");
        ImGui::SameLine();
        const ImVec4 queueColor = autoQueue ? green : red;
        if (delayQueueSec > 0) {
            ImGui::TextColored(queueColor, "Next Match Queue Delayed: %s", queueDelayStr.c_str());
        } else {
            ImGui::TextColored(queueColor, "Next Match Queue");
        }
        ImGui::NewLine();
    }
    
    ImGui::Separator();
    
    // Main tab bar for organizing settings
    if (ImGui::BeginTabBar("SuiteSpotTabs", ImGuiTabBarFlags_None)) {
        
        // ===== MAIN SETTINGS TAB =====
        if (ImGui::BeginTabItem("Main Settings")) {
            
            // 1) Enable/Disable toggle on same line with Map Mode label
            CVarWrapper enableCvar = cvarManager->getCvar("suitespot_enabled");
            if (!enableCvar) {
                ImGui::EndTabItem();
                ImGui::EndTabBar();
                return;
            }
            enabled = enableCvar.getBoolValue();
            if (ImGui::Checkbox("Enable SuiteSpot", &enabled)) {
                enableCvar.setValue(enabled);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable/disable all SuiteSpot auto-loading and queuing features");
            }

            ImGui::SameLine();
            if (ImGui::Button("Open Standalone Window")) {
                cvarManager->executeCommand("suitespot_toggle_window");
            }

            ImGui::SameLine(450);
            ImGui::TextUnformatted("Map Mode:");
            ImGui::SameLine(400);
            
            // 2) Select Map Type (radio buttons on same line)
            const char* mapLabels[] = {"Freeplay", "Training", "Workshop"};
            for (int i = 0; i < 3; i++) {
                if (i > 0) ImGui::SameLine(0, 20);
                if (ImGui::RadioButton(mapLabels[i], mapType == i)) {
                    mapType = i;
                    cvarManager->getCvar("suitespot_map_type").setValue(mapType);
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Choose which map type loads after matches:\nFreeplay = Official | Training = Custom Packs | Workshop = Modded Maps");
            }

            ImGui::Separator();

    // 3) Auto-Queue section (under Enable/Map Mode)
    if (ImGui::Checkbox("Auto-Queue Next Match", &autoQueue)) {
        cvarManager->getCvar("suitespot_auto_queue").setValue(autoQueue);
    }
    if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Automatically queue into the next match after the current match ends.\nQueue delay starts at match end, independent of map load.");
}

ImGui::SetNextItemWidth(220);
if (ImGui::InputInt("Delay Queue (sec)", &delayQueueSec)) {
        delayQueueSec = std::max(0, delayQueueSec);
        cvarManager->getCvar("suitespot_delay_queue_sec").setValue(delayQueueSec);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Wait this many seconds before queuing (independent of map load time)");
    }

    ImGui::Separator();

    // 4) Map Selection & Type-Specific Settings
    ImGui::TextUnformatted("Map Selection:");
    ImGui::Spacing();
    
    if (mapType == 0) {
        // Freeplay maps
        if (!RLMaps.empty()) {
            currentIndex = std::clamp(currentIndex, 0, static_cast<int>(RLMaps.size() - 1));
        } else {
            currentIndex = 0;
        }
        
        const char* freeplayLabel = RLMaps.empty() ? "<none>" : RLMaps[currentIndex].name.c_str();
        if (ImGui::BeginCombo("Freeplay Maps", freeplayLabel)) {
            for (int i = 0; i < (int)RLMaps.size(); ++i) {
                bool selected = (i == currentIndex);
                if (ImGui::Selectable(RLMaps[i].name.c_str(), selected)) {
                    currentIndex = i;
                    cvarManager->getCvar("suitespot_current_freeplay_index").setValue(currentIndex);
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select which stadium to load after matches");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Load Now##freeplay")) {
            if (!RLMaps.empty() && currentIndex >= 0 && currentIndex < (int)RLMaps.size()) {
                std::string mapCode = RLMaps[currentIndex].code;
                gameWrapper->SetTimeout([this, mapCode](GameWrapper* gw) {
                    cvarManager->executeCommand("load_freeplay " + mapCode);
                }, 0.0f);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load the selected freeplay map immediately");
        }
        
        ImGui::Spacing();
        ImGui::TextUnformatted("Freeplay Settings:");
        ImGui::SetNextItemWidth(220);
        if (ImGui::InputInt("Delay Freeplay (sec)", &delayFreeplaySec)) { 
            delayFreeplaySec = std::max(0, delayFreeplaySec); 
            cvarManager->getCvar("suitespot_delay_freeplay_sec").setValue(delayFreeplaySec);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Wait this many seconds after match ends before loading Freeplay");
        }
    }
    else if (mapType == 1) {
        // Training maps
        if (!RLTraining.empty()) {
            currentTrainingIndex = std::clamp(currentTrainingIndex, 0, static_cast<int>(RLTraining.size() - 1));
        } else {
            currentTrainingIndex = 0;
        }

        const char* trainingLabel = "<none>";
        static std::string trainingLabelBuf;
        if (!RLTraining.empty() && currentTrainingIndex >= 0 && currentTrainingIndex < (int)RLTraining.size()) {
            trainingLabelBuf = RLTraining[currentTrainingIndex].name + " (Shots:" + std::to_string(RLTraining[currentTrainingIndex].shotCount) + ")";
            trainingLabel = trainingLabelBuf.c_str();
        }

        if (ImGui::BeginCombo("Training Packs", trainingLabel)) {
            for (int i = 0; i < (int)RLTraining.size(); ++i) {
                bool selected = (i == currentTrainingIndex);
                std::string itemLabel = RLTraining[i].name + " (Shots:" + std::to_string(RLTraining[i].shotCount) + ")";
                if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                    currentTrainingIndex = i;
                    cvarManager->getCvar("suitespot_current_training_index").setValue(currentTrainingIndex);
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select which training pack to load after matches");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Refresh Training##maps")) {
            std::string previousCode;
            if (currentTrainingIndex >= 0 && currentTrainingIndex < (int)RLTraining.size()) {
                previousCode = RLTraining[currentTrainingIndex].code;
            }
            LoadTrainingMaps();
            if (!previousCode.empty()) {
                auto it = std::find_if(RLTraining.begin(), RLTraining.end(),
                    [&](const TrainingEntry& entry){ return entry.code == previousCode; });
                if (it != RLTraining.end()) {
                    currentTrainingIndex = static_cast<int>(std::distance(RLTraining.begin(), it));
                    cvarManager->getCvar("suitespot_current_training_index").setValue(currentTrainingIndex);
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Refresh the training pack list from storage");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Load Now##training")) {
            if (!RLTraining.empty()) {
                int indexToLoad = currentTrainingIndex;

                if (trainingShuffleEnabled) {
                    // Build bag from current selection; if none selected and bag empty, fall back to dropdown.
                    if (trainingShuffleBag.empty()) {
                        trainingShuffleBag.clear();
                        if (!selectedTrainingIndices.empty()) {
                            for (int idx : selectedTrainingIndices) {
                                if (idx >= 0 && idx < static_cast<int>(RLTraining.size())) {
                                    trainingShuffleBag.push_back(RLTraining[idx]);
                                }
                            }
                        }
                    }

                    if (!trainingShuffleBag.empty()) {
                        indexToLoad = GetRandomTrainingIndex();
                    } // else fall back to the currently selected pack
                }

                indexToLoad = std::clamp(indexToLoad, 0, (int)RLTraining.size() - 1);
                std::string packCode = RLTraining[indexToLoad].code;
                gameWrapper->SetTimeout([this, packCode](GameWrapper* gw) {
                    cvarManager->executeCommand("load_training " + packCode);
                }, 0.0f);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load the selected training pack immediately");
        }

        ImGui::SameLine();
        static bool showAddTrainingForm = false;
        if (ImGui::Button("Add Pack##training_toggle")) {
            showAddTrainingForm = !showAddTrainingForm;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show or hide the custom training pack form");
        }
        
        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextUnformatted("Training Settings:");
        ImGui::SetNextItemWidth(220);
        if (ImGui::InputInt("Delay Training (sec)", &delayTrainingSec)) {
            delayTrainingSec = std::max(0, delayTrainingSec);
            cvarManager->getCvar("suitespot_delay_training_sec").setValue(delayTrainingSec);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Wait this many seconds after match ends before loading Training");
        }
        
        if (showAddTrainingForm) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("Add Custom Training Pack:");
            ImGui::Spacing();
            
            static char newMapCode[64] = {0};
            static char newMapName[64] = {0};
            static bool addSuccess = false;
            static float addSuccessTimer = 0.0f;
            
            ImGui::InputText("Training Map Code##input", newMapCode, IM_ARRAYSIZE(newMapCode), 0);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enter the code (e.g., 555F-7503-BBB9-E1E3)");
            }
            
            ImGui::InputText("Training Map Name##input", newMapName, IM_ARRAYSIZE(newMapName), 0);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enter a custom name for this pack");
            }
            
            if (ImGui::Button("Add Training Map")) {
                if (strlen(newMapCode) > 0 && strlen(newMapName) > 0) {
                    const std::string codeStr(newMapCode);
                    const std::string nameStr(newMapName);
                    RLTraining.push_back({ codeStr, nameStr });
                    SaveTrainingMaps();
                    LoadTrainingMaps();
                    auto it = std::find_if(RLTraining.begin(), RLTraining.end(),
                        [&](const TrainingEntry& entry){ return entry.code == codeStr; });
                    if (it != RLTraining.end()) {
                        currentTrainingIndex = static_cast<int>(std::distance(RLTraining.begin(), it));
                        cvarManager->getCvar("suitespot_current_training_index").setValue(currentTrainingIndex);
                    }
                    addSuccess = true;
                    addSuccessTimer = 3.0f;
                    newMapCode[0] = 0;
                    newMapName[0] = 0;
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Add this training pack to your collection");
            }
            
            if (addSuccess && addSuccessTimer > 0.0f) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, addSuccessTimer / 3.0f), "Pack added!");
                addSuccessTimer -= ImGui::GetIO().DeltaTime;
                if (addSuccessTimer <= 0.0f) {
                    addSuccess = false;
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Training Options:");
        ImGui::Spacing();
        
        CVarWrapper shuffleCvar = cvarManager->getCvar("suitespot_training_shuffle");
        if (shuffleCvar) {
            trainingShuffleEnabled = shuffleCvar.getBoolValue();
            if (ImGui::Checkbox("Shuffle Training Packs", &trainingShuffleEnabled)) {
                shuffleCvar.setValue(trainingShuffleEnabled);
                trainingShuffleBag.clear();
                if (trainingShuffleEnabled) {
                    if (selectedTrainingIndices.empty()) {
                        // If nothing is selected, default to all packs.
                        for (int i = 0; i < static_cast<int>(RLTraining.size()); ++i) {
                            selectedTrainingIndices.insert(i);
                            trainingShuffleBag.push_back(RLTraining[i]);
                        }
                    } else {
                        for (int idx : selectedTrainingIndices) {
                            if (idx >= 0 && idx < static_cast<int>(RLTraining.size())) {
                                trainingShuffleBag.push_back(RLTraining[idx]);
                            }
                        }
                    }
                }
                trainingBagSize = static_cast<int>(trainingShuffleBag.size());
                cvarManager->getCvar("suitespot_training_bag_size").setValue(trainingBagSize);
                SaveShuffleBag();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Shuffle rotates through the selected training packs");
            }

            ImGui::SameLine();
            if (ImGui::Button("Toggle All##training_shuffle")) {
                if (selectedTrainingIndices.size() == RLTraining.size()) {
                    selectedTrainingIndices.clear();
                    trainingShuffleBag.clear();
                } else {
                    selectedTrainingIndices.clear();
                    trainingShuffleBag.clear();
                    for (int i = 0; i < static_cast<int>(RLTraining.size()); ++i) {
                        selectedTrainingIndices.insert(i);
                        trainingShuffleBag.push_back(RLTraining[i]);
                    }
                }
                trainingBagSize = static_cast<int>(trainingShuffleBag.size());
                cvarManager->getCvar("suitespot_training_bag_size").setValue(trainingBagSize);
                SaveShuffleBag();
            }
        }

        if (trainingShuffleEnabled) {
            ImGui::Spacing();
            int selectedCount = static_cast<int>(selectedTrainingIndices.size());
            ImGui::TextUnformatted(("Selected for shuffle: " + std::to_string(selectedCount)).c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Shuffle will rotate through the checked training packs");
            }

            if (ImGui::CollapsingHeader("Shuffle Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextDisabled("Check packs to include in shuffle");
                for (int i = 0; i < (int)RLTraining.size(); ++i) {
                    bool inBag = selectedTrainingIndices.count(i) > 0;
                    std::string label = RLTraining[i].name + " (Shots:" + std::to_string(RLTraining[i].shotCount) + ")";
                    if (ImGui::Checkbox(label.c_str(), &inBag)) {
                        if (inBag) {
                            selectedTrainingIndices.insert(i);
                            trainingShuffleBag.push_back(RLTraining[i]);
                        } else {
                            selectedTrainingIndices.erase(i);
                            auto it = std::find_if(trainingShuffleBag.begin(), trainingShuffleBag.end(),
                                [&](const TrainingEntry& e) { return e.code == RLTraining[i].code; });
                            if (it != trainingShuffleBag.end()) {
                                trainingShuffleBag.erase(it);
                            }
                        }
                        trainingBagSize = static_cast<int>(trainingShuffleBag.size());
                        cvarManager->getCvar("suitespot_training_bag_size").setValue(trainingBagSize);
                        SaveShuffleBag();
                    }
                }
            }
        }
    }
    else if (mapType == 2) {
        // Workshop maps
        if (!RLWorkshop.empty()) {
            currentWorkshopIndex = std::clamp(currentWorkshopIndex, 0, static_cast<int>(RLWorkshop.size() - 1));
        } else {
            currentWorkshopIndex = 0;
        }

        const char* workshopLabel = RLWorkshop.empty() ? "<none>" : RLWorkshop[currentWorkshopIndex].name.c_str();
        if (ImGui::BeginCombo("Workshop Maps", workshopLabel)) {
            for (int i = 0; i < (int)RLWorkshop.size(); ++i) {
                bool selected = (i == currentWorkshopIndex);
                if (ImGui::Selectable(RLWorkshop[i].name.c_str(), selected)) {
                    currentWorkshopIndex = i;
                    cvarManager->getCvar("suitespot_current_workshop_index").setValue(currentWorkshopIndex);
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select which workshop map to load after matches");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Refresh Workshop##maps")) {
            std::string previousPath;
            if (currentWorkshopIndex >= 0 && currentWorkshopIndex < (int)RLWorkshop.size()) {
                previousPath = RLWorkshop[currentWorkshopIndex].filePath;
            }
            LoadWorkshopMaps();
            if (!previousPath.empty()) {
                auto it = std::find_if(RLWorkshop.begin(), RLWorkshop.end(),
                    [&](const WorkshopEntry& entry){ return entry.filePath == previousPath; });
                if (it != RLWorkshop.end()) {
                    currentWorkshopIndex = static_cast<int>(std::distance(RLWorkshop.begin(), it));
                    cvarManager->getCvar("suitespot_current_workshop_index").setValue(currentWorkshopIndex);
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Refresh the workshop map list");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Load Now##workshop")) {
            if (!RLWorkshop.empty() && currentWorkshopIndex >= 0 && currentWorkshopIndex < (int)RLWorkshop.size()) {
                std::string filePath = RLWorkshop[currentWorkshopIndex].filePath;
                gameWrapper->SetTimeout([this, filePath](GameWrapper* gw) {
                    cvarManager->executeCommand("load_workshop \"" + filePath + "\"");
                }, 0.0f);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load the selected workshop map immediately");
        }
        
        ImGui::Spacing();
        if (ImGui::TreeNodeEx("Workshop Source", ImGuiTreeNodeFlags_DefaultOpen)) {
            static bool workshopPathInit = false;
            static std::string workshopPathCache;
            static char workshopPathBuf[512] = {0};

            if (!workshopPathInit) {
                auto resolved = ResolveConfiguredWorkshopRoot();
                if (!resolved.empty()) {
                    workshopPathCache = resolved.string();
                } else {
                    workshopPathCache = R"(C:\Program Files (x86)\Steam\steamapps\common\rocketleague\TAGame\CookedPCConsole\mods)";
                }
                strncpy_s(workshopPathBuf, workshopPathCache.c_str(), sizeof(workshopPathBuf) - 1);
                workshopPathInit = true;
            }

            ImGui::TextWrapped("Workshop maps root folder:");
            ImGui::SetNextItemWidth(420);
            ImGui::InputText("##workshop_root", workshopPathBuf, IM_ARRAYSIZE(workshopPathBuf));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Set the root folder to scan for workshop maps (contains subfolders with .upk files).");
            }

            if (ImGui::Button("Save Workshop Source")) {
                std::filesystem::path cfgPath = GetWorkshopLoaderConfigPath();
                std::error_code ec;
                std::filesystem::create_directories(cfgPath.parent_path(), ec);
                std::ofstream cfg(cfgPath.string(), std::ios::trunc);
                if (cfg.is_open()) {
                    cfg << "MapsFolderPath=" << workshopPathBuf << "\n";
                    cfg.close();
                    workshopPathCache = workshopPathBuf;
                    LoadWorkshopMaps();
                    // Keep current selection in range
                    if (!RLWorkshop.empty()) {
                        currentWorkshopIndex = std::clamp(currentWorkshopIndex, 0, static_cast<int>(RLWorkshop.size() - 1));
                    } else {
                        currentWorkshopIndex = 0;
                    }
                } else {
                    LOG("SuiteSpot: Failed to write workshopmaploader.cfg");
                }
            }

            ImGui::TreePop();
        }
        
        ImGui::Spacing();
        ImGui::TextUnformatted("Workshop Settings:");
        ImGui::SetNextItemWidth(220);
        if (ImGui::InputInt("Delay Workshop (sec)", &delayWorkshopSec)) { 
            delayWorkshopSec = std::max(0, delayWorkshopSec); 
            cvarManager->getCvar("suitespot_delay_workshop_sec").setValue(delayWorkshopSec);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Wait this many seconds after match ends before loading Workshop");
        }
    }

            // Close Main Settings tab
            ImGui::EndTabItem();
        } // End Main Settings tab

        // ===== LOADOUT MANAGEMENT TAB =====
        if (ImGui::BeginTabItem("Loadout Management")) {
            ImGui::Spacing();
            
            if (loadoutManager) {
                // Get current loadout name
                static std::string currentLoadoutName;
                static std::vector<std::string> loadoutNames;
                static int selectedLoadoutIndex = 0;
                static bool loadoutsInitialized = false;
                
                // Initialize loadout list on first render
                if (!loadoutsInitialized) {
                    loadoutNames = loadoutManager->GetLoadoutNames();
                    currentLoadoutName = loadoutManager->GetCurrentLoadoutName();
                    loadoutsInitialized = true;
                }
                
                // Display current active loadout
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Current Loadout:");
                ImGui::SameLine();
                if (currentLoadoutName.empty()) {
                    ImGui::TextUnformatted("<Unknown>");
                } else {
                    ImGui::TextUnformatted(currentLoadoutName.c_str());
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Your currently equipped loadout preset");
                }
                
                ImGui::Spacing();
                
                // Loadout selection dropdown
                const char* comboLabel = loadoutNames.empty() ? "<No loadouts found>" : 
                    (selectedLoadoutIndex >= 0 && selectedLoadoutIndex < (int)loadoutNames.size() ? 
                        loadoutNames[selectedLoadoutIndex].c_str() : "<Select loadout>");
                
                ImGui::SetNextItemWidth(220);
                if (ImGui::BeginCombo("##loadout_combo", comboLabel)) {
                    for (int i = 0; i < (int)loadoutNames.size(); ++i) {
                        bool isSelected = (i == selectedLoadoutIndex);
                        if (ImGui::Selectable(loadoutNames[i].c_str(), isSelected)) {
                            selectedLoadoutIndex = i;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Select a loadout preset to equip");
                }
                
                // Apply loadout button
                ImGui::SameLine();
                if (ImGui::Button("Apply Loadout")) {
                    if (selectedLoadoutIndex >= 0 && selectedLoadoutIndex < (int)loadoutNames.size()) {
                        std::string selectedName = loadoutNames[selectedLoadoutIndex];
                        gameWrapper->SetTimeout([this, selectedName](GameWrapper* gw) {
                            if (loadoutManager) {
                                bool success = loadoutManager->SwitchLoadout(selectedName);
                                if (success) {
                                    LOG("SuiteSpot: Successfully switched to loadout: " + selectedName);
                                } else {
                                    LOG("SuiteSpot: Failed to switch to loadout: " + selectedName);
                                }
                            }
                        }, 0.0f);
                        
                        // Update current loadout display
                        currentLoadoutName = selectedName;
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Equip the selected loadout preset");
                }
                
                // Refresh loadouts button
                ImGui::SameLine();
                if (ImGui::Button("Refresh Loadouts")) {
                    // Reset initialization flag to force refresh on next render
                    loadoutsInitialized = false;
                    
                    gameWrapper->SetTimeout([this](GameWrapper* gw) {
                        if (loadoutManager) {
                            loadoutManager->RefreshLoadoutCache();
                            LOG("SuiteSpot: Loadout list refreshed, found " + 
                                std::to_string(loadoutManager->GetLoadoutNames().size()) + " loadout(s)");
                        }
                    }, 0.0f);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Refresh the list of available loadout presets");
                }
                
                // Display loadout count
                ImGui::Spacing();
                ImGui::TextDisabled(("Available loadouts: " + std::to_string(loadoutNames.size())).c_str());
                if (ImGui::IsItemHovered() && loadoutNames.empty()) {
                    ImGui::SetTooltip("No loadout presets found. Create loadouts in-game in the Garage menu.");
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "LoadoutManager not initialized");
            }

            ImGui::EndTabItem();
        } // End Loadout Management tab

        // ===== OVERLAY LAYOUT TAB =====
        if (ImGui::BeginTabItem("Overlay Layout")) {
            ImGui::Spacing();

            // Reset button
            if (ImGui::Button("Reset to Defaults")) {
                // Reset all values to defaults
                overlayWidth = 880.0f; overlayHeight = 400.0f;
                teamHeaderHeight = 28.0f; playerRowHeight = 24.0f; teamSectionSpacing = 12.0f;
                nameColumnX = 50.0f; scoreColumnX = 230.0f; goalsColumnX = 290.0f;
                assistsColumnX = 350.0f; savesColumnX = 410.0f; shotsColumnX = 470.0f; pingColumnX = 530.0f;
                blueTeamHue = 240.0f; orangeTeamHue = 25.0f; overlayAlpha = 0.85f;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Reset all overlay settings to default values");
            }
            ImGui::Separator();

            // === WINDOW LAYOUT ===
            if (ImGui::CollapsingHeader("Window Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Display Time (sec)", &postMatchDurationSec, 5.0f, 60.0f, "%.1f");
                
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Width##overlay", &overlayWidth, 400.0f, 1600.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Height##overlay", &overlayHeight, 200.0f, 800.0f, "%.0f");

                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Offset X##overlay", &overlayOffsetX, -1000.0f, 1000.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Offset Y##overlay", &overlayOffsetY, -500.0f, 500.0f, "%.0f");
            }

            // === TEAM SECTIONS ===
            if (ImGui::CollapsingHeader("Team Sections")) {
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Header Height", &teamHeaderHeight, 20.0f, 50.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Player Row Height", &playerRowHeight, 16.0f, 40.0f, "%.0f");
                
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Section Spacing", &teamSectionSpacing, 0.0f, 50.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180);
                ImGui::SliderFloat("Section Padding", &sectionPadding, 0.0f, 20.0f, "%.0f");
            }

            // === COLUMN POSITIONS ===
            if (ImGui::CollapsingHeader("Column Positions")) {
                ImGui::Text("Adjust horizontal position of each stat column:");
                
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Name##col", &nameColumnX, 20.0f, 200.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Score##col", &scoreColumnX, 150.0f, 400.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Goals##col", &goalsColumnX, 200.0f, 450.0f, "%.0f");
                
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Assists##col", &assistsColumnX, 250.0f, 500.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Saves##col", &savesColumnX, 300.0f, 550.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Shots##col", &shotsColumnX, 350.0f, 600.0f, "%.0f");
                
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Ping##col", &pingColumnX, 400.0f, 700.0f, "%.0f");
            }

            // === COLORS & STYLING ===
            if (ImGui::CollapsingHeader("Colors & Styling")) {
                ImGui::Text("Blue Team Colors:");
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Blue Hue", &blueTeamHue, 0.0f, 360.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Blue Saturation", &blueTeamSat, 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Blue Brightness", &blueTeamVal, 0.0f, 1.0f, "%.2f");
                
                ImGui::Text("Orange Team Colors:");
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Orange Hue", &orangeTeamHue, 0.0f, 360.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Orange Saturation", &orangeTeamSat, 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Orange Brightness", &orangeTeamVal, 0.0f, 1.0f, "%.2f");
                
                ImGui::Text("Transparency:");
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Overall Alpha", &overlayAlpha, 0.1f, 1.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Background Alpha", &backgroundAlpha, 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Header Alpha", &headerAlpha, 0.0f, 1.0f, "%.2f");
            }

            // === CONTENT OPTIONS ===
            if (ImGui::CollapsingHeader("Content Options")) {
                ImGui::Checkbox("Show Team Scores in Headers", &showTeamScores);
                ImGui::SameLine();
                ImGui::Checkbox("Show Column Headers", &showColumnHeaders);
                
                ImGui::Checkbox("Show MVP Glow Effect", &showMvpGlow);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("MVP Checkmark Size", &mvpCheckmarkSize, 0.5f, 3.0f, "%.1f");
                
                ImGui::Checkbox("Enable Fade Effects", &enableFadeEffects);
                if (enableFadeEffects) {
                    ImGui::SetNextItemWidth(140);
                    ImGui::SliderFloat("Fade In Time", &fadeInDuration, 0.1f, 2.0f, "%.1f");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(140);
                    ImGui::SliderFloat("Fade Out Time", &fadeOutDuration, 0.5f, 5.0f, "%.1f");
                }
            }

            // === FONT SIZES ===
            if (ImGui::CollapsingHeader("Font Sizes")) {
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Main Text Size", &mainFontSize, 8.0f, 24.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Header Text Size", &headerFontSize, 8.0f, 20.0f, "%.0f");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(140);
                ImGui::SliderFloat("Team Header Size", &teamHeaderFontSize, 10.0f, 28.0f, "%.0f");
            }

            // Real-time preview info
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "💡 Changes apply in real-time to the test overlay");
            ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "✨ All settings are automatically saved");

            ImGui::EndTabItem();
        } // End Overlay Layout tab

        // ===== PREJUMP PACKS TAB =====
        if (ImGui::BeginTabItem("Prejump Packs")) {
            RenderPrejumpPacksTab();
            ImGui::EndTabItem();
        } // End Prejump Packs tab

        // Close the tab bar
        ImGui::EndTabBar();
    } // End tab bar
}

// #detailed comments: RenderPrejumpPacksTab
// Purpose: Render the Prejump Packs browser UI with filtering, sorting, and shuffle integration
// This tab provides access to the 2,000+ training packs scraped from prejump.com
// with direct loading and shuffle bag management capabilities.
void SuiteSpot::RenderPrejumpPacksTab() {
    ImGui::Spacing();

    // ===== HEADER SECTION =====
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Prejump.com Training Pack Browser");
    ImGui::Spacing();

    // Status line: pack count and last updated
    if (prejumpPackCount > 0) {
        ImGui::Text("Loaded: %d packs", prejumpPackCount);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " | Last updated: %s", prejumpLastUpdated.c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No packs loaded - click 'Scrape Prejump' to download");
    }

    // Control buttons
    ImGui::SameLine();
    float buttonX = ImGui::GetWindowWidth() - 280;
    if (buttonX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(buttonX);
    }

    if (prejumpScrapingInProgress) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Scraping...");
    } else {
        if (ImGui::Button("Scrape Prejump")) {
            ScrapeAndLoadPrejumpPacks();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Download latest training packs from prejump.com (~2-3 minutes)");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload Cache")) {
        LoadPrejumpPacksFromFile(GetPrejumpPacksPath());
        prejumpLastUpdated = FormatLastUpdatedTime();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reload packs from cached prejump_packs.json file");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Early return if no packs loaded
    if (prejumpPacks.empty()) {
        ImGui::TextWrapped("No packs available. Click 'Scrape Prejump' to download the training pack database from prejump.com.");
        return;
    }

    // ===== SHUFFLE BAG STATUS =====
    int shufflePackCount = static_cast<int>(trainingShuffleBag.size());
    if (shufflePackCount > 0) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Shuffle Bag: %d pack%s", shufflePackCount, shufflePackCount == 1 ? "" : "s");
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Shuffle")) {
            trainingShuffleBag.clear();
            selectedTrainingIndices.clear();
            SaveShuffleBag();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Clear all packs from shuffle bag");
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Shuffle Bag: Empty (click 'Add to Shuffle' to build your rotation)");
    }
    ImGui::Spacing();

    // ===== FILTER & SEARCH CONTROLS =====
    ImGui::TextUnformatted("Search & Filters:");
    ImGui::Spacing();

    // Track if filters changed to rebuild filtered list
    static char lastSearchText[256] = {0};
    static std::string lastDifficultyFilter = "All";
    static std::string lastTagFilter = "";
    static int lastMinShots = 0;
    static int lastSortColumn = 0;
    static bool lastSortAscending = true;

    bool filtersChanged = (strcmp(prejumpSearchText, lastSearchText) != 0) ||
                          (prejumpDifficultyFilter != lastDifficultyFilter) ||
                          (prejumpTagFilter != lastTagFilter) ||
                          (prejumpMinShots != lastMinShots) ||
                          (prejumpSortColumn != lastSortColumn) ||
                          (prejumpSortAscending != lastSortAscending);

    // Search box
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##search", prejumpSearchText, IM_ARRAYSIZE(prejumpSearchText))) {
        filtersChanged = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Search by pack name, creator, or tag");
    }

    // Difficulty filter
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    const char* difficulties[] = {"All", "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Champion", "Grand Champion", "Supersonic Legend"};
    if (ImGui::BeginCombo("##difficulty", prejumpDifficultyFilter.c_str())) {
        for (int i = 0; i < IM_ARRAYSIZE(difficulties); i++) {
            bool selected = (prejumpDifficultyFilter == difficulties[i]);
            if (ImGui::Selectable(difficulties[i], selected)) {
                prejumpDifficultyFilter = difficulties[i];
                filtersChanged = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Filter by difficulty level");
    }

    // Shot count range filter
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderInt("Min Shots", &prejumpMinShots, 0, 50)) {
        filtersChanged = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Minimum number of shots in pack");
    }

    // Tag filter dropdown (second row)
    ImGui::SetNextItemWidth(200);

    // Build unique tag list on first render or when packs change
    static std::vector<std::string> availableTags;
    static bool tagsInitialized = false;
    static int lastPackCount = 0;

    if (!tagsInitialized || lastPackCount != prejumpPackCount) {
        std::set<std::string> uniqueTags;
        for (const auto& pack : prejumpPacks) {
            for (const auto& tag : pack.tags) {
                uniqueTags.insert(tag);
            }
        }
        availableTags.clear();
        availableTags.push_back("All Tags");
        for (const auto& tag : uniqueTags) {
            availableTags.push_back(tag);
        }
        tagsInitialized = true;
        lastPackCount = prejumpPackCount;
    }

    std::string displayTag = prejumpTagFilter.empty() ? "All Tags" : prejumpTagFilter;
    if (ImGui::BeginCombo("##tagfilter", displayTag.c_str())) {
        for (const auto& tag : availableTags) {
            bool selected = (tag == displayTag);
            if (ImGui::Selectable(tag.c_str(), selected)) {
                prejumpTagFilter = (tag == "All Tags") ? "" : tag;
                filtersChanged = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Filter by tag");
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Filters")) {
        prejumpSearchText[0] = '\0';
        prejumpDifficultyFilter = "All";
        prejumpTagFilter = "";
        prejumpMinShots = 0;
        prejumpMaxShots = 100;
        filtersChanged = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ===== FILTERED & SORTED PACK LIST (cached) =====

    // Static cache for filtered/sorted results
    static std::vector<TrainingEntry> filteredPacks;

    // Rebuild filtered list only when needed
    if (filtersChanged) {
        filteredPacks.clear();

        // Convert search text to lowercase once
        std::string searchLower(prejumpSearchText);
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        for (const auto& pack : prejumpPacks) {
            // Skip if doesn't match search
            if (searchLower.length() > 0) {
                std::string nameLower = pack.name;
                std::string creatorLower = pack.creator;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                std::transform(creatorLower.begin(), creatorLower.end(), creatorLower.begin(), ::tolower);

                bool matchesSearch = false;
                if (nameLower.find(searchLower) != std::string::npos) matchesSearch = true;
                if (creatorLower.find(searchLower) != std::string::npos) matchesSearch = true;

                // Check tags
                for (const auto& tag : pack.tags) {
                    std::string tagLower = tag;
                    std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
                    if (tagLower.find(searchLower) != std::string::npos) {
                        matchesSearch = true;
                        break;
                    }
                }

                if (!matchesSearch) continue;
            }

            // Skip if doesn't match difficulty filter
            if (prejumpDifficultyFilter != "All" && pack.difficulty != prejumpDifficultyFilter) {
                continue;
            }

            // Skip if doesn't match tag filter
            if (!prejumpTagFilter.empty()) {
                bool hasTag = false;
                for (const auto& tag : pack.tags) {
                    if (tag == prejumpTagFilter) {
                        hasTag = true;
                        break;
                    }
                }
                if (!hasTag) continue;
            }

            // Skip if doesn't match shot count filter
            if (pack.shotCount < prejumpMinShots) {
                continue;
            }

            filteredPacks.push_back(pack);
        }

        // Sort the filtered packs
        std::sort(filteredPacks.begin(), filteredPacks.end(), [this](const TrainingEntry& a, const TrainingEntry& b) {
            int cmp = 0;
            switch (prejumpSortColumn) {
                case 0: // Name
                    cmp = a.name.compare(b.name);
                    break;
                case 1: // Creator
                    cmp = a.creator.compare(b.creator);
                    break;
                case 2: // Difficulty
                    cmp = a.difficulty.compare(b.difficulty);
                    break;
                case 3: // Shots
                    cmp = (a.shotCount < b.shotCount) ? -1 : (a.shotCount > b.shotCount) ? 1 : 0;
                    break;
                case 4: // Likes
                    cmp = (a.likes < b.likes) ? -1 : (a.likes > b.likes) ? 1 : 0;
                    break;
                case 5: // Plays
                    cmp = (a.plays < b.plays) ? -1 : (a.plays > b.plays) ? 1 : 0;
                    break;
            }
            return prejumpSortAscending ? (cmp < 0) : (cmp > 0);
        });

        // Update cached filter state
        strncpy_s(lastSearchText, prejumpSearchText, sizeof(lastSearchText) - 1);
        lastDifficultyFilter = prejumpDifficultyFilter;
        lastTagFilter = prejumpTagFilter;
        lastMinShots = prejumpMinShots;
        lastSortColumn = prejumpSortColumn;
        lastSortAscending = prejumpSortAscending;
    }

    // Display filtered count
    ImGui::Text("Showing %d of %d packs", (int)filteredPacks.size(), prejumpPackCount);
    ImGui::Spacing();

    // ===== TABLE WITH RESIZABLE COLUMNS (ImGui 1.75 Columns API) =====
    // No BeginChild needed - we're already inside the settings tab

    ImGui::Separator();

    // Setup columns (ImGui 1.75 compatible)
    ImGui::Columns(8, "PrejumpColumns", true);  // true = resizable borders

    // Don't set initial column widths - let ImGui auto-distribute across full tab width
    // Users can resize individual columns as needed

    // ===== HEADER ROW WITH SORT INDICATORS =====
    ImGui::Separator();

    // Name column header
    if (SortableColumnHeader("Name", 0, prejumpSortColumn, prejumpSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Creator column header
    if (SortableColumnHeader("Creator", 1, prejumpSortColumn, prejumpSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Difficulty column header
    if (SortableColumnHeader("Difficulty", 2, prejumpSortColumn, prejumpSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Shots column header
    if (SortableColumnHeader("Shots", 3, prejumpSortColumn, prejumpSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Tags column header (non-sortable)
    ImGui::TextUnformatted("Tags");
    ImGui::NextColumn();

    // Likes column header
    if (SortableColumnHeader("Likes", 5, prejumpSortColumn, prejumpSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Plays column header
    if (SortableColumnHeader("Plays", 6, prejumpSortColumn, prejumpSortAscending)) {
        filtersChanged = true;
    }
    ImGui::NextColumn();

    // Actions column header (non-sortable)
    ImGui::TextUnformatted("Actions");
    ImGui::NextColumn();

    ImGui::Separator();

    // ===== RENDER PACK ROWS =====
    for (size_t row = 0; row < filteredPacks.size(); row++) {
        const auto& pack = filteredPacks[row];

        // Name column
        ImGui::TextUnformatted(pack.name.c_str());
        if (ImGui::IsItemHovered() && !pack.staffComments.empty()) {
            ImGui::SetTooltip("%s", pack.staffComments.c_str());
        }
        ImGui::NextColumn();

        // Creator column
        ImGui::TextUnformatted(pack.creator.c_str());
        ImGui::NextColumn();

        // Difficulty column with color coding
        ImVec4 diffColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        if (pack.difficulty == "Bronze") diffColor = ImVec4(0.8f, 0.5f, 0.2f, 1.0f);
        else if (pack.difficulty == "Silver") diffColor = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
        else if (pack.difficulty == "Gold") diffColor = ImVec4(1.0f, 0.84f, 0.0f, 1.0f);
        else if (pack.difficulty == "Platinum") diffColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
        else if (pack.difficulty == "Diamond") diffColor = ImVec4(0.4f, 0.4f, 1.0f, 1.0f);
        else if (pack.difficulty == "Champion") diffColor = ImVec4(0.8f, 0.3f, 0.8f, 1.0f);
        else if (pack.difficulty == "Grand Champion") diffColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        else if (pack.difficulty == "Supersonic Legend") diffColor = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
        ImGui::TextColored(diffColor, "%s", pack.difficulty.c_str());
        ImGui::NextColumn();

        // Shots column
        ImGui::Text("%d", pack.shotCount);
        ImGui::NextColumn();

        // Tags column (truncated)
        if (!pack.tags.empty()) {
            std::string tagsStr;
            for (size_t i = 0; i < std::min(size_t(2), pack.tags.size()); i++) {
                if (i > 0) tagsStr += ", ";
                tagsStr += pack.tags[i];
            }
            if (pack.tags.size() > 2) {
                tagsStr += "...";
            }
            ImGui::TextUnformatted(tagsStr.c_str());

            if (ImGui::IsItemHovered() && pack.tags.size() > 2) {
                std::string allTags;
                for (size_t i = 0; i < pack.tags.size(); i++) {
                    if (i > 0) allTags += ", ";
                    allTags += pack.tags[i];
                }
                ImGui::SetTooltip("%s", allTags.c_str());
            }
        }
        ImGui::NextColumn();

        // Likes column
        ImGui::Text("%d", pack.likes);
        ImGui::NextColumn();

        // Plays column
        ImGui::Text("%d", pack.plays);
        ImGui::NextColumn();

        // Actions column - NEW: Watch Video, Load Now and Add to Shuffle
        
        // Watch Video button
        if (!pack.videoUrl.empty()) {
            std::string watchLabel = "Watch##" + std::to_string(row);
            if (ImGui::SmallButton(watchLabel.c_str())) {
                // Open URL in default browser
                // ShellExecute is available via Windows headers in pch.h
                ShellExecuteA(NULL, "open", pack.videoUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Watch preview video");
            }
            ImGui::SameLine();
        }

        // Load Now button
        std::string loadLabel = "Load##" + std::to_string(row);
        if (ImGui::SmallButton(loadLabel.c_str())) {
            // Directly load the training pack
            gameWrapper->SetTimeout([this, pack](GameWrapper* gw) {
                std::string cmd = "load_training " + pack.code;
                cvarManager->executeCommand(cmd);
                LOG("SuiteSpot: Loading prejump pack: " + pack.name);
            }, 0.0f);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load this pack now");
        }

        ImGui::SameLine();

        // Add to Shuffle button with visual feedback
        std::string shuffleLabel = "+Shuffle##" + std::to_string(row);

        // Check if pack already in shuffle bag
        bool inShuffleBag = std::find_if(trainingShuffleBag.begin(), trainingShuffleBag.end(),
            [&pack](const TrainingEntry& e) { return e.code == pack.code; }) != trainingShuffleBag.end();

        if (inShuffleBag) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        }

        if (ImGui::SmallButton(shuffleLabel.c_str())) {
            if (!inShuffleBag) {
                // Add to shuffle bag
                trainingShuffleBag.push_back(pack);
                SaveShuffleBag();
                LOG("SuiteSpot: Added to shuffle: " + pack.name);
            } else {
                // Remove from shuffle bag
                trainingShuffleBag.erase(
                    std::remove_if(trainingShuffleBag.begin(), trainingShuffleBag.end(),
                        [&pack](const TrainingEntry& e) { return e.code == pack.code; }),
                    trainingShuffleBag.end()
                );
                SaveShuffleBag();
                LOG("SuiteSpot: Removed from shuffle: " + pack.name);
            }
        }

        if (inShuffleBag) {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(inShuffleBag ? "Remove from shuffle bag" : "Add to shuffle bag");
        }

        ImGui::NextColumn();
    }

    // End columns
    ImGui::Columns(1);
    ImGui::Separator();

        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "💡 Tip: Click column headers to sort | Drag column borders to resize");

    }

void SuiteSpot::RenderWindow() {
    ImGui::TextUnformatted("SuiteSpot Standalone Window");
    ImGui::Separator();
    
    if (enabled) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Plugin is ENABLED");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Plugin is DISABLED");
    }

    ImGui::Spacing();
    ImGui::Text("Current Map Mode: ");
    ImGui::SameLine();
    const char* modeNames[] = { "Freeplay", "Training", "Workshop" };
    ImGui::TextUnformatted(modeNames[mapType]);

    ImGui::Separator();
    ImGui::TextUnformatted("Post-Match Statistics:");
    
    if (postMatch.active) {
        ImGui::Text("Overlay is currently ACTIVE");
    } else {
        ImGui::Text("Overlay is currently INACTIVE");
    }

    if (ImGui::Button("Trigger Test Overlay")) {
        // Mock some data if empty
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
        
        postMatch.start = std::chrono::steady_clock::now();
        postMatch.active = true;
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Show the post-match stats overlay for testing");
    }
}
