#pragma once
#include "types.h"
#include "pet.h"
#include "monster.h"
#include <random>
#include <functional>

// ─── RNG ─────────────────────────────────────────────────────────────────────

static std::mt19937& raid_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}
static int raid_rand(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(raid_rng());
}

// ─── Boss definitions ─────────────────────────────────────────────────────────

struct RaidBoss {
    std::string key;
    std::string name;
    std::string image;
    int         hp;
    int         atk;  // base single target
    int         def;
};

static const std::vector<RaidBoss> RAID_BOSSES = {
    {
        "latus",
        "拉圖斯",
        "https://cdn.discordapp.com/attachments/1514918524164898966/1524451843139436656/049842261287f346e4b5cb0906a01163.png?ex=6a4fcbea&is=6a4e7a6a&hm=1a6ee99e2b505f874d7d1c118350e023bf8fe48d1afc5d71508047793621a76a&",
        1500, 40, 2
    },
    {
        "dark_dragon",
        "暗黑龍王",
        "",
        2500, 55, 5
    },
};

static const RaidBoss* find_raid_boss(const std::string& key) {
    for (auto& b : RAID_BOSSES) if (b.key == key) return &b;
    return nullptr;
}

// ─── Boss attack types ────────────────────────────────────────────────────────

enum class BossAttack { AOE, SINGLE, STUN, HEAL, DRAIN };

static BossAttack pick_boss_attack(bool forbid_aoe = false, bool forbid_single = false,
                                    const std::string& boss_key = "") {
    for (int tries = 0; tries < 10; tries++) {
        int r = raid_rand(1, 100);
        BossAttack a;
        if (boss_key == "dark_dragon") {
            // 暗黑龍王：無回血，以生命汲取代替；強化眩暈率
            if (r <= 30) a = BossAttack::AOE;
            else if (r <= 60) a = BossAttack::SINGLE;
            else if (r <= 80) a = BossAttack::STUN;
            else a = BossAttack::DRAIN;
        } else {
            if (r <= 35) a = BossAttack::AOE;
            else if (r <= 70) a = BossAttack::SINGLE;
            else if (r <= 90) a = BossAttack::STUN;
            else a = BossAttack::HEAL;
        }
        if (a == BossAttack::AOE    && forbid_aoe)    continue;
        if (a == BossAttack::SINGLE && forbid_single) continue;
        return a;
    }
    return BossAttack::STUN;
}

// ─── Room message ─────────────────────────────────────────────────────────────

static dpp::message make_raid_room_msg(const RaidRoom& room) {
    dpp::embed e;
    e.set_title("⚔️ 組隊王挑戰 — 等待中");
    e.set_color(0x3498DB);
    { auto* bd = find_raid_boss(room.boss_key); if (bd && !bd->image.empty()) e.set_image(bd->image); }

    std::string members_str;
    for (auto& uid : room.member_uids) {
        auto it = room.member_names.find(uid);
        std::string nm = (it != room.member_names.end()) ? it->second : "?";
        members_str += "• " + nm + "\n";
    }
    e.add_field("👥 目前成員 (" + std::to_string(room.member_uids.size()) + "/4)", members_str.empty() ? "無" : members_str, false);
    if (!room.practice_mode)
        e.add_field("🎫 需要道具", "每週怪物狩獵卷 ×1（開始時消耗）", false);
    e.add_field("ℹ️ 說明", "最多 4 人，最少需 2 人，房間 10 分鐘後自動解散", false);
    e.set_footer(dpp::embed_footer().set_text("🆔 頻道組隊房"));

    dpp::component row1;
    row1.set_type(dpp::cot_action_row);

    dpp::component join_btn;
    join_btn.set_type(dpp::cot_button)
        .set_label("加入")
        .set_id("rroom_join_" + std::to_string((uint64_t)room.channel_id))
        .set_style(dpp::cos_primary)
        .set_emoji("🚪", 0)
        .set_disabled(room.member_uids.size() >= 4);
    row1.add_component(join_btn);

    dpp::component start_btn;
    start_btn.set_type(dpp::cot_button)
        .set_label("開始戰鬥")
        .set_id("rroom_start_" + std::to_string((uint64_t)room.channel_id))
        .set_style(dpp::cos_success)
        .set_emoji("⚔️", 0)
        .set_disabled(room.member_uids.size() < 2);
    row1.add_component(start_btn);

    dpp::component dissolve_btn;
    dissolve_btn.set_type(dpp::cot_button)
        .set_label("解散")
        .set_id("rroom_dissolve_" + std::to_string((uint64_t)room.channel_id))
        .set_style(dpp::cos_danger)
        .set_emoji("🗑️", 0);
    row1.add_component(dissolve_btn);

    dpp::message msg;
    msg.add_embed(e).add_component(row1);
    return msg;
}

