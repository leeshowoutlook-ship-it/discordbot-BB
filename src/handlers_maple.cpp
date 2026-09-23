#include "types.h"
#include "chips.h"
#include "maple.h"
#include "handler_decls.h"
#include <cstdio>

void load_maple_all_data() {
    load_maple_data();
    load_maple_wb_state();
    load_maple_raid_rooms();
    load_maple_exp_event();
    load_maple_faction_state();
}

void maple_save_exp_event() { save_maple_exp_event(); }

// ─── 交易輔助（讓 !交易／/交易 支援楓之谷卷軸、裝備與瘋幣）────────────────────
// 只有「沒點過卷軸」的裝備可交易，而且要放在背包（未穿在身上）。強化過的裝備不可交易。

bool mv_item_info(int id, std::string& key_out, std::string& name_out) {
    if (const MapleScrollDef* s = maple_find_scroll_by_id(id)) {
        key_out = s->key; name_out = s->name; return true;
    }
    if (const MapleItemDef* it = maple_find_item_by_id(id)) {
        if (!it->sellable) return false;            // 新手木劍等不可交易
        key_out = it->key; name_out = it->name; return true;
    }
    return false;
}
bool mv_is_item_key(const std::string& key) {
    if (maple_find_scroll(key)) return true;
    const MapleItemDef* it = maple_find_item(key);
    return it && it->sellable;
}

// 強化過（點過卷軸）的裝備不可交易，只算背包裡「純裝備」的份數。
bool mv_locked_has_item(dpp::snowflake uid, const std::string& key, int64_t qty) {
    auto it = maple_data.find(uid);
    if (it == maple_data.end()) return false;
    if (maple_find_scroll(key)) {
        auto sit = it->second.scrolls.find(key);
        return sit != it->second.scrolls.end() && sit->second >= qty;
    }
    auto eit = it->second.equipment.find(key);
    return eit != it->second.equipment.end() && eit->second >= qty;
}
bool mv_has_item(dpp::snowflake uid, const std::string& key, int64_t qty) {
    std::lock_guard<std::mutex> lk(data_mutex);
    return mv_locked_has_item(uid, key, qty);
}
void mv_locked_transfer_item(dpp::snowflake from, dpp::snowflake to, const std::string& key, int64_t qty) {
    bool is_scroll = maple_find_scroll(key) != nullptr;
    auto& fm = is_scroll ? maple_data[from].scrolls : maple_data[from].equipment;
    auto& tm = is_scroll ? maple_data[to].scrolls   : maple_data[to].equipment;
    fm[key] -= qty;
    if (fm[key] <= 0) fm.erase(key);
    tm[key] += qty;
}
int64_t mv_locked_get_coins(dpp::snowflake uid) {
    auto it = maple_data.find(uid);
    return it == maple_data.end() ? 0 : it->second.coins;
}
int64_t mv_get_coins(dpp::snowflake uid) {
    std::lock_guard<std::mutex> lk(data_mutex);
    return mv_locked_get_coins(uid);
}
void mv_locked_add_coins(dpp::snowflake uid, int64_t delta) {
    maple_data[uid].coins += delta;
    if (maple_data[uid].coins < 0) maple_data[uid].coins = 0;
}
void mv_save_data() { save_maple_data(); }

// 管理員給道具用：接受道具ID（數字）或 key 字串，找楓之谷卷軸／裝備
bool mv_resolve_item(const std::string& raw, std::string& key_out, std::string& name_out) {
    try {
        int id = std::stoi(raw);
        if (mv_item_info(id, key_out, name_out)) return true;
    } catch (...) {}
    if (const MapleScrollDef* s = maple_find_scroll(raw)) { key_out = s->key; name_out = s->name; return true; }
    if (const MapleItemDef* it = maple_find_item(raw)) {
        if (!it->sellable) return false;
        key_out = it->key; name_out = it->name; return true;
    }
    return false;
}
// 管理員給道具用：直接給予（不扣除任何人），qty 可為負（沒收，下限0）；回傳實際變動量。呼叫前需持有 data_mutex。
int64_t mv_locked_give_item(dpp::snowflake uid, const std::string& key, int64_t qty) {
    bool is_scroll = maple_find_scroll(key) != nullptr;
    auto& m = is_scroll ? maple_data[uid].scrolls : maple_data[uid].equipment;
    int64_t before = m.count(key) ? m[key] : 0;
    int64_t after = before + qty;
    if (after < 0) after = 0;
    if (after <= 0) m.erase(key); else m[key] = after;
    return after - before;
}

std::vector<MvOwnedItem> mv_list_owned_items(dpp::snowflake uid) {
    std::vector<MvOwnedItem> out;
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = maple_data.find(uid);
    if (it == maple_data.end()) return out;
    for (auto& [key, qty] : it->second.scrolls) {
        if (qty <= 0) continue;
        if (auto* s = maple_find_scroll(key)) out.push_back({key, s->name, s->item_id, qty});
    }
    for (auto& [key, qty] : it->second.equipment) {
        if (qty <= 0) continue;
        if (auto* i = maple_find_item(key)) out.push_back({key, i->name, i->item_id, qty});
    }
    return out;
}

static std::string maple_display_name(const dpp::user& user, const std::string& nick) {
    return nick.empty() ? user.username : nick;
}

// ─── Message (!養成) ────────────────────────────────────────────────────────

void handle_maple_message(const dpp::message_create_t& ev, const std::string& content, dpp::snowflake uid, dpp::snowflake ch) {
    if (content == "!養成") {
        std::string dn = maple_display_name(ev.msg.author, ev.msg.member.get_nickname());
        std::string av = ev.msg.author.get_avatar_url();
        dpp::message m = make_maple_home_msg(uid, dn, av);
        m.channel_id = ch;
        g_bot->message_create(m);
        return;
    }
}

// ─── Slash (/養成, /growth) ──────────────────────────────────────────────────

void handle_maple_slash(const dpp::slashcommand_t& ev, const std::string& cmd_name, dpp::snowflake uid, dpp::snowflake ch) {
    (void)cmd_name; (void)ch;
    dpp::user user = ev.command.get_issuing_user();
    std::string dn = maple_display_name(user, ev.command.member.get_nickname());
    std::string av = user.get_avatar_url();
    ev.reply(dpp::ir_channel_message_with_source, make_maple_home_msg(uid, dn, av));
}

// ─── Buttons ─────────────────────────────────────────────────────────────────

