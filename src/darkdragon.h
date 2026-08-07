#pragma once
#include "types.h"
#include "pet.h"
#include <random>

// ─── RNG ─────────────────────────────────────────────────────────────────────

static std::mt19937& dd_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}
static int dd_rand(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(dd_rng());
}

// ─── Player stat helpers ──────────────────────────────────────────────────────

static int dd_eff_atk(const DDPlayer& p, bool triple) {
    int a = p.atk;
    // 維京：狂暴被動
    if (p.orb_key == "EQ_K_VIKING" && p.max_hp > 0) {
        double r = (double)p.hp / p.max_hp;
        if (r < 0.25)      a = (int)(a * 1.7);
        else if (r < 0.50) a = (int)(a * 1.4);
    }
    if (p.atk_down_turns > 0) a = a / 2;
    if (triple) a = (int)(a * 2.5);
    return a;
}

static int dd_eff_def(const DDPlayer& p, const DDGame& g) {
    int d = p.def;
    if (p.def_down_turns > 0) d = std::max(0, d - 4);
    // 無名女神寶珠：每有一名存活玩家裝備 UR，全體 +2 DEF
    for (auto& tp : g.players) if (tp.alive && tp.orb_key == "EQ_K_UR") d += 2;
    return d;
}

// ─── HP bar helper ────────────────────────────────────────────────────────────

static std::string dd_hp_bar(int hp, int max, int len = 10) {
    if (max <= 0) return "";
    int filled = (int)((double)hp / max * len + 0.5);
    filled = std::max(0, std::min(len, filled));
    std::string s;
    for (int i = 0; i < len; i++) s += (i < filled) ? "█" : "░";
    return s;
}

// ─── Combat message ───────────────────────────────────────────────────────────

