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

// 換下一場活動、或手動提前結束前呼叫：把目前這場（如果真的曾經生效過）存進歷史，
// until 夾到「現在」避免超出實際生效時間；同時順手清掉超過30天的舊紀錄。呼叫前需持有 data_mutex。
void maple_exp_event_archive_current() {
    time_t now = time(nullptr);
    if (maple_exp_event.mult > 1.0 && maple_exp_event.start > 0) {
        time_t until = std::min(maple_exp_event.until, now);
        if (until > maple_exp_event.start)
            maple_exp_event_history.push_back({maple_exp_event.mult, maple_exp_event.start, until});
    }
    const time_t MAX_AGE = 30 * 86400;
    maple_exp_event_history.erase(
        std::remove_if(maple_exp_event_history.begin(), maple_exp_event_history.end(),
                        [&](const MapleExpEventWindow& w) { return w.until < now - MAX_AGE; }),
        maple_exp_event_history.end());
}

std::string maple_admin_give_exp(dpp::snowflake uid, int64_t amount) {
    if (amount == 0) return "";
    int level_ups = 0, new_level = 0;
    int64_t new_exp = 0, deducted = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (amount < 0) {
            // 扣除：只扣目前這一級已累積的經驗，最低到 0，不會降級
            auto it = maple_data.find(uid);
            if (it == maple_data.end()) return "（這位玩家還沒有養成角色，沒有扣除）";
            deducted = std::min(-amount, it->second.exp);
            it->second.exp -= deducted;
            new_level = it->second.level;
            new_exp   = it->second.exp;
        } else {
            auto& c = maple_data[uid];
            level_ups = maple_apply_exp(c, amount); // 內部已經會處理陣營經驗（升每一級 +N），不用再呼叫一次
            new_level = c.level;
            new_exp   = c.exp;
        }
    }
    save_maple_data();
    if (level_ups > 0) save_maple_faction_state();
    std::string msg = "Lv." + std::to_string(new_level) + "，剩餘經驗 " + std::to_string(new_exp);
    if (level_ups > 0) msg += "（升了 **" + std::to_string(level_ups) + "** 級）";
    if (amount < 0) {
        msg = "實際扣除 **" + std::to_string(deducted) + "**，" + msg;
        if (deducted < -amount) msg += "（已扣到 0，不會降級）";
    }
    return msg;
}

// ─── 交易輔助（讓 !交易／/交易 支援楓之谷卷軸、裝備、強化實例與瘋幣）──────────────
// 純裝備（沒點過卷軸）：這裡的 mv_item_info/mv_is_item_key/mv_locked_has_item/mv_locked_transfer_item
// 負責，一樣要放在背包（未穿在身上）才能交易。
// 強化過的裝備（點過卷軸）：另外一套（mv_resolve_enh_item/mv_locked_has_enh_item/
// mv_locked_transfer_enh_item，往下看「強化裝備交易」區塊），因為每件強化實例有自己的加成，
// 不是靠數量計數，需要用交易用臨時ID（見 types.h 的 MV_ENH_TRADE_ID_BASE）指定是哪一件。

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

// 只算背包裡「純裝備」的份數（強化過的裝備走 mv_locked_has_enh_item，不是這個函式管）。
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

// ─── 強化裝備交易 ──────────────────────────────────────────────────────────────
// 共用：這個強化實例目前能不能交易（存在／沒穿在身上／底子本身可交易），呼叫前需持有 data_mutex。
// 可以的話把顯示名稱（含強化徽章與加成文字）填進 name_out，回傳該實例指標；不行回傳 nullptr。
static const MapleEnhItem* mv_enh_tradeable_locked(MapleCharacter& c, int enh_id, std::string* name_out = nullptr) {
    MapleEnhItem* e = maple_find_enh(c, enh_id);
    if (!e) return nullptr;
    if (maple_enh_is_equipped(c, enh_id)) return nullptr; // 穿在身上要先卸下才能交易，跟純裝備規則一致
    const MapleItemDef* def = maple_find_item(e->base_key);
    if (!def || !def->sellable) return nullptr; // 新手木劍等不可交易的底子，強化版也一樣不行
    if (name_out) {
        *name_out = def->name + maple_enh_badge(*e, def->slot);
        if (maple_enh_has_bonus(*e)) *name_out += "（" + maple_enh_bonus_text(*e) + "）";
    }
    return e;
}

bool mv_locked_has_enh_item(dpp::snowflake uid, int enh_id) {
    auto it = maple_data.find(uid);
    if (it == maple_data.end()) return false;
    return mv_enh_tradeable_locked(it->second, enh_id) != nullptr;
}

// 呼叫前必須持有 data_mutex（trade 執行流程在同一個鎖裡面驗證時用這個版本，避免重複上鎖死鎖）
bool mv_locked_resolve_enh_item(dpp::snowflake owner_uid, int enh_id, std::string& key_out, std::string& name_out) {
    auto it = maple_data.find(owner_uid);
    if (it == maple_data.end()) return false;
    std::string name;
    if (!mv_enh_tradeable_locked(it->second, enh_id, &name)) return false;
    key_out  = "mvenh:" + std::to_string(enh_id);
    name_out = name;
    return true;
}
bool mv_resolve_enh_item(dpp::snowflake owner_uid, int enh_id, std::string& key_out, std::string& name_out) {
    std::lock_guard<std::mutex> lk(data_mutex);
    return mv_locked_resolve_enh_item(owner_uid, enh_id, key_out, name_out);
}

