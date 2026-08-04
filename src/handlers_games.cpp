#include "types.h"
#include "chips.h"
#include "helpers.h"
#include "dice.h"
#include "shoot.h"
#include "rocket.h"
#include "scratch.h"
#include "scroll.h"
#include "guess.h"
#include "handler_decls.h"

// ─── Games message handler (!骰子 !射 !火箭 !刮 !猜 !卷軸使用) ─────────────────

void handle_games_message(const dpp::message_create_t& ev,
                          const std::string& content,
                          dpp::snowflake uid, dpp::snowflake ch)
{
    // !骰子 <碼|ALL>
    if (content.rfind("!骰子", 0) == 0) {
        size_t sp = content.find(' ');
        std::string rest = (sp != std::string::npos) ? content.substr(sp + 1) : "";
        std::string rest_lo = rest;
        for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
        bool is_all = (rest_lo == "all");
        int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
        if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
            bet = get_chips(uid);
            if (bet < 5000) {
                dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                m.channel_id = ch; g_bot->message_create(m); return;
            }
        } else {
            if (bet <= 0) {
                dpp::message m; m.set_content("用法：`!骰子 <籌碼量>`  例：`!骰子 100` 或 `!骰子 ALL`");
                m.channel_id = ch; g_bot->message_create(m); return;
            }
            if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                dpp::message m; m.set_content("❌ 此討論串最低下注為 **1,000** 碼！");
                m.channel_id = ch; g_bot->message_create(m); return;
            }
        }
        if (get_chips(uid) < bet) {
            dpp::embed e; e.set_title("❌  籌碼不足").set_color(0xE74C3C);
            dpp::message m; m.add_embed(e); m.channel_id = ch; g_bot->message_create(m); return;
        }
        dpp::message m = start_dice(uid, ch, bet,
            ev.msg.author.get_avatar_url(), ev.msg.author.username);
        m.channel_id = ch; g_bot->message_create(m);
        return;
    }

    // !射 <碼|ALL>
    if (content.rfind("!射", 0) == 0 && (content.size() == 4 || content[4] == ' ')) {
        size_t sp = content.find(' ');
        std::string rest = (sp != std::string::npos) ? content.substr(sp + 1) : "";
        std::string rest_lo = rest;
        for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
        bool is_all = (rest_lo == "all");
        int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
        if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
            bet = get_chips(uid);
            if (bet < 5000) {
                dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
        } else {
            if (bet <= 0) {
                dpp::message m; m.set_content("用法：`!射 <籌碼量>`  例：`!射 100` 或 `!射 ALL`");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
            if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                dpp::message m; m.set_content("❌ 此討論串最低下柱為 **1,000** 碼！");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
        }
        dpp::message start_msg = handle_shoot_start(uid, ch, bet,
            ev.msg.author.get_avatar_url(), ev.msg.member.get_nickname());
        start_msg.set_reference(ev.msg.id); start_msg.channel_id = ch;
        g_bot->message_create(start_msg, [uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = shoot_games.find(uid);
                if (it != shoot_games.end())
                    it->second.msg_id = std::get<dpp::message>(cb.value).id;
            }
        });
        return;
    }

    // !火箭 <碼|ALL>
    if (content.rfind("!火箭", 0) == 0 && (content.size() == 7 || content[7] == ' ')) {
        size_t sp = content.find(' ');
        std::string rest = (sp != std::string::npos) ? content.substr(sp + 1) : "";
        std::string rest_lo = rest;
        for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
        bool is_all = (rest_lo == "all");
        int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
        if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
            bet = get_chips(uid);
            if (bet < 5000) {
                dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
        } else {
            if (bet <= 0) {
                dpp::message m; m.set_content("用法：`!火箭 <籌碼量>`  例：`!火箭 100` 或 `!火箭 ALL`");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
            if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                dpp::message m; m.set_content("❌ 此討論串最低下注為 **1,000** 碼！");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
        }
        start_cmd(*g_bot, uid, ch, handle_rocket_start(uid, ch, bet,
            ev.msg.author.get_avatar_url(), ev.msg.member.get_nickname()), ev.msg.id);
        return;
    }

    // !卷軸使用 [成功率%] [張數]
    if (content.rfind("!卷軸使用", 0) == 0 && (content.size() == 13 || content[13] == ' ')) {
        std::string rest = (content.size() > 14) ? content.substr(14) : "";
        int pct = 0, cnt = 1;
        if (!rest.empty()) {
            std::istringstream iss(rest);
            iss >> pct >> cnt;
            if (pct != 10 && pct != 30 && pct != 60 && pct != 70) pct = 0;
            if (cnt <= 0) cnt = 1;
            if (cnt > 100) cnt = 100;
        }
        dpp::message m;
        if (pct == 0) m = make_scroll_sel_msg(uid);
        else          m = make_scroll_result_msg(uid, pct, cnt);
        m.set_reference(ev.msg.id); m.channel_id = ch;
        g_bot->message_create(m);
        return;
    }

    // !刮 <碼|ALL>
    if (content.rfind("!刮", 0) == 0 && (content.size() == 4 || content[4] == ' ')) {
        size_t sp = content.find(' ');
        std::string rest = (sp != std::string::npos) ? content.substr(sp + 1) : "";
        std::string rest_lo = rest;
        for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
        bool is_all = (rest_lo == "all");
        int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
        if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
            bet = get_chips(uid);
            if (bet < 5000) {
                dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
        } else {
            if (bet <= 0) {
                dpp::message m; m.set_content("用法：`!刮 <籌碼量>`  例：`!刮 100` 或 `!刮 ALL`");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
            if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                dpp::message m; m.set_content("❌ 此討論串最低下注為 **1,000** 碼！");
                m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
            }
        }
        start_cmd(*g_bot, uid, ch, handle_scratch_start(uid, ch, bet,
            ev.msg.author.get_avatar_url(), ev.msg.member.get_nickname()), ev.msg.id);
        return;
    }

    // !猜（排除 !猜拳）
    if ((content.rfind("!猜", 0) == 0 || content.rfind("！猜", 0) == 0)
         && content.rfind("!猜拳", 0) != 0 && content.rfind("！猜拳", 0) != 0) {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (guess_games.count(uid)) {
                dpp::message m; m.set_content("❌ 你已有進行中的猜數字遊戲！");
                m.channel_id = ch; g_bot->message_create(m); return;
            }
            GuessGame g;
            g.uid = uid; g.channel_id = ch;
            g.secret = guess_gen_secret();
            g.avatar_url   = ev.msg.author.get_avatar_url();
            g.display_name = ev.msg.member.get_nickname().empty()
                             ? ev.msg.author.username : ev.msg.member.get_nickname();
            guess_games[uid] = g;
        }
        GuessGame snap; { std::lock_guard<std::mutex> lk(data_mutex); snap = guess_games[uid]; }
        auto msg = make_guess_msg(snap); msg.channel_id = ch;
        g_bot->message_create(msg, [uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (guess_games.count(uid))
                    guess_games[uid].msg_id = std::get<dpp::message>(cb.value).id;
            }
        });
        return;
    }
}

