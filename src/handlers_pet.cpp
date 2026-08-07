#include "types.h"
#include "chips.h"
#include "helpers.h"
#include "pet.h"
#include "shop.h"
#include "handler_decls.h"

// ─── Pet / Lobby / Talent-pick button handler ─────────────────────────────────

void handle_pet_button(const dpp::button_click_t& ev)
{
    const std::string& cid  = ev.custom_id;
    const auto& user        = ev.command.get_issuing_user();
    dpp::snowflake uid      = user.id;

    // ── 大廳按鈕 ─────────────────────────────────────────────────────────────
    if (cid.rfind("lobby_", 0) == 0) {
        if (cid.rfind("lobby_main_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(11)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
            }
            bool src_v2 = (ev.command.message.flags & dpp::m_using_components_v2) != 0;
            ev.reply(src_v2 ? dpp::ir_update_message : dpp::ir_channel_message_with_source,
                make_lobby_msg(uid, user.get_avatar_url(), ev.command.member.get_nickname()));
        } else if (cid.rfind("lobby_shop_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(11)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_shop_main_msg(std::to_string((uint64_t)uid)));
        }
        return;
    }

    // ── 寵物按鈕 ─────────────────────────────────────────────────────────────
    if (cid.rfind("pet_", 0) == 0) {
        if (cid.rfind("pet_work_select_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(16)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_pet_work_select_msg(uid));
        } else if (cid.rfind("pet_work_", 0) == 0) {
            std::string rest = cid.substr(9);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
            int task = std::stoi(rest.substr(sep + 1));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, handle_pet_work_start(uid, task));
        } else if (cid.rfind("pet_claim_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(10)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, handle_pet_work_claim(uid));
        } else if (cid.rfind("pet_view_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(9)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_pet_view_msg(uid,
                user.get_avatar_url(), ev.command.member.get_nickname()));
        } else if (cid.rfind("pet_refresh_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(12)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_pet_view_msg(uid,
                user.get_avatar_url(), ev.command.member.get_nickname()));
        } else if (cid.rfind("pet_cancel_work_confirm_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(24)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, handle_pet_cancel_work(uid));
        } else if (cid.rfind("pet_cancel_work_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(16)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::string uid_s = std::to_string((uint64_t)uid);
                dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
                ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
                    .set_content("## ❓ 確定要取消打工？\n本次打工將**立即中止**，不會獲得任何報酬。"));
                dpp::component cr; cr.set_type(dpp::cot_action_row);
                cr.add_component(dpp::component().set_type(dpp::cot_button)
                    .set_label("✅ 是，取消打工").set_id("pet_cancel_work_confirm_" + uid_s).set_style(dpp::cos_danger));
                cr.add_component(dpp::component().set_type(dpp::cot_button)
                    .set_label("❌ 不，繼續打工").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_success));
                dpp::message cm; cm.set_flags(dpp::m_using_components_v2);
                cm.add_component_v2(ct); cm.add_component_v2(cr);
                ev.reply(dpp::ir_update_message, cm);
            }
        } else if (cid.rfind("pet_start_onsen_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(16)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, handle_pet_start_onsen(uid));
        } else if (cid.rfind("pet_cancel_onsen_confirm_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(25)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, handle_pet_cancel_onsen(uid));
        } else if (cid.rfind("pet_cancel_onsen_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(17)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::string uid_s = std::to_string((uint64_t)uid);
                dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE6, 0x7E, 0x22));
                ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
                    .set_content("## ⚠️ 確定要取消泡溫泉？\n取消後**負面狀態不會清除**，已花費的溫泉時間也不會退回。"));
                dpp::component row; row.set_type(dpp::cot_action_row);
                row.add_component(dpp::component().set_type(dpp::cot_button).set_label("✅ 確認取消")
                    .set_id("pet_cancel_onsen_confirm_" + uid_s).set_style(dpp::cos_danger));
                row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩️ 返回")
                    .set_id("pet_view_" + uid_s).set_style(dpp::cos_secondary));
                dpp::message cm; cm.set_flags(dpp::m_using_components_v2);
                cm.add_component_v2(ct); cm.add_component_v2(row);
                ev.reply(dpp::ir_update_message, cm);
            }
        } else if (cid.rfind("pet_notify_toggle_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(18)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = pet_data.find(uid);
                if (it != pet_data.end())
                    it->second.notify_after_work = !it->second.notify_after_work;
            }
            save_pet_data();
            ev.reply(dpp::ir_update_message, make_pet_view_msg(uid,
                user.get_avatar_url(), ev.command.member.get_nickname()));
        } else if (cid.rfind("pet_open_use_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(13)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_pet_use_msg(uid, 0));
        } else if (cid.rfind("pet_bag_page_", 0) == 0) {
            std::string rest = cid.substr(13);
            size_t sep = rest.rfind('_');
            if (sep == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
            int page = std::stoi(rest.substr(sep + 1));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的背包！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_pet_use_msg(uid, page));
        } else if (cid.rfind("pet_use_", 0) == 0) {
            std::string rest = cid.substr(8);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
            std::string item_key = rest.substr(sep + 1);
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            if (item_key == "path_reincarnate") {
                ev.reply(dpp::ir_update_message, make_reincarnate_pick_msg(uid)); return;
            }
            if (item_key == "path_rebirth") {
                ev.reply(dpp::ir_update_message, make_rebirth_pick_msg(uid)); return;
            }
            int cnt = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto ii = inventory_data.find(uid);
                if (ii != inventory_data.end()) { auto ci = ii->second.find(item_key); if (ci != ii->second.end()) cnt = ci->second; }
            }
            if (cnt > 1) { ev.reply(dpp::ir_update_message, make_pet_use_qty_msg(uid, item_key)); return; }
            ev.reply(dpp::ir_update_message, handle_pet_use_item(uid, item_key, 1));
        } else if (cid.rfind("pet_useqty_", 0) == 0) {
            std::string rest = cid.substr(11);
            size_t s1 = rest.find('_');
            if (s1 == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, s1)));
            rest = rest.substr(s1 + 1);
            size_t s2 = rest.find('_');
            if (s2 == std::string::npos) return;
            int qty = std::stoi(rest.substr(0, s2));
            std::string item_key = rest.substr(s2 + 1);
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, handle_pet_use_item(uid, item_key, qty));
        } else if (cid.rfind("pet_reincarnate_", 0) == 0) {
            std::string rest = cid.substr(16);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
            std::string slug = rest.substr(sep + 1);
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string notice; bool ok = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto pit = pet_data.find(uid);
                std::string new_chain = slug_to_chain(slug);
                if (pit == pet_data.end() || pit->second.stage == 0) {
                    notice = "❌ 需要已進化的寵物才能使用轉生卡！";
                } else if (new_chain.empty() || new_chain == pit->second.chain) {
                    notice = "❌ 無效的品種！";
                } else {
                    auto& inv = inventory_data[uid];
                    auto ci = inv.find("path_reincarnate");
                    if (ci == inv.end() || ci->second <= 0) {
                        notice = "❌ 沒有轉生卡了！";
                    } else {
                        ci->second--;
                        auto& p = pit->second;
                        p.chain = new_chain;
                        p.variant = "";
                        notice = "✅ 轉生成功！新品種：**" + pet_name(p.chain, p.stage, p.variant) + "**";
                        ok = true;
                    }
                }
            }
            if (ok) { save_pet_data(); save_inventory(); }
            ev.reply(dpp::ir_update_message, make_reincarnate_pick_msg(uid, notice));
        } else if (cid.rfind("pet_rebirth_", 0) == 0) {
            std::string rest = cid.substr(12);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
            std::string slug = rest.substr(sep + 1);
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string notice; bool ok = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto pit = pet_data.find(uid);
                if (pit == pet_data.end() || pit->second.stage < 2) {
                    notice = "❌ 需要二階以上的寵物才能使用重生卡！（一階分支尚未分歧）";
                } else {
                    auto& p = pit->second;
                    auto opts = rebirth_options(p.chain);
                    const RebirthOption* picked = nullptr;
                    for (auto& o : opts) if (o.slug == slug) { picked = &o; break; }
                    if (!picked) {
                        notice = "❌ 無效的路線！";
                    } else if (picked->variant == p.variant) {
                        notice = "❌ 這跟目前的路線一樣，請選擇不同的路線！";
                    } else {
                        auto& inv = inventory_data[uid];
                        auto ci = inv.find("path_rebirth");
                        if (ci == inv.end() || ci->second <= 0) {
                            notice = "❌ 沒有重生卡了！";
                        } else {
                            ci->second--;
                            p.variant = picked->variant;
                            notice = "✅ 重生成功！新路線：**" + pet_name(p.chain, p.stage, p.variant) + "**";
                            ok = true;
                        }
                    }
                }
            }
            if (ok) { save_pet_data(); save_inventory(); }
            ev.reply(dpp::ir_update_message, make_rebirth_pick_msg(uid, notice));
        } else if (cid.rfind("pet_discard_mode_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(17)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_pet_discard_mode_msg(uid));
        } else if (cid.rfind("pet_discard_confirm_", 0) == 0) {
            std::string rest = cid.substr(20);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
            std::string item_key = rest.substr(sep + 1);
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_pet_discard_confirm_msg(uid, item_key));
        } else if (cid.rfind("pet_discard_do_", 0) == 0) {
            std::string rest = cid.substr(15);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
            std::string item_key = rest.substr(sep + 1);
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, handle_pet_discard_item(uid, item_key));
        } else if (cid.rfind("pet_rename_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(11)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            dpp::interaction_modal_response modal(
                "pet_rename_modal_" + std::to_string((uint64_t)uid), "為寵物改名");
            modal.add_component(dpp::component().set_type(dpp::cot_text)
                .set_label("寵物暱稱（留空則清除暱稱）").set_id("new_name")
                .set_text_style(dpp::text_short).set_min_length(0).set_max_length(20)
                .set_placeholder("輸入新名字，或留空清除"));
            ev.dialog(modal);
        } else if (cid.rfind("pet_refine_star_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(16)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::string sid = std::to_string((uint64_t)uid);
                dpp::message cm; cm.set_flags(dpp::m_using_components_v2 | dpp::m_ephemeral);
                dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF1, 0xC4, 0x0F));
                ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
                    .set_content("## ✨ 提煉星星\n確定要消耗 **50 exp** 提煉星星嗎？\n成功率：**90%**"));
                cm.add_component_v2(ct);
                dpp::component row; row.set_type(dpp::cot_action_row);
                row.add_component(dpp::component().set_type(dpp::cot_button)
                    .set_label("✅ 確認提煉").set_id("pet_refine_confirm_" + sid).set_style(dpp::cos_success));
                row.add_component(dpp::component().set_type(dpp::cot_button)
                    .set_label("❌ 取消").set_id("pet_refine_cancel_" + sid).set_style(dpp::cos_secondary));
                cm.add_component_v2(row);
                ev.reply(dpp::ir_channel_message_with_source, cm);
            }
        } else if (cid.rfind("pet_refine_confirm_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(19)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, handle_pet_refine_star(uid));
        } else if (cid.rfind("pet_refine_cancel_", 0) == 0) {
            {
                dpp::message cm; cm.set_flags(dpp::m_using_components_v2 | dpp::m_ephemeral);
                dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x95, 0xA5, 0xA6));
                ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content("❌ 已取消提煉。"));
                cm.add_component_v2(ct);
                ev.reply(dpp::ir_update_message, cm);
            }
        } else if (cid.rfind("pet_release_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(12)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            dpp::interaction_modal_response modal(
                "pet_release_modal_" + std::to_string((uint64_t)uid), "🕊️ 放生確認");
            modal.add_component(dpp::component().set_type(dpp::cot_text)
                .set_label("請輸入「放生」以確認").set_id("confirm_text")
                .set_text_style(dpp::text_short).set_min_length(2).set_max_length(4)
                .set_placeholder("放生"));
            ev.dialog(modal);
        }
        return;
    }

    // ── 天賦選擇按鈕（talent_pick_ 不以 pet_ 開頭）────────────────────────────
    if (cid.rfind("talent_pick_", 0) == 0) {
        // talent_pick_{talent}_{uid}
        std::string rest = cid.substr(12);
        size_t last = rest.rfind('_');
        if (last == std::string::npos) return;
        std::string talent = rest.substr(0, last);
        dpp::snowflake btn_uid(std::stoull(rest.substr(last + 1)));
        if (uid != btn_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的背包！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string old_talent;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& inv = inventory_data[uid];
            auto it = inv.find("talent_scroll");
            if (it == inv.end() || it->second <= 0) {
                dpp::component ct3; ct3.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
                ct3.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content("## ❌ 失敗\n你已沒有天賦賦予卷軸！"));
                dpp::message em3; em3.set_flags(dpp::m_using_components_v2); em3.add_component_v2(ct3);
                ev.reply(dpp::ir_update_message, em3); return;
            }
            auto& pet = pet_data[uid];
            old_talent = pet.talent;
            inv["talent_scroll"]--;
            pet.talent = talent;
        }
        save_inventory();
        save_pet_data();
        auto talent_desc_fn = [](const std::string& t) -> std::string {
            if (t == "迅捷")      return "打工時間縮短 10%！";
            if (t == "招人喜歡")  return "打工報酬提升 10%！";
            if (t == "幸運")      return "打工有 5% 機率獲得雙倍報酬！";
            if (t == "天然呆")    return "使用道具時有 5% 機率不消耗道具！";
            if (t == "喜歡作夢")  return "每次打工完有 0.1% 機率將現有籌碼翻倍！";
            return "";
        };
        std::string result_txt;
        if (!old_talent.empty())
            result_txt = "## 🌟 天賦覺醒！\n🔄 天賦已替換！\n**" + old_talent + "** → **" + talent + "**\n" + talent_desc_fn(talent);
        else
            result_txt = "## 🌟 天賦覺醒！\n✨ 天賦賦予成功！\n**" + talent + "** — " + talent_desc_fn(talent);
        dpp::component ct4; ct4.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF3, 0x9C, 0x12));
        ct4.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(result_txt));
        dpp::message rm; rm.set_flags(dpp::m_using_components_v2); rm.add_component_v2(ct4);
        ev.reply(dpp::ir_update_message, rm);
        return;
    }
}