// ─── Boss selection screen ────────────────────────────────────────────────────

static dpp::message make_raid_boss_select_msg(dpp::snowflake uid,
                                              const std::string& dn,
                                              const std::string& av) {
    std::string uid_s = std::to_string((uint64_t)uid);

    dpp::embed e;
    e.set_title("⚔️  組隊遠征 — 選擇王");
    e.set_color(0xE74C3C);
    e.set_description("選擇要挑戰的王，需持有 **每週怪物狩獵卷** ×1。");
    e.add_field("👑 可挑戰",
        "🐉 **拉圖斯**　HP 1500　ATK 40　DEF 2\n"
        "🌑 **暗黑龍王**　HP 2500　ATK 55　DEF 5　⚠️ 生命汲取無視防禦", false);
    e.add_field("🔒 未開放",
        "🔥 **混沌炎魔**", false);
    if (!av.empty()) e.set_thumbnail(av);
    e.set_footer(dpp::embed_footer().set_text("👤 " + dn));

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐉 拉圖斯")
        .set_id("hunt_boss_latus_" + uid_s)
        .set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🌑 暗黑龍王")
        .set_id("hunt_boss_dark_" + uid_s)
        .set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔥 混沌炎魔（未開放）")
        .set_id("hunt_boss_chaos_" + uid_s)
        .set_style(dpp::cos_secondary)
        .set_disabled(true));
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏋️ 練習：拉圖斯")
        .set_id("hunt_boss_latus_p_" + uid_s)
        .set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏋️ 練習：暗黑龍王")
        .set_id("hunt_boss_dark_p_" + uid_s)
        .set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回")
        .set_id("hunt_main_" + uid_s)
        .set_style(dpp::cos_secondary));

    dpp::message msg; msg.add_embed(e);
    msg.add_component(row).add_component(row2);
    return msg;
}

// ─── Combat message ───────────────────────────────────────────────────────────