// ─── Games button handler ─────────────────────────────────────────────────────

void handle_games_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    const dpp::user&   user = ev.command.get_issuing_user();
    dpp::snowflake     uid  = user.id;

    // ── 骰子押注選擇 ─────────────────────────────────────────────────────────
    if (cid.rfind("dc_", 0) == 0 && cid.rfind("dc_again_", 0) != 0) {
        std::string rest = cid.substr(3);
        size_t sep = rest.rfind('_');
        if (sep == std::string::npos) return;
        uint64_t gid = std::stoull(rest.substr(0, sep));
        int choice   = std::stoi(rest.substr(sep + 1));
        dpp::message res = handle_dice_pick(gid, choice, uid);
        if (res.embeds.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message, res);
        return;
    }

    // ── 猜數字按鈕 ───────────────────────────────────────────────────────────
    if (cid.rfind("guess_kbd_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(10)));
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (!guess_games.count(uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
        }
        dpp::interaction_modal_response modal(
            "guess_modal_" + std::to_string((uint64_t)uid), "💬 輸入猜測");
        modal.add_component(dpp::component()
            .set_type(dpp::cot_text)
            .set_label("輸入 4 位數字（不重複，可含 0）")
            .set_id("guess_input")
            .set_required(true)
            .set_min_length(4).set_max_length(4)
            .set_text_style(dpp::text_short));
        ev.dialog(modal);
        return;
    }
    if (cid.rfind("guess_digit_", 0) == 0) {
        size_t last = cid.rfind('_');
        char digit = cid[last + 1];
        dpp::snowflake owner(std::stoull(cid.substr(12, last - 12)));
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        GuessGame snap; bool found = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = guess_games.find(owner);
            if (it != guess_games.end()) {
                auto& g = it->second;
                if (g.input_buf.size() < 4 && g.input_buf.find(digit) == std::string::npos)
                    g.input_buf += digit;
                snap = g; found = true;
            }
        }
        if (!found) { ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return; }
        ev.reply(dpp::ir_update_message, make_guess_msg(snap));
        return;
    }
    if (cid.rfind("guess_back_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(11)));
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        GuessGame snap; bool found = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = guess_games.find(owner);
            if (it != guess_games.end()) {
                if (!it->second.input_buf.empty()) it->second.input_buf.pop_back();
                snap = it->second; found = true;
            }
        }
        if (!found) { ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return; }
        ev.reply(dpp::ir_update_message, make_guess_msg(snap));
        return;
    }
    if (cid.rfind("guess_confirm_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(14)));
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        bool won = false, game_over = false;
        GuessGame snap;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = guess_games.find(owner);
            if (it == guess_games.end()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            auto& g = it->second;
            if (!guess_valid(g.input_buf)) {
                g.input_buf = "";
                ev.reply(dpp::ir_update_message, make_guess_msg(g)); return;
            }
            std::string ab = guess_calc_ab(g.secret, g.input_buf);
            g.history.emplace_back(g.input_buf, ab);
            g.attempts++;
            g.input_buf = "";
            won = (ab == "4A0B");
            game_over = won || (g.attempts >= GuessGame::MAX_ATTEMPTS);
            if (game_over) {
                auto& st = guess_stats_data[(uint64_t)owner];
                st.games++;
                if (won) { st.wins++; st.total_win_attempts += g.attempts; }
            }
            snap = g;
            if (game_over) guess_games.erase(it);
        }
        if (game_over) {
            save_guess_stats();
            ev.reply(dpp::ir_update_message, make_guess_result_msg(snap, won));
        } else {
            ev.reply(dpp::ir_update_message, make_guess_msg(snap));
        }
        return;
    }
    if (cid.rfind("guess_quit_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(11)));
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        GuessGame snap; bool found = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = guess_games.find(uid);
            if (it != guess_games.end()) { snap = it->second; guess_games.erase(it); found = true; }
        }
        if (!found) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        { std::lock_guard<std::mutex> lk(data_mutex); guess_stats_data[(uint64_t)uid].games++; }
        save_guess_stats();
        ev.reply(dpp::ir_update_message, make_guess_result_msg(snap, false));
        return;
    }
    if (cid.rfind("guess_again_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(12)));
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        if (guess_games.count(uid)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 你已有進行中的猜數字遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            GuessGame g;
            g.uid = uid; g.channel_id = ev.command.channel_id;
            g.secret = guess_gen_secret();
            g.avatar_url   = user.get_avatar_url();
            g.display_name = ev.command.member.get_nickname().empty()
                             ? user.username : ev.command.member.get_nickname();
            guess_games[uid] = g;
        }
        GuessGame snap; { std::lock_guard<std::mutex> lk(data_mutex); snap = guess_games[uid]; }
        ev.reply(dpp::ir_channel_message_with_source, make_guess_msg(snap));
        ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (guess_games.count(uid))
                    guess_games[uid].msg_id = std::get<dpp::message>(cb.value).id;
            }
        });
        return;
    }

    // ── 骰子再來一局 ─────────────────────────────────────────────────────────
    if (cid.rfind("dc_again_", 0) == 0) {
        std::string rest = cid.substr(9);
        size_t sep = rest.find('_');
        if (sep == std::string::npos) return;
        dpp::snowflake orig_uid(std::stoull(rest.substr(0, sep)));
        int64_t bet = std::stoll(rest.substr(sep + 1));
        if (uid != orig_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ev.command.channel_id) == cfg.allin_thread_id) {
            bet = get_chips(uid);
            if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
        } else if (get_chips(uid) < bet) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 籌碼不足 " + std::to_string(bet) + " 碼！").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_channel_message_with_source, start_dice(uid, ev.command.channel_id, bet,
            user.get_avatar_url(), user.username));
        return;
    }

    // ── 射龍門按鈕 ───────────────────────────────────────────────────────────
    if (cid.rfind("shoot_", 0) == 0) {
        auto sh_get_uid = [&]() -> dpp::snowflake {
            if (cid.rfind("shoot_again_", 0) == 0) {
                std::string rest = cid.substr(12);
                size_t sep = rest.rfind('_');
                return dpp::snowflake(std::stoull(rest.substr(0, sep)));
            }
            size_t us = cid.rfind('_');
            return dpp::snowflake(std::stoull(cid.substr(us + 1)));
        };
        dpp::snowflake owner = sh_get_uid();
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        if (cid.rfind("shoot_again_", 0) == 0) {
            std::string rest = cid.substr(12);
            size_t sep = rest.rfind('_');
            int64_t bet = std::stoll(rest.substr(sep + 1));
            if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 無效下柱！").set_flags(dpp::m_ephemeral)); return; }
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ev.command.channel_id) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
            } else if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ev.command.channel_id) == cfg.min_bet_thread_id && bet < 1000) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此討論串最低下柱為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return;
            }
            dpp::message start_msg = handle_shoot_start(uid, ev.command.channel_id, bet,
                user.get_avatar_url(), ev.command.member.get_nickname());
            ev.reply(dpp::ir_channel_message_with_source, start_msg);
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = shoot_games.find(uid);
                    if (it != shoot_games.end())
                        it->second.msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        } else {
            ShootGame sg;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = shoot_games.find(uid);
                if (it == shoot_games.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                }
                sg = it->second;
                shoot_games.erase(it);
            }
            dpp::message result;
            if (cid.rfind("shoot_pass_", 0) == 0) {
                result = make_shoot_pass_msg(sg);
            } else {
                int direction = 0;
                if      (cid.rfind("shoot_up_", 0) == 0) direction =  1;
                else if (cid.rfind("shoot_dn_", 0) == 0) direction = -1;
                result = make_shoot_result_msg(sg, direction);
            }
            ev.reply(dpp::ir_update_message, result);
        }
        return;
    }

    // ── 火箭升空按鈕 ─────────────────────────────────────────────────────────
    if (cid.rfind("rocket_", 0) == 0) {
        auto rk_get_uid = [&]() -> dpp::snowflake {
            if (cid.rfind("rocket_again_", 0) == 0) {
                std::string rest = cid.substr(13);
                size_t sep = rest.rfind('_');
                return dpp::snowflake(std::stoull(rest.substr(0, sep)));
            }
            size_t us = cid.rfind('_');
            return dpp::snowflake(std::stoull(cid.substr(us + 1)));
        };
        dpp::snowflake owner = rk_get_uid();
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        if (cid.rfind("rocket_again_", 0) == 0) {
            std::string rest = cid.substr(13);
            size_t sep = rest.rfind('_');
            int64_t bet = std::stoll(rest.substr(sep + 1));
            if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 無效下注！").set_flags(dpp::m_ephemeral)); return; }
            ev.reply(dpp::ir_channel_message_with_source,
                handle_rocket_start(uid, ev.command.channel_id, bet,
                    user.get_avatar_url(), ev.command.member.get_nickname()));
        } else {
            RocketGame rg;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = rocket_games.find(uid);
                if (it == rocket_games.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                }
                rg = it->second;
            }
            if (cid.rfind("rocket_cash_", 0) == 0) {
                { std::lock_guard<std::mutex> lk(data_mutex); rocket_games.erase(uid); }
                ev.reply(dpp::ir_update_message, make_rocket_cash_msg(rg));
            } else {
                rg.presses++;
                if (rk_explodes()) {
                    { std::lock_guard<std::mutex> lk(data_mutex); rocket_games.erase(uid); }
                    ev.reply(dpp::ir_update_message, make_rocket_explode_msg(rg));
                } else if (rg.presses >= 10) {
                    { std::lock_guard<std::mutex> lk(data_mutex); rocket_games.erase(uid); }
                    ev.reply(dpp::ir_update_message, make_rocket_moon_msg(rg));
                } else {
                    { std::lock_guard<std::mutex> lk(data_mutex); rocket_games[uid] = rg; }
                    ev.reply(dpp::ir_update_message, make_rocket_play_msg(rg));
                }
            }
        }
        return;
    }

    // ── 卷軸按鈕 ─────────────────────────────────────────────────────────────
    if (cid.rfind("scroll_", 0) == 0) {
        auto sc_get_uid = [&]() -> dpp::snowflake {
            size_t prefix_end = cid.find('_', 7);
            size_t uid_start  = prefix_end + 1;
            size_t uid_end    = cid.find('_', uid_start);
            std::string uid_s = (uid_end == std::string::npos)
                ? cid.substr(uid_start)
                : cid.substr(uid_start, uid_end - uid_start);
            return dpp::snowflake(std::stoull(uid_s));
        };
        dpp::snowflake owner = sc_get_uid();
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的模擬器！").set_flags(dpp::m_ephemeral)); return;
        }
        if (cid.rfind("scroll_sel_", 0) == 0) {
            ev.reply(dpp::ir_update_message, make_scroll_sel_msg(uid));
        } else {
            std::string rest = cid.substr(10);
            size_t p1 = rest.find('_');
            size_t p2 = rest.find('_', p1 + 1);
            int pct   = std::stoi(rest.substr(p1 + 1, p2 - p1 - 1));
            int count = std::stoi(rest.substr(p2 + 1));
            if (count < 1) count = 1;
            if (count > 100) count = 100;
            ev.reply(dpp::ir_update_message, make_scroll_result_msg(uid, pct, count));
        }
        return;
    }

    // ── 刮刮樂按鈕 ───────────────────────────────────────────────────────────
    if (cid.rfind("sc9_", 0) == 0) {
        auto sk_get_uid = [&]() -> dpp::snowflake {
            if (cid.rfind("sc9_again_", 0) == 0) {
                std::string rest = cid.substr(10);
                size_t sep = rest.rfind('_');
                return dpp::snowflake(std::stoull(rest.substr(0, sep)));
            }
            if (cid.rfind("sc9_rev_", 0) == 0) {
                std::string rest = cid.substr(8);
                size_t sep = rest.rfind('_');
                return dpp::snowflake(std::stoull(rest.substr(0, sep)));
            }
            size_t us = cid.rfind('_');
            return dpp::snowflake(std::stoull(cid.substr(us + 1)));
        };
        dpp::snowflake owner = sk_get_uid();
        if (uid != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的刮刮樂！").set_flags(dpp::m_ephemeral)); return;
        }
        if (cid.rfind("sc9_again_", 0) == 0) {
            std::string rest = cid.substr(10);
            size_t sep = rest.rfind('_');
            int64_t bet = std::stoll(rest.substr(sep + 1));
            if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 無效下注！").set_flags(dpp::m_ephemeral)); return; }
            ev.reply(dpp::ir_channel_message_with_source,
                handle_scratch_start(uid, ev.command.channel_id, bet,
                    user.get_avatar_url(), ev.command.member.get_nickname()));
        } else {
            ScratchGame g;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = scratch_games.find(uid);
                if (it == scratch_games.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                }
                g = it->second;
            }
            if (cid.rfind("sc9_cash_", 0) == 0) {
                { std::lock_guard<std::mutex> lk(data_mutex); scratch_games.erase(uid); }
                save_scratch_games();
                ev.reply(dpp::ir_update_message, make_scratch_cash_msg(g));
            } else if (cid.rfind("sc9_early_", 0) == 0) {
                double fee_mult = (g.safe_scratches == 1) ? 0.6 : 0.3;
                int64_t fee = std::max((int64_t)1, (int64_t)(g.bet * fee_mult));
                if (get_chips(uid) < fee) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 籌碼不足，無法提前出場！").set_flags(dpp::m_ephemeral)); return;
                }
                g.total_paid += fee;
                { std::lock_guard<std::mutex> lk(data_mutex); scratch_games.erase(uid); }
                save_scratch_games();
                ev.reply(dpp::ir_update_message, make_scratch_cash_msg(g));
            } else if (cid.rfind("sc9_extra_", 0) == 0) {
                if (g.extra_count >= SK_MAX_EXTRA) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 已達追加上限！").set_flags(dpp::m_ephemeral)); return;
                }
                int64_t extra_cost = std::max((int64_t)1, g.bet / 2);
                if (get_chips(uid) < extra_cost) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 籌碼不足，無法追加刮格！").set_flags(dpp::m_ephemeral)); return;
                }
                g.total_paid += extra_cost;
                g.extra_mode = true;
                g.extra_count++;
                { std::lock_guard<std::mutex> lk(data_mutex); scratch_games[uid] = g; }
                save_scratch_games();
                ev.reply(dpp::ir_update_message, make_scratch_play_msg(g));
            } else if (cid.rfind("sc9_rev_", 0) == 0) {
                std::string rest = cid.substr(8);
                size_t sep = rest.rfind('_');
                int idx = std::stoi(rest.substr(sep + 1));
                if (idx < 0 || idx > 8 || ((g.revealed >> idx) & 1)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 無效操作！").set_flags(dpp::m_ephemeral)); return;
                }
                if (g.safe_scratches >= 3 && !g.extra_mode) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 請先選擇收手或付費多刮！").set_flags(dpp::m_ephemeral)); return;
                }
                g.revealed |= (1 << idx);
                if (g.extra_mode) g.extra_mode = false;
                if (g.sq[idx] == -1) {
                    { std::lock_guard<std::mutex> lk(data_mutex); scratch_games.erase(uid); }
                    save_scratch_games();
                    ev.reply(dpp::ir_update_message, make_scratch_bomb_msg(g, idx));
                } else {
                    g.safe_scratches++;
                    { std::lock_guard<std::mutex> lk(data_mutex); scratch_games[uid] = g; }
                    save_scratch_games();
                    ev.reply(dpp::ir_update_message, make_scratch_play_msg(g));
                }
            }
        }
        return;
    }
}

