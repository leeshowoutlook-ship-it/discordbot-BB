#include "types.h"
#include "chips.h"
#include "helpers.h"
#include "shop.h"
#include "adventure.h"
#include "enhance.h"
#include "handler_decls.h"

// ─── Adventure / Collection / Enhance slash ──────────────────────────────────

void handle_adv_slash(const dpp::slashcommand_t& ev,
                      const std::string& cmd_name,
                      dpp::snowflake uid, dpp::snowflake ch)
{
    const auto& user = ev.command.get_issuing_user();
    std::string dn   = ev.command.member.get_nickname();
    std::string av   = user.get_avatar_url();

    if (cmd_name == "收藏" || cmd_name == "collect") {
        ev.reply(dpp::ir_channel_message_with_source,
            make_collection_msg(uid, dn, av));
        ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                msg_owner[std::get<dpp::message>(cb.value).id] = uid;
            }
        });
    }
    else if (cmd_name == "探險" || cmd_name == "adventure") {
        ev.reply(dpp::ir_channel_message_with_source,
            make_adv_main_msg(uid, dn, av));
        ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                msg_owner[std::get<dpp::message>(cb.value).id] = uid;
            }
        });
    }
    else if (cmd_name == "強化" || cmd_name == "enhance") {
        ev.reply(dpp::ir_channel_message_with_source,
            make_enhance_main_msg(uid, dn, av));
        ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                msg_owner[std::get<dpp::message>(cb.value).id] = uid;
            }
        });
    }
}

// ─── Adventure modal (adv_funds_modal_) ──────────────────────────────────────

void handle_adv_modal(const dpp::form_submit_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake issuer   = ev.command.get_issuing_user().id;

    if (cid.rfind("adv_funds_modal_", 0) == 0) {
        dpp::snowflake modal_uid(std::stoull(cid.substr(16)));
        if (issuer != modal_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的操作！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string input;
        for (auto& row : ev.components) {
            if (std::holds_alternative<std::string>(row.value))
                input = std::get<std::string>(row.value);
            for (auto& sub : row.components)
                if (std::holds_alternative<std::string>(sub.value))
                    input = std::get<std::string>(sub.value);
        }
        int64_t amount = 0;
        try { amount = std::stoll(input); } catch (...) {}
        int64_t requested = amount;
        amount = std::max((int64_t)0, std::min((int64_t)10000, amount));
        int64_t chips = get_chips(modal_uid);
        std::string notice;
        if (amount > chips) {
            amount = std::max((int64_t)0, chips);
            notice = "⚠️ 你輸入的 " + std::to_string(requested) + " 碼超過錢包餘額，已自動調整為 **" + std::to_string(amount) + "** 碼。";
        }
        dpp::snowflake setup_msg_id = 0, setup_ch_id = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            adv_setups[modal_uid].funds = amount;
            setup_msg_id = adv_setups[modal_uid].setup_msg_id;
            setup_ch_id  = adv_setups[modal_uid].setup_ch_id;
        }
        std::string mdn = ev.command.member.get_nickname();
        if (mdn.empty()) mdn = ev.command.get_issuing_user().global_name.empty()
                             ? ev.command.get_issuing_user().username
                             : ev.command.get_issuing_user().global_name;
        std::string mav = ev.command.get_issuing_user().get_avatar_url();
        if (setup_msg_id && setup_ch_id) {
            auto upd = make_adv_setup_msg(modal_uid, mdn, mav, notice);
            upd.id = setup_msg_id; upd.channel_id = setup_ch_id;
            g_bot->message_edit(upd);
        }
        std::string ack = notice.empty()
            ? "✅ 探險資金已設定為 **" + std::to_string(amount) + "** 碼。"
            : notice;
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message(ack).set_flags(dpp::m_ephemeral));
        return;
    }
}