static dpp::message make_raid_combat_msg(const RaidGame& g) {
    dpp::embed e;
    e.set_color(0xE74C3C);

    // Boss section
    std::string boss_line = "**" + g.boss_name + "**\n";
    boss_line += hp_bar(g.boss_hp, g.boss_max_hp) + " " +
                 std::to_string(g.boss_hp) + "/" + std::to_string(g.boss_max_hp) + " HP\n";
    boss_line += "⚔️ ATK " + std::to_string(g.boss_atk) + "  🛡️ DEF " + std::to_string(g.boss_def);
    e.set_description(boss_line);

    // Players section
    std::string players_str;
    for (int i = 0; i < (int)g.players.size(); i++) {
        auto& p = g.players[i];
        bool is_turn = (!g.boss_turn && i == g.current_player);
        std::string status;
        if (!p.alive)              status = " 💀";
        else if (p.stunned_turns > 0) status = " 🔒 封鎖中(" + std::to_string(p.stunned_turns) + ")";
        else if (p.power_skip)    status = " 💤 強攻疲勞";
        else if (is_turn)          status = " ◀️";
        // 寶珠圖示：固定顯示自己帶的寶珠；維京/拉圖斯觸發時圖示會變化
        if (p.alive && !p.orb_key.empty()) {
            if (p.orb_key == "EQ_K_VIKING" && p.max_hp > 0) {
                double r = (double)p.hp / p.max_hp;
                if (r < 0.25)      status += " 🔥狂暴×1.7";
                else if (r < 0.50) status += " ⚡憤怒×1.4";
                else               status += " " + orb_baseline_icon(p.orb_key);
            } else if (p.orb_key == "EQ_K_LATUS") {
                status += p.latus_orb_triggered ? " ✨拉圖斯(已觸發)" : " " + orb_baseline_icon(p.orb_key);
            } else {
                status += " " + orb_baseline_icon(p.orb_key);
            }
        }
        players_str += (is_turn ? "▶ " : "  ") + p.display_name + status + "\n";
        players_str += "  " + hp_bar(p.hp, p.max_hp, 8) + " " +
                       std::to_string(p.hp) + "/" + std::to_string(p.max_hp) + " HP\n";
    }
    e.add_field("👥 隊員", players_str, false);

    // Log
    if (!g.log_line.empty())
        e.add_field("📋 最近行動", g.log_line, false);

    if (g.boss_turn)
        e.set_title("Round " + std::to_string(g.round) + " — 🔴 Boss 行動中...");
    else {
        auto& cp = g.players[g.current_player];
        e.set_title("Round " + std::to_string(g.round) + " — " + cp.display_name + " 的回合");
    }
    e.set_image(g.boss_image);
    e.set_footer(dpp::embed_footer().set_text("⏱️ 20 分鐘內未完成視為失敗"));

    dpp::message msg;
    msg.add_embed(e);

    if (g.game_over || g.boss_turn) return msg;

    // Action buttons for current player
    auto& cp = g.players[g.current_player];
    std::string ch_s = std::to_string((uint64_t)g.channel_id);
    std::string uid_s = std::to_string((uint64_t)cp.uid);

    // If waiting for battlecry target selection
    if (g.cry_pending_uid == cp.uid) {
        dpp::component row_t;
        row_t.set_type(dpp::cot_action_row);
        for (int i = 0; i < (int)g.players.size(); i++) {
            auto& tp = g.players[i];
            if (!tp.alive) continue;
            dpp::component tbtn;
            tbtn.set_type(dpp::cot_button)
                .set_label(tp.display_name)
                .set_id("raid_cryt_" + ch_s + "_" + uid_s + "_" + std::to_string(i))
                .set_style(dpp::cos_primary);
            row_t.add_component(tbtn);
        }
        msg.add_component(row_t);
        return msg;
    }

    dpp::component row1;
    row1.set_type(dpp::cot_action_row);

    dpp::component atk_btn;
    atk_btn.set_type(dpp::cot_button)
        .set_label("攻擊")
        .set_id("raid_atk_" + ch_s + "_" + uid_s)
        .set_style(dpp::cos_danger)
        .set_emoji("⚔️", 0);
    row1.add_component(atk_btn);

    dpp::component gamble_btn;
    gamble_btn.set_type(dpp::cot_button)
        .set_label("耗費氣力")
        .set_id("raid_gamble_" + ch_s + "_" + uid_s)
        .set_style(dpp::cos_primary)
        .set_emoji("🎲", 0);
    row1.add_component(gamble_btn);

    dpp::component pow_btn;
    pow_btn.set_type(dpp::cot_button)
        .set_label("強攻")
        .set_id("raid_pow_" + ch_s + "_" + uid_s)
        .set_style(dpp::cos_danger)
        .set_emoji("💥", 0);
    row1.add_component(pow_btn);

    // Block button (巨山狂熊寶珠)
    bool has_bear_orb = false;
    for (auto& p : g.players) if (p.orb_key == "EQ_K_BEAR" && p.alive) { has_bear_orb = true; break; }
    if (has_bear_orb && cp.orb_key == "EQ_K_BEAR") {
        dpp::component blk_btn;
        blk_btn.set_type(dpp::cot_button)
            .set_label("防禦")
            .set_id("raid_block_" + ch_s + "_" + uid_s)
            .set_style(dpp::cos_secondary)
            .set_emoji("🛡️", 0);
        row1.add_component(blk_btn);
    }

    // 維京寶珠：被動狂暴，無需按鈕

    msg.add_component(row1);

    dpp::component row2;
    row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("刷新狀態").set_id("raid_refresh_" + ch_s)
        .set_style(dpp::cos_secondary).set_emoji("🔄", 0));
    msg.add_component(row2);

    return msg;
}