// ─── Games modal handler (猜數字) ─────────────────────────────────────────────

void handle_games_modal(const dpp::form_submit_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake issuer  = ev.command.get_issuing_user().id;

    if (cid.rfind("guess_modal_", 0) == 0) {
        dpp::snowflake owner(std::stoull(cid.substr(12)));
        if (issuer != owner) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string input;
        for (auto& row : ev.components) {
            if (std::holds_alternative<std::string>(row.value))
                input = std::get<std::string>(row.value);
            for (auto& sub : row.components)
                if (std::holds_alternative<std::string>(sub.value))
                    input = std::get<std::string>(sub.value);
        }
        while (!input.empty() && input.front() == ' ') input.erase(input.begin());
        while (!input.empty() && input.back()  == ' ') input.pop_back();

        if (!guess_valid(input)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 請輸入 4 位不重複的數字（0-9），例如 `1023`！").set_flags(dpp::m_ephemeral)); return;
        }
        bool game_over = false, won = false;
        GuessGame snap;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = guess_games.find(owner);
            if (it == guess_games.end()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            auto& g = it->second;
            g.input_buf = "";
            std::string ab = guess_calc_ab(g.secret, input);
            g.history.emplace_back(input, ab);
            g.attempts++;
            won       = (ab == "4A0B");
            game_over = won || (g.attempts >= GuessGame::MAX_ATTEMPTS);
            if (game_over) {
                auto& st = guess_stats_data[(uint64_t)owner];
                st.games++;
                if (won) { st.wins++; st.total_win_attempts += g.attempts; }
            }
            snap = g;
            if (game_over) guess_games.erase(it);
        }
        if (game_over) {
            save_guess_stats();
            auto result = make_guess_result_msg(snap, won);
            result.id = snap.msg_id; result.channel_id = snap.channel_id;
            g_bot->message_edit(result);
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message(won ? "🎉 猜中了！" : "💀 失敗！").set_flags(dpp::m_ephemeral));
        } else {
            auto upd = make_guess_msg(snap);
            upd.id = snap.msg_id; upd.channel_id = snap.channel_id;
            g_bot->message_edit(upd);
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 已記錄：`" + input + "` → **" + snap.history.back().second + "**").set_flags(dpp::m_ephemeral));
        }
        return;
    }
}

