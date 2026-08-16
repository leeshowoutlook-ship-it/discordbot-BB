#include "types.h"
#include "chips.h"
#include "monster.h"
#include "handler_decls.h"

// ─── 狩獵規則說明（! 與 / 共用同一份內容，避免兩邊敘述分歧）───────────────────────
static dpp::embed make_hunt_rules_embed() {
    dpp::embed e;
    e.set_title("⚔️  怪物狩獵 — 系統規則").set_color(0xC0392B);
    e.add_field("📜 狩獵卷",
        "每天第一次 `!領取` 會獲得 **2 張狩獵卷**。\n"
        "也可在 **商店 → 虛擬商店 → 特殊道具** 購買（3000 碼/張）。\n"
        "每次狩獵消耗 **1 張**，失敗也會消耗。", false);
    e.add_field("🗺️ 難度解鎖",
        "**簡單**：直接開放\n"
        "**普通**：需通關所有簡單怪物（各至少一次）\n"
        "**困難**：需通關所有普通怪物\n"
        "**怪物之王**：需通關所有困難怪物", false);
    e.add_field("⚔️ 戰鬥系統",
        "**普通攻擊**：傷害 = 寵物 ATK − 怪物 DEF（最小 0）\n"
        "**耗費氣力**：隨機 0.1~2.0× 傷害，不跳回合（賭注攻擊）\n"
        "**強攻**：固定 2.0× 傷害，下回合跳過\n"
        "怪物每回合反擊：傷害 = 怪物 ATK − 寵物 DEF（最小 0）\n"
        "先手機率 **75%**（裝備**迅捷狼王的寶珠**則必定先手）", false);
    e.add_field("🔮 寶珠效果",
        "**無名女神**：單人+5防禦；組隊全體+2防禦\n"
        "**迅捷狼王**：單人必定先手；組隊：40%機率多行動一回合\n"
        "**雅典娜**：單人 30% 恢復 8 HP；組隊 20% 全體恢復 5 HP\n"
        "**巨山狂熊**：單人：防禦降低怪物下兩次攻擊60%；組隊：防禦降低範圍攻擊傷害30%／集中攻擊傷害60%\n"
        "**維京**：HP≤50% 傷害×1.4，HP≤25% 傷害×1.7（被動狂暴）\n"
        "**狂怒戰神**：攻擊力 +10", false);
    e.add_field("🏆 通關獎勵",
        "通關後獲得 **隨機範圍的籌碼**，首次通關有**額外獎勵**。\n"
        "有機率掉落**成長道具**（可在背包或商店查看）。", false);
    e.add_field("🩹 負面狀態影響",
        "**受傷**：無法進行狩獵\n"
        "**肌肉緊繃**：每回合攻擊有 30% 機率失敗（怪物仍反擊）", false);
    e.add_field("⚔️ 裝備系統",
        "使用 `!轉蛋` 抽取裝備，有武器・手套・衣服・鞋子・靈魂寶珠五個部位。\n"
        "使用 `!裝備` 管理與更換裝備，裝備會提升寵物的 ATK / HP / DEF。", false);
    e.add_field("⏰ 注意事項",
        "戰鬥有 **20 分鐘**限時，超時視為失敗。\n"
        "失敗（或逾時）寵物會獲得「**受傷**」狀態，需使用**高級傷藥**恢復。", false);
    return e;
}

// ─── Message handler ──────────────────────────────────────────────────────────

void handle_hunt_message(const dpp::message_create_t& ev, const std::string& content,
                         dpp::snowflake uid, dpp::snowflake ch)
{
    if (content == "!怪物狩獵") {
        std::string dn = ev.msg.member.get_nickname().empty()
                       ? ev.msg.author.username : ev.msg.member.get_nickname();
        std::string av = ev.msg.author.get_avatar_url();
        Pet pet;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = pet_data.find(uid); if (it != pet_data.end()) pet = it->second; }
        dpp::message m = make_hunt_main_msg(uid, pet, dn, av);
        m.channel_id = ch;
        g_bot->message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                msg_owner[std::get<dpp::message>(cb.value).id] = uid;
            }
        });
    }
    else if (content == "!狩獵規則") {
        dpp::message m; m.channel_id = ch; m.add_embed(make_hunt_rules_embed());
        g_bot->message_create(m);
    }
}

// ─── Button handler ───────────────────────────────────────────────────────────

