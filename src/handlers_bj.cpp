#include "types.h"
#include "chips.h"
#include "blackjack.h"
#include "handler_decls.h"

// ─── !21 / /21 共用：開局邏輯 ────────────────────────────────────────────────

struct BJStartResult { BJGame game; std::string status; };

static BJStartResult bj_start_game(dpp::snowflake uid, dpp::snowflake ch, int64_t bet,
                                    const std::string& avatar, const std::string& uname)
{
    // Kill any existing game for this player
    BJGame old_g; bool had_old = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = user_bj.find(uid);
        if (it != user_bj.end()) {
            auto git = bj_games.find(it->second);
            if (git != bj_games.end()) { old_g = git->second; had_old = true; }
            bj_games.erase(it->second); user_bj.erase(it);
        }
    }
    if (had_old) bj_disable_old_msg(*g_bot, old_g);

    add_chips(uid, -bet);
    BJGame g = start_bj(uid, ch, bet, avatar, uname);

    std::string status;
    if (is_blackjack(g.main_hand.cards)) {
        bool dbj = is_blackjack(g.dealer_cards);
        g.game_over = true;
        if (dbj) {
            add_chips(uid, bet);
            status = "雙 BJ — 平局！";
            { std::lock_guard<std::mutex> lk(data_mutex); bj_stats_data[uid].pushes++; }
        } else {
            int64_t win = (int64_t)(bet * 1.5);
            add_chips(uid, bet + win);
            status = "🌟 Blackjack！贏得 **" + std::to_string(win) + "** 碼！";
            { std::lock_guard<std::mutex> lk(data_mutex); bj_stats_data[uid].wins++; bj_stats_data[uid].profit += win; }
        }
        save_bjstats();
    }
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        uint64_t gid = g.id;
        bj_games[gid] = g;
        user_bj[uid]  = gid;
    }
    save_bj_games();
    return {g, status};
}

// ─── !21 訊息指令 ────────────────────────────────────────────────────────────

void handle_bj_message(const dpp::message_create_t& ev, const std::string& content,
                        dpp::snowflake uid, dpp::snowflake ch)
{
    std::string rest = content.size() > 4 ? content.substr(4) : "";
    std::string rest_lo = rest; for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
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
            dpp::message m; m.set_content("用法：`!21 <籌碼量>`  例：`!21 100` 或 `!21 ALL`");
            m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
        }
        if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
            dpp::message m; m.set_content("❌ 此討論串最低下注為 **1,000** 碼！");
            m.set_reference(ev.msg.id); m.channel_id = ch; g_bot->message_create(m); return;
        }
    }
    int64_t bal = get_chips(uid);
    if (bal < bet) {
        dpp::embed e; e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        e.set_description("你持有 **" + std::to_string(bal) + "** 碼，無法下注 **" + std::to_string(bet) + "** 碼。");
        dpp::message m; m.add_embed(e);
        m.set_reference(ev.msg.id); m.channel_id = ch;
        g_bot->message_create(m); return;
    }

    auto [g, status] = bj_start_game(uid, ch, bet, ev.msg.author.get_avatar_url(), ev.msg.author.username);
    dpp::message bj_msg = make_bj_msg(g, status);
    bj_msg.set_reference(ev.msg.id); bj_msg.channel_id = ch;
    g_bot->message_create(bj_msg, [uid, gid = g.id](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            auto& m = std::get<dpp::message>(cb.value);
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = bj_games.find(gid);
              if (it != bj_games.end()) it->second.msg_id = m.id; }
            save_bj_games();
        }
    });
}

// ─── BJ 按鈕 ─────────────────────────────────────────────────────────────────

void handle_bj_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;

    // ── 再來一局 ─────────────────────────────────────────────────────────────
    if (cid.rfind("bj_again_", 0) == 0) {
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
        const dpp::user& u = ev.command.get_issuing_user();
        auto [g, status] = bj_start_game(uid, ev.command.channel_id, bet, u.get_avatar_url(), u.username);
        ev.reply(dpp::ir_channel_message_with_source, make_bj_msg(g, status));
        ev.get_original_response([uid, gid = g.id](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                auto& m = std::get<dpp::message>(cb.value);
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = bj_games.find(gid);
                  if (it != bj_games.end()) it->second.msg_id = m.id;
                  msg_owner[m.id] = uid; }
                save_bj_games();
            }
        });
        return;
    }

    // ── 遊戲操作（要牌/停牌/加倍/分牌）──────────────────────────────────────
    if (cid.rfind("bj_", 0) == 0) {
        size_t p1 = cid.find('_', 3);
        if (p1 == std::string::npos) return;
        std::string action = cid.substr(3, p1 - 3);
        uint64_t gid = std::stoull(cid.substr(p1 + 1));
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = bj_games.find(gid);
            if (it == bj_games.end()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("⚠️ 遊戲已結束。").set_flags(dpp::m_ephemeral)); return;
            }
            if (it->second.user_id != uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
        }
        // 先 ack 避免 Discord 3 秒 timeout（存檔 I/O 可能耗時）
        ev.reply(dpp::ir_deferred_update_message, dpp::message());
        dpp::message updated = handle_bj_button(action, gid, uid);
        if (updated.embeds.empty()) {
            ev.edit_original_response(dpp::message("⚠️ 遊戲不存在。")); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = bj_games.find(gid);
            if (it != bj_games.end() && it->second.game_over) {
                user_bj.erase(it->second.user_id);
                bj_games.erase(it);
            }
        }
        ev.edit_original_response(updated);
    }
}

// ─── /21 slash 指令 ──────────────────────────────────────────────────────────

void handle_bj_slash(const dpp::slashcommand_t& ev, const std::string& cmd_name,
                     dpp::snowflake uid, dpp::snowflake ch)
{
    (void)cmd_name;
    int64_t bet = 0;
    auto bp = ev.get_parameter("籌碼");
    std::string bet_str = std::holds_alternative<std::string>(bp) ? std::get<std::string>(bp) : "";
    std::string bet_lo = bet_str; for (auto& c2 : bet_lo) c2 = (char)std::tolower((unsigned char)c2);
    bool is_all = (bet_lo == "all");
    bet = is_all ? get_chips(uid) : (bet_str.empty() ? 0 : std::atoll(bet_str.c_str()));

    if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
        bet = get_chips(uid);
        if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
    } else {
        if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("用法：`/21 籌碼:100` 或 `/21 籌碼:ALL`").set_flags(dpp::m_ephemeral)); return; }
        if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 此討論串最低下注為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return; }
    }
    int64_t bal = get_chips(uid);
    if (bal < bet) {
        dpp::embed e; e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        e.set_description("你持有 **" + std::to_string(bal) + "** 碼，無法下注 **" + std::to_string(bet) + "** 碼。");
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message().add_embed(e).set_flags(dpp::m_ephemeral)); return;
    }

    const dpp::user& u = ev.command.get_issuing_user();
    auto [g, status] = bj_start_game(uid, ch, bet, u.get_avatar_url(), u.username);
    ev.reply(dpp::ir_channel_message_with_source, make_bj_msg(g, status));
    ev.get_original_response([uid, gid = g.id](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            auto& m = std::get<dpp::message>(cb.value);
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = bj_games.find(gid);
              if (it != bj_games.end()) it->second.msg_id = m.id; }
            save_bj_games();
        }
    });
}
