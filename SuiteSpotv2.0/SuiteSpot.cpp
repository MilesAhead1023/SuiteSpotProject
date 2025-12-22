#include "pch.h"
#include "SuiteSpot.h"
#include "MapList.h"
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

// #detailed comments: INTERNAL HELPERS NAMESPACE
// The functions declared in this unnamed namespace are intentionally
// given internal linkage so they remain private to this translation unit
// (SuiteSpot.cpp). Keeping these helpers local avoids accidental
// dependencies from other compilation units and simplifies reasoning
// about side-effects. They are small, focused utilities used by the
// SuiteSpot class implementation below (string trimming, case-insensitive
// compares, env expansion, etc.).
//
// INVARIANT: These helpers must not be promoted to public API because
// other code assumes SuiteSpot's public interface remains the single
// integration surface. Changing signatures or visibility may break
// plugin ABI or runtime interactions with BakkesMod.
//
// DO NOT CHANGE: Trimming/quoting logic is conservative and intentionally
// tolerates malformed input; altering it risks changing map lookup
// behaviour and file resolution. Maintain exact semantics unless a
// comprehensive integration test guarantees no behavioral change.
namespace
{
    std::string Trim(const std::string& value)
    {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            return {};
        }
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    std::string StripQuotes(const std::string& value)
    {
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\'')))
        {
            return value.substr(1, value.size() - 2);
        }
        return value;
    }

    std::string ExpandEnvAndHome(const std::string& input)
    {
        std::string expanded;
        expanded.reserve(input.size());

        for (size_t i = 0; i < input.size();)
        {
            if (input[i] == '%' )
            {
                const auto end = input.find('%', i + 1);
                if (end != std::string::npos)
                {
                    const auto varName = input.substr(i + 1, end - i - 1);
                    if (!varName.empty())
                    {
                        if (const char* val = std::getenv(varName.c_str()))
                        {
                            expanded.append(val);
                        }
                    }
                    i = end + 1;
                    continue;
                }
            }

            expanded.push_back(input[i]);
            ++i;
        }

        if (!expanded.empty() && expanded.front() == '~')
        {
            const char* home = std::getenv("USERPROFILE");
            if (!home)
            {
                home = std::getenv("HOME");
            }
            if (home)
            {
                std::filesystem::path homePath(home);
                std::filesystem::path remainder = expanded.substr(1);
                expanded = (homePath / remainder).string();
            }
        }

        return expanded;
    }

    int CaseInsensitiveCompare(const std::string& a, const std::string& b)
    {
        const size_t len = std::min(a.size(), b.size());
        for (size_t i = 0; i < len; ++i)
        {
            const unsigned char ca = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(a[i])));
            const unsigned char cb = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(b[i])));
            if (ca < cb) return -1;
            if (ca > cb) return 1;
        }

        if (a.size() < b.size()) return -1;
        if (a.size() > b.size()) return 1;
        return 0;
    }

    bool StartsWithCaseInsensitive(const std::string& value, const std::string& prefix)
    {
        if (value.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(value[i])) != std::tolower(static_cast<unsigned char>(prefix[i])))
            {
                return false;
            }
        }
        return true;
    }

int ParseTrailingShots(const std::string& nameField)
{
        // Legacy format: "Name (#)" or "Name( # )"
        const auto openPos = nameField.find_last_of('(');
        const auto closePos = nameField.find_last_of(')');
        if (openPos == std::string::npos || closePos == std::string::npos || closePos <= openPos) {
            return 0;
        }
        std::string inside = Trim(nameField.substr(openPos + 1, closePos - openPos - 1));
        // Expect something like "12" or "Shots:12" but in legacy it was just number.
        if (StartsWithCaseInsensitive(inside, "shots")) {
            auto colonPos = inside.find(':');
            if (colonPos != std::string::npos) {
                inside = Trim(inside.substr(colonPos + 1));
            }
        }
        try {
            return std::max(0, std::stoi(inside));
        } catch (...) {
            return 0;
        }
    }
}


// ===== SuiteSpot persistence helpers =====
std::filesystem::path SuiteSpot::GetDataRoot() const {
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return std::filesystem::path();
    return std::filesystem::path(appdata) / "bakkesmod" / "bakkesmod" / "data";
}
std::filesystem::path SuiteSpot::GetSuiteTrainingDir() const { return GetDataRoot() / "SuiteTraining"; }
std::filesystem::path SuiteSpot::GetTrainingFilePath() const { return GetSuiteTrainingDir() / "SuiteSpotTrainingMaps.txt"; }
std::filesystem::path SuiteSpot::GetShuffleBagPath() const { return GetSuiteTrainingDir() / "SuiteShuffleBag.txt"; }
void SuiteSpot::EnsureDataDirectories() const {
    std::error_code ec;
    auto root = GetDataRoot();
    if (!root.empty()) std::filesystem::create_directories(root, ec);
    ec.clear(); std::filesystem::create_directories(GetSuiteTrainingDir(), ec);
}

