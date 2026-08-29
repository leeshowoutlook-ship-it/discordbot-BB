#include "types.h"
#include "chips.h"
#include "darkdragon.h"
#include "handler_decls.h"

// ─── Button handler: dark dragon combat ───────────────────────────────────────
// Handles: rroom_start_ (when boss is dark_dragon), all dd_* buttons

void handle_dd_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    const dpp::user& user = ev.command.get_issuing_user();
    dpp::snowflake uid = user.id;
    std::string dn = user.global_name.empty() ? user.username : user.global_name;
    dpp::snowflake ch = ev.command.channel_id;

    // ── rroom_start_ for dark_dragon boss ────────────────────────────────────
    if (cid.rfind("rroom_start_", 0) == 0) {
        dpp::snowflake rch(std::stoull(cid.substr(12)));
        RaidRoom room;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = raid_rooms.find(rch);
            if (it == raid_rooms.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 找不到組隊房間！").set_flags(dpp::m_ephemeral)); return; }
            room = it->second;
            if (uid != room.host_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有開房者可以開始戰鬥！").set_flags(dpp::m_ephemeral)); return; }
            if ((int)room.member_uids.size() < 2) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 需要至少 2 名成員才可開始！").set_flags(dpp::m_ephemeral)); return; }
            if (!room.practice_mode) {
                for (auto& muid : room.member_uids) {
                    int cnt = 0;
                    auto iit = inventory_data.find(muid);
                    if (iit != inventory_data.end() && iit->second.count("weekly_hunt_scroll"))
                        cnt = iit->second.at("weekly_hunt_scroll");
                    if (cnt <= 0) {
                        std::string nm = room.member_names.count(muid) ? room.member_names.at(muid) : "某成員";
                        ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ **" + nm + "** 沒有每週怪物狩獵卷，無法開始！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                }
            }
            raid_rooms.erase(rch);
        }
        // 停掉房間的 10 分鐘逾時計時器，不然之後同頻道開新房間會被這個殘留的計時器誤殺
        if (room.timer_id) g_bot->stop_timer(room.timer_id);

        DDGame dg;
        dg.channel_id = room.channel_id; dg.started_at = time(nullptr); dg.bomb_cooldown = 3;
        dg.heads[0] = DDHead{"左頭", 600, 600, 27, 5};
        dg.heads[1] = DDHead{"中頭", 650, 650, 30, 3};
        dg.heads[2] = DDHead{"右頭", 450, 450, 24, 10};
        for (auto& muid : room.member_uids) {
            Pet pet2; PetStats ps; std::string orb_key;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto pit = pet_data.find(muid); if (pit != pet_data.end()) pet2 = pit->second;
              auto eit = equipped_data.find(muid); if (eit != equipped_data.end()) orb_key = eit->second.orb;
            }
            ps = calc_pet_stats(muid, pet2);
            { std::lock_guard<std::mutex> lk(data_mutex);
              int max_hp = ps.hp;
              apply_pet_basic_set_bonus(muid, pet2, ps.atk, ps.hp, max_hp, ps.def);
              ps.atk += col_pet_atk_bonus(muid);
              ps.def += col_pet_def_bonus(muid);
              ps.hp  += col_pet_hp_bonus(muid);
            }
            DDPlayer p;
            p.uid = muid; p.display_name = room.member_names.count(muid) ? room.member_names.at(muid) : "?";
            p.avatar_url = room.member_avatars.count(muid) ? room.member_avatars.at(muid) : "";
            p.hp = ps.hp; p.max_hp = ps.hp; p.atk = ps.atk; p.def = ps.def; p.crit_pct = ps.crit_pct;
            p.orb_key = orb_key; p.alive = true;
            if (orb_key == "EQ_K_UR" && room.member_uids.size() > 1) {
                auto* ur_gi = find_gacha_item("EQ_K_UR"); if (ur_gi) p.def = std::max(0, p.def - ur_gi->stat_val);
            }
            dg.players.push_back(p);
        }
        dg.current_player = -1;
        for (int i = 0; i < (int)dg.players.size(); i++) if (dg.players[i].alive) { dg.current_player = i; break; }
        dg.practice_mode = room.practice_mode;
        { std::lock_guard<std::mutex> lk(data_mutex); dd_games[rch] = dg; }
        auto ddmsg = make_dd_combat_msg(dg); ddmsg.channel_id = rch;
        g_bot->message_create(ddmsg, [rch](const dpp::confirmation_callback_t& cb) {
            if (cb.is_error()) return;
            auto mid = std::get<dpp::message>(cb.value).id;
            std::lock_guard<std::mutex> lk(data_mutex);
            if (dd_games.count(rch)) dd_games[rch].msg_id = mid;
        });
        ev.reply(dpp::ir_update_message, dpp::message("⚔️ **暗黑龍王**挑戰已開始！"));
        dpp::timer dd_tid = g_bot->start_timer([rch](dpp::timer t) {
            std::vector<dpp::snowflake> player_uids; bool is_practice = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = dd_games.find(rch); if (it == dd_games.end()) { g_bot->stop_timer(t); return; }
              auto& dg2 = it->second; if (dg2.game_over) { g_bot->stop_timer(t); return; }
              dg2.game_over = true; is_practice = dg2.practice_mode;
              for (auto& p : dg2.players) player_uids.push_back(p.uid);
              dd_games.erase(it);
            }
            if (!is_practice) {
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    for (auto puid : player_uids) {
                        auto& pet2 = pet_data[puid]; bool already = false;
                        for (auto& s : pet2.statuses) if (s == "受傷") { already = true; break; }
                        if (!already) pet2.statuses.push_back("受傷");
                    }
                }
                save_pet_data();
            }
            g_bot->stop_timer(t);
            dpp::message tm; tm.channel_id = rch;
            tm.set_content("⏱️ **暗黑龍王挑戰** 已超時，遠征失敗！");
            g_bot->message_create(tm);
        }, 1800);
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (dd_games.count(rch)) dd_games[rch].timer_id = dd_tid;
        }
        return;
    }

    // ── dd_* combat buttons ───────────────────────────────────────────────────
    if (cid.rfind("dd_", 0) == 0) {
        bool dd_need_pet_save = false;
        auto dd_try_end = [&](DDGame& dg, bool need_lock) -> bool {
            if (!dg.game_over) return false;
            g_bot->stop_timer(dg.timer_id);
            if (dg.victory) {
                std::vector<std::pair<std::string,std::string>> rewards;
                if (!dg.practice_mode) {
                    // 狩獵卷扣除與獎勵發放必須在同一個鎖區塊內，避免按鈕 handler 持鎖時重複加鎖（死鎖）
                    auto do_reward = [&]() {
                        for (auto& p : dg.players)
                            if (inventory_data.count(p.uid) && inventory_data[p.uid].count("weekly_hunt_scroll") && inventory_data[p.uid].at("weekly_hunt_scroll") > 0)
                                inventory_data[p.uid]["weekly_hunt_scroll"]--;
                        for (auto& p : dg.players) rewards.push_back({p.display_name, dd_give_rewards_one(p.uid)});
                    };
                    if (need_lock) { std::lock_guard<std::mutex> lk2(data_mutex); do_reward(); }
                    else           { do_reward(); }
                    save_chips(); save_inventory();
                }
                auto emsg = make_dd_end_msg(dg, rewards); emsg.channel_id = ch;
                dd_games.erase(ch); ev.reply(dpp::ir_update_message, emsg); return true;
            }
            if (!dg.practice_mode) {
                for (auto& p : dg.players) {
                    auto& pet2 = pet_data[p.uid]; bool al = false;
                    for (auto& s : pet2.statuses) if (s == "受傷") { al = true; break; }
                    if (!al) pet2.statuses.push_back("受傷");
                }
                dd_need_pet_save = true; // 鎖釋放後由 caller save
            }
            auto emsg = make_dd_end_msg(dg, {}); emsg.channel_id = ch;
            dd_games.erase(ch); ev.reply(dpp::ir_update_message, emsg); return true;
        };

        if (cid.rfind("dd_refresh_", 0) == 0) {
            dpp::snowflake dch(std::stoull(cid.substr(11)));
            DDGame g; bool found = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = dd_games.find(dch);
              if (it != dd_games.end()) { g = it->second; found = true; }
            }
            if (!found) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這場戰鬥已經結束了！").set_flags(dpp::m_ephemeral)); return;
            }
            // 不是原地編輯：刪掉舊訊息，在頻道底部重新發一則新的戰鬥狀態，避免被洗上去看不到
            ev.reply(dpp::ir_deferred_update_message, dpp::message());
            g_bot->message_delete(ev.command.message_id, ev.command.channel_id);
            auto newmsg = make_dd_combat_msg(g); newmsg.channel_id = dch;
            g_bot->message_create(newmsg, [dch](const dpp::confirmation_callback_t& cb) {
                if (cb.is_error()) return;
                auto mid = std::get<dpp::message>(cb.value).id;
                std::lock_guard<std::mutex> lk(data_mutex);
                if (dd_games.count(dch)) dd_games[dch].msg_id = mid;
            });
            return;
        }
        if (cid.rfind("dd_target_", 0) == 0) {
            std::string rest = cid.substr(10); size_t us = rest.rfind('_'); if (us == std::string::npos) return;
            dpp::snowflake bu(std::stoull(rest.substr(0, us))); int head_idx = std::stoi(rest.substr(us+1));
            if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 不是你的回合！").set_flags(dpp::m_ephemeral)); return; }
            std::lock_guard<std::mutex> lk(data_mutex);
            auto git = dd_games.find(ch); if (git == dd_games.end()) return;
            auto& dg = git->second;
            if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
            if (head_idx < 0 || head_idx >= 3 || !dg.heads[head_idx].alive) return;
            dg.selected_head = head_idx;
            ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg)); return;
        }
        if (cid.rfind("dd_back_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(8))); if (uid != bu) return;
            std::lock_guard<std::mutex> lk(data_mutex);
            auto git = dd_games.find(ch); if (git == dd_games.end()) return;
            git->second.selected_head = -1;
            ev.reply(dpp::ir_update_message, make_dd_combat_msg(git->second)); return;
        }
        if (cid.rfind("dd_atk_", 0) == 0 || cid.rfind("dd_gamble_", 0) == 0 || cid.rfind("dd_pow_", 0) == 0) {
            std::string pfx = (cid.rfind("dd_atk_",0)==0) ? "dd_atk_" : (cid.rfind("dd_gamble_",0)==0) ? "dd_gamble_" : "dd_pow_";
            dpp::snowflake bu(std::stoull(cid.substr(pfx.size()))); if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 不是你的回合！").set_flags(dpp::m_ephemeral)); return; }
            int atype = (pfx == "dd_atk_") ? 0 : (pfx == "dd_gamble_") ? 1 : 2;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch); if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid || dg.selected_head < 0) return;
                dg.log_line = dd_do_attack(dg, atype);
                if (!dd_try_end(dg, false)) {
                    dd_finish_turn(dg);
                    if (!dd_try_end(dg, false))
                        ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
                }
            }
            if (dd_need_pet_save) save_pet_data();
            return;
        }
        if (cid.rfind("dd_block_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(9))); if (uid != bu) return;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch); if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                dg.block_active = true; dg.selected_head = -1;
                dg.log_line = "🛡️ **" + dg.players[dg.current_player].display_name + "** 進入防禦姿態！";
                dd_finish_turn(dg);
                if (!dd_try_end(dg, false))
                    ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            if (dd_need_pet_save) save_pet_data();
            return;
        }
        if (cid.rfind("dd_heal_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(8))); if (uid != bu) return;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch); if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                if (dg.lifegoddess_used) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 生命女神的祝福本場已用完！").set_flags(dpp::m_ephemeral)); return; }
                dg.lifegoddess_used = true; dg.selected_head = -1;
                std::string healed;
                for (auto& p : dg.players) {
                    if (!p.alive) continue;
                    int heal = std::min((int)std::ceil(p.max_hp * 0.2), p.max_hp - p.hp);
                    if (heal > 0) {
                        p.hp += heal;
                        if (!healed.empty()) healed += "、";
                        healed += p.display_name + "(+" + std::to_string(heal) + ")";
                    }
                }
                dg.log_line = "💗 **" + dg.players[dg.current_player].display_name + "** 發動生命女神的祝福！";
                dg.log_line += healed.empty() ? "\n  → 全體HP已滿，未回復。" : ("\n  → " + healed + " 恢復 HP！");
                dd_finish_turn(dg);
                if (!dd_try_end(dg, false))
                    ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            if (dd_need_pet_save) save_pet_data();
            return;
        }
        if (cid.rfind("dd_altar_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(9))); if (uid != bu) return;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch); if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                if (dg.atk_triple) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 祭壇已毀滅！").set_flags(dpp::m_ephemeral)); return; }
                auto& cp = dg.players[dg.current_player];
                cp.at_altar = true; dg.selected_head = -1;
                dg.log_line = "🏛️ **" + cp.display_name + "** 移動至祭壇。";
                dd_finish_turn(dg);
                if (!dd_try_end(dg, false))
                    ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            if (dd_need_pet_save) save_pet_data();
            return;
        }
        if (cid.rfind("dd_pool_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(8))); if (uid != bu) return;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch); if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                auto& cp = dg.players[dg.current_player];
                cp.at_altar = false; dg.log_line = "🏊 **" + cp.display_name + "** 回到龍池。";
                dd_finish_turn(dg);
                if (!dd_try_end(dg, false))
                    ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            if (dd_need_pet_save) save_pet_data();
            return;
        }
        if (cid.rfind("dd_pray_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(8))); if (uid != bu) return;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch); if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                auto& cp = dg.players[dg.current_player];
                if (cp.has_bomb) { cp.has_bomb = false; cp.bomb_turns = 0; dg.log_line = "🙏 **" + cp.display_name + "** 向女神祈禱，炸彈解除！"; }
                else dg.log_line = "🙏 **" + cp.display_name + "** 你誠心誠意的祈禱...";
                if (cp.orb_key == "EQ_K_SPEED" && !cp.speed_extra_used && dd_rand(1,100) <= 40) {
                    cp.speed_extra_used = true; dg.speed_extra_pending = true;
                    dg.log_line += "\n⚡ **先鋒再行動！**";
                }
                dd_finish_turn(dg);
                if (!dd_try_end(dg, false))
                    ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            if (dd_need_pet_save) save_pet_data();
            return;
        }
        if (cid.rfind("dd_demolish_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(12))); if (uid != bu) return;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch); if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                auto& cp = dg.players[dg.current_player];
                if (!cp.at_altar || dg.atk_triple) return;
                dg.altar_hp--;
                std::string plog = "⛏️ **" + cp.display_name + "** 拆除祭壇！（剩餘 " + std::to_string(dg.altar_hp) + " 格血）";
                if (dg.altar_hp <= 0) {
                    dg.atk_triple = true; plog += "\n💥 **祭壇毀滅！全體 ATK×3！**";
                    for (auto& p : dg.players) {
                        if (!p.alive) continue;
                        if (p.at_altar) p.at_altar = false;
                        std::string cleared;
                        if (p.stunned_turns > 0)  { p.stunned_turns = 0;  cleared += "封鎖 "; }
                        if (p.has_bomb)            { p.has_bomb = false; p.bomb_turns = 0; cleared += "炸彈 "; }
                        if (p.atk_down_turns > 0)  { p.atk_down_turns = 0; cleared += "力量削弱 "; }
                        if (p.def_down_turns > 0)  { p.def_down_turns = 0; cleared += "防禦削弱 "; }
                        if (p.burning)             { p.burning = false;   cleared += "燃燒 "; }
                        int old = p.hp; p.hp = std::min(p.hp + 40, p.max_hp);
                        plog += "\n  → " + p.display_name + " +HP " + std::to_string(p.hp - old);
                        if (!cleared.empty()) plog += "　【" + cleared + "】解除";
                    }
                }
                dg.log_line = plog;
                if (cp.orb_key == "EQ_K_SPEED" && !cp.speed_extra_used && dd_rand(1,100) <= 40) {
                    cp.speed_extra_used = true; dg.speed_extra_pending = true;
                    dg.log_line += "\n⚡ **先鋒再行動！**";
                }
                dd_finish_turn(dg);
                if (!dd_try_end(dg, false))
                    ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            if (dd_need_pet_save) save_pet_data();
            return;
        }
    }
}

std::string give_darkdragon_chest_reward(dpp::snowflake uid) {
    // 呼叫前必須已持有 data_mutex
    return dd_give_rewards_one(uid);
}