void handle_hunt_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    const dpp::user& user = ev.command.get_issuing_user();
    dpp::snowflake uid = user.id;
    std::string dn = user.global_name.empty() ? user.username : user.global_name;
    std::string av = user.get_avatar_url();

    // ── hunt_village_{uid}: 選擇村落 ─────────────────────────────────────────
    if (cid.rfind("hunt_village_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(13)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (village_games.count(uid)) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你已有進行中的村落挑戰！").set_flags(dpp::m_ephemeral));
            return;
          }
          bool has_scroll = inventory_data.count(uid) && inventory_data[uid]["hunt_scroll"] > 0;
          if (!has_scroll) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有怪物狩獵卷！").set_flags(dpp::m_ephemeral)); return; }
        }
        ev.reply(dpp::ir_update_message, make_village_select_msg(uid, dn, av));
        return;
    }

    // ── village_start_{uid}_{group_key}: 確認選擇並開始 ──────────────────────
    if (cid.rfind("village_start_", 0) == 0) {
        std::string rest = cid.substr(14);
        size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
        dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
        std::string group_key = rest.substr(s1 + 1);
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        const VillageGroupDef* gdp = find_village_group(group_key);
        if (!gdp) return;
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (village_games.count(uid)) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你已有進行中的村落挑戰！").set_flags(dpp::m_ephemeral));
            return;
          }
        }
        bool has_scroll = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = inventory_data.find(uid);
          if (it != inventory_data.end() && it->second["hunt_scroll"] > 0) { it->second["hunt_scroll"]--; has_scroll = true; }
        }
        if (!has_scroll) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有怪物狩獵卷！").set_flags(dpp::m_ephemeral)); return; }
        save_inventory();
        Pet pet2; { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) pet2 = it->second; }
        PetStats ps = calc_pet_stats(uid, pet2);
        VillageGame vg;
        vg.uid = uid; vg.channel_id = ev.command.channel_id;
        vg.group_key = gdp->key;
        vg.spirits   = build_village_spirits(*gdp);
        vg.pet_hp = ps.hp; vg.pet_max_hp = ps.hp;
        vg.pet_atk = ps.atk; vg.pet_def = ps.def;
        vg.started_at = time(nullptr);
        { std::lock_guard<std::mutex> lk(data_mutex);
          vg.orb_key = equipped_data.count(uid) ? equipped_data[uid].orb : "";
          apply_pet_basic_set_bonus(uid, pet2, vg.pet_atk, vg.pet_hp, vg.pet_max_hp, vg.pet_def);
          vg.pet_atk += col_pet_atk_bonus(uid);
          vg.pet_def += col_pet_def_bonus(uid);
          int hp_bonus = col_pet_hp_bonus(uid);
          vg.pet_hp += hp_bonus; vg.pet_max_hp += hp_bonus;
        }
        vg.msg_id = ev.command.message_id;
        dpp::timer vtid = g_bot->start_timer([uid](dpp::timer) {
            VillageGame tg;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = village_games.find(uid);
              if (it == village_games.end()) return;
              tg = it->second; village_games.erase(it);
            }
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto& p = pet_data[uid]; bool already = false;
              for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
              if (!already) p.statuses.push_back("受傷");
            }
            save_pet_data();
            if ((uint64_t)tg.msg_id != 0) {
                dpp::message tm = make_village_timeout_msg(tg, "", "");
                tm.id = tg.msg_id; tm.channel_id = tg.channel_id;
                g_bot->message_edit(tm);
            }
        }, 600);
        vg.timer_id = vtid;
        { std::lock_guard<std::mutex> lk(data_mutex); village_games[uid] = vg; }
        ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
        return;
    }

    // ── village_atk_{uid}_{idx}: 選擇目標 ────────────────────────────────────
    if (cid.rfind("village_atk_", 0) == 0) {
        std::string rest = cid.substr(12);
        size_t s1 = rest.rfind('_'); if (s1 == std::string::npos) return;
        dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
        int tidx = std::stoi(rest.substr(s1+1));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = village_games.find(uid);
          if (it == village_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
          if (tidx < 0 || tidx >= (int)it->second.spirits.size() || it->second.spirits[tidx].hp <= 0) {
              ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 無效目標！").set_flags(dpp::m_ephemeral)); return;
          }
          it->second.selected_target = tidx;
        }
        VillageGame vg;
        { std::lock_guard<std::mutex> lk(data_mutex); vg = village_games[uid]; }
        ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
        return;
    }

    // ── village_back_{uid}: 取消目標選擇 ─────────────────────────────────────
    if (cid.rfind("village_back_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(13)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = village_games.find(uid);
          if (it == village_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
          it->second.selected_target = -1;
        }
        VillageGame vg;
        { std::lock_guard<std::mutex> lk(data_mutex); vg = village_games[uid]; }
        ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
        return;
    }

    // ── village_exec_{uid}_{n|p}: 執行攻擊 ───────────────────────────────────
    if (cid.rfind("village_exec_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t s1 = rest.rfind('_'); if (s1 == std::string::npos) return;
        dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
        std::string atype = rest.substr(s1+1);
        int attack_type = (atype == "p") ? 1 : 0;
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        VillageGame vg;
        bool found = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = village_games.find(uid);
          if (it != village_games.end()) { vg = it->second; found = true; }
        }
        if (!found) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
        int tidx = vg.selected_target;
        if (tidx < 0 || tidx >= (int)vg.spirits.size() || vg.spirits[tidx].hp <= 0) {
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 請先選擇目標！").set_flags(dpp::m_ephemeral)); return;
        }
        vg.selected_target = -1;
        bool vwin = false; int64_t vreward = 0; int vkilled_unused = 0;
        HuntDropList vdrops;
        bool vended = process_village_combat(vg, tidx, attack_type, vwin, vreward, vkilled_unused, vdrops);
        if (vended) {
            int vkilled = 0;
            for (auto& s : vg.spirits) if (s.hp <= 0) vkilled++;
            dpp::timer vt = 0;
            { std::lock_guard<std::mutex> lk(data_mutex); vt = vg.timer_id; village_games.erase(uid); }
            if (vt) g_bot->stop_timer(vt);
            bool vfirst = false;
            if (vwin) {
                { std::lock_guard<std::mutex> lk(data_mutex);
                  vfirst = hunt_clear_data[uid].count(vg.group_key) == 0;
                  hunt_clear_data[uid].insert(vg.group_key);
                }
                if (vfirst) { auto* gd2 = find_village_group(vg.group_key); if (gd2) vreward += gd2->first_clear_reward; }
                add_chips(uid, vreward);
                { std::lock_guard<std::mutex> lk(data_mutex);
                  for (auto& [k, c] : vdrops) inventory_data[uid][k] += c;
                }
                save_chips(); save_hunt_clear();
                if (!vdrops.empty()) save_inventory();
            } else {
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto& p = pet_data[uid]; bool already = false;
                  for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                  if (!already) p.statuses.push_back("受傷");
                }
                save_pet_data();
            }
            ev.reply(dpp::ir_update_message, make_village_end_msg(vwin, vg, vreward, vfirst, vdrops, dn, av, vkilled));
        } else {
            { std::lock_guard<std::mutex> lk(data_mutex); village_games[uid] = vg; }
            ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
        }
        return;
    }

    // ── village_refresh_{uid}: 刷新村落戰鬥訊息 ──────────────────────────────
    if (cid.rfind("village_refresh_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(16)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        VillageGame vg;
        bool found = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = village_games.find(uid);
          if (it != village_games.end()) { vg = it->second; found = true; }
        }
        if (!found) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
        dpp::embed se; se.set_title("🔄 訊息已刷新").set_color(0x95A5A6).set_description("請往下滑查看最新戰況！");
        ev.reply(dpp::ir_update_message, dpp::message().add_embed(se));
        auto fresh = make_village_combat_msg(vg, dn, av);
        fresh.channel_id = vg.channel_id;
        g_bot->message_create(fresh, [uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                auto nid = std::get<dpp::message>(cb.value).id;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = village_games.find(uid);
                if (it != village_games.end()) it->second.msg_id = nid;
            }
        });
        return;
    }

    // ── village_block_{uid}: 熊寶珠防禦（怪物村落）──────────────────────────
    if (cid.rfind("village_block_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(14)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        VillageGame vg; bool found = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = village_games.find(uid);
          if (it != village_games.end()) { vg = it->second; found = true; }
        }
        if (!found) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
        vg.selected_target = -1;
        bool vwin = false; int64_t vreward = 0; int vkilled_unused = 0;
        HuntDropList vdrops;
        bool vended = process_village_combat(vg, 0, 0, vwin, vreward, vkilled_unused, vdrops, true);
        if (vended) {
            int vkilled = 0;
            for (auto& s : vg.spirits) if (s.hp <= 0) vkilled++;
            dpp::timer vt = 0;
            { std::lock_guard<std::mutex> lk(data_mutex); vt = vg.timer_id; village_games.erase(uid); }
            if (vt) g_bot->stop_timer(vt);
            bool vfirst = false;
            if (vwin) {
                { std::lock_guard<std::mutex> lk(data_mutex);
                  vfirst = hunt_clear_data[uid].count(vg.group_key) == 0;
                  hunt_clear_data[uid].insert(vg.group_key);
                }
                if (vfirst) { auto* gd2 = find_village_group(vg.group_key); if (gd2) vreward += gd2->first_clear_reward; }
                add_chips(uid, vreward);
                { std::lock_guard<std::mutex> lk(data_mutex);
                  for (auto& [k, c] : vdrops) inventory_data[uid][k] += c;
                }
                save_chips(); save_hunt_clear();
                if (!vdrops.empty()) save_inventory();
            } else {
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto& p = pet_data[uid]; bool already = false;
                  for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                  if (!already) p.statuses.push_back("受傷");
                }
                save_pet_data();
            }
            ev.reply(dpp::ir_update_message, make_village_end_msg(vwin, vg, vreward, vfirst, vdrops, dn, av, vkilled));
        } else {
            { std::lock_guard<std::mutex> lk(data_mutex); village_games[uid] = vg; }
            ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
        }
        return;
    }

    // ── Solo hunt navigation ───────────────────────────────────────────────────
    if (cid.rfind("hunt_main_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(10)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        Pet pet2; { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) pet2 = it->second; }
        ev.reply(dpp::ir_update_message, make_hunt_main_msg(uid, pet2, dn, av));
        return;
    }

    if (cid.rfind("hunt_diff_", 0) == 0) {
        std::string rest = cid.substr(10);
        size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
        dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        std::string diff = rest.substr(s1+1);
        ev.reply(dpp::ir_update_message, make_hunt_diff_msg(uid, diff, dn, av));
        return;
    }

    // ── hunt_monster_{uid}_{key}: 開始狩獵 ───────────────────────────────────
    if (cid.rfind("hunt_monster_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
        dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        std::string mkey = rest.substr(s1+1);
        auto* md = find_monster(mkey);
        if (!md) return;

        { std::lock_guard<std::mutex> lk(data_mutex);
          if (monster_hunt_games.count(uid)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 你已有進行中的狩獵！請先完成或等待逾時。").set_flags(dpp::m_ephemeral));
            return;
          }
        }

        bool has_scroll = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = inventory_data.find(uid);
          if (it != inventory_data.end() && it->second["hunt_scroll"] > 0) {
              it->second["hunt_scroll"]--; has_scroll = true;
          }
        }
        if (!has_scroll) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 沒有怪物狩獵卷！").set_flags(dpp::m_ephemeral));
            return;
        }
        save_inventory();

        Pet pet2; { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) pet2 = it->second; }
        PetStats ps = calc_pet_stats(uid, pet2);

        MonsterHuntGame g;
        g.uid            = uid;
        g.channel_id     = ev.command.channel_id;
        g.difficulty     = md->difficulty;
        g.monster_key    = mkey;
        g.monster_name   = md->name;
        g.monster_hp     = md->hp;
        g.monster_max_hp = md->hp;
        g.monster_atk    = md->atk;
        g.monster_def    = md->def;
        g.pet_hp         = ps.hp;
        g.pet_max_hp     = ps.hp;
        g.pet_atk        = ps.atk;
        g.pet_def        = ps.def;
        g.started_at     = time(nullptr);
        { std::lock_guard<std::mutex> lk(data_mutex);
          g.orb_key = equipped_data.count(uid) ? equipped_data[uid].orb : "";
          apply_pet_basic_set_bonus(uid, pet2, g.pet_atk, g.pet_hp, g.pet_max_hp, g.pet_def);
          g.pet_atk += col_pet_atk_bonus(uid);
          g.pet_def += col_pet_def_bonus(uid);
          int hp_bonus = col_pet_hp_bonus(uid);
          g.pet_hp += hp_bonus; g.pet_max_hp += hp_bonus;
        }
        g.player_first = (g.orb_key == "EQ_K_SPEED") ||
                         std::uniform_int_distribution<int>(0,3)(hunt_rng()) < 3;

        { std::lock_guard<std::mutex> lk(data_mutex);
          (void)(hunt_clear_data[uid].count(mkey) == 0); }

        if (!g.player_first) {
            int mon_dmg = std::max(0, g.monster_atk - g.pet_def);
            g.pet_hp -= mon_dmg;
            g.log_line = "👹 怪物先手！**" + g.monster_name + "** 造成 **" + std::to_string(mon_dmg) + "** 傷害！";
            if (g.pet_hp <= 0) {
                g.pet_hp = 0;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto& p = pet_data[uid]; bool already = false;
                  for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                  if (!already) p.statuses.push_back("受傷");
                }
                save_pet_data();
                ev.reply(dpp::ir_update_message, make_combat_end_msg(false, g, 0, false, {}, dn, av));
                return;
            }
        }

        g.msg_id = ev.command.message_id;
        { std::lock_guard<std::mutex> lk(data_mutex); monster_hunt_games[uid] = g; }
        ev.reply(dpp::ir_update_message, make_combat_msg(g, dn, av));

        dpp::timer tid = g_bot->start_timer([uid, dn, av](dpp::timer t) {
            g_bot->stop_timer(t);
            MonsterHuntGame tg; bool found2 = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = monster_hunt_games.find(uid);
              if (it != monster_hunt_games.end()) { tg = it->second; found2 = true; monster_hunt_games.erase(it); }
            }
            if (!found2) return;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto& p = pet_data[uid]; bool already = false;
              for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
              if (!already) p.statuses.push_back("受傷");
            }
            save_pet_data();
            if (tg.msg_id && tg.channel_id) {
                auto tmsg = make_combat_timeout_msg(tg, dn, av);
                tmsg.id = tg.msg_id; tmsg.channel_id = tg.channel_id;
                g_bot->message_edit(tmsg);
            }
        }, 600);
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (monster_hunt_games.count(uid)) monster_hunt_games[uid].timer_id = tid;
        }
        return;
    }

    // ── hunt_atk_{uid} / hunt_pow_{uid}: 攻擊 ───────────────────────────────
    if (cid.rfind("hunt_atk_", 0) == 0 || cid.rfind("hunt_pow_", 0) == 0) {
        bool power = cid.rfind("hunt_pow_", 0) == 0;
        dpp::snowflake bu(std::stoull(cid.substr(9)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }

        MonsterHuntGame g; bool found = false; std::string pet_muscle_tense;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = monster_hunt_games.find(uid);
          if (it != monster_hunt_games.end()) { g = it->second; found = true; }
          auto pit = pet_data.find(uid);
          if (pit != pet_data.end())
              for (auto& s : pit->second.statuses) if (s == "肌肉緊繃") { pet_muscle_tense = s; break; }
        }
        if (!found) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的狩獵！").set_flags(dpp::m_ephemeral)); return; }

        bool win = false; int64_t reward = 0; HuntDropList hunt_drops;
        bool ended = process_combat(g, power, !pet_muscle_tense.empty(), win, reward, hunt_drops);

        if (ended) {
            dpp::timer tid = 0;
            { std::lock_guard<std::mutex> lk(data_mutex); tid = g.timer_id; monster_hunt_games.erase(uid); }
            if (tid) g_bot->stop_timer(tid);
            if (win) {
                bool first_clear = false;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  first_clear = hunt_clear_data[uid].count(g.monster_key) == 0;
                  hunt_clear_data[uid].insert(g.monster_key);
                }
                if (first_clear) { auto* md2 = find_monster(g.monster_key); if (md2) reward += md2->first_clear_reward; }
                add_chips(uid, reward);
                { std::lock_guard<std::mutex> lk(data_mutex);
                  for (auto& [k, c] : hunt_drops) inventory_data[uid][k] += c; }
                save_chips(); save_hunt_clear();
                if (!hunt_drops.empty()) save_inventory();
                ev.reply(dpp::ir_update_message, make_combat_end_msg(true, g, reward, first_clear, hunt_drops, dn, av));
            } else {
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto& p = pet_data[uid]; bool already = false;
                  for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                  if (!already) p.statuses.push_back("受傷");
                }
                save_pet_data();
                ev.reply(dpp::ir_update_message, make_combat_end_msg(false, g, 0, false, {}, dn, av));
            }
        } else {
            { std::lock_guard<std::mutex> lk(data_mutex); monster_hunt_games[uid] = g; }
            ev.reply(dpp::ir_update_message, make_combat_msg(g, dn, av));
        }
        return;
    }

    // ── hunt_block_{uid} / hunt_cry_{uid}: 防禦 / 戰吼 ─────────────────────
    if (cid.rfind("hunt_block_", 0) == 0 || cid.rfind("hunt_cry_", 0) == 0) {
        bool is_block     = cid.rfind("hunt_block_", 0) == 0;
        bool is_battlecry = !is_block;
        dpp::snowflake bu(std::stoull(cid.substr(is_block ? 11 : 9)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }

        MonsterHuntGame g; bool found = false; std::string pet_muscle_tense;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = monster_hunt_games.find(uid);
          if (it != monster_hunt_games.end()) { g = it->second; found = true; }
          auto pit = pet_data.find(uid);
          if (pit != pet_data.end())
              for (auto& s : pit->second.statuses) if (s == "肌肉緊繃") { pet_muscle_tense = s; break; }
        }
        if (!found) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的狩獵！").set_flags(dpp::m_ephemeral)); return; }

        bool win = false; int64_t reward = 0; HuntDropList hunt_drops2;
        bool ended = process_combat(g, false, !pet_muscle_tense.empty(), win, reward, hunt_drops2, is_block, is_battlecry);

        if (ended) {
            dpp::timer tid = 0;
            { std::lock_guard<std::mutex> lk(data_mutex); tid = g.timer_id; monster_hunt_games.erase(uid); }
            if (tid) g_bot->stop_timer(tid);
            if (win) {
                bool first_clear = false;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  first_clear = hunt_clear_data[uid].count(g.monster_key) == 0;
                  hunt_clear_data[uid].insert(g.monster_key);
                }
                if (first_clear) { auto* md2 = find_monster(g.monster_key); if (md2) reward += md2->first_clear_reward; }
                add_chips(uid, reward);
                { std::lock_guard<std::mutex> lk(data_mutex);
                  for (auto& [k, c] : hunt_drops2) inventory_data[uid][k] += c; }
                save_chips(); save_hunt_clear();
                if (!hunt_drops2.empty()) save_inventory();
                ev.reply(dpp::ir_update_message, make_combat_end_msg(true, g, reward, first_clear, hunt_drops2, dn, av));
            } else {
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto& p = pet_data[uid]; bool already = false;
                  for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                  if (!already) p.statuses.push_back("受傷");
                }
                save_pet_data();
                ev.reply(dpp::ir_update_message, make_combat_end_msg(false, g, 0, false, {}, dn, av));
            }
        } else {
            { std::lock_guard<std::mutex> lk(data_mutex); monster_hunt_games[uid] = g; }
            ev.reply(dpp::ir_update_message, make_combat_msg(g, dn, av));
        }
        return;
    }

    // ── hunt_refresh_{uid}: 刷新戰鬥訊息 ────────────────────────────────────
    if (cid.rfind("hunt_refresh_", 0) == 0) {
        dpp::snowflake bu(std::stoull(cid.substr(13)));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
        MonsterHuntGame hg; bool hfound = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = monster_hunt_games.find(uid);
          if (it != monster_hunt_games.end()) { hg = it->second; hfound = true; }
        }
        if (!hfound) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的狩獵！").set_flags(dpp::m_ephemeral)); return; }
        dpp::embed se; se.set_title("🔄 訊息已刷新").set_color(0x95A5A6).set_description("請往下滑查看最新戰況！");
        ev.reply(dpp::ir_update_message, dpp::message().add_embed(se));
        auto fresh = make_combat_msg(hg, dn, av);
        fresh.channel_id = hg.channel_id;
        g_bot->message_create(fresh, [uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                auto nid = std::get<dpp::message>(cb.value).id;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = monster_hunt_games.find(uid);
                if (it != monster_hunt_games.end()) it->second.msg_id = nid;
            }
        });
    }
}

// ─── /怪物狩獵 & /狩獵規則 slash 指令 ────────────────────────────────────────

void handle_hunt_slash(const dpp::slashcommand_t& ev, const std::string& cmd_name,
                       dpp::snowflake uid, dpp::snowflake ch)
{
    (void)ch;
    const dpp::user& user = ev.command.get_issuing_user();
    std::string dn = ev.command.member.get_nickname().empty() ? user.username : ev.command.member.get_nickname();
    std::string av = user.get_avatar_url();

    if (cmd_name == "怪物狩獵" || cmd_name == "hunt") {
        Pet pet2;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto it = pet_data.find(uid); if (it != pet_data.end()) pet2 = it->second; }
        ev.reply(dpp::ir_channel_message_with_source, make_hunt_main_msg(uid, pet2, dn, av));
    }
    else if (cmd_name == "狩獵規則" || cmd_name == "huntrules") {
        ev.reply(dpp::ir_channel_message_with_source, dpp::message().add_embed(make_hunt_rules_embed()));
    }
}
