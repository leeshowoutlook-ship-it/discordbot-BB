#pragma once
#include "types.h"
#include "chips.h"
#include "helpers.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <random>
#include <cmath>
#include <algorithm>

// ─── Loot table definitions ───────────────────────────────────────────────────

struct AdvLootEntry {
    std::string key; // "" = 無
    int pct;
};

struct AdvCheckpoint {
    int progress;
    int pool_pct; // relative weight (out of 100 total)
    std::vector<AdvLootEntry> items;
};

struct AdvRegion {
    std::string key;
    std::string name;
    std::string emoji;
    std::vector<AdvCheckpoint> checkpoints;
};

static const std::vector<AdvRegion> ADV_REGIONS = {
    {
        "mushroom_kingdom", "菇菇王國", "🍄",
        {
            { 19, 41, {
                {"col_ms_handkerchief", 37},
                {"col_gm_beret",        26},
                {"col_sm_spine",        23},
                {"",                    14},
            }},
            { 67, 56, {
                {"col_bm_tear",     37},
                {"col_zm_cheese",   38},
                {"",                24},
                {"col_yaya_bounty",  1},
            }},
            {100,  3, {
                {"col_mushroom_head", 30},
                {"col_mb_crown",      28},
                {"col_mb_staff",      20},
                {"",                   2},
                {"col_slim_wallet",   10},
                {"col_fat_wallet",    10},
            }},
        }
    },
    {
        "water_spirit_cave", "綠水靈洞窟", "💧",
        {
            { 23, 37, {
                {"",                    26},
                {"col_gwl_popsicle",    36},
                {"col_bwl_cake",        36},
                {"col_phone_tianxin",    2},
            }},
            { 61, 32, {
                {"col_dwl_tiramisu",    42},
                {"col_rwl_velvet",      41},
                {"",                    15},
                {"col_bath_huaxuan",     2},
            }},
            {100, 31, {
                {"",                    32},
                {"col_awl_avocado",     22},
                {"col_sqwl_brownie",    22},
                {"col_ywl_caramel",     22},
                {"col_cat_tears",        1},
                {"col_golden_staff",     1},
            }},
        }
    },
    {
        "ghost_graveyard", "亡魂墓地", "💀",
        {
            { 25, 40, {
                {"",                     30},
                {"col_ghost_heels",      20},
                {"col_kappa_cucumber",   20},
                {"col_zombie_eyepatch",  20},
                {"col_ghost_cloak",      10},
            }},
            { 61, 10, {
                {"",                     60},
                {"col_penguin_relic",     2},
                {"col_shark_relic",       2},
                {"col_koala_relic",       2},
                {"col_witch_broom",      34},
            }},
            {100, 40, {
                {"",                     32},
                {"col_demon_tear",       19},
                {"col_demon_heart",      19},
                {"col_demon_horn",       19},
                {"col_demon_costume",    10},
                {"col_koala_autograph",   1},
            }},
        }
    },
    {
        "bb_museum", "BB自然博物館", "🦴",
        {
            { 25, 55, {
                {"",                      40},
                {"col_bb_pink_cup",       17},
                {"col_bb_desk_terror",    17},
                {"col_bb_signus_chalice", 17},
                {"col_bb_mercury_staff",   8},
                {"col_bb_risk_dice",       1},
            }},
            // 這格機率含小數，內部改用千分位（總和 1000），跟其他格互不影響
            { 75, 35, {
                {"",                     400},
                {"col_bb_horn",          190},
                {"col_bb_death_ring",    190},
                {"col_bb_ski",           190},
                {"col_bb_sian_cloak",     15},
                {"col_bb_lost_underwear",  5},
                {"col_bb_magnifier",      10},
            }},
            {100, 10, {
                {"",                      40},
                {"col_bb_blood_gem",      24},
                {"col_bb_bracelet",       24},
                {"col_bb_mirror",         10},
                {"col_bb_wig_broken",      1},
                {"col_bb_undies_broken",   1},
            }},
        }
    },
    {
        "red_dragon_range", "赤龍山脈", "🐉",
        {
            { 46, 65, {
                {"",                     10},
                {"dragon_chest_small",   30},
                {"col_rd_amber",          9},
                {"col_rd_claw",           9},
                {"col_rd_azurescale",    10},
                {"col_rd_azuremarrow",   10},
                {"col_rd_earthbone",     11},
                {"col_rd_earthblood",    11},
            }},
            { 70, 30, {
                {"",                      9},
                {"dragon_chest_mid",     29},
                {"col_rd_iceeye",        11},
                {"col_rd_icescale",      11},
                {"col_rd_blackwing",     10},
                {"col_rd_blackeye",      10},
                {"col_rd_demonclaw",      9},
                {"col_rd_demoneye",       9},
                {"col_rd_rainbow",        2},
            }},
            {100,  5, {
                {"",                     13},
                {"col_rd_azureorb",       6},
                {"col_rd_redorb",         6},
                {"col_rd_iceorb",         6},
                {"col_rd_blackorb",       6},
                {"col_rd_demonorb",       6},
                {"col_rd_earthorb",       6},
                {"dragon_chest_grand",   47},
                {"col_rd_campticket",     1},
                {"col_rd_lovebook",       1},
                {"col_rd_simpmanual",     1},
                {"col_rd_dogbook",        1},
            }},
        }
    },
};

static const AdvRegion* find_adv_region(const std::string& key) {
    for (auto& r : ADV_REGIONS) if (r.key == key) return &r;
    return nullptr;
}

// 赤龍山脈：必須攜帶寵物、時長至少 6 小時
static bool adv_region_requires_pet6h(const std::string& region_key) {
    return region_key == "red_dragon_range";
}

// Collection display regions (3 pages; empty adv_key = locked/coming soon)
struct ColDisplayRegion { std::string adv_key; std::string name; std::string emoji; };
static const std::vector<ColDisplayRegion> COL_DISPLAY_REGIONS = {
    {"mushroom_kingdom",   "菇菇王國",   "🍄"},
    {"water_spirit_cave",  "綠水靈洞窟", "💧"},
    {"ghost_graveyard",    "亡魂墓地",   "💀"},
    {"bb_museum",          "BB自然博物館", "🦴"},
    {"red_dragon_range",   "赤龍山脈",   "🐉"},
};

static const std::set<std::string> LIMITED_COL_ITEMS = {
    "col_yaya_bounty", "col_slim_wallet", "col_fat_wallet",
    "col_phone_tianxin", "col_bath_huaxuan", "col_rod_zoey",
    "col_penguin_relic", "col_shark_relic", "col_koala_relic", "col_koala_autograph",
    // BB博物館限定（全球唯一）
    "col_bb_sian_cloak", "col_bb_lost_underwear", "col_bb_magnifier", "col_bb_risk_dice",
    // BB博物館限定（全球限量，上限見 LIMITED_MAX_COUNT）
    "col_bb_wig_broken", "col_bb_undies_broken", "col_bb_wig_full", "col_bb_undies_full",
    // 赤龍山脈限定（全球唯一）
    "col_rd_campticket", "col_rd_lovebook", "col_rd_simpmanual", "col_rd_dogbook",
    // 綠水靈洞窟限定（全球唯一）
    "col_cat_tears", "col_golden_staff",
};

// 全球限量道具：跟 LIMITED_COL_ITEMS（全球僅 1 份）不同，這些是全球固定份數上限。
// 合成消耗掉的不會釋放新名額——用「戰損版持有數 + 合成版持有數 × craft_ratio」估計歷史上總共出現過幾份，
// 這個數字只會增加不會減少（合成不會讓總數變少，只是把 5 份戰損版換成 1 份合成版）。
struct LimitedMaxRule { int cap; std::string crafted_key; int craft_ratio; };
static const std::map<std::string, LimitedMaxRule> LIMITED_MAX_COUNT = {
    {"col_bb_wig_broken",    {5, "col_bb_wig_full",    5}},
    {"col_bb_undies_broken", {5, "col_bb_undies_full", 5}},
};

// 「特殊」分頁：可使用的消耗型道具（風險骰子）
static const std::set<std::string> SPECIAL_COL_ITEMS = {
    "col_bb_risk_dice", "col_rd_lovebook", "col_rd_simpmanual", "col_rd_dogbook",
};

// ─── Collectible selling（限定收藏品全球唯一，不可售出，只能 !交易）──────────────
// 低級 1000／中級 2000／高級 3000。賣掉套組其中一件，該分級的套組加成就會失效。
static const std::map<std::string,int64_t> COL_SELL_PRICE = {
    // 低級（1000）
    {"col_ms_handkerchief", 1000}, {"col_gm_beret", 1000}, {"col_sm_spine", 1000},
    {"col_gwl_popsicle",    1000}, {"col_bwl_cake", 1000},
    {"col_ghost_heels",     1000}, {"col_kappa_cucumber", 1000}, {"col_zombie_eyepatch", 1000}, {"col_ghost_cloak", 1000},
    // 中級（2000）
    {"col_bm_tear",      2000}, {"col_zm_cheese", 2000},
    {"col_dwl_tiramisu", 2000}, {"col_rwl_velvet", 2000},
    {"col_witch_broom",  2000},
    // 高級（3000）
    {"col_mushroom_head", 3000}, {"col_mb_crown", 3000}, {"col_mb_staff", 3000},
    {"col_awl_avocado",   3000}, {"col_sqwl_brownie", 3000}, {"col_ywl_caramel", 3000},
    {"col_demon_tear",    3000}, {"col_demon_heart", 3000}, {"col_demon_horn", 3000}, {"col_demon_costume", 3000},
    // BB自然博物館 — 低級（1000）
    {"col_bb_pink_cup", 1000}, {"col_bb_desk_terror", 1000}, {"col_bb_signus_chalice", 1000}, {"col_bb_mercury_staff", 1000},
    // BB自然博物館 — 中級（2000）
    {"col_bb_horn", 2000}, {"col_bb_death_ring", 2000}, {"col_bb_ski", 2000},
    // BB自然博物館 — 高級（3000）
    {"col_bb_blood_gem", 3000}, {"col_bb_bracelet", 3000}, {"col_bb_mirror", 3000},
    // 赤龍山脈 — 低級（1000）
    {"col_rd_amber", 1000}, {"col_rd_claw", 1000}, {"col_rd_azurescale", 1000},
    {"col_rd_azuremarrow", 1000}, {"col_rd_earthbone", 1000}, {"col_rd_earthblood", 1000},
    // 赤龍山脈 — 中級（2000）
    {"col_rd_iceeye", 2000}, {"col_rd_icescale", 2000}, {"col_rd_blackwing", 2000},
    {"col_rd_blackeye", 2000}, {"col_rd_demonclaw", 2000}, {"col_rd_demoneye", 2000}, {"col_rd_rainbow", 2000},
    // 赤龍山脈 — 高級（3000）
    {"col_rd_azureorb", 3000}, {"col_rd_redorb", 3000}, {"col_rd_iceorb", 3000},
    {"col_rd_blackorb", 3000}, {"col_rd_demonorb", 3000}, {"col_rd_earthorb", 3000},
};
static std::string col_tier_label(const std::string& key) {
    auto it = COL_SELL_PRICE.find(key);
    if (it == COL_SELL_PRICE.end()) return "";
    if (it->second == 1000) return "低級";
    if (it->second == 2000) return "中級";
    return "高級";
}