// ─── Pet modal handler ────────────────────────────────────────────────────────

void handle_pet_modal(const dpp::form_submit_t& ev)
{
    const std::string& cid  = ev.custom_id;
    dpp::snowflake issuer   = ev.command.get_issuing_user().id;

    auto get_text = [&]() -> std::string {
        std::string v;
        for (auto& row : ev.components) {
            if (std::holds_alternative<std::string>(row.value)) v = std::get<std::string>(row.value);
            for (auto& sub : row.components)
                if (std::holds_alternative<std::string>(sub.value)) v = std::get<std::string>(sub.value);
        }
        return v;
    };

    if (cid.rfind("pet_rename_modal_", 0) == 0) {
        dpp::snowflake modal_uid(std::stoull(cid.substr(17)));
        if (issuer != modal_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string new_name = get_text();
        while (!new_name.empty() && (new_name.front()==' '||new_name.front()=='\t')) new_name.erase(new_name.begin());
        while (!new_name.empty() && (new_name.back()==' '||new_name.back()=='\r'||new_name.back()=='\n')) new_name.pop_back();
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (!pet_data.count(modal_uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你沒有寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            pet_data[modal_uid].custom_name = new_name;
        }
        save_pet_data();
        std::string msg_text = new_name.empty() ? "✅ 已清除寵物暱稱！" : "✅ 已將寵物改名為 **" + new_name + "**！";
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message(msg_text).set_flags(dpp::m_ephemeral));
        return;
    }

    if (cid.rfind("pet_release_modal_", 0) == 0) {
        dpp::snowflake modal_uid(std::stoull(cid.substr(18)));
        if (issuer != modal_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
        }
        if (get_text() != "放生") {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 輸入錯誤，放生已取消。").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_channel_message_with_source,
            handle_pet_release(modal_uid).set_flags(dpp::m_ephemeral));
        return;
    }
}

// ─── Pet slash handler ────────────────────────────────────────────────────────

void handle_pet_slash(const dpp::slashcommand_t& ev, const std::string& cmd_name)
{
    const auto& user   = ev.command.get_issuing_user();
    dpp::snowflake uid = user.id;

    auto track_owner = [&]() {
        ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                msg_owner[std::get<dpp::message>(cb.value).id] = uid;
            }
        });
    };

    if (cmd_name == "大廳" || cmd_name == "lobby") {
        ev.reply(dpp::ir_channel_message_with_source, make_lobby_msg(uid,
            user.get_avatar_url(), ev.command.member.get_nickname()));
        track_owner();
    }
    else if (cmd_name == "寵物" || cmd_name == "pet") {
        ev.reply(dpp::ir_channel_message_with_source, make_pet_view_msg(uid,
            user.get_avatar_url(), ev.command.member.get_nickname()));
        track_owner();
    }
    else if (cmd_name == "背包" || cmd_name == "bag" || cmd_name == "petuse") {
        std::string dn = ev.command.member.get_nickname();
        ev.reply(dpp::ir_channel_message_with_source,
            make_bag_home_msg(uid, dn, user.get_avatar_url()));
        track_owner();
    }
    else if (cmd_name == "寵物圖鑑" || cmd_name == "petdex") {
        ev.reply(dpp::ir_channel_message_with_source, make_petdex_msg("嫩寶"));
        track_owner();
    }
}
