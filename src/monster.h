#pragma once
#include "gacha.h"
#include "pet.h"
#include <random>
#include <string>
#include <vector>
#include <algorithm>

using HuntDropList = std::vector<std::pair<std::string,int>>; // {item_key, count}

// ─── Monster definitions ──────────────────────────────────────────────────────

struct MonsterDef {
    std::string key;
    std::string name;
    std::string difficulty; // "easy"/"normal"/"hard"/"king"
    int         atk;
    int         hp;
    int         def;
    int64_t     first_clear_reward; // chips
    int64_t     daily_min;
    int64_t     daily_max;
    int         drop_chance;  // % to drop an exp item
    std::string image_url;
};

static const std::vector<MonsterDef> MONSTERS = {
    // ── 簡單 ──
    {"easy_1", "練習用木頭人", "easy",  20, 100, 0, 500,  150, 300, 10,
     "https://media.discordapp.net/attachments/1514918524164898966/1521821688084299847/2026-07-01_5.59.35.png?ex=6a463a64&is=6a44e8e4&hm=b82e65d31f8a06b9c044e06c097ab275b77a6163379799d2884b250aa44de52b&=&format=webp&quality=lossless"},
    {"easy_2", "綠水靈",       "easy",  25,  55, 5, 500,  150, 300, 10,
     "https://media.discordapp.net/attachments/1514918524164898966/1521821688277504152/2026-07-01_5.59.49.png?ex=6a463a64&is=6a44e8e4&hm=a3c342a54cdc42abdf284b54a8fef6c966c03d70fa0a8ac352f699fb30099c8d&=&format=webp&quality=lossless"},
    // ── 普通 ──
    {"norm_1", "黑斧木妖",     "normal",18,  60,15,1000,  350, 500, 10,
     "https://media.discordapp.net/attachments/1514918524164898966/1521821688789209089/2026-07-01_6.14.25.png?ex=6a463a64&is=6a44e8e4&hm=729da3cd7918f351fb9019224b07dde98248c553b98ce3044ac7315cde29eab6&=&format=webp&quality=lossless"},
    {"norm_2", "惡魔水靈",     "normal",10, 160,10,1000,  350, 500, 10,
     "https://media.discordapp.net/attachments/1514918524164898966/1521821688545804338/2026-07-01_6.13.43.png?ex=6a463a64&is=6a44e8e4&hm=5fc17acd98051a877f93c65c6298961db6fb236aac76503068ba89820da41898&=&format=webp&quality=lossless"},
    {"norm_3", "小雪球",       "normal",30,  90, 0,1000,  350, 500, 10,
     "https://media.discordapp.net/attachments/1514918524164898966/1521821689011503165/2026-07-01_6.14.54.png?ex=6a463a64&is=6a44e8e4&hm=a6530195dbf803e785ad4ef0371bf6c88632169414896b09afbe8b3993c0752f&=&format=webp&quality=lossless"},
    {"norm_4", "青龍",         "normal",40,  60, 5,1000,  350, 500, 10,
     "https://media.discordapp.net/attachments/1514918524164898966/1521821689279811704/2026-07-01_6.15.14.png?ex=6a463a64&is=6a44e8e4&hm=f1ef0a982582a8b1759e95f8e2aec7a8057453d669fcfaf06461efccda067998&=&format=webp&quality=lossless"},
    // ── 困難 ──
    {"hard_1", "咕咕鐘",       "hard",  18, 170, 3,1500,  500, 750, 10,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1522286430871879750/image.png?ex=6a47eb37&is=6a4699b7&hm=6942c56ded1008258e60178fd596e557bdf9ca503df56a096898bb24369c73b3&"},
    {"hard_2", "怨靈女巫",     "hard",  20, 200, 0,1500,  500, 750, 10,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1522289042778095676/image.png?ex=6a47eda6&is=6a469c26&hm=92d2c6728549f0caf569db6edf5d506346f1b6c096234fb42f71805d48eebbf6&"},
    {"hard_3", "巴洛古",       "hard",  60,  70, 1,1500,  500, 750, 10,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1522286491659665439/image.png?ex=6a47eb46&is=6a4699c6&hm=d86293653c58f51b1131063d342ae71b4b8bf3543e0b5b456d93b2d8e6d1f4e7c3&"},
    {"hard_4", "雪毛怪人",     "hard", 120,  30,10,1500,  500, 750, 10,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1522294231245258752/image.png?ex=6a47f27b&is=6a46a0fb&hm=21c7e9ea71ac2b0768e081a188e9e4a2a162f043e449b38d378470669d5f5c02&"},
    // ── 王 ──
    {"king_1", "地獄巴洛谷",   "king",  28, 140, 5,3000,  600,1000, 30,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1522294232323326024/image.png?ex=6a47f27b&is=6a46a0fb&hm=ece7b03782a16406a1a6021eaecdc8fd0c281f624b1b7150ee533286618948ac&"},
    {"king_2", "噴火龍",       "king",  45, 100, 0,3000,  600,1000, 30,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1522289043210113284/image.png?ex=6a47eda6&is=6a469c26&hm=64454106196fb58547f6cab644d5ff54dcee8ca0f27827c20ac9a5faa7bae9f8&"},
    {"king_3", "天使綠水靈",   "king",  14, 200,10,3000,  600,1000, 30,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1522294231899832471/images.png?ex=6a47f27b&is=6a46a0fb&hm=f4b81d0b4f3c6f83e475bb6801b01615623c1f327f98d717b504ed899a6dc6b5&"},
    {"king_4", "拉圖斯",       "king",  60,  20,25,3000,  600,1000, 30,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1522289092690317322/image.png?ex=6a4f2df2&is=6a4ddc72&hm=0243c8eb3bab92c815006b706f7c5335c02af8945ee670cacf8e44a647f42dbc&"},
};

static const MonsterDef* find_monster(const std::string& key) {
    for (auto& m : MONSTERS) if (m.key == key) return &m;
    return nullptr;
}

static std::mt19937& hunt_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

// Exp items that can drop
static const std::vector<std::string> DROP_EXP_ITEMS = {
    "grow_1","grow_2","grow_3","grow_4","grow_5","grow_6"
};
static const std::vector<std::string> DROP_RECOVERY_ITEMS = {
    "recover_depress","recover_injury","recover_muscle","recover_fatigue"
};

// ─── HP bar helper ────────────────────────────────────────────────────────────

static std::string hp_bar(int cur, int max_hp, int len = 10) {
    int filled = (max_hp > 0) ? std::max(0, cur * len / max_hp) : 0;
    filled = std::max(0, std::min(len, filled));
    std::string bar;
    for (int i = 0; i < len; i++) bar += (i < filled) ? "█" : "░";
    return bar + " " + std::to_string(cur) + "/" + std::to_string(max_hp);
}

// ─── Difficulty helpers ───────────────────────────────────────────────────────

static std::string diff_label(const std::string& d) {
    if (d == "easy")   return "⭐ 簡單";
    if (d == "normal") return "⭐⭐ 普通";
    if (d == "hard")   return "⭐⭐⭐ 困難";
    if (d == "king")   return "👑 怪物之王";
    return d;
}

static bool diff_unlocked(dpp::snowflake uid, const std::string& diff) {
    if (diff == "easy") return true;
    std::string prereq;
    if      (diff == "normal") prereq = "easy";
    else if (diff == "hard")   prereq = "normal";
    else if (diff == "king")   prereq = "hard";
    else return false;

    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = hunt_clear_data.find(uid);
    if (it == hunt_clear_data.end()) return false;
    // Check all monsters of prereq difficulty are cleared
    for (auto& m : MONSTERS)
        if (m.difficulty == prereq && !it->second.count(m.key)) return false;
    return true;
}

// ─── Hunt main page ───────────────────────────────────────────────────────────

static dpp::message make_hunt_main_msg(dpp::snowflake uid, const Pet& pet,
                                       const std::string& display_name,
                                       const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)uid);
    int scrolls = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid);
        if (it != inventory_data.end()) {
            auto sit = it->second.find("hunt_scroll");
            if (sit != it->second.end()) scrolls = sit->second;
        }
    }
    PetStats stats = calc_pet_stats(uid, pet);
    {
        int max_hp = stats.hp;
        std::lock_guard<std::mutex> lk(data_mutex);
        apply_pet_basic_set_bonus(uid, pet, stats.atk, stats.hp, max_hp, stats.def);
    }

    bool has_injured = false;
    for (auto& s : pet.statuses) if (s == "受傷") { has_injured = true; break; }

    std::string content = "## ⚔️ 怪物狩獵\n";
    if (has_injured)    content += "⚠️ 你的寵物**受傷**了，無法進行狩獵！\n請使用「高級傷藥」恢復後再來。\n";
    else if (pet.stage == 0) content += "❌ 需要已進化的寵物才能進行狩獵！\n";
    else                content += "選擇難度開始狩獵！\n";
    content += "\n📜 狩獵卷剩餘：**" + std::to_string(scrolls) + "** 張";
    if (pet.stage > 0)
        content += "　⚔️ 攻擊力：**" + std::to_string(stats.atk) + "**　❤️ 生命值：**" + std::to_string(stats.hp) + "**";
    content += "\n\n-# 👤 " + display_name;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xC0, 0x39, 0x2B));
    container.add_component_v2(v2_section(content, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    bool can_hunt = !has_injured && pet.stage > 0 && scrolls > 0;
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& [lbl, diff] : std::vector<std::pair<std::string,std::string>>{
            {"⭐ 簡單","easy"},{"⭐⭐ 普通","normal"},
            {"⭐⭐⭐ 困難","hard"},{"👑 怪物之王","king"}}) {
        bool unlocked = diff_unlocked(uid, diff);
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(lbl)
         .set_id("hunt_diff_" + uid_s + "_" + diff)
         .set_style(dpp::cos_primary)
         .set_disabled(!can_hunt || !unlocked);
        row.add_component(b);
    }
    msg.add_component_v2(row);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏘️ 怪物村落").set_id("hunt_village_" + uid_s).set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⚔️ 組隊遠征").set_id("hunt_team_" + uid_s).set_style(dpp::cos_success));
    msg.add_component_v2(row2);
    return msg;
}

// ─── Difficulty selection (monster list) ─────────────────────────────────────

static dpp::message make_hunt_diff_msg(dpp::snowflake uid, const std::string& diff,
                                       const std::string& display_name,
                                       const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::set<std::string> cleared;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = hunt_clear_data.find(uid);
        if (it != hunt_clear_data.end()) cleared = it->second;
    }

    std::string content = "## ⚔️ " + diff_label(diff) + " — 選擇怪物\n";
    for (auto& m : MONSTERS) {
        if (m.difficulty != diff) continue;
        bool done = cleared.count(m.key) > 0;
        content += std::string(done ? "✅ " : "🔲 ") + "**" + m.name + "**"
                 + "　ATK:" + std::to_string(m.atk)
                 + " HP:" + std::to_string(m.hp)
                 + " DEF:" + std::to_string(m.def) + "\n";
        content += "　首通：" + std::to_string(m.first_clear_reward) + " 碼";
        content += (done ? " （已首通）" : " ⬅ 首通獎勵");
        content += "　通關：" + std::to_string(m.daily_min) + "~" + std::to_string(m.daily_max) + " 碼\n";
    }
    content += "\n-# 👤 " + display_name;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xC0, 0x39, 0x2B));
    container.add_component_v2(v2_section(content, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& m : MONSTERS) {
        if (m.difficulty != diff) continue;
        row.add_component(dpp::component().set_type(dpp::cot_button).set_label(m.name)
             .set_id("hunt_monster_" + uid_s + "_" + m.key).set_style(dpp::cos_danger));
    }
    msg.add_component_v2(row);

    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    nav_row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回")
        .set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav_row);
    return msg;
}

