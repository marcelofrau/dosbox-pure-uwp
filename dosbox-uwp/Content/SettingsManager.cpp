#include "pch.h"
#include "SettingsManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>

std::string SettingsManager::s_settingsPath;
ThemeColors SettingsManager::s_theme;
std::map<std::string, std::string> SettingsManager::s_coreOptions;
bool SettingsManager::s_loaded = false;
bool SettingsManager::s_dirty = false;
uint64_t SettingsManager::s_lastSaveTime = 0;
std::vector<SettingsManager::HistoryEntry> SettingsManager::s_history;
std::string SettingsManager::s_historyPath;

static const char* SETTINGS_FILENAME = "dosbox-pure-settings.json";

uint32_t SettingsManager::ParseHexColor(const char* str, uint32_t defaultVal)
{
    if (!str || !str[0]) return defaultVal;
    while (*str == ' ') ++str;
    if (str[0] == '#' && strlen(str) >= 7)
    {
        ++str;
        uint32_t r = 0, g = 0, b = 0, a = 0xFF;
        if (strlen(str) >= 8)
            a = (uint32_t)strtoul(std::string(str, 2).c_str(), nullptr, 16);
        if (strlen(str) >= 6)
        {
            r = (uint32_t)strtoul(std::string(str + (strlen(str) >= 8 ? 2 : 0), 2).c_str(), nullptr, 16);
            g = (uint32_t)strtoul(std::string(str + (strlen(str) >= 8 ? 4 : 2), 2).c_str(), nullptr, 16);
            b = (uint32_t)strtoul(std::string(str + (strlen(str) >= 8 ? 6 : 4), 2).c_str(), nullptr, 16);
        }
        return (a << 24) | (r << 16) | (g << 8) | b;
    }
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
        return (uint32_t)strtoul(str, nullptr, 16);
    return defaultVal;
}

std::string SettingsManager::StripJsonComments(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    enum { CODE, SLASH, LINE_COMMENT, BLOCK_COMMENT, BLOCK_STAR, IN_STRING, STRING_BS } state = CODE;
    for (size_t i = 0; i < raw.size(); ++i)
    {
        char c = raw[i];
        switch (state)
        {
        case CODE:
            if (c == '/') { state = SLASH; }
            else if (c == '"') { out += c; state = IN_STRING; }
            else { out += c; }
            break;
        case SLASH:
            if (c == '/') { state = LINE_COMMENT; }
            else if (c == '*') { state = BLOCK_COMMENT; }
            else { out += '/'; out += c; state = CODE; }
            break;
        case LINE_COMMENT:
            if (c == '\n') { out += '\n'; state = CODE; }
            break;
        case BLOCK_COMMENT:
            if (c == '*') { state = BLOCK_STAR; }
            else if (c == '\n') { out += '\n'; }
            break;
        case BLOCK_STAR:
            if (c == '/') { state = CODE; }
            else if (c == '*') { state = BLOCK_STAR; }
            else { state = BLOCK_COMMENT; }
            break;
        case IN_STRING:
            if (c == '\\') { out += c; state = STRING_BS; }
            else if (c == '"') { out += c; state = CODE; }
            else { out += c; }
            break;
        case STRING_BS:
            out += c; state = IN_STRING;
            break;
        }
    }
    return out;
}

void SettingsManager::LoadDefaults()
{
    s_theme = ThemeColors();
    s_coreOptions.clear();
    s_coreOptions["dosbox_pure_menu_transparency"] = "70";
    // Frontend-only options
    s_coreOptions["frontend_vsync"] = "On";
    s_coreOptions["frontend_scaler"] = "Bilinear";
}