// #detailed comments: RenderOverlayWindow
// Purpose: Centralized rendering entry used by both Render() and
// RenderWindow() to draw the post-match overlay using ImGui low-level
// primitives (GetWindowDrawList, AddText, AddRectFilled, etc.).
//
// Key responsibilities:
//  - Ensure an ImGui context is active before any ImGui calls.
//  - Handle the overlay's lifetime (start time + duration) and fading.
//  - Compute layout and clamp values to safe ranges to avoid degenerate
//    geometry which can cause visual glitches or ImGui asserts.
//
// INVARIANT: This code must not perform any blocking I/O or scheduling
// and must be safe to call every frame from the UI thread. The fading
// math relies on postMatch.start being set to steady_clock::now() when
// the overlay is activated; changing the time source will affect timing.
//
// DO NOT CHANGE: The overlayOpenedByUs logic integrates with external
// menu toggling via cvarManager->executeCommand("togglemenu SuiteSpot");
// altering this behavior can break the delicate interaction between
// automatic overlay open/close and user-initiated menu state.
void SuiteSpot::RenderOverlayWindow() {
    // Ensure an ImGui context is active before doing any rendering work.
    if (!ImGui::GetCurrentContext()) {
        if (imguiCtx) {
            ImGui::SetCurrentContext(imguiCtx);
        } else {
            return;
        }
    }

    if (!postMatch.active) {
        return;
    }

    static bool loggedOnce = false;
    if (!loggedOnce) {
        LOG("SuiteSpot: RenderOverlayWindow - overlay is active, starting to render");
        loggedOnce = true;
    }

    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration<float>(now - postMatch.start).count();
    if (elapsed >= postMatchDurationSec) {
        postMatch.active = false;
        return;
    }

    // Fade during the last 2 seconds
    float alpha = 1.0f;
    const float fade = 2.0f;
    if (elapsed > postMatchDurationSec - fade) {
        alpha = std::max(0.0f, (postMatchDurationSec - elapsed) / fade);
    }

    // Determine local team index for layout
    int myTeamIndex = -1;
    for (const auto& p : postMatch.players) {
        if (p.isLocal && p.teamIndex >= 0) {
            myTeamIndex = p.teamIndex;
            break;
        }
    }
    if (myTeamIndex < 0 && !postMatch.players.empty()) {
        myTeamIndex = postMatch.players.front().teamIndex;
    }

    ImVec2 display = ImGui::GetIO().DisplaySize;
    // Clamp layout inputs to avoid degenerate windows
    overlayWidth = std::max(400.0f, overlayWidth);
    overlayHeight = std::max(180.0f, overlayHeight);
    rowSpacing = std::max(10.0f, rowSpacing);

    const ImVec2 overlaySize = ImVec2(overlayWidth, overlayHeight);
    ImVec2 pos = ImVec2((display.x - overlaySize.x) * 0.5f + overlayOffsetX, display.y * 0.08f + overlayOffsetY);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(overlaySize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f); // we draw our own background

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoNavFocus;

    if (!ImGui::Begin("SuiteSpot Post Match Overlay", nullptr, flags)) {
        ImGui::End();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImVec2 winEnd = ImVec2(winPos.x + winSize.x, winPos.y + winSize.y);

    ImU32 baseBg = ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.55f * alpha));
    ImU32 headerBg = ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.8f * alpha));
    ImU32 white = ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, alpha));

    dl->AddRectFilled(winPos, winEnd, baseBg, 8.0f);
    dl->AddRectFilled(winPos, ImVec2(winEnd.x, winPos.y + 34.0f), headerBg, 8.0f);
    dl->AddText(ImVec2(winPos.x + 12.0f, winPos.y + 8.0f), white, "POST MATCH");

    // New Rocket League style layout with full customization
    const float panelY = winPos.y + 45.0f;
    const float panelHeight = winSize.y - 60.0f;
    
    // Calculate team player counts for headers
    int blueCount = 0, orangeCount = 0;
    for (const auto& row : postMatch.players) {
        if (row.teamIndex == 0) blueCount++;
        else if (row.teamIndex == 1) orangeCount++;
    }
    
    // Team section layout using customizable variables
    const float blueTeamHeight = teamHeaderHeight + (blueCount * playerRowHeight) + sectionPadding * 2;
    const float orangeTeamStart = panelY + blueTeamHeight + teamSectionSpacing;
    const float orangeTeamHeight = teamHeaderHeight + (orangeCount * playerRowHeight) + sectionPadding * 2;
    
    // Convert HSV colors to RGB for team backgrounds
    auto hsvToRgb = [](float h, float s, float v, float a) -> ImVec4 {
        float c = v * s;
        float x = c * (1 - abs(fmod(h / 60.0f, 2) - 1));
        float m = v - c;
        float r, g, b;
        if (h < 60) { r = c; g = x; b = 0; }
        else if (h < 120) { r = x; g = c; b = 0; }
        else if (h < 180) { r = 0; g = c; b = x; }
        else if (h < 240) { r = 0; g = x; b = c; }
        else if (h < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        return ImVec4(r + m, g + m, b + m, a);
    };
    
    // Blue team section
    ImVec4 blueHeaderColor = hsvToRgb(blueTeamHue, blueTeamSat, blueTeamVal, headerAlpha * alpha);
    ImVec4 blueRowColor = hsvToRgb(blueTeamHue, blueTeamSat * 0.5f, blueTeamVal * 0.4f, backgroundAlpha * alpha);
    ImU32 blueHeaderBg = ImGui::GetColorU32(blueHeaderColor);
    ImU32 blueRowBg = ImGui::GetColorU32(blueRowColor);
    
    dl->AddRectFilled(
        ImVec2(winPos.x + 8.0f, panelY), 
        ImVec2(winEnd.x - 8.0f, panelY + blueTeamHeight), 
        blueRowBg, 6.0f
    );
    dl->AddRectFilled(
        ImVec2(winPos.x + 8.0f, panelY), 
        ImVec2(winEnd.x - 8.0f, panelY + teamHeaderHeight), 
        blueHeaderBg, 6.0f
    );
    
    // Orange team section
    ImVec4 orangeHeaderColor = hsvToRgb(orangeTeamHue, orangeTeamSat, orangeTeamVal, headerAlpha * alpha);
    ImVec4 orangeRowColor = hsvToRgb(orangeTeamHue, orangeTeamSat * 0.5f, orangeTeamVal * 0.4f, backgroundAlpha * alpha);
    ImU32 orangeHeaderBg = ImGui::GetColorU32(orangeHeaderColor);
    ImU32 orangeRowBg = ImGui::GetColorU32(orangeRowColor);
    
    dl->AddRectFilled(
        ImVec2(winPos.x + 8.0f, orangeTeamStart), 
        ImVec2(winEnd.x - 8.0f, orangeTeamStart + orangeTeamHeight), 
        orangeRowBg, 6.0f
    );
    dl->AddRectFilled(
        ImVec2(winPos.x + 8.0f, orangeTeamStart), 
        ImVec2(winEnd.x - 8.0f, orangeTeamStart + teamHeaderHeight), 
        orangeHeaderBg, 6.0f
    );

    // Team headers with customizable content
    ImU32 whiteText = ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, alpha));
    if (showTeamScores) {
        std::string blueHeader = std::to_string(postMatch.myScore) + " BLUE";
        dl->AddText(ImVec2(winPos.x + 20.0f, panelY + 6.0f), whiteText, blueHeader.c_str());
        
        std::string orangeHeader = std::to_string(postMatch.oppScore) + " ORANGE";
        dl->AddText(ImVec2(winPos.x + 20.0f, orangeTeamStart + 6.0f), whiteText, orangeHeader.c_str());
    } else {
        dl->AddText(ImVec2(winPos.x + 20.0f, panelY + 6.0f), whiteText, "BLUE");
        dl->AddText(ImVec2(winPos.x + 20.0f, orangeTeamStart + 6.0f), whiteText, "ORANGE");
    }
    
    // Column headers with customizable positions
    if (showColumnHeaders) {
        ImU32 headerText = ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, alpha));
        const float headerY = panelY + teamHeaderHeight + 4.0f;
        
        dl->AddText(ImVec2(winPos.x + scoreColumnX, headerY), headerText, "SCORE");
        dl->AddText(ImVec2(winPos.x + goalsColumnX, headerY), headerText, "GOALS");
        dl->AddText(ImVec2(winPos.x + assistsColumnX, headerY), headerText, "ASSISTS");
        dl->AddText(ImVec2(winPos.x + savesColumnX, headerY), headerText, "SAVES");
        dl->AddText(ImVec2(winPos.x + shotsColumnX, headerY), headerText, "SHOTS");
        dl->AddText(ImVec2(winPos.x + pingColumnX, headerY), headerText, "PING");
    }

    // Player rows with customizable positioning
    float blueRowY = panelY + teamHeaderHeight + (showColumnHeaders ? 24.0f : 8.0f);
    float orangeRowY = orangeTeamStart + teamHeaderHeight + 8.0f;
    
    for (const auto& row : postMatch.players) {
        float* rowY = (row.teamIndex == 0) ? &blueRowY : &orangeRowY;
        
        // MVP checkmark with customizable styling
        std::string displayName = row.name;
        if (row.isLocal) displayName = "[YOU] " + displayName;
        
        if (row.isMVP) {
            ImU32 mvpColor = showMvpGlow ? 
                ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.2f, alpha)) : 
                ImGui::GetColorU32(ImVec4(0.0f, 0.8f, 0.0f, alpha));
            
            // Scale checkmark based on size setting
            float checkmarkX = winPos.x + nameColumnX - 25.0f;
            if (mvpCheckmarkSize != 1.0f) {
                // Could implement scaled rendering here
            }
            dl->AddText(ImVec2(checkmarkX, *rowY), mvpColor, "✓");
        }
        
        // Player name and stats using customizable column positions
        dl->AddText(ImVec2(winPos.x + nameColumnX, *rowY), whiteText, displayName.c_str());
        dl->AddText(ImVec2(winPos.x + scoreColumnX, *rowY), whiteText, std::to_string(row.score).c_str());
        dl->AddText(ImVec2(winPos.x + goalsColumnX, *rowY), whiteText, std::to_string(row.goals).c_str());
        dl->AddText(ImVec2(winPos.x + assistsColumnX, *rowY), whiteText, std::to_string(row.assists).c_str());
        dl->AddText(ImVec2(winPos.x + savesColumnX, *rowY), whiteText, std::to_string(row.saves).c_str());
        dl->AddText(ImVec2(winPos.x + shotsColumnX, *rowY), whiteText, std::to_string(row.shots).c_str());
        dl->AddText(ImVec2(winPos.x + pingColumnX, *rowY), whiteText, std::to_string(row.ping).c_str());
        
        *rowY += playerRowHeight;
    }

    // Match info at bottom
    std::ostringstream status;
    status << postMatch.playlist;
    if (postMatch.overtime) status << " | OVERTIME";
    ImU32 statusColor = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, alpha));
    dl->AddText(ImVec2(winPos.x + 12.0f, winEnd.y - 25.0f), statusColor, status.str().c_str());

    ImGui::End();
}