// 收藏品 key → 所屬套組的完成度判定函式，供售出／交易前檢查是否會打破套組加成
using ColSetCheckFn = bool(*)(dpp::snowflake);
static const std::map<std::string, ColSetCheckFn> COL_KEY_TO_SET_CHECK = {
    {"col_ms_handkerchief", col_set_mushroom_basic}, {"col_gm_beret", col_set_mushroom_basic}, {"col_sm_spine", col_set_mushroom_basic},
    {"col_bm_tear", col_set_mushroom_mid}, {"col_zm_cheese", col_set_mushroom_mid},
    {"col_mushroom_head", col_set_mushroom_adv}, {"col_mb_crown", col_set_mushroom_adv}, {"col_mb_staff", col_set_mushroom_adv},
    {"col_gwl_popsicle", col_set_water_basic}, {"col_bwl_cake", col_set_water_basic},
    {"col_dwl_tiramisu", col_set_water_mid}, {"col_rwl_velvet", col_set_water_mid},
    {"col_awl_avocado", col_set_water_adv}, {"col_sqwl_brownie", col_set_water_adv}, {"col_ywl_caramel", col_set_water_adv},
    {"col_ghost_heels", col_set_ghost_basic}, {"col_kappa_cucumber", col_set_ghost_basic}, {"col_zombie_eyepatch", col_set_ghost_basic}, {"col_ghost_cloak", col_set_ghost_basic},
    {"col_witch_broom", col_set_ghost_mid},
    {"col_demon_tear", col_set_ghost_adv}, {"col_demon_heart", col_set_ghost_adv}, {"col_demon_horn", col_set_ghost_adv}, {"col_demon_costume", col_set_ghost_adv},
    {"col_bb_pink_cup", col_set_bb_basic}, {"col_bb_desk_terror", col_set_bb_basic}, {"col_bb_signus_chalice", col_set_bb_basic}, {"col_bb_mercury_staff", col_set_bb_basic},
    {"col_bb_horn", col_set_bb_mid}, {"col_bb_death_ring", col_set_bb_mid}, {"col_bb_ski", col_set_bb_mid},
    {"col_bb_blood_gem", col_set_bb_adv}, {"col_bb_bracelet", col_set_bb_adv}, {"col_bb_mirror", col_set_bb_adv},
    {"col_rd_amber", col_set_rd_basic}, {"col_rd_claw", col_set_rd_basic}, {"col_rd_azurescale", col_set_rd_basic},
    {"col_rd_azuremarrow", col_set_rd_basic}, {"col_rd_earthbone", col_set_rd_basic}, {"col_rd_earthblood", col_set_rd_basic},
    {"col_rd_iceeye", col_set_rd_mid}, {"col_rd_icescale", col_set_rd_mid}, {"col_rd_blackwing", col_set_rd_mid},
    {"col_rd_blackeye", col_set_rd_mid}, {"col_rd_demonclaw", col_set_rd_mid}, {"col_rd_demoneye", col_set_rd_mid}, {"col_rd_rainbow", col_set_rd_mid},
    {"col_rd_azureorb", col_set_rd_adv}, {"col_rd_redorb", col_set_rd_adv}, {"col_rd_iceorb", col_set_rd_adv},
    {"col_rd_blackorb", col_set_rd_adv}, {"col_rd_demonorb", col_set_rd_adv}, {"col_rd_earthorb", col_set_rd_adv},
};

// 若移交（售出／交易）1 個 key 會讓該玩家目前完整的套組加成失效，回傳 true。
// 呼叫前不可持有 data_mutex（自己上鎖）。
static bool col_would_break_set(dpp::snowflake uid, const std::string& key) {
    auto sit = COL_KEY_TO_SET_CHECK.find(key);
    if (sit == COL_KEY_TO_SET_CHECK.end()) return false;
    std::lock_guard<std::mutex> lk(data_mutex);
    int cnt = 0;
    auto it = inventory_data.find(uid);
    if (it != inventory_data.end()) { auto jt = it->second.find(key); if (jt != it->second.end()) cnt = jt->second; }
    if (cnt != 1) return false; // 交易/售出後還會剩下，不影響套組
    return sit->second(uid);
}

// ─── Progress calculation ──────────────────────────────────────────────────────

// pet_stage: 0=無寵物同行, 1/2/3=同行寵物的階段（一階+10／二階+15／三階+20）
static int calc_adv_progress(int hours, int64_t funds, int pet_stage) {
    int p = 2 + hours * 4 + (int)(funds / 250);
    if      (pet_stage == 1) p += 10;
    else if (pet_stage == 2) p += 15;
    else if (pet_stage == 3) p += 20;
    return std::min(p, 100);
}

// ─── Loot rolling ─────────────────────────────────────────────────────────────

// excluded_keys：不能中的道具（例如全球唯一的限定收藏品已被別人拿走）——
// 從候選池排除後在剩下的道具間重骰，機率會依原本比例重新分配，不會白白浪費掉。
static std::string roll_adv_loot(const std::string& region_key, int progress,
                                  const std::set<std::string>& excluded_keys = {}) {
    static std::mt19937 adv_rng(std::random_device{}());
    const AdvRegion* region = find_adv_region(region_key);
    if (!region || region->checkpoints.empty()) return "";

    std::vector<double> weights;
    for (auto& cp : region->checkpoints) {
        int dist = std::abs(progress - cp.progress);
        double w = (dist == 0) ? 1.0 : (1.0 / (double)dist);
        weights.push_back(w * cp.pool_pct / 100.0);
    }
    double total = 0; for (double w : weights) total += w;
    if (total <= 0) return "";

    double pick = std::uniform_real_distribution<double>(0.0, total)(adv_rng);
    int cp_idx = (int)region->checkpoints.size() - 1;
    double acc = 0;
    for (int i = 0; i < (int)weights.size(); i++) {
        acc += weights[i];
        if (pick < acc) { cp_idx = i; break; }
    }

    auto& cp = region->checkpoints[cp_idx];
    int total_pct = 0;
    for (auto& item : cp.items) {
        if (!item.key.empty() && excluded_keys.count(item.key)) continue;
        total_pct += item.pct;
    }
    if (total_pct <= 0) return "";
    int roll = std::uniform_int_distribution<int>(1, total_pct)(adv_rng);
    int cum = 0;
    for (auto& item : cp.items) {
        if (!item.key.empty() && excluded_keys.count(item.key)) continue;
        cum += item.pct;
        if (roll <= cum) return item.key;
    }
    return "";
}

// 出發前預覽：目前探索度最可能落在哪個 checkpoint，以及整體「空手」機率是多少。
// 只用預設機率表估算（不考慮限定道具當下是否已被排除），僅供玩家出發前參考。
struct AdvPreview { std::string likely_tier; double miss_pct; };
static AdvPreview calc_adv_preview(const std::string& region_key, int progress) {
    AdvPreview result{"未知", 100.0};
    const AdvRegion* region = find_adv_region(region_key);
    if (!region || region->checkpoints.empty()) return result;

    std::vector<double> weights;
    for (auto& cp : region->checkpoints) {
        int dist = std::abs(progress - cp.progress);
        double w = (dist == 0) ? 1.0 : (1.0 / (double)dist);
        weights.push_back(w * cp.pool_pct / 100.0);
    }
    double total = 0; for (double w : weights) total += w;
    if (total <= 0) return result;

    static const char* TIER_NAMES[] = {"初級區", "中級區", "高級區"};
    int best_idx = 0;
    for (int i = 1; i < (int)weights.size(); i++) if (weights[i] > weights[best_idx]) best_idx = i;
    result.likely_tier = (best_idx < 3) ? TIER_NAMES[best_idx] : ("第" + std::to_string(best_idx + 1) + "區");

    double miss_prob = 0.0;
    for (int i = 0; i < (int)region->checkpoints.size(); i++) {
        auto& cp = region->checkpoints[i];
        int cp_total = 0, cp_empty = 0;
        for (auto& item : cp.items) {
            cp_total += item.pct;
            if (item.key.empty()) cp_empty += item.pct;
        }
        if (cp_total > 0) miss_prob += (weights[i] / total) * ((double)cp_empty / cp_total);
    }
    result.miss_pct = miss_prob * 100.0;
    return result;
}

// ─── Persistence ──────────────────────────────────────────────────────────────

static const std::string ADV_FILE = "adventure.json";

static void save_adv_games() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, g] : adv_games) {
            j[std::to_string((uint64_t)uid)] = {
                {"region_key",     g.region_key},
                {"duration_hours", g.duration_hours},
                {"funds",          (int64_t)g.funds},
                {"pet_along",      g.pet_along},
                {"pet_stage",      g.pet_stage},
                {"start_time",     (int64_t)g.start_time},
                {"end_time",       (int64_t)g.end_time},
                {"notify_on_finish", g.notify_on_finish},
                {"finish_notified",  g.finish_notified},
                {"star_boost",       g.star_boost},
            };
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(ADV_FILE, j.dump(2));
}

static void load_adv_games() {
    std::ifstream f(ADV_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        adv_games.clear();
        for (auto& [k, v] : j.items()) {
            AdventureGame g;
            g.uid            = dpp::snowflake(std::stoull(k));
            g.region_key     = v.value("region_key",     std::string{});
            g.duration_hours = v.value("duration_hours", 0);
            g.funds          = (int64_t)v.value("funds", (int64_t)0);
            g.pet_along      = v.value("pet_along",      false);
            // 舊資料沒有 pet_stage：帶寵物就當作三階（沿用舊版固定 +20 的行為）
            g.pet_stage      = v.value("pet_stage", g.pet_along ? 3 : 0);
            g.start_time     = (time_t)v.value("start_time", (int64_t)0);
            g.end_time       = (time_t)v.value("end_time",   (int64_t)0);
            g.notify_on_finish = v.value("notify_on_finish", false);
            g.finish_notified  = v.value("finish_notified",  false);
            g.star_boost       = v.value("star_boost",       false);
            adv_games[g.uid] = g;
        }
    } catch (...) {}
}

// 柴犬百科全書：全域每週次數，獨立小檔案持久化
static const std::string DOGBOOK_FILE = "dogbook.json";

