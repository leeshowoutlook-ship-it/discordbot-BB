#include "types.h"
#include "chips.h"
#include "rl_stats.h"
#include "roulette.h"
#include "handler_decls.h"

// ─── Message: !輪盤 ───────────────────────────────────────────────────────────
void handle_roulette_message(const dpp::message_create_t& ev,
                              const std::string& content,
                              dpp::snowflake uid, dpp::snowflake ch)
{
    // 頻道已有房間
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (roulette_rooms.count(ch)) {
            dpp::message m; m.channel_id = ch;
            m.set_content("❌ 此頻道已有進行中的輪盤！");
            g_bot->message_create(m); return;
        }
    }
    // 解析 "!輪盤 [amount] [@mention]"
    const std::string prefix = "!輪盤";
    std::string rest;
    if (content.size() > prefix.size() && content[prefix.size()] == ' ')
        rest = content.substr(prefix.size() + 1);
    if (rest.empty()) {
        dpp::message m; m.channel_id = ch;
        m.set_content("用法：`!輪盤 [籌碼]` 或 `!輪盤 [籌碼] @玩家`\n例：`!輪盤 1000` 或 `!輪盤 5000 @someone`");
        g_bot->message_create(m); return;
    }
    std::istringstream iss_rl(rest);
    std::string amount_str, mention_str;
    iss_rl >> amount_str >> mention_str;
    int64_t stake_rl = 0;
    try { stake_rl = std::stoll(amount_str); } catch (...) {}
    if (stake_rl <= 0) {
        dpp::message m; m.channel_id = ch;
        m.set_content("❌ 籌碼金額必須是正整數！"); g_bot->message_create(m); return;
    }
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ci = chip_data.find(uid);
        int64_t cur_chips = (ci != chip_data.end()) ? ci->second.chips : 0;
        if (cur_chips < stake_rl) {
            dpp::message m; m.channel_id = ch;
            m.set_content("❌ 你的籌碼不足 " + std::to_string(stake_rl) + " 碼！");
            g_bot->message_create(m); return;
        }
    }
    // 解析邀請 mention
    dpp::snowflake invited_rl = 0;
    if (!mention_str.empty() && mention_str.size() > 3 && mention_str[0] == '<' && mention_str[1] == '@') {
        std::string id_s = mention_str.substr(2);
        if (id_s.back() == '>') id_s.pop_back();
        if (!id_s.empty() && id_s[0] == '!') id_s = id_s.substr(1);
        try { invited_rl = dpp::snowflake(std::stoull(id_s)); } catch (...) {}
    }
    std::string dn_rl = ev.msg.member.get_nickname();
    if (dn_rl.empty()) dn_rl = ev.msg.author.username;
    RouletteRoom rr;
    rr.channel_id = ch;
    rr.p1_uid = uid; rr.p1_name = dn_rl;
    rr.p1_avatar = ev.msg.author.get_avatar_url();
    rr.stake = stake_rl;
    rr.invited_uid = invited_rl;
    if (invited_rl != 0)
        rr.invited_name = "<@" + std::to_string((uint64_t)invited_rl) + ">";
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        roulette_rooms[ch] = rr;
    }
    dpp::message rl_msg = make_roulette_room_msg(roulette_rooms[ch]);
    rl_msg.channel_id = ch;
    g_bot->message_create(rl_msg, [ch](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (roulette_rooms.count(ch))
                roulette_rooms[ch].msg_id = std::get<dpp::message>(cb.value).id;
        }
    });
    // 10 分鐘後若房間未開始則自動解散
    dpp::timer rl_tid = g_bot->start_timer([ch](dpp::timer t) {
        std::vector<std::pair<dpp::snowflake, int64_t>> refunds;
        dpp::snowflake mid = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch);
            if (it == roulette_rooms.end() || it->second.started) return;
            mid = it->second.msg_id;
            for (auto& b : it->second.side_bets) refunds.emplace_back(b.uid, b.amount);
            roulette_rooms.erase(it);
        }
        if (!refunds.empty()) {
            { std::lock_guard<std::mutex> lk(data_mutex);
              for (auto& [bid, bamt] : refunds) chip_data[bid].chips += bamt; }
            save_chips();
        }
        if (mid && g_bot) {
            dpp::embed e; e.set_title("⌛ 輪盤房間逾時解散").set_color(0x808080)
              .set_description("10 分鐘內無人開始遊戲，房間自動解散，邊注已退還。");
            dpp::message dm; dm.id = mid; dm.channel_id = ch; dm.add_embed(e);
            g_bot->message_edit(dm);
        }
        g_bot->stop_timer(t);
    }, 600);
    { std::lock_guard<std::mutex> lk(data_mutex);
      if (roulette_rooms.count(ch)) roulette_rooms[ch].timer_id = rl_tid; }
}