void SettingsManager::Initialize(const std::string& settingsPath)
{
    s_settingsPath = settingsPath;
    s_loaded = false;
    LoadDefaults();

    std::ifstream file(settingsPath);
    if (!file.is_open())
    {
        Save();
        s_loaded = true;
        LoadHistory();
        return;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    std::string raw = ss.str();
    std::string cleaned = StripJsonComments(raw);

    try
    {
        auto j = nlohmann::json::parse(cleaned);

        if (j.contains("theme") && j["theme"].is_object())
        {
            auto& t = j["theme"];
            auto gc = [&](const char* key, uint32_t& dst) {
                if (t.contains(key) && t[key].is_string())
                    dst = ParseHexColor(t[key].get<std::string>().c_str(), dst);
            };
            auto gf = [&](const char* key, float& dst) {
                if (t.contains(key) && t[key].is_string())
                    dst = (float)atof(t[key].get<std::string>().c_str());
            };

            gc("bg_panel",       s_theme.bg_panel);
            gc("bg_fullscreen",  s_theme.bg_fullscreen);
            gc("frame",          s_theme.frame);
            gc("title_bg",       s_theme.title_bg);
            gc("text_title",     s_theme.text_title);
            gc("text_normal",    s_theme.text_normal);
            gc("text_value",     s_theme.text_value);
            gc("text_disabled",  s_theme.text_disabled);
            gc("text_bios",      s_theme.text_bios);
            gc("selection_bg",   s_theme.selection_bg);
            gc("selection_text", s_theme.selection_text);
            gc("file_text",      s_theme.file_text);
            gf("overlay_alpha",  s_theme.overlay_alpha);
            gc("col_warn",       s_theme.col_warn);
            gc("col_dim",        s_theme.col_dim);
            gc("col_white",      s_theme.col_white);
            gc("bg_btn_off",     s_theme.bg_btn_off);
            gc("bg_btn_on",      s_theme.bg_btn_on);
            gc("bg_btn_hover",   s_theme.bg_btn_hover);
            gc("col_btn_text",   s_theme.col_btn_text);
            gc("bg_key",         s_theme.bg_key);
            gc("bg_key_hover",   s_theme.bg_key_hover);
            gc("bg_key_press",   s_theme.bg_key_press);
            gc("bg_key_held",    s_theme.bg_key_held);
            gc("bg_key_outline", s_theme.bg_key_outline);
            gc("col_key_text",   s_theme.col_key_text);
        }

        if (j.contains("core_options") && j["core_options"].is_object())
        {
            for (auto it = j["core_options"].begin(); it != j["core_options"].end(); ++it)
            {
                const std::string& k = it.key();
                const nlohmann::json& v = it.value();
                if (v.is_string())
                    s_coreOptions[k] = v.get<std::string>();
                else if (v.is_number_integer())
                    s_coreOptions[k] = std::to_string(v.get<int>());
                else if (v.is_number_float())
                    s_coreOptions[k] = std::to_string(v.get<double>());
            }
        }
    }
    catch (const std::exception&)
    {
        LoadDefaults();
    }

    s_loaded = true;
    LoadHistory();
}

std::string SettingsManager::SerializeJson()
{
    nlohmann::json j;

    auto& t = s_theme;
    j["theme"]["bg_panel"]       = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_panel & 0xFFFFFF); return b; }();
    j["theme"]["bg_fullscreen"]  = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_fullscreen & 0xFFFFFF); return b; }();
    j["theme"]["frame"]          = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.frame & 0xFFFFFF); return b; }();
    j["theme"]["title_bg"]       = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.title_bg & 0xFFFFFF); return b; }();
    j["theme"]["text_title"]     = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.text_title & 0xFFFFFF); return b; }();
    j["theme"]["text_normal"]    = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.text_normal & 0xFFFFFF); return b; }();
    j["theme"]["text_value"]     = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.text_value & 0xFFFFFF); return b; }();
    j["theme"]["text_disabled"]  = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.text_disabled & 0xFFFFFF); return b; }();
    j["theme"]["text_bios"]      = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.text_bios & 0xFFFFFF); return b; }();
    j["theme"]["selection_bg"]   = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.selection_bg & 0xFFFFFF); return b; }();
    j["theme"]["selection_text"] = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.selection_text & 0xFFFFFF); return b; }();
    j["theme"]["file_text"]      = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.file_text & 0xFFFFFF); return b; }();
    j["theme"]["overlay_alpha"]  = std::to_string(t.overlay_alpha);
    j["theme"]["col_warn"]       = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.col_warn & 0xFFFFFF); return b; }();
    j["theme"]["col_dim"]        = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.col_dim & 0xFFFFFF); return b; }();
    j["theme"]["col_white"]      = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.col_white & 0xFFFFFF); return b; }();
    j["theme"]["bg_btn_off"]     = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_btn_off & 0xFFFFFF); return b; }();
    j["theme"]["bg_btn_on"]      = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_btn_on & 0xFFFFFF); return b; }();
    j["theme"]["bg_btn_hover"]   = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_btn_hover & 0xFFFFFF); return b; }();
    j["theme"]["col_btn_text"]   = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.col_btn_text & 0xFFFFFF); return b; }();
    j["theme"]["bg_key"]         = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_key & 0xFFFFFF); return b; }();
    j["theme"]["bg_key_hover"]   = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_key_hover & 0xFFFFFF); return b; }();
    j["theme"]["bg_key_press"]   = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_key_press & 0xFFFFFF); return b; }();
    j["theme"]["bg_key_held"]    = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_key_held & 0xFFFFFF); return b; }();
    j["theme"]["bg_key_outline"] = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.bg_key_outline & 0xFFFFFF); return b; }();
    j["theme"]["col_key_text"]   = [&]() -> std::string { char b[16]; sprintf_s(b, "#%06x", t.col_key_text & 0xFFFFFF); return b; }();

    j["core_options"] = nlohmann::json::object();
    for (std::map<std::string, std::string>::iterator it = s_coreOptions.begin(); it != s_coreOptions.end(); ++it)
        j["core_options"][it->first] = it->second;

    j["shaders"] = nlohmann::json::object();
    j["filters"] = nlohmann::json::object();

    return j.dump(4);
}

