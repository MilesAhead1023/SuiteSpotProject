#include "pch.h"

#include "SettingsUI.h"
#include "LoadoutUI.h"
#include "PrejumpUI.h"
#include "SuiteSpot.h"
#include "OverlayRenderer.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>

SettingsUI::SettingsUI(SuiteSpot* plugin) : plugin_(plugin) {}

void SettingsUI::RenderMainSettingsWindow() {
    if (!plugin_) {
        return;
    }

    // Header with metadata
    ImGui::TextUnformatted("SuiteSpot");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "By: Flicks Creations");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Version: %s", plugin_version);

    bool enabledValue = plugin_->IsEnabled();
    int mapTypeValue = plugin_->GetMapType();
    bool autoQueueValue = plugin_->IsAutoQueueEnabled();
    bool trainingShuffleEnabledValue = plugin_->IsTrainingShuffleEnabled();
    int delayQueueSecValue = plugin_->GetDelayQueueSec();
    int delayFreeplaySecValue = plugin_->GetDelayFreeplaySec();
    int delayTrainingSecValue = plugin_->GetDelayTrainingSec();
    int delayWorkshopSecValue = plugin_->GetDelayWorkshopSec();
    int currentIndexValue = plugin_->GetCurrentIndex();
    int currentTrainingIndexValue = plugin_->GetCurrentTrainingIndex();
    int currentWorkshopIndexValue = plugin_->GetCurrentWorkshopIndex();

    // Only show status if enabled
    if (enabledValue) {
        ImGui::SameLine(420);

        const char* modeNames[] = {"Freeplay", "Training", "Workshop"};
        std::string currentMap = "<none>";
        std::string queueDelayStr = std::to_string(delayQueueSecValue) + "s";
        std::string mapDelayStr = "0s";
        std::string shotProgressStr = "";
        const ImVec4 white = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        // Get current selection and appropriate delay
        if (mapTypeValue == 0) {
            if (!RLMaps.empty() && currentIndexValue >= 0 && currentIndexValue < (int)RLMaps.size()) {
                currentMap = RLMaps[currentIndexValue].name;
            }
            mapDelayStr = std::to_string(delayFreeplaySecValue) + "s";
        } else if (mapTypeValue == 1) {
            if (!RLTraining.empty() && currentTrainingIndexValue >= 0 && currentTrainingIndexValue < (int)RLTraining.size()) {
                currentMap = RLTraining[currentTrainingIndexValue].name + " (Shots:" + std::to_string(RLTraining[currentTrainingIndexValue].shotCount) + ")";
            }
            mapDelayStr = std::to_string(delayTrainingSecValue) + "s";

            if (trainingShuffleEnabledValue) {
                int shuffleCount = 0;
                if (!plugin_->trainingShuffleBag.empty()) {
                    shuffleCount = static_cast<int>(plugin_->trainingShuffleBag.size());
                } else if (!plugin_->selectedTrainingIndices.empty()) {
                    shuffleCount = static_cast<int>(plugin_->selectedTrainingIndices.size());
                }
                // Only show shuffle indicator when we have a non-empty selection/bag.
                if (shuffleCount > 0) {
                    if (shuffleCount == 1) {
                        // Single pack in shuffle – show its name/shots.
                        const TrainingEntry* entry = nullptr;
                        if (!plugin_->trainingShuffleBag.empty()) {
                            entry = &plugin_->trainingShuffleBag.front();
                        } else if (!plugin_->selectedTrainingIndices.empty()) {
                            int idx = *plugin_->selectedTrainingIndices.begin();
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
        } else if (mapTypeValue == 2) {
            if (!RLWorkshop.empty() && currentWorkshopIndexValue >= 0 && currentWorkshopIndexValue < (int)RLWorkshop.size()) {
                currentMap = RLWorkshop[currentWorkshopIndexValue].name;
            }
            mapDelayStr = std::to_string(delayWorkshopSecValue) + "s";
        }

        // Map mode status (always green when enabled)
        const ImVec4 green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 red   = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        std::string modeText = "Mode: " + std::string(modeNames[mapTypeValue]);
        if (delayFreeplaySecValue > 0 || delayTrainingSecValue > 0 || delayWorkshopSecValue > 0) {
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
        const ImVec4 queueColor = autoQueueValue ? green : red;
        if (delayQueueSecValue > 0) {
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
            ImGui::Spacing();

            // 1) Enable/Disable + Standalone window entry
            CVarWrapper enableCvar = plugin_->cvarManager->getCvar("suitespot_enabled");
            if (!enableCvar) {
                ImGui::EndTabItem();
                ImGui::EndTabBar();
                return;
            }

            RenderGeneralTab(enabledValue, mapTypeValue);
            RenderAutoQueueTab(autoQueueValue, delayQueueSecValue);
            RenderMapSelectionTab(mapTypeValue, trainingShuffleEnabledValue, currentIndexValue,
                currentTrainingIndexValue, currentWorkshopIndexValue, delayFreeplaySecValue,
                delayTrainingSecValue, delayWorkshopSecValue);

            // Close Main Settings tab
            ImGui::EndTabItem();
        } // End Main Settings tab

        // ===== LOADOUT MANAGEMENT TAB =====
        if (ImGui::BeginTabItem("Loadout Management")) {
            if (plugin_->loadoutUI) {
            plugin_->loadoutUI->RenderLoadoutControls();
        }
            ImGui::EndTabItem();
        } // End Loadout Management tab

        // ===== OVERLAY LAYOUT TAB =====
        if (ImGui::BeginTabItem("Overlay Layout")) {
            RenderOverlayTab();
            ImGui::EndTabItem();
        } // End Overlay Layout tab

        // ===== PREJUMP PACKS TAB =====
        if (ImGui::BeginTabItem("Prejump Packs")) {
            if (plugin_->prejumpUI) {
            plugin_->prejumpUI->RenderPrejumpTab();
        }
            ImGui::EndTabItem();
        } // End Prejump Packs tab

        // Close the tab bar
        ImGui::EndTabBar();
    } // End tab bar
}

void SettingsUI::RenderGeneralTab(bool& enabledValue, int& mapTypeValue) {
    ImGui::BeginGroup();
    if (ImGui::Checkbox("Enable SuiteSpot", &enabledValue)) {
        plugin_->cvarManager->getCvar("suitespot_enabled").setValue(enabledValue);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable/disable all SuiteSpot auto-loading and queuing features");
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    if (ImGui::Button("Open Standalone Window")) {
        plugin_->cvarManager->executeCommand("suitespot_toggle_window");
    }
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::TextUnformatted("Map Mode:");
    ImGui::SameLine();
    ImGui::BeginGroup();
    const char* mapLabels[] = {"Freeplay", "Training", "Workshop"};
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine(0, 16);
        if (ImGui::RadioButton(mapLabels[i], mapTypeValue == i)) {
            mapTypeValue = i;
            plugin_->cvarManager->getCvar("suitespot_map_type").setValue(mapTypeValue);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Choose which map type loads after matches:\nFreeplay = Official | Training = Custom Packs | Workshop = Modded Maps");
    }
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::Separator();
}

void SettingsUI::RenderAutoQueueTab(bool& autoQueueValue, int& delayQueueSecValue) {
    if (ImGui::Checkbox("Auto-Queue Next Match", &autoQueueValue)) {
        plugin_->cvarManager->getCvar("suitespot_auto_queue").setValue(autoQueueValue);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically queue into the next match after the current match ends.\nQueue delay starts at match end, independent of map load.");
    }

    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt("Delay Queue (sec)", &delayQueueSecValue)) {
        delayQueueSecValue = std::clamp(delayQueueSecValue, 0, 300);
        plugin_->cvarManager->getCvar("suitespot_delay_queue_sec").setValue(delayQueueSecValue);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("0-300s");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Wait this many seconds before queuing (independent of map load). Range: 0-300s");
    }

    ImGui::Spacing();
    ImGui::Separator();
}

void SettingsUI::RenderMapSelectionTab(int mapTypeValue,
    bool trainingShuffleEnabledValue,
    int& currentIndexValue,
    int& currentTrainingIndexValue,
    int& currentWorkshopIndexValue,
    int& delayFreeplaySecValue,
    int& delayTrainingSecValue,
    int& delayWorkshopSecValue) {
    ImGui::TextUnformatted("Map Selection:");
    ImGui::Spacing();

    if (mapTypeValue == 0) {
        // Freeplay maps
        if (!RLMaps.empty()) {
            currentIndexValue = std::clamp(currentIndexValue, 0, static_cast<int>(RLMaps.size() - 1));
        } else {
            currentIndexValue = 0;
        }

        const char* freeplayLabel = RLMaps.empty() ? "<none>" : RLMaps[currentIndexValue].name.c_str();
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Freeplay Maps", freeplayLabel)) {
            for (int i = 0; i < (int)RLMaps.size(); ++i) {
                bool selected = (i == currentIndexValue);
                if (ImGui::Selectable(RLMaps[i].name.c_str(), selected)) {
                    currentIndexValue = i;
                    plugin_->cvarManager->getCvar("suitespot_current_freeplay_index").setValue(currentIndexValue);
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select which stadium to load after matches");
        }

        float rightEdge = ImGui::GetWindowContentRegionMax().x;
        float loadBtnWidth = ImGui::CalcTextSize("Load Now").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), rightEdge - loadBtnWidth));
        if (ImGui::Button("Load Now##freeplay")) {
            if (!RLMaps.empty() && currentIndexValue >= 0 && currentIndexValue < (int)RLMaps.size()) {
                std::string mapCode = RLMaps[currentIndexValue].code;
                SuiteSpot* plugin = plugin_;
                plugin_->gameWrapper->SetTimeout([plugin, mapCode](GameWrapper* gw) {
                    plugin->cvarManager->executeCommand("load_freeplay " + mapCode);
                }, 0.0f);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load the selected freeplay map immediately");
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Freeplay Settings:");
        ImGui::SetNextItemWidth(220);
        if (ImGui::InputInt("Delay Freeplay (sec)", &delayFreeplaySecValue)) {
            delayFreeplaySecValue = std::clamp(delayFreeplaySecValue, 0, 300);
            plugin_->cvarManager->getCvar("suitespot_delay_freeplay_sec").setValue(delayFreeplaySecValue);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("0-300s");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Wait this many seconds after match ends before loading Freeplay. Range: 0-300s");
        }
    }
    else if (mapTypeValue == 1) {
        // Training maps
        if (!RLTraining.empty()) {
            currentTrainingIndexValue = std::clamp(currentTrainingIndexValue, 0, static_cast<int>(RLTraining.size() - 1));
        } else {
            currentTrainingIndexValue = 0;
        }

        const char* trainingLabel = "<none>";
        if (!RLTraining.empty() && currentTrainingIndexValue >= 0 && currentTrainingIndexValue < (int)RLTraining.size()) {
            trainingLabelBuf = RLTraining[currentTrainingIndexValue].name + " (Shots:" + std::to_string(RLTraining[currentTrainingIndexValue].shotCount) + ")";
            trainingLabel = trainingLabelBuf.c_str();
        }

        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Training Packs", trainingLabel)) {
            for (int i = 0; i < (int)RLTraining.size(); ++i) {
                bool selected = (i == currentTrainingIndexValue);
                std::string itemLabel = RLTraining[i].name + " (Shots:" + std::to_string(RLTraining[i].shotCount) + ")";
                if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                    currentTrainingIndexValue = i;
                    plugin_->cvarManager->getCvar("suitespot_current_training_index").setValue(currentTrainingIndexValue);
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
            if (currentTrainingIndexValue >= 0 && currentTrainingIndexValue < (int)RLTraining.size()) {
                previousCode = RLTraining[currentTrainingIndexValue].code;
            }
            plugin_->LoadTrainingMaps();
            if (!previousCode.empty()) {
                auto it = std::find_if(RLTraining.begin(), RLTraining.end(),
                    [&](const TrainingEntry& entry){ return entry.code == previousCode; });
                if (it != RLTraining.end()) {
                    currentTrainingIndexValue = static_cast<int>(std::distance(RLTraining.begin(), it));
                    plugin_->cvarManager->getCvar("suitespot_current_training_index").setValue(currentTrainingIndexValue);
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Refresh the training pack list from storage");
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Now##training")) {
            if (!RLTraining.empty()) {
                int indexToLoad = currentTrainingIndexValue;

                if (trainingShuffleEnabledValue) {
                    if (plugin_->trainingShuffleBag.empty()) {
                        plugin_->trainingShuffleBag.clear();
                        if (!plugin_->selectedTrainingIndices.empty()) {
                            for (int idx : plugin_->selectedTrainingIndices) {
                                if (idx >= 0 && idx < static_cast<int>(RLTraining.size())) {
                                    plugin_->trainingShuffleBag.push_back(RLTraining[idx]);
                                }
                            }
                        }
                    }

                    if (!plugin_->trainingShuffleBag.empty()) {
                        indexToLoad = plugin_->GetRandomTrainingIndex();
                    }
                }

                indexToLoad = std::clamp(indexToLoad, 0, (int)RLTraining.size() - 1);
                std::string packCode = RLTraining[indexToLoad].code;
                SuiteSpot* plugin = plugin_;
                plugin_->gameWrapper->SetTimeout([plugin, packCode](GameWrapper* gw) {
                    plugin->cvarManager->executeCommand("load_training " + packCode);
                }, 0.0f);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load the selected training pack immediately");
        }

        ImGui::SameLine();
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
        if (ImGui::InputInt("Delay Training (sec)", &delayTrainingSecValue)) {
            delayTrainingSecValue = std::clamp(delayTrainingSecValue, 0, 300);
            plugin_->cvarManager->getCvar("suitespot_delay_training_sec").setValue(delayTrainingSecValue);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("0-300s");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Wait this many seconds after match ends before loading Training. Range: 0-300s");
        }

        if (showAddTrainingForm) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("Add Custom Training Pack:");
            ImGui::Spacing();

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
                    plugin_->SaveTrainingMaps();
                    plugin_->LoadTrainingMaps();
                    auto it = std::find_if(RLTraining.begin(), RLTraining.end(),
                        [&](const TrainingEntry& entry){ return entry.code == codeStr; });
                    if (it != RLTraining.end()) {
                        currentTrainingIndexValue = static_cast<int>(std::distance(RLTraining.begin(), it));
                        plugin_->cvarManager->getCvar("suitespot_current_training_index").setValue(currentTrainingIndexValue);
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
    }
    else if (mapTypeValue == 2) {
        // Workshop maps
        if (!RLWorkshop.empty()) {
            currentWorkshopIndexValue = std::clamp(currentWorkshopIndexValue, 0, static_cast<int>(RLWorkshop.size() - 1));
        } else {
            currentWorkshopIndexValue = 0;
        }

        const char* workshopLabel = RLWorkshop.empty() ? "<none>" : RLWorkshop[currentWorkshopIndexValue].name.c_str();
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Workshop Maps", workshopLabel)) {
            for (int i = 0; i < (int)RLWorkshop.size(); ++i) {
                bool selected = (i == currentWorkshopIndexValue);
                if (ImGui::Selectable(RLWorkshop[i].name.c_str(), selected)) {
                    currentWorkshopIndexValue = i;
                    plugin_->cvarManager->getCvar("suitespot_current_workshop_index").setValue(currentWorkshopIndexValue);
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
            if (currentWorkshopIndexValue >= 0 && currentWorkshopIndexValue < (int)RLWorkshop.size()) {
                previousPath = RLWorkshop[currentWorkshopIndexValue].filePath;
            }
            plugin_->LoadWorkshopMaps();
            if (!previousPath.empty()) {
                auto it = std::find_if(RLWorkshop.begin(), RLWorkshop.end(),
                    [&](const WorkshopEntry& entry){ return entry.filePath == previousPath; });
                if (it != RLWorkshop.end()) {
                    currentWorkshopIndexValue = static_cast<int>(std::distance(RLWorkshop.begin(), it));
                    plugin_->cvarManager->getCvar("suitespot_current_workshop_index").setValue(currentWorkshopIndexValue);
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Refresh the workshop map list");
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Now##workshop")) {
            if (!RLWorkshop.empty() && currentWorkshopIndexValue >= 0 && currentWorkshopIndexValue < (int)RLWorkshop.size()) {
                std::string filePath = RLWorkshop[currentWorkshopIndexValue].filePath;
                SuiteSpot* plugin = plugin_;
                plugin_->gameWrapper->SetTimeout([plugin, filePath](GameWrapper* gw) {
                    plugin->cvarManager->executeCommand("load_workshop \"" + filePath + "\"");
                }, 0.0f);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load the selected workshop map immediately");
        }

        ImGui::Spacing();
        if (ImGui::TreeNodeEx("Workshop Source", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!workshopPathInit) {
                auto resolved = plugin_->ResolveConfiguredWorkshopRoot();
                if (!resolved.empty()) {
                    workshopPathCache = resolved.string();
                } else {
                    workshopPathCache = R"(C:\\Program Files (x86)\\Steam\\steamapps\\common\\rocketleague\\TAGame\\CookedPCConsole\\mods)";
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
                std::filesystem::path cfgPath = plugin_->GetWorkshopLoaderConfigPath();
                std::error_code ec;
                std::filesystem::create_directories(cfgPath.parent_path(), ec);
                std::ofstream cfg(cfgPath.string(), std::ios::trunc);
                if (cfg.is_open()) {
                    cfg << "MapsFolderPath=" << workshopPathBuf << "\n";
                    cfg.close();
                    workshopPathCache = workshopPathBuf;
                    plugin_->LoadWorkshopMaps();
                    if (!RLWorkshop.empty()) {
                        currentWorkshopIndexValue = std::clamp(currentWorkshopIndexValue, 0, static_cast<int>(RLWorkshop.size() - 1));
                    } else {
                        currentWorkshopIndexValue = 0;
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
        if (ImGui::InputInt("Delay Workshop (sec)", &delayWorkshopSecValue)) {
            delayWorkshopSecValue = std::clamp(delayWorkshopSecValue, 0, 300);
            plugin_->cvarManager->getCvar("suitespot_delay_workshop_sec").setValue(delayWorkshopSecValue);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("0-300s");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Wait this many seconds after match ends before loading Workshop. Range: 0-300s");
        }
    }
}

void SettingsUI::RenderOverlayTab() {
    ImGui::Spacing();

    auto* overlay = plugin_->overlayRenderer;
    if (!overlay) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "OverlayRenderer not initialized");
        return;
    }

    float postMatchDurationSecValue = plugin_->GetPostMatchDurationSec();
    float overlayWidthValue = plugin_->GetOverlayWidth();
    float overlayHeightValue = plugin_->GetOverlayHeight();
    float overlayAlphaValue = plugin_->GetOverlayAlpha();
    float blueTeamHueValue = plugin_->GetBlueTeamHue();
    float orangeTeamHueValue = plugin_->GetOrangeTeamHue();

    // Reset button
    if (ImGui::Button("Reset to Defaults")) {
        // Reset all values to defaults
        overlayWidthValue = 880.0f; overlayHeightValue = 400.0f;
        blueTeamHueValue = 240.0f; orangeTeamHueValue = 25.0f; overlayAlphaValue = 0.85f;
        postMatchDurationSecValue = 15.0f;
        overlay->ResetDefaults();
        plugin_->cvarManager->getCvar("overlay_width").setValue(overlayWidthValue);
        plugin_->cvarManager->getCvar("overlay_height").setValue(overlayHeightValue);
        plugin_->cvarManager->getCvar("overlay_alpha").setValue(overlayAlphaValue);
        plugin_->cvarManager->getCvar("overlay_duration").setValue(postMatchDurationSecValue);
        plugin_->cvarManager->getCvar("blue_team_hue").setValue(blueTeamHueValue);
        plugin_->cvarManager->getCvar("orange_team_hue").setValue(orangeTeamHueValue);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset all overlay settings to default values");
    }
    ImGui::Separator();

    // === WINDOW LAYOUT ===
    if (ImGui::CollapsingHeader("Window Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Display Time (sec)", &postMatchDurationSecValue, 5.0f, 60.0f, "%.1f")) {
            plugin_->cvarManager->getCvar("overlay_duration").setValue(postMatchDurationSecValue);
        }

        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Width##overlay", &overlayWidthValue, 400.0f, 1600.0f, "%.0f")) {
            plugin_->cvarManager->getCvar("overlay_width").setValue(overlayWidthValue);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Height##overlay", &overlayHeightValue, 200.0f, 800.0f, "%.0f")) {
            plugin_->cvarManager->getCvar("overlay_height").setValue(overlayHeightValue);
        }

        float offsetX = overlay->GetOverlayOffsetX();
        float offsetY = overlay->GetOverlayOffsetY();
        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Offset X##overlay", &offsetX, -1000.0f, 1000.0f, "%.0f")) {
            overlay->SetOverlayOffsetX(offsetX);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Offset Y##overlay", &offsetY, -500.0f, 500.0f, "%.0f")) {
            overlay->SetOverlayOffsetY(offsetY);
        }
    }

    // === TEAM SECTIONS ===
    if (ImGui::CollapsingHeader("Team Sections")) {
        float teamHeaderHeight = overlay->GetTeamHeaderHeight();
        float playerRowHeight = overlay->GetPlayerRowHeight();
        float teamSectionSpacing = overlay->GetTeamSectionSpacing();
        float sectionPadding = overlay->GetSectionPadding();

        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Header Height", &teamHeaderHeight, 20.0f, 50.0f, "%.0f")) {
            overlay->SetTeamHeaderHeight(teamHeaderHeight);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Player Row Height", &playerRowHeight, 16.0f, 40.0f, "%.0f")) {
            overlay->SetPlayerRowHeight(playerRowHeight);
        }

        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Section Spacing", &teamSectionSpacing, 0.0f, 50.0f, "%.0f")) {
            overlay->SetTeamSectionSpacing(teamSectionSpacing);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        if (ImGui::SliderFloat("Section Padding", &sectionPadding, 0.0f, 20.0f, "%.0f")) {
            overlay->SetSectionPadding(sectionPadding);
        }
    }

    // === COLUMN POSITIONS ===
    if (ImGui::CollapsingHeader("Column Positions")) {
        ImGui::Text("Adjust horizontal position of each stat column:");

        float nameColumnX = overlay->GetNameColumnX();
        float scoreColumnX = overlay->GetScoreColumnX();
        float goalsColumnX = overlay->GetGoalsColumnX();
        float assistsColumnX = overlay->GetAssistsColumnX();
        float savesColumnX = overlay->GetSavesColumnX();
        float shotsColumnX = overlay->GetShotsColumnX();
        float pingColumnX = overlay->GetPingColumnX();

        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Name##col", &nameColumnX, 20.0f, 200.0f, "%.0f")) {
            overlay->SetNameColumnX(nameColumnX);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Score##col", &scoreColumnX, 150.0f, 400.0f, "%.0f")) {
            overlay->SetScoreColumnX(scoreColumnX);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Goals##col", &goalsColumnX, 200.0f, 450.0f, "%.0f")) {
            overlay->SetGoalsColumnX(goalsColumnX);
        }

        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Assists##col", &assistsColumnX, 250.0f, 500.0f, "%.0f")) {
            overlay->SetAssistsColumnX(assistsColumnX);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Saves##col", &savesColumnX, 300.0f, 550.0f, "%.0f")) {
            overlay->SetSavesColumnX(savesColumnX);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Shots##col", &shotsColumnX, 350.0f, 600.0f, "%.0f")) {
            overlay->SetShotsColumnX(shotsColumnX);
        }

        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Ping##col", &pingColumnX, 400.0f, 700.0f, "%.0f")) {
            overlay->SetPingColumnX(pingColumnX);
        }
    }

    // === COLORS & STYLING ===
    if (ImGui::CollapsingHeader("Colors & Styling")) {
        float blueSat = overlay->GetBlueTeamSat();
        float blueVal = overlay->GetBlueTeamVal();
        float orangeSat = overlay->GetOrangeTeamSat();
        float orangeVal = overlay->GetOrangeTeamVal();
        float backgroundAlpha = overlay->GetBackgroundAlpha();
        float headerAlpha = overlay->GetHeaderAlpha();

        ImGui::Text("Blue Team Colors:");
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Blue Hue", &blueTeamHueValue, 0.0f, 360.0f, "%.0f")) {
            plugin_->cvarManager->getCvar("blue_team_hue").setValue(blueTeamHueValue);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Blue Saturation", &blueSat, 0.0f, 1.0f, "%.2f")) {
            overlay->SetBlueTeamSat(blueSat);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Blue Brightness", &blueVal, 0.0f, 1.0f, "%.2f")) {
            overlay->SetBlueTeamVal(blueVal);
        }

        ImGui::Text("Orange Team Colors:");
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Orange Hue", &orangeTeamHueValue, 0.0f, 360.0f, "%.0f")) {
            plugin_->cvarManager->getCvar("orange_team_hue").setValue(orangeTeamHueValue);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Orange Saturation", &orangeSat, 0.0f, 1.0f, "%.2f")) {
            overlay->SetOrangeTeamSat(orangeSat);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Orange Brightness", &orangeVal, 0.0f, 1.0f, "%.2f")) {
            overlay->SetOrangeTeamVal(orangeVal);
        }

        ImGui::Text("Transparency:");
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Overall Alpha", &overlayAlphaValue, 0.1f, 1.0f, "%.2f")) {
            plugin_->cvarManager->getCvar("overlay_alpha").setValue(overlayAlphaValue);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Background Alpha", &backgroundAlpha, 0.0f, 1.0f, "%.2f")) {
            overlay->SetBackgroundAlpha(backgroundAlpha);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Header Alpha", &headerAlpha, 0.0f, 1.0f, "%.2f")) {
            overlay->SetHeaderAlpha(headerAlpha);
        }
    }

    // === CONTENT OPTIONS ===
    if (ImGui::CollapsingHeader("Content Options")) {
        bool showTeamScores = overlay->GetShowTeamScores();
        bool showColumnHeaders = overlay->GetShowColumnHeaders();
        bool showMvpGlow = overlay->GetShowMvpGlow();
        float mvpCheckmarkSize = overlay->GetMvpCheckmarkSize();
        bool enableFadeEffects = overlay->GetEnableFadeEffects();
        float fadeInDuration = overlay->GetFadeInDuration();
        float fadeOutDuration = overlay->GetFadeOutDuration();

        if (ImGui::Checkbox("Show Team Scores in Headers", &showTeamScores)) {
            overlay->SetShowTeamScores(showTeamScores);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Show Column Headers", &showColumnHeaders)) {
            overlay->SetShowColumnHeaders(showColumnHeaders);
        }

        if (ImGui::Checkbox("Show MVP Glow Effect", &showMvpGlow)) {
            overlay->SetShowMvpGlow(showMvpGlow);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("MVP Checkmark Size", &mvpCheckmarkSize, 0.5f, 3.0f, "%.1f")) {
            overlay->SetMvpCheckmarkSize(mvpCheckmarkSize);
        }

        if (ImGui::Checkbox("Enable Fade Effects", &enableFadeEffects)) {
            overlay->SetEnableFadeEffects(enableFadeEffects);
        }
        if (enableFadeEffects) {
            ImGui::SetNextItemWidth(140);
            if (ImGui::SliderFloat("Fade In Time", &fadeInDuration, 0.1f, 2.0f, "%.1f")) {
                overlay->SetFadeInDuration(fadeInDuration);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140);
            if (ImGui::SliderFloat("Fade Out Time", &fadeOutDuration, 0.5f, 5.0f, "%.1f")) {
                overlay->SetFadeOutDuration(fadeOutDuration);
            }
        }
    }

    // === FONT SIZES ===
    if (ImGui::CollapsingHeader("Font Sizes")) {
        float mainFontSize = overlay->GetMainFontSize();
        float headerFontSize = overlay->GetHeaderFontSize();
        float teamHeaderFontSize = overlay->GetTeamHeaderFontSize();

        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Main Text Size", &mainFontSize, 8.0f, 24.0f, "%.0f")) {
            overlay->SetMainFontSize(mainFontSize);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Header Text Size", &headerFontSize, 8.0f, 20.0f, "%.0f")) {
            overlay->SetHeaderFontSize(headerFontSize);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::SliderFloat("Team Header Size", &teamHeaderFontSize, 10.0f, 28.0f, "%.0f")) {
            overlay->SetTeamHeaderFontSize(teamHeaderFontSize);
        }
    }

    // Real-time preview info
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "💡 Changes apply in real-time to the test overlay");
    ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "✨ All settings are automatically saved");
}