static dpp::message make_dd_combat_msg(const DDGame& g) {
    dpp::embed e;
    e.set_color(0x6C3483);

    // Boss title
    std::string title = "Round " + std::to_string(g.round);
    if (g.boss_turn)
        title += " — 🔴 Boss 行動中...";
    else {
        auto& cp = g.players[g.current_player];
        title += " — " + cp.display_name + " 的回合";
    }
    e.set_title(title);

    // Three heads
    static const char* head_emojis[] = {"◀", "🔴", "▶"};
    std::string boss_desc;
    for (int i = 0; i < 3; i++) {
        auto& h = g.heads[i];
        if (!h.alive) {
            boss_desc += "~~" + h.name + "~~　💀\n";
        } else {
            boss_desc += std::string(head_emojis[i]) + " **" + h.name + "**　";
            boss_desc += dd_hp_bar(h.hp, h.max_hp, 8) + " " + std::to_string(h.hp) + "/" + std::to_string(h.max_hp) + "\n";
        }
    }
    // Altar
    if (!g.atk_triple) {
        boss_desc += "\n🏛️ **祭壇**　";
        for (int i = 0; i < 3; i++) boss_desc += (i < g.altar_hp) ? "❤️" : "🖤";
    } else {
        boss_desc += "\n💥 **祭壇已毀滅！全體 ATK×2.5！**";
    }
    e.set_description(boss_desc);

    // Players
    std::string players_str;
    for (int i = 0; i < (int)g.players.size(); i++) {
        auto& p = g.players[i];
        bool is_cur = (!g.boss_turn && i == g.current_player);
        std::string pline = (is_cur ? "▶ " : "  ") + p.display_name;
        if (!p.alive) {
            pline += " 💀";
        } else {
            if (p.at_altar)           pline += " 🏛️";
            if (p.stunned_turns > 0)  pline += " 🔒(" + std::to_string(p.stunned_turns) + ")";
            if (p.has_bomb)           pline += " 💣(" + std::to_string(p.bomb_turns) + ")";
            if (p.burning)            pline += " 🔥";
            if (p.atk_down_turns > 0) pline += " ⬇ATK";
            if (p.def_down_turns > 0) pline += " ⬇DEF";
            if (p.power_skip)         pline += " 💤";
            if (is_cur)               pline += " ◀️";
            // 寶珠圖示：固定顯示自己帶的寶珠；維京/拉圖斯觸發時圖示會變化
            if (!p.orb_key.empty()) {
                if (p.orb_key == "EQ_K_VIKING" && p.max_hp > 0) {
                    double r = (double)p.hp / p.max_hp;
                    if (r < 0.25)      pline += " 💥狂暴×1.7";
                    else if (r < 0.50) pline += " 🌀憤怒×1.4";
                    else               pline += " " + orb_baseline_icon(p.orb_key);
                } else if (p.orb_key == "EQ_K_LATUS") {
                    pline += p.latus_orb_triggered ? " ✨拉圖斯(已觸發)" : " " + orb_baseline_icon(p.orb_key);
                } else {
                    pline += " " + orb_baseline_icon(p.orb_key);
                }
            }
        }
        players_str += pline + "\n";
        if (p.alive)
            players_str += "  " + dd_hp_bar(p.hp, p.max_hp, 8) + " " + std::to_string(p.hp) + "/" + std::to_string(p.max_hp) + "\n";
    }
    e.add_field("👥 隊員", players_str, false);

    if (!g.log_line.empty())
        e.add_field("📋 最近行動", g.log_line, false);

    e.set_footer(dpp::embed_footer().set_text("⏱️ 30 分鐘內未完成視為失敗"));

    dpp::message msg;
    msg.add_embed(e);

    if (g.game_over || g.boss_turn) return msg;

    auto& cp = g.players[g.current_player];
    std::string uid_s = std::to_string((uint64_t)cp.uid);
    std::string ch_s  = std::to_string((uint64_t)g.channel_id);

    if (cp.at_altar) {
        // Altar actions
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🙏 祈禱").set_id("dd_pray_" + uid_s)
            .set_style(dpp::cos_success));
        if (!g.atk_triple && g.altar_hp > 0)
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("⛏️ 拆除祭壇").set_id("dd_demolish_" + uid_s)
                .set_style(dpp::cos_danger));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏊 移動至龍池").set_id("dd_pool_" + uid_s)
            .set_style(dpp::cos_secondary));
        msg.add_component(row);
    } else {
        // Pool actions
        if (g.selected_head < 0) {
            // Step 1: select target head
            dpp::component row; row.set_type(dpp::cot_action_row);
            for (int i = 0; i < 3; i++) {
                auto& h = g.heads[i];
                dpp::component b;
                b.set_type(dpp::cot_button)
                 .set_label(h.alive ? h.name : "💀 " + h.name)
                 .set_id("dd_target_" + uid_s + "_" + std::to_string(i))
                 .set_style(h.alive ? dpp::cos_danger : dpp::cos_secondary)
                 .set_disabled(!h.alive);
                row.add_component(b);
            }
            // Move to altar (only if altar exists)
            if (!g.atk_triple)
                row.add_component(dpp::component().set_type(dpp::cot_button)
                    .set_label("🏛️ 移動至祭壇").set_id("dd_altar_" + uid_s)
                    .set_style(dpp::cos_secondary));
            msg.add_component(row);
        } else {
            // Step 2: select attack type
            auto& th = g.heads[g.selected_head];
            dpp::component row; row.set_type(dpp::cot_action_row);
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("⚔️ 攻擊　→ " + th.name)
                .set_id("dd_atk_" + uid_s)
                .set_style(dpp::cos_danger));
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🎲 耗費氣力（×0.1~2.0）")
                .set_id("dd_gamble_" + uid_s)
                .set_style(dpp::cos_primary));
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("💥 強攻（×2.0，跳回合）")
                .set_id("dd_pow_" + uid_s)
                .set_style(dpp::cos_danger));
            // Bear block
            bool has_bear = false;
            for (auto& p : g.players) if (p.orb_key == "EQ_K_BEAR" && p.alive) { has_bear = true; break; }
            if (has_bear && cp.orb_key == "EQ_K_BEAR")
                row.add_component(dpp::component().set_type(dpp::cot_button)
                    .set_label("🛡️ 防禦").set_id("dd_block_" + uid_s)
                    .set_style(dpp::cos_secondary));
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("↩ 返回").set_id("dd_back_" + uid_s)
                .set_style(dpp::cos_secondary));
            msg.add_component(row);
        }
    }

    // Refresh button
    dpp::component ref_row; ref_row.set_type(dpp::cot_action_row);
    ref_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 刷新狀態").set_id("dd_refresh_" + ch_s)
        .set_style(dpp::cos_secondary));
    msg.add_component(ref_row);

    return msg;
}

