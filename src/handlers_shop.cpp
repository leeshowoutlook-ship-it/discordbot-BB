#include "types.h"
#include "chips.h"
#include "helpers.h"
#include "pet.h"
#include "shop.h"
#include "adventure.h"
#include "handler_decls.h"

// ─── Shop / Gacha / Equip / Bag button handler ────────────────────────────────

void handle_shop_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    const dpp::user&   user = ev.command.get_issuing_user();
    dpp::snowflake     uid  = user.id;

    // ── 商店按鈕 ─────────────────────────────────────────────────────────────
    if (cid.rfind("shop_", 0) == 0) {
        if (!page_is_mine(ev.command.message_id, uid)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的頁面！").set_flags(dpp::m_ephemeral)); return;
        }
        if (cid == "shop_main") {
            ev.reply(dpp::ir_update_message, make_shop_main_msg(std::to_string((uint64_t)uid)));
        } else if (cid == "shop_virtual" || cid == "shop_vback") {
            ev.reply(dpp::ir_update_message, make_virtual_shop_msg());
        } else if (cid.rfind("shop_vcat_", 0) == 0) {
            ev.reply(dpp::ir_update_message, make_vcat_shop_msg(uid, cid.substr(10)));
        } else if (cid.rfind("shop_vbuy_", 0) == 0) {
            ev.reply(dpp::ir_update_message, make_vbuy_confirm_msg(uid, cid.substr(10)));
        } else if (cid.rfind("shop_vconfirm_", 0) == 0) {
            ev.reply(dpp::ir_update_message, handle_vbuy(uid, user.username, cid.substr(14)));
        } else if (cid.rfind("shop_maple_", 0) == 0) {
            int page = std::stoi(cid.substr(11));
            ev.reply(dpp::ir_update_message, make_maple_shop_msg(page));
        } else if (cid.rfind("shop_buy_", 0) == 0) {
            int idx = std::stoi(cid.substr(9));
            ev.reply(dpp::ir_update_message, make_buy_confirm_msg(uid, idx));
        } else if (cid.rfind("shop_confirm_", 0) == 0) {
            int idx = std::stoi(cid.substr(13));
            ev.reply(dpp::ir_update_message, handle_buy(uid, user.username, idx));
        }
        return;
    }

    // ── 轉蛋按鈕 ─────────────────────────────────────────────────────────────
    if (cid.rfind("gacha_", 0) == 0) {
        auto get_btn_uid = [&](const std::string& s) -> dpp::snowflake {
            size_t p = s.rfind('_');
            if (p == std::string::npos) return 0;
            return dpp::snowflake(std::stoull(s.substr(p+1)));
        };
        dpp::snowflake btn_uid = get_btn_uid(cid);
        if (uid != btn_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string dn = user.global_name.empty() ? user.username : user.global_name;
        std::string av = user.get_avatar_url();

        if (cid.rfind("gacha_main_", 0) == 0) {
            ev.reply(dpp::ir_update_message, make_gacha_main_msg(uid, dn, av));
        } else if (cid.rfind("gacha_banner_normal_", 0) == 0) {
            ev.reply(dpp::ir_update_message, make_gacha_banner_msg(uid, false, dn, av));
        } else if (cid.rfind("gacha_banner_star_", 0) == 0) {
            ev.reply(dpp::ir_update_message, make_gacha_banner_msg(uid, true, dn, av));
        } else if (cid.rfind("gacha_norm_", 0) == 0) {
            std::string mid = cid.substr(11);
            size_t sep = mid.find('_');
            int count = std::stoi(mid.substr(0, sep));
            int cost = count * 50;
            if (get_chips(uid) < cost) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 籌碼不足！需要 " + std::to_string(cost) + " 碼").set_flags(dpp::m_ephemeral)); return;
            }
            add_chips(uid, -(int64_t)cost);
            std::vector<const GachaItem*> pulls;
            for (int i = 0; i < count; i++) pulls.push_back(&gacha_pull_one(false));

            int pity_after = 0; bool pity_fired = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                gacha_pity_data[(uint64_t)uid] += count;
                if (gacha_pity_data[(uint64_t)uid] >= 200) {
                    gacha_pity_data[(uint64_t)uid] -= 200;
                    pity_fired = true;
                }
                pity_after = gacha_pity_data[(uint64_t)uid];
            }
            if (pity_fired) pulls.push_back(&gacha_pull_ur_pity());
            save_chips(); save_inventory(); save_gacha_pity();
            ev.reply(dpp::ir_update_message, make_gacha_result_msg(uid, pulls, false, dn, av, pity_after, pity_fired));
        } else if (cid.rfind("gacha_star_", 0) == 0) {
            std::string mid = cid.substr(11);
            size_t sep = mid.find('_');
            int count = std::stoi(mid.substr(0, sep));
            int cur_stars = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = inventory_data.find(uid);
                if (it != inventory_data.end()) {
                    auto sit = it->second.find("star_unknown");
                    if (sit != it->second.end()) cur_stars = sit->second;
                }
            }
            if (cur_stars < count) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 星星不足！需要 " + std::to_string(count) + " 顆").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid]["star_unknown"] -= count;
            }
            std::vector<const GachaItem*> pulls;
            for (int i = 0; i < count; i++) pulls.push_back(&gacha_pull_one(true));
            save_inventory();
            ev.reply(dpp::ir_update_message, make_gacha_result_msg(uid, pulls, true, dn, av));
        }
        return;
    }

    // ── 裝備按鈕 ─────────────────────────────────────────────────────────────
    if (cid.rfind("equip_", 0) == 0) {
        std::string dn = user.global_name.empty() ? user.username : user.global_name;
        std::string av = user.get_avatar_url();
        Pet pet;
        { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) pet = it->second; }

        if (cid.rfind("equip_main_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(11)));
            if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
            ev.reply(dpp::ir_update_message, make_equip_msg(uid, pet, dn, av));
        } else if (cid.rfind("equip_slot_", 0) == 0) {
            std::string rest = cid.substr(11);
            size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
            dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
            if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
            std::string rest2 = rest.substr(s1+1);
            size_t s2 = rest2.find('_');
            std::string slot = (s2 == std::string::npos) ? rest2 : rest2.substr(0, s2);
            int page = (s2 == std::string::npos) ? 0 : std::stoi(rest2.substr(s2+1));
            ev.reply(dpp::ir_update_message, make_equip_slot_msg(uid, slot, dn, av, page));
        } else if (cid.rfind("equip_set_", 0) == 0) {
            std::string rest = cid.substr(10);
            size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
            dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
            if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
            std::string eq_key = rest.substr(s1+1);
            auto* gi = find_gacha_item(eq_key);
            if (!gi) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 裝備不存在！").set_flags(dpp::m_ephemeral)); return; }
            bool has_item = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = inventory_data.find(uid);
                has_item = it != inventory_data.end() && it->second.count(eq_key) && it->second.at(eq_key) > 0;
                if (has_item) {
                    auto& eq = equipped_data[uid];
                    if      (gi->slot == "W") eq.weapon  = eq_key;
                    else if (gi->slot == "G") eq.glove   = eq_key;
                    else if (gi->slot == "C") eq.clothes = eq_key;
                    else if (gi->slot == "S") eq.shoes   = eq_key;
                    else if (gi->slot == "K") eq.orb     = eq_key;
                }
            }
            if (!has_item) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 背包中沒有此裝備！").set_flags(dpp::m_ephemeral)); return; }
            save_equipped();
            ev.reply(dpp::ir_update_message, make_equip_slot_msg(uid, gi->slot, dn, av));
        } else if (cid.rfind("equip_unequip_", 0) == 0) {
            std::string rest = cid.substr(14);
            size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
            dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
            if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
            std::string slot = rest.substr(s1+1);
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& eq = equipped_data[uid];
                if      (slot == "W") eq.weapon  = "";
                else if (slot == "G") eq.glove   = "";
                else if (slot == "C") eq.clothes = "";
                else if (slot == "S") eq.shoes   = "";
                else if (slot == "K") eq.orb     = "";
            }
            save_equipped();
            ev.reply(dpp::ir_update_message, make_equip_slot_msg(uid, slot, dn, av));
        }
        return;
    }

    // ── 背包按鈕 ─────────────────────────────────────────────────────────────
    if (cid.rfind("bag_", 0) == 0) {
        auto bag_uid = [&](size_t pfx) -> dpp::snowflake {
            std::string rest = cid.substr(pfx);
            size_t sep = rest.find('_');
            return dpp::snowflake(std::stoull(sep == std::string::npos ? rest : rest.substr(0, sep)));
        };
        auto chk = [&](dpp::snowflake bu) -> bool {
            if (uid != bu) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的背包！").set_flags(dpp::m_ephemeral));
                return false;
            }
            return true;
        };

        if (cid.rfind("bag_home_", 0) == 0) {
            if (!chk(dpp::snowflake(std::stoull(cid.substr(9))))) return;
            std::string dn = ev.command.member.get_nickname().empty()
                ? (user.global_name.empty() ? user.username : user.global_name)
                : ev.command.member.get_nickname();
            std::string av = user.get_avatar_url();
            ev.reply(dpp::ir_update_message, make_bag_home_msg(uid, dn, av));

        } else if (cid.rfind("bag_tab_equip_", 0) == 0) {
            if (!chk(dpp::snowflake(std::stoull(cid.substr(14))))) return;
            ev.reply(dpp::ir_update_message, make_bag_equip_msg(uid));

        } else if (cid.rfind("bag_tab_items_", 0) == 0) {
            if (!chk(dpp::snowflake(std::stoull(cid.substr(14))))) return;
            ev.reply(dpp::ir_update_message, make_pet_use_msg(uid));

        } else if (cid.rfind("bag_tab_other_", 0) == 0) {
            if (!chk(dpp::snowflake(std::stoull(cid.substr(14))))) return;
            ev.reply(dpp::ir_update_message, make_pet_other_msg(uid));

        } else if (cid.rfind("bag_tab_special_", 0) == 0) {
            if (!chk(dpp::snowflake(std::stoull(cid.substr(16))))) return;
            ev.reply(dpp::ir_update_message, make_bag_special_msg(uid));

        } else if (cid.rfind("bag_sell_page_equip_", 0) == 0) {
            if (!chk(dpp::snowflake(std::stoull(cid.substr(20))))) return;
            ev.reply(dpp::ir_update_message, make_bag_sell_equip_msg(uid));

        } else if (cid.rfind("bag_sell_page_items_", 0) == 0) {
            if (!chk(dpp::snowflake(std::stoull(cid.substr(20))))) return;
            ev.reply(dpp::ir_update_message, make_bag_sell_items_msg(uid));

        } else if (cid.rfind("bag_sell_eq_", 0) == 0) {
            std::string rest = cid.substr(12);
            size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
            dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
            if (!chk(bu)) return;
            std::string eq_key = rest.substr(s1 + 1);
            auto* gi = find_gacha_item(eq_key);
            if (!gi) return;
            int64_t price = eq_sell_price(gi->rarity);
            bool sold = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& inv = inventory_data[uid];
                auto& eq  = equipped_data[uid];
                int cnt = inv.count(eq_key) ? inv[eq_key] : 0;
                bool is_eq = (eq_key == eq.weapon || eq_key == eq.glove ||
                              eq_key == eq.clothes || eq_key == eq.shoes || eq_key == eq.orb);
                int sellable = cnt - (is_eq ? 1 : 0);
                if (sellable > 0) {
                    inv[eq_key]--;
                    chip_data[uid].chips += price;
                    sold = true;
                }
            }
            if (sold) { save_inventory(); save_chips(); }
            ev.reply(dpp::ir_update_message, make_bag_equip_msg(uid));

        } else if (cid.rfind("bag_sell_item_", 0) == 0) {
            std::string rest = cid.substr(14);
            size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
            dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
            if (!chk(bu)) return;
            std::string vi_key = rest.substr(s1 + 1);
            auto* vi = find_virtual_item(vi_key);
            if (!vi || vi->price <= 0 || vi->category == "hunt") return;
            int64_t sell_p = vi_sell_price(vi);
            bool sold = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& inv = inventory_data[uid];
                if (inv.count(vi_key) && inv[vi_key] > 0) {
                    inv[vi_key]--;
                    chip_data[uid].chips += sell_p;
                    sold = true;
                }
            }
            if (sold) { save_inventory(); save_chips(); }
            ev.reply(dpp::ir_update_message, make_pet_use_msg(uid));

        } else if (cid.rfind("bag_sell_bulk_", 0) == 0) {
            std::string rest = cid.substr(14);
            size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
            dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
            if (!chk(bu)) return;
            std::string rarity = rest.substr(s1 + 1);
            int64_t price = eq_sell_price(rarity);
            int64_t total = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& inv = inventory_data[uid];
                auto& eq  = equipped_data[uid];
                for (auto& [k, cnt] : inv) {
                    if (k.size() < 3 || k.substr(0, 3) != "EQ_" || cnt <= 0) continue;
                    auto* gi = find_gacha_item(k);
                    if (!gi || gi->rarity != rarity) continue;
                    bool is_eq = (k == eq.weapon || k == eq.glove ||
                                  k == eq.clothes || k == eq.shoes || k == eq.orb);
                    int sellable = cnt - (is_eq ? 1 : 0);
                    if (sellable > 0) {
                        total += (int64_t)sellable * price;
                        inv[k] -= sellable;
                    }
                }
                chip_data[uid].chips += total;
            }
            if (total > 0) { save_inventory(); save_chips(); }
            ev.reply(dpp::ir_update_message, make_bag_equip_msg(uid));
        }
        return;
    }
}