// ─── End message ──────────────────────────────────────────────────────────────

static dpp::message make_raid_end_msg(const RaidGame& g,
    const std::vector<std::pair<std::string, std::string>>& reward_lines) {
    dpp::embed e;
    if (g.victory) {
        e.set_title("🏆 討伐成功！").set_color(0xF1C40F);
        e.set_description("**" + g.boss_name + "** 已被擊倒！");
    } else {
        e.set_title("💀 討伐失敗").set_color(0x808080);
        e.set_description("全滅或時間到，**" + g.boss_name + "** 存活了下來。");
    }
    e.set_image(g.boss_image);

    std::string reward_str;
    for (auto& [name, drops] : reward_lines)
        reward_str += "**" + name + "**：" + drops + "\n";
    if (!reward_str.empty())
        e.add_field("🎁 獎勵", reward_str, false);

    dpp::message msg; msg.add_embed(e); return msg;
}

// ─── Advance turn ─────────────────────────────────────────────────────────────
// Returns true if game should be updated (always), modifies g in place.
// Calls boss_attack_fn when it's the boss's turn.

static void raid_advance_turn(RaidGame& g) {
    if (!g.boss_turn) {
        // 迅捷：extra turn → 保持當前玩家，清旗標
        // 若同時有強攻疲勞，疲勞優先消耗額外行動
        if (g.speed_extra_pending) {
            g.speed_extra_pending = false;
            auto& cp = g.players[g.current_player];
            if (cp.power_skip) {
                cp.power_skip = false;
                if (!g.log_line.empty()) g.log_line += "\n";
                g.log_line += "💤 **" + cp.display_name + "** 強攻疲勞消耗了先鋒再行動！";
                // fall through → 正常換人
            } else {
                return;
            }
        }

        int start = g.current_player;
        int next = (g.current_player + 1) % (int)g.players.size();
        while (true) {
            if (next <= start) {
                g.boss_turn = true;
                break;
            }
            if (g.players[next].alive) {
                if (g.players[next].stunned_turns > 0) {
                    g.players[next].stunned_turns--;
                    if (!g.log_line.empty()) g.log_line += "\n";
                    g.log_line += "🔒 **" + g.players[next].display_name + "** 被封鎖，跳過本回合！";
                    next = (next + 1) % (int)g.players.size();
                    continue;
                }
                if (g.players[next].power_skip) {
                    g.players[next].power_skip = false;
                    if (!g.log_line.empty()) g.log_line += "\n";
                    g.log_line += "💤 **" + g.players[next].display_name + "** 強攻後疲勞，跳過本回合！";
                    next = (next + 1) % (int)g.players.size();
                    continue;
                }
                g.current_player = next;
                g.boss_turn = false;
                break;
            }
            next = (next + 1) % (int)g.players.size();
        }
        bool any_alive = false;
        for (auto& p : g.players) if (p.alive) { any_alive = true; break; }
        if (!any_alive) { g.game_over = true; g.victory = false; }
    } else {
        // After boss turn → new round, reset to first alive non-stunned player
        g.round++;
        g.block_active = false;
        g.round_first_action = true;
        g.boss_turn = false;
        // 重置迅捷再行動旗標
        for (auto& p : g.players) p.speed_extra_used = false;
        g.current_player = -1;
        for (int i = 0; i < (int)g.players.size(); i++) {
            if (!g.players[i].alive) continue;
            if (g.players[i].stunned_turns > 0) {
                g.players[i].stunned_turns--;
                if (!g.log_line.empty()) g.log_line += "\n";
                g.log_line += "🔒 **" + g.players[i].display_name + "** 被封鎖，跳過本回合！";
                continue;
            }
            if (g.players[i].power_skip) {
                g.players[i].power_skip = false;
                if (!g.log_line.empty()) g.log_line += "\n";
                g.log_line += "💤 **" + g.players[i].display_name + "** 強攻後疲勞，跳過本回合！";
                continue;
            }
            g.current_player = i;
            break;
        }
        if (g.current_player == -1) {
            // 全員封鎖或陣亡
            bool any_alive = false;
            for (auto& p : g.players) if (p.alive) { any_alive = true; break; }
            if (!any_alive) { g.game_over = true; g.victory = false; }
            else { g.boss_turn = true; } // 全員封鎖 → boss 再行動
        }
    }
}