// ─── Games slash handler (射/火箭/刮刮樂/骰子) ──────────────────────────────

void handle_games_slash(const dpp::slashcommand_t& ev,
                        const std::string& cmd_name,
                        dpp::snowflake uid, dpp::snowflake ch)
{
    const auto& user = ev.command.get_issuing_user();

    auto get_bet = [&]() -> int64_t {
        auto bp = ev.get_parameter("籌碼");
        std::string bet_str = std::holds_alternative<std::string>(bp) ? std::get<std::string>(bp) : "";
        std::string bet_lo = bet_str;
        for (auto& c2 : bet_lo) c2 = (char)std::tolower((unsigned char)c2);
        return (bet_lo == "all") ? get_chips(uid) : (bet_str.empty() ? 0 : std::atoll(bet_str.c_str()));
    };

    auto check_allin_min = [&](int64_t& bet, const std::string& usage) -> bool {
        if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
            bet = get_chips(uid);
            if (bet < 5000) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return false;
            }
        } else {
            if (bet <= 0) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message(usage).set_flags(dpp::m_ephemeral)); return false;
            }
            if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此討論串最低下注為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return false;
            }
        }
        return true;
    };

    if (cmd_name == "射" || cmd_name == "inbetween") {
        int64_t bet = get_bet();
        if (!check_allin_min(bet, "用法：`/射 籌碼:100` 或 `/射 籌碼:ALL`")) return;
        ev.reply(dpp::ir_channel_message_with_source, handle_shoot_start(uid, ch, bet,
            user.get_avatar_url(), ev.command.member.get_nickname()));
        ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = shoot_games.find(uid);
                if (it != shoot_games.end())
                    it->second.msg_id = std::get<dpp::message>(cb.value).id;
            }
        });
    }
    else if (cmd_name == "火箭" || cmd_name == "rocket") {
        int64_t bet = get_bet();
        if (!check_allin_min(bet, "用法：`/火箭 籌碼:100` 或 `/火箭 籌碼:ALL`")) return;
        ev.reply(dpp::ir_channel_message_with_source,
            handle_rocket_start(uid, ch, bet,
                user.get_avatar_url(), ev.command.member.get_nickname()));
    }
    else if (cmd_name == "刮刮樂" || cmd_name == "scratch") {
        int64_t bet = get_bet();
        if (!check_allin_min(bet, "用法：`/刮刮樂 籌碼:100` 或 `/刮刮樂 籌碼:ALL`")) return;
        ev.reply(dpp::ir_channel_message_with_source,
            handle_scratch_start(uid, ch, bet,
                user.get_avatar_url(), ev.command.member.get_nickname()));
    }
    else if (cmd_name == "骰子" || cmd_name == "dice") {
        int64_t bet = get_bet();
        if (!check_allin_min(bet, "用法：`/骰子 籌碼:100` 或 `/骰子 籌碼:ALL`")) return;
        if (get_chips(uid) < bet) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 籌碼不足").set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_channel_message_with_source, start_dice(uid, ch, bet,
            user.get_avatar_url(), user.username));
    }
}