// ─── Combat message ───────────────────────────────────────────────────────────

static dpp::message make_combat_msg(const MonsterHuntGame& g,
                                    const std::string& display_name,
                                    const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);

    std::string content = "## ⚔️ 狩獵中：" + g.monster_name + "　回合 " + std::to_string(g.turn) + "\n";
    content += "**👹 " + g.monster_name + "**\n";
    content += "❤️ " + hp_bar(g.monster_hp, g.monster_max_hp) + "\n";
    if (g.atk_down_turns > 0)
        content += "⬇️ 攻擊力削弱中（剩 **" + std::to_string(g.atk_down_turns) + "** 次）\n";
    content += "\n**🐾 你的寵物**\n";
    content += "❤️ " + hp_bar(g.pet_hp, g.pet_max_hp) + "\n";
    content += "⚔️ 攻擊力 " + std::to_string(g.pet_atk) + "　🛡️ 防禦力 " + std::to_string(g.pet_def) + "\n";
    if (g.orb_key == "EQ_K_VIKING") {
        double r = (g.pet_max_hp > 0) ? (double)g.pet_hp / g.pet_max_hp : 1.0;
        if (r < 0.25)      content += "🔥 **狂暴爆發！** 傷害 ×1.7（HP≤25%）\n";
        else if (r < 0.50) content += "⚡ **憤怒之力！** 傷害 ×1.4（HP≤50%）\n";
    }
    if (!g.log_line.empty()) content += "\n📋 " + g.log_line;
    content += "\n\n-# 👤 " + display_name + "　|　限時 10 分鐘";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xC0, 0x39, 0x2B));
    container.add_component_v2(v2_section(content, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("⚔️ 攻擊")
        .set_id("hunt_atk_" + uid_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("💥 耗費氣力的攻擊")
        .set_id("hunt_pow_" + uid_s).set_style(dpp::cos_danger));
    if (g.orb_key == "EQ_K_BEAR") {
        row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🛡️ 防禦")
            .set_id("hunt_block_" + uid_s).set_style(dpp::cos_secondary));
    }
    if (g.orb_key == "EQ_K_LIFEGODDESS") {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("💗 生命女神 (" + std::to_string(3 - g.lifegoddess_uses) + ")")
            .set_id("hunt_heal_" + uid_s).set_style(dpp::cos_secondary)
            .set_disabled(g.lifegoddess_uses >= 3));
    }
    msg.add_component_v2(row);

    dpp::component ref_row; ref_row.set_type(dpp::cot_action_row);
    ref_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 刷新").set_id("hunt_refresh_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(ref_row);
    return msg;
}