// ─── Handle stunned skip ──────────────────────────────────────────────────────
// Returns true if the current player was stunned and turn was advanced
static bool raid_skip_if_stunned(RaidGame& g, std::string& log_out) {
    if (g.boss_turn || g.game_over) return false;
    auto& cp = g.players[g.current_player];
    if (!cp.alive || cp.stunned_turns <= 0) return false;
    cp.stunned_turns--;
    log_out = "🔒 **" + cp.display_name + "** 被封鎖，跳過本回合（剩餘 " +
              std::to_string(cp.stunned_turns) + " 回合）";
    raid_advance_turn(g);
    return true;
}

// ─── 雅典娜 heal at turn start ────────────────────────────────────────────────
static std::string raid_athena_heal(RaidGame& g) {
    if (g.boss_turn || g.game_over) return "";
    auto& cp = g.players[g.current_player];
    if (!cp.alive || cp.orb_key != "EQ_K_ATHENA") return "";
    if (raid_rand(1, 100) > 20) return "";
    std::string healed;
    for (auto& p : g.players) {
        if (!p.alive) continue;
        int old_hp = p.hp;
        p.hp = std::min(p.hp + 5, p.max_hp);
        if (p.hp > old_hp) {
            if (!healed.empty()) healed += "、";
            healed += p.display_name + "(+" + std::to_string(p.hp - old_hp) + ")";
        }
    }
    if (healed.empty()) return "";
    return "✨ **雅典娜祝福** — " + healed + " 恢復 HP！";
}

// ─── Boss action processor ────────────────────────────────────────────────────