static void save_dogbook() {
    nlohmann::json j;
    { std::lock_guard<std::mutex> lk(data_mutex); j = {{"week", g_dogbook_week}, {"uses_left", g_dogbook_uses_left}}; }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(DOGBOOK_FILE, j.dump(2));
}

static void load_dogbook() {
    std::ifstream f(DOGBOOK_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        g_dogbook_week      = j.value("week",      0);
        g_dogbook_uses_left = j.value("uses_left", 0);
    } catch (...) {}
}

// ─── UI helpers ───────────────────────────────────────────────────────────────

static std::string adv_fmt_remain(time_t end_t) {
    int secs = (int)(end_t - time(nullptr));
    if (secs <= 0) return "已完成";
    int h = secs / 3600, m = (secs % 3600) / 60;
    if (h > 0) return std::to_string(h) + " 小時 " + std::to_string(m) + " 分";
    return std::to_string(m) + " 分鐘";
}

static std::string adv_fmt_clock(time_t t) {
    struct tm tm_{}; localtime_s(&tm_, &t);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d", tm_.tm_mon+1, tm_.tm_mday, tm_.tm_hour, tm_.tm_min);
    return std::string(buf);
}

static std::string adv_progress_bar(double ratio, int w = 10) {
    int filled = std::max(0, std::min(w, (int)(ratio * w)));
    std::string bar;
    for (int i = 0; i < w; i++) bar += (i < filled) ? "⬛" : "⬜";
    return bar;
}

// ─── Collection pages ──────────────────────────────────────────────────────────

// Main collection hub
static dpp::message make_collection_msg(dpp::snowflake uid,
                                         const std::string& dn = "",
                                         const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    int normal_cnt = 0, limited_cnt = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid);
        if (it != inventory_data.end())
            for (auto& [k, cnt] : it->second)
                if (k.rfind("col_", 0) == 0 && cnt > 0)
                    (LIMITED_COL_ITEMS.count(k) ? limited_cnt : normal_cnt) += cnt;
    }
    std::string user_tag = dn.empty() ? uid_s : dn;
    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
    container.add_component_v2(v2_section(
        "## 📚 收藏\n"
        "📗 **一般收藏**：各地區探險掉落的蒐藏品，未獲得者顯示 `????`。\n"
        "⭐ **限定收藏**：全球唯一的限定品，可透過 `!交易` 轉讓。\n\n"
        "目前持有　📗 一般：**" + std::to_string(normal_cnt) + "**　⭐ 限定：**" + std::to_string(limited_cnt) + "**"
        "\n\n-# 👤 " + user_tag, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);
    add_bag_home_button(msg, uid);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📗 一般收藏").set_id("adv_col_normal_" + uid_s + "_1").set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⭐ 限定收藏").set_id("adv_col_limited_" + uid_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 售出").set_id("adv_col_sell_" + uid_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

// Normal collection – one page per region
static dpp::message make_normal_col_msg(dpp::snowflake uid,
                                         const std::string& dn, const std::string& av,
                                         int page) {
    std::string uid_s = std::to_string((uint64_t)uid);
    int total_pages = (int)COL_DISPLAY_REGIONS.size();
    if (page < 1) page = 1;
    if (page > total_pages) page = total_pages;
    const ColDisplayRegion& cr = COL_DISPLAY_REGIONS[page - 1];

    std::map<std::string,int> inv;
    { std::lock_guard<std::mutex> lk(data_mutex); auto it = inventory_data.find(uid); if (it != inventory_data.end()) inv = it->second; }

    std::string user_tag = dn.empty() ? uid_s : dn;
    std::string content = "## 📗 一般收藏 — " + cr.emoji + " " + cr.name +
                          "  [" + std::to_string(page) + "/" + std::to_string(total_pages) + "]\n";

    if (cr.adv_key.empty()) {
        content += "🔒 **此地區尚未開放**，敬請期待！";
    } else {
        struct SubDef { std::string name; std::string reward; std::vector<std::string> keys; };
        static const std::vector<std::vector<SubDef>> PAGE_SUBS = {
            { // 菇菇王國
                {"🌱 初級區", "寵物攻擊力 ×1.01", {"col_ms_handkerchief","col_gm_beret","col_sm_spine"}},
                {"🌿 中級區", "探險時長 -1%",      {"col_bm_tear","col_zm_cheese"}},
                {"🌳 高級區", "打工時長 -1%",      {"col_mushroom_head","col_mb_crown","col_mb_staff"}},
            },
            { // 綠水靈洞窟
                {"🌱 初級區", "寵物生命值 ×1.01", {"col_gwl_popsicle","col_bwl_cake"}},
                {"🌿 中級區", "探險時長 -1%",      {"col_dwl_tiramisu","col_rwl_velvet"}},
                {"🌳 高級區", "溫泉時長 -5%",      {"col_awl_avocado","col_sqwl_brownie","col_ywl_caramel"}},
            },
            { // 亡魂墓地
                {"🌱 初級區", "寵物防禦力 ×1.02", {"col_ghost_heels","col_kappa_cucumber","col_zombie_eyepatch","col_ghost_cloak"}},
                {"🌿 中級區", "探險時長 -1%",      {"col_witch_broom"}},
                {"🌳 高級區", "打工報酬 +1%",      {"col_demon_tear","col_demon_heart","col_demon_horn","col_demon_costume"}},
            },
            { // BB自然博物館（低於2%機率的特殊道具另外顯示在背包「特殊」分頁）
                {"🌱 初級區", "強化成功率 +5%", {"col_bb_pink_cup","col_bb_desk_terror","col_bb_signus_chalice","col_bb_mercury_staff"}},
                {"🌿 中級區", "狩獵／王團獎勵籌碼 +3%", {"col_bb_horn","col_bb_death_ring","col_bb_ski"}},
                {"🌳 高級區", "銀行借款利率 -0.5%", {"col_bb_blood_gem","col_bb_bracelet","col_bb_mirror"}},
            },
            { // 赤龍山脈（1%機率的限定道具另外顯示在「限定收藏」頁）
                {"🌱 初級區", "強化所需金額 -5%",  {"col_rd_amber","col_rd_claw","col_rd_azurescale","col_rd_azuremarrow","col_rd_earthbone","col_rd_earthblood"}},
                {"🌿 中級區", "強化所需金額 -10%", {"col_rd_iceeye","col_rd_icescale","col_rd_blackwing","col_rd_blackeye","col_rd_demonclaw","col_rd_demoneye","col_rd_rainbow"}},
                {"🌳 高級區", "強化所需金額 -15%", {"col_rd_azureorb","col_rd_redorb","col_rd_iceorb","col_rd_blackorb","col_rd_demonorb","col_rd_earthorb"}},
            },
        };
        auto has_all = [&](const std::vector<std::string>& ks) {
            for (auto& k : ks) { auto it = inv.find(k); if (it == inv.end() || it->second <= 0) return false; }
            return true;
        };
        for (auto& sr : PAGE_SUBS[page - 1]) {
            bool done = has_all(sr.keys);
            std::string header = sr.name + "　獎勵：" + sr.reward;
            if (done) header = "✅ " + header + "（**已完成！**）";
            content += "\n**" + header + "**\n";
            for (auto& k : sr.keys) {
                auto* vi = find_virtual_item(k);
                int owned = 0; auto it = inv.find(k); if (it != inv.end()) owned = it->second;
                std::string id_s = vi ? "ID: " + std::to_string(vi->item_id) : k;
                if (owned > 0)
                    content += "✅ **" + id_s + "**　" + (vi ? vi->name : k) + " ×" + std::to_string(owned) + "\n";
                else
                    content += "❓ **" + id_s + "**　????\n";
            }
        }
    }
    content += "\n-# 👤 " + user_tag;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x27, 0xAE, 0x60));
    container.add_component_v2(v2_section(content, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("◀").set_id("adv_col_normal_" + uid_s + "_" + std::to_string(page - 1))
        .set_style(dpp::cos_secondary).set_disabled(page <= 1));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶").set_id("adv_col_normal_" + uid_s + "_" + std::to_string(page + 1))
        .set_style(dpp::cos_secondary).set_disabled(page >= total_pages));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

// Limited collection – global uniqueness display
// 限定收藏顯示清單（獨立於 LIMITED_COL_ITEMS）：需要合成才能取得的完整版（Zoey的秀髮／皮包遺失的粉紅內衣）
// 不列在這裡，只顯示可以直接探險拿到的戰損版；完整版本身仍受 LIMITED_MAX_COUNT／不可售出等規則保護，只是不佔這頁版面。
static const std::vector<std::pair<std::string,std::string>> LIMITED_ORDER = {
    // 全球唯一（1份）
    {"col_yaya_bounty",     "每日狩獵卷上限 +1"},
    {"col_slim_wallet",     "打工收益 +3%"},
    {"col_fat_wallet",      "打工收益 +7%"},
    {"col_phone_tianxin",   "借款上限 +30000，利率降至 2.98%"},
    {"col_bath_huaxuan",    "每週 3 次，阻擋寵物負面狀態"},
    {"col_rod_zoey",        "寵物泡溫泉完成後觸發釣魚效果"},
    {"col_penguin_relic",   "寵物戰鬥防禦力 +1"},
    {"col_shark_relic",     "寵物戰鬥攻擊力 +1"},
    {"col_koala_relic",     "寵物戰鬥血量 +10"},
    {"col_koala_autograph", "打工時間縮短 3%"},
    {"col_cat_tears",       "戰鬥中受到傷害時，5% 機率恢復 5 點血量"},
    {"col_golden_staff",    "戰鬥中攻擊時，1% 機率額外多打一下"},
    // BB博物館 — 全球唯一（1份）
    {"col_bb_sian_cloak",     "戰鬥中每回合機率閃避怪物攻擊（1%）"},
    {"col_bb_lost_underwear", "每場戰鬥第一次攻擊，攻擊力 +5"},
    {"col_bb_magnifier",      "探索時長 -5%"},
    {"col_bb_risk_dice",      "背包「特殊」分頁可用，一天可擲 2 次：1% -5000碼／10% +5000碼／89% -500碼"},
    // BB博物館 — 全球限量（5份，可疊加）
    {"col_bb_wig_broken",     "探險收取時有 1% 機率額外骰一次戰利品（可疊加）── 全球限量 5 份"},
    {"col_bb_undies_broken",  "探險完成有 2% 機率返還探索資金（可疊加）── 全球限量 5 份"},
    // 赤龍山脈 — 全球唯一（1份）
    {"col_rd_campticket", "持有時打工只剩「24小時／12000碼／20exp／必定憂鬱」單一選項，不觸發監工代打與醫療保險"},
    {"col_rd_lovebook",   "虛擬商店購買 95 折、轉帳與交易免手續費 ── 背包「特殊」分頁可付 2000 碼解鎖一次性交易"},
    {"col_rd_simpmanual", "背包「特殊」分頁每天可領取 1 杯高級強效咖啡（本人無法使用/售出，須交易給別人才能用）"},
    {"col_rd_dogbook",    "背包「特殊」分頁每週 4 次隨機抽取虛擬商店 3000 碼以下道具（25% 失敗機率）"},
};