int SuiteSpot::GetRandomTrainingIndex() const {
    if (trainingShuffleBag.empty()) {
        return 0;
    }
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, static_cast<int>(trainingShuffleBag.size()) - 1);
    int bagIndex = dist(rng);
    auto it = std::find_if(RLTraining.begin(), RLTraining.end(),
        [&](const TrainingEntry& e) { return e.code == trainingShuffleBag[bagIndex].code; });
    if (it != RLTraining.end()) {
        return static_cast<int>(std::distance(RLTraining.begin(), it));
    }
    return 0;
}

// #detailed comments: LoadTrainingMaps
// Purpose: Read SuiteTraining\SuiteSpotTrainingMaps.txt and populate
// the RLTraining vector. The parser is intentionally permissive to
// preserve backward compatibility with legacy formats (name may include
// trailing "(N)" shot counts) while normalizing into the modern CSV
// form: code,name,Shots:N.
//
// Important parsing invariants:
//  - Lines are split on commas but whitespace is trimmed from parts.
//  - If only two parts exist, ParseTrailingShots attempts to extract a
//    trailing shot count from the name in parentheses and strips it.
//  - Errors or malformed lines are skipped silently: this avoids
//    throwing during startup due to a single malformed line in the file.
//
// DO NOT CHANGE: The sort order is case-insensitive; changing the
// comparator could alter user-visible ordering of maps in the UI.
void SuiteSpot::LoadTrainingMaps() {
    EnsureDataDirectories();
    EnsureReadmeFiles();
    RLTraining.clear();
    auto f = GetTrainingFilePath();
    std::error_code ec;
    if (!std::filesystem::exists(f, ec)) return;
    std::ifstream in(f.string());
    if (!in.is_open()) return;
    std::string line;
    bool sawLegacyFormat = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        // Expected format: code, Name, Shots:#  (shots optional for backward compatibility)
        std::vector<std::string> parts;
        size_t start = 0;
        while (start <= line.size())
        {
            const auto commaPos = line.find(',', start);
            if (commaPos == std::string::npos)
            {
                parts.push_back(Trim(line.substr(start)));
                break;
            }
            parts.push_back(Trim(line.substr(start, commaPos - start)));
            start = commaPos + 1;
        }

        if (parts.size() < 2) {
            continue; // malformed
        }

        std::string code = parts[0];
        std::string name = parts[1];
        int shots = 0;
        // Legacy: shots embedded in name "(#)" when only 2 parts existed.
        if (parts.size() == 2) {
            shots = ParseTrailingShots(name);
            if (shots > 0) {
                sawLegacyFormat = true;
            }
            // strip trailing "(#)" from name if present
            const auto openPos = name.find_last_of('(');
            const auto closePos = name.find_last_of(')');
            if (openPos != std::string::npos && closePos != std::string::npos && closePos > openPos) {
                name = Trim(name.substr(0, openPos));
            }
        }
        if (parts.size() >= 3) {
            std::string shotsPart = parts[2];
            if (StartsWithCaseInsensitive(shotsPart, "shots:")) {
                auto valStr = Trim(shotsPart.substr(6));
                try {
                    shots = std::stoi(valStr);
                } catch (...) {
                    shots = 0;
                }
            }
        }

        if (!code.empty() && !name.empty()) {
            TrainingEntry entry;
            entry.code = code;
            entry.name = name;
            entry.shotCount = shots;
            RLTraining.push_back(entry);
        }
    }

    std::sort(RLTraining.begin(), RLTraining.end(),
        [](const TrainingEntry& lhs, const TrainingEntry& rhs)
        {
            return CaseInsensitiveCompare(lhs.name, rhs.name) < 0;
        });

    if (RLTraining.empty())
    {
        currentTrainingIndex = 0;
    }
    else
    {
        currentTrainingIndex = std::clamp(currentTrainingIndex, 0, static_cast<int>(RLTraining.size() - 1));
    }

    // If we saw legacy entries, rewrite file in new format so future loads are clean.
    if (sawLegacyFormat) {
        SaveTrainingMaps();
    }
}