void SettingsManager::Save()
{
    if (s_settingsPath.empty()) return;
    std::string json = SerializeJson();
    std::ofstream file(s_settingsPath, std::ios::trunc);
    if (file.is_open())
    {
        file.write(json.c_str(), (std::streamsize)json.size());
        file.close();
    }
    s_dirty = false;
}

const ThemeColors& SettingsManager::GetTheme() { return s_theme; }

std::string SettingsManager::GetOption(const char* key, const char* defaultVal)
{
    auto it = s_coreOptions.find(key);
    if (it != s_coreOptions.end()) return it->second;
    return defaultVal ? defaultVal : "";
}

void SettingsManager::SetOption(const char* key, const char* value)
{
    if (key && value)
    {
        s_coreOptions[key] = value;
        s_dirty = true;
    }
}

bool SettingsManager::IsLoaded() { return s_loaded; }

void SettingsManager::ResetToDefaults()
{
    LoadDefaults();
    s_dirty = true;
    Save();
}

static const char* GetDefaultForOption(const char* key)
{
    if (strcmp(key, "frontend_vsync") == 0) return "On";
    if (strcmp(key, "frontend_scaler") == 0) return "Bilinear";
    if (strcmp(key, "dosbox_pure_menu_transparency") == 0) return "70";
    if (strcmp(key, "dosbox_pure_aspect_correction") == 0) return "Off";
    if (strcmp(key, "dosbox_pure_machine") == 0) return "SVGA";
    if (strcmp(key, "dosbox_pure_svgamem") == 0) return "2 (2MB)";
    if (strcmp(key, "dosbox_pure_overscan") == 0) return "0";
    if (strcmp(key, "dosbox_pure_audiorate") == 0) return "48000";
    if (strcmp(key, "dosbox_pure_sblaster_type") == 0) return "SB16";
    if (strcmp(key, "dosbox_pure_volume_sb") == 0) return "100%";
    if (strcmp(key, "dosbox_pure_volume_midi") == 0) return "100%";
    if (strcmp(key, "dosbox_pure_volume_adlib") == 0) return "100%";
    if (strcmp(key, "dosbox_pure_volume_speaker") == 0) return "100%";
    return nullptr;
}