static dpp::message make_limited_col_msg(dpp::snowflake uid,
                                          const std::string& dn, const std::string& av, int page = 1) {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::map<std::string,int> inv;
    std::map<std::string,dpp::snowflake> holder;
    std::map<std::string,int> capped_remaining; // 限量N份道具：剩餘可探索份數
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid); if (it != inventory_data.end()) inv = it->second;
        for (auto& lk2 : LIMITED_COL_ITEMS) {
            holder[lk2] = 0;
            for (auto& [oid, oinv] : inventory_data) {
                auto jt = oinv.find(lk2);
                if (jt != oinv.end() && jt->second > 0) { holder[lk2] = oid; break; }
            }
        }
        for (auto& [cap_key, rule] : LIMITED_MAX_COUNT) {
            int64_t total = 0;
            for (auto& [oid, oinv] : inventory_data) {
                auto jt = oinv.find(cap_key);
                if (jt != oinv.end()) total += jt->second;
                if (!rule.crafted_key.empty()) {
                    auto jt2 = oinv.find(rule.crafted_key);
                    if (jt2 != oinv.end()) total += (int64_t)jt2->second * rule.craft_ratio;
                }
            }
            capped_remaining[cap_key] = (int)std::max((int64_t)0, (int64_t)rule.cap - total);
        }
    }

    const int PAGE_SIZE = 10;
    int total_pages = std::max(1, (int)((LIMITED_ORDER.size() + PAGE_SIZE - 1) / PAGE_SIZE));
    page = std::max(1, std::min(page, total_pages));
    int start = (page - 1) * PAGE_SIZE;
    int end = std::min((int)LIMITED_ORDER.size(), start + PAGE_SIZE);

    std::string user_tag = dn.empty() ? uid_s : dn;
    std::string desc = "## ⭐ 限定收藏\n限定收藏品全球稀有，可透過探險獲得，也可透過 `!交易` 轉讓。\n"
                        "第 **" + std::to_string(page) + "/" + std::to_string(total_pages) + "** 頁\n\n";
    for (int i = start; i < end; i++) {
        auto& [key, effect] = LIMITED_ORDER[i];
        auto* vi = find_virtual_item(key);
        int owned = 0; auto oit = inv.find(key); if (oit != inv.end()) owned = oit->second;
        std::string id_s = vi ? "**ID: " + std::to_string(vi->item_id) + "**" : key;
        std::string name = vi ? vi->name : key;
        if (owned > 0) {
            std::string cnt_s = (owned > 1) ? " ×" + std::to_string(owned) : "";
            desc += "✅ " + id_s + "　" + name + cnt_s + "\n　效果：" + effect + "\n\n";
        } else if (LIMITED_MAX_COUNT.count(key)) {
            // 限量N份：顯示剩餘可探索數
            int rem = capped_remaining.count(key) ? capped_remaining.at(key) : 0;
            int cap = LIMITED_MAX_COUNT.at(key).cap;
            if (rem > 0) {
                desc += "🔓 " + id_s + "　" + name + "\n　效果：" + effect
                      + "　（全球還剩 **" + std::to_string(rem) + "/" + std::to_string(cap) + "** 份可探索）\n\n";
            } else {
                desc += "🔒 " + id_s + "　" + name + "\n　效果：" + effect
                      + "　（全球 " + std::to_string(cap) + " 份已全數出現）\n\n";
            }
        } else {
            dpp::snowflake h = holder[key];
            if (h != 0) {
                desc += "🔒 " + id_s + "　" + name + "\n　效果：" + effect
                      + "　（持有者：<@" + std::to_string((uint64_t)h) + ">）\n\n";
            } else {
                desc += "❓ " + id_s + "　" + name + "\n　效果：?????　（尚未被任何人發現）\n\n";
            }
        }
    }
    desc += "-# 👤 " + user_tag;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF3, 0x9C, 0x12));
    container.add_component_v2(v2_section(desc, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    if (total_pages > 1) {
        dpp::component pg_row; pg_row.set_type(dpp::cot_action_row);
        pg_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("◀").set_id("adv_col_limited_page_" + uid_s + "_" + std::to_string(page - 1))
            .set_style(dpp::cos_secondary).set_disabled(page <= 1));
        pg_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("▶").set_id("adv_col_limited_page_" + uid_s + "_" + std::to_string(page + 1))
            .set_style(dpp::cos_secondary).set_disabled(page >= total_pages));
        msg.add_component_v2(pg_row);
    }

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

// ─── Collection sell page ──────────────────────────────────────────────────────
// 限定收藏品全球唯一，不列在這裡（不可售出，只能 !交易 轉讓）

static dpp::message make_col_sell_msg(dpp::snowflake uid,
                                       const std::string& dn, const std::string& av, int page = 1) {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::map<std::string,int> inv;
    { std::lock_guard<std::mutex> lk(data_mutex); auto it = inventory_data.find(uid); if (it != inventory_data.end()) inv = it->second; }

    struct ColEntry { std::string key; int count; int64_t price; std::string tier; bool breaks; };
    std::vector<ColEntry> entries;
    for (auto& [key, price] : COL_SELL_PRICE) {
        auto it = inv.find(key);
        if (it != inv.end() && it->second > 0)
            entries.push_back({key, it->second, price, col_tier_label(key), false});
    }
    std::sort(entries.begin(), entries.end(), [](const ColEntry& a, const ColEntry& b) {
        if (a.price != b.price) return a.price < b.price;
        return a.key < b.key;
    });
    // ⚠️ 標示：賣掉這一件是否會讓目前完成的套組失效（只有剩最後一個時才會，col_would_break_set 已處理）
    for (auto& en : entries) en.breaks = col_would_break_set(uid, en.key);

    bool has_excess_low = false, has_excess_mid = false, has_excess_high = false;
    for (auto& en : entries) {
        if (en.count <= 1) continue; // 只剩1件的話賣多餘的沒有意義
        if (en.tier == "低級") has_excess_low = true;
        else if (en.tier == "中級") has_excess_mid = true;
        else if (en.tier == "高級") has_excess_high = true;
    }

    const int PAGE_SIZE = 10; // 2 列 × 5 顆，留空間給分頁/批量/返回列（V2 訊息一次最多約 5 個 action row）
    int total_pages = std::max(1, (int)((entries.size() + PAGE_SIZE - 1) / PAGE_SIZE));
    page = std::max(1, std::min(page, total_pages));
    int start = (page - 1) * PAGE_SIZE;
    int end = std::min((int)entries.size(), start + PAGE_SIZE);

    std::string user_tag = dn.empty() ? uid_s : dn;
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    std::string content = "## 💰 售出收藏品\n";
    if (entries.empty()) {
        content += "沒有可售出的收藏品。\n限定收藏品無法售出，只能透過 `!交易` 轉讓。";
    } else {
        content += "低級 **1000** 碼／中級 **2000** 碼／高級 **3000** 碼。\n"
                   "⚠️ 標示代表賣掉這件會讓套組加成失效（僅剩最後一件時才會標示）。\n"
                   "「售出多餘」會保留每種 1 件、賣掉其餘的，不會打破套組。\n"
                   "限定收藏品無法售出，只能透過 `!交易` 轉讓。\n"
                   "第 **" + std::to_string(page) + "/" + std::to_string(total_pages) + "** 頁（共 " + std::to_string(entries.size()) + " 種）";
    }
    content += "\n\n-# 👤 " + user_tag;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(v2_section(content, av));
    msg.add_component_v2(container);

    if (!entries.empty()) {
        dpp::component cur_row; cur_row.set_type(dpp::cot_action_row);
        int n = 0;
        for (int i = start; i < end; i++) {
            auto& en = entries[i];
            auto* vi = find_virtual_item(en.key);
            if (!vi) continue;
            if (n > 0 && n % 5 == 0) {
                msg.add_component_v2(cur_row);
                cur_row = dpp::component(); cur_row.set_type(dpp::cot_action_row);
            }
            std::string label = (en.breaks ? "⚠️ " : "") + vi->name + "（" + en.tier + "）+" + std::to_string(en.price) + "碼";
            cur_row.add_component(dpp::component().set_type(dpp::cot_button)
               .set_label(label)
               .set_id("adv_col_sellitem_" + uid_s + "_" + en.key)
               .set_style(dpp::cos_danger));
            n++;
        }
        if (n > 0) msg.add_component_v2(cur_row);

        if (total_pages > 1) {
            dpp::component pg_row; pg_row.set_type(dpp::cot_action_row);
            pg_row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("◀").set_id("adv_col_sell_page_" + uid_s + "_" + std::to_string(page - 1))
                .set_style(dpp::cos_secondary).set_disabled(page <= 1));
            pg_row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("▶").set_id("adv_col_sell_page_" + uid_s + "_" + std::to_string(page + 1))
                .set_style(dpp::cos_secondary).set_disabled(page >= total_pages));
            msg.add_component_v2(pg_row);
        }

        dpp::component excess_row; excess_row.set_type(dpp::cot_action_row);
        auto mk_excess = [&](const std::string& label, const std::string& tier, bool has_excess) {
            excess_row.add_component(dpp::component().set_type(dpp::cot_button).set_label(label)
                 .set_id("adv_col_sellexcess_" + uid_s + "_" + tier)
                 .set_style(dpp::cos_danger).set_disabled(!has_excess));
        };
        mk_excess("售出多餘 低級", "low", has_excess_low);
        mk_excess("售出多餘 中級", "mid", has_excess_mid);
        mk_excess("售出多餘 高級", "high", has_excess_high);
        msg.add_component_v2(excess_row);
    }

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

// ─── Backpack — Special tab（機率 < 2% 的限定道具，目前主要是园园的風險骰子）──────

static bool risk_dice_same_day(time_t a, time_t b) {
    struct tm ta{}, tb{};
    localtime_s(&ta, &a); localtime_s(&tb, &b);
    return ta.tm_year == tb.tm_year && ta.tm_yday == tb.tm_yday;
}