// #detailed comments: SaveTrainingMaps
// Purpose: Persist RLTraining to SuiteTraining\SuiteSpotTrainingMaps.txt
// using the canonical CSV format (code,name,Shots:N). The function
// creates parent directories as needed and writes a sorted list so the
// file remains deterministic across runs.
//
// DO NOT CHANGE: The output format is intentionally kept stable since
// downstream tooling and user scripts depend on the exact CSV layout.
void SuiteSpot::SaveTrainingMaps() const {
    auto f = GetTrainingFilePath();
    EnsureDataDirectories();
    EnsureReadmeFiles();
    std::ofstream out(f.string(), std::ios::trunc);
    if (!out.is_open()) return;
    auto sorted = RLTraining;
    std::sort(sorted.begin(), sorted.end(),
        [](const TrainingEntry& lhs, const TrainingEntry& rhs)
        {
            return CaseInsensitiveCompare(lhs.name, rhs.name) < 0;
        });
    for (const auto& e : sorted) {
        out << e.code << "," << e.name << ",Shots:" << e.shotCount << "\n";
    }
}

void SuiteSpot::LoadShuffleBag() {
    trainingShuffleBag.clear();
    selectedTrainingIndices.clear();
    auto f = GetShuffleBagPath();
    std::error_code ec;
    if (!std::filesystem::exists(f, ec)) return;
    std::ifstream in(f.string());
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto pos = line.find(',');
        if (pos == std::string::npos) continue;
        std::string code = Trim(line.substr(0, pos));
        std::string name = Trim(line.substr(pos + 1));
        if (!code.empty() && !name.empty()) {
            trainingShuffleBag.push_back({ code, name });
            auto it = std::find_if(RLTraining.begin(), RLTraining.end(),
                [&](const TrainingEntry& e) { return e.code == code; });
            if (it != RLTraining.end()) {
                selectedTrainingIndices.insert(static_cast<int>(std::distance(RLTraining.begin(), it)));
            }
        }
    }
}

void SuiteSpot::SaveShuffleBag() const {
    auto f = GetShuffleBagPath();
    EnsureDataDirectories();
    std::ofstream out(f.string(), std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& e : trainingShuffleBag) {
        out << e.code << "," << e.name << "\n";
    }
}