// ─── Slash: /輪盤 ─────────────────────────────────────────────────────────────
void handle_roulette_slash(const dpp::slashcommand_t& ev, dpp::snowflake uid, dpp::snowflake ch)
{
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (roulette_rooms.count(ch)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 此頻道已有進行中的輪盤！").set_flags(dpp::m_ephemeral)); return;
        }
    }
    auto stake_p = ev.get_parameter("籌碼");
    int64_t stake_rl = std::holds_alternative<int64_t>(stake_p) ? std::get<int64_t>(stake_p) : 0LL;
    if (stake_rl <= 0) {
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("用法：`/輪盤 籌碼:1000` 或 `/輪盤 籌碼:5000 對象:@someone`").set_flags(dpp::m_ephemeral)); return;
    }
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ci = chip_data.find(uid);
        int64_t cur_chips = (ci != chip_data.end()) ? ci->second.chips : 0;
        if (cur_chips < stake_rl) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 你的籌碼不足 " + std::to_string(stake_rl) + " 碼！").set_flags(dpp::m_ephemeral)); return;
        }
    }
    dpp::snowflake invited_rl = 0;
    auto inv_p = ev.get_parameter("對象");
    std::string invited_name_rl;
    if (std::holds_alternative<dpp::snowflake>(inv_p)) {
        invited_rl = std::get<dpp::snowflake>(inv_p);
        auto uit = ev.command.resolved.users.find(invited_rl);
        if (uit != ev.command.resolved.users.end()) {
            auto mit = ev.command.resolved.members.find(invited_rl);
            invited_name_rl = (mit != ev.command.resolved.members.end() && !mit->second.get_nickname().empty())
                              ? mit->second.get_nickname() : uit->second.username;
        }
        if (invited_name_rl.empty())
            invited_name_rl = "<@" + std::to_string((uint64_t)invited_rl) + ">";
    }
    std::string dn_rl = ev.command.member.get_nickname();
    if (dn_rl.empty()) dn_rl = ev.command.get_issuing_user().username;
    RouletteRoom rr;
    rr.channel_id = ch;
    rr.p1_uid = uid; rr.p1_name = dn_rl;
    rr.p1_avatar = ev.command.get_issuing_user().get_avatar_url();
    rr.stake = stake_rl;
    rr.invited_uid = invited_rl;
    rr.invited_name = invited_name_rl;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        roulette_rooms[ch] = rr;
    }
    ev.reply(dpp::ir_channel_message_with_source, make_roulette_room_msg(roulette_rooms[ch]));
    ev.get_original_response([ch](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (roulette_rooms.count(ch))
                roulette_rooms[ch].msg_id = std::get<dpp::message>(cb.value).id;
        }
    });
    // 10 分鐘後若房間未開始則自動解散
    dpp::timer rl_tid2 = g_bot->start_timer([ch](dpp::timer t) {
        std::vector<std::pair<dpp::snowflake, int64_t>> refunds;
        dpp::snowflake mid = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch);
            if (it == roulette_rooms.end() || it->second.started) return;
            mid = it->second.msg_id;
            for (auto& b : it->second.side_bets) refunds.emplace_back(b.uid, b.amount);
            roulette_rooms.erase(it);
        }
        if (!refunds.empty()) {
            { std::lock_guard<std::mutex> lk(data_mutex);
              for (auto& [bid, bamt] : refunds) chip_data[bid].chips += bamt; }
            save_chips();
        }
        if (mid && g_bot) {
            dpp::embed e; e.set_title("⌛ 輪盤房間逾時解散").set_color(0x808080)
              .set_description("10 分鐘內無人開始遊戲，房間自動解散，邊注已退還。");
            dpp::message dm; dm.id = mid; dm.channel_id = ch; dm.add_embed(e);
            g_bot->message_edit(dm);
        }
        g_bot->stop_timer(t);
    }, 600);
    { std::lock_guard<std::mutex> lk(data_mutex);
      if (roulette_rooms.count(ch)) roulette_rooms[ch].timer_id = rl_tid2; }
}