static dpp::message make_bag_special_msg(dpp::snowflake uid,
                                          const std::string& dn = "", const std::string& av = "",
                                          const std::string& notice = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::map<std::string,int> inv;
    int risk_uses_today = 0;
    bool lovebook_unlocked = false;
    bool coffee_claimed_today = false;
    std::map<std::string, StockHolding> holdings;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid);
        if (it != inventory_data.end()) inv = it->second;
        auto cit = chip_data.find(uid);
        if (cit != chip_data.end()) {
            time_t now = time(nullptr);
            risk_uses_today = risk_dice_same_day(now, cit->second.risk_dice_day) ? cit->second.risk_dice_uses : 0;
        }
        auto sit = player_stocks.find(uid);
        if (sit != player_stocks.end()) holdings = sit->second;
        if (it != inventory_data.end()) {
            auto lit = it->second.find("_lovebook_unlocked");
            if (lit != it->second.end() && lit->second > 0) lovebook_unlocked = true;
            auto cit2 = it->second.find("_coffee_last_claim");
            if (cit2 != it->second.end() && risk_dice_same_day(time(nullptr), (time_t)cit2->second)) coffee_claimed_today = true;
        }
        // 柴犬百科：全域每週次數，跨週重置
        time_t now_t = time(nullptr); struct tm tmw = {}; localtime_s(&tmw, &now_t);
        char wbuf[8]; strftime(wbuf, sizeof(wbuf), "%G%V", &tmw);
        int cur_week = std::stoi(wbuf);
        if (g_dogbook_week != cur_week) { g_dogbook_week = cur_week; g_dogbook_uses_left = 4; }
    }

    std::string user_tag = dn.empty() ? uid_s : dn;
    std::string desc = "## 🌟 背包 — 特殊\n";
    if (!notice.empty()) desc += notice + "\n\n";

    bool has_any = false;
    for (auto& key : SPECIAL_COL_ITEMS) {
        auto it = inv.find(key);
        int cnt = (it != inv.end()) ? it->second : 0;
        if (cnt <= 0) continue;
        has_any = true;
        auto* vi = find_virtual_item(key);
        if (!vi) continue;
        std::string id_str = vi->item_id ? ("`" + std::to_string(vi->item_id) + "`  ") : "";
        desc += id_str + "**" + vi->name + "** ×" + std::to_string(cnt) + "\n　" + vi->desc + "\n\n";
    }
    bool has_stocks = false;
    for (auto& [key, h] : holdings) {
        if (h.shares <= 0) continue;
        has_stocks = true;
        auto* def = find_stock_def(key);
        int64_t price = 0;
        { std::lock_guard<std::mutex> lk(stock_mutex); auto sit = stock_market.find(key); if (sit != stock_market.end()) price = sit->second.price; }
        int64_t pnl = (price - h.avg_cost) * h.shares;
        desc += (def ? def->emoji + " " + def->name : key) + "　持有 **" + std::to_string(h.shares)
              + "** 股　均價 " + std::to_string(h.avg_cost) + " 碼　損益 " + (pnl >= 0 ? "+" : "") + std::to_string(pnl) + " 碼\n";
    }
    if (has_stocks) desc += "\n";
    if (!has_any && !has_stocks) desc += "目前沒有特殊道具。機率低於 2% 掉落的限定道具與股票持有會顯示在這裡。";
    desc += "\n\n-# 👤 " + user_tag;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE6, 0x7E, 0x22));
    container.add_component_v2(v2_section(desc, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    add_bag_home_button(msg, uid);

    {
        dpp::component srow; srow.set_type(dpp::cot_action_row);
        srow.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📊 前往股市").set_id("stock_home_" + uid_s).set_style(dpp::cos_primary));
        msg.add_component_v2(srow);
    }

    bool has_dice = inv.count("col_bb_risk_dice") && inv.at("col_bb_risk_dice") > 0;
    if (has_dice) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        bool can_use = risk_uses_today < 2;
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🎲 使用風險骰子（今日已用 " + std::to_string(risk_uses_today) + "/2）")
            .set_id("adv_risk_dice_" + uid_s)
            .set_style(dpp::cos_danger)
            .set_disabled(!can_use));
        msg.add_component_v2(row);
    }

    bool has_lovebook = inv.count("col_rd_lovebook") && inv.at("col_rd_lovebook") > 0;
    if (has_lovebook) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(lovebook_unlocked ? "✅ 已解鎖交易中" : "💕 付費解鎖交易（2000碼）")
            .set_id("adv_lovebook_unlock_" + uid_s)
            .set_style(lovebook_unlocked ? dpp::cos_secondary : dpp::cos_danger)
            .set_disabled(lovebook_unlocked));
        msg.add_component_v2(row);
    }

    bool has_simpmanual = inv.count("col_rd_simpmanual") && inv.at("col_rd_simpmanual") > 0;
    if (has_simpmanual) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(coffee_claimed_today ? "☕ 今日已領取" : "☕ 領取今日咖啡")
            .set_id("adv_simpmanual_claim_" + uid_s)
            .set_style(dpp::cos_danger)
            .set_disabled(coffee_claimed_today));
        msg.add_component_v2(row);
    }

    bool has_dogbook = inv.count("col_rd_dogbook") && inv.at("col_rd_dogbook") > 0;
    if (has_dogbook) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        bool can_draw = g_dogbook_uses_left > 0;
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🐕 隨機抽取道具（本週剩 " + std::to_string(g_dogbook_uses_left) + "/4）")
            .set_id("adv_dogbook_draw_" + uid_s)
            .set_style(dpp::cos_danger)
            .set_disabled(!can_draw));
        msg.add_component_v2(row);
    }

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

// ─── Setup page ───────────────────────────────────────────────────────────────