// Mirror src directory recursively into dst
// #detailed comments: MirrorDirectory
// Purpose: Recursively copy files from `src` to `dst` preserving
// directory structure. This helper is careful to only copy when files
// differ by timestamp/size to avoid unnecessary disk activity.
//
// Note: This routine tolerates filesystem errors by clearing the
// std::error_code after each check; it is defensive and will skip
// entries where errors occur rather than aborting.
//
// BE CONSERVATIVE: Copy behavior must remain as-is; changing the
// comparison strategy or copy options can alter update semantics for
// replicated resource directories.
void SuiteSpot::MirrorDirectory(const std::filesystem::path& src, const std::filesystem::path& dst) const {
    std::error_code ec;
    if (!std::filesystem::exists(src, ec) || !std::filesystem::is_directory(src, ec)) return;
    std::filesystem::create_directories(dst, ec);
    for (auto const& entry : std::filesystem::recursive_directory_iterator(src, ec)) {
        if (ec) { ec.clear(); continue; }
        auto rel = std::filesystem::relative(entry.path(), src, ec);
        if (ec) { ec.clear(); continue; }
        auto target = dst / rel;
        if (entry.is_directory()) {
            std::filesystem::create_directories(target, ec);
        } else if (entry.is_regular_file()) {
            // copy if missing or different timestamp/size
            bool copyNeeded = true;
            if (std::filesystem::exists(target, ec)) {
                auto src_time = std::filesystem::last_write_time(entry.path(), ec);
                auto dst_time = std::filesystem::last_write_time(target, ec);
                auto src_size = std::filesystem::file_size(entry.path(), ec);
                auto dst_size = std::filesystem::file_size(target, ec);
                copyNeeded = (src_time != dst_time) || (src_size != dst_size);
            }
            if (copyNeeded) {
                std::filesystem::create_directories(target.parent_path(), ec);
                std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, ec);
            }
        }
    }
}

// #detailed comments: EnsureReadmeFiles
// Purpose: Create a small README.txt in the SuiteTraining folder that
// explains the expected format of SuiteSpotTrainingMaps.txt. This file
// is informational only and its presence is used as a heuristic that
// the directory has been created/initialized.
//
// DO NOT DELETE: Many users rely on this README to understand file
// layout; recreating it if missing is a non-destructive operation.
void SuiteSpot::EnsureReadmeFiles() const {
    // SuiteTraining README
    auto tr = GetSuiteTrainingDir() / "README.txt";
    if (!std::filesystem::exists(tr)) {
        std::ofstream o(tr.string(), std::ios::trunc);
        o << "SuiteTraining\\SuiteSpotTrainingMaps.txt\n"
             "CSV format:\n"
             "    <training_code>,<display_name>\n"
             "One entry per line. This file is read on game start and updated when you add a map in SuiteSpot.\n";
    }
}

std::filesystem::path SuiteSpot::GetWorkshopLoaderConfigPath() const
{
    return GetDataRoot() / "WorkshopMapLoader" / "workshopmaploader.cfg";
}

std::filesystem::path SuiteSpot::ResolveConfiguredWorkshopRoot() const
{
    const auto cfg = GetWorkshopLoaderConfigPath();
    std::ifstream in(cfg);
    if (!in.is_open())
    {
        return {};
    }

    std::string line;
    while (std::getline(in, line))
    {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#')
        {
            continue;
        }

        const auto keyPos = trimmed.find("MapsFolderPath");
        if (keyPos == std::string::npos)
        {
            continue;
        }

        const auto eqPos = trimmed.find('=', keyPos);
        if (eqPos == std::string::npos)
        {
            continue;
        }

        std::string value = trimmed.substr(eqPos + 1);
        value = StripQuotes(Trim(value));
        value = ExpandEnvAndHome(value);

        if (value.empty())
        {
            continue;
        }

        std::error_code ec;
        std::filesystem::path candidate(value);
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec))
        {
            return candidate;
        }

        LOG("SuiteSpot: Configured workshop path not found: " + value);
    }

    return {};
}


void SuiteSpot::DiscoverWorkshopInDir(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) return;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!entry.is_directory()) continue;

        // Look for any .upk file in the directory
        std::string foundMapFile;
        for (const auto& file : std::filesystem::directory_iterator(entry.path(), ec)) {
            if (ec) { ec.clear(); continue; }
            if (!file.is_regular_file()) continue;
            
            const auto& path = file.path();
            if (path.extension().string() == ".upk") {
                foundMapFile = path.string();
                break;
            }
        }
        
        if (!foundMapFile.empty()) {
            RLWorkshop.push_back({ foundMapFile, entry.path().filename().string() });
        }
    }
}


void SuiteSpot::SaveWorkshopMaps() const {
    // No-op
}

// ===== PREJUMP SCRAPER INTEGRATION =====

std::filesystem::path SuiteSpot::GetPrejumpPacksPath() const {
    return GetSuiteTrainingDir() / "prejump_packs.json";
}

bool SuiteSpot::IsPrejumpCacheStale() const {
    auto cachePath = GetPrejumpPacksPath();
    if (!std::filesystem::exists(cachePath)) {
        return true;  // File doesn't exist, so it's "stale"
    }
    
    try {
        auto lastWriteTime = std::filesystem::last_write_time(cachePath);
        auto now = std::filesystem::file_time_type::clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - lastWriteTime);
        
        // Cache is stale after 7 days (168 hours)
        return age.count() > 168;
    } catch (...) {
        return true;  // If we can't check, assume stale
    }
}

std::string SuiteSpot::FormatLastUpdatedTime() const {
    auto cachePath = GetPrejumpPacksPath();
    if (!std::filesystem::exists(cachePath)) {
        return "Never";
    }
    
    try {
        auto lastWriteTime = std::filesystem::last_write_time(cachePath);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            lastWriteTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        auto tt = std::chrono::system_clock::to_time_t(sctp);
        
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M UTC");
        return oss.str();
    } catch (...) {
        return "Unknown";
    }
}