static void handle_maple_button_impl(const dpp::button_click_t& ev) {
    const std::string& cid = ev.custom_id;
    dpp::user user = ev.command.get_issuing_user();
    dpp::snowflake uid = user.id;
    std::string dn = maple_display_name(user, ev.command.member.get_nickname());
    std::string av = user.get_avatar_url();

    auto check_owner = [&](const std::string& prefix) -> bool {
        dpp::snowflake owner(std::stoull(cid.substr(prefix.size())));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral));
            return false;
        }
        return true;
    };

    if (cid.rfind("maple_home_", 0) == 0) {
        if (!check_owner("maple_home_")) return;
        ev.reply(dpp::ir_update_message, make_maple_home_msg(uid, dn, av));
        return;
    }

    if (cid.rfind("maple_ap_", 0) == 0) {
        if (!check_owner("maple_ap_")) return;
        MapleCharacter c0 = maple_get_or_create(uid);
        if (maple_is_adventuring(c0)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 冒險中無法調整能力值！").set_flags(dpp::m_ephemeral)); return;
        }
        if (!maple_ap_unlocked(c0)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 尚未點滿「能力值自由」，還不能使用！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_ap_msg(uid));
        return;
    }

    if (cid.rfind("maple_eq_", 0) == 0) {
        if (!check_owner("maple_eq_")) return;
        MapleCharacter c0 = maple_get_or_create(uid);
        if (maple_is_adventuring(c0)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 冒險中無法調整裝備！").set_flags(dpp::m_ephemeral)); return;
        }
        if (!maple_equip_unlocked(c0)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 尚未點滿「換裝自由」，還不能使用！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_equip_msg(uid));
        return;
    }

    if (cid.rfind("maple_adv_", 0) == 0) {
        if (!check_owner("maple_adv_")) return;
        ev.reply(dpp::ir_update_message, make_maple_adventure_msg(uid));
        return;
    }

    if (cid.rfind("maple_advopen_", 0) == 0) {
        std::string rest = cid.substr(14);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string region_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_adv_preview_msg(uid, region_key));
        return;
    }

    if (cid.rfind("maple_advstart_", 0) == 0) {
        std::string rest = cid.substr(15);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string region_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleAdvRegionDef* region = maple_find_adv_region(region_key);
        if (!region || !region->open) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 已經在冒險中了！").set_flags(dpp::m_ephemeral)); return;
            }
            if (maple_is_wb_fighting(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 正在挑戰野外首領，無法同時出發冒險！").set_flags(dpp::m_ephemeral)); return;
            }
            c.adv_region = region_key;
            c.adv_started_at = time(nullptr);
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_adv_status_msg(uid));
        return;
    }

    if (cid.rfind("maple_advstatus_", 0) == 0) {
        if (!check_owner("maple_advstatus_")) return;
        if (!maple_is_adventuring(maple_get_or_create(uid))) {
            ev.reply(dpp::ir_update_message, make_maple_adv_region_list_msg(uid)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_adv_status_msg(uid));
        return;
    }

    if (cid.rfind("maple_advsettle_", 0) == 0) {
        if (!check_owner("maple_advsettle_")) return;
        int64_t exp_gain = 0, coin_gain = 0, secs = 0, faction_exp_gained = 0;
        int level_ups = 0;
        std::string region_name, faction_name;
        bool ok = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 目前沒有進行中的冒險！").set_flags(dpp::m_ephemeral)); return;
            }
            const MapleAdvRegionDef* region = maple_find_adv_region(c.adv_region);
            region_name = region ? region->name : c.adv_region;
            maple_adv_progress(c, exp_gain, coin_gain, secs);
            c.coins += coin_gain;
            level_ups = maple_apply_exp(c, exp_gain);
            // 每升一級陣營+該等級經驗，這裡算出這次總共加了多少（等差級數）方便顯示
            if (level_ups > 0 && !c.faction_key.empty()) {
                faction_exp_gained = (int64_t)level_ups * (2 * c.level - level_ups + 1) / 2;
                const MapleFactionDef* fd = maple_find_faction(c.faction_key);
                faction_name = fd ? fd->name : c.faction_key;
            }
            c.adv_region.clear();
            c.adv_started_at = 0;
            ok = true;
        }
        if (!ok) return;
        save_maple_data();
        if (level_ups > 0) save_maple_faction_state();
        ev.reply(dpp::ir_update_message, make_maple_adv_settle_msg(uid, region_name, exp_gain, coin_gain, secs,
                                                                    level_ups, faction_name, faction_exp_gained));
        return;
    }

    if (cid.rfind("maple_advcancel_", 0) == 0) {
        if (!check_owner("maple_advcancel_")) return;
        if (!maple_is_adventuring(maple_get_or_create(uid))) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 目前沒有進行中的冒險！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_adv_cancel_confirm_msg(uid));
        return;
    }

    if (cid.rfind("maple_advcancelok_", 0) == 0) {
        if (!check_owner("maple_advcancelok_")) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 目前沒有進行中的冒險！").set_flags(dpp::m_ephemeral)); return;
            }
            c.adv_region.clear();
            c.adv_started_at = 0;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_adv_region_list_msg(uid));
        return;
    }

    // ── 野外首領 ─────────────────────────────────────────────────────────
    if (cid.rfind("maple_wb_", 0) == 0) {
        if (!check_owner("maple_wb_")) return;
        ev.reply(dpp::ir_update_message, make_maple_wb_msg(uid));
        return;
    }

    if (cid.rfind("maple_ambush_", 0) == 0) {
        if (!check_owner("maple_ambush_")) return;
        ev.reply(dpp::ir_update_message, make_maple_ambush_msg(uid));
        return;
    }

    // ── 突襲首領：選王／房間列表 ─────────────────────────────────────────────
    if (cid.rfind("maple_raidsel_", 0) == 0) {
        std::string rest = cid.substr(14);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string boss_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_raid_room_list_msg(uid, boss_key));
        return;
    }

    if (cid.rfind("maple_raidcreate_", 0) == 0) {
        std::string rest = cid.substr(17);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string boss_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleRaidBossDef* boss = maple_find_raid_boss(boss_key);
        if (!boss || !boss->open) return;
        std::string room_id;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!c.raid_room_id.empty() && maple_find_raid_room(c.raid_room_id)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你已經在一個房間裡了！").set_flags(dpp::m_ephemeral)); return;
            }
            if (maple_is_adventuring(c) || maple_is_wb_fighting(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中或挑戰野外首領中無法開房間！").set_flags(dpp::m_ephemeral)); return;
            }
            maple_raid_week_reset_if_needed(c);
            if (!maple_raid_week_has_attempt(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 本週的突襲首領次數已經用完了！（" + std::to_string(maple_raid_week_used(c))
                        + "/" + std::to_string(maple_raid_week_allowed(c)) + "，可以去特殊商店花瘋幣買額外次數）")
                        .set_flags(dpp::m_ephemeral)); return;
            }
            room_id = "r" + std::to_string(maple_raid_room_seq++);
            MapleRaidRoom room;
            room.id = room_id;
            room.boss_key = boss_key;
            room.leader_uid = uid;
            room.channel_id = ev.command.channel_id;
            room.members.push_back({uid, dn});
            room.created_at = time(nullptr);
            maple_raid_rooms[room_id] = room;
            c.raid_room_id = room_id;
        }
        save_maple_raid_rooms();
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_raid_lobby_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidjoin_", 0) == 0) {
        std::string rest = cid.substr(15);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            maple_raid_week_reset_if_needed(c);
            if (!room || room->state != "waiting") err = "這間房間已經不能加入了。";
            else if (!c.raid_room_id.empty() && maple_find_raid_room(c.raid_room_id)) err = "你已經在一個房間裡了！";
            else if (maple_is_adventuring(c) || maple_is_wb_fighting(c)) err = "冒險中或挑戰野外首領中無法加入房間！";
            else if ((int)room->members.size() >= MAPLE_RAID_ROOM_MAX_MEMBERS) err = "這間房間已經滿了。";
            else if (!maple_raid_week_has_attempt(c))
                err = "本週的突襲首領次數已經用完了！（" + std::to_string(maple_raid_week_used(c))
                    + "/" + std::to_string(maple_raid_week_allowed(c)) + "，可以去特殊商店花瘋幣買額外次數）";
            else {
                room->members.push_back({uid, dn});
                c.raid_room_id = room_id;
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_raid_rooms();
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_raid_lobby_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidlobby_", 0) == 0) {
        std::string rest = cid.substr(16);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_raid_lobby_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidleave_", 0) == 0) {
        std::string rest = cid.substr(16);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            if (room && room->state == "waiting" && !maple_raid_room_is_leader(*room, uid)) {
                room->members.erase(std::remove_if(room->members.begin(), room->members.end(),
                    [&](const MapleRaidMember& m) { return m.uid == uid; }), room->members.end());
                maple_data[uid].raid_room_id.clear();
            }
        }
        save_maple_raid_rooms();
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_raid_boss_list_msg(uid));
        return;
    }

    if (cid.rfind("maple_raiddisband_", 0) == 0) {
        std::string rest = cid.substr(18);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            if (room && maple_raid_room_is_leader(*room, uid) && room->state == "waiting") {
                for (auto& m : room->members) maple_data[m.uid].raid_room_id.clear();
                maple_raid_rooms.erase(room_id);
            }
        }
        save_maple_raid_rooms();
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_raid_boss_list_msg(uid));
        return;
    }

    if (cid.rfind("maple_raidkickopen_", 0) == 0) {
        std::string rest = cid.substr(19);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_raid_kick_pick_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidkickdo_", 0) == 0) {
        std::string rest = cid.substr(17);
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string rest2 = rest.substr(sep1 + 1);
        size_t sep2 = rest2.rfind('_');
        if (sep2 == std::string::npos) return;
        std::string room_id = rest2.substr(0, sep2);
        dpp::snowflake target(std::stoull(rest2.substr(sep2 + 1)));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            if (room && maple_raid_room_is_leader(*room, uid) && room->state == "waiting" && target != uid) {
                room->members.erase(std::remove_if(room->members.begin(), room->members.end(),
                    [&](const MapleRaidMember& m) { return m.uid == target; }), room->members.end());
                maple_data[target].raid_room_id.clear();
            }
        }
        save_maple_raid_rooms();
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_raid_kick_pick_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidstart_", 0) == 0) {
        std::string rest = cid.substr(16);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            if (!room || !maple_raid_room_is_leader(*room, uid)) err = "找不到房間或你不是隊長。";
            else if (room->state != "waiting") err = "這場討伐已經開始了。";
            else {
                double dps = maple_raid_team_total_dps_locked(*room);
                room->team_dps_x100 = (int64_t)llround(dps * 100.0);
                room->accum_secs = 0;
                room->resume_at  = time(nullptr);
                room->state = "fighting";
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_raid_rooms();
        ev.reply(dpp::ir_update_message, make_maple_raid_status_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidstatus_", 0) == 0) {
        std::string rest = cid.substr(17);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        dpp::message m = make_maple_raid_status_msg(uid, room_id);
        save_maple_raid_rooms(); // 上面那次呼叫可能觸發了自動暫停或擊敗判定，存一下
        ev.reply(dpp::ir_update_message, m);
        return;
    }

    if (cid.rfind("maple_raidcheckin_", 0) == 0) {
        std::string rest = cid.substr(18);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            if (room && room->state == "fighting" && room->resume_at <= 0 && maple_raid_room_has_member(*room, uid))
                room->resume_at = time(nullptr);
        }
        save_maple_raid_rooms();
        ev.reply(dpp::ir_update_message, make_maple_raid_status_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidsettle_", 0) == 0) {
        std::string rest = cid.substr(17);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err, summary, boss_name;
        dpp::snowflake announce_ch = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            if (!room || !maple_raid_room_is_leader(*room, uid)) { err = "找不到房間或你不是隊長。"; }
            else {
                const MapleRaidBossDef* boss = maple_find_raid_boss(room->boss_key);
                int64_t hp = boss ? boss->hp : 0;
                int64_t dealt = maple_raid_damage_dealt_locked(*room, time(nullptr));
                if (dealt < hp) { err = "首領還沒被擊敗。"; }
                else {
                    boss_name = boss ? boss->name : room->boss_key;
                    announce_ch = room->channel_id;
                    std::mt19937& rng = maple_wb_rng();
                    summary = "## 🏆 " + boss_name + " 討伐成功！\n";
                    for (auto& m : room->members) {
                        auto& c = maple_data[m.uid];
                        maple_raid_week_reset_if_needed(c);
                        c.raid_week_used++;
                        int64_t exp_gain = boss ? boss->exp : 0;
                        int64_t coin_gain = boss ? std::uniform_int_distribution<int64_t>(boss->coin_min, boss->coin_max)(rng) : 0;
                        c.coins += coin_gain;
                        int level_ups = maple_apply_exp(c, exp_gain);
                        if (boss && boss->faction_exp > 0) maple_faction_apply_exp(c.faction_key, boss->faction_exp);
                        std::vector<std::string> drops;
                        if (boss) {
                            for (auto& d : boss->drops) {
                                if (d.set.empty()) continue;
                                if (std::uniform_int_distribution<int>(0, 999)(rng) >= d.per_mille) continue;
                                const std::string& key = d.set.size() == 1 ? d.set[0]
                                    : d.set[std::uniform_int_distribution<int>(0, (int)d.set.size() - 1)(rng)];
                                if (const MapleScrollDef* sd = maple_find_scroll(key)) { c.scrolls[key]++; drops.push_back(sd->name); }
                                else if (const MapleItemDef* it = maple_find_item(key)) { c.equipment[key]++; drops.push_back(it->name); }
                            }
                        }
                        summary += "**" + m.display_name + "**：+" + std::to_string(exp_gain) + " EXP、+"
                                 + std::to_string(coin_gain) + " 瘋幣" + (level_ups > 0 ? "（升級！）" : "");
                        if (boss && boss->faction_exp > 0 && !c.faction_key.empty()) {
                            const MapleFactionDef* fd = maple_find_faction(c.faction_key);
                            summary += "、陣營「" + (fd ? fd->name : c.faction_key) + "」+" + std::to_string(boss->faction_exp) + " 經驗";
                        }
                        if (!drops.empty()) {
                            summary += "\n　掉落：";
                            for (size_t i = 0; i < drops.size(); i++) summary += (i ? "、" : "") + drops[i];
                        }
                        summary += "\n";
                        c.raid_room_id.clear();
                    }
                    maple_raid_rooms.erase(room_id);
                }
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_raid_rooms();
        save_maple_data();
        save_maple_faction_state();
        dpp::message result;
        result.set_content(summary);
        if (announce_ch) result.channel_id = announce_ch;
        ev.reply(dpp::ir_update_message, make_maple_ambush_msg(uid));
        if (announce_ch) g_bot->message_create(result);
        return;
    }

    if (cid.rfind("maple_faction_", 0) == 0) {
        if (!check_owner("maple_faction_")) return;
        ev.reply(dpp::ir_update_message, make_maple_faction_msg(uid));
        return;
    }

    if (cid.rfind("maple_factionpick_", 0) == 0) {
        if (!check_owner("maple_factionpick_")) return;
        ev.reply(dpp::ir_update_message, make_maple_faction_pick_msg(uid));
        return;
    }

    if (cid.rfind("maple_factionjoin_", 0) == 0) {
        std::string rest = cid.substr(18);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string fkey = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        if (!maple_find_faction(fkey)) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.faction_key == fkey) { err = "你已經在這個陣營了。"; }
            else {
                bool switching = !c.faction_key.empty();
                int64_t cost = switching ? (MAPLE_FACTION_JOIN_FEE + MAPLE_FACTION_LEAVE_FEE) : MAPLE_FACTION_JOIN_FEE;
                if (c.coins < cost) err = "瘋幣不足！";
                else {
                    c.coins -= cost;
                    c.faction_key = fkey;
                }
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_faction_msg(uid));
        return;
    }

    if (cid.rfind("maple_factionleaveconfirm_", 0) == 0) {
        if (!check_owner("maple_factionleaveconfirm_")) return;
        MapleCharacter c = maple_get_or_create(uid);
        if (c.faction_key.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 你目前沒有加入任何陣營！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_faction_leave_confirm_msg(uid));
        return;
    }

    if (cid.rfind("maple_factionleaveok_", 0) == 0) {
        if (!check_owner("maple_factionleaveok_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.faction_key.empty()) err = "你目前沒有加入任何陣營！";
            else if (c.coins < MAPLE_FACTION_LEAVE_FEE) err = "瘋幣不足！";
            else {
                c.coins -= MAPLE_FACTION_LEAVE_FEE;
                c.faction_key.clear(); // 陣營本身的等級/經驗是全服共用，退出不影響
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_faction_msg(uid));
        return;
    }

    if (cid.rfind("maple_factionmembers_", 0) == 0) {
        if (!check_owner("maple_factionmembers_")) return;
        ev.reply(dpp::ir_update_message, make_maple_faction_members_msg(uid));
        return;
    }

    if (cid.rfind("maple_factionbuff_", 0) == 0) {
        if (!check_owner("maple_factionbuff_")) return;
        ev.reply(dpp::ir_update_message, make_maple_faction_buff_msg(uid));
        return;
    }

    if (cid.rfind("maple_factiondonate_", 0) == 0) {
        if (!check_owner("maple_factiondonate_")) return;
        ev.reply(dpp::ir_update_message, make_maple_faction_donate_msg(uid));
        return;
    }

    if (cid.rfind("maple_factiondonateok_", 0) == 0) {
        if (!check_owner("maple_factiondonateok_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            maple_faction_donate_week_reset_if_needed(c);
            if (c.faction_key.empty()) err = "你目前沒有加入任何陣營！";
            else if (maple_faction_donate_used_this_week(c)) err = "本週已經捐贈過了！";
            else if (c.coins < MAPLE_FACTION_DONATE_COST) err = "瘋幣不足！";
            else {
                c.coins -= MAPLE_FACTION_DONATE_COST;
                c.faction_donate_week_used++;
                maple_faction_apply_exp(c.faction_key, MAPLE_FACTION_DONATE_EXP);
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_update_message, make_maple_faction_donate_msg(uid, "❌ " + err));
            return;
        }
        save_maple_data();
        save_maple_faction_state();
        ev.reply(dpp::ir_update_message, make_maple_faction_donate_msg(uid, "✅ 捐贈成功！陣營 +" + std::to_string(MAPLE_FACTION_DONATE_EXP) + " 經驗"));
        return;
    }

    if (cid.rfind("maple_wbopen_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string region_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleWbRegionDef* region = maple_find_wb_region(region_key);
        if (!region || !region->open) return;
        time_t now = time(nullptr);
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            bool up_now = maple_wb_is_up_locked(region_key, now);
            if (!up_now) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 首領已經被討伐，還沒重生！").set_flags(dpp::m_ephemeral)); return;
            }
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中無法挑戰野外首領！").set_flags(dpp::m_ephemeral)); return;
            }
            if (maple_is_wb_fighting(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 已經在挑戰野外首領了！").set_flags(dpp::m_ephemeral)); return;
            }
            c.wb_region        = region_key;
            c.wb_started_at    = now;
            c.wb_required_secs = maple_wb_kill_secs(c, region->boss);
            c.wb_epoch         = maple_wb_dead_since_locked(region_key);
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_wb_status_msg(uid));
        return;
    }

    if (cid.rfind("maple_wbcheck_", 0) == 0) {
        if (!check_owner("maple_wbcheck_")) return;
        MapleCharacter snap = maple_get_or_create(uid);
        if (!maple_is_wb_fighting(snap)) {
            ev.reply(dpp::ir_update_message, make_maple_wb_region_list_msg(uid)); return;
        }
        time_t now = time(nullptr);
        if (now - snap.wb_started_at < snap.wb_required_secs) {
            ev.reply(dpp::ir_update_message, make_maple_wb_status_msg(uid)); return;
        }
        const MapleWbRegionDef* region = maple_find_wb_region(snap.wb_region);
        std::string boss_name = region ? region->boss.name : "野外首領";
        bool win = false;
        int place = 0; // 本輪第幾位擊殺成功（1~MAPLE_WB_MAX_WINNERS）
        int64_t exp_gain = 0, coin_gain = 0;
        int level_ups = 0;
        std::vector<std::string> drops;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_is_wb_fighting(c)) { ev.reply(dpp::ir_update_message, make_maple_wb_region_list_msg(uid)); return; }
            auto& st = maple_wb_state[c.wb_region];
            // 這段期間本輪還沒關閉（沒人搶先湊滿名額）、而且本輪名額還沒滿，才算擊殺成功
            win = (st.dead_since == c.wb_epoch) && (st.round_kills < MAPLE_WB_MAX_WINNERS);
            if (win && region) {
                // 注意：這裡一定要用 std::mt19937+random_device，不能用裸的 rand()——
                // rand() 如果沒在「這條執行緒」上呼叫過 srand()，會從同一組固定序列開始跑，
                // 導致不同玩家/不同次結算全部拿到一模一樣的「隨機」結果（之前掉落物重複就是這個原因）。
                std::mt19937& wb_rng = maple_wb_rng();
                place = ++st.round_kills;
                if (st.round_kills >= MAPLE_WB_MAX_WINNERS) {
                    st.dead_since  = now; // 本輪湊滿名額，關閉本輪、開始算重生
                    st.round_kills = 0;   // 歸零給下一輪用
                    st.total_kills++;     // 這隻首領累計討伐數 +1（一輪關閉算一隻，不是每人+1）
                    int lo = region->respawn_min_lo, hi = region->respawn_min_hi;
                    int mins = (lo >= hi) ? lo : std::uniform_int_distribution<int>(lo, hi)(wb_rng);
                    st.respawn_secs = mins * 60; // 重生間隔在範圍內隨機決定，存起來給 maple_wb_is_up 判斷用
                }
                exp_gain  = region->boss.exp;
                coin_gain = std::uniform_int_distribution<int64_t>(region->boss.coin_min, region->boss.coin_max)(wb_rng);
                c.coins  += coin_gain;
                level_ups = maple_apply_exp(c, exp_gain);
                // 掉落表：每一筆各自獨立擲一次機率，中的話從 set 裡隨機選一個（scroll/item key 都支援）
                for (auto& d : region->boss.drops) {
                    if (d.set.empty()) continue;
                    if (std::uniform_int_distribution<int>(0, 999)(wb_rng) >= d.per_mille) continue;
                    const std::string& key = d.set.size() == 1 ? d.set[0]
                        : d.set[std::uniform_int_distribution<int>(0, (int)d.set.size() - 1)(wb_rng)];
                    if (const MapleScrollDef* sd = maple_find_scroll(key)) {
                        c.scrolls[key]++;
                        drops.push_back(sd->name + " ×1");
                    } else if (const MapleItemDef* it = maple_find_item(key)) {
                        c.equipment[key]++;
                        drops.push_back(it->name + " ×1");
                    }
                }
            }
            c.wb_region.clear();
            c.wb_started_at = 0;
            c.wb_required_secs = 0;
            c.wb_epoch = 0;
        }
        if (win) save_maple_wb_state();
        save_maple_data();
        if (level_ups > 0) save_maple_faction_state();
        ev.reply(dpp::ir_update_message, make_maple_wb_result_msg(uid, win, boss_name, exp_gain, coin_gain, level_ups, drops, place));
        return;
    }

    if (cid.rfind("maple_wbcancel_", 0) == 0) {
        if (!check_owner("maple_wbcancel_")) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_is_wb_fighting(c)) { ev.reply(dpp::ir_update_message, make_maple_wb_region_list_msg(uid)); return; }
            // 中斷不會結算戰果（沒有獎勵、不會標記王死亡），王會繼續開放給其他人挑戰
            c.wb_region.clear();
            c.wb_started_at = 0;
            c.wb_required_secs = 0;
            c.wb_epoch = 0;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_wb_region_list_msg(uid));
        return;
    }

    // ── 背包 ─────────────────────────────────────────────────────────────
    if (cid.rfind("maple_bag_", 0) == 0) {
        std::string rest = cid.substr(10);
        size_t sep = rest.rfind('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string tab = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, tab));
        return;
    }

    // 背包「其他」分頁翻頁：maple_bagpg_<uid>_<page>_<subcat>
    if (cid.rfind("maple_bagpg_", 0) == 0) {
        std::string rest = cid.substr(12);
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string rem = rest.substr(sep1 + 1);
        size_t sep2 = rem.rfind('_');
        int page = 0; std::string subcat;
        if (sep2 == std::string::npos) { try { page = std::stoi(rem); } catch (...) {} }
        else {
            try { page = std::stoi(rem.substr(0, sep2)); } catch (...) {}
            subcat = rem.substr(sep2 + 1);
        }
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "other", page, subcat));
        return;
    }

    // 背包分類切換：maple_bagcat_<uid>_<tab>_<subcat>
    if (cid.rfind("maple_bagcat_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string rem = rest.substr(sep1 + 1);
        size_t sep2 = rem.rfind('_');
        if (sep2 == std::string::npos) return;
        std::string tab = rem.substr(0, sep2);
        std::string subcat = rem.substr(sep2 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, tab, 0, subcat));
        return;
    }

    // 售出裝備：maple_sellconfirm_<uid>_<kind>_<ref>（kind=item/enh），確認後 maple_sellok_ 才真的扣掉
    if (cid.rfind("maple_sellconfirm_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_sellconfirm_").size());
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string after_uid = rest.substr(sep1 + 1);
        size_t sep2 = after_uid.find('_');
        if (sep2 == std::string::npos) return;
        std::string kind = after_uid.substr(0, sep2);
        std::string ref  = after_uid.substr(sep2 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_sell_confirm_msg(uid, kind, ref));
        return;
    }
    if (cid.rfind("maple_sellok_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_sellok_").size());
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string after_uid = rest.substr(sep1 + 1);
        size_t sep2 = after_uid.find('_');
        if (sep2 == std::string::npos) return;
        std::string kind = after_uid.substr(0, sep2);
        std::string ref  = after_uid.substr(sep2 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        int64_t gained = 0;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (kind == "enh") {
                int eid = 0; try { eid = std::stoi(ref); } catch (...) {}
                auto eit = std::find_if(c.enh_items.begin(), c.enh_items.end(),
                                        [&](const MapleEnhItem& e){ return e.id == eid; });
                if (eit == c.enh_items.end() || maple_enh_is_equipped(c, eid)) {
                    err = "找不到這件裝備，可能已經賣掉或穿上了。";
                } else {
                    const MapleItemDef* it = maple_find_item(eit->base_key);
                    if (!it) { err = "資料異常，找不到裝備定義。"; }
                    else {
                        gained = maple_item_sell_price(*it) + maple_enh_extra_sell_value(*eit);
                        c.coins += gained;
                        c.enh_items.erase(eit);
                    }
                }
            } else {
                auto qit = c.equipment.find(ref);
                const MapleItemDef* it = maple_find_item(ref);
                if (qit == c.equipment.end() || qit->second <= 0 || !it) {
                    err = "找不到這件裝備，可能已經賣掉或穿上了。";
                } else {
                    gained = maple_item_sell_price(*it);
                    c.coins += gained;
                    qit->second--;
                    if (qit->second <= 0) c.equipment.erase(qit);
                }
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "other"));
        return;
    }

    // ── 商店 ─────────────────────────────────────────────────────────────
    if (cid.rfind("maple_shop_", 0) == 0) {
        if (!check_owner("maple_shop_")) return;
        ev.reply(dpp::ir_update_message, make_maple_shop_msg(uid));
        return;
    }

    if (cid.rfind("maple_tokenshop_", 0) == 0) {
        if (!check_owner("maple_tokenshop_")) return;
        ev.reply(dpp::ir_update_message, make_maple_tokenshop_msg(uid));
        return;
    }

    // maple_tokenexmax_<uid>：兌換上限（金額伺服器端算）／maple_tokenex_<uid>_<amount>：固定金額
    if (cid.rfind("maple_tokenexmax_", 0) == 0 || cid.rfind("maple_tokenex_", 0) == 0) {
        bool is_max = cid.rfind("maple_tokenexmax_", 0) == 0;
        dpp::snowflake owner;
        int64_t amount = 0;
        if (is_max) {
            owner = dpp::snowflake(std::stoull(cid.substr(17)));
        } else {
            std::string rest = cid.substr(14);
            size_t sep = rest.rfind('_');
            if (sep == std::string::npos) return;
            owner  = dpp::snowflake(std::stoull(rest.substr(0, sep)));
            amount = std::atoll(rest.substr(sep + 1).c_str());
        }
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            int64_t wk = maple_token_week_now();
            if (c.token_week_id != wk) { c.token_week_id = wk; c.token_week_spent = 0; }
            int64_t remaining = MAPLE_TOKEN_WEEKLY_CAP - c.token_week_spent;
            int64_t have = chip_data.count(uid) ? chip_data[uid].chips : 0;
            if (is_max) amount = std::min(remaining, have);
            if (amount <= 0)             err = "沒有可兌換的額度或籌碼。";
            else if (amount > remaining) err = "本週兌換額度不足（剩 " + std::to_string(remaining < 0 ? 0 : remaining) + "）。";
            else if (amount > have)      err = "籌碼不足。";
            else {
                chip_data[uid].chips  -= amount;
                c.coins               += maple_token_coins_for(amount);
                c.token_week_spent    += amount;
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_chips();
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_tokenshop_msg(uid));
        return;
    }

    if (cid.rfind("maple_eqshop_", 0) == 0) {
        std::string rest = cid.substr(13);            // <uid>_<mode>_<cat>_<page>
        size_t s1 = rest.find('_');
        size_t s4 = rest.rfind('_');
        if (s1 == std::string::npos || s4 == s1) return;
        size_t s2 = rest.find('_', s1 + 1);
        if (s2 == std::string::npos || s2 >= s4) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, s1)));
        std::string mode = rest.substr(s1 + 1, s2 - s1 - 1);
        std::string cat  = rest.substr(s2 + 1, s4 - s2 - 1);
        int page = std::atoi(rest.substr(s4 + 1).c_str());
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_eqshop_msg(uid, mode, cat, page));
        return;
    }

    if (cid.rfind("maple_eqbuyok_", 0) == 0) {
        std::string rest = cid.substr(14);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string item_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleItemDef* it = maple_find_item(item_key);
        if (!it || it->price <= 0) return;
        bool bought = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.coins >= it->price) {
                c.coins -= it->price;
                c.equipment[item_key]++;
                bought = true;
            }
        }
        if (!bought) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 瘋幣不足！").set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message,
            make_maple_eqshop_msg(uid, maple_eqshop_mode_of_item(*it), maple_cat_of_item(*it), 0));
        return;
    }

    if (cid.rfind("maple_eqbuy_", 0) == 0) {
        std::string rest = cid.substr(12);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string item_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_eqbuy_confirm_msg(uid, item_key));
        return;
    }

    if (cid.rfind("maple_scshop_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t sep = rest.rfind('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        int page = std::atoi(rest.substr(sep + 1).c_str());
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_scshop_msg(uid, page));
        return;
    }

    // maple_scbuyok_<uid>_<key>_<qty>：固定數量／maple_scbuymax_<uid>_<key>：買到上限（數量伺服器端算）
    if (cid.rfind("maple_scbuyok_", 0) == 0 || cid.rfind("maple_scbuymax_", 0) == 0) {
        bool is_max = cid.rfind("maple_scbuymax_", 0) == 0;
        dpp::snowflake owner;
        std::string scroll_key;
        int64_t qty = 0;
        if (is_max) {
            std::string rest = cid.substr(15);          // <uid>_<key>
            size_t s1 = rest.find('_');
            if (s1 == std::string::npos) return;
            owner      = dpp::snowflake(std::stoull(rest.substr(0, s1)));
            scroll_key = rest.substr(s1 + 1);
        } else {
            std::string rest = cid.substr(14);           // <uid>_<key>_<qty>
            size_t s1 = rest.find('_');
            size_t s2 = rest.rfind('_');
            if (s1 == std::string::npos || s2 == std::string::npos || s2 <= s1) return;
            owner      = dpp::snowflake(std::stoull(rest.substr(0, s1)));
            scroll_key = rest.substr(s1 + 1, s2 - s1 - 1);
            qty        = std::atoll(rest.substr(s2 + 1).c_str());
        }
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleScrollDef* s = maple_find_scroll(scroll_key);
        if (!s || s->price <= 0) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (is_max) qty = std::min((int64_t)999, c.coins / s->price);
            int64_t cost = qty * s->price;
            if (qty <= 0)          err = "數量不對。";
            else if (c.coins < cost) err = "瘋幣不足。";
            else {
                c.coins -= cost;
                c.scrolls[scroll_key] += (int)qty;
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_scshop_msg(uid, 0));
        return;
    }

    if (cid.rfind("maple_scbuy_", 0) == 0) {
        std::string rest = cid.substr(12);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string scroll_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_scbuy_confirm_msg(uid, scroll_key));
        return;
    }

    if (cid.rfind("maple_skill_", 0) == 0) {
        if (!check_owner("maple_skill_")) return;
        ev.reply(dpp::ir_update_message, make_maple_skill_msg(uid));
        return;
    }

    if (cid.rfind("maple_rank_", 0) == 0) {
        std::string rest = cid.substr(11);           // <uid>_<filter>_<page>
        size_t sep2 = rest.rfind('_');
        if (sep2 == std::string::npos) return;
        int page = std::atoi(rest.substr(sep2 + 1).c_str());
        std::string rest2 = rest.substr(0, sep2);     // <uid>_<filter>
        size_t sep1 = rest2.rfind('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest2.substr(0, sep1)));
        std::string filter = rest2.substr(sep1 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_rank_msg(uid, page, filter));
        return;
    }

    if (cid.rfind("maple_atktype_", 0) == 0) {
        if (!check_owner("maple_atktype_")) return;
        MapleCharacter c0 = maple_get_or_create(uid);
        if (maple_is_adventuring(c0)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 冒險中無法調整攻擊方式！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_atktype_msg(uid));
        return;
    }

    if (cid.rfind("maple_atkpick_", 0) == 0) {
        std::string rest = cid.substr(14);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string skill_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中無法調整攻擊方式！").set_flags(dpp::m_ephemeral)); return;
            }
            if (skill_key == "normal") {
                c.adv_atk_skill.clear();
            } else {
                const MapleSkillDef* sd = maple_find_skill(skill_key);
                if (!sd || (sd->type != "damage_fixed" && sd->type != "damage_coef")
                    || maple_skill_level(c, skill_key) <= 0) return;
                if (!maple_skill_weapon_ok(c, *sd)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 武器不符，無法選用這個攻擊技能！").set_flags(dpp::m_ephemeral)); return;
                }
                c.adv_atk_skill = skill_key;
            }
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_atktype_msg(uid));
        return;
    }

    if (cid.rfind("maple_skreset_", 0) == 0) {
        if (!check_owner("maple_skreset_")) return;
        MapleCharacter c = maple_get_or_create(uid);
        if (c.skill_reset_used) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 你已經使用過重製技能點數了！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_skill_reset_confirm_msg(uid));
        return;
    }

    if (cid.rfind("maple_skresetok_", 0) == 0) {
        if (!check_owner("maple_skresetok_")) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.skill_reset_used) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你已經使用過重製技能點數了！").set_flags(dpp::m_ephemeral)); return;
            }
            c.skill_levels.clear();
            c.skill_reset_used = true;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_skill_msg(uid));
        return;
    }

    if (cid.rfind("maple_skresetno_", 0) == 0) {
        if (!check_owner("maple_skresetno_")) return;
        ev.reply(dpp::ir_update_message, make_maple_skill_msg(uid));
        return;
    }

    // 特殊商店：付費技能點數重製（10000 籌碼，可重複）
    if (cid.rfind("maple_spbuyresetok_", 0) == 0) {
        if (!check_owner("maple_spbuyresetok_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            int64_t have = chip_data.count(uid) ? chip_data[uid].chips : 0;
            if (have < MAPLE_SP_BUYRESET_COST) err = "籌碼不足！";
            else {
                chip_data[uid].chips -= MAPLE_SP_BUYRESET_COST;
                c.skill_levels.clear();
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_chips();
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_tokenshop_msg(uid));
        return;
    }
    if (cid.rfind("maple_spbuyreset_", 0) == 0) {
        if (!check_owner("maple_spbuyreset_")) return;
        if (get_chips(uid) < MAPLE_SP_BUYRESET_COST) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 籌碼不足！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_spbuyreset_confirm_msg(uid));
        return;
    }

    if (cid.rfind("maple_skadd_", 0) == 0) {
        std::string rest = cid.substr(12);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string skill_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleSkillDef* sd = maple_find_skill(skill_key);
        if (!sd) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_skill_visible(c, sd->job)) return;
            int lvl = maple_skill_level(c, skill_key);
            if (maple_sp_unspent(c) > 0 && lvl < sd->max_level && maple_skill_unlockable(c, *sd))
                c.skill_levels[skill_key] = lvl + 1;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_skill_msg(uid));
        return;
    }

    if (cid.rfind("maple_eqopen_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string slot = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_equip_slot_msg(uid, slot));
        return;
    }

    // 裝備「裸的」那份庫存：maple_eqpickraw_<uid>_<slot>_<item_key>
    if (cid.rfind("maple_eqpickraw_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_eqpickraw_").size());
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string rest2 = rest.substr(sep1 + 1);
        size_t sep2 = rest2.find('_');
        if (sep2 == std::string::npos) return;
        std::string slot = rest2.substr(0, sep2);
        std::string item_key = rest2.substr(sep2 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleItemDef* item = maple_find_item(item_key);
        if (!item || item->slot != slot) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中無法調整裝備！").set_flags(dpp::m_ephemeral)); return;
            }
            bool worn_is_raw = (maple_equipped_raw(c, slot) == item_key);
            int qty = c.equipment.count(item_key) ? c.equipment.at(item_key) : 0;
            if (worn_is_raw) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 已經裝備這個了！").set_flags(dpp::m_ephemeral)); return;
            }
            if (qty <= 0) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你沒有這件裝備的庫存！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!maple_meets_requirement(c, *item)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 條件不符，無法裝備！").set_flags(dpp::m_ephemeral)); return;
            }
            // 把目前這個部位的裝備放回去（純裝備回背包；強化實例不用處理，脫下來自動變成備用）
            std::string old_raw = maple_equipped_raw(c, slot);
            if (!maple_eq_is_enh(old_raw) && !old_raw.empty() && old_raw != "wooden_sword")
                c.equipment[old_raw]++;
            maple_set_equipped(c, slot, item_key);
            auto eqi = c.equipment.find(item_key);
            if (eqi != c.equipment.end() && eqi->second > 0) {
                eqi->second--;
                if (eqi->second <= 0) c.equipment.erase(eqi);
            }
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_equip_slot_msg(uid, slot));
        return;
    }

    // 裝備某個特定的強化實例：maple_eqpickenh_<uid>_<slot>_<enh_id>（同種類可能同時擁有好幾個不同強化結果的）
    if (cid.rfind("maple_eqpickenh_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_eqpickenh_").size());
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string rest2 = rest.substr(sep1 + 1);
        size_t sep2 = rest2.find('_');
        if (sep2 == std::string::npos) return;
        std::string slot = rest2.substr(0, sep2);
        int enh_id = 0;
        try { enh_id = std::stoi(rest2.substr(sep2 + 1)); } catch (...) { return; }
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中無法調整裝備！").set_flags(dpp::m_ephemeral)); return;
            }
            auto eit = std::find_if(c.enh_items.begin(), c.enh_items.end(),
                                    [&](const MapleEnhItem& e){ return e.id == enh_id; });
            if (eit == c.enh_items.end()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 找不到這件強化裝備，可能已經賣掉了！").set_flags(dpp::m_ephemeral)); return;
            }
            const MapleItemDef* item = maple_find_item(eit->base_key);
            if (!item || item->slot != slot) return;
            if (maple_enh_is_equipped(c, enh_id)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 已經裝備這個了！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!maple_meets_requirement(c, *item)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 條件不符，無法裝備！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string old_raw = maple_equipped_raw(c, slot);
            if (!maple_eq_is_enh(old_raw) && !old_raw.empty() && old_raw != "wooden_sword")
                c.equipment[old_raw]++;
            maple_set_equipped(c, slot, "#" + std::to_string(enh_id));
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_equip_slot_msg(uid, slot));
        return;
    }

    if (cid.rfind("maple_equnequip_", 0) == 0) {
        std::string rest = cid.substr(16);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string slot = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中無法調整裝備！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string old_raw = maple_equipped_raw(c, slot);
            if (!maple_eq_is_enh(old_raw) && !old_raw.empty() && old_raw != "wooden_sword")
                c.equipment[old_raw]++;
            maple_set_equipped(c, slot, slot == "weapon" ? std::string("wooden_sword") : std::string());
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_equip_slot_msg(uid, slot));
        return;
    }

    if (cid.rfind("maple_enhpick_", 0) == 0) {
        if (!check_owner("maple_enhpick_")) return;
        MapleCharacter c0 = maple_get_or_create(uid);
        if (maple_is_adventuring(c0)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 冒險中無法強化裝備！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_enh_pick_msg(uid));
        return;
    }

    if (cid.rfind("maple_enhopen_", 0) == 0) {
        std::string rest = cid.substr(14);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string slot = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_enh_msg(uid, slot));
        return;
    }

    if (cid.rfind("maple_enhuse_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string rest2 = rest.substr(sep1 + 1);
        size_t sep2 = rest2.find('_');
        if (sep2 == std::string::npos) return;
        std::string slot = rest2.substr(0, sep2);
        std::string scroll_key = rest2.substr(sep2 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string result;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中無法強化裝備！").set_flags(dpp::m_ephemeral)); return;
            }
            bool ok = false;
            result = maple_enh_apply(c, slot, scroll_key, ok);
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_enh_msg(uid, slot, result));
        return;
    }

    if (cid.rfind("maple_apadd_", 0) == 0) {
        std::string rest = cid.substr(12);
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string rest2 = rest.substr(sep1 + 1);
        size_t sep2 = rest2.rfind('_');
        if (sep2 == std::string::npos) return;
        std::string stat = rest2.substr(0, sep2);
        int amount = std::stoi(rest2.substr(sep2 + 1));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中無法調整能力值！").set_flags(dpp::m_ephemeral)); return;
            }
            int add = std::min(amount, maple_unspent_ap(c));
            if (add > 0) {
                if      (stat == "str") c.str_stat += add;
                else if (stat == "dex") c.dex_stat += add;
                else if (stat == "int") c.int_stat += add;
                else if (stat == "luk") c.luk_stat += add;
            }
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_ap_msg(uid));
        return;
    }

    if (cid.rfind("maple_apreset_", 0) == 0) {
        if (!check_owner("maple_apreset_")) return;
        MapleCharacter c = maple_get_or_create(uid);
        if (maple_is_adventuring(c)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 冒險中無法調整能力值！").set_flags(dpp::m_ephemeral)); return;
        }
        if (c.ap_reset_used) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 你已經使用過重新配點了！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_ap_reset_confirm_msg(uid));
        return;
    }

    if (cid.rfind("maple_apresetok_", 0) == 0) {
        if (!check_owner("maple_apresetok_")) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 冒險中無法調整能力值！").set_flags(dpp::m_ephemeral)); return;
            }
            if (c.ap_reset_used) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你已經使用過重新配點了！").set_flags(dpp::m_ephemeral)); return;
            }
            c.str_stat = 4; c.dex_stat = 4; c.int_stat = 4; c.luk_stat = 4;
            c.ap_reset_used = true;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_ap_msg(uid));
        return;
    }

    if (cid.rfind("maple_apresetno_", 0) == 0) {
        if (!check_owner("maple_apresetno_")) return;
        ev.reply(dpp::ir_update_message, make_maple_ap_msg(uid));
        return;
    }

    // 特殊商店：付費能力值重製（10000 籌碼，可重複）
    if (cid.rfind("maple_apbuyresetok_", 0) == 0) {
        if (!check_owner("maple_apbuyresetok_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            int64_t have = chip_data.count(uid) ? chip_data[uid].chips : 0;
            if (maple_is_adventuring(c))          err = "冒險中無法調整能力值！";
            else if (have < MAPLE_AP_BUYRESET_COST) err = "籌碼不足！";
            else {
                chip_data[uid].chips -= MAPLE_AP_BUYRESET_COST;
                c.str_stat = 4; c.dex_stat = 4; c.int_stat = 4; c.luk_stat = 4;
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_chips();
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_tokenshop_msg(uid));
        return;
    }
    if (cid.rfind("maple_raidbuyattempt_", 0) == 0) {
        if (!check_owner("maple_raidbuyattempt_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            maple_raid_week_reset_if_needed(c);
            if (c.raid_week_extra > 0) err = "這週已經買過額外次數了！";
            else if (c.coins < MAPLE_RAID_EXTRA_ATTEMPT_PRICE) err = "瘋幣不足！";
            else {
                c.coins -= MAPLE_RAID_EXTRA_ATTEMPT_PRICE;
                c.raid_week_extra = 1;
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_tokenshop_msg(uid));
        return;
    }

    if (cid.rfind("maple_apbuyreset_", 0) == 0) {
        if (!check_owner("maple_apbuyreset_")) return;
        MapleCharacter c = maple_get_or_create(uid);
        if (maple_is_adventuring(c)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 冒險中無法調整能力值！").set_flags(dpp::m_ephemeral)); return;
        }
        if (get_chips(uid) < MAPLE_AP_BUYRESET_COST) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 籌碼不足！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_apbuyreset_confirm_msg(uid));
        return;
    }

    if (cid.rfind("maple_j1open_", 0) == 0) {
        if (!check_owner("maple_j1open_")) return;
        MapleCharacter c = maple_get_or_create(uid);
        if (!maple_can_first_job(c)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 還不能轉職！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_job1_select_msg(uid));
        return;
    }

    if (cid.rfind("maple_j1pick_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string job_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        if (!maple_find_job(job_key) || maple_find_job(job_key)->tier != 1) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_can_first_job(c)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 還不能轉職！").set_flags(dpp::m_ephemeral)); return;
            }
            c.job = job_key;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_home_msg(uid, dn, av));
        return;
    }

    if (cid.rfind("maple_j2open_", 0) == 0) {
        if (!check_owner("maple_j2open_")) return;
        MapleCharacter c = maple_get_or_create(uid);
        if (!maple_can_second_job(c)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 還不能二轉！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_job2_select_msg(uid));
        return;
    }

    if (cid.rfind("maple_j2pick_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string job_key = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleJobDef* jd = maple_find_job(job_key);
        if (!jd || jd->tier != 2) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_can_second_job(c) || jd->parent != c.job) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 還不能二轉！").set_flags(dpp::m_ephemeral)); return;
            }
            c.job = job_key;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_home_msg(uid, dn, av));
        return;
    }
}