static std::string raid_do_boss_turn(RaidGame& g) {
    std::vector<int> alive_idx;
    for (int i = 0; i < (int)g.players.size(); i++)
        if (g.players[i].alive) alive_idx.push_back(i);
    if (alive_idx.empty()) { g.game_over = true; g.victory = false; return "全滅！"; }

    BossAttack atk = pick_boss_attack(
        g.last_boss_aoe || g.last_boss_single,  // 上次AOE或單體 → 禁AOE
        g.last_boss_single,                      // 上次單體 → 禁單體
        g.boss_key
    );
    int aoe_dmg    = (int)(g.boss_atk * 0.85);  // 85% of atk, ~34
    int single_dmg = (int)(g.boss_atk * 1.625); // 162.5% of atk, ~65
    int heal_amt   = 120;
    std::string log;

    // 無名女神寶珠：組隊全體+2防（每多一個持有者再+2，組隊模式限定）
    int ur_orb_count = 0;
    if (g.players.size() > 1)
        for (auto& p : g.players) if (p.alive && p.orb_key == "EQ_K_UR") ur_orb_count++;
    bool ur_def_active = ur_orb_count > 0;
    int ur_def_bonus = ur_orb_count * 2;

    switch (atk) {
    case BossAttack::AOE: {
        log = "🌊 **" + g.boss_name + "** 發動【全體攻擊】！";
        if (ur_def_active) log += "\n🌟 *女神守護 ×" + std::to_string(ur_orb_count) + "：全體 -" + std::to_string(ur_def_bonus) + " 傷害*";
        for (int idx : alive_idx) {
            auto& p = g.players[idx];
            int raw = aoe_dmg;
            if (g.block_active) raw = (int)(raw * 0.8);
            int dmg = std::max(1, raw - p.def - ur_def_bonus);
            p.hp -= dmg;
            log += "\n  → " + p.display_name + " 受到 **" + std::to_string(dmg) + "** 點傷害";
            if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
            if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
                p.latus_orb_triggered = true; p.hp = p.max_hp / 2;
                log += " 🔶（拉圖斯寶珠！回復至50%）";
            }
        }
        break;
    }
    case BossAttack::SINGLE: {
        int tidx = alive_idx[raid_rand(0, (int)alive_idx.size()-1)];
        auto& p = g.players[tidx];
        int raw = single_dmg;
        if (g.block_active) raw = raw / 2;
        int dmg = std::max(1, raw - p.def - ur_def_bonus);
        p.hp -= dmg;
        log = "🎯 **" + g.boss_name + "** 對 **" + p.display_name +
              "** 發動【集中攻擊】，造成 **" + std::to_string(dmg) + "** 點傷害！";
        if (ur_def_active) log += "\n🌟 *女神守護 ×" + std::to_string(ur_orb_count) + "：-" + std::to_string(ur_def_bonus) + " 傷害*";
        if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
        if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
            p.latus_orb_triggered = true; p.hp = p.max_hp / 2;
            log += "\n🔶 **" + p.display_name + "** 拉圖斯寶珠發動！回復至 50% HP！";
        }
        break;
    }
    case BossAttack::STUN: {
        int tidx = alive_idx[raid_rand(0, (int)alive_idx.size()-1)];
        auto& p = g.players[tidx];
        p.stunned_turns = 2;
        log = "🔒 **" + g.boss_name + "** 封鎖了 **" + p.display_name + "** 的行動！（封鎖 **2** 回合）";
        break;
    }
    case BossAttack::HEAL: {
        int old_hp = g.boss_hp;
        g.boss_hp = std::min(g.boss_hp + heal_amt, g.boss_max_hp);
        int actual = g.boss_hp - old_hp;
        log = "💚 **" + g.boss_name + "** 回復了 **" + std::to_string(actual) + "** 點 HP！";
        break;
    }
    case BossAttack::DRAIN: {
        // 生命汲取：對隨機目標造成 30% 最大HP真實傷害，龍王回復 100 HP
        int tidx = alive_idx[raid_rand(0, (int)alive_idx.size()-1)];
        auto& p = g.players[tidx];
        int dmg = std::max(1, p.max_hp * 30 / 100);
        p.hp = std::max(0, p.hp - dmg);
        int healed = std::min(100, g.boss_max_hp - g.boss_hp);
        g.boss_hp += healed;
        log = "🌑 **" + g.boss_name + "** 發動【生命汲取】！\n  → 奪取 **" + p.display_name + "** 的生命力 **" +
              std::to_string(dmg) + "** 點（無視防禦），自身回復 **" + std::to_string(healed) + "** HP！";
        if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
        if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
            p.latus_orb_triggered = true; p.hp = p.max_hp / 2;
            log += "\n🔶 **" + p.display_name + "** 拉圖斯寶珠發動！回復至 50% HP！";
        }
        break;
    }
    }

    g.block_active      = false;
    g.last_boss_aoe    = (atk == BossAttack::AOE);
    g.last_boss_single = (atk == BossAttack::SINGLE);

    // Check all dead
    bool any_alive = false;
    for (auto& p : g.players) if (p.alive) { any_alive = true; break; }
    if (!any_alive) { g.game_over = true; g.victory = false; }

    return log;
}

// ─── Helper: advance turn + auto-run boss turns until a player can act ────────

