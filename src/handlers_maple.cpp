#include "types.h"
#include "chips.h"
#include "maple.h"
#include "handler_decls.h"

void load_maple_all_data() {
    load_maple_data();
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

void handle_maple_button(const dpp::button_click_t& ev) {
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
        int64_t exp_gain = 0, coin_gain = 0, secs = 0;
        int level_ups = 0;
        std::string region_name;
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
            c.adv_region.clear();
            c.adv_started_at = 0;
            ok = true;
        }
        if (!ok) return;
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_adv_settle_msg(uid, region_name, exp_gain, coin_gain, secs, level_ups));
        return;
    }

    if (cid.rfind("maple_skill_", 0) == 0) {
        if (!check_owner("maple_skill_")) return;
        ev.reply(dpp::ir_update_message, make_maple_skill_msg(uid));
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
                c.adv_atk_skill = skill_key;
            }
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_atktype_msg(uid));
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
            if (maple_sp_unspent(c) > 0 && lvl < sd->max_level)
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

    if (cid.rfind("maple_eqpick_", 0) == 0) {
        std::string rest = cid.substr(13);
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
            if (!maple_meets_requirement(c, *item)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 條件不符，無法裝備！").set_flags(dpp::m_ephemeral)); return;
            }
            maple_set_equipped(c, slot, item_key);
        }
        save_maple_data();
        ev.reply(dpp::ir_update_message, make_maple_equip_slot_msg(uid, slot));
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