void SuiteSpot::LoadPrejumpPacksFromFile(const std::filesystem::path& filePath) {
    if (!std::filesystem::exists(filePath)) {
        LOG("SuiteSpot: Prejump packs file not found: " + filePath.string());
        return;
    }
    
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG("SuiteSpot: Failed to open Prejump packs file");
            return;
        }
        
        // Parse JSON using nlohmann/json
        nlohmann::json jsonData;
        file >> jsonData;
        file.close();
        
        // Clear existing prejump packs
        prejumpPacks.clear();
        
        // Validate JSON structure
        if (!jsonData.contains("packs") || !jsonData["packs"].is_array()) {
            LOG("SuiteSpot: Invalid Prejump packs file format - missing 'packs' array");
            return;
        }
        
        // Parse each pack
        for (const auto& pack : jsonData["packs"]) {
            TrainingEntry entry;
            
            // Required fields
            if (pack.contains("code") && pack["code"].is_string()) {
                entry.code = pack["code"].get<std::string>();
            }
            if (pack.contains("name") && pack["name"].is_string()) {
                entry.name = pack["name"].get<std::string>();
            }
            
            // Skip if missing required fields
            if (entry.code.empty() || entry.name.empty()) {
                continue;
            }
            
            // Optional Prejump metadata
            if (pack.contains("creator") && pack["creator"].is_string()) {
                entry.creator = pack["creator"].get<std::string>();
            }
            if (pack.contains("creatorSlug") && pack["creatorSlug"].is_string()) {
                entry.creatorSlug = pack["creatorSlug"].get<std::string>();
            }
            if (pack.contains("difficulty") && pack["difficulty"].is_string()) {
                entry.difficulty = pack["difficulty"].get<std::string>();
            }
            if (pack.contains("shotCount") && pack["shotCount"].is_number()) {
                entry.shotCount = pack["shotCount"].get<int>();
            }
            if (pack.contains("staffComments") && pack["staffComments"].is_string()) {
                entry.staffComments = pack["staffComments"].get<std::string>();
            }
            if (pack.contains("notes") && pack["notes"].is_string()) {
                entry.notes = pack["notes"].get<std::string>();
            }
            if (pack.contains("videoUrl") && pack["videoUrl"].is_string()) {
                entry.videoUrl = pack["videoUrl"].get<std::string>();
            }
            if (pack.contains("likes") && pack["likes"].is_number()) {
                entry.likes = pack["likes"].get<int>();
            }
            if (pack.contains("plays") && pack["plays"].is_number()) {
                entry.plays = pack["plays"].get<int>();
            }
            if (pack.contains("status") && pack["status"].is_number()) {
                entry.status = pack["status"].get<int>();
            }
            
            // Parse tags array
            if (pack.contains("tags") && pack["tags"].is_array()) {
                for (const auto& tag : pack["tags"]) {
                    if (tag.is_string()) {
                        entry.tags.push_back(tag.get<std::string>());
                    }
                }
            }
            
            prejumpPacks.push_back(entry);
        }
        
        prejumpPackCount = static_cast<int>(prejumpPacks.size());
        LOG("SuiteSpot: Loaded " + std::to_string(prejumpPackCount) + " prejump packs from file");
        
    } catch (const std::exception& e) {
        LOG("SuiteSpot: Error loading Prejump packs: " + std::string(e.what()));
        prejumpPacks.clear();
        prejumpPackCount = 0;
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
    if (prejumpScrapingInProgress) {
        LOG("SuiteSpot: Prejump scraping already in progress");
        return;
    }
    
    // Get paths
    auto scraperScript = std::filesystem::path(R"(C:\Users\bmile\Source\Repos\SuiteSpot\scrape_prejump.ps1)");
    auto outputPath = GetPrejumpPacksPath();
    
    // Verify scraper script exists
    if (!std::filesystem::exists(scraperScript)) {
        LOG("SuiteSpot: Prejump scraper script not found at: " + scraperScript.string());
        return;
    }
    
    // Build PowerShell command
    std::string cmd = "powershell -NoProfile -ExecutionPolicy Bypass -File \"" + scraperScript.string() 
                    + "\" -OutputPath \"" + outputPath.string() + "\" -QuietMode:$true";
    
    prejumpScrapingInProgress = true;
    LOG("SuiteSpot: Started Prejump scraper...");
    
    // Execute in background (detached process)
    // On Windows, we use system() which creates a detached process by default
    gameWrapper->SetTimeout([this, cmd, outputPath](GameWrapper* gw) {
        int result = system(cmd.c_str());
        
        if (result == 0) {
            LOG("SuiteSpot: Prejump scraper completed successfully");
            // Load the generated file
            LoadPrejumpPacksFromFile(outputPath);
            prejumpLastUpdated = FormatLastUpdatedTime();
        } else {
            LOG("SuiteSpot: Prejump scraper failed with exit code " + std::to_string(result));
        }
        
        prejumpScrapingInProgress = false;
    }, 0.1f);  // Execute next frame to avoid blocking
}


using namespace std;
using namespace std::filesystem;