// ─── Process one combat round ─────────────────────────────────────────────────
// Returns true if game ended.
// Fills `win_out` with result and `reward_out` with chips reward.

static bool process_combat(MonsterHuntGame& g, bool power_attack,
                            bool has_muscle_tense,
                            bool& win_out, int64_t& reward_out,
                            HuntDropList& drops_out,
                            bool is_block = false,
                            bool is_battlecry = false,
                            bool is_heal = false) {
    auto randint = [&](int a, int b) {
        return std::uniform_int_distribution<int>(a, b)(hunt_rng());
    };
    static const std::vector<std::string> SHARD_TYPES = {
        "orb_shard_speed","orb_shard_athena","orb_shard_bear","orb_shard_viking","orb_shard_wargod"
    };

    std::string log;

    // ── 雅典娜寶珠：每回合 30% 機率恢復 8 HP ─────────────────────────────────
    if (g.orb_key == "EQ_K_ATHENA" && randint(1, 10) <= 3) {
        int heal = std::min(8, g.pet_max_hp - g.pet_hp);
        if (heal > 0) {
            g.pet_hp += heal;
            log += "💚 雅典娜的祝福！恢復 **" + std::to_string(heal) + "** HP！\n";
        }
    }

    // ── Player attacks ────────────────────────────────────────────────────────
    bool atk_failed = has_muscle_tense && randint(1, 100) <= 30;
    int pet_dmg = 0;
    if (!is_block && !is_battlecry && !is_heal) {
        if (!atk_failed) {
            // 維京寶珠：狂暴被動 — HP 越低傷害越高
            double base_mult = 1.0;
            if (g.orb_key == "EQ_K_VIKING" && g.pet_max_hp > 0) {
                double hp_r = (double)g.pet_hp / g.pet_max_hp;
                if (hp_r < 0.25)      { base_mult = 1.7; log += "🔥 **狂暴爆發**！×1.7\n"; }
                else if (hp_r < 0.50) { base_mult = 1.4; log += "⚡ **憤怒之力**！×1.4\n"; }
            }
            // BB博物館限定：觀觀遺失的胖次 — 本場戰鬥第一次攻擊 +5 攻擊力
            int effective_pet_atk = g.pet_atk;
            if (!g.underwear_first_atk_used) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto wi = inventory_data.find(g.uid);
                if (wi != inventory_data.end() && wi->second.count("col_bb_lost_underwear") && wi->second.at("col_bb_lost_underwear") > 0) {
                    effective_pet_atk += 5;
                    log += "🩲 **胖次加持**！首次攻擊 +5 攻擊力\n";
                }
                g.underwear_first_atk_used = true;
            }
            if (power_attack) {
                double mult = base_mult * (0.1 + std::uniform_real_distribution<double>(0.0, 1.9)(hunt_rng()));
                pet_dmg = std::max(0, (int)(effective_pet_atk * mult) - g.monster_def);
            } else {
                pet_dmg = std::max(0, (int)(effective_pet_atk * base_mult) - g.monster_def);
            }
            // 江湖套裝：爆擊率機率造成雙倍傷害
            bool is_crit = g.pet_crit > 0 && pet_dmg > 0 && randint(1, 100) <= g.pet_crit;
            if (is_crit) pet_dmg *= 2;
            if (power_attack) log += "💥 氣力攻擊對 **" + g.monster_name + "** 造成 **" + std::to_string(pet_dmg) + "** 傷害！";
            else              log += "⚔️ 攻擊對 **" + g.monster_name + "** 造成 **" + std::to_string(pet_dmg) + "** 傷害！";
            if (is_crit) log += " 🗡️**爆擊！**（雙倍傷害）";
            g.monster_hp -= pet_dmg;
            // 暗黑龍王寶珠：攻擊後回復傷害的 1/10（最多 10 HP）
            if (g.orb_key == "EQ_K_DARKDRAGON" && pet_dmg > 0) {
                int heal = std::min(pet_dmg / 10, 10);
                if (heal > 0) {
                    g.pet_hp = std::min(g.pet_hp + heal, g.pet_max_hp);
                    log += " 🌑（+" + std::to_string(heal) + " HP）";
                }
            }
            // 綠水靈洞窟限定：李秀的金箍棒 — 1% 機率攻擊時額外多打一下
            {
                bool has_staff = false;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto wi = inventory_data.find(g.uid);
                  has_staff = wi != inventory_data.end() && wi->second.count("col_golden_staff") && wi->second.at("col_golden_staff") > 0;
                }
                if (has_staff && randint(1, 100) <= 1) {
                    int extra_dmg = std::max(0, (int)(effective_pet_atk * base_mult) - g.monster_def);
                    g.monster_hp -= extra_dmg;
                    log += "\n🥢 **金箍棒**！額外多打一下，追加 **" + std::to_string(extra_dmg) + "** 傷害！";
                }
            }
        } else {
            log += "😓 **肌肉緊繃**！攻擊失敗了！";
        }
    } else if (is_block) {
        g.atk_down_turns = 2;
        log += "🛡️ **防禦！** 怪物下兩次攻擊降低 **60%**！";
    } else if (is_heal) {
        g.lifegoddess_uses++;
        int heal = std::min((int)std::ceil(g.pet_max_hp * 0.2), g.pet_max_hp - g.pet_hp);
        if (heal > 0) {
            g.pet_hp += heal;
            log += "💗 **生命女神的祝福！** 回復 **" + std::to_string(heal) + "** HP！";
        } else {
            log += "💗 **生命女神的祝福！** HP已滿，未回復。";
        }
    }

    if (g.monster_hp <= 0) {
        g.monster_hp = 0;
        win_out = true;
        const MonsterDef* md = find_monster(g.monster_key);
        if (md) reward_out = randint((int)md->daily_min, (int)md->daily_max);
        // BB自然博物館中級套組：狩獵／王團獎勵籌碼 +3%
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (col_set_bb_mid(g.uid)) reward_out = (int64_t)std::ceil(reward_out * 1.03); }
        // 各掉落獨立判定
        // 成長道具 / 回復道具共用機率，各半機率
        if (md && randint(1, 100) <= md->drop_chance) {
            static const std::vector<std::string> ALL_HUNT_DROPS = {
                "grow_1","grow_2","grow_3","grow_4","grow_5","grow_6",
                "recover_depress","recover_injury","recover_muscle","recover_fatigue"
            };
            drops_out.push_back({ALL_HUNT_DROPS[randint(0,(int)ALL_HUNT_DROPS.size()-1)], 1});
        }
        // 寶珠碎片 — 王級 30% 掉 1-3 片，一般怪 10% 掉 1 片
        {
            bool is_king = md && md->difficulty == "king";
            if (randint(1, 100) <= (is_king ? 30 : 10))
                drops_out.push_back({SHARD_TYPES[randint(0,(int)SHARD_TYPES.size()-1)],
                                     is_king ? randint(1, 3) : 1});
        }
        // 未知的星星 5%
        if (randint(1, 100) <= 5)
            drops_out.push_back({"star_unknown", 1});
        g.log_line = log + "\n🎉 **怪物倒下了！勝利！**";
        return true;
    }

    // ── Monster attacks ───────────────────────────────────────────────────────
    int effective_mon_atk = g.monster_atk;
    bool debuff_active = g.atk_down_turns > 0;
    if (debuff_active) {
        effective_mon_atk = (int)(effective_mon_atk * 0.4); // 60% reduction
        g.atk_down_turns--;
    }
    // BB博物館限定：Sian的隱形斗篷 — 1% 機率完全閃避怪物攻擊
    bool dodged = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto wi = inventory_data.find(g.uid);
        if (wi != inventory_data.end() && wi->second.count("col_bb_sian_cloak") && wi->second.at("col_bb_sian_cloak") > 0)
            dodged = (randint(1, 100) <= 1);
    }
    int mon_dmg = dodged ? 0 : std::max(0, effective_mon_atk - g.pet_def);
    g.pet_hp -= mon_dmg;
    if (dodged) {
        log += "　💨 **完全閃避了怪物攻擊！**（隱形斗篷）";
    } else {
        log += "　👹 **" + g.monster_name + "** 反擊造成 **" + std::to_string(mon_dmg) + "** 傷害！";
        if (debuff_active) log += "（削弱-60%）";
        // 綠水靈洞窟限定：貓哥的眼淚 — 受到傷害時 5% 機率恢復 5 點血量
        if (g.pet_hp > 0) {
            bool has_tears = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto wi = inventory_data.find(g.uid);
              has_tears = wi != inventory_data.end() && wi->second.count("col_cat_tears") && wi->second.at("col_cat_tears") > 0;
            }
            if (has_tears && randint(1, 100) <= 5) {
                int heal = std::min(5, g.pet_max_hp - g.pet_hp);
                if (heal > 0) { g.pet_hp += heal; log += " 💧（貓哥的眼淚：恢復" + std::to_string(heal) + "HP）"; }
            }
        }
    }

    // 拉圖斯寶珠：HP≤20% 回復至 50%（每場一次）
    if (g.pet_hp > 0 && g.orb_key == "EQ_K_LATUS" && !g.latus_orb_triggered && g.pet_hp <= g.pet_max_hp / 5) {
        g.latus_orb_triggered = true;
        g.pet_hp = g.pet_max_hp / 2;
        log += " 🔶（拉圖斯寶珠發動！回復至50%）";
    }

    if (g.pet_hp <= 0) {
        g.pet_hp = 0;
        win_out = false;
        g.log_line = log + "\n💀 **寵物倒下了！失敗！**";
        return true;
    }

    g.log_line = log;
    g.turn++;
    return false;
}