// ─── Button: rl_* ─────────────────────────────────────────────────────────────
void handle_roulette_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    const dpp::user&   user = ev.command.get_issuing_user();
    dpp::snowflake     uid  = user.id;

    auto rl_err = [&](const std::string& msg) {
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message(msg).set_flags(dpp::m_ephemeral));
    };

    // ── 加入房間 ──────────────────────────────────────────────────────────────
    if (cid.rfind("rl_join_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(8)));
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = roulette_rooms.find(ch);
        if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
        auto& r = it->second;
        if (r.started)          { rl_err("❌ 遊戲已開始！"); return; }
        if (r.p2_uid != 0)      { rl_err("❌ 房間已有兩名玩家！"); return; }
        if (uid == r.p1_uid)    { rl_err("❌ 你已是玩家一！"); return; }
        if (r.invited_uid != 0 && uid != r.invited_uid) { rl_err("❌ 此房間為邀請制！"); return; }
        std::string dn = ev.command.member.get_nickname().empty()
                       ? user.username : ev.command.member.get_nickname();
        r.p2_uid = uid; r.p2_name = dn; r.p2_avatar = user.get_avatar_url();
        ev.reply(dpp::ir_update_message, make_roulette_room_msg(r));
    }
    // ── 調整籌碼量（Modal）────────────────────────────────────────────────────
    else if (cid.rfind("rl_stake_", 0) == 0) {
        std::string ch_s = cid.substr(9);
        dpp::snowflake ch(std::stoull(ch_s));
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch);
            if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
            if (uid != it->second.p1_uid)   { rl_err("❌ 只有玩家一（先手）可調整籌碼量！"); return; }
            if (it->second.started)          { rl_err("❌ 遊戲已開始！"); return; }
        }
        std::string uid_s = std::to_string((uint64_t)uid);
        dpp::interaction_modal_response modal("rl_stake_m_" + ch_s + "_" + uid_s, "調整對賭籌碼");
        modal.add_component(dpp::component()
            .set_type(dpp::cot_text).set_id("amount")
            .set_label("籌碼量（正整數）")
            .set_min_length(1).set_max_length(15)
            .set_required(true).set_text_style(dpp::text_short));
        ev.dialog(modal);
    }
    // ── 開始遊戲 ──────────────────────────────────────────────────────────────
    else if (cid.rfind("rl_start_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(9)));
        dpp::message game_msg;
        dpp::snowflake room_msg_id = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch);
            if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
            auto& r = it->second;
            if (uid != r.p1_uid)  { rl_err("❌ 只有玩家一（先手）可開始遊戲！"); return; }
            if (r.p2_uid == 0)    { rl_err("❌ 尚未有第二名玩家！"); return; }
            if (r.started)        { rl_err("❌ 遊戲已開始！"); return; }
            auto c1 = chip_data.find(r.p1_uid), c2 = chip_data.find(r.p2_uid);
            if (c1 == chip_data.end() || c1->second.chips < r.stake) { rl_err("❌ 玩家一籌碼不足！"); return; }
            if (c2 == chip_data.end() || c2->second.chips < r.stake) { rl_err("❌ 玩家二籌碼不足！"); return; }
            g_bot->stop_timer(r.timer_id);
            r.started = true;
            r.bullet_chamber = rl_rand(1, 6);
            r.current_chamber = 1;
            r.active_player = 1;
            game_msg    = make_roulette_game_msg(r);
            room_msg_id = r.msg_id;
        }
        // 發新訊息視窗（不更新舊房間訊息）
        ev.reply(dpp::ir_channel_message_with_source, game_msg);
        // 把舊房間訊息改成「遊戲進行中」並移除按鈕
        if (room_msg_id != 0 && g_bot) {
            dpp::embed e;
            e.set_title("🎲  俄羅斯輪盤 — 遊戲進行中").set_color(0xE74C3C);
            e.set_description("已開始遊戲，請看下方訊息繼續操作。");
            dpp::message dm; dm.id = room_msg_id; dm.channel_id = ch; dm.add_embed(e);
            g_bot->message_edit(dm);
        }
    }
    // ── 解散房間 ──────────────────────────────────────────────────────────────
    else if (cid.rfind("rl_dissolve_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(12)));
        std::vector<std::pair<dpp::snowflake, int64_t>> refunds;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch);
            if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
            if (uid != it->second.p1_uid)   { rl_err("❌ 只有玩家一可解散房間！"); return; }
            if (it->second.started)          { rl_err("❌ 遊戲進行中無法解散！"); return; }
            g_bot->stop_timer(it->second.timer_id);
            for (auto& b : it->second.side_bets) refunds.emplace_back(b.uid, b.amount);
            roulette_rooms.erase(it);
        }
        if (!refunds.empty()) {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                for (auto& [bid, bamt] : refunds) chip_data[bid].chips += bamt;
            }
            save_chips();
        }
        dpp::embed e;
        e.set_title("🗑  房間已解散").set_color(0x808080)
         .set_description("俄羅斯輪盤房間已解散，邊注已退還。");
        ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
    }
    // ── 旁觀者下注按鈕 ────────────────────────────────────────────────────────
    else if (cid.rfind("rl_bet_p1_",   0) == 0 || cid.rfind("rl_bet_p2_",   0) == 0 ||
             cid.rfind("rl_bet_odd_",  0) == 0 || cid.rfind("rl_bet_even_", 0) == 0) {
        std::string bet_type, ch_s;
        if      (cid.rfind("rl_bet_p1_",   0) == 0) { bet_type = "p1";   ch_s = cid.substr(10); }
        else if (cid.rfind("rl_bet_p2_",   0) == 0) { bet_type = "p2";   ch_s = cid.substr(10); }
        else if (cid.rfind("rl_bet_odd_",  0) == 0) { bet_type = "odd";  ch_s = cid.substr(11); }
        else                                          { bet_type = "even"; ch_s = cid.substr(12); }
        dpp::snowflake ch(std::stoull(ch_s));
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch);
            if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
            if (it->second.started)          { rl_err("❌ 遊戲已開始，不可下注！"); return; }
            if (uid == it->second.p1_uid || uid == it->second.p2_uid) {
                rl_err("❌ 參賽玩家不可下注！"); return; }
        }
        rl_open_bet_modal(ev, bet_type, ch_s);
    }
    // ── 開自己一槍 ────────────────────────────────────────────────────────────
    else if (cid.rfind("rl_shoot_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(9)));
        bool game_ended = false;
        dpp::message reply_msg;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch);
            if (it == roulette_rooms.end()) { rl_err("❌ 遊戲不存在！"); return; }
            auto& r = it->second;
            if (!r.started || r.game_over) { rl_err("❌ 遊戲未進行中！"); return; }
            dpp::snowflake active_uid = (r.active_player == 1) ? r.p1_uid : r.p2_uid;
            if (uid != active_uid)    { rl_err("❌ 現在不是你的回合！"); return; }
            if (rl_shoot_disabled(r)) { rl_err("❌ 已射過第 5 發，這發只能 PASS！"); return; }
            r.shots_this_turn++;
            if (r.current_chamber == 5) r.shot5_shooter = r.active_player;
            if (r.current_chamber == r.bullet_chamber) {
                r.loser = r.active_player;
                r.game_over = true;
                dpp::snowflake winner_uid = (r.loser == 1) ? r.p2_uid : r.p1_uid;
                dpp::snowflake loser_uid  = (r.loser == 1) ? r.p1_uid : r.p2_uid;
                int64_t winner_net = (int64_t)(r.stake * 0.95);
                chip_data[winner_uid].chips += winner_net;
                chip_data[loser_uid].chips  -= r.stake;
                // 主賽統計
                roulette_stats_data[winner_uid].wins++;
                roulette_stats_data[winner_uid].profit += winner_net;
                roulette_stats_data[loser_uid].losses++;
                roulette_stats_data[loser_uid].profit -= r.stake;
                // 邊注結算（不計入勝負統計）
                for (auto& b : r.side_bets) {
                    bool bet_won = rl_bet_wins(b.bet_type, r.bullet_chamber, r.loser);
                    if (bet_won)
                        chip_data[b.uid].chips += (int64_t)(b.amount * rl_multiplier(b.bet_type));
                }
                reply_msg = make_roulette_result_msg(r);
                roulette_rooms.erase(it);
                game_ended = true;
            } else {
                r.current_chamber++;
                reply_msg = make_roulette_game_msg(r);
            }
        } // mutex released
        if (game_ended) { save_chips(); save_roulettestats(); }
        ev.reply(dpp::ir_update_message, reply_msg);
    }
    // ── PASS ──────────────────────────────────────────────────────────────────
    else if (cid.rfind("rl_pass_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(8)));
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = roulette_rooms.find(ch);
        if (it == roulette_rooms.end()) { rl_err("❌ 遊戲不存在！"); return; }
        auto& r = it->second;
        if (!r.started || r.game_over) { rl_err("❌ 遊戲未進行中！"); return; }
        dpp::snowflake active_uid = (r.active_player == 1) ? r.p1_uid : r.p2_uid;
        if (uid != active_uid) { rl_err("❌ 現在不是你的回合！"); return; }
        if (!rl_shoot_disabled(r) && !rl_can_pass(r)) {
            rl_err("❌ 需要先開自己一槍才能 PASS！"); return; }
        r.shots_this_turn = 0;
        r.active_player   = (r.active_player == 1) ? 2 : 1;
        ev.reply(dpp::ir_update_message, make_roulette_game_msg(r));
    }
    else if (cid.rfind("rl_refresh_", 0) == 0) {
        dpp::snowflake ch(std::stoull(cid.substr(11)));
        dpp::message fresh;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch);
            if (it == roulette_rooms.end()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 遊戲不存在或已結束！").set_flags(dpp::m_ephemeral)); return;
            }
            auto& r = it->second;
            fresh = r.started ? make_roulette_game_msg(r) : make_roulette_room_msg(r);
        }
        // 把舊訊息標記為過期，另外發一則新訊息（跳到頻道最新位置）
        dpp::embed se; se.set_title("🔄 訊息已刷新").set_color(0x95A5A6)
                          .set_description("請往下滑查看最新狀態！");
        ev.reply(dpp::ir_update_message, dpp::message().add_embed(se));
        fresh.channel_id = ch;
        g_bot->message_create(fresh, [ch](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = roulette_rooms.find(ch);
                if (it != roulette_rooms.end())
                    it->second.msg_id = std::get<dpp::message>(cb.value).id;
            }
        });
    }
    else {
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 未知的輪盤按鈕！").set_flags(dpp::m_ephemeral));
    }
}

