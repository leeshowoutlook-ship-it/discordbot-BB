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
    std::string bar(filled, '█');
    bar += std::string(len - filled, '░');
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

    dpp::embed e;
    e.set_title("⚔️  怪物狩獵").set_color(0xC0392B);

    bool has_injured = false;
    for (auto& s : pet.statuses) if (s == "受傷") { has_injured = true; break; }

    if (has_injured) {
        e.set_description("⚠️ 你的寵物**受傷**了，無法進行狩獵！\n請使用「高級傷藥」恢復後再來。");
    } else if (pet.stage == 0) {
        e.set_description("❌ 需要已進化的寵物才能進行狩獵！");
    } else {
        e.set_description("選擇難度開始狩獵！");
    }

    e.add_field("📜 狩獵卷剩餘", std::to_string(scrolls) + " 張", true);
    if (pet.stage > 0) {
        e.add_field("⚔️ 攻擊力", std::to_string(stats.atk), true);
        e.add_field("❤️ 生命值", std::to_string(stats.hp),  true);
    }

    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    if (!avatar_url.empty()) e.set_thumbnail(avatar_url);
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);

    bool can_hunt = !has_injured && pet.stage > 0 && scrolls > 0;
    // Row 1: difficulty buttons
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
    msg.add_component(row);
    // Row 2: 怪物村落 + 組隊遠征
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏘️ 怪物村落")
        .set_id("hunt_village_" + uid_s)
        .set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⚔️ 組隊遠征")
        .set_id("hunt_team_" + uid_s)
        .set_style(dpp::cos_success));
    msg.add_component(row2);
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

    dpp::embed e;
    e.set_title("⚔️  " + diff_label(diff) + " — 選擇怪物").set_color(0xC0392B);
    std::string desc;
    for (auto& m : MONSTERS) {
        if (m.difficulty != diff) continue;
        bool done = cleared.count(m.key) > 0;
        desc += std::string(done ? "✅ " : "🔲 ") + "**" + m.name + "**"
              + "　ATK:" + std::to_string(m.atk)
              + " HP:" + std::to_string(m.hp)
              + " DEF:" + std::to_string(m.def) + "\n";
        desc += "　首通：" + std::to_string(m.first_clear_reward) + " 碼";
        desc += (done ? " （已首通）" : " ⬅ 首通獎勵");
        desc += "　通關：" + std::to_string(m.daily_min) + "~" + std::to_string(m.daily_max) + " 碼\n";
    }
    e.set_description(desc);

    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& m : MONSTERS) {
        if (m.difficulty != diff) continue;
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(m.name)
         .set_id("hunt_monster_" + uid_s + "_" + m.key).set_style(dpp::cos_danger);
        row.add_component(b);
    }
    msg.add_component(row);
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回")
        .set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary);
    nav_row.add_component(back);
    msg.add_component(nav_row);
    return msg;
}

// ─── Combat message ───────────────────────────────────────────────────────────