// ─── End message ──────────────────────────────────────────────────────────────

static dpp::message make_dd_end_msg(const DDGame& g,
    const std::vector<std::pair<std::string,std::string>>& reward_lines) {
    dpp::embed e;
    if (g.victory) {
        e.set_title("🏆 討伐成功！暗黑龍王已被擊倒！").set_color(0xF1C40F);
        if (g.practice_mode) e.set_description("🏋️ 練習模式：無獎勵");
    } else {
        e.set_title("💀 討伐失敗").set_color(0x808080);
        std::string fdesc = "全滅或時間到，**暗黑龍王** 存活了下來。";
        if (g.practice_mode) fdesc += "\n🏋️ 練習模式：寵物不受傷";
        e.set_description(fdesc);
    }
    if (!reward_lines.empty()) {
        std::string rs;
        for (auto& [nm, dr] : reward_lines) rs += "**" + nm + "**：" + dr + "\n";
        e.add_field("🎁 獎勵", rs, false);
    }
    dpp::message msg; msg.add_embed(e); return msg;
}

// ─── Advance turn ─────────────────────────────────────────────────────────────

static void dd_advance_turn(DDGame& g) {
    if (!g.boss_turn) {
        if (g.speed_extra_pending) {
            g.speed_extra_pending = false;
            auto& cp = g.players[g.current_player];
            if (cp.power_skip) {
                cp.power_skip = false;
                if (!g.log_line.empty()) g.log_line += "\n";
                g.log_line += "💤 **" + cp.display_name + "** 強攻疲勞消耗了先鋒再行動！";
            } else { return; }
        }
        int start = g.current_player;
        int next  = (g.current_player + 1) % (int)g.players.size();
        while (true) {
            if (next <= start) { g.boss_turn = true; break; }
            auto& p = g.players[next];
            if (!p.alive) { next = (next+1) % (int)g.players.size(); continue; }
            if (p.stunned_turns > 0) {
                p.stunned_turns--;
                if (!g.log_line.empty()) g.log_line += "\n";
                g.log_line += "🔒 **" + p.display_name + "** 被封鎖，跳過本回合！";
                next = (next+1) % (int)g.players.size(); continue;
            }
            if (p.power_skip) {
                p.power_skip = false;
                if (!g.log_line.empty()) g.log_line += "\n";
                g.log_line += "💤 **" + p.display_name + "** 強攻後疲勞，跳過本回合！";
                next = (next+1) % (int)g.players.size(); continue;
            }
            g.current_player = next; break;
        }
        bool any = false;
        for (auto& p : g.players) if (p.alive) { any = true; break; }
        if (!any) { g.game_over = true; g.victory = false; }
    } else {
        g.round++;
        g.block_active = false;
        g.round_first_action = true;
        g.boss_turn = false;
        for (auto& p : g.players) p.speed_extra_used = false;
        g.current_player = -1;
        for (int i = 0; i < (int)g.players.size(); i++) {
            auto& p = g.players[i];
            if (!p.alive) continue;
            if (p.stunned_turns > 0) {
                p.stunned_turns--;
                if (!g.log_line.empty()) g.log_line += "\n";
                g.log_line += "🔒 **" + p.display_name + "** 被封鎖，跳過本回合！";
                continue;
            }
            if (p.power_skip) {
                p.power_skip = false;
                if (!g.log_line.empty()) g.log_line += "\n";
                g.log_line += "💤 **" + p.display_name + "** 強攻後疲勞，跳過本回合！";
                continue;
            }
            g.current_player = i; break;
        }
        if (g.current_player == -1) {
            bool any = false;
            for (auto& p : g.players) if (p.alive) { any = true; break; }
            if (!any) { g.game_over = true; g.victory = false; }
            else       g.boss_turn = true;
        }
    }
}