static dpp::message make_adv_setup_msg(dpp::snowflake uid,
                                        const std::string& dn = "",
                                        const std::string& av = "",
                                        const std::string& notice = "") {
    AdventureSetup setup;
    { std::lock_guard<std::mutex> lk(data_mutex); auto it = adv_setups.find(uid); if (it != adv_setups.end()) setup = it->second; }
    std::string uid_s = std::to_string((uint64_t)uid);

    const AdvRegion* reg = setup.region_key.empty() ? nullptr : find_adv_region(setup.region_key);
    std::string user_tag = dn.empty() ? uid_s : dn;
    std::string desc = "## 🗺️ 探險準備\n";
    if (!notice.empty()) desc += notice + "\n\n";
    desc += "🌍 **探險地區**：" + (reg ? reg->emoji + " " + reg->name : "（未選擇）") + "\n";
    desc += "⏰ **探險時長**：";
    if (setup.duration_hours > 0) desc += std::to_string(setup.duration_hours) + " 小時";
    else desc += "（未選擇）";
    desc += "\n💰 **探險資金**：";
    if (setup.funds >= 0) desc += std::to_string(setup.funds) + " 碼";
    else desc += "（未設定）";
    desc += "\n🐾 **探險夥伴**：";
    if (setup.partner == 1) desc += "🐾 帶寵物";
    else if (setup.partner == 0) desc += "🚫 不帶寵物";
    else desc += "（未選擇）";

    bool region_reqs_met = true;
    if (reg && adv_region_requires_pet6h(reg->key)) {
        region_reqs_met = setup.duration_hours >= 6 && setup.partner == 1;
        if (!region_reqs_met) desc += "\n\n⚠️ **此地區需要攜帶寵物且時長至少 6 小時才能出發。**";
    }
    bool all_set = reg && setup.duration_hours > 0 && setup.funds >= 0 && setup.partner >= 0 && region_reqs_met;
    if (all_set) {
        int pet_stage = 0;
        if (setup.partner == 1) {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto pit = pet_data.find(uid);
            if (pit != pet_data.end()) pet_stage = pit->second.stage;
        }
        int prog = calc_adv_progress(setup.duration_hours, setup.funds, pet_stage);
        if (setup.star_boost) prog += 10;
        desc += "\n\n✨ **預計探索度：" + std::to_string(prog) + "**";
        if (setup.star_boost) desc += "（含星星 +10）";
        if (reg) {
            AdvPreview prev = calc_adv_preview(reg->key, prog);
            if (setup.star_boost) {
                desc += "\n📊 有 **0%** 機率無法獲得戰利品（星星保證重骰到有為止），目前最有可能前往 **" + prev.likely_tier + "**";
            } else {
                desc += "\n📊 有 **" + std::to_string((int)std::lround(prev.miss_pct))
                      + "%** 機率無法獲得戰利品，目前最有可能前往 **" + prev.likely_tier + "**";
            }
        }
    }
    desc += "\n\n⚠️ **注意：探索度並不是越高越好。**";
    desc += "\n\n-# 👤 " + user_tag;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(v2_section(desc, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row1; row1.set_type(dpp::cot_action_row);
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🌍 探險地區").set_id("adv_set_region_" + uid_s)
        .set_style(reg ? dpp::cos_success : dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⏰ 探險時長").set_id("adv_set_duration_" + uid_s)
        .set_style(setup.duration_hours > 0 ? dpp::cos_success : dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 探險資金").set_id("adv_set_funds_" + uid_s)
        .set_style(setup.funds >= 0 ? dpp::cos_success : dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐾 探險夥伴").set_id("adv_set_partner_" + uid_s)
        .set_style(setup.partner >= 0 ? dpp::cos_success : dpp::cos_secondary));
    msg.add_component_v2(row1);


    int star_count = 0;
    { std::lock_guard<std::mutex> lk(data_mutex);
      auto iit = inventory_data.find(uid);
      if (iit != inventory_data.end()) {
          auto kit = iit->second.find("star_unknown");
          if (kit != iit->second.end()) star_count = kit->second;
      }
    }
    bool star_boosted = setup.star_boost;
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚀 開始探索").set_id("adv_start_" + uid_s)
        .set_style(dpp::cos_primary).set_disabled(!all_set));
    {
        std::string star_label = star_boosted ? "✅ 已投入星星（點擊取消）" : ("⭐ 投入星星（" + std::to_string(star_count) + "）");
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(star_label).set_id("adv_star_boost_" + uid_s)
            .set_style(star_boosted ? dpp::cos_success : dpp::cos_secondary)
            .set_disabled(!star_boosted && star_count <= 0));
    }
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📚 收藏").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);
    return msg;
}

// ─── Active / done page ────────────────────────────────────────────────────────

static dpp::message make_adv_active_msg(dpp::snowflake uid,
                                         const std::string& dn = "",
                                         const std::string& av = "") {
    AdventureGame g; bool found = false;
    { std::lock_guard<std::mutex> lk(data_mutex); auto it = adv_games.find(uid); if (it != adv_games.end()) { g = it->second; found = true; } }
    if (!found) return make_adv_setup_msg(uid, dn, av);
    std::string uid_s = std::to_string((uint64_t)uid);
    const AdvRegion* reg = find_adv_region(g.region_key);
    bool done = g.end_time <= time(nullptr);
    int progress = calc_adv_progress(g.duration_hours, g.funds, g.pet_stage);

    std::string user_tag = dn.empty() ? uid_s : dn;
    uint32_t accent_color = done ? dpp::utility::rgb(0xF1, 0xC4, 0x0F) : dpp::utility::rgb(0x34, 0x98, 0xDB);
    std::string title = done ? ("## 🎉 探險完成！" + (reg ? reg->emoji + " " + reg->name : ""))
                              : ("## 🗺️ 探險中！" + (reg ? reg->emoji + " " + reg->name : ""));

    std::string desc = title + "\n";
    desc += "▸ 探索時長：" + std::to_string(g.duration_hours) + " 小時\n";
    desc += "▸ 探險資金：" + std::to_string(g.funds) + " 碼\n";
    desc += "▸ 探險夥伴：" + (g.pet_along ? std::string("🐾 寵物同行") : std::string("🚫 無寵物")) + "\n";
    if (!done) {
        desc += "\n📅 完成時間：" + adv_fmt_clock(g.end_time) + "\n";
        desc += "⏳ 剩餘時間：" + adv_fmt_remain(g.end_time) + "\n\n";
        time_t now = time(nullptr);
        double ratio = (now - g.start_time) / std::max(1.0, (double)(g.end_time - g.start_time));
        ratio = std::max(0.0, std::min(1.0, ratio));
        desc += adv_progress_bar(ratio) + " **" + std::to_string((int)(ratio * 100)) + "%**";
    } else {
        desc += "\n✅ 探險已結束，請收取結果！";
    }
    desc += "\n\n-# 👤 " + user_tag;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(accent_color);
    container.add_component_v2(v2_section(desc, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    if (done) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📬 收取結果").set_id("adv_collect_" + uid_s).set_style(dpp::cos_success));
    } else {
        std::string cancel_label = "❌ 取消探索";
        if (g.funds > 0) cancel_label += "（退還 " + std::to_string((int64_t)(g.funds * 0.6 + 0.5)) + " 碼）";
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(cancel_label).set_id("adv_cancel_" + uid_s).set_style(dpp::cos_danger));
    }
    if (!done) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🔄 重整").set_id("adv_refresh_" + uid_s).set_style(dpp::cos_secondary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(g.notify_on_finish ? "🔔 通知" : "🔕 通知")
            .set_id("adv_notify_toggle_" + uid_s)
            .set_style(g.notify_on_finish ? dpp::cos_success : dpp::cos_secondary));
    }
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

static dpp::message make_adv_main_msg(dpp::snowflake uid, const std::string& dn = "", const std::string& av = "") {
    bool has_active = false;
    { std::lock_guard<std::mutex> lk(data_mutex); has_active = adv_games.count(uid) > 0; }
    if (has_active) return make_adv_active_msg(uid, dn, av);
    return make_adv_setup_msg(uid, dn, av);
}

// ─── Sub-page: region select ──────────────────────────────────────────────────

static dpp::message make_adv_region_select_msg(dpp::snowflake uid, const std::string& dn = "", const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string user_tag = dn.empty() ? uid_s : dn;
    std::string content = "## 🌍 選擇探險地區\n";
    for (auto& r : ADV_REGIONS)
        content += r.emoji + " **" + r.name + "**\n";
    content += "\n-# 👤 " + user_tag;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(v2_section(content, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& r : ADV_REGIONS)
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(r.emoji + " " + r.name)
            .set_id("adv_region_" + uid_s + "_" + r.key)
            .set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

// ─── Sub-page: duration select ────────────────────────────────────────────────

static dpp::message make_adv_duration_select_msg(dpp::snowflake uid, const std::string& dn = "", const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string user_tag = dn.empty() ? uid_s : dn;

    AdventureSetup setup;
    { std::lock_guard<std::mutex> lk(data_mutex); auto it = adv_setups.find(uid); if (it != adv_setups.end()) setup = it->second; }
    bool min6 = adv_region_requires_pet6h(setup.region_key);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    std::string dur_desc = min6
        ? "## ⏰ 選擇探險時長\n此地區最少 **6 小時**，最多 **12 小時**。\n\n-# 👤 " + user_tag
        : "## ⏰ 選擇探險時長\n最少 **2 小時**，最多 **12 小時**。\n\n-# 👤 " + user_tag;
    container.add_component_v2(v2_section(dur_desc, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row1; row1.set_type(dpp::cot_action_row);
    for (int h : {2, 3, 4, 5, 6})
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(std::to_string(h) + "小時")
            .set_id("adv_dur_" + uid_s + "_" + std::to_string(h))
            .set_style(dpp::cos_primary).set_disabled(min6 && h < 6));
    msg.add_component_v2(row1);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    for (int h : {7, 8, 9, 10, 11})
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(std::to_string(h) + "小時")
            .set_id("adv_dur_" + uid_s + "_" + std::to_string(h))
            .set_style(dpp::cos_primary));
    msg.add_component_v2(row2);

    dpp::component row3; row3.set_type(dpp::cot_action_row);
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("12小時").set_id("adv_dur_" + uid_s + "_12").set_style(dpp::cos_primary));
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row3);
    return msg;
}

// ─── Sub-page: funds select ───────────────────────────────────────────────────

static dpp::message make_adv_funds_select_msg(dpp::snowflake uid, const std::string& dn = "", const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string user_tag = dn.empty() ? uid_s : dn;
    int64_t cur = get_chips(uid);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(v2_section(
        "## 💰 選擇探險資金\n"
        "投入的資金在探索完成前無法取回；若中途取消探索，只會退還 60%。\n上限 10000 碼。\n\n"
        "目前錢包：**" + std::to_string(cur) + "** 碼\n\n-# 👤 " + user_tag, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row1; row1.set_type(dpp::cot_action_row);
    for (int64_t amt : {(int64_t)0, (int64_t)2500, (int64_t)5000, (int64_t)7500, (int64_t)10000}) {
        bool disabled = (amt > 0 && cur < amt);
        std::string lbl = (amt == 0) ? "0碼" : (std::to_string(amt) + "碼+" + std::to_string((int)(amt/250)));
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(lbl)
            .set_id("adv_funds_" + uid_s + "_" + std::to_string(amt))
            .set_style(disabled ? dpp::cos_secondary : dpp::cos_primary)
            .set_disabled(disabled));
    }
    msg.add_component_v2(row1);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);
    return msg;
}

// ─── Sub-page: partner select ─────────────────────────────────────────────────

static dpp::message make_adv_partner_select_msg(dpp::snowflake uid, const std::string& dn = "", const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    Pet pet; bool has_pet = false; bool pet_busy = false;
    AdventureSetup setup;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end() && it->second.stage > 0) {
            pet = it->second; has_pet = true;
            pet_busy = (pet.work_task > 0 && pet.work_end > time(nullptr));
        }
        // Also blocked if already on adventure with pet
        auto ai = adv_games.find(uid);
        if (ai != adv_games.end() && ai->second.pet_along) pet_busy = true;
        auto sit = adv_setups.find(uid); if (sit != adv_setups.end()) setup = sit->second;
    }
    bool require_pet = adv_region_requires_pet6h(setup.region_key);
    std::string user_tag = dn.empty() ? uid_s : dn;
    std::string desc = "## 🐾 選擇探險夥伴\n探險期間寵物無法打工。\n\n";
    if (require_pet) desc += "⚠️ 此地區必須攜帶寵物才能出發。\n";
    if (!has_pet) desc += "⚠️ 你沒有可以出行的寵物。\n";
    else if (pet_busy) desc += "⚠️ 寵物打工中或已在探險，無法攜帶！\n";
    desc += "\n-# 👤 " + user_tag;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(v2_section(desc, av));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    bool can_bring = has_pet && !pet_busy;
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐾 帶寵物").set_id("adv_partner_" + uid_s + "_1")
        .set_style(dpp::cos_primary).set_disabled(!can_bring));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚫 不帶寵物").set_id("adv_partner_" + uid_s + "_0")
        .set_style(dpp::cos_secondary).set_disabled(require_pet));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

// ─── Button handler ────────────────────────────────────────────────────────────