// ─── Combat end embed ─────────────────────────────────────────────────────────

static dpp::message make_combat_end_msg(bool win, const MonsterHuntGame& g,
                                        int64_t reward, bool first_clear,
                                        const HuntDropList& drops,
                                        const std::string& display_name,
                                        const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);
    uint32_t accent = win ? dpp::utility::rgb(0x2E, 0xCC, 0x71) : dpp::utility::rgb(0xE7, 0x4C, 0x3C);
    std::string content;
    if (win) {
        content = "## 🎉 狩獵成功！\n你擊敗了 **" + g.monster_name + "**！\n";
        if (first_clear) content += "⭐ **首次通關！額外獎勵！**\n";
        content += "💰 獲得 **" + std::to_string(reward) + "** 籌碼！";
        for (auto& [key, cnt] : drops) {
            std::string nm = key;
            for (auto& vi : VIRTUAL_ITEMS) if (vi.key == key) { nm = vi.name; break; }
            std::string icon = key.find("shard") != std::string::npos ? "💎"
                             : key == "star_unknown" ? "⭐" : "🎁";
            content += "\n" + icon + " 掉落：**" + nm + "** ×" + std::to_string(cnt) + "！";
        }
    } else {
        content = "## 💀 狩獵失敗\n**" + g.monster_name + "** 打倒了你的寵物...\n"
                  "💔 寵物獲得了「**受傷**」狀態，需要高級傷藥才能再次狩獵。";
    }
    content += "\n\n-# 👤 " + display_name;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(accent);
    container.add_component_v2(v2_section(content, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回狩獵頁面")
        .set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

// ─── Village definitions ──────────────────────────────────────────────────────

struct VillageSpiritDef { std::string name; int hp; int atk; int def; int count; };
struct VillageGroupDef {
    std::string key, name, description;
    std::vector<VillageSpiritDef> spirit_types;
    int64_t first_clear_reward, daily_min, daily_max;
    int drop_chance;
    std::string image_url;
};

static const std::vector<VillageGroupDef> VILLAGE_GROUPS = {
    {"water_spirits", "綠水靈們",
     "五隻綠水靈一齊出現！全體同時反擊，消滅越多傷害越低。建議使用高防禦裝備。",
     {{"嫩芽水靈",30,8,0,2},{"水靈衛兵",50,10,3,2},{"水靈首領",70,15,5,1}},
     3000, 600, 1000, 30,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1526986470987006074/image.png?ex=6a590478&is=6a57b2f8&hm=8223bc4aee19ba51385263522d50b7dc4e35e0f50a71262a99a2aa0e57bbff8b&"},
    {"mushroom_village", "菇菇村落",
     "四隻菇菇佔據村落！防禦力高，消滅越多反擊傷害越低。建議使用高攻擊力裝備。",
     {{"胖菇菇",30,6,8,2},{"毒菇衛兵",60,9,13,1},{"菇菇族長",80,12,15,1}},
     3000, 700, 1200, 30,
     "https://cdn.discordapp.com/attachments/1514918524164898966/1526986858192437408/image.png?ex=6a5904d5&is=6a57b355&hm=02c1906a7f7e02d6466fcf7fa5a6f6b4d8e548d09d9dc775236e15a3bc5156b0&"},
};

static const VillageGroupDef* find_village_group(const std::string& key) {
    for (auto& g : VILLAGE_GROUPS) if (g.key == key) return &g;
    return nullptr;
}

static std::vector<VillageSpirit> build_village_spirits(const VillageGroupDef& gd) {
    std::vector<VillageSpirit> out;
    for (auto& sd : gd.spirit_types)
        for (int i = 0; i < sd.count; i++)
            out.push_back({sd.name, sd.hp, sd.hp, sd.atk, sd.def});
    return out;
}

// ─── Village group selection message ─────────────────────────────────────────

static dpp::message make_village_select_msg(dpp::snowflake uid,
                                             const std::string& display_name,
                                             const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string content = "## 🏘️ 選擇目標村落\n選擇要挑戰的怪物村落：\n\n";
    for (auto& g : VILLAGE_GROUPS)
        content += "**" + g.name + "**\n" + g.description + "\n\n";
    content += "-# 👤 " + display_name + "　|　消耗 1 張狩獵卷";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    container.add_component_v2(v2_section(content, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& grp : VILLAGE_GROUPS) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(grp.name)
            .set_id("village_start_" + uid_s + "_" + grp.key)
            .set_style(dpp::cos_primary));
    }
    msg.add_component_v2(row);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);
    return msg;
}

// ─── Village combat message ───────────────────────────────────────────────────

static dpp::message make_village_combat_msg(const VillageGame& g,
                                             const std::string& display_name,
                                             const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);
    const VillageGroupDef* gd = find_village_group(g.group_key);

    std::string content = "## 🏘️ " + (gd ? gd->name : "怪物村落") + "　回合 " + std::to_string(g.turn) + "\n";
    content += "**👹 敵方**\n";
    for (auto& s : g.spirits)
        content += "• **" + s.name + "**　" + hp_bar(s.hp, s.max_hp, 8) + "\n";
    int total_atk = 0, alive_count = 0;
    for (auto& s : g.spirits) if (s.hp > 0) { total_atk += s.atk; alive_count++; }
    content += "（存活 " + std::to_string(alive_count) + " 隻，合計攻擊 " + std::to_string(total_atk) + "）\n";
    content += "\n**🐾 你的寵物**\n";
    content += "❤️ " + hp_bar(g.pet_hp, g.pet_max_hp) + "\n";
    content += "⚔️ 攻擊力 " + std::to_string(g.pet_atk) + "　🛡️ 防禦力 " + std::to_string(g.pet_def) + "\n";
    if (!g.log_line.empty()) content += "\n📋 " + g.log_line;
    content += "\n\n-# 👤 " + display_name + "　|　限時 10 分鐘";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x1A, 0xBC, 0x9C));
    container.add_component_v2(v2_section(content, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (int i = 0; i < (int)g.spirits.size(); i++) {
        auto& s = g.spirits[i];
        bool is_selected = (i == g.selected_target);
        row.add_component(dpp::component().set_type(dpp::cot_button)
             .set_label(s.hp <= 0 ? "💀 " + s.name : (is_selected ? "🎯 " : "") + s.name)
             .set_id("village_atk_" + uid_s + "_" + std::to_string(i))
             .set_style(s.hp <= 0 ? dpp::cos_secondary : is_selected ? dpp::cos_success : dpp::cos_danger)
             .set_disabled(s.hp <= 0));
    }
    msg.add_component_v2(row);

    if (g.selected_target >= 0 && g.selected_target < (int)g.spirits.size() && g.spirits[g.selected_target].hp > 0) {
        dpp::component exec_row; exec_row.set_type(dpp::cot_action_row);
        exec_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("⚔️ 一般攻擊").set_id("village_exec_" + uid_s + "_n").set_style(dpp::cos_primary));
        exec_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🎲 氣力攻擊（×0.1~2.0）").set_id("village_exec_" + uid_s + "_p").set_style(dpp::cos_danger));
        exec_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回").set_id("village_back_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(exec_row);
    }

    if (g.orb_key == "EQ_K_BEAR") {
        dpp::component bear_row; bear_row.set_type(dpp::cot_action_row);
        bear_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🛡️ 防禦").set_id("village_block_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(bear_row);
    }
    if (g.orb_key == "EQ_K_LIFEGODDESS") {
        dpp::component heal_row; heal_row.set_type(dpp::cot_action_row);
        heal_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("💗 生命女神 (" + std::to_string(3 - g.lifegoddess_uses) + ")")
            .set_id("village_heal_" + uid_s).set_style(dpp::cos_secondary)
            .set_disabled(g.lifegoddess_uses >= 3));
        msg.add_component_v2(heal_row);
    }
    dpp::component ref_row; ref_row.set_type(dpp::cot_action_row);
    ref_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 刷新").set_id("village_refresh_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(ref_row);
    return msg;
}