static dpp::message make_combat_msg(const MonsterHuntGame& g,
                                    const std::string& display_name,
                                    const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);
    dpp::embed e;
    e.set_title("⚔️  狩獵中：" + g.monster_name + "　回合 " + std::to_string(g.turn))
     .set_color(0xC0392B);

    // Show monster image as thumbnail if available
    const MonsterDef* cmd = find_monster(g.monster_key);
    if (cmd && !cmd->image_url.empty()) e.set_thumbnail(cmd->image_url);

    std::string desc;
    desc += "**👹 " + g.monster_name + "**\n";
    desc += "❤️ " + hp_bar(g.monster_hp, g.monster_max_hp) + "\n";
    if (g.atk_down_turns > 0)
        desc += "⬇️ 攻擊力削弱中（剩 **" + std::to_string(g.atk_down_turns) + "** 次）\n";
    desc += "\n**🐾 你的寵物**\n";
    desc += "❤️ " + hp_bar(g.pet_hp, g.pet_max_hp) + "\n";
    desc += "⚔️ 攻擊力 " + std::to_string(g.pet_atk) + "　🛡️ 防禦力 " + std::to_string(g.pet_def) + "\n";

    if (g.orb_key == "EQ_K_VIKING") {
        double r = (g.pet_max_hp > 0) ? (double)g.pet_hp / g.pet_max_hp : 1.0;
        if (r < 0.25)      desc += "🔥 **狂暴爆發！** 傷害 ×1.7（HP≤25%）\n";
        else if (r < 0.50) desc += "⚡ **憤怒之力！** 傷害 ×1.4（HP≤50%）\n";
    }
    if (!g.log_line.empty()) desc += "\n📋 " + g.log_line;
    e.set_description(desc);

    dpp::embed_footer footer;
    footer.text = "👤 " + display_name + "　|　限時 10 分鐘";
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component atk, pow_atk;
    atk.set_type(dpp::cot_button).set_label("⚔️ 攻擊")
       .set_id("hunt_atk_" + uid_s).set_style(dpp::cos_primary);
    pow_atk.set_type(dpp::cot_button).set_label("💥 耗費氣力的攻擊")
            .set_id("hunt_pow_" + uid_s).set_style(dpp::cos_danger);
    row.add_component(atk); row.add_component(pow_atk);
    // 寶珠特殊行動按鈕
    if (g.orb_key == "EQ_K_BEAR") {
        dpp::component block_btn;
        block_btn.set_type(dpp::cot_button).set_label("🛡️ 防禦")
                 .set_id("hunt_block_" + uid_s).set_style(dpp::cos_secondary);
        row.add_component(block_btn);
    }
    // 維京寶珠：被動狂暴，無需按鈕
    msg.add_component(row);

    // Refresh row (separate from combat actions)
    dpp::component ref_row; ref_row.set_type(dpp::cot_action_row);
    ref_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 刷新").set_id("hunt_refresh_" + uid_s)
        .set_style(dpp::cos_secondary));
    msg.add_component(ref_row);

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
                            bool is_battlecry = false) {
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
    if (!is_block && !is_battlecry) {
        if (!atk_failed) {
            // 維京寶珠：狂暴被動 — HP 越低傷害越高
            double base_mult = 1.0;
            if (g.orb_key == "EQ_K_VIKING" && g.pet_max_hp > 0) {
                double hp_r = (double)g.pet_hp / g.pet_max_hp;
                if (hp_r < 0.25)      { base_mult = 1.7; log += "🔥 **狂暴爆發**！×1.7\n"; }
                else if (hp_r < 0.50) { base_mult = 1.4; log += "⚡ **憤怒之力**！×1.4\n"; }
            }
            if (power_attack) {
                double mult = base_mult * (0.1 + std::uniform_real_distribution<double>(0.0, 1.9)(hunt_rng()));
                pet_dmg = std::max(0, (int)(g.pet_atk * mult) - g.monster_def);
                log += "💥 氣力攻擊對 **" + g.monster_name + "** 造成 **" + std::to_string(pet_dmg) + "** 傷害！";
            } else {
                pet_dmg = std::max(0, (int)(g.pet_atk * base_mult) - g.monster_def);
                log += "⚔️ 攻擊對 **" + g.monster_name + "** 造成 **" + std::to_string(pet_dmg) + "** 傷害！";
            }
            g.monster_hp -= pet_dmg;
            // 暗黑龍王寶珠：攻擊後回復傷害的 1/10（最多 10 HP）
            if (g.orb_key == "EQ_K_DARKDRAGON" && pet_dmg > 0) {
                int heal = std::min(pet_dmg / 10, 10);
                if (heal > 0) {
                    g.pet_hp = std::min(g.pet_hp + heal, g.pet_max_hp);
                    log += " 🌑（+" + std::to_string(heal) + " HP）";
                }
            }
        } else {
            log += "😓 **肌肉緊繃**！攻擊失敗了！";
        }
    } else if (is_block) {
        g.atk_down_turns = 2;
        log += "🛡️ **防禦！** 怪物下兩次攻擊降低 **60%**！";
    }

    if (g.monster_hp <= 0) {
        g.monster_hp = 0;
        win_out = true;
        const MonsterDef* md = find_monster(g.monster_key);
        if (md) reward_out = randint((int)md->daily_min, (int)md->daily_max);
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
    int mon_dmg = std::max(0, effective_mon_atk - g.pet_def);
    g.pet_hp -= mon_dmg;
    log += "　👹 **" + g.monster_name + "** 反擊造成 **" + std::to_string(mon_dmg) + "** 傷害！";
    if (debuff_active) log += "（削弱-60%）";

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
    dpp::embed e;
    const MonsterDef* emd = find_monster(g.monster_key);
    if (emd && !emd->image_url.empty()) e.set_thumbnail(emd->image_url);
    if (win) {
        e.set_title("🎉  狩獵成功！").set_color(0x2ECC71);
        std::string desc = "你擊敗了 **" + g.monster_name + "**！\n";
        if (first_clear) desc += "⭐ **首次通關！額外獎勵！**\n";
        desc += "💰 獲得 **" + std::to_string(reward) + "** 籌碼！";
        for (auto& [key, cnt] : drops) {
            std::string nm = key;
            for (auto& vi : VIRTUAL_ITEMS) if (vi.key == key) { nm = vi.name; break; }
            std::string icon = key.find("shard") != std::string::npos ? "💎"
                             : key == "star_unknown" ? "⭐" : "🎁";
            desc += "\n" + icon + " 掉落：**" + nm + "** ×" + std::to_string(cnt) + "！";
        }
        e.set_description(desc);
    } else {
        e.set_title("💀  狩獵失敗").set_color(0xE74C3C);
        e.set_description("**" + g.monster_name + "** 打倒了你的寵物...\n"
                          "💔 寵物獲得了「**受傷**」狀態，需要高級傷藥才能再次狩獵。");
    }
    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回狩獵頁面")
        .set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(back);
    msg.add_component(row);
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
    dpp::embed e;
    e.set_title("🏘️  選擇目標村落").set_color(0x3498DB);
    std::string desc = "選擇要挑戰的怪物村落：\n\n";
    for (auto& g : VILLAGE_GROUPS) {
        desc += "**" + g.name + "**\n" + g.description + "\n\n";
    }
    e.set_description(desc);
    dpp::embed_footer footer;
    footer.text = "👤 " + display_name + "　|　消耗 1 張狩獵卷";
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& grp : VILLAGE_GROUPS) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(grp.name)
            .set_id("village_start_" + uid_s + "_" + grp.key)
            .set_style(dpp::cos_primary));
    }
    msg.add_component(row);
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回")
        .set_id("hunt_main_" + uid_s)
        .set_style(dpp::cos_secondary));
    msg.add_component(row2);
    return msg;
}