void SuiteSpot::LoadWorkshopMaps()
{
    RLWorkshop.clear();

    std::vector<std::filesystem::path> roots;
    if (const auto configured = ResolveConfiguredWorkshopRoot(); !configured.empty())
    {
        roots.push_back(configured);
    }

    // Common install fallbacks (Epic + Steam).
    roots.emplace_back(R"(C:\Program Files\Epic Games\rocketleague\TAGame\CookedPCConsole\mods)");
    roots.emplace_back(R"(C:\Program Files (x86)\Steam\steamapps\common\rocketleague\TAGame\CookedPCConsole\mods)");

    for (const auto& root : roots)
    {
        if (!root.empty())
        {
            DiscoverWorkshopInDir(root);
        }
    }

    std::unordered_set<std::string> seen;
    std::vector<WorkshopEntry> unique;
    unique.reserve(RLWorkshop.size());
    for (const auto& entry : RLWorkshop)
    {
        if (seen.insert(entry.filePath).second)
        {
            unique.push_back(entry);
        }
    }
    RLWorkshop.swap(unique);

    std::sort(RLWorkshop.begin(), RLWorkshop.end(),
        [](const WorkshopEntry& lhs, const WorkshopEntry& rhs)
        {
            const int cmp = CaseInsensitiveCompare(lhs.name, rhs.name);
            if (cmp == 0)
            {
                return lhs.filePath < rhs.filePath;
            }
            return cmp < 0;
        });

    if (RLWorkshop.empty())
    {
        currentWorkshopIndex = 0;
    }
    else
    {
        currentWorkshopIndex = std::clamp(currentWorkshopIndex, 0, static_cast<int>(RLWorkshop.size() - 1));
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
    if (!enabled) return;

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
                LOG("SuiteSpot: Post-match overlay activated - {} vs {}, Score: {}-{}",
                    postMatch.myTeamName, postMatch.oppTeamName, postMatch.myScore, postMatch.oppScore);
                // Overlay is now independent from settings window - no menu toggle needed
            }
        }
    } catch (const std::exception& e) {
        LOG("SuiteSpot: Failed to capture post-match overlay data: {}", e.what());
    }

    auto safeExecute = [&](int delaySec, const std::string& cmd) {
        if (delaySec <= 0) {
            cvarManager->executeCommand(cmd);
        } else {
            gameWrapper->SetTimeout([this, cmd](GameWrapper* gw) { cvarManager->executeCommand(cmd); }, static_cast<float>(delaySec));
        }
    };

    int mapLoadDelay = 0;

    // Dispatch based on mapType
    if (mapType == 0) { // Freeplay
        if (currentIndex < 0 || currentIndex >= (int)RLMaps.size()) {
            LOG("SuiteSpot: Freeplay index out of range; skipping load.");
        } else {
            safeExecute(delayFreeplaySec, "load_freeplay " + RLMaps[currentIndex].code);
            mapLoadDelay = delayFreeplaySec;
            LOG("SuiteSpot: Loading freeplay map: " + RLMaps[currentIndex].name);
        }
    } else if (mapType == 1) { // Training
        if (RLTraining.empty()) {
            LOG("SuiteSpot: No training maps configured.");
        } else {
            int indexToLoad = trainingShuffleEnabled ? GetRandomTrainingIndex() : currentTrainingIndex;
            indexToLoad = std::clamp(indexToLoad, 0, (int)RLTraining.size()-1);
            safeExecute(delayTrainingSec, "load_training " + RLTraining[indexToLoad].code);
            mapLoadDelay = delayTrainingSec;
            LOG("SuiteSpot: Loading training map: " + RLTraining[indexToLoad].name);
        }
    } else if (mapType == 2) { // Workshop
        if (RLWorkshop.empty()) {
            LOG("SuiteSpot: No workshop maps configured.");
        } else {
            currentWorkshopIndex = std::clamp(currentWorkshopIndex, 0, (int)RLWorkshop.size()-1);
            safeExecute(delayWorkshopSec, "load_workshop \"" + RLWorkshop[currentWorkshopIndex].filePath + "\"");
            mapLoadDelay = delayWorkshopSec;
            LOG("SuiteSpot: Loading workshop map: " + RLWorkshop[currentWorkshopIndex].name);
        }
    }

    // Auto-queue runs independently of map load
    if (autoQueue) {
        safeExecute(delayQueueSec, "queue");
        LOG("SuiteSpot: Auto-Queuing scheduled with delay: " + std::to_string(delayQueueSec) + "s.");
    }
}