// ─── Player attack helper ─────────────────────────────────────────────────────

static std::string dd_do_attack(DDGame& g, int attack_type) {
    auto& cp = g.players[g.current_player];
    int head_idx = g.selected_head;
    g.selected_head = -1;
    if (head_idx < 0 || head_idx >= 3 || !g.heads[head_idx].alive)
        return "❌ 無效目標";
    auto& h = g.heads[head_idx];
    int eff_atk = dd_eff_atk(cp, g.atk_triple);
    g.round_first_action = false;
    std::string log;

    // Viking log
    std::string vk_log;
    if (cp.orb_key == "EQ_K_VIKING" && cp.max_hp > 0) {
        double r = (double)cp.hp / cp.max_hp;
        if (r < 0.25)      vk_log = " 🔥狂暴×1.7";
        else if (r < 0.50) vk_log = " ⚡憤怒×1.4";
    }

    int raw = 0;
    int atk_dmg = 0;
    if (attack_type == 2) {
        // 強攻
        raw = (int)(eff_atk * 2.0);
        int dmg = std::max(1, raw - h.def);
        atk_dmg = dmg;
        h.hp -= dmg;
        log = "💥 **" + cp.display_name + "** 強攻 **" + h.name + "**，造成 **" + std::to_string(dmg) + "** 傷害！" + vk_log;
        cp.power_skip = true;
    } else if (attack_type == 1) {
        // 耗費氣力
        double mult = 0.1 + dd_rand(0, 190) / 100.0;
        char buf[8]; snprintf(buf, sizeof(buf), "%.1f", mult);
        raw = (int)(eff_atk * mult);
        int dmg = std::max(1, raw - h.def);
        atk_dmg = dmg;
        h.hp -= dmg;
        log = "🎲 **" + cp.display_name + "** 耗費氣力（×" + std::string(buf) + "）攻擊 **" + h.name + "**，造成 **" + std::to_string(dmg) + "** 傷害！" + vk_log;
    } else {
        // 普通攻擊
        raw = eff_atk;
        int dmg = std::max(1, raw - h.def);
        atk_dmg = dmg;
        h.hp -= dmg;
        log = "⚔️ **" + cp.display_name + "** 攻擊 **" + h.name + "**，造成 **" + std::to_string(dmg) + "** 傷害！" + vk_log;
    }

    if (h.hp <= 0) {
        h.hp = 0; h.alive = false;
        log += "\n💥 **" + h.name + "** 被擊倒！";
    }

    // 暗黑龍王寶珠：攻擊後回復傷害的 1/10（最多 10 HP）
    if (cp.orb_key == "EQ_K_DARKDRAGON" && atk_dmg > 0) {
        int heal = std::min(atk_dmg / 10, 10);
        if (heal > 0) {
            cp.hp = std::min(cp.hp + heal, cp.max_hp);
            log += "\n🌑 暗黑龍王寶珠：回復 **" + std::to_string(heal) + "** HP";
        }
    }

    // Check victory
    bool all_dead = true;
    for (auto& hh : g.heads) if (hh.alive) { all_dead = false; break; }
    if (all_dead) { g.victory = true; g.game_over = true; }

    // 迅捷
    if (!g.game_over && cp.orb_key == "EQ_K_SPEED" && !cp.speed_extra_used) {
        if (dd_rand(1,100) <= 40) {
            cp.speed_extra_used = true;
            g.speed_extra_pending = true;
            log += "\n⚡ **先鋒再行動！**";
        }
    }

    return log;
}

// ─── Boss turn ────────────────────────────────────────────────────────────────