// ─── Modal: rl_stake_m_ / rl_bet_m_ ──────────────────────────────────────────
void handle_roulette_modal(const dpp::form_submit_t& ev)
{
    const std::string& cid    = ev.custom_id;
    dpp::snowflake     issuer = ev.command.get_issuing_user().id;

    auto get_text = [&]() -> std::string {
        for (auto& row : ev.components) {
            if (std::holds_alternative<std::string>(row.value))
                return std::get<std::string>(row.value);
            for (auto& sub : row.components)
                if (std::holds_alternative<std::string>(sub.value))
                    return std::get<std::string>(sub.value);
        }
        return "";
    };

    // ── 調整籌碼 Modal ──────────────────────────────────────────────────────
    if (cid.rfind("rl_stake_m_", 0) == 0) {
        std::string rest_s = cid.substr(11);
        size_t up = rest_s.rfind('_');
        if (up == std::string::npos) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 格式錯誤！").set_flags(dpp::m_ephemeral)); return;
        }
        dpp::snowflake ch_s(std::stoull(rest_s.substr(0, up)));
        dpp::snowflake modal_uid(std::stoull(rest_s.substr(up + 1)));
        if (issuer != modal_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 非你操作！").set_flags(dpp::m_ephemeral)); return;
        }
        int64_t new_stake = 0;
        try { new_stake = std::stoll(get_text()); } catch (...) {}
        if (new_stake <= 0) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 籌碼必須是正整數！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch_s);
            if (it == roulette_rooms.end() || it->second.started) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 房間已不存在或遊戲已開始！").set_flags(dpp::m_ephemeral)); return;
            }
            auto ciss = chip_data.find(issuer);
            if (ciss == chip_data.end() || ciss->second.chips < new_stake) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你的籌碼不足！").set_flags(dpp::m_ephemeral)); return;
            }
            it->second.stake = new_stake;
            if (it->second.msg_id) {
                auto upd = make_roulette_room_msg(it->second);
                upd.id = it->second.msg_id; upd.channel_id = ch_s;
                g_bot->message_edit(upd);
            }
        }
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("✅ 籌碼已更新為 **" + std::to_string(new_stake) + "** 碼！").set_flags(dpp::m_ephemeral));
    }
    // ── 旁觀者下注 Modal ────────────────────────────────────────────────────
    else if (cid.rfind("rl_bet_m_", 0) == 0) {
        std::string rest_s = cid.substr(9);
        size_t p1 = rest_s.find('_');
        if (p1 == std::string::npos) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 格式錯誤！").set_flags(dpp::m_ephemeral)); return;
        }
        std::string bet_type = rest_s.substr(0, p1);
        std::string rest2    = rest_s.substr(p1 + 1);
        size_t p2 = rest2.rfind('_');
        if (p2 == std::string::npos) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 格式錯誤！").set_flags(dpp::m_ephemeral)); return;
        }
        dpp::snowflake ch_s(std::stoull(rest2.substr(0, p2)));
        dpp::snowflake modal_uid(std::stoull(rest2.substr(p2 + 1)));
        if (issuer != modal_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 非你操作！").set_flags(dpp::m_ephemeral)); return;
        }
        int64_t bet_amt = 0;
        try { bet_amt = std::stoll(get_text()); } catch (...) {}
        if (bet_amt <= 0) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 下注金額必須是正整數！").set_flags(dpp::m_ephemeral)); return;
        }
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = roulette_rooms.find(ch_s);
            if (it == roulette_rooms.end() || it->second.started) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 房間已不存在或遊戲已開始！").set_flags(dpp::m_ephemeral)); return;
            }
            if (issuer == it->second.p1_uid || issuer == it->second.p2_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 參賽玩家不可下注！").set_flags(dpp::m_ephemeral)); return;
            }
            auto cbet = chip_data.find(issuer);
            if (cbet == chip_data.end() || cbet->second.chips < bet_amt) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 籌碼不足！").set_flags(dpp::m_ephemeral)); return;
            }
            chip_data[issuer].chips -= bet_amt;
            std::string dn_bet = ev.command.member.get_nickname().empty()
                               ? ev.command.get_issuing_user().username
                               : ev.command.member.get_nickname();
            RouletteSideBet sb; sb.uid = issuer; sb.display_name = dn_bet;
            sb.bet_type = bet_type; sb.amount = bet_amt;
            it->second.side_bets.push_back(sb);
            if (it->second.msg_id) {
                auto upd = make_roulette_room_msg(it->second);
                upd.id = it->second.msg_id; upd.channel_id = ch_s;
                g_bot->message_edit(upd);
            }
        }
        save_chips(); // outside data_mutex lock
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("✅ 已下注 **" + std::to_string(bet_amt) + "** 碼（" + rl_bet_label(bet_type) + "）！").set_flags(dpp::m_ephemeral));
    }
}

