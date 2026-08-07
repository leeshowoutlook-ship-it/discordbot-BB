#include "types.h"
#include "chips.h"
#include "helpers.h"
#include "stock.h"
#include "handler_decls.h"

// ─── 股票按鈕 ───────────────────────────────────────────────────────────────────

void handle_stock_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;
    std::string dn = ev.command.member.get_nickname();
    if (dn.empty()) dn = ev.command.get_issuing_user().global_name.empty()
                         ? ev.command.get_issuing_user().username
                         : ev.command.get_issuing_user().global_name;
    std::string av = ev.command.get_issuing_user().get_avatar_url();
    std::string uid_s = std::to_string((uint64_t)uid);

    auto split_uid_key = [&](const std::string& rest) -> std::pair<dpp::snowflake, std::string> {
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return {0, ""};
        return {dpp::snowflake(std::stoull(rest.substr(0, sep))), rest.substr(sep + 1)};
    };

    if (cid == "stock_home_" + uid_s) {
        // 股市首頁使用 Components V2，不能 ir_update_message 更新非 V2 訊息（例如大廳），
        // 一律送新訊息，避免 V1/V2 相容性問題。
        ev.reply(dpp::ir_channel_message_with_source, make_stock_home_msg(uid, dn, av)); return;
    }

    if (cid.rfind("stock_view_", 0) == 0) {
        auto [bu, key] = split_uid_key(cid.substr(11));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 這不是你的股市！").set_flags(dpp::m_ephemeral)); return; }
        // 個股詳細頁為一般 embed，不可 ir_update_message 更新 V2 首頁，改送新訊息。
        ev.reply(dpp::ir_channel_message_with_source, make_stock_detail_msg(uid, key, dn, av)); return;
    }

    if (cid.rfind("stock_buy_", 0) == 0) {
        auto [bu, key] = split_uid_key(cid.substr(10));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 這不是你的股市！").set_flags(dpp::m_ephemeral)); return; }
        int64_t price = 0;
        { std::lock_guard<std::mutex> lk(stock_mutex); auto it = stock_market.find(key); if (it != stock_market.end()) price = it->second.price; }
        dpp::interaction_modal_response modal("stock_buy_modal_" + uid_s + "_" + key, "買進股票");
        modal.add_component(dpp::component().set_type(dpp::cot_text)
            .set_label("要買幾股？（現價 " + std::to_string(price) + " 碼／股）")
            .set_id("qty").set_text_style(dpp::text_short)
            .set_min_length(1).set_max_length(10).set_placeholder("輸入股數"));
        ev.dialog(modal); return;
    }

    if (cid.rfind("stock_sell_", 0) == 0) {
        auto [bu, key] = split_uid_key(cid.substr(11));
        if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 這不是你的股市！").set_flags(dpp::m_ephemeral)); return; }
        int64_t shares = 0;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto pit = player_stocks.find(uid);
          if (pit != player_stocks.end()) { auto hit = pit->second.find(key); if (hit != pit->second.end()) shares = hit->second.shares; }
        }
        dpp::interaction_modal_response modal("stock_sell_modal_" + uid_s + "_" + key, "賣出股票");
        modal.add_component(dpp::component().set_type(dpp::cot_text)
            .set_label("要賣幾股？（持有 " + std::to_string(shares) + " 股，或輸入 all）")
            .set_id("qty").set_text_style(dpp::text_short)
            .set_min_length(1).set_max_length(10).set_placeholder("輸入股數或 all"));
        ev.dialog(modal); return;
    }

    if (cid == "stock_mood_set_" + uid_s) {
        if (cfg.notify_user_id.empty() || uid_s != cfg.notify_user_id) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
        }
        int64_t cur = 0;
        { std::lock_guard<std::mutex> lk(stock_mutex); auto it = stock_market.find("stock_mood"); if (it != stock_market.end()) cur = it->second.price; }
        dpp::interaction_modal_response modal("stock_mood_modal_" + uid_s, "調整 LeeShoW 的心情");
        modal.add_component(dpp::component().set_type(dpp::cot_text)
            .set_label("新價格（目前 " + std::to_string(cur) + " 碼）")
            .set_id("price").set_text_style(dpp::text_short)
            .set_min_length(1).set_max_length(10).set_placeholder("輸入新的股價"));
        ev.dialog(modal); return;
    }
}

// ─── 股票 Modal ─────────────────────────────────────────────────────────────────

void handle_stock_modal(const dpp::form_submit_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;
    std::string dn = ev.command.member.get_nickname();
    if (dn.empty()) dn = ev.command.get_issuing_user().global_name.empty()
                         ? ev.command.get_issuing_user().username
                         : ev.command.get_issuing_user().global_name;
    std::string av = ev.command.get_issuing_user().get_avatar_url();
    std::string uid_s = std::to_string((uint64_t)uid);

    auto get_input = [&]() -> std::string {
        std::string input;
        for (auto& row : ev.components) {
            if (std::holds_alternative<std::string>(row.value))
                input = std::get<std::string>(row.value);
            for (auto& sub : row.components)
                if (std::holds_alternative<std::string>(sub.value))
                    input = std::get<std::string>(sub.value);
        }
        return input;
    };

    if (cid.rfind("stock_buy_modal_" + uid_s + "_", 0) == 0) {
        std::string key = cid.substr(std::string("stock_buy_modal_" + uid_s + "_").size());
        int64_t qty = 0;
        try { qty = std::stoll(get_input()); } catch (...) {}
        std::string notice = do_stock_buy(uid, key, qty);
        ev.reply(dpp::ir_update_message, make_stock_detail_msg(uid, key, dn, av, notice)); return;
    }

    if (cid.rfind("stock_sell_modal_" + uid_s + "_", 0) == 0) {
        std::string key = cid.substr(std::string("stock_sell_modal_" + uid_s + "_").size());
        std::string input = get_input();
        std::string inp_lower = input;
        for (auto& c : inp_lower) c = (char)std::tolower((unsigned char)c);
        int64_t qty = 0;
        if (inp_lower == "all") {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto pit = player_stocks.find(uid);
            if (pit != player_stocks.end()) { auto hit = pit->second.find(key); if (hit != pit->second.end()) qty = hit->second.shares; }
        } else {
            try { qty = std::stoll(input); } catch (...) {}
        }
        std::string notice = do_stock_sell(uid, key, qty);
        ev.reply(dpp::ir_update_message, make_stock_detail_msg(uid, key, dn, av, notice)); return;
    }

    if (cid == "stock_mood_modal_" + uid_s) {
        if (cfg.notify_user_id.empty() || uid_s != cfg.notify_user_id) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
        }
        int64_t new_price = 0;
        try { new_price = std::stoll(get_input()); } catch (...) {}
        std::string notice;
        if (new_price <= 0) {
            notice = "❌ 價格需為正整數！";
        } else {
            { std::lock_guard<std::mutex> lk(stock_mutex);
              auto& s = stock_market["stock_mood"];
              if (s.price > 0) {
                  s.history.push_back(s.price);
                  if (s.history.size() > 30) s.history.erase(s.history.begin());
              }
              s.prev_close = s.price;
              s.price = new_price;
              s.last_update = time(nullptr);
            }
            save_stock_market();
            notice = "✅ 已調整心情股價為 **" + std::to_string(new_price) + "** 碼！";
        }
        ev.reply(dpp::ir_update_message, make_stock_detail_msg(uid, "stock_mood", dn, av, notice)); return;
    }
}