// ─── Village combat message ───────────────────────────────────────────────────

static dpp::message make_village_combat_msg(const VillageGame& g,
                                             const std::string& display_name,
                                             const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);
    const VillageGroupDef* gd = find_village_group(g.group_key);
    dpp::embed e;
    e.set_title("🏘️  " + (gd ? gd->name : "怪物村落") + "　回合 " + std::to_string(g.turn))
     .set_color(0x1ABC9C);
    if (gd && !gd->image_url.empty()) e.set_thumbnail(gd->image_url);

    // Spirits
    std::string desc = "**👹 敵方**\n";
    for (auto& s : g.spirits)
        desc += "• **" + s.name + "**　" + hp_bar(s.hp, s.max_hp, 8) + "\n";
    // Combined attack
    int total_atk = 0;
    int alive_count = 0;
    for (auto& s : g.spirits) if (s.hp > 0) { total_atk += s.atk; alive_count++; }
    desc += "（存活 " + std::to_string(alive_count) + " 隻，合計攻擊 " + std::to_string(total_atk) + "）\n";
    desc += "\n**🐾 你的寵物**\n";
    desc += "❤️ " + hp_bar(g.pet_hp, g.pet_max_hp) + "\n";
    desc += "⚔️ 攻擊力 " + std::to_string(g.pet_atk) + "　🛡️ 防禦力 " + std::to_string(g.pet_def) + "\n";
    if (!g.log_line.empty()) desc += "\n📋 " + g.log_line;
    e.set_description(desc);

    dpp::embed_footer footer;
    footer.text = "👤 " + display_name + "　|　限時 10 分鐘";
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);

    // Target selection row
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (int i = 0; i < (int)g.spirits.size(); i++) {
        auto& s = g.spirits[i];
        bool is_selected = (i == g.selected_target);
        dpp::component b;
        b.set_type(dpp::cot_button)
         .set_label(s.hp <= 0 ? "💀 " + s.name : (is_selected ? "🎯 " : "") + s.name)
         .set_id("village_atk_" + uid_s + "_" + std::to_string(i))
         .set_style(s.hp <= 0 ? dpp::cos_secondary : is_selected ? dpp::cos_success : dpp::cos_danger)
         .set_disabled(s.hp <= 0);
        row.add_component(b);
    }
    msg.add_component(row);

    // Attack type row (only when target selected)
    if (g.selected_target >= 0 && g.selected_target < (int)g.spirits.size() && g.spirits[g.selected_target].hp > 0) {
        dpp::component exec_row; exec_row.set_type(dpp::cot_action_row);
        exec_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("⚔️ 一般攻擊")
            .set_id("village_exec_" + uid_s + "_n")
            .set_style(dpp::cos_primary));
        exec_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🎲 氣力攻擊（×0.1~2.0）")
            .set_id("village_exec_" + uid_s + "_p")
            .set_style(dpp::cos_danger));
        exec_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回")
            .set_id("village_back_" + uid_s)
            .set_style(dpp::cos_secondary));
        msg.add_component(exec_row);
    }

    if (g.orb_key == "EQ_K_BEAR") {
        dpp::component bear_row; bear_row.set_type(dpp::cot_action_row);
        bear_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🛡️ 防禦").set_id("village_block_" + uid_s)
            .set_style(dpp::cos_secondary));
        msg.add_component(bear_row);
    }
    dpp::component ref_row; ref_row.set_type(dpp::cot_action_row);
    ref_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 刷新").set_id("village_refresh_" + uid_s)
        .set_style(dpp::cos_secondary));
    msg.add_component(ref_row);
    return msg;
}