static std::string dd_do_boss_turn(DDGame& g) {
    std::string log;

    // 1. Apply burning damage
    for (auto& p : g.players) {
        if (!p.alive || !p.burning) continue;
        p.burning = false;
        int dmg = 10;
        p.hp -= dmg;
        if (!log.empty()) log += "\n";
        log += "🔥 **" + p.display_name + "** 燃燒受到 **10** 點傷害！";
        if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
        if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
            p.latus_orb_triggered = true; p.hp = p.max_hp / 2;
            log += " 🔶（拉圖斯寶珠！）";
        }
    }

    // 2. Decrement per-player debuffs and bomb
    for (auto& p : g.players) {
        if (!p.alive) continue;
        if (p.atk_down_turns > 0) p.atk_down_turns--;
        if (p.def_down_turns > 0) p.def_down_turns--;
        if (p.has_bomb) {
            p.bomb_turns--;
            if (p.bomb_turns <= 0) {
                p.hp = 0; p.alive = false; p.has_bomb = false;
                if (!log.empty()) log += "\n";
                log += "💣 **" + p.display_name + "** 的未來炸彈引爆！ 💀";
            }
        }
    }

    // 3. Check altar counter (before heads act)
    bool anyone_at_altar = false;
    for (auto& p : g.players) if (p.alive && p.at_altar) { anyone_at_altar = true; break; }
    if (anyone_at_altar) g.altar_counter++;
    else                  g.altar_counter = 0;

    // ── LEFT HEAD ─────────────────────────────────────────────────────────────
    auto& lh = g.heads[0];
    if (lh.alive) {
        // Get pool players
        std::vector<int> pool_idx;
        for (int i = 0; i < (int)g.players.size(); i++)
            if (g.players[i].alive && !g.players[i].at_altar) pool_idx.push_back(i);

        int sk = lh.skill_idx;
        lh.skill_idx = (lh.skill_idx + 1) % 4;

        if (!log.empty()) log += "\n";
        if (sk == 0) {
            // 龍焰掃射：AOE 75%
            int raw = (int)(lh.atk * 0.75);
            log += "🌊 **左頭** 【龍焰掃射】！";
            for (int idx : pool_idx) {
                auto& p = g.players[idx];
                int dmg = std::max(1, raw - dd_eff_def(p, g));
                if (g.block_active) dmg = std::max(1, (int)(dmg * 0.8));
                p.hp -= dmg;
                log += "\n  → " + p.display_name + " 受到 **" + std::to_string(dmg) + "** 傷害";
                if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
                if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
                    p.latus_orb_triggered = true; p.hp = p.max_hp / 2; log += " 🔶（拉圖斯！）";
                }
            }
        } else if (sk == 1) {
            // 龍爪橫掃：2隨機
            log += "🐉 **左頭** 【龍爪橫掃】！";
            if (!pool_idx.empty()) {
                for (int t = 0; t < 2; t++) {
                    if (pool_idx.empty()) break;
                    int ti = pool_idx[dd_rand(0, (int)pool_idx.size()-1)];
                    auto& p = g.players[ti];
                    int dmg = std::max(1, lh.atk - dd_eff_def(p, g));
                    if (g.block_active) dmg = std::max(1, dmg / 2);
                    p.hp -= dmg;
                    log += "\n  → " + p.display_name + " 受到 **" + std::to_string(dmg) + "** 傷害";
                    if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
                    if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
                        p.latus_orb_triggered = true; p.hp = p.max_hp / 2; log += " 🔶（拉圖斯！）";
                    }
                }
            }
        } else if (sk == 2) {
            // 龍焰爆炎：AOE 110%
            int raw = (int)(lh.atk * 1.1);
            log += "💥 **左頭** 【龍焰爆炎】！";
            for (int idx : pool_idx) {
                auto& p = g.players[idx];
                int dmg = std::max(1, raw - dd_eff_def(p, g));
                if (g.block_active) dmg = std::max(1, (int)(dmg * 0.8));
                p.hp -= dmg;
                log += "\n  → " + p.display_name + " 受到 **" + std::to_string(dmg) + "** 傷害";
                if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
                if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
                    p.latus_orb_triggered = true; p.hp = p.max_hp / 2; log += " 🔶（拉圖斯！）";
                }
            }
        } else {
            // 黑暗恢復：三頭各+50
            log += "💚 **左頭** 【黑暗恢復】！三頭各回復 HP！";
            for (auto& h : g.heads) {
                if (!h.alive) continue;
                int old = h.hp;
                h.hp = std::min(h.hp + 50, h.max_hp);
                if (h.hp > old) log += "\n  → " + h.name + " +50 HP";
            }
        }
    }

    // ── MIDDLE HEAD ───────────────────────────────────────────────────────────
    auto& mh = g.heads[1];
    if (mh.alive) {
        g.bomb_cooldown--;
        if (!log.empty()) log += "\n";

        if (g.altar_counter >= 2) {
            // 振翅：send all altar players back, reset counter
            log += "💨 **中頭** 【振翅】！所有人被送回龍池！";
            for (auto& p : g.players) {
                if (p.alive && p.at_altar) {
                    p.at_altar = false;
                    log += "\n  → " + p.display_name + " 被送回龍池";
                }
            }
            g.altar_counter = 0;
        } else if (g.bomb_cooldown <= 0) {
            // 未來炸彈：random non-bomb, non-stunned, alive pool player
            std::vector<int> valid;
            for (int i = 0; i < (int)g.players.size(); i++) {
                auto& p = g.players[i];
                if (p.alive && !p.has_bomb && p.stunned_turns == 0) valid.push_back(i);
            }
            g.bomb_cooldown = dd_rand(3, 4);
            if (!valid.empty()) {
                int ti = valid[dd_rand(0, (int)valid.size()-1)];
                auto& p = g.players[ti];
                p.has_bomb    = true;
                p.bomb_turns  = 3;
                log += "💣 **中頭** 【未來炸彈】！→ **" + p.display_name + "** 身上出現炸彈！（3 回合後引爆）";
            } else {
                log += "💣 **中頭** 嘗試投彈但找不到目標…";
            }
        } else {
            // 黑暗鎖鍊 / 暗黑凝視 交替
            std::vector<int> alive_idx;
            for (int i = 0; i < (int)g.players.size(); i++)
                if (g.players[i].alive) alive_idx.push_back(i);

            if (mh.chain_cd == 0) {
                // 黑暗鎖鍊：single 130%
                int raw = (int)(mh.atk * 1.3);
                if (!alive_idx.empty()) {
                    int ti = alive_idx[dd_rand(0, (int)alive_idx.size()-1)];
                    auto& p = g.players[ti];
                    int dmg = std::max(1, raw - dd_eff_def(p, g));
                    if (g.block_active) dmg = std::max(1, (int)(dmg * 0.8));
                    p.hp -= dmg;
                    log += "⛓️ **中頭** 【黑暗鎖鍊】→ **" + p.display_name + "** 受到 **" + std::to_string(dmg) + "** 傷害！";
                    if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
                    if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
                        p.latus_orb_triggered = true; p.hp = p.max_hp / 2; log += " 🔶（拉圖斯！）";
                    }
                }
                mh.chain_cd = 1;
            } else {
                // 暗黑凝視：AOE 50% + 下回ATK-20%（這裡用 atk_down_turns 模擬）
                int raw = (int)(mh.atk * 0.5);
                log += "👁️ **中頭** 【暗黑凝視】！AOE + 力量削弱！";
                for (auto& p : g.players) {
                    if (!p.alive) continue;
                    int dmg = std::max(1, raw - dd_eff_def(p, g));
                    p.hp -= dmg;
                    p.atk_down_turns = std::max(p.atk_down_turns, 1); // 下一次 boss 回合 -ATK
                    log += "\n  → " + p.display_name + " 受到 **" + std::to_string(dmg) + "** 傷害，力量被削弱！";
                    if (p.hp <= 0) { p.hp = 0; p.alive = false; log += " 💀"; }
                    if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
                        p.latus_orb_triggered = true; p.hp = p.max_hp / 2; log += " 🔶（拉圖斯！）";
                    }
                }
                mh.chain_cd = 0;
            }
        }
    }

    // ── RIGHT HEAD ────────────────────────────────────────────────────────────
    auto& rh = g.heads[2];
    if (rh.alive) {
        std::vector<int> pool_idx;
        for (int i = 0; i < (int)g.players.size(); i++)
            if (g.players[i].alive && !g.players[i].at_altar) pool_idx.push_back(i);
        // 龍池無目標時，退而求其次瞄準所有存活玩家（含祭壇）
        std::vector<int> rh_targets = pool_idx;
        if (rh_targets.empty())
            for (int i = 0; i < (int)g.players.size(); i++)
                if (g.players[i].alive) rh_targets.push_back(i);

        // 右頭 HP ≤ 50% 首次觸發狂暴，連續 5 回合龍息燃燒
        if (!rh.rage_triggered && rh.hp <= rh.max_hp / 2) {
            rh.rage_triggered = true;
            rh.rage_turns = 5;
            rh.atk += 10;
            if (!log.empty()) log += "\n";
            log += "🔥 **右頭** 進入**狂暴模式**！ATK+10，連續 5 回合龍息燃燒！";
        }

        int sk;
        if (rh.rage_turns > 0) {
            sk = 3; // 龍息燃燒
            rh.rage_turns--;
        } else {
            sk = rh.skill_idx;
            rh.skill_idx = (rh.skill_idx + 1) % 4;
        }

        if (!log.empty()) log += "\n";
        if (sk == 0) {
            // 黑暗封鎖：stun，不鎖炸彈持有者
            std::vector<int> valid;
            for (int i : rh_targets) if (!g.players[i].has_bomb && g.players[i].stunned_turns == 0) valid.push_back(i);
            if (valid.empty()) valid = rh_targets; // fallback
            if (!valid.empty()) {
                int ti = valid[dd_rand(0, (int)valid.size()-1)];
                auto& p = g.players[ti];
                p.stunned_turns = 2;
                log += "🔒 **右頭** 【黑暗封鎖】→ **" + p.display_name + "** 被封鎖 2 回合！";
            }
        } else if (sk == 1) {
            // 力量竊取：ATK-50% 2回合
            if (!rh_targets.empty()) {
                int ti = rh_targets[dd_rand(0, (int)rh_targets.size()-1)];
                auto& p = g.players[ti];
                p.atk_down_turns = 2;
                log += "💔 **右頭** 【力量竊取】→ **" + p.display_name + "** ATK -50%（2 回合）！";
            }
        } else if (sk == 2) {
            // 防禦瓦解：DEF-4 2回合
            if (!rh_targets.empty()) {
                int ti = rh_targets[dd_rand(0, (int)rh_targets.size()-1)];
                auto& p = g.players[ti];
                p.def_down_turns = 2;
                log += "🛡️ **右頭** 【防禦瓦解】→ **" + p.display_name + "** DEF -4（2 回合）！";
            }
        } else {
            // 龍息燃燒：AOE 60% + burning
            int raw = (int)(rh.atk * 0.6);
            log += "🔥 **右頭** 【龍息燃燒】！AOE + 燃燒！";
            for (int idx : rh_targets) {
                auto& p = g.players[idx];
                int dmg = std::max(1, raw - dd_eff_def(p, g));
                if (g.block_active) dmg = std::max(1, (int)(dmg * 0.8));
                p.hp -= dmg;
                p.burning = true;
                log += "\n  → " + p.display_name + " 受到 **" + std::to_string(dmg) + "** 傷害（下回合再燃燒 10）";
                if (p.hp <= 0) { p.hp = 0; p.alive = false; p.burning = false; log += " 💀"; }
                if (p.alive && p.orb_key == "EQ_K_LATUS" && !p.latus_orb_triggered && p.hp <= p.max_hp / 5) {
                    p.latus_orb_triggered = true; p.hp = p.max_hp / 2; log += " 🔶（拉圖斯！）";
                }
            }
        }
    }

    g.block_active = false;

    // Check all players dead
    bool any_alive = false;
    for (auto& p : g.players) if (p.alive) { any_alive = true; break; }
    if (!any_alive) { g.game_over = true; g.victory = false; }

    return log;
}

