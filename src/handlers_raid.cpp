#include "types.h"
#include "chips.h"
#include "raid.h"
#include "handler_decls.h"
// darkdragon.h → moved to handlers_dd.cpp

// ─── Button handler: raid rooms + Rathalos raid combat ───────────────────────

void handle_raid_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    const dpp::user& user = ev.command.get_issuing_user();
    dpp::snowflake uid = user.id;
    std::string dn = user.global_name.empty() ? user.username : user.global_name;
    std::string av = user.get_avatar_url();

    // ── hunt_team_{uid}: show raid boss selection screen ─────────────────────
    if (cid.rfind("hunt_team_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(10)));
        if (uid != bu) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral));
            return;
        }
        ev.reply(dpp::ir_update_message, make_raid_boss_select_msg(uid, dn, av));
        return;
    }

    // ── hunt_boss_latus_p_{uid}: practice Rathalos ──────────────────────────
    if (cid.rfind("hunt_boss_latus_p_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(18)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        dpp::snowflake ch = ev.command.channel_id;
        RaidRoom new_room;
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (raid_games.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有進行中的組隊戰鬥！").set_flags(dpp::m_ephemeral)); return; }
          if (raid_rooms.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有等待中的組隊房間！").set_flags(dpp::m_ephemeral)); return; }
          new_room.channel_id = ch; new_room.host_uid = uid;
          new_room.boss_key = "latus"; new_room.practice_mode = true;
          new_room.created_at = time(nullptr);
          new_room.member_uids.push_back(uid);
          new_room.member_names[uid] = dn; new_room.member_avatars[uid] = av;
          raid_rooms[ch] = new_room;
        }
        dpp::message rmsg = make_raid_room_msg(new_room); rmsg.channel_id = ch;
        ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ **拉圖斯**練習房間已開啟！").set_flags(dpp::m_ephemeral));
        g_bot->message_create(rmsg, [ch](const dpp::confirmation_callback_t& cb){
            if (cb.is_error()) return;
            auto mid = std::get<dpp::message>(cb.value).id;
            std::lock_guard<std::mutex> lk(data_mutex);
            if (raid_rooms.count(ch)) raid_rooms[ch].msg_id = mid;
        });
        { dpp::timer troom = g_bot->start_timer([ch](dpp::timer t){
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = raid_rooms.find(ch); if (it == raid_rooms.end()) return;
            auto mid = it->second.msg_id; raid_rooms.erase(it);
            if (mid) { dpp::message m; m.id = mid; m.channel_id = ch; m.set_content("⌛ 組隊房間因逾時（10 分鐘）自動解散。"); g_bot->message_edit(m); }
            g_bot->stop_timer(t);
          }, 600);
          std::lock_guard<std::mutex> lk(data_mutex);
          if (raid_rooms.count(ch)) raid_rooms[ch].timer_id = troom;
        }
        return;
    }

    // ── hunt_boss_dark_p_{uid}: open room for DarkDragon (practice) ──────────
    if (cid.rfind("hunt_boss_dark_p_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(17)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        dpp::snowflake ch = ev.command.channel_id;
        RaidRoom new_room;
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (raid_games.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有進行中的組隊戰鬥！").set_flags(dpp::m_ephemeral)); return; }
          if (raid_rooms.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有等待中的組隊房間！").set_flags(dpp::m_ephemeral)); return; }
          new_room.channel_id = ch; new_room.host_uid = uid;
          new_room.boss_key = "dark_dragon"; new_room.practice_mode = true;
          new_room.created_at = time(nullptr);
          new_room.member_uids.push_back(uid);
          new_room.member_names[uid] = dn; new_room.member_avatars[uid] = av;
          raid_rooms[ch] = new_room;
        }
        dpp::message rmsg = make_raid_room_msg(new_room); rmsg.channel_id = ch;
        ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ **暗黑龍王**練習房間已開啟！").set_flags(dpp::m_ephemeral));
        g_bot->message_create(rmsg, [ch](const dpp::confirmation_callback_t& cb){
            if (cb.is_error()) return;
            auto mid = std::get<dpp::message>(cb.value).id;
            std::lock_guard<std::mutex> lk(data_mutex);
            if (raid_rooms.count(ch)) raid_rooms[ch].msg_id = mid;
        });
        { dpp::timer troom = g_bot->start_timer([ch](dpp::timer t){
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = raid_rooms.find(ch); if (it == raid_rooms.end()) return;
            auto mid = it->second.msg_id; raid_rooms.erase(it);
            if (mid) { dpp::message m; m.id = mid; m.channel_id = ch; m.set_content("⌛ 組隊房間因逾時（10 分鐘）自動解散。"); g_bot->message_edit(m); }
            g_bot->stop_timer(t);
          }, 600);
          std::lock_guard<std::mutex> lk(data_mutex);
          if (raid_rooms.count(ch)) raid_rooms[ch].timer_id = troom;
        }
        return;
    }

    // ── hunt_boss_latus_{uid}: open raid room for Rathalos ───────────────────
    if (cid.rfind("hunt_boss_latus_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(16)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        dpp::snowflake ch = ev.command.channel_id;
        RaidRoom new_room;
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (raid_games.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有進行中的組隊戰鬥！").set_flags(dpp::m_ephemeral)); return; }
          if (raid_rooms.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有等待中的組隊房間！").set_flags(dpp::m_ephemeral)); return; }
          new_room.channel_id = ch; new_room.host_uid = uid;
          new_room.boss_key = "latus"; new_room.created_at = time(nullptr);
          new_room.member_uids.push_back(uid);
          new_room.member_names[uid] = dn; new_room.member_avatars[uid] = av;
          raid_rooms[ch] = new_room;
        }
        dpp::message rmsg = make_raid_room_msg(new_room); rmsg.channel_id = ch;
        ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ **拉圖斯**組隊房間已開啟！").set_flags(dpp::m_ephemeral));
        g_bot->message_create(rmsg, [ch](const dpp::confirmation_callback_t& cb){
            if (cb.is_error()) return;
            auto mid = std::get<dpp::message>(cb.value).id;
            std::lock_guard<std::mutex> lk(data_mutex);
            if (raid_rooms.count(ch)) raid_rooms[ch].msg_id = mid;
        });
        { dpp::timer troom = g_bot->start_timer([ch](dpp::timer t){
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = raid_rooms.find(ch); if (it == raid_rooms.end()) return;
            auto mid = it->second.msg_id; raid_rooms.erase(it);
            if (mid) { dpp::message m; m.id = mid; m.channel_id = ch; m.set_content("⌛ 組隊房間因逾時（10 分鐘）自動解散。"); g_bot->message_edit(m); }
            g_bot->stop_timer(t);
          }, 600);
          std::lock_guard<std::mutex> lk(data_mutex);
          if (raid_rooms.count(ch)) raid_rooms[ch].timer_id = troom;
        }
        return;
    }

    // ── hunt_boss_dark_{uid}: open room for DarkDragon (normal) ─────────────
    if (cid.rfind("hunt_boss_dark_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(15)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        dpp::snowflake ch = ev.command.channel_id;
        RaidRoom new_room;
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (raid_games.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有進行中的組隊戰鬥！").set_flags(dpp::m_ephemeral)); return; }
          if (raid_rooms.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有等待中的組隊房間！").set_flags(dpp::m_ephemeral)); return; }
          new_room.channel_id = ch; new_room.host_uid = uid;
          new_room.boss_key = "dark_dragon"; new_room.created_at = time(nullptr);
          new_room.member_uids.push_back(uid);
          new_room.member_names[uid] = dn; new_room.member_avatars[uid] = av;
          raid_rooms[ch] = new_room;
        }
        dpp::message rmsg = make_raid_room_msg(new_room); rmsg.channel_id = ch;
        ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ **暗黑龍王**組隊房間已開啟！").set_flags(dpp::m_ephemeral));
        g_bot->message_create(rmsg, [ch](const dpp::confirmation_callback_t& cb){
            if (cb.is_error()) return;
            auto mid = std::get<dpp::message>(cb.value).id;
            std::lock_guard<std::mutex> lk(data_mutex);
            if (raid_rooms.count(ch)) raid_rooms[ch].msg_id = mid;
        });
        { dpp::timer troom = g_bot->start_timer([ch](dpp::timer t){
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = raid_rooms.find(ch); if (it == raid_rooms.end()) return;
            auto mid = it->second.msg_id; raid_rooms.erase(it);
            if (mid) { dpp::message m; m.id = mid; m.channel_id = ch; m.set_content("⌛ 組隊房間因逾時（10 分鐘）自動解散。"); g_bot->message_edit(m); }
            g_bot->stop_timer(t);
          }, 600);
          std::lock_guard<std::mutex> lk(data_mutex);
          if (raid_rooms.count(ch)) raid_rooms[ch].timer_id = troom;
        }
        return;
    }

    // ── rroom_join_{ch} ───────────────────────────────────────────────────────
    if (cid.rfind("rroom_join_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(11)));
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = raid_rooms.find(ch);
        if (it == raid_rooms.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 找不到組隊房間！").set_flags(dpp::m_ephemeral)); return; }
        auto& room = it->second;
        if (room.msg_id == 0) room.msg_id = ev.command.message_id;
        if (room.member_uids.size() >= 4) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 房間已滿（4 人）！").set_flags(dpp::m_ephemeral)); return; }
        for (auto& m : room.member_uids) if (m == uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你已在房間內！").set_flags(dpp::m_ephemeral)); return; }
        room.member_uids.push_back(uid);
        room.member_names[uid] = dn; room.member_avatars[uid] = av;
        ev.reply(dpp::ir_update_message, make_raid_room_msg(room));
        return;
    }

    // ── rroom_dissolve_{ch} ───────────────────────────────────────────────────
    if (cid.rfind("rroom_dissolve_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(15)));
        dpp::timer tid = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = raid_rooms.find(ch);
            if (it == raid_rooms.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 找不到組隊房間！").set_flags(dpp::m_ephemeral)); return; }
            if (uid != it->second.host_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有開房者可以解散！").set_flags(dpp::m_ephemeral)); return; }
            tid = it->second.timer_id;
            raid_rooms.erase(it);
        }
        // 停掉房間的 10 分鐘逾時計時器，不然之後同頻道開新房間會被這個殘留的計時器誤殺
        if (tid) g_bot->stop_timer(tid);
        ev.reply(dpp::ir_update_message, dpp::message("🗑️ **" + dn + "** 解散了組隊房間。"));
        return;
    }

    // ── rroom_start_{ch}: Rathalos only (dark_dragon → handle_dd_button) ─────
    if (cid.rfind("rroom_start_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(12)));
        RaidRoom room;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = raid_rooms.find(ch);
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
            raid_rooms.erase(ch);
        }
        // 停掉房間的 10 分鐘逾時計時器，不然之後同頻道開新房間會被這個殘留的計時器誤殺
        if (room.timer_id) g_bot->stop_timer(room.timer_id);

        const RaidBoss* boss = find_raid_boss(room.boss_key);
        RaidGame g;
        g.channel_id = ch; g.boss_key = boss->key; g.boss_name = boss->name;
        g.boss_image = boss->image; g.boss_hp = boss->hp; g.boss_max_hp = boss->hp;
        g.boss_atk = boss->atk; g.boss_def = boss->def;
        g.started_at = time(nullptr); g.round = 1; g.boss_turn = false;
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
            RaidPlayer p;
            p.uid = muid; p.display_name = room.member_names.count(muid) ? room.member_names.at(muid) : "?";
            p.avatar_url = room.member_avatars.count(muid) ? room.member_avatars.at(muid) : "";
            p.hp = ps.hp; p.max_hp = ps.hp; p.atk = ps.atk; p.def = ps.def;
            p.orb_key = orb_key; p.alive = true;
            if (orb_key == "EQ_K_UR" && room.member_uids.size() > 1) {
                auto* ur_gi = find_gacha_item("EQ_K_UR"); if (ur_gi) p.def = std::max(0, p.def - ur_gi->stat_val);
            }
            g.players.push_back(p);
        }
        g.current_player = -1;
        for (int i = 0; i < (int)g.players.size(); i++) if (g.players[i].alive) { g.current_player = i; break; }
        g.practice_mode = room.practice_mode;
        { std::lock_guard<std::mutex> lk(data_mutex); raid_games[ch] = g; }
        dpp::message gmsg = make_raid_combat_msg(g);
        ev.reply(dpp::ir_update_message, gmsg);
        { std::lock_guard<std::mutex> lk(data_mutex); if (raid_games.count(ch)) raid_games[ch].msg_id = ev.command.message_id; }
        dpp::timer raid_tid = g_bot->start_timer([ch](dpp::timer t){
            dpp::snowflake mid = 0; std::string boss_name;
            std::vector<dpp::snowflake> player_uids; bool is_practice_latus = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = raid_games.find(ch); if (it == raid_games.end()) return;
              if (it->second.game_over) return;
              it->second.game_over = true; it->second.victory = false;
              mid = it->second.msg_id; boss_name = it->second.boss_name;
              is_practice_latus = it->second.practice_mode;
              for (auto& p : it->second.players) player_uids.push_back(p.uid);
              raid_games.erase(it);
            }
            if (!is_practice_latus) {
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    for (auto puid : player_uids) {
                        auto& pet = pet_data[puid]; bool already = false;
                        for (auto& s : pet.statuses) if (s == "受傷") { already = true; break; }
                        if (!already) pet.statuses.push_back("受傷");
                    }
                }
                save_pet_data();
            }
            if (mid) {
                dpp::embed e; e.set_title("⌛ 討伐失敗").set_color(0x808080).set_description("戰鬥逾時（20 分鐘），討伐失敗，無獎勵。");
                dpp::message edit_m; edit_m.id = mid; edit_m.channel_id = ch; edit_m.add_embed(e);
                g_bot->message_edit(edit_m);
                g_bot->message_create(dpp::message(ch, "⌛ **討伐逾時！** 20 分鐘內未能擊敗 **" + boss_name + "**，討伐失敗，本次無獎勵。"));
            }
            g_bot->stop_timer(t);
        }, 1200);
        { std::lock_guard<std::mutex> lk(data_mutex); if (raid_games.count(ch)) raid_games[ch].timer_id = raid_tid; }
        return;
    }

    // ── raid_refresh_{ch} ─────────────────────────────────────────────────────
    if (cid.rfind("raid_refresh_", 0) == 0) {
        dpp::snowflake game_ch(std::stoull(cid.substr(13)));
        dpp::message new_msg; dpp::snowflake old_mid = 0;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = raid_games.find(game_ch);
          if (it == raid_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 戰鬥已結束！").set_flags(dpp::m_ephemeral)); return; }
          new_msg = make_raid_combat_msg(it->second); old_mid = it->second.msg_id;
        }
        new_msg.channel_id = game_ch;
        ev.reply(dpp::ir_channel_message_with_source, new_msg,
            [game_ch, old_mid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    dpp::snowflake new_mid = std::get<dpp::message>(cb.value).id;
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = raid_games.find(game_ch);
                    if (it != raid_games.end()) it->second.msg_id = new_mid;
                }
                if (old_mid) {
                    dpp::message old_edit; old_edit.id = old_mid; old_edit.channel_id = game_ch;
                    old_edit.set_content("↩️ 已刷新，請看新訊息。");
                    g_bot->message_edit(old_edit);
                }
            });
        return;
    }

    // ── raid_cryt_ ────────────────────────────────────────────────────────────
    if (cid.rfind("raid_cryt_", 0) == 0) {
        std::string rest = cid.substr(10);
        size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
        dpp::snowflake ch(std::stoull(rest.substr(0, s1)));
        std::string rest2 = rest.substr(s1+1);
        size_t s2 = rest2.find('_'); if (s2 == std::string::npos) return;
        dpp::snowflake src_uid(std::stoull(rest2.substr(0, s2)));
        int tidx = std::stoi(rest2.substr(s2+1));
        if (uid != src_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的戰吼！").set_flags(dpp::m_ephemeral)); return; }
        bool cryt_over = false; RaidGame cryt_snap;
        std::vector<std::pair<std::string,std::string>> reward_lines; dpp::message cryt_combat;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = raid_games.find(ch); if (it == raid_games.end()) return;
          auto& g = it->second;
          if (tidx < 0 || tidx >= (int)g.players.size()) return;
          g.players[tidx].battlecry_next = true; g.cry_pending_uid = 0;
          g.log_line = "📣 **" + g.players[g.current_player].display_name + "** 為 **" + g.players[tidx].display_name + "** 施加戰吼！下次攻擊 +25%";
          raid_finish_turn(g); cryt_over = g.game_over;
          if (cryt_over) { cryt_snap = g; raid_games.erase(it); }
          else cryt_combat = make_raid_combat_msg(g);
        }
        if (cryt_over) {
            g_bot->stop_timer(cryt_snap.timer_id);
            if (cryt_snap.victory && !cryt_snap.practice_mode) {
                { std::lock_guard<std::mutex> lk(data_mutex);
                  for (auto& p : cryt_snap.players)
                      if (inventory_data.count(p.uid) && inventory_data[p.uid].count("weekly_hunt_scroll") && inventory_data[p.uid].at("weekly_hunt_scroll") > 0)
                          inventory_data[p.uid]["weekly_hunt_scroll"]--;
                }
                for (auto& p : cryt_snap.players) reward_lines.push_back({p.display_name, raid_give_rewards(p.uid, p.display_name)});
                save_chips(); save_inventory();
            }
            if (!cryt_snap.victory && !cryt_snap.practice_mode) {
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    for (auto& p : cryt_snap.players) {
                        auto& pet2 = pet_data[p.uid]; bool already = false;
                        for (auto& s : pet2.statuses) if (s == "受傷") { already = true; break; }
                        if (!already) pet2.statuses.push_back("受傷");
                    }
                }
                save_pet_data();
            }
            ev.reply(dpp::ir_update_message, make_raid_end_msg(cryt_snap, reward_lines)); return;
        }
        ev.reply(dpp::ir_update_message, cryt_combat); return;
    }

    // ── Raid combat attack helper ─────────────────────────────────────────────
    auto parse_raid_btn = [&](const std::string& prefix, size_t plen,
                              dpp::snowflake& ch_out, dpp::snowflake& actor_out) -> bool {
        if (cid.rfind(prefix, 0) != 0) return false;
        std::string rest = cid.substr(plen);
        size_t s = rest.find('_'); if (s == std::string::npos) return false;
        ch_out    = dpp::snowflake(std::stoull(rest.substr(0, s)));
        actor_out = dpp::snowflake(std::stoull(rest.substr(s+1)));
        return true;
    };

    auto do_raid_attack = [&](int attack_type, bool is_block, bool is_cry, bool is_heal = false) {
        std::string pfx; size_t plen;
        if      (is_block)        { pfx = "raid_block_";  plen = 11; }
        else if (is_heal)         { pfx = "raid_heal_";   plen = 10; }
        else if (is_cry)          { pfx = "raid_cry_";    plen = 9;  }
        else if (attack_type == 2){ pfx = "raid_pow_";    plen = 9;  }
        else if (attack_type == 1){ pfx = "raid_gamble_"; plen = 12; }
        else                      { pfx = "raid_atk_";    plen = 9;  }
        dpp::snowflake ch, actor;
        if (!parse_raid_btn(pfx, plen, ch, actor)) return;
        if (uid != actor) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 不是你的回合！").set_flags(dpp::m_ephemeral)); return; }
        bool game_over = false, victory = false;
        RaidGame g_snap;
        std::vector<std::pair<std::string,std::string>> reward_lines;
        dpp::message combat_msg_out;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = raid_games.find(ch);
            if (it == raid_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 戰鬥不存在！").set_flags(dpp::m_ephemeral)); return; }
            auto& g = it->second;
            if (g.game_over) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 戰鬥已結束！").set_flags(dpp::m_ephemeral)); return; }
            if (g.boss_turn || g.current_player < 0 || g.players[g.current_player].uid != uid) {
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 不是你的回合！").set_flags(dpp::m_ephemeral)); return;
            }
            if (g.cry_pending_uid != 0) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 請先選擇戰吼目標！").set_flags(dpp::m_ephemeral)); return; }
            if (is_cry) { g.log_line = ""; g.cry_pending_uid = uid; ev.reply(dpp::ir_update_message, make_raid_combat_msg(g)); return; }
            std::string heal_log = raid_athena_heal(g);
            std::string log;
            if (is_block) {
                g.block_active = true;
                log = "🛡️ **" + g.players[g.current_player].display_name + "** 進入防禦姿態！";
            } else if (is_heal) {
                if (g.lifegoddess_used) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 生命女神的祝福本場已用完！").set_flags(dpp::m_ephemeral)); return; }
                g.lifegoddess_used = true;
                std::string healed;
                for (auto& p : g.players) {
                    if (!p.alive) continue;
                    int heal = std::min((int)std::ceil(p.max_hp * 0.2), p.max_hp - p.hp);
                    if (heal > 0) {
                        p.hp += heal;
                        if (!healed.empty()) healed += "、";
                        healed += p.display_name + "(+" + std::to_string(heal) + ")";
                    }
                }
                log = "💗 **" + g.players[g.current_player].display_name + "** 發動生命女神的祝福！";
                log += healed.empty() ? "\n  → 全體HP已滿，未回復。" : ("\n  → " + healed + " 恢復 HP！");
            } else {
                log = raid_do_player_attack(g, attack_type);
            }
            if (!heal_log.empty()) log = heal_log + "\n" + log;
            g.log_line = log; raid_finish_turn(g);
            game_over = g.game_over; victory = g.victory;
            if (game_over) { g_snap = g; raid_games.erase(it); }
            else combat_msg_out = make_raid_combat_msg(g);
        }
        if (game_over) {
            g_bot->stop_timer(g_snap.timer_id);
            if (victory && !g_snap.practice_mode) {
                { std::lock_guard<std::mutex> lk(data_mutex);
                  for (auto& p : g_snap.players)
                      if (inventory_data.count(p.uid) && inventory_data[p.uid].count("weekly_hunt_scroll") && inventory_data[p.uid].at("weekly_hunt_scroll") > 0)
                          inventory_data[p.uid]["weekly_hunt_scroll"]--;
                }
                for (auto& p : g_snap.players) reward_lines.push_back({p.display_name, raid_give_rewards(p.uid, p.display_name)});
                save_chips(); save_inventory();
            }
            if (!victory && !g_snap.practice_mode) {
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    for (auto& p : g_snap.players) {
                        auto& pet2 = pet_data[p.uid]; bool already = false;
                        for (auto& s : pet2.statuses) if (s == "受傷") { already = true; break; }
                        if (!already) pet2.statuses.push_back("受傷");
                    }
                }
                save_pet_data();
            }
            ev.reply(dpp::ir_update_message, make_raid_end_msg(g_snap, reward_lines));
        } else {
            ev.reply(dpp::ir_update_message, combat_msg_out);
        }
    };

    if (cid.rfind("raid_atk_", 0) == 0)   { do_raid_attack(0,false,false); return; }
    if (cid.rfind("raid_gamble_", 0) == 0) { do_raid_attack(1,false,false); return; }
    if (cid.rfind("raid_pow_", 0) == 0)    { do_raid_attack(2,false,false); return; }
    if (cid.rfind("raid_block_", 0) == 0)  { do_raid_attack(0,true,false);  return; }
    if (cid.rfind("raid_cry_", 0) == 0)    { do_raid_attack(0,false,true);  return; }
    if (cid.rfind("raid_heal_", 0) == 0)   { do_raid_attack(0,false,false,true); return; }
}