// ─── Village process one attack ───────────────────────────────────────────────

// attack_type: 0=一般攻擊, 1=氣力攻擊（×0.1~2.0隨機）; is_block=true 時跳過攻擊、啟動熊寶珠防禦
static bool process_village_combat(VillageGame& g, int target_idx, int attack_type,
                                    bool& win_out, int64_t& reward_out,
                                    int& spirits_killed_out,
                                    HuntDropList& drops_out,
                                    bool is_block = false,
                                    bool is_heal = false) {
    static const std::vector<std::string> SHARD_TYPES = {
        "orb_shard_speed","orb_shard_athena","orb_shard_bear","orb_shard_viking","orb_shard_wargod"
    };
    auto randint = [&](int a, int b) {
        return std::uniform_int_distribution<int>(a, b)(hunt_rng());
    };
    std::string log;

    // Player attacks chosen spirit (skipped when blocking)
    if (is_block) {
        g.bear_block_turns = 2;
        log += "🛡️ **防禦！** 怪物下兩次攻擊降低 **60%**！";
    }
    if (is_heal) {
        g.lifegoddess_uses++;
        int heal = std::min((int)std::ceil(g.pet_max_hp * 0.2), g.pet_max_hp - g.pet_hp);
        if (heal > 0) {
            g.pet_hp += heal;
            log += "💗 **生命女神的祝福！** 回復 **" + std::to_string(heal) + "** HP！";
        } else {
            log += "💗 **生命女神的祝福！** HP已滿，未回復。";
        }
    }
    if (!is_block && !is_heal) {
    auto& tgt = g.spirits[target_idx];
    int dmg = 0;
    // BB博物館限定：觀觀遺失的胖次 — 本場戰鬥第一次攻擊 +5 攻擊力
    int effective_pet_atk = g.pet_atk;
    if (!g.underwear_first_atk_used) {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto wi = inventory_data.find(g.uid);
        if (wi != inventory_data.end() && wi->second.count("col_bb_lost_underwear") && wi->second.at("col_bb_lost_underwear") > 0) {
            effective_pet_atk += 5;
            log += "🩲 **胖次加持**！首次攻擊 +5 攻擊力 ";
        }
        g.underwear_first_atk_used = true;
    }
    if (attack_type == 1) {
        // 氣力攻擊：隨機 0.1~2.0× 有效傷害
        int base = std::max(0, effective_pet_atk - tgt.def);
        double mult = 0.1 + randint(0, 190) / 100.0;
        dmg = std::max(1, (int)(base * mult));
        char buf[8]; snprintf(buf, sizeof(buf), "%.1f", mult);
        log += "🎲 氣力攻擊（×" + std::string(buf) + "）";
    } else {
        dmg = std::max(0, effective_pet_atk - tgt.def);
    }
    // 江湖套裝：爆擊率機率造成雙倍傷害
    bool is_crit = g.pet_crit > 0 && dmg > 0 && randint(1, 100) <= g.pet_crit;
    if (is_crit) { dmg *= 2; log += (log.empty() ? "" : " ") + std::string("🗡️**爆擊！**（雙倍傷害）"); }
    tgt.hp = std::max(0, tgt.hp - dmg);
    // 暗黑龍王寶珠：攻擊後回復傷害的 1/10（最多 10 HP）
    if (g.orb_key == "EQ_K_DARKDRAGON" && dmg > 0) {
        int heal = std::min(dmg / 10, 10);
        if (heal > 0) {
            g.pet_hp = std::min(g.pet_hp + heal, g.pet_max_hp);
            log += " 🌑（+" + std::to_string(heal) + " HP）";
        }
    }
    // 綠水靈洞窟限定：李秀的金箍棒 — 1% 機率攻擊時額外多打一下
    {
        bool has_staff = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto wi = inventory_data.find(g.uid);
          has_staff = wi != inventory_data.end() && wi->second.count("col_golden_staff") && wi->second.at("col_golden_staff") > 0;
        }
        if (has_staff && tgt.hp > 0 && randint(1, 100) <= 1) {
            int extra_dmg = std::max(0, effective_pet_atk - tgt.def);
            tgt.hp = std::max(0, tgt.hp - extra_dmg);
            log += " 🥢（金箍棒：追加" + std::to_string(extra_dmg) + "傷害）";
        }
    }
    if (!log.empty()) log += " ";
    if (tgt.hp == 0) {
        log += "💥 **" + tgt.name + "** 被擊倒！（" + std::to_string(dmg) + " 傷害）";
        spirits_killed_out++;
    } else {
        log += "⚔️ 攻擊 **" + tgt.name + "**（" + std::to_string(dmg) + " 傷害，剩 " + std::to_string(tgt.hp) + "/" + std::to_string(tgt.max_hp) + " HP）";
    }

    // Check win
    bool all_dead = true;
    for (auto& s : g.spirits) if (s.hp > 0) { all_dead = false; break; }
    if (all_dead) {
        win_out = true;
        const VillageGroupDef* gd = find_village_group(g.group_key);
        if (gd) reward_out = randint((int)gd->daily_min, (int)gd->daily_max);
        // BB自然博物館中級套組：狩獵／王團獎勵籌碼 +3%
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (col_set_bb_mid(g.uid)) reward_out = (int64_t)std::ceil(reward_out * 1.03); }
        // 各掉落獨立判定
        // 成長道具 / 回復道具共用機率，各半機率
        if (gd && randint(1, 100) <= gd->drop_chance) {
            static const std::vector<std::string> ALL_HUNT_DROPS = {
                "grow_1","grow_2","grow_3","grow_4","grow_5","grow_6",
                "recover_depress","recover_injury","recover_muscle","recover_fatigue"
            };
            drops_out.push_back({ALL_HUNT_DROPS[randint(0,(int)ALL_HUNT_DROPS.size()-1)], 1});
        }
        if (randint(1, 100) <= 20)
            drops_out.push_back({SHARD_TYPES[randint(0,(int)SHARD_TYPES.size()-1)], randint(1, 2)});
        if (randint(1, 100) <= 5)
            drops_out.push_back({"star_unknown", 1});
        g.log_line = log + "\n🎉 **所有敵人被消滅！勝利！**";
        return true;
    }
    } // end if (!is_block && !is_heal)

    // All alive spirits counter-attack
    int total_atk = 0;
    for (auto& s : g.spirits) if (s.hp > 0) total_atk += s.atk;
    bool block_active = g.bear_block_turns > 0;
    int effective_atk = block_active ? (int)(total_atk * 0.4) : total_atk;
    if (block_active) g.bear_block_turns--;
    // BB博物館限定：Sian的隱形斗篷 — 1% 機率完全閃避怪物攻擊
    bool dodged = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto wi = inventory_data.find(g.uid);
        if (wi != inventory_data.end() && wi->second.count("col_bb_sian_cloak") && wi->second.at("col_bb_sian_cloak") > 0)
            dodged = (randint(1, 100) <= 1);
    }
    int mon_dmg = dodged ? 0 : std::max(0, effective_atk - g.pet_def);
    g.pet_hp = std::max(0, g.pet_hp - mon_dmg);
    if (dodged) {
        log += "　💨 **全體攻擊被完全閃避了！**（隱形斗篷）";
    } else {
        log += "　👹 全體反擊 **" + std::to_string(mon_dmg) + "** 傷害（合計 " + std::to_string(total_atk) + "-防 " + std::to_string(g.pet_def) + "）";
        if (block_active) log += "（削弱-60%）";
        // 綠水靈洞窟限定：貓哥的眼淚 — 受到傷害時 5% 機率恢復 5 點血量
        if (g.pet_hp > 0) {
            bool has_tears = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto wi = inventory_data.find(g.uid);
              has_tears = wi != inventory_data.end() && wi->second.count("col_cat_tears") && wi->second.at("col_cat_tears") > 0;
            }
            if (has_tears && randint(1, 100) <= 5) {
                int heal = std::min(5, g.pet_max_hp - g.pet_hp);
                if (heal > 0) { g.pet_hp += heal; log += " 💧（貓哥的眼淚：恢復" + std::to_string(heal) + "HP）"; }
            }
        }
    }

    // 拉圖斯寶珠：HP≤20% 回復至 50%（每場一次）
    if (g.pet_hp > 0 && g.orb_key == "EQ_K_LATUS" && !g.latus_orb_triggered && g.pet_hp <= g.pet_max_hp / 5) {
        g.latus_orb_triggered = true;
        g.pet_hp = g.pet_max_hp / 2;
        log += " 🔶（拉圖斯！）";
    }

    if (g.pet_hp <= 0) {
        win_out = false;
        g.log_line = log + "\n💀 **寵物倒下了！失敗！**";
        return true;
    }
    g.log_line = log;
    g.turn++;
    return false;
}