// ─── Helper: advance turn and auto-process boss turns until a player can act ──

static void dd_finish_turn(DDGame& g) {
    dd_advance_turn(g);
    while (!g.game_over && g.boss_turn) {
        std::string blog = dd_do_boss_turn(g);
        if (!g.log_line.empty()) g.log_line += "\n";
        g.log_line += blog;
        if (g.game_over) break;
        dd_advance_turn(g);
    }
}

// ─── Rewards ─────────────────────────────────────────────────────────────────

static const std::vector<std::string> DD_GROW_ITEMS = {
    "grow_1","grow_2","grow_3","grow_4","grow_5","grow_6"
};
static const std::vector<std::string> DD_SHARD_KEYS = {
    "orb_shard_speed","orb_shard_athena","orb_shard_bear","orb_shard_viking","orb_shard_wargod"
};
static const std::vector<std::string> DD_ORB_KEYS = {
    "EQ_K_UR","EQ_K_SPEED","EQ_K_ATHENA","EQ_K_BEAR","EQ_K_VIKING","EQ_K_WARGOD"
};

// 呼叫前必須持有 data_mutex
static std::string dd_give_rewards_one(dpp::snowflake uid) {
    std::vector<std::string> parts;

    // 固定 4400 籌碼（BB自然博物館中級套組：狩獵／王團獎勵籌碼 +3%；呼叫前已持有 data_mutex，直接查不用再上鎖）
    int64_t base_reward = col_set_bb_mid(uid) ? (int64_t)std::ceil(4400 * 1.03) : 4400;
    chip_data[uid].chips += base_reward;
    parts.push_back("💰 " + std::to_string(base_reward) + " 籌碼");

    // 80% 成長道具
    if (dd_rand(1, 100) <= 80) {
        std::string item = DD_GROW_ITEMS[dd_rand(0, (int)DD_GROW_ITEMS.size()-1)];
        inventory_data[uid][item]++;
        auto* vi = find_virtual_item(item);
        parts.push_back("🌱 " + (vi ? vi->name : item));
    }
    // 50% 寶珠碎片 1~5
    if (dd_rand(1, 100) <= 50) {
        int cnt = dd_rand(1, 5);
        std::string sk = DD_SHARD_KEYS[dd_rand(0, (int)DD_SHARD_KEYS.size()-1)];
        inventory_data[uid][sk] += cnt;
        auto* vi = find_virtual_item(sk);
        parts.push_back("💎 " + (vi ? vi->name : sk) + " ×" + std::to_string(cnt));
    }
    // 20% 未知的星星
    if (dd_rand(1, 100) <= 20) {
        inventory_data[uid]["star_unknown"]++;
        parts.push_back("⭐ 未知的星星 ×1");
    }
    // 2% 隨機寶珠
    if (dd_rand(1, 100) <= 2) {
        std::string ok = DD_ORB_KEYS[dd_rand(0, (int)DD_ORB_KEYS.size()-1)];
        inventory_data[uid][ok]++;
        auto* gi = find_gacha_item(ok);
        parts.push_back("🔮 " + (gi ? gi->name : ok) + " ×1");
    }
    // 2% 對不起我錯了
    if (dd_rand(1, 100) <= 2) {
        inventory_data[uid]["half_refund"]++;
        parts.push_back("💸 對不起我錯了 ×1");
    }
    // 20% 暗黑龍王寶珠碎片 1-4 片
    if (dd_rand(1, 100) <= 20) {
        int cnt = dd_rand(1, 4);
        inventory_data[uid]["orb_shard_darkdragon"] += cnt;
        parts.push_back("🌑 暗黑龍王的寶珠碎片 ×" + std::to_string(cnt));
    }

    std::string result;
    for (int i = 0; i < (int)parts.size(); i++) {
        if (i) result += "、";
        result += parts[i];
    }
    return result;
}