static void handle_adv_button(const dpp::button_click_t& ev) {
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;
    std::string dn = ev.command.member.get_nickname();
    if (dn.empty()) dn = ev.command.get_issuing_user().global_name.empty()
                         ? ev.command.get_issuing_user().username
                         : ev.command.get_issuing_user().global_name;
    std::string av = ev.command.get_issuing_user().get_avatar_url();
    std::string uid_s = std::to_string((uint64_t)uid);

    if (cid == "adv_main_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_adv_main_msg(uid, dn, av)); return;
    }
    if (cid == "adv_collection_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_collection_msg(uid, dn, av)); return;
    }
    // 一般收藏翻頁: adv_col_normal_<uid>_<page>
    if (cid.rfind("adv_col_normal_" + uid_s + "_", 0) == 0) {
        int page = 1;
        try { page = std::stoi(cid.substr(16 + uid_s.size())); } catch (...) {}
        ev.reply(dpp::ir_update_message, make_normal_col_msg(uid, dn, av, page)); return;
    }
    // 限定收藏: adv_col_limited_<uid>
    if (cid == "adv_col_limited_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_limited_col_msg(uid, dn, av)); return;
    }
    // 限定收藏翻頁: adv_col_limited_page_<uid>_<page>
    if (cid.rfind("adv_col_limited_page_" + uid_s + "_", 0) == 0) {
        int page = 1;
        try { page = std::stoi(cid.substr(std::string("adv_col_limited_page_" + uid_s + "_").size())); } catch (...) {}
        ev.reply(dpp::ir_update_message, make_limited_col_msg(uid, dn, av, page)); return;
    }
    // 收藏品售出頁: adv_col_sell_<uid>
    if (cid == "adv_col_sell_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_col_sell_msg(uid, dn, av)); return;
    }
    // 收藏品售出頁翻頁: adv_col_sell_page_<uid>_<page>
    if (cid.rfind("adv_col_sell_page_" + uid_s + "_", 0) == 0) {
        int page = 1;
        try { page = std::stoi(cid.substr(std::string("adv_col_sell_page_" + uid_s + "_").size())); } catch (...) {}
        ev.reply(dpp::ir_update_message, make_col_sell_msg(uid, dn, av, page)); return;
    }
    // 售出單一收藏品: adv_col_sellitem_<uid>_<key>
    if (cid.rfind("adv_col_sellitem_" + uid_s + "_", 0) == 0) {
        std::string key = cid.substr(std::string("adv_col_sellitem_" + uid_s + "_").size());
        auto pit = COL_SELL_PRICE.find(key);
        if (pit != COL_SELL_PRICE.end()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& inv = inventory_data[uid];
            auto it = inv.find(key);
            if (it != inv.end() && it->second > 0) {
                it->second--;
                chip_data[uid].chips += pit->second;
            }
        }
        save_inventory(); save_chips();
        ev.reply(dpp::ir_update_message, make_col_sell_msg(uid, dn, av)); return;
    }
    // 售出多餘（保留每種1件）: adv_col_sellexcess_<uid>_<tier>（low/mid/high）— 不會打破套組，不需確認
    if (cid.rfind("adv_col_sellexcess_" + uid_s + "_", 0) == 0) {
        std::string tier = cid.substr(std::string("adv_col_sellexcess_" + uid_s + "_").size());
        int64_t target_price = (tier == "low") ? 1000 : (tier == "mid") ? 2000 : 3000;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& inv = inventory_data[uid];
            int64_t total = 0;
            for (auto& [key, price] : COL_SELL_PRICE) {
                if (price != target_price) continue;
                auto it = inv.find(key);
                if (it != inv.end() && it->second > 1) {
                    int excess = it->second - 1;
                    total += (int64_t)excess * price;
                    it->second = 1;
                }
            }
            chip_data[uid].chips += total;
        }
        save_inventory(); save_chips();
        ev.reply(dpp::ir_update_message, make_col_sell_msg(uid, dn, av)); return;
    }
    // 园园的風險骰子：一天限用 2 次
    if (cid == "adv_risk_dice_" + uid_s) {
        std::string notice;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto iit = inventory_data.find(uid);
            if (iit == inventory_data.end() || iit->second.count("col_bb_risk_dice") == 0
                || iit->second.at("col_bb_risk_dice") <= 0) {
                notice = "❌ 你沒有园园的風險骰子！";
            } else {
                auto& cd = chip_data[uid];
                time_t now = time(nullptr);
                if (!risk_dice_same_day(now, cd.risk_dice_day)) cd.risk_dice_uses = 0;
                if (cd.risk_dice_uses >= 2) {
                    notice = "❌ 今天的風險骰子已經用完了（上限 2 次）！";
                } else {
                    cd.risk_dice_uses++;
                    cd.risk_dice_day = now;
                    static std::mt19937 risk_dice_rng(std::random_device{}());
                    int roll = std::uniform_int_distribution<int>(1, 100)(risk_dice_rng);
                    int64_t delta; std::string outcome;
                    if      (roll <= 1)  { delta = -5000; outcome = "😱 大凶！"; }
                    else if (roll <= 11) { delta =  5000; outcome = "🎉 大吉！"; }
                    else                 { delta =  -500; outcome = "😐 小虧。"; }
                    cd.chips += delta;
                    notice = outcome + "（" + (delta >= 0 ? "+" : "") + std::to_string(delta) + " 碼，今日已用 "
                           + std::to_string(cd.risk_dice_uses) + "/2）";
                }
            }
        }
        save_chips();
        ev.reply(dpp::ir_update_message, make_bag_special_msg(uid, dn, av, notice)); return;
    }
    // 貓哥的戀愛教典：付 2000 碼解鎖一次性可交易
    if (cid == "adv_lovebook_unlock_" + uid_s) {
        std::string notice;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto iit = inventory_data.find(uid);
            if (iit == inventory_data.end() || iit->second.count("col_rd_lovebook") == 0
                || iit->second.at("col_rd_lovebook") <= 0) {
                notice = "❌ 你沒有貓哥的戀愛教典！";
            } else if (iit->second.count("_lovebook_unlocked") && iit->second.at("_lovebook_unlocked") > 0) {
                notice = "❌ 已經是解鎖狀態了！";
            } else if (chip_data[uid].chips < 2000) {
                notice = "❌ 籌碼不足，需要 2000 碼！";
            } else {
                chip_data[uid].chips -= 2000;
                inventory_data[uid]["_lovebook_unlocked"] = 1;
                notice = "✅ 已解鎖一次性可交易！交易完成後會自動恢復不可交易。";
            }
        }
        save_chips(); save_inventory();
        ev.reply(dpp::ir_update_message, make_bag_special_msg(uid, dn, av, notice)); return;
    }
    // 天元的舔狗密笈：每日領取一杯高級強效咖啡（被詛咒版，本人無法使用/售出）
    if (cid == "adv_simpmanual_claim_" + uid_s) {
        std::string notice;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto iit = inventory_data.find(uid);
            if (iit == inventory_data.end() || iit->second.count("col_rd_simpmanual") == 0
                || iit->second.at("col_rd_simpmanual") <= 0) {
                notice = "❌ 你沒有天元的舔狗密笈！";
            } else {
                time_t now = time(nullptr);
                auto& inv = inventory_data[uid];
                auto cit = inv.find("_coffee_last_claim");
                bool already = (cit != inv.end() && risk_dice_same_day(now, (time_t)cit->second));
                if (already) {
                    notice = "❌ 今天已經領取過了！";
                } else {
                    inv["_coffee_last_claim"] = (int)now;
                    inv["recover_fatigue_cursed"]++;
                    notice = "☕ 領取了一杯被詛咒的高級強效咖啡！你無法使用或售出，交易給別人後對方就能正常使用。";
                }
            }
        }
        save_inventory();
        ev.reply(dpp::ir_update_message, make_bag_special_msg(uid, dn, av, notice)); return;
    }
    // 左邊畫個龍的柴犬百科全書：每週4次隨機抽取虛擬商店 <3000 道具（25%失敗），全域計次
    if (cid == "adv_dogbook_draw_" + uid_s) {
        std::string notice;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto iit = inventory_data.find(uid);
            if (iit == inventory_data.end() || iit->second.count("col_rd_dogbook") == 0
                || iit->second.at("col_rd_dogbook") <= 0) {
                notice = "❌ 你沒有柴犬百科全書！";
            } else {
                time_t now_t = time(nullptr); struct tm tmw = {}; localtime_s(&tmw, &now_t);
                char wbuf[8]; strftime(wbuf, sizeof(wbuf), "%G%V", &tmw);
                int cur_week = std::stoi(wbuf);
                if (g_dogbook_week != cur_week) { g_dogbook_week = cur_week; g_dogbook_uses_left = 4; }
                if (g_dogbook_uses_left <= 0) {
                    notice = "❌ 本週次數已用完（上限 4 次）！";
                } else {
                    g_dogbook_uses_left--;
                    static std::mt19937 dogbook_rng(std::random_device{}());
                    if (std::uniform_int_distribution<int>(1, 100)(dogbook_rng) <= 25) {
                        notice = "😢 抽取失敗！（本週剩 " + std::to_string(g_dogbook_uses_left) + "/4 次）";
                    } else {
                        std::vector<const VirtualShopItem*> pool;
                        for (auto& vi : VIRTUAL_ITEMS)
                            if (vi.price > 0 && vi.price < 3000) pool.push_back(&vi);
                        if (pool.empty()) {
                            notice = "😢 商店裡沒有符合條件的道具！";
                        } else {
                            auto* got = pool[std::uniform_int_distribution<int>(0, (int)pool.size()-1)(dogbook_rng)];
                            inventory_data[uid][got->key]++;
                            notice = "🎉 抽到了 **" + got->name + "**！（本週剩 " + std::to_string(g_dogbook_uses_left) + "/4 次）";
                        }
                    }
                }
            }
        }
        save_inventory(); save_dogbook();
        ev.reply(dpp::ir_update_message, make_bag_special_msg(uid, dn, av, notice)); return;
    }
    if (cid == "adv_set_region_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_adv_region_select_msg(uid, dn, av)); return;
    }
    if (cid == "adv_set_duration_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_adv_duration_select_msg(uid, dn, av)); return;
    }
    if (cid == "adv_set_funds_" + uid_s) {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            adv_setups[uid].setup_msg_id = ev.command.msg.id;
            adv_setups[uid].setup_ch_id  = ev.command.channel_id;
        }
        dpp::interaction_modal_response modal("adv_funds_modal_" + uid_s, "設定探險資金");
        modal.add_component(dpp::component()
            .set_type(dpp::cot_text)
            .set_label("資金（0 ~ 10000 碼）")
            .set_id("funds_input")
            .set_min_length(1).set_max_length(5)
            .set_placeholder("輸入 0 ~ 10000")
            .set_required(true).set_text_style(dpp::text_short));
        ev.dialog(modal);
        return;
    }
    if (cid == "adv_set_partner_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_adv_partner_select_msg(uid, dn, av)); return;
    }

    // 選擇地區
    if (cid.rfind("adv_region_" + uid_s + "_", 0) == 0) {
        std::string key = cid.substr(12 + uid_s.size());
        if (!find_adv_region(key)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 無效地區！").set_flags(dpp::m_ephemeral)); return; }
        { std::lock_guard<std::mutex> lk(data_mutex); adv_setups[uid].region_key = key; }
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av)); return;
    }

    // 選擇時長
    if (cid.rfind("adv_dur_" + uid_s + "_", 0) == 0) {
        int h = 0;
        try { h = std::stoi(cid.substr(9 + uid_s.size())); } catch (...) {}
        if (h < 2 || h > 12) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 時長須為 2~12 小時！").set_flags(dpp::m_ephemeral)); return; }
        bool too_short = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            too_short = (h < 6 && adv_region_requires_pet6h(adv_setups[uid].region_key));
            if (!too_short) adv_setups[uid].duration_hours = h;
        }
        if (too_short) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此地區最少需要 6 小時！").set_flags(dpp::m_ephemeral)); return; }
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av)); return;
    }

    // 選擇資金
    if (cid.rfind("adv_funds_" + uid_s + "_", 0) == 0) {
        int64_t amt = 0;
        try { amt = std::stoll(cid.substr(11 + uid_s.size())); } catch (...) {}
        if (amt < 0 || amt > 10000) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 資金須為 0~10000 碼！").set_flags(dpp::m_ephemeral)); return; }
        int64_t bal = get_chips(uid);
        if (amt > bal) amt = std::max((int64_t)0, bal);
        { std::lock_guard<std::mutex> lk(data_mutex); adv_setups[uid].funds = amt; }
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av)); return;
    }

    // 選擇夥伴
    if (cid == "adv_partner_" + uid_s + "_0" || cid == "adv_partner_" + uid_s + "_1") {
        bool with_pet = (cid.back() == '1');
        if (with_pet) {
            bool ok = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = pet_data.find(uid);
              if (it != pet_data.end() && it->second.stage > 0) {
                  auto& p = it->second;
                  ok = (p.work_task == 0 && p.onsen_end == 0);
              }
            }
            if (!ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 寵物必須空閒才能帶去探險！\n（打工中、有未領取的打工、泡溫泉中皆無法出發）").set_flags(dpp::m_ephemeral)); return; }
        } else {
            bool require_pet = false;
            { std::lock_guard<std::mutex> lk(data_mutex); require_pet = adv_region_requires_pet6h(adv_setups[uid].region_key); }
            if (require_pet) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此地區必須攜帶寵物！").set_flags(dpp::m_ephemeral)); return; }
        }
        { std::lock_guard<std::mutex> lk(data_mutex); adv_setups[uid].partner = with_pet ? 1 : 0; }
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av)); return;
    }

    // 開始探索
    if (cid == "adv_star_boost_" + uid_s) {
        bool changed = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto& setup = adv_setups[uid];
          if (setup.star_boost) {
              // 取消：退還星星
              inventory_data[uid]["star_unknown"]++;
              setup.star_boost = false;
              changed = true;
          } else {
              // 投入：消耗星星
              auto& inv = inventory_data[uid];
              auto it = inv.find("star_unknown");
              if (it != inv.end() && it->second > 0) {
                  it->second--;
                  setup.star_boost = true;
                  changed = true;
              }
          }
        }
        if (!changed) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 沒有未知的星星！").set_flags(dpp::m_ephemeral)); return;
        }
        save_inventory();
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av)); return;
    }

    if (cid == "adv_start_" + uid_s) {
        AdventureSetup setup;
        { std::lock_guard<std::mutex> lk(data_mutex); auto it = adv_setups.find(uid); if (it != adv_setups.end()) setup = it->second; }
        if (setup.region_key.empty() || setup.duration_hours <= 0 || setup.funds < 0 || setup.partner < 0) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 請先完成所有設定！").set_flags(dpp::m_ephemeral)); return;
        }
        if (adv_region_requires_pet6h(setup.region_key) && (setup.duration_hours < 6 || setup.partner != 1)) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此地區必須攜帶寵物且時長至少 6 小時！").set_flags(dpp::m_ephemeral)); return;
        }
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (adv_games.count(uid)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你已有進行中的探險！").set_flags(dpp::m_ephemeral)); return; }
        }
        if (setup.funds > 0 && get_chips(uid) < setup.funds) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 籌碼不足！需要 **" + std::to_string(setup.funds) + "** 碼。").set_flags(dpp::m_ephemeral)); return;
        }
        int pet_stage = 0;
        if (setup.partner == 1) {
            bool ok = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = pet_data.find(uid);
              if (it != pet_data.end() && it->second.stage > 0) {
                  auto& p = it->second;
                  ok = (p.work_task == 0 && p.onsen_end == 0);
                  pet_stage = p.stage;
              }
            }
            if (!ok) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 寵物必須空閒才能帶去探險！\n（打工中、有未領取的打工、泡溫泉中皆無法出發）").set_flags(dpp::m_ephemeral));
                return;
            }
        }
        if (setup.funds > 0) { add_chips(uid, -setup.funds); save_chips(); }
        AdventureGame g;
        g.uid = uid; g.region_key = setup.region_key;
        g.duration_hours = setup.duration_hours; g.funds = setup.funds;
        g.pet_along = (setup.partner == 1);
        g.pet_stage = pet_stage;
        g.notify_on_finish = setup.notify_on_finish;
        g.star_boost = setup.star_boost;
        g.start_time = time(nullptr);
        int64_t adv_secs = (int64_t)g.duration_hours * 3600LL;
        { std::lock_guard<std::mutex> lk(data_mutex);
          // 探索時長乘區：每組地區中級套組 -1%（最多3組）+ BB博物館放大鏡 -5%，加總後一次套用（無條件進位）
          double dur_delta = -0.01 * col_adv_reduction_count(uid);
          if (col_has_bb_magnifier(uid)) dur_delta += -0.05;
          adv_secs = (int64_t)std::ceil(adv_secs * std::max(0.0, 1.0 + dur_delta));
          g.end_time = g.start_time + adv_secs;
          adv_games[uid] = g; adv_setups.erase(uid);
        }
        save_adv_games();
        ev.reply(dpp::ir_update_message, make_adv_active_msg(uid, dn, av)); return;
    }

    // 重整：刷新目前進度（同一則訊息，不另開新視窗）
    if (cid == "adv_refresh_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_adv_active_msg(uid, dn, av)); return;
    }

    // 通知開關：探險完成時私訊通知
    if (cid == "adv_notify_toggle_" + uid_s) {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = adv_games.find(uid);
            if (it != adv_games.end()) {
                it->second.notify_on_finish = !it->second.notify_on_finish;
                adv_setups[uid].notify_on_finish = it->second.notify_on_finish;
            }
        }
        save_adv_games();
        ev.reply(dpp::ir_update_message, make_adv_active_msg(uid, dn, av)); return;
    }

    // 取消探索：已投入的資金退還 60%
    if (cid == "adv_cancel_" + uid_s) {
        int64_t refund = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = adv_games.find(uid);
            if (it != adv_games.end() && it->second.funds > 0)
                refund = (int64_t)(it->second.funds * 0.6 + 0.5);
            adv_games.erase(uid);
        }
        if (refund > 0) { add_chips(uid, refund); save_chips(); }
        save_adv_games();
        std::string notice = refund > 0
            ? "❌ 已取消探索，退還 **" + std::to_string(refund) + "** 碼（投入資金的 60%）。"
            : "";
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av, notice)); return;
    }

    // 收取結果
    if (cid == "adv_collect_" + uid_s) {
        AdventureGame g; bool found = false;
        { std::lock_guard<std::mutex> lk(data_mutex); auto it = adv_games.find(uid); if (it != adv_games.end()) { g = it->second; found = true; } }
        if (!found || g.end_time > time(nullptr)) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 探險尚未結束！").set_flags(dpp::m_ephemeral)); return;
        }
        int progress = calc_adv_progress(g.duration_hours, g.funds, g.pet_stage);
        if (g.star_boost) progress += 10;
        std::string item_key, item_key2;
        bool item_added = false;
        bool refund_triggered = false;
        bool star_rerolled = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& inv = inventory_data[uid];
            // 限定收藏品全球唯一：先找出已被拿走的，骰的時候直接排除、在剩下道具間重骰
            std::set<std::string> claimed_limited;
            for (auto& lk2 : LIMITED_COL_ITEMS) {
                for (auto& [oid, oinv] : inventory_data) {
                    auto jt = oinv.find(lk2);
                    if (jt != oinv.end() && jt->second > 0) { claimed_limited.insert(lk2); break; }
                }
            }
            // 全球限量道具（例如全球僅 5 份）：算進已合成消耗掉的份數，達上限就排除，合成不會釋放新名額
            for (auto& [lk2, rule] : LIMITED_MAX_COUNT) {
                int64_t total = 0;
                for (auto& [oid, oinv] : inventory_data) {
                    auto jt = oinv.find(lk2);
                    if (jt != oinv.end()) total += jt->second;
                    if (!rule.crafted_key.empty()) {
                        auto jt2 = oinv.find(rule.crafted_key);
                        if (jt2 != oinv.end()) total += (int64_t)jt2->second * rule.craft_ratio;
                    }
                }
                if (total >= rule.cap) claimed_limited.insert(lk2);
            }

            // 骰之前先記錄目前持有的假髮／內衣（新骰到的這次不算數）
            auto owned = [&](const std::string& k) { auto it = inv.find(k); return it != inv.end() ? it->second : 0; };
            int wig_broken   = owned("col_bb_wig_broken");
            int wig_full     = owned("col_bb_wig_full");
            int undies_broken = owned("col_bb_undies_broken");
            int undies_full   = owned("col_bb_undies_full");

            item_key = roll_adv_loot(g.region_key, progress, claimed_limited);
            if (item_key.empty() && g.star_boost) {
                for (int _r = 0; _r < 50 && item_key.empty(); _r++)
                    item_key = roll_adv_loot(g.region_key, progress, claimed_limited);
                star_rerolled = true;
            }
            if (!item_key.empty()) { inv[item_key]++; item_added = true; }

            static std::mt19937 bb_effect_rng(std::random_device{}());
            // Zoey的假髮：機率額外骰一次戰利品（戰損版每個+1%，完整版每個+10%，可疊加）
            int double_pct = wig_broken * 1 + wig_full * 10;
            if (double_pct > 0 && std::uniform_int_distribution<int>(1, 100)(bb_effect_rng) <= double_pct) {
                item_key2 = roll_adv_loot(g.region_key, progress, claimed_limited);
                if (!item_key2.empty()) { inv[item_key2]++; item_added = true; }
            }
            // 皮包的內衣：機率返還這次探索花費的資金（戰損版每個+2%，完整版每個+20%，可疊加）
            int refund_pct = undies_broken * 2 + undies_full * 20;
            if (g.funds > 0 && refund_pct > 0 && std::uniform_int_distribution<int>(1, 100)(bb_effect_rng) <= refund_pct)
                refund_triggered = true;

            adv_games.erase(uid);
            // 恢復上次設定，讓玩家可以馬上再出發（資金不足時出發時才會拒絕）
            AdventureSetup& ns  = adv_setups[uid];
            ns.region_key       = g.region_key;
            ns.duration_hours   = g.duration_hours;
            ns.funds            = g.funds;
            ns.partner          = g.pet_along ? 1 : 0;
            ns.notify_on_finish = g.notify_on_finish;
        }
        if (item_added) save_inventory();
        save_adv_games();
        if (refund_triggered) add_chips(uid, g.funds);

        const AdvRegion* reg = find_adv_region(g.region_key);
        auto* vi = find_virtual_item(item_key);
        std::string item_name = vi ? vi->name : "";

        std::string user_tag_r = dn.empty() ? uid_s : dn;
        std::string desc_r;
        desc_r += "## 🎉 探險結果！" + (reg ? reg->emoji + " " + reg->name : "") + "\n";
        desc_r += "▸ 地區：" + (reg ? reg->emoji + " " + reg->name : g.region_key) + "\n";
        desc_r += "▸ 時長：" + std::to_string(g.duration_hours) + " 小時\n";
        desc_r += "▸ 資金：" + std::to_string(g.funds) + " 碼\n";
        desc_r += "▸ 夥伴：" + (g.pet_along ? std::string("🐾 寵物同行") : std::string("無")) + "\n";
        if (g.star_boost) desc_r += "▸ 星星加成：✅ 探索度+10" + std::string(star_rerolled ? "、空包彈重骰一次" : "") + "\n";
        desc_r += "\n";
        if (item_key.empty())
            desc_r += "📦 這次探險沒有找到蒐藏品...";
        else {
            desc_r += "🎁 獲得蒐藏品：**" + item_name + "** ×1！";
            if (vi && !vi->desc.empty()) desc_r += "\n　*" + vi->desc + "*";
        }
        if (!item_key2.empty()) {
            auto* vi2 = find_virtual_item(item_key2);
            desc_r += "\n✨ 假髮加成觸發，額外骰到：**" + (vi2 ? vi2->name : item_key2) + "** ×1！";
        }
        if (refund_triggered)
            desc_r += "\n💰 內衣加成觸發，返還本次探索資金 **" + std::to_string(g.funds) + "** 碼！";
        desc_r += "\n\n-# 👤 " + user_tag_r;

        dpp::component ct_r;
        ct_r.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF1, 0xC4, 0x0F));
        ct_r.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(desc_r));
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🗺️ 再次探險").set_id("adv_main_" + uid_s).set_style(dpp::cos_primary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📚 收藏").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        dpp::message msg; msg.set_flags(dpp::m_using_components_v2);
        msg.add_component_v2(ct_r); msg.add_component_v2(row);
        ev.reply(dpp::ir_update_message, msg); return;
    }
}