static void raid_finish_turn(RaidGame& g) {
    raid_advance_turn(g);
    while (!g.game_over && g.boss_turn) {
        std::string blog = raid_do_boss_turn(g);
        if (!g.log_line.empty()) g.log_line += "\n";
        g.log_line += blog;
        if (g.game_over) break;
        raid_advance_turn(g);
    }
}

// ─── Player action processor ──────────────────────────────────────────────────

// attack_type: 0=普通, 1=耗費氣力（隨機0.1~2.0×），2=強攻（2.0×跳回合）
static std::string raid_do_player_attack(RaidGame& g, int attack_type) {
    auto& cp = g.players[g.current_player];
    int base_atk = cp.atk;

    // 維京寶珠：狂暴被動 — HP 越低傷害越高
    double vk_mult = 1.0;
    std::string vk_log;
    if (cp.orb_key == "EQ_K_VIKING" && cp.max_hp > 0) {
        double hp_r = (double)cp.hp / cp.max_hp;
        if (hp_r < 0.25)      { vk_mult = 1.7; vk_log = " 🔥（狂暴×1.7）"; }
        else if (hp_r < 0.50) { vk_mult = 1.4; vk_log = " ⚡（憤怒×1.4）"; }
    }

    g.round_first_action = false;

    std::string extra_log = vk_log;
    std::string log;
    int atk_dmg = 0;

    if (attack_type == 2) {
        // 強攻：固定 2.0× 傷害，下回合跳過
        int raw = (int)(base_atk * 2.0 * vk_mult);
        int dmg = std::max(1, raw - g.boss_def);
        atk_dmg = dmg;
        g.boss_hp -= dmg;
        log = "💥 **" + cp.display_name + "** 強攻 Boss，造成 **" + std::to_string(dmg) + "** 點傷害！" + extra_log;
    } else if (attack_type == 1) {
        // 耗費氣力：隨機 0.1~2.0× 傷害，不跳回合
        double gamble_mult = 0.1 + raid_rand(0, 190) / 100.0;
        char gm_buf[8]; snprintf(gm_buf, sizeof(gm_buf), "%.1f", gamble_mult);
        int raw = (int)(base_atk * gamble_mult * vk_mult);
        int dmg = std::max(1, raw - g.boss_def);
        atk_dmg = dmg;
        g.boss_hp -= dmg;
        log = "🎲 **" + cp.display_name + "** 耗費氣力攻擊（×" + std::string(gm_buf) + "），造成 **" + std::to_string(dmg) + "** 點傷害！" + extra_log;
    } else {
        // 普通攻擊：1.0×
        int raw = (int)(base_atk * vk_mult);
        int dmg = std::max(1, raw - g.boss_def);
        atk_dmg = dmg;
        g.boss_hp -= dmg;
        log = "⚔️ **" + cp.display_name + "** 攻擊 Boss，造成 **" + std::to_string(dmg) + "** 點傷害！" + extra_log;
    }
    if (g.boss_hp <= 0) { g.boss_hp = 0; g.victory = true; g.game_over = true; log += " 🏆 Boss 倒下！"; }

    // 暗黑龍王寶珠：攻擊後回復傷害的 1/10（最多 10 HP）
    if (cp.orb_key == "EQ_K_DARKDRAGON" && atk_dmg > 0) {
        int heal = std::min(atk_dmg / 10, 10);
        if (heal > 0) {
            cp.hp = std::min(cp.hp + heal, cp.max_hp);
            log += "\n🌑 暗黑龍王寶珠：回復 **" + std::to_string(heal) + "** HP";
        }
    }

    if (!g.game_over) {
        // 強攻代價：下回合跳過
        if (attack_type == 2) cp.power_skip = true;

        // 迅捷寶珠：40% 機率多行動一次（每輪只能觸發一次）
        if (cp.orb_key == "EQ_K_SPEED" && !cp.speed_extra_used) {
            if (raid_rand(1, 100) <= 40) {
                cp.speed_extra_used = true;
                g.speed_extra_pending = true;
                log += "\n⚡ **先鋒再行動！** 可繼續出手！";
            }
        }
    }

    return log;
}