void SuiteSpot::onLoad() {
    _globalCvarManager = cvarManager;
    LOG("SuiteSpot loaded");
    EnsureDataDirectories();
    EnsureReadmeFiles();
    LoadTrainingMaps();
    LoadWorkshopMaps();
    LoadShuffleBag();
    
    // Initialize LoadoutManager
    loadoutManager = std::make_unique<LoadoutManager>(gameWrapper);
    LOG("SuiteSpot: LoadoutManager initialized");
    
    // Check Prejump cache and load if available
    prejumpLastUpdated = FormatLastUpdatedTime();
    if (!std::filesystem::exists(GetPrejumpPacksPath())) {
        LOG("SuiteSpot: No Prejump cache found. Schedule scraping on next opportunity.");
        // Will be scraped on first Settings render or user request
    } else {
        // Load existing Prejump cache
        LoadPrejumpPacksFromFile(GetPrejumpPacksPath());
        LOG("SuiteSpot: Prejump cache loaded");
    }
    
    LoadHooks();

    // Enable/Disable plugin
    cvarManager->registerCvar("suitespot_enabled", "0", "Enable SuiteSpot", true, true, 0, true, 1)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            enabled = cvar.getBoolValue();
        });

    // Map type: 0=Freeplay, 1=Training, 2=Workshop
    cvarManager->registerCvar("suitespot_map_type", "0", "Map type: 0=Freeplay, 1=Training, 2=Workshop", true, true, 0, true, 2)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            mapType = cvar.getIntValue();
        });

    // Auto-queue enabled
    cvarManager->registerCvar("suitespot_auto_queue", "0", "Enable auto-queuing after map load", true, true, 0, true, 1)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            autoQueue = cvar.getBoolValue();
        });

    // Training shuffle enabled
    cvarManager->registerCvar("suitespot_training_shuffle", "0", "Enable shuffle for training maps", true, true, 0, true, 1)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            trainingShuffleEnabled = cvar.getBoolValue();
        });

    // Training shuffle bag size (legacy; now reflects selected count)
    cvarManager->registerCvar("suitespot_training_bag_size", "0", "Shuffle bag size (legacy, reflects selected count)", true, true, 0, true, 1000)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            trainingBagSize = cvar.getIntValue();
        });

    // Delay settings (in seconds)
    cvarManager->registerCvar("suitespot_delay_queue_sec", "0", "Delay before queuing (seconds)", true, true, 0, true, 300)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            delayQueueSec = std::max(0, cvar.getIntValue());
        });

    cvarManager->registerCvar("suitespot_delay_freeplay_sec", "0", "Delay before loading freeplay map (seconds)", true, true, 0, true, 300)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            delayFreeplaySec = std::max(0, cvar.getIntValue());
        });

    cvarManager->registerCvar("suitespot_delay_training_sec", "0", "Delay before loading training map (seconds)", true, true, 0, true, 300)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            delayTrainingSec = std::max(0, cvar.getIntValue());
        });

    cvarManager->registerCvar("suitespot_delay_workshop_sec", "0", "Delay before loading workshop map (seconds)", true, true, 0, true, 300)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            delayWorkshopSec = std::max(0, cvar.getIntValue());
        });

    // Map selection indices
    cvarManager->registerCvar("suitespot_current_freeplay_index", "0", "Currently selected freeplay map index", true, true, 0, true, 1000)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            currentIndex = std::max(0, cvar.getIntValue());
        });

    cvarManager->registerCvar("suitespot_current_training_index", "0", "Currently selected training map index", true, true, 0, true, 1000)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            currentTrainingIndex = std::max(0, cvar.getIntValue());
        });

    cvarManager->registerCvar("suitespot_current_workshop_index", "0", "Currently selected workshop map index", true, true, 0, true, 1000)
        .addOnValueChanged([this](string oldValue, CVarWrapper cvar) {
            currentWorkshopIndex = std::max(0, cvar.getIntValue());
        });

    // Store training maps string for persistence compatibility
    cvarManager->registerCvar("ss_training_maps", "", "Stored training maps", true, false, 0, false, 0);

    // Sync CVars with loaded settings
    cvarManager->getCvar("suitespot_enabled").setValue(enabled ? 1 : 0);
    cvarManager->getCvar("suitespot_map_type").setValue(mapType);
    cvarManager->getCvar("suitespot_auto_queue").setValue(autoQueue ? 1 : 0);
    cvarManager->getCvar("suitespot_training_shuffle").setValue(trainingShuffleEnabled ? 1 : 0);
    trainingBagSize = static_cast<int>(trainingShuffleBag.size());
    cvarManager->getCvar("suitespot_training_bag_size").setValue(trainingBagSize);
    cvarManager->getCvar("suitespot_delay_queue_sec").setValue(delayQueueSec);
    cvarManager->getCvar("suitespot_delay_freeplay_sec").setValue(delayFreeplaySec);
    cvarManager->getCvar("suitespot_delay_training_sec").setValue(delayTrainingSec);
    cvarManager->getCvar("suitespot_delay_workshop_sec").setValue(delayWorkshopSec);
    cvarManager->getCvar("suitespot_current_freeplay_index").setValue(currentIndex);
    cvarManager->getCvar("suitespot_current_training_index").setValue(currentTrainingIndex);
    cvarManager->getCvar("suitespot_current_workshop_index").setValue(currentWorkshopIndex);
    
    LOG("SuiteSpot: Plugin initialization complete");
}

void SuiteSpot::RenderPostMatchOverlay() {
    if (!ImGui::GetCurrentContext()) {
        if (imguiCtx) {
            ImGui::SetCurrentContext(imguiCtx);
        } else {
            return;
        }
    }

    // Check if overlay should still be shown
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration<float>(now - postMatch.start).count();
    
    // Auto-hide after duration
    if (postMatch.active && elapsed >= postMatchDurationSec) {
        postMatch.active = false;
        return;
    }

    // Calculate fade
    float alpha = overlayAlpha;
    if (enableFadeEffects) {
        if (elapsed < fadeInDuration) {
            alpha *= (elapsed / fadeInDuration);
        } else if (elapsed > postMatchDurationSec - fadeOutDuration) {
            alpha *= std::max(0.0f, (postMatchDurationSec - elapsed) / fadeOutDuration);
        }
    }

    ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 overlaySize = ImVec2(std::max(400.0f, overlayWidth), std::max(180.0f, overlayHeight));
    ImVec2 pos = ImVec2((display.x - overlaySize.x) * 0.5f + overlayOffsetX, display.y * 0.08f + overlayOffsetY);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(overlaySize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoNavFocus;

    bool windowOpen = true;
    if (!ImGui::Begin("SuiteSpot Post-Match Overlay", &windowOpen, flags)) {
        ImGui::End();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImVec2 winEnd = ImVec2(winPos.x + winSize.x, winPos.y + winSize.y);

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
        if (teamIdx == 0) {
            float h = blueTeamHue / 360.0f;
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(h, blueTeamSat, blueTeamVal, r, g, b);
            teamColor = ImGui::GetColorU32(ImVec4(r, g, b, alpha));
        } else {
            float h = orangeTeamHue / 360.0f;
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(h, orangeTeamSat, orangeTeamVal, r, g, b);
            teamColor = ImGui::GetColorU32(ImVec4(r, g, b, alpha));
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

    ImGui::End();
}

void SuiteSpot::onUnload() {
    LOG("SuiteSpot unloaded");
}
