#include "pch.h"

#include "OverlayRenderer.h"
#include "SuiteSpot.h"

#include <algorithm>
#include <cmath>

OverlayRenderer::OverlayRenderer(SuiteSpot* plugin) : plugin_(plugin) {}

void OverlayRenderer::ResetDefaults() {
    overlayOffsetX = 0.0f;
    overlayOffsetY = 0.0f;

    teamHeaderHeight = 28.0f;
    playerRowHeight = 24.0f;
    teamSectionSpacing = 12.0f;
    sectionPadding = 8.0f;

    nameColumnX = 50.0f;
    scoreColumnX = 230.0f;
    goalsColumnX = 290.0f;
    assistsColumnX = 350.0f;
    savesColumnX = 410.0f;
    shotsColumnX = 470.0f;
    pingColumnX = 530.0f;

    mainFontSize = 14.0f;
    headerFontSize = 12.0f;
    teamHeaderFontSize = 16.0f;

    blueTeamSat = 0.8f;
    blueTeamVal = 0.6f;
    orangeTeamSat = 0.9f;
    orangeTeamVal = 0.7f;
    backgroundAlpha = 0.4f;
    headerAlpha = 0.8f;

    mvpCheckmarkSize = 1.2f;
    showMvpGlow = true;
    showTeamScores = true;
    showColumnHeaders = true;

    fadeInDuration = 0.5f;
    fadeOutDuration = 2.0f;
    enableFadeEffects = true;
}