// ─── Village end message ──────────────────────────────────────────────────────

static dpp::message make_village_end_msg(bool win, const VillageGame& g,
                                          int64_t reward, bool first_clear,
                                          const HuntDropList& drops,
                                          const std::string& display_name,
                                          const std::string& avatar_url,
                                          int spirits_killed) {
    std::string uid_s = std::to_string((uint64_t)g.uid);
    const VillageGroupDef* gd = find_village_group(g.group_key);
    uint32_t accent = win ? dpp::utility::rgb(0x2E, 0xCC, 0x71) : dpp::utility::rgb(0xE7, 0x4C, 0x3C);
    std::string content;
    if (win) {
        content = "## 🎉 全部消滅！\n全數擊敗 **" + (gd ? gd->name : "怪物") + "**！\n";
        if (first_clear) content += "⭐ **首次通關！額外獎勵 3000 碼！**\n";
        content += "💰 獲得 **" + std::to_string(reward) + "** 碼！";
        for (auto& [key, cnt] : drops) {
            std::string nm = key;
            for (auto& vi : VIRTUAL_ITEMS) if (vi.key == key) { nm = vi.name; break; }
            std::string icon = key.find("shard") != std::string::npos ? "💎"
                             : key == "star_unknown" ? "⭐" : "🎁";
            content += "\n" + icon + " 掉落：**" + nm + "** ×" + std::to_string(cnt) + "！";
        }
    } else {
        int total_spirits = gd ? (int)([&]{ int n=0; for(auto& sd:gd->spirit_types) n+=sd.count; return n; }()) : (int)g.spirits.size();
        content = "## 💀 挑戰失敗\n消滅了 **" + std::to_string(spirits_killed) + "/" + std::to_string(total_spirits) + "** 隻" + (gd ? gd->name : "怪物") + "。\n"
                  "💔 寵物獲得了「**受傷**」狀態，需要高級傷藥才能再次狩獵。";
    }
    content += "\n\n-# 👤 " + display_name;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(accent);
    container.add_component_v2(v2_section(content, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回狩獵頁面").set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

// ─── Village timeout ──────────────────────────────────────────────────────────

static dpp::message make_village_timeout_msg(const VillageGame& g,
                                              const std::string& display_name,
                                              const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(v2_section("## ⏰ 時間到！\n10 分鐘限時到了！戰鬥自動判定失敗。\n💔 寵物獲得了「**受傷**」狀態。\n\n-# 👤 " + display_name, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回狩獵頁面").set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

// ─── Timeout combat end ───────────────────────────────────────────────────────

static dpp::message make_combat_timeout_msg(const MonsterHuntGame& g,
                                            const std::string& display_name,
                                            const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(v2_section("## ⏰ 時間到！\n10 分鐘限時到了！戰鬥自動判定失敗。\n"
                     "💔 寵物獲得了「**受傷**」狀態。\n\n-# 👤 " + display_name, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回狩獵頁面")
        .set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}