// 轉移一件強化實例：從 from 的 enh_items 移除，用 to 的 enh_next_id 重新編號後加進 to 的 enh_items。
// 呼叫前必須持有 data_mutex。呼叫前應該已經用 mv_locked_has_enh_item 確認過存在且未裝備中。
void mv_locked_transfer_enh_item(dpp::snowflake from, dpp::snowflake to, int enh_id) {
    auto fit = maple_data.find(from);
    if (fit == maple_data.end()) return;
    auto& fvec = fit->second.enh_items;
    for (size_t i = 0; i < fvec.size(); i++) {
        if (fvec[i].id != enh_id) continue;
        MapleEnhItem moved = fvec[i];
        fvec.erase(fvec.begin() + i);
        auto& tc = maple_data[to];
        moved.id = tc.enh_next_id++;
        tc.enh_items.push_back(moved);
        break;
    }
}

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
    for (auto& e : it->second.enh_items) {
        std::string name;
        if (!mv_enh_tradeable_locked(it->second, e.id, &name)) continue; // 穿在身上的不會出現在這份清單
        out.push_back({"mvenh:" + std::to_string(e.id), name, mv_enh_trade_id(e.id), 1});
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
            if (!c.raid_room_id.empty() && maple_find_raid_room(c.raid_room_id)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 正在突襲首領房間裡，無法同時出發冒險！").set_flags(dpp::m_ephemeral)); return;
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

    if (cid.rfind("maple_wbrefresh_", 0) == 0) {
        if (!check_owner("maple_wbrefresh_")) return;
        // maple_wbrefresh_<uid>_<page>（舊按鈕沒有頁碼就是第 1 頁）
        int page = 0;
        std::string rest = cid.substr(std::string("maple_wbrefresh_").size());
        size_t psep = rest.find('_');
        if (psep != std::string::npos) { try { page = std::stoi(rest.substr(psep + 1)); } catch (...) {} }
        ev.reply(dpp::ir_update_message, make_maple_wb_region_list_msg(uid, page));
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
            if (c.level < boss->min_level) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 挑戰 " + boss->name + " 需要等級 " + std::to_string(boss->min_level) + " 以上！")
                        .set_flags(dpp::m_ephemeral)); return;
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
            const MapleRaidBossDef* boss = room ? maple_find_raid_boss(room->boss_key) : nullptr;
            if (!room || room->state != "waiting") err = "這間房間已經不能加入了。";
            else if (!c.raid_room_id.empty() && maple_find_raid_room(c.raid_room_id)) err = "你已經在一個房間裡了！";
            else if (maple_is_adventuring(c) || maple_is_wb_fighting(c)) err = "冒險中或挑戰野外首領中無法加入房間！";
            else if ((int)room->members.size() >= MAPLE_RAID_ROOM_MAX_MEMBERS) err = "這間房間已經滿了。";
            else if (boss && c.level < boss->min_level)
                err = "挑戰 " + boss->name + " 需要等級 " + std::to_string(boss->min_level) + " 以上！";
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
                room->checkin_turn = 0; // 輪流指定簽到：從隊長（members[0]）開始輪
                room->accum_dmg = 0.0;
                room->dmg_mult_x10 = 10;
                room->checkin_pending.clear();
                room->guess_active = false;
                room->guess_note.clear();
                const MapleRaidBossDef* boss = maple_find_raid_boss(room->boss_key);
                room->next_guess_secs = (boss && boss->guess_game) ? MAPLE_RAID_GUESS_INTERVAL_SEC : 0;
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

    if (cid.rfind("maple_raidroulette_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_raidroulette_").size());
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_raid_roulette_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidrlfire_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_raidrlfire_").size());
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
            if (room && room->rd_active && room->rd_uid == uid && room->rd_player_turn) {
                MapleRdFireResult res = maple_raid_rd_player_fire_locked(*room);
                if (res == RD_PLAYER_HIT) {
                    maple_raid_rd_handle_player_loss_locked(*room);
                } else if (res == RD_FORCED_PASS) {
                    room->rd_log.push_back("⚠️ 這發規則上不能自己開，強制過");
                    maple_raid_rd_end_player_turn_locked(*room);
                }
                if (!room->rd_active && room->checkin_pending.empty() && room->fullcheckin_pending.empty() && !room->guess_active)
                    room->resume_at = time(nullptr);
            }
        }
        save_maple_raid_rooms();
        ev.reply(dpp::ir_update_message, make_maple_raid_roulette_msg(uid, room_id));
        return;
    }

    if (cid.rfind("maple_raidrlpass_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_raidrlpass_").size());
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
            if (room && room->rd_active && room->rd_uid == uid && room->rd_player_turn && room->rd_shots_this_turn >= 1) {
                maple_raid_rd_end_player_turn_locked(*room);
                if (!room->rd_active && room->checkin_pending.empty() && room->fullcheckin_pending.empty() && !room->guess_active)
                    room->resume_at = time(nullptr);
            }
        }
        save_maple_raid_rooms();
        ev.reply(dpp::ir_update_message, make_maple_raid_roulette_msg(uid, room_id));
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
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            if (!room || room->state != "fighting" || room->resume_at > 0 || !maple_raid_room_has_member(*room, uid)) {
                // 房間不存在／沒在暫停中／不是隊員：靜默忽略，讓畫面刷新即可
            } else if (!maple_raid_can_checkin(*room, maple_find_raid_boss(room->boss_key), uid)) {
                err = "現在還沒輪到你簽到，換其他隊員試試。";
            } else {
                const MapleRaidBossDef* boss = maple_find_raid_boss(room->boss_key);
                bool pool_mode = boss && (boss->checkin_count >= 2 || (boss->checkin_count == 1 && boss->checkin_random));
                bool checkin_done = true; // 輪流制：這個人簽到了這關就過了
                if (pool_mode) {
                    auto& pend = room->checkin_pending;
                    pend.erase(std::remove(pend.begin(), pend.end(), uid), pend.end());
                    checkin_done = pend.empty();
                } else if (boss && boss->checkin_count == 1) {
                    room->checkin_turn = (room->checkin_turn + 1) % (int)room->members.size(); // 輪到下一位
                }
                // 12分鐘全員點名：如果這個人也在待簽到名單裡，一併簽掉（跟5分鐘那關各自獨立）
                auto& fpend = room->fullcheckin_pending;
                fpend.erase(std::remove(fpend.begin(), fpend.end(), uid), fpend.end());
                if (checkin_done && fpend.empty() && !room->guess_active && !room->rd_active) room->resume_at = time(nullptr);
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

    // 猜數字：被指定的人按下後跳出輸入框（結果在 handle_maple_modal 處理）
    if (cid.rfind("maple_raidguess_", 0) == 0) {
        std::string rest = cid.substr(16);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string room_id = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        int tries = 0;
        bool ok = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            MapleRaidRoom* room = maple_find_raid_room(room_id);
            if (room && room->state == "fighting" && room->guess_active && room->guess_uid == uid) {
                ok = true; tries = (int)room->guess_history.size();
            }
        }
        if (!ok) {
            ev.reply(dpp::ir_update_message, make_maple_raid_status_msg(uid, room_id)); return;
        }
        dpp::interaction_modal_response modal("maple_raidguessm_" + std::to_string((uint64_t)uid) + "_" + room_id, "猜數字（幾A幾B）");
        modal.add_component(dpp::component().set_type(dpp::cot_text)
            .set_label("4 個不重複數字（第 " + std::to_string(tries + 1)
                       + "/" + std::to_string(MAPLE_RAID_GUESS_MAX_TRIES) + " 次）")
            .set_id("guess").set_text_style(dpp::text_short)
            .set_min_length(4).set_max_length(4).set_placeholder("例如 0123"));
        ev.dialog(modal);
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
        std::vector<dpp::snowflake> member_uids; // 公告頻道發送失敗時，備援私訊每個隊員用
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
                    for (auto& m : room->members) member_uids.push_back(m.uid);
                    std::mt19937& rng = maple_wb_rng();
                    summary = "## 🏆 " + boss_name + " 討伐成功！\n";
                    for (auto& m : room->members) {
                        auto& c = maple_data[m.uid];
                        maple_raid_week_reset_if_needed(c);
                        c.raid_week_used++;
                        int64_t exp_gain = boss ? boss->exp : 0;
                        int64_t coin_gain = boss ? std::uniform_int_distribution<int64_t>(boss->coin_min, boss->coin_max)(rng) : 0;
                        c.coins += coin_gain;
                        c.monsters_defeated++;
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
                            c.raid_clears[boss->key]++;
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
        if (announce_ch) {
            g_bot->message_create(result, [announce_ch, summary, member_uids](const dpp::confirmation_callback_t& cb) {
                if (cb.is_error()) {
                    auto err = cb.get_error();
                    fprintf(stderr, "[突襲結算] 公告發送失敗 ch=%llu http=%d code=%d msg=%s，改私訊每位隊員\n",
                        (unsigned long long)(uint64_t)announce_ch, cb.http_info.status, err.code, err.message.c_str());
                    fflush(stderr);
                    // 頻道發送失敗（權限／討論串被封存等），改私訊每個隊員，確保獎勵結果不會完全沒人看到
                    for (auto mu : member_uids) {
                        dpp::message dm(summary);
                        g_bot->direct_message_create(mu, dm);
                    }
                }
            });
        }
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
        std::string rest = cid.substr(std::string("maple_factionmembers_").size());
        size_t sep = rest.find('_');
        int page = 0;
        if (sep != std::string::npos) { try { page = std::stoi(rest.substr(sep + 1)); } catch (...) {} }
        ev.reply(dpp::ir_update_message, make_maple_faction_members_msg(uid, page));
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

    if (cid.rfind("maple_factionoverview_", 0) == 0) {
        if (!check_owner("maple_factionoverview_")) return;
        ev.reply(dpp::ir_update_message, make_maple_faction_overview_msg(uid));
        return;
    }

    if (cid.rfind("maple_factioncommander_", 0) == 0) {
        if (!check_owner("maple_factioncommander_")) return;
        std::string rest = cid.substr(std::string("maple_factioncommander_").size());
        size_t sep = rest.find('_');
        int page = 0;
        if (sep != std::string::npos) { try { page = std::stoi(rest.substr(sep + 1)); } catch (...) {} }
        ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, page));
        return;
    }

    if (cid.rfind("maple_factionvote_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_factionvote_").size());
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        dpp::snowflake target(std::stoull(rest.substr(sep + 1)));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            auto tit = maple_data.find(target);
            if (c.faction_key.empty()) err = "你目前沒有加入任何陣營！";
            else if (tit == maple_data.end() || tit->second.faction_key != c.faction_key) err = "對方不是你陣營裡的成員。";
            else c.faction_vote_for = target;
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "❌ " + err));
            return;
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "✅ 已投票"));
        return;
    }

    if (cid.rfind("maple_factionptsadd_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_factionptsadd_").size());
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string type = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.faction_key.empty()) err = "你目前沒有加入任何陣營！";
            else if (maple_faction_commander_locked(c.faction_key) != uid) err = "只有統帥可以分配陣營技能點。";
            else {
                MapleFactionState& st = maple_faction_state[c.faction_key];
                if (maple_faction_unspent_pts(st) <= 0) err = "沒有剩餘的陣營技能點了。";
                else if (type == "atk") st.atk_pts++;
                else if (type == "exp") st.exp_pts++;
                else if (type == "coin") st.coin_pts++;
                else err = "未知的類型。";
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "❌ " + err));
            return;
        }
        save_maple_faction_state();
        ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "✅ 已分配 1 點"));
        return;
    }

    if (cid.rfind("maple_factionptssub_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_factionptssub_").size());
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string type = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.faction_key.empty()) err = "你目前沒有加入任何陣營！";
            else if (maple_faction_commander_locked(c.faction_key) != uid) err = "只有統帥可以分配陣營技能點。";
            else {
                MapleFactionState& st = maple_faction_state[c.faction_key];
                int* pts = type == "atk" ? &st.atk_pts : type == "exp" ? &st.exp_pts : type == "coin" ? &st.coin_pts : nullptr;
                if (!pts) err = "未知的類型。";
                else if (*pts <= 0) err = "這一項已經是 0 點了。";
                else (*pts)--;
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "❌ " + err));
            return;
        }
        save_maple_faction_state();
        ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "✅ 已收回 1 點"));
        return;
    }

    // 統帥輸入分配：跳出輸入框，三項直接填數字（預填目前的分配）
    if (cid.rfind("maple_factionptsinput_", 0) == 0) {
        if (!check_owner("maple_factionptsinput_")) return;
        MapleFactionState st; bool ok = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!c.faction_key.empty() && maple_faction_commander_locked(c.faction_key) == uid) {
                st = maple_faction_state_of(c.faction_key); ok = true;
            }
        }
        if (!ok) {
            ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "❌ 只有統帥可以分配陣營技能點。"));
            return;
        }
        dpp::interaction_modal_response modal("maple_factionptsm_" + std::to_string((uint64_t)uid),
                                              "分配陣營技能點（共 " + std::to_string(st.level) + " 點）");
        // Discord 規定一行只能放一個輸入框，所以每個欄位之間換行（最後一個後面不能再多一個空行）
        auto field = [&](const std::string& id, const std::string& label, int cur) {
            modal.add_component(dpp::component().set_type(dpp::cot_text)
                .set_label(label).set_id(id).set_text_style(dpp::text_short)
                .set_min_length(1).set_max_length(4).set_default_value(std::to_string(cur)));
        };
        field("atk",  "⚔️ 武器攻擊力（每點 +1）", st.atk_pts);
        modal.add_row();
        field("exp",  "📈 經驗加成（每點 +1%）",  st.exp_pts);
        modal.add_row();
        field("coin", "🪙 瘋幣加成（每點 +1%）",  st.coin_pts);
        ev.dialog(modal);
        return;
    }

    if (cid.rfind("maple_factionptsreset_", 0) == 0) {
        if (!check_owner("maple_factionptsreset_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.faction_key.empty()) err = "你目前沒有加入任何陣營！";
            else if (maple_faction_commander_locked(c.faction_key) != uid) err = "只有統帥可以分配陣營技能點。";
            else {
                MapleFactionState& st = maple_faction_state[c.faction_key];
                st.atk_pts = 0; st.exp_pts = 0; st.coin_pts = 0;
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "❌ " + err));
            return;
        }
        save_maple_faction_state();
        ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "✅ 已重置陣營技能點"));
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
            if (!c.raid_room_id.empty() && maple_find_raid_room(c.raid_room_id)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 正在突襲首領房間裡，無法挑戰野外首領！").set_flags(dpp::m_ephemeral)); return;
            }
            c.wb_region        = region_key;
            c.wb_started_at    = now;
            c.wb_required_secs = maple_wb_kill_secs(c, *region);
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
        int max_winners = region ? region->max_winners : MAPLE_WB_MAX_WINNERS;
        bool win = false;
        int place = 0; // 本輪第幾位擊殺成功（1~max_winners）
        int64_t exp_gain = 0, coin_gain = 0;
        int level_ups = 0;
        std::vector<std::string> drops;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_is_wb_fighting(c)) { ev.reply(dpp::ir_update_message, make_maple_wb_region_list_msg(uid)); return; }
            auto& st = maple_wb_state[c.wb_region];
            // 這段期間本輪還沒關閉（沒人搶先湊滿名額）、而且本輪名額還沒滿，才算擊殺成功
            win = (st.dead_since == c.wb_epoch) && (st.round_kills < max_winners);
            if (win && region) {
                // 注意：這裡一定要用 std::mt19937+random_device，不能用裸的 rand()——
                // rand() 如果沒在「這條執行緒」上呼叫過 srand()，會從同一組固定序列開始跑，
                // 導致不同玩家/不同次結算全部拿到一模一樣的「隨機」結果（之前掉落物重複就是這個原因）。
                std::mt19937& wb_rng = maple_wb_rng();
                place = ++st.round_kills;
                if (st.round_kills >= max_winners) {
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
        ev.reply(dpp::ir_update_message, make_maple_wb_result_msg(uid, win, boss_name, exp_gain, coin_gain, level_ups, drops, place, max_winners));
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

    // 背包「卷軸」分頁翻頁：maple_bagscrollpg_<uid>_<page>_<subcat>
    if (cid.rfind("maple_bagscrollpg_", 0) == 0) {
        std::string rest = cid.substr(18);
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
        ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "scroll", page, subcat));
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
        std::string err, scroll_subcat;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            maple_sell_execute_locked(c, kind, ref, 1, err, scroll_subcat);
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        if (kind == "scroll") ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "scroll", 0, scroll_subcat));
        else ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "other"));
        return;
    }

    // maple_sellqty_<uid>_<kind>_<qty>_<ref>：賣出固定數量（item／scroll 可堆疊）
    if (cid.rfind("maple_sellqty_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_sellqty_").size());
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string after_uid = rest.substr(sep1 + 1);
        size_t sep2 = after_uid.find('_');
        if (sep2 == std::string::npos) return;
        std::string kind = after_uid.substr(0, sep2);
        std::string after_kind = after_uid.substr(sep2 + 1);
        size_t sep3 = after_kind.find('_');
        if (sep3 == std::string::npos) return;
        int64_t qty = std::atoll(after_kind.substr(0, sep3).c_str());
        std::string ref = after_kind.substr(sep3 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err, scroll_subcat;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            maple_sell_execute_locked(c, kind, ref, qty, err, scroll_subcat);
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        if (kind == "scroll") ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "scroll", 0, scroll_subcat));
        else ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "other"));
        return;
    }

    // maple_sellmax_<uid>_<kind>_<ref>：賣出目前持有的全部數量（item／scroll 可堆疊）
    if (cid.rfind("maple_sellmax_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_sellmax_").size());
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
        std::string err, scroll_subcat;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            maple_sell_execute_locked(c, kind, ref, (int64_t)999999, err, scroll_subcat);
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
        }
        save_maple_data();
        if (kind == "scroll") ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "scroll", 0, scroll_subcat));
        else ev.reply(dpp::ir_update_message, make_maple_bag_msg(uid, "other"));
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

    // maple_tokenexmax_<uid>：顯示兌換上限二階段確認視窗（不直接執行）
    if (cid.rfind("maple_tokenexmax_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(17)));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_tokenexmax_confirm_msg(uid));
        return;
    }

    // maple_tokenexmaxok_<uid>：確認後真正執行兌換上限（金額伺服器端算）／maple_tokenex_<uid>_<amount>：固定金額
    if (cid.rfind("maple_tokenexmaxok_", 0) == 0 || cid.rfind("maple_tokenex_", 0) == 0) {
        bool is_max = cid.rfind("maple_tokenexmaxok_", 0) == 0;
        dpp::snowflake owner;
        int64_t amount = 0;
        if (is_max) {
            owner = dpp::snowflake(std::stoull(cid.substr(19)));
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

    // maple_coinexmax_<uid>：顯示兌換上限二階段確認視窗（不直接執行）
    if (cid.rfind("maple_coinexmax_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(16)));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_coinexmax_confirm_msg(uid));
        return;
    }

    // maple_coinexmaxok_<uid>：確認後真正執行瘋幣換籌碼上限（金額伺服器端算）／maple_coinex_<uid>_<chips>：固定籌碼數
    if (cid.rfind("maple_coinexmaxok_", 0) == 0 || cid.rfind("maple_coinex_", 0) == 0) {
        bool is_max = cid.rfind("maple_coinexmaxok_", 0) == 0;
        dpp::snowflake owner;
        int64_t chips_wanted = 0;
        if (is_max) {
            owner = dpp::snowflake(std::stoull(cid.substr(18)));
        } else {
            std::string rest = cid.substr(13);
            size_t sep = rest.rfind('_');
            if (sep == std::string::npos) return;
            owner       = dpp::snowflake(std::stoull(rest.substr(0, sep)));
            chips_wanted = std::atoll(rest.substr(sep + 1).c_str());
        }
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (is_max) chips_wanted = c.coins / MAPLE_COIN_TO_CHIP_RATE;
            int64_t cost = maple_coins_for_chips(chips_wanted);
            if (chips_wanted <= 0) err = "沒有可兌換的瘋幣。";
            else if (cost > c.coins) err = "瘋幣不足。";
            else {
                c.coins -= cost;
                chip_data[uid].chips += chips_wanted;
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

    // maple_eqbuymax_<uid>_<key>：顯示買到上限二階段確認視窗（不直接執行）
    if (cid.rfind("maple_eqbuymax_", 0) == 0) {
        std::string rest = cid.substr(15);
        size_t s1 = rest.find('_');
        if (s1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, s1)));
        std::string item_key = rest.substr(s1 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_eqbuymax_confirm_msg(uid, item_key));
        return;
    }

    // maple_eqbuyok_<uid>_<qty>_<key>：固定數量／maple_eqbuymaxok_<uid>_<key>：確認後真正買到上限（數量伺服器端算）
    // 相容舊格式 maple_eqbuyok_<uid>_<key>（舊訊息上的按鈕）：數量欄位不是純數字時當作買 1 件
    if (cid.rfind("maple_eqbuyok_", 0) == 0 || cid.rfind("maple_eqbuymaxok_", 0) == 0) {
        bool is_max = cid.rfind("maple_eqbuymaxok_", 0) == 0;
        std::string rest = cid.substr(is_max ? 17 : 14);
        size_t s1 = rest.find('_');
        if (s1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, s1)));
        std::string item_key = rest.substr(s1 + 1);
        int64_t qty = 1;
        if (!is_max) {
            size_t s2 = item_key.find('_');
            if (s2 != std::string::npos && s2 > 0 &&
                item_key.substr(0, s2).find_first_not_of("0123456789") == std::string::npos) {
                qty = std::atoll(item_key.substr(0, s2).c_str());
                item_key = item_key.substr(s2 + 1);
            }
        }
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        const MapleItemDef* it = maple_find_item(item_key);
        if (!it || it->price <= 0 || !it->shop) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (is_max) qty = std::min((int64_t)999, c.coins / it->price);
            if (qty > 999) qty = 999;
            int64_t cost = qty * it->price;
            if (qty <= 0)          err = "數量不對。";
            else if (c.coins < cost) err = "瘋幣不足！";
            else {
                c.coins -= cost;
                c.equipment[item_key] += (int)qty;
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ " + err).set_flags(dpp::m_ephemeral)); return;
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

    // maple_scbuymax_<uid>_<key>：顯示買到上限二階段確認視窗（不直接執行）
    if (cid.rfind("maple_scbuymax_", 0) == 0) {
        std::string rest = cid.substr(15);          // <uid>_<key>
        size_t s1 = rest.find('_');
        if (s1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, s1)));
        std::string scroll_key = rest.substr(s1 + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_scbuymax_confirm_msg(uid, scroll_key));
        return;
    }

    // maple_scbuyok_<uid>_<key>_<qty>：固定數量／maple_scbuymaxok_<uid>_<key>：確認後真正買到上限（數量伺服器端算）
    if (cid.rfind("maple_scbuyok_", 0) == 0 || cid.rfind("maple_scbuymaxok_", 0) == 0) {
        bool is_max = cid.rfind("maple_scbuymaxok_", 0) == 0;
        dpp::snowflake owner;
        std::string scroll_key;
        int64_t qty = 0;
        if (is_max) {
            std::string rest = cid.substr(17);          // <uid>_<key>
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

    // 特殊商店：付費技能點數重製（10000 瘋幣，可重複）
    if (cid.rfind("maple_spbuyresetok_", 0) == 0) {
        if (!check_owner("maple_spbuyresetok_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.coins < MAPLE_SP_BUYRESET_COST) err = "瘋幣不足！";
            else {
                c.coins -= MAPLE_SP_BUYRESET_COST;
                c.skill_levels.clear();
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
    if (cid.rfind("maple_spbuyreset_", 0) == 0) {
        if (!check_owner("maple_spbuyreset_")) return;
        if (maple_get_or_create(uid).coins < MAPLE_SP_BUYRESET_COST) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 瘋幣不足！").set_flags(dpp::m_ephemeral)); return;
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
        ev.reply(dpp::ir_update_message, make_maple_skill_msg(uid, std::to_string(maple_skill_tab_of(*sd))));
        return;
    }

    if (cid.rfind("maple_skmax_", 0) == 0) {
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
            if (maple_skill_visible(c, sd->job) && maple_skill_unlockable(c, *sd)) {
                int lvl = maple_skill_level(c, skill_key);
                while (maple_sp_unspent(c) > 0 && lvl < sd->max_level) { lvl++; c.skill_levels[skill_key] = lvl; }
            }
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_skill_msg(uid, std::to_string(maple_skill_tab_of(*sd))));
        return;
    }

    if (cid.rfind("maple_eqopen_", 0) == 0) {
        std::string rest = cid.substr(13);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string tail = rest.substr(sep + 1);
        std::string slot = tail;
        int page = 0;
        size_t psep = tail.rfind('_');
        if (psep != std::string::npos) {
            try { page = std::stoi(tail.substr(psep + 1)); slot = tail.substr(0, psep); } catch (...) {}
        }
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_equip_slot_msg(uid, slot, page));
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
        if (!item || item->slot != maple_slot_item_kind(slot)) return;
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
            if (!maple_meets_requirement(c, *item, slot)) {
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
            if (slot == "weapon") maple_auto_unequip_invalid_offhand_locked(c);
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
            if (!item || item->slot != maple_slot_item_kind(slot)) return;
            if (maple_enh_is_equipped(c, enh_id)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 已經裝備這個了！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!maple_meets_requirement(c, *item, slot)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 條件不符，無法裝備！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string old_raw = maple_equipped_raw(c, slot);
            if (!maple_eq_is_enh(old_raw) && !old_raw.empty() && old_raw != "wooden_sword")
                c.equipment[old_raw]++;
            maple_set_equipped(c, slot, "#" + std::to_string(enh_id));
            if (slot == "weapon") maple_auto_unequip_invalid_offhand_locked(c);
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
            if (slot == "weapon") maple_auto_unequip_invalid_offhand_locked(c);
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_equip_slot_msg(uid, slot));
        return;
    }

    if (cid.rfind("maple_potential_", 0) == 0) {
        if (!check_owner("maple_potential_")) return;
        MapleCharacter c0 = maple_get_or_create(uid);
        if (maple_is_adventuring(c0)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 冒險中無法洗潛能！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_potential_pick_msg(uid));
        return;
    }

    if (cid.rfind("maple_potentialopen_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_potentialopen_").size());
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string slot = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_potential_confirm_msg(uid, slot));
        return;
    }

    if (cid.rfind("maple_potentialok_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_potentialok_").size());
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
        std::string slot = rest.substr(sep + 1);
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string result;
        bool ok = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c)) {
                result = "❌ 冒險中無法洗潛能！";
            } else {
                result = maple_potential_reroll(c, slot, ok);
                if (!ok) result = "❌ " + result;
            }
        }
        if (ok) save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_potential_confirm_msg(uid, slot, result));
        return;
    }

    // maple_potentiallock_<uid>_<slot>_<line>：切換第line行（1/2/3）的鎖定狀態，最多鎖2行
    if (cid.rfind("maple_potentiallock_", 0) == 0) {
        std::string rest = cid.substr(std::string("maple_potentiallock_").size());
        size_t sep1 = rest.find('_');
        if (sep1 == std::string::npos) return;
        dpp::snowflake owner(std::stoull(rest.substr(0, sep1)));
        std::string after_uid = rest.substr(sep1 + 1);
        size_t sep2 = after_uid.rfind('_');
        if (sep2 == std::string::npos) return;
        std::string slot = after_uid.substr(0, sep2);
        int line = std::atoi(after_uid.substr(sep2 + 1).c_str());
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            std::string raw = maple_equipped_raw(c, slot);
            if (maple_eq_is_enh(raw)) {
                MapleEnhItem* e = maple_find_enh(c, maple_eq_enh_id(raw));
                if (e) {
                    bool* locked_f = line == 1 ? &e->pot1_locked : line == 2 ? &e->pot2_locked : line == 3 ? &e->pot3_locked : nullptr;
                    int   line_type = line == 1 ? e->pot1_type : line == 2 ? e->pot2_type : line == 3 ? e->pot3_type : -1;
                    if (locked_f && line_type >= 0) {
                        if (*locked_f) *locked_f = false;
                        else if (maple_pot_locked_count(*e) < 2) *locked_f = true;
                    }
                }
            }
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_potential_confirm_msg(uid, slot));
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
        // 結尾 _eq 標記是從「更換裝備」頁面進來的，強化頁的「返回」要回那裡
        bool from_equip = false;
        if (slot.size() > 3 && slot.compare(slot.size() - 3, 3, "_eq") == 0) {
            from_equip = true;
            slot = slot.substr(0, slot.size() - 3);
        }
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_enh_msg(uid, slot, "", from_equip));
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
        std::string tail = rest2.substr(sep2 + 1);
        bool from_equip = false;
        if (tail.rfind("eq_", 0) == 0) { from_equip = true; tail = tail.substr(3); }
        std::string scroll_key = tail;
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
        ev.reply(dpp::ir_update_message, make_maple_enh_msg(uid, slot, result, from_equip));
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

    // 特殊商店：付費能力值重製（10000 瘋幣，可重複）
    if (cid.rfind("maple_apbuyresetok_", 0) == 0) {
        if (!check_owner("maple_apbuyresetok_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (maple_is_adventuring(c))            err = "冒險中無法調整能力值！";
            else if (c.coins < MAPLE_AP_BUYRESET_COST) err = "瘋幣不足！";
            else {
                c.coins -= MAPLE_AP_BUYRESET_COST;
                c.str_stat = 4; c.dex_stat = 4; c.int_stat = 4; c.luk_stat = 4;
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
    if (cid.rfind("maple_raidbuyattempt_", 0) == 0) {
        if (!check_owner("maple_raidbuyattempt_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            maple_raid_week_reset_if_needed(c);
            if (c.raid_week_extra >= MAPLE_RAID_EXTRA_ATTEMPT_MAX_BUYS) err = "這週的額外次數已經買滿了！";
            else if (c.coins < MAPLE_RAID_EXTRA_ATTEMPT_PRICE) err = "瘋幣不足！";
            else {
                c.coins -= MAPLE_RAID_EXTRA_ATTEMPT_PRICE;
                c.raid_week_extra += 1;
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

    if (cid.rfind("maple_wolftokenex_", 0) == 0) {
        if (!check_owner("maple_wolftokenex_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            auto it = c.equipment.find("mat_wolf_king_token");
            if (it == c.equipment.end() || it->second <= 0) err = "你沒有狼王象徵。";
            else {
                it->second--;
                if (it->second <= 0) c.equipment.erase(it);
                c.equipment["potential_cube"] += MAPLE_WOLF_TOKEN_CUBE_RATE;
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

    if (cid.rfind("maple_empresstokenex_", 0) == 0) {
        if (!check_owner("maple_empresstokenex_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            auto it = c.equipment.find("mat_empress_token");
            if (it == c.equipment.end() || it->second <= 0) err = "你沒有女皇象徵。";
            else {
                it->second--;
                if (it->second <= 0) c.equipment.erase(it);
                c.equipment["potential_cube"] += MAPLE_EMPRESS_TOKEN_CUBE_RATE;
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

    if (cid.rfind("maple_flametokenex_", 0) == 0) {
        if (!check_owner("maple_flametokenex_")) return;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            auto it = c.equipment.find("mat_flame_demon_token");
            if (it == c.equipment.end() || it->second <= 0) err = "你沒有炎魔象徵。";
            else {
                it->second--;
                if (it->second <= 0) c.equipment.erase(it);
                c.equipment["potential_cube"] += MAPLE_FLAME_TOKEN_CUBE_RATE;
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
        if (c.coins < MAPLE_AP_BUYRESET_COST) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 瘋幣不足！").set_flags(dpp::m_ephemeral)); return;
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

    if (cid.rfind("maple_j3open_", 0) == 0) {
        if (!check_owner("maple_j3open_")) return;
        MapleCharacter c = maple_get_or_create(uid);
        if (!maple_can_third_job(c)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 還不能三轉！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, make_maple_job3_select_msg(uid));
        return;
    }

    if (cid.rfind("maple_j3pick_", 0) == 0) {
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
        if (!jd || jd->tier != 3) return;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (!maple_can_third_job(c) || jd->parent != c.job) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 還不能三轉！").set_flags(dpp::m_ephemeral)); return;
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

    // maple_skillsel_<uid>　值＝選中的轉職層級（"0"/"1"/"2"）
    if (cid.rfind("maple_skillsel_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(std::string("maple_skillsel_").size())));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string tab = ev.values.empty() ? std::string() : ev.values[0];
        ev.reply(dpp::ir_update_message, make_maple_skill_msg(uid, tab));
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

// ─── 輸入框（目前只有突襲首領猜數字）─────────────────────────────────────────
void handle_maple_modal(const dpp::form_submit_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;

    // 統帥輸入分配陣營技能點：maple_factionptsm_<uid>，欄位 atk / exp / coin
    if (cid.rfind("maple_factionptsm_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(std::string("maple_factionptsm_").size())));
        if (owner != uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
        }
        std::map<std::string, std::string> vals;
        for (auto& row : ev.components) {
            if (std::holds_alternative<std::string>(row.value)) vals[row.custom_id] = std::get<std::string>(row.value);
            for (auto& sub : row.components)
                if (std::holds_alternative<std::string>(sub.value)) vals[sub.custom_id] = std::get<std::string>(sub.value);
        }
        auto parse = [&](const std::string& k, int& out) -> bool {
            auto it = vals.find(k);
            if (it == vals.end()) return false;
            try { size_t pos = 0; out = std::stoi(it->second, &pos); return pos == it->second.size() && out >= 0; }
            catch (...) { return false; }
        };
        int atk = 0, exp = 0, coin = 0;
        std::string err;
        if (!parse("atk", atk) || !parse("exp", exp) || !parse("coin", coin)) err = "請在三格都填 0 以上的整數。";
        else {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& c = maple_data[uid];
            if (c.faction_key.empty()) err = "你目前沒有加入任何陣營！";
            else if (maple_faction_commander_locked(c.faction_key) != uid) err = "只有統帥可以分配陣營技能點。";
            else {
                MapleFactionState& st = maple_faction_state[c.faction_key];
                if ((int64_t)atk + exp + coin > st.level)
                    err = "總共只有 " + std::to_string(st.level) + " 點，你填了 " + std::to_string(atk + exp + coin) + " 點。";
                else { st.atk_pts = atk; st.exp_pts = exp; st.coin_pts = coin; }
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "❌ " + err));
            return;
        }
        save_maple_faction_state();
        ev.reply(dpp::ir_update_message, make_maple_faction_commander_msg(uid, 0, "✅ 已更新陣營技能點分配"));
        return;
    }

    if (cid.rfind("maple_raidguessm_", 0) != 0) return;
    std::string rest = cid.substr(17);
    size_t sep = rest.find('_');
    if (sep == std::string::npos) return;
    dpp::snowflake owner(std::stoull(rest.substr(0, sep)));
    std::string room_id = rest.substr(sep + 1);
    if (owner != uid) {
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 這不是你的角色！").set_flags(dpp::m_ephemeral)); return;
    }
    std::string input;
    for (auto& row : ev.components) {
        if (std::holds_alternative<std::string>(row.value)) input = std::get<std::string>(row.value);
        for (auto& sub : row.components)
            if (std::holds_alternative<std::string>(sub.value)) input = std::get<std::string>(sub.value);
    }
    input.erase(std::remove_if(input.begin(), input.end(), [](unsigned char ch){ return std::isspace(ch); }), input.end());
    if (!guess_valid(input)) {
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 請輸入 4 個不重複的數字，例如 `0123`（這次不算次數）").set_flags(dpp::m_ephemeral)); return;
    }

    bool failed = false;
    std::string fail_text;
    dpp::snowflake announce_ch = 0;
    std::vector<dpp::snowflake> member_uids;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        MapleRaidRoom* room = maple_find_raid_room(room_id);
        if (room && maple_raid_apply_guess_locked(*room, uid, input, time(nullptr)) == MRG_FAILED) {
            const MapleRaidBossDef* boss = maple_find_raid_boss(room->boss_key);
            failed = true;
            announce_ch = room->channel_id;
            fail_text = "## 💥 突襲失敗 — " + (boss ? boss->name : room->boss_key) + "\n<@"
                      + std::to_string((uint64_t)uid) + "> " + std::to_string(MAPLE_RAID_GUESS_MAX_TRIES)
                      + " 次都沒有猜中（答案是 `" + room->guess_secret + "`），隊伍被擊退了。\n"
                      + "本次不消耗突襲次數，可以重新組隊挑戰。\n隊員：";
            for (auto& m : room->members) {
                member_uids.push_back(m.uid);
                fail_text += m.display_name + " ";
                auto it = maple_data.find(m.uid);
                if (it != maple_data.end()) it->second.raid_room_id.clear();
            }
            maple_raid_rooms.erase(room_id);
        }
    }
    save_maple_raid_rooms();
    if (!failed) {
        ev.reply(dpp::ir_update_message, make_maple_raid_status_msg(uid, room_id));
        return;
    }
    save_maple_data();
    ev.reply(dpp::ir_update_message, make_maple_ambush_msg(uid));
    auto dm_all = [fail_text, member_uids]() {
        for (auto mu : member_uids) g_bot->direct_message_create(mu, dpp::message(fail_text));
    };
    if (!announce_ch) { dm_all(); return; }
    dpp::message msg(announce_ch, fail_text);
    g_bot->message_create(msg, [dm_all](const dpp::confirmation_callback_t& cb) { if (cb.is_error()) dm_all(); });
}