void SettingsManager::ResetSectionDefaults(const std::vector<std::string>& keys)
{
    for (auto& key : keys)
    {
        const char* def = GetDefaultForOption(key.c_str());
        if (def)
            s_coreOptions[key] = def;
        else
            s_coreOptions.erase(key);
    }
    s_dirty = true;
    Save();
}

void SettingsManager::ApplyThemeToPUREMENU()
{
    DBPS_SetMenuColorsFromTheme(s_theme);
}

static const char* HISTORY_FILENAME = "dosbox-pure-history.json";

void SettingsManager::LoadHistory()
{
    s_history.clear();
    std::string historyPath = s_settingsPath;
    // Replace settings filename with history filename
    auto lastSlash = historyPath.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        historyPath = historyPath.substr(0, lastSlash + 1) + HISTORY_FILENAME;
    else
        historyPath = HISTORY_FILENAME;

    std::ifstream file(historyPath);
    if (!file.is_open()) return;

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    std::string raw = ss.str();
    std::string cleaned = StripJsonComments(raw);

    try
    {
        auto j = nlohmann::json::parse(cleaned);
        if (j.contains("history") && j["history"].is_array())
        {
            for (auto& entry : j["history"])
            {
                HistoryEntry he;
                if (entry.contains("filename") && entry["filename"].is_string())
                    he.filename = entry["filename"].get<std::string>();
                if (entry.contains("fullPath") && entry["fullPath"].is_string())
                    he.fullPath = entry["fullPath"].get<std::string>();
                if (entry.contains("timestamp") && entry["timestamp"].is_number_unsigned())
                    he.timestamp = entry["timestamp"].get<uint64_t>();
                if (!he.filename.empty() && !he.fullPath.empty())
                    s_history.push_back(he);
            }
        }
    }
    catch (...) {}
}

void SettingsManager::SaveHistory()
{
    std::string historyPath = s_settingsPath;
    auto lastSlash = historyPath.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        historyPath = historyPath.substr(0, lastSlash + 1) + HISTORY_FILENAME;
    else
        historyPath = HISTORY_FILENAME;

    nlohmann::json j;
    j["history"] = nlohmann::json::array();
    for (auto& he : s_history)
    {
        nlohmann::json entry;
        entry["filename"] = he.filename;
        entry["fullPath"] = he.fullPath;
        entry["timestamp"] = he.timestamp;
        j["history"].push_back(entry);
    }

    std::string json = j.dump(4);
    std::ofstream file(historyPath, std::ios::trunc);
    if (file.is_open())
    {
        file.write(json.c_str(), (std::streamsize)json.size());
        file.close();
    }
}

void SettingsManager::AddToHistory(const std::string& filename, const std::string& fullPath)
{
    // Remove existing entry with same path
    s_history.erase(
        std::remove_if(s_history.begin(), s_history.end(),
            [&fullPath](const HistoryEntry& e) { return e.fullPath == fullPath; }),
        s_history.end());

    // Add to front
    HistoryEntry he;
    he.filename = filename;
    he.fullPath = fullPath;
    he.timestamp = (uint64_t)time(nullptr);
    s_history.insert(s_history.begin(), he);

    // Trim to max
    while ((int)s_history.size() > MAX_HISTORY)
        s_history.pop_back();

    SaveHistory();
}

const std::vector<SettingsManager::HistoryEntry>& SettingsManager::GetHistory()
{
    return s_history;
}

void SettingsManager::ClearHistory()
{
    s_history.clear();
    SaveHistory();
}
