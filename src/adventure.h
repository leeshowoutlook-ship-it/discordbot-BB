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
                {"",                    43},
                {"col_gwl_popsicle",    30},
                {"col_bwl_cake",        25},
                {"col_phone_tianxin",    2},
            }},
            { 61, 32, {
                {"col_dwl_tiramisu",    41},
                {"col_rwl_velvet",      41},
                {"",                    16},
                {"col_bath_huaxuan",     2},
            }},
            {100, 31, {
                {"",                    35},
                {"col_awl_avocado",     21},
                {"col_sqwl_brownie",    21},
                {"col_ywl_caramel",     21},
                {"col_rod_zoey",         2},
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
};

static const AdvRegion* find_adv_region(const std::string& key) {
    for (auto& r : ADV_REGIONS) if (r.key == key) return &r;
    return nullptr;
}

// Collection display regions (3 pages; empty adv_key = locked/coming soon)
struct ColDisplayRegion { std::string adv_key; std::string name; std::string emoji; };
static const std::vector<ColDisplayRegion> COL_DISPLAY_REGIONS = {
    {"mushroom_kingdom",   "菇菇王國",   "🍄"},
    {"water_spirit_cave",  "綠水靈洞窟", "💧"},
    {"ghost_graveyard",    "亡魂墓地",   "💀"},
};

static const std::set<std::string> LIMITED_COL_ITEMS = {
    "col_yaya_bounty", "col_slim_wallet", "col_fat_wallet",
    "col_phone_tianxin", "col_bath_huaxuan", "col_rod_zoey",
    "col_penguin_relic", "col_shark_relic", "col_koala_relic", "col_koala_autograph"
};

// ─── Progress calculation ──────────────────────────────────────────────────────

static int calc_adv_progress(int hours, int64_t funds, bool pet_along) {
    int p = hours * 4 + (int)(funds / 250);
    if (pet_along) p += 20;
    return std::min(p, 100);
}

// ─── Loot rolling ─────────────────────────────────────────────────────────────

static std::string roll_adv_loot(const std::string& region_key, int progress) {
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
    int roll = std::uniform_int_distribution<int>(1, 100)(adv_rng);
    int cum = 0;
    for (auto& item : cp.items) {
        cum += item.pct;
        if (roll <= cum) return item.key;
    }
    return "";
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
                {"start_time",     (int64_t)g.start_time},
                {"end_time",       (int64_t)g.end_time},
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
            g.start_time     = (time_t)v.value("start_time", (int64_t)0);
            g.end_time       = (time_t)v.value("end_time",   (int64_t)0);
            adv_games[g.uid] = g;
        }
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
    dpp::embed e; e.set_title("📚  收藏").set_color(0x9B59B6);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }
    e.set_description(
        "📗 **一般收藏**：各地區探險掉落的蒐藏品，未獲得者顯示 `????`。\n"
        "⭐ **限定收藏**：全球唯一的限定品，可透過 `!交易` 轉讓。\n\n"
        "目前持有　📗 一般：**" + std::to_string(normal_cnt) + "**　⭐ 限定：**" + std::to_string(limited_cnt) + "**"
    );
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📗 一般收藏").set_id("adv_col_normal_" + uid_s + "_1").set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⭐ 限定收藏").set_id("adv_col_limited_" + uid_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row);
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

    dpp::embed e;
    e.set_title("📗  一般收藏 — " + cr.emoji + " " + cr.name +
                "  [" + std::to_string(page) + "/" + std::to_string(total_pages) + "]")
     .set_color(0x27AE60);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }

    std::string desc;
    if (cr.adv_key.empty()) {
        desc = "🔒 **此地區尚未開放**，敬請期待！";
    } else {
        const AdvRegion* reg = find_adv_region(cr.adv_key);
        std::vector<std::string> keys;
        if (reg)
            for (auto& cp : reg->checkpoints)
                for (auto& entry : cp.items)
                    if (!entry.key.empty() && !LIMITED_COL_ITEMS.count(entry.key))
                        if (std::find(keys.begin(), keys.end(), entry.key) == keys.end())
                            keys.push_back(entry.key);
        if (keys.empty()) {
            desc = "此地區暫無一般收藏品。";
        } else {
            for (auto& k : keys) {
                auto* vi = find_virtual_item(k);
                int owned = 0; auto it = inv.find(k); if (it != inv.end()) owned = it->second;
                std::string id_s = vi ? "ID: " + std::to_string(vi->item_id) : k;
                if (owned > 0)
                    desc += "✅ **" + id_s + "**　" + (vi ? vi->name : k) + " ×" + std::to_string(owned) + "\n";
                else
                    desc += "❓ **" + id_s + "**　????\n";
            }
        }
    }
    e.set_description(desc);

    dpp::message msg; msg.add_embed(e);
    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("◀").set_id("adv_col_normal_" + uid_s + "_" + std::to_string(page - 1))
        .set_style(dpp::cos_secondary).set_disabled(page <= 1));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶").set_id("adv_col_normal_" + uid_s + "_" + std::to_string(page + 1))
        .set_style(dpp::cos_secondary).set_disabled(page >= total_pages));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(nav);
    return msg;
}