void handle_maple_button(const dpp::button_click_t& ev) {
    try {
        handle_maple_button_impl(ev);
    } catch (const std::exception& e) {
        FILE* f = fopen("C:\\bot_debug.txt", "a");
        if (f) {
            fprintf(f, "[maple_button EXCEPTION] cid=%s what=%s\n",
                    ev.custom_id.c_str(), e.what());
            fclose(f);
        }
        try {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("⚠️ 這個操作發生錯誤，請稍後再試。").set_flags(dpp::m_ephemeral));
        } catch (...) {}
    }
}

// ─── Select menus ────────────────────────────────────────────────────────────

void handle_maple_select(const dpp::select_click_t& ev, dpp::snowflake uid) {
    const std::string& cid = ev.custom_id;

    // maple_ranksel_<uid>　值＝選中的職業分類 key（排行榜職業篩選）
    if (cid.rfind("maple_ranksel_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(std::string("maple_ranksel_").size())));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string filter = ev.values.empty() ? std::string("all") : ev.values[0];
        ev.reply(dpp::ir_update_message, make_maple_rank_msg(uid, 0, filter));
        return;
    }

    // maple_eqshopsel_<uid>_<mode>　值＝選中的分類 key
    if (cid.rfind("maple_eqshopsel_", 0) == 0) {
        std::string rest = cid.substr(16);
        size_t s1 = rest.find('_');
        if (s1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, s1)));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string mode = rest.substr(s1 + 1);
        std::string cat  = ev.values.empty() ? std::string() : ev.values[0];
        ev.reply(dpp::ir_update_message, make_maple_eqshop_msg(uid, mode, cat, 0));
        return;
    }

    // maple_advbracket_<uid>　值＝選中的等級區間 index
    if (cid.rfind("maple_advbracket_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(17)));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        int bracket = 0;
        try { if (!ev.values.empty()) bracket = std::stoi(ev.values[0]); } catch (...) {}
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            maple_data[uid].adv_last_bracket = bracket;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_adv_region_list_msg(uid, bracket));
        return;
    }
}