// ─── Village process one attack ───────────────────────────────────────────────

// attack_type: 0=一般攻擊, 1=氣力攻擊（×0.1~2.0隨機）; is_block=true 時跳過攻擊、啟動熊寶珠防禦
static bool process_village_combat(VillageGame& g, int target_idx, int attack_type,
                                    bool& win_out, int64_t& reward_out,
                                    int& spirits_killed_out,
                                    HuntDropList& drops_out,
                                    bool is_block = false) {
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
    if (!is_block) {
    auto& tgt = g.spirits[target_idx];
    int dmg = 0;
    if (attack_type == 1) {
        // 氣力攻擊：隨機 0.1~2.0× 有效傷害
        int base = std::max(0, g.pet_atk - tgt.def);
        double mult = 0.1 + randint(0, 190) / 100.0;
        dmg = std::max(1, (int)(base * mult));
        char buf[8]; snprintf(buf, sizeof(buf), "%.1f", mult);
        log += "🎲 氣力攻擊（×" + std::string(buf) + "）";
    } else {
        dmg = std::max(0, g.pet_atk - tgt.def);
    }
    tgt.hp = std::max(0, tgt.hp - dmg);
    // 暗黑龍王寶珠：攻擊後回復傷害的 1/10（最多 10 HP）
    if (g.orb_key == "EQ_K_DARKDRAGON" && dmg > 0) {
        int heal = std::min(dmg / 10, 10);
        if (heal > 0) {
            g.pet_hp = std::min(g.pet_hp + heal, g.pet_max_hp);
            log += " 🌑（+" + std::to_string(heal) + " HP）";
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
    } // end if (!is_block)

    // All alive spirits counter-attack
    int total_atk = 0;
    for (auto& s : g.spirits) if (s.hp > 0) total_atk += s.atk;
    bool block_active = g.bear_block_turns > 0;
    int effective_atk = block_active ? (int)(total_atk * 0.4) : total_atk;
    if (block_active) g.bear_block_turns--;
    int mon_dmg = std::max(0, effective_atk - g.pet_def);
    g.pet_hp = std::max(0, g.pet_hp - mon_dmg);
    log += "　👹 全體反擊 **" + std::to_string(mon_dmg) + "** 傷害（合計 " + std::to_string(total_atk) + "-防 " + std::to_string(g.pet_def) + "）";
    if (block_active) log += "（削弱-60%）";

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
    dpp::embed e;
    if (win) {
        e.set_title("🎉  全部消滅！").set_color(0x2ECC71);
        std::string desc = "全數擊敗 **" + (gd ? gd->name : "怪物") + "**！\n";
        if (first_clear) desc += "⭐ **首次通關！額外獎勵 3000 碼！**\n";
        desc += "💰 獲得 **" + std::to_string(reward) + "** 碼！";
        for (auto& [key, cnt] : drops) {
            std::string nm = key;
            for (auto& vi : VIRTUAL_ITEMS) if (vi.key == key) { nm = vi.name; break; }
            std::string icon = key.find("shard") != std::string::npos ? "💎"
                             : key == "star_unknown" ? "⭐" : "🎁";
            desc += "\n" + icon + " 掉落：**" + nm + "** ×" + std::to_string(cnt) + "！";
        }
        e.set_description(desc);
    } else {
        e.set_title("💀  挑戰失敗").set_color(0xE74C3C);
        int total_spirits = gd ? (int)([&]{ int n=0; for(auto& sd:gd->spirit_types) n+=sd.count; return n; }()) : (int)g.spirits.size();
        std::string group_label = gd ? gd->name : "怪物";
        e.set_description("消滅了 **" + std::to_string(spirits_killed) + "/" + std::to_string(total_spirits) + "** 隻" + group_label + "。\n"
                          "💔 寵物獲得了「**受傷**」狀態，需要高級傷藥才能再次狩獵。");
    }
    if (gd && !gd->image_url.empty()) e.set_thumbnail(gd->image_url);
    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回狩獵頁面").set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row);
    return msg;
}

// ─── Village timeout ──────────────────────────────────────────────────────────

static dpp::message make_village_timeout_msg(const VillageGame& g,
                                              const std::string& display_name,
                                              const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);
    dpp::embed e;
    e.set_title("⏰  時間到！").set_color(0xE74C3C);
    e.set_description("10 分鐘限時到了！戰鬥自動判定失敗。\n💔 寵物獲得了「**受傷**」狀態。");
    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);
    dpp::message msg; msg.add_embed(e);
    msg.add_component(dpp::component().set_type(dpp::cot_action_row)
        .add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回狩獵頁面").set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary)));
    return msg;
}

// ─── Timeout combat end ───────────────────────────────────────────────────────

static dpp::message make_combat_timeout_msg(const MonsterHuntGame& g,
                                            const std::string& display_name,
                                            const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)g.uid);
    dpp::embed e;
    e.set_title("⏰  時間到！").set_color(0xE74C3C);
    e.set_description("10 分鐘限時到了！戰鬥自動判定失敗。\n"
                      "💔 寵物獲得了「**受傷**」狀態。");
    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回狩獵頁面")
        .set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(back);
    msg.add_component(row);
    return msg;
}