// Limited collection – global uniqueness display
static dpp::message make_limited_col_msg(dpp::snowflake uid,
                                          const std::string& dn, const std::string& av) {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::map<std::string,int> inv;
    std::map<std::string,bool> globally_held;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid); if (it != inventory_data.end()) inv = it->second;
        for (auto& lk2 : LIMITED_COL_ITEMS) {
            globally_held[lk2] = false;
            for (auto& [oid, oinv] : inventory_data) {
                auto jt = oinv.find(lk2);
                if (jt != oinv.end() && jt->second > 0) { globally_held[lk2] = true; break; }
            }
        }
    }

    dpp::embed e; e.set_title("⭐  限定收藏").set_color(0xF39C12);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }

    std::string desc = "限定收藏品為**全球唯一**，每種只有一件存在。\n可透過探險獲得，也可透過 `!交易` 轉讓。\n\n";
    static const std::vector<std::pair<std::string,std::string>> LIMITED_ORDER = {
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
    };
    for (auto& [key, effect] : LIMITED_ORDER) {
        auto* vi = find_virtual_item(key);
        int owned = 0; auto oit = inv.find(key); if (oit != inv.end()) owned = oit->second;
        std::string id_s = vi ? "**ID: " + std::to_string(vi->item_id) + "**" : key;
        std::string name = vi ? vi->name : key;
        bool discovered = globally_held[key];
        if (owned > 0) {
            desc += "✅ " + id_s + "　" + name + "\n　效果：" + effect + "\n\n";
        } else if (discovered) {
            desc += "🔒 " + id_s + "　" + name + "\n　效果：" + effect + "　（已被其他探險者持有）\n\n";
        } else {
            desc += "❓ " + id_s + "　" + name + "\n　效果：?????　（尚未被任何人發現）\n\n";
        }
    }
    e.set_description(desc);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row);
    return msg;
}

// ─── Setup page ───────────────────────────────────────────────────────────────