void OverlayRenderer::RenderPostMatchOverlay() {
    if (!plugin_) {
        return;
    }

    if (!ImGui::GetCurrentContext()) {
        if (plugin_->imguiCtx) {
            ImGui::SetCurrentContext(plugin_->imguiCtx);
        } else {
            return;
        }
    }

    auto& postMatch = plugin_->GetPostMatchInfo();

    // Check if overlay should still be shown
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration<float>(now - postMatch.start).count();
    const float postMatchDurationSec = plugin_->GetPostMatchDurationSec();

    // Auto-hide after duration
    if (postMatch.active && elapsed >= postMatchDurationSec) {
        postMatch.active = false;
        if (plugin_->postMatchOverlayWindow) {
            plugin_->postMatchOverlayWindow->Close();
        }
        return;
    }

    // Calculate fade
    float alpha = plugin_->GetOverlayAlpha();
    if (enableFadeEffects) {
        if (elapsed < fadeInDuration) {
            alpha *= (elapsed / fadeInDuration);
        } else if (elapsed > postMatchDurationSec - fadeOutDuration) {
            alpha *= std::max(0.0f, (postMatchDurationSec - elapsed) / fadeOutDuration);
        }
    }

    ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 overlaySize = ImVec2(std::max(400.0f, plugin_->GetOverlayWidth()),
                                      std::max(180.0f, plugin_->GetOverlayHeight()));
    ImVec2 pos = ImVec2((display.x - overlaySize.x) * 0.5f + overlayOffsetX, display.y * 0.08f + overlayOffsetY);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = pos; // Use pos as start for drawing since we are in the overlay window context
    ImVec2 winSize = overlaySize;
    ImVec2 winEnd = ImVec2(winPos.x + winSize.x, winPos.y + winSize.y);

    // Set window pos and size for the draw list if needed, but since we are called inside Begin/End
    // of PostMatchOverlayWindow::Render(), the draw list coordinates are usually absolute or relative to window.
    // However, PostMatchOverlayWindow::Render() calls SetNextWindowPos/Size.

    // Let's adjust to use window pos
    winPos = ImGui::GetWindowPos();
    winSize = ImGui::GetWindowSize();
    winEnd = ImVec2(winPos.x + winSize.x, winPos.y + winSize.y);

    // Draw background
    ImU32 baseBg = ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, backgroundAlpha * alpha));
    dl->AddRectFilled(winPos, winEnd, baseBg, 8.0f);

    // Draw header
    ImU32 headerBg = ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, headerAlpha * alpha));
    dl->AddRectFilled(winPos, ImVec2(winEnd.x, winPos.y + 34.0f), headerBg, 8.0f);

    // Title
    ImU32 titleColor = ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, alpha));
    std::string title = "MATCH COMPLETE";
    if (postMatch.overtime) title += " - OVERTIME";
    dl->AddText(ImGui::GetFont(), teamHeaderFontSize, ImVec2(winPos.x + 12.0f, winPos.y + 8.0f), titleColor, title.c_str());

    // Match info line
    std::string matchInfo = postMatch.playlist + " | " + postMatch.myTeamName + " " +
                           std::to_string(postMatch.myScore) + " - " +
                           std::to_string(postMatch.oppScore) + " " + postMatch.oppTeamName;

    dl->AddText(ImGui::GetFont(), headerFontSize, ImVec2(winPos.x + 12.0f, winPos.y + 45.0f), titleColor, matchInfo.c_str());

    // Column headers if enabled
    float contentY = winPos.y + 70.0f;
    if (showColumnHeaders) {
        ImU32 headerTextColor = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, alpha));
        dl->AddText(ImGui::GetFont(), headerFontSize, ImVec2(winPos.x + nameColumnX, contentY), headerTextColor, "Player");
        dl->AddText(ImGui::GetFont(), headerFontSize, ImVec2(winPos.x + scoreColumnX, contentY), headerTextColor, "Score");
        dl->AddText(ImGui::GetFont(), headerFontSize, ImVec2(winPos.x + goalsColumnX, contentY), headerTextColor, "Goals");
        dl->AddText(ImGui::GetFont(), headerFontSize, ImVec2(winPos.x + assistsColumnX, contentY), headerTextColor, "Assists");
        dl->AddText(ImGui::GetFont(), headerFontSize, ImVec2(winPos.x + savesColumnX, contentY), headerTextColor, "Saves");
        dl->AddText(ImGui::GetFont(), headerFontSize, ImVec2(winPos.x + shotsColumnX, contentY), headerTextColor, "Shots");
        dl->AddText(ImGui::GetFont(), headerFontSize, ImVec2(winPos.x + pingColumnX, contentY), headerTextColor, "Ping");
        contentY += playerRowHeight;
    }

    // Render players by team
    for (int teamIdx = 0; teamIdx <= 1; teamIdx++) {
        // Team header
        bool isMyTeam = false;
        for (const auto& p : postMatch.players) {
            if (p.isLocal && p.teamIndex == teamIdx) {
                isMyTeam = true;
                break;
            }
        }

        std::string teamName = isMyTeam ? postMatch.myTeamName : postMatch.oppTeamName;
        int teamScore = isMyTeam ? postMatch.myScore : postMatch.oppScore;

        // Team section
        ImU32 teamColor;
        auto hsvToRgb = [](float h, float s, float v, float a) -> ImVec4 {
            float c = v * s;
            float x = c * (1 - abs(fmod(h / 60.0f, 2) - 1));
            float m = v - c;
            float r = 0, g = 0, b = 0;
            if (h < 60) { r = c; g = x; b = 0; }
            else if (h < 120) { r = x; g = c; b = 0; }
            else if (h < 180) { r = 0; g = c; b = x; }
            else if (h < 240) { r = 0; g = x; b = c; }
            else if (h < 300) { r = x; g = 0; b = c; }
            else { r = c; g = 0; b = x; }
            return ImVec4(r + m, g + m, b + m, a);
        };

        if (teamIdx == 0) {
            teamColor = ImGui::GetColorU32(hsvToRgb(plugin_->GetBlueTeamHue(), blueTeamSat, blueTeamVal, alpha));
        } else {
            teamColor = ImGui::GetColorU32(hsvToRgb(plugin_->GetOrangeTeamHue(), orangeTeamSat, orangeTeamVal, alpha));
        }

        std::string teamHeader = teamName;
        if (showTeamScores) {
            teamHeader += " - " + std::to_string(teamScore);
        }

        dl->AddRectFilled(ImVec2(winPos.x + sectionPadding, contentY),
                         ImVec2(winEnd.x - sectionPadding, contentY + teamHeaderHeight),
                         teamColor, 4.0f);
        dl->AddText(ImGui::GetFont(), teamHeaderFontSize, ImVec2(winPos.x + nameColumnX, contentY + 4.0f), titleColor, teamHeader.c_str());
        contentY += teamHeaderHeight + 4.0f;

        // Players in this team
        for (const auto& player : postMatch.players) {
            if (player.teamIndex != teamIdx) continue;

            ImU32 playerColor = player.isMVP && showMvpGlow ?
                              ImGui::GetColorU32(ImVec4(1.f, 0.84f, 0.f, alpha)) : titleColor;

            // MVP indicator
            if (player.isMVP) {
                dl->AddText(ImGui::GetFont(), mainFontSize * mvpCheckmarkSize,
                           ImVec2(winPos.x + nameColumnX - 20.0f, contentY),
                           ImGui::GetColorU32(ImVec4(1.f, 0.84f, 0.f, alpha)), "★");
            }

            // Player stats
            dl->AddText(ImGui::GetFont(), mainFontSize, ImVec2(winPos.x + nameColumnX, contentY), playerColor, player.name.c_str());
            dl->AddText(ImGui::GetFont(), mainFontSize, ImVec2(winPos.x + scoreColumnX, contentY), playerColor, std::to_string(player.score).c_str());
            dl->AddText(ImGui::GetFont(), mainFontSize, ImVec2(winPos.x + goalsColumnX, contentY), playerColor, std::to_string(player.goals).c_str());
            dl->AddText(ImGui::GetFont(), mainFontSize, ImVec2(winPos.x + assistsColumnX, contentY), playerColor, std::to_string(player.assists).c_str());
            dl->AddText(ImGui::GetFont(), mainFontSize, ImVec2(winPos.x + savesColumnX, contentY), playerColor, std::to_string(player.saves).c_str());
            dl->AddText(ImGui::GetFont(), mainFontSize, ImVec2(winPos.x + shotsColumnX, contentY), playerColor, std::to_string(player.shots).c_str());
            dl->AddText(ImGui::GetFont(), mainFontSize, ImVec2(winPos.x + pingColumnX, contentY), playerColor, std::to_string(player.ping).c_str());

            contentY += playerRowHeight;
        }

        contentY += teamSectionSpacing;
    }
}