// ─── Rewards ──────────────────────────────────────────────────────────────────

static const std::vector<std::string> SHARD_KEYS_RAID = {
    "orb_shard_speed","orb_shard_athena","orb_shard_bear","orb_shard_viking","orb_shard_wargod"
};

static std::string raid_give_rewards(dpp::snowflake uid,
                                     const std::string& display_name) {
    std::vector<std::string> parts;
    // 2750 chips always（BB自然博物館中級套組：狩獵／王團獎勵籌碼 +3%）
    int64_t base_reward = 2750;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (col_set_bb_mid(uid)) base_reward = (int64_t)std::ceil(base_reward * 1.03);
        chip_data[uid].chips += base_reward;
    }
    parts.push_back("💰 " + std::to_string(base_reward) + " 籌碼");

    // 各項獨立判定，可同時觸發
    if (raid_rand(1, 100) <= 50) {
        // 50% growth item
        static const std::vector<std::string> GROW_ITEMS = {
            "grow_1","grow_2","grow_3","grow_4","grow_5","grow_6"
        };
        std::string item = GROW_ITEMS[raid_rand(0, (int)GROW_ITEMS.size()-1)];
        std::lock_guard<std::mutex> lk(data_mutex);
        inventory_data[uid][item]++;
        auto* vi = find_virtual_item(item);
        parts.push_back("🌱 " + (vi ? vi->name : item));
    }
    if (raid_rand(1, 100) <= 30) {
        // 30% orb shard 1-5
        int cnt = raid_rand(1, 5);
        std::string sk = SHARD_KEYS_RAID[raid_rand(0, (int)SHARD_KEYS_RAID.size()-1)];
        std::lock_guard<std::mutex> lk(data_mutex);
        inventory_data[uid][sk] += cnt;
        auto* vi = find_virtual_item(sk);
        parts.push_back("💎 " + (vi ? vi->name : sk) + " ×" + std::to_string(cnt));
    }
    if (raid_rand(1, 100) <= 10) {
        // 10% star
        std::lock_guard<std::mutex> lk(data_mutex);
        inventory_data[uid]["star_unknown"]++;
        parts.push_back("⭐ 未知的星星 ×1");
    }
    if (raid_rand(1, 100) <= 1) {
        // 1% random orb
        static const std::vector<std::string> ORB_KEYS = {
            "EQ_K_UR","EQ_K_SPEED","EQ_K_ATHENA","EQ_K_BEAR","EQ_K_VIKING","EQ_K_WARGOD"
        };
        std::string ok = ORB_KEYS[raid_rand(0, (int)ORB_KEYS.size()-1)];
        std::lock_guard<std::mutex> lk(data_mutex);
        inventory_data[uid][ok]++;
        auto* gi = find_gacha_item(ok);
        parts.push_back("🔮 " + (gi ? gi->name : ok) + " ×1");
    }

    // 1.22% half_refund (對不起我錯了)
    if (raid_rand(1, 10000) <= 122) {
        std::lock_guard<std::mutex> lk(data_mutex);
        inventory_data[uid]["half_refund"]++;
        parts.push_back("💸 對不起我錯了 ×1");
    }
    // 20% 拉圖斯寶珠碎片 1-4 片
    if (raid_rand(1, 100) <= 20) {
        int cnt = raid_rand(1, 4);
        std::lock_guard<std::mutex> lk(data_mutex);
        inventory_data[uid]["orb_shard_latus"] += cnt;
        parts.push_back("🔶 拉圖斯的寶珠碎片 ×" + std::to_string(cnt));
    }

    std::string result;
    for (int i = 0; i < (int)parts.size(); i++) {
        if (i) result += "、";
        result += parts[i];
    }
    return result;
}