static dpp::message make_adv_setup_msg(dpp::snowflake uid,
                                        const std::string& dn = "",
                                        const std::string& av = "") {
    AdventureSetup setup;
    { std::lock_guard<std::mutex> lk(data_mutex); auto it = adv_setups.find(uid); if (it != adv_setups.end()) setup = it->second; }
    std::string uid_s = std::to_string((uint64_t)uid);

    dpp::embed e; e.set_title("🗺️  探險準備").set_color(0x2ECC71);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }

    const AdvRegion* reg = setup.region_key.empty() ? nullptr : find_adv_region(setup.region_key);
    std::string desc;
    desc += "🌍 **探險地區**：" + (reg ? reg->emoji + " " + reg->name : "（未選擇）") + "\n";
    desc += "⏰ **探險時長**：";
    if (setup.duration_hours > 0) desc += std::to_string(setup.duration_hours) + " 小時（+**" + std::to_string(setup.duration_hours * 4) + "** 探索度）";
    else desc += "（未選擇）";
    desc += "\n💰 **探險資金**：";
    if (setup.funds >= 0) desc += std::to_string(setup.funds) + " 碼（+**" + std::to_string((int)(setup.funds / 250)) + "** 探索度）";
    else desc += "（未設定）";
    desc += "\n🐾 **探險夥伴**：";
    if (setup.partner == 1) desc += "🐾 帶寵物（+**20** 探索度）";
    else if (setup.partner == 0) desc += "🚫 不帶寵物";
    else desc += "（未選擇）";

    bool all_set = reg && setup.duration_hours > 0 && setup.funds >= 0 && setup.partner >= 0;
    if (all_set) {
        int prog = calc_adv_progress(setup.duration_hours, setup.funds, setup.partner == 1);
        desc += "\n\n✨ **預計探索度：" + std::to_string(prog) + "**";
    }
    e.set_description(desc);

    dpp::message msg; msg.add_embed(e);
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
    msg.add_component(row1);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚀 開始探索").set_id("adv_start_" + uid_s)
        .set_style(dpp::cos_primary).set_disabled(!all_set));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📚 收藏").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row2);
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
    int progress = calc_adv_progress(g.duration_hours, g.funds, g.pet_along);

    dpp::embed e;
    if (done) e.set_title("🎉  探險完成！" + (reg ? reg->emoji + " " + reg->name : "")).set_color(0xF1C40F);
    else      e.set_title("🗺️  探險中！" + (reg ? reg->emoji + " " + reg->name : "")).set_color(0x3498DB);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }

    std::string desc;
    desc += "▸ 探索時長：" + std::to_string(g.duration_hours) + " 小時\n";
    desc += "▸ 探險資金：" + std::to_string(g.funds) + " 碼\n";
    desc += "▸ 探險夥伴：" + (g.pet_along ? std::string("🐾 寵物同行") : std::string("🚫 無寵物")) + "\n";
    desc += "▸ 預計探索度：**" + std::to_string(progress) + "**\n";
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
    e.set_description(desc);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    if (done) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📬 收取結果").set_id("adv_collect_" + uid_s).set_style(dpp::cos_success));
    } else {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("❌ 取消探索").set_id("adv_cancel_" + uid_s).set_style(dpp::cos_danger));
    }
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row);
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
    dpp::embed e; e.set_title("🌍  選擇探險地區").set_color(0x2ECC71);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }
    std::string desc;
    for (auto& r : ADV_REGIONS) {
        desc += r.emoji + " **" + r.name + "**\n";
        for (auto& cp : r.checkpoints)
            desc += "　進度 " + std::to_string(cp.progress) + "（池重 " + std::to_string(cp.pool_pct) + "%）\n";
        desc += "\n";
    }
    e.set_description(desc);
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& r : ADV_REGIONS)
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(r.emoji + " " + r.name)
            .set_id("adv_region_" + uid_s + "_" + r.key)
            .set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row);
    return msg;
}

// ─── Sub-page: duration select ────────────────────────────────────────────────

static dpp::message make_adv_duration_select_msg(dpp::snowflake uid, const std::string& dn = "", const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::embed e; e.set_title("⏰  選擇探險時長").set_color(0x2ECC71);
    e.set_description("最少 **2 小時**，最多 **12 小時**。\n每小時 **+4** 探索度。");
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }
    dpp::message msg; msg.add_embed(e);
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    for (int h : {2, 3, 4, 5, 6})
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(std::to_string(h) + "小時")
            .set_id("adv_dur_" + uid_s + "_" + std::to_string(h))
            .set_style(dpp::cos_primary));
    msg.add_component(row1);
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    for (int h : {7, 8, 9, 10, 11})
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(std::to_string(h) + "小時")
            .set_id("adv_dur_" + uid_s + "_" + std::to_string(h))
            .set_style(dpp::cos_primary));
    msg.add_component(row2);
    dpp::component row3; row3.set_type(dpp::cot_action_row);
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("12小時").set_id("adv_dur_" + uid_s + "_12").set_style(dpp::cos_primary));
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row3);
    return msg;
}

// ─── Sub-page: funds select ───────────────────────────────────────────────────