// ─── Select: rl_ch_sel_ ───────────────────────────────────────────────────────
void handle_roulette_select(const dpp::select_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake     uid = ev.command.get_issuing_user().id;

    if (ev.values.empty()) return;
    std::string ch_s = cid.substr(10);  // "rl_ch_sel_" = 10 chars
    dpp::snowflake ch(std::stoull(ch_s));
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = roulette_rooms.find(ch);
        if (it == roulette_rooms.end()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 房間已不存在！").set_flags(dpp::m_ephemeral)); return;
        }
        if (it->second.started) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 遊戲已開始，不可下注！").set_flags(dpp::m_ephemeral)); return;
        }
        if (uid == it->second.p1_uid || uid == it->second.p2_uid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 參賽玩家不可下注！").set_flags(dpp::m_ephemeral)); return;
        }
    }
    std::string chamber_num = ev.values[0];
    std::string bet_type    = "ch" + chamber_num;
    std::string uid_s       = std::to_string((uint64_t)uid);
    dpp::interaction_modal_response modal(
        "rl_bet_m_" + bet_type + "_" + ch_s + "_" + uid_s,
        "下注：第" + chamber_num + "發 ×5.5");
    modal.add_component(dpp::component()
        .set_type(dpp::cot_text).set_id("amount")
        .set_label("下注金額（籌碼）")
        .set_min_length(1).set_max_length(15)
        .set_required(true).set_text_style(dpp::text_short));
    ev.dialog(modal);
}