static dpp::message make_adv_funds_select_msg(dpp::snowflake uid, const std::string& dn = "", const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    int64_t cur = get_chips(uid);
    dpp::embed e; e.set_title("💰  選擇探險資金").set_color(0x2ECC71);
    e.set_description("投入的資金在探索期間不可取回。\n每 **250 碼** +1 探索度（上限 10000 碼 = +40）。\n\n目前錢包：**" + std::to_string(cur) + "** 碼");
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }
    dpp::message msg; msg.add_embed(e);
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    // 0, 2500, 5000, 7500, 10000
    for (int64_t amt : {(int64_t)0, (int64_t)2500, (int64_t)5000, (int64_t)7500, (int64_t)10000}) {
        bool disabled = (amt > 0 && cur < amt);
        std::string lbl = (amt == 0) ? "0碼" : (std::to_string(amt) + "碼+" + std::to_string((int)(amt/250)));
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(lbl)
            .set_id("adv_funds_" + uid_s + "_" + std::to_string(amt))
            .set_style(disabled ? dpp::cos_secondary : dpp::cos_primary)
            .set_disabled(disabled));
    }
    msg.add_component(row1);
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row2);
    return msg;
}

// ─── Sub-page: partner select ─────────────────────────────────────────────────

static dpp::message make_adv_partner_select_msg(dpp::snowflake uid, const std::string& dn = "", const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    Pet pet; bool has_pet = false; bool pet_busy = false;
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
    }
    dpp::embed e; e.set_title("🐾  選擇探險夥伴").set_color(0x2ECC71);
    std::string desc = "帶寵物探險 **+20** 探索度。\n探險期間寵物無法打工。\n\n";
    if (!has_pet) desc += "⚠️ 你沒有可以出行的寵物。\n";
    else if (pet_busy) desc += "⚠️ 寵物打工中或已在探險，無法攜帶！\n";
    e.set_description(desc);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    bool can_bring = has_pet && !pet_busy;
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐾 帶寵物（+20探索度）").set_id("adv_partner_" + uid_s + "_1")
        .set_style(dpp::cos_primary).set_disabled(!can_bring));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚫 不帶寵物").set_id("adv_partner_" + uid_s + "_0")
        .set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row);
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
    if (cid == "adv_set_region_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_adv_region_select_msg(uid, dn, av)); return;
    }
    if (cid == "adv_set_duration_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_adv_duration_select_msg(uid, dn, av)); return;
    }
    if (cid == "adv_set_funds_" + uid_s) {
        dpp::interaction_modal_response modal;
        modal.set_custom_id("adv_funds_modal_" + uid_s);
        modal.set_title("設定探險資金");
        modal.add_component(
            dpp::component().set_type(dpp::cot_action_row).add_component(
                dpp::component()
                    .set_type(dpp::cot_text)
                    .set_label("資金（0 ~ 10000 碼，每 250 碼 +1 探索度）")
                    .set_id("funds_input")
                    .set_min_length(1).set_max_length(5)
                    .set_placeholder("輸入 0 ~ 10000")
                    .set_required(true).set_text_style(dpp::text_short)));
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
        { std::lock_guard<std::mutex> lk(data_mutex); adv_setups[uid].duration_hours = h; }
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av)); return;
    }

    // 選擇資金
    if (cid.rfind("adv_funds_" + uid_s + "_", 0) == 0) {
        int64_t amt = 0;
        try { amt = std::stoll(cid.substr(11 + uid_s.size())); } catch (...) {}
        if (amt < 0 || amt > 10000) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 資金須為 0~10000 碼！").set_flags(dpp::m_ephemeral)); return; }
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
                  bool busy = (it->second.work_task > 0 && it->second.work_end > time(nullptr));
                  ok = !busy;
              }
            }
            if (!ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 寵物不可用！").set_flags(dpp::m_ephemeral)); return; }
        }
        { std::lock_guard<std::mutex> lk(data_mutex); adv_setups[uid].partner = with_pet ? 1 : 0; }
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av)); return;
    }

    // 開始探索
    if (cid == "adv_start_" + uid_s) {
        AdventureSetup setup;
        { std::lock_guard<std::mutex> lk(data_mutex); auto it = adv_setups.find(uid); if (it != adv_setups.end()) setup = it->second; }
        if (setup.region_key.empty() || setup.duration_hours <= 0 || setup.funds < 0 || setup.partner < 0) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 請先完成所有設定！").set_flags(dpp::m_ephemeral)); return;
        }
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (adv_games.count(uid)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你已有進行中的探險！").set_flags(dpp::m_ephemeral)); return; }
        }
        if (setup.funds > 0 && get_chips(uid) < setup.funds) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 籌碼不足！需要 **" + std::to_string(setup.funds) + "** 碼。").set_flags(dpp::m_ephemeral)); return;
        }
        if (setup.partner == 1) {
            bool ok = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = pet_data.find(uid);
              if (it != pet_data.end() && it->second.stage > 0) {
                  bool busy = (it->second.work_task > 0 && it->second.work_end > time(nullptr));
                  ok = !busy;
              }
            }
            if (!ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 寵物不可用！").set_flags(dpp::m_ephemeral)); return; }
        }
        if (setup.funds > 0) { add_chips(uid, -setup.funds); save_chips(); }
        AdventureGame g;
        g.uid = uid; g.region_key = setup.region_key;
        g.duration_hours = setup.duration_hours; g.funds = setup.funds;
        g.pet_along = (setup.partner == 1);
        g.start_time = time(nullptr); g.end_time = g.start_time + g.duration_hours * 3600LL;
        { std::lock_guard<std::mutex> lk(data_mutex); adv_games[uid] = g; adv_setups.erase(uid); }
        save_adv_games();
        ev.reply(dpp::ir_update_message, make_adv_active_msg(uid, dn, av)); return;
    }

    // 取消探索
    if (cid == "adv_cancel_" + uid_s) {
        { std::lock_guard<std::mutex> lk(data_mutex); adv_games.erase(uid); }
        save_adv_games();
        ev.reply(dpp::ir_update_message, make_adv_setup_msg(uid, dn, av)); return;
    }

    // 收取結果
    if (cid == "adv_collect_" + uid_s) {
        AdventureGame g; bool found = false;
        { std::lock_guard<std::mutex> lk(data_mutex); auto it = adv_games.find(uid); if (it != adv_games.end()) { g = it->second; found = true; } }
        if (!found || g.end_time > time(nullptr)) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 探險尚未結束！").set_flags(dpp::m_ephemeral)); return;
        }
        int progress = calc_adv_progress(g.duration_hours, g.funds, g.pet_along);
        std::string item_key = roll_adv_loot(g.region_key, progress);
        bool item_added = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            // Limited items: only 1 copy exists globally — skip if already held
            if (!item_key.empty() && LIMITED_COL_ITEMS.count(item_key)) {
                for (auto& [oid, oinv] : inventory_data) {
                    auto jt = oinv.find(item_key);
                    if (jt != oinv.end() && jt->second > 0) { item_key = ""; break; }
                }
            }
            if (!item_key.empty()) { inventory_data[uid][item_key]++; item_added = true; }
            adv_games.erase(uid);
        }
        if (item_added) save_inventory();
        save_adv_games();

        const AdvRegion* reg = find_adv_region(g.region_key);
        auto* vi = find_virtual_item(item_key);
        std::string item_name = vi ? vi->name : "";

        dpp::embed e;
        e.set_title("🎉  探險結果！" + (reg ? reg->emoji + " " + reg->name : "")).set_color(0xF1C40F);
        {
            dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
            if (!av.empty()) f.icon_url = av; e.set_footer(f);
        }
        std::string desc;
        desc += "▸ 地區：" + (reg ? reg->emoji + " " + reg->name : g.region_key) + "\n";
        desc += "▸ 時長：" + std::to_string(g.duration_hours) + " 小時\n";
        desc += "▸ 資金：" + std::to_string(g.funds) + " 碼\n";
        desc += "▸ 夥伴：" + (g.pet_along ? std::string("🐾 寵物同行") : std::string("無")) + "\n";
        desc += "▸ 探索度：**" + std::to_string(progress) + "**\n\n";
        if (item_key.empty())
            desc += "📦 這次探險沒有找到蒐藏品...";
        else {
            desc += "🎁 獲得蒐藏品：**" + item_name + "** ×1！";
            if (vi && !vi->desc.empty()) desc += "\n　*" + vi->desc + "*";
        }
        e.set_description(desc);

        dpp::message msg; msg.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🗺️ 再次探險").set_id("adv_main_" + uid_s).set_style(dpp::cos_primary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📚 收藏").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component(row);
        ev.reply(dpp::ir_update_message, msg); return;
    }
}
