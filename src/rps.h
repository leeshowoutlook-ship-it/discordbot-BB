#pragma once
#include "types.h"
#include "chips.h"
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

// ── 常數 ──────────────────────────────────────────────────────────────────────
static const char* RPS_NAME[] = {"", "石頭", "剪刀", "布"};
static const char* RPS_EMO[]  = {"", "✊", "✌️", "🖐️"};
static const std::string RPSSTATS_FILE = "rpsstats.json";

// ── 持久化 ────────────────────────────────────────────────────────────────────
static void save_rps_stats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : rps_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"wins", s.wins}, {"losses", s.losses}, {"profit", s.profit}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(RPSSTATS_FILE, j.dump(2));
}

static void load_rps_stats() {
    std::ifstream f(RPSSTATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = rps_stats_data[uid];
            s.wins   = v.value("wins",   0);
            s.losses = v.value("losses", 0);
            s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}

// ── 決定贏家（空 vector = 平手） ──────────────────────────────────────────────
static std::vector<dpp::snowflake> rps_winners(
    const std::map<dpp::snowflake, int>& choices)
{
    std::set<int> used;
    for (auto& [uid, c] : choices) if (c > 0) used.insert(c);
    if (used.size() != 2) return {};  // 全同 or 三種 → 平手

    int a = *used.begin(), b = *used.rbegin();
    int win = -1;
    if      ((a==1&&b==2)||(a==2&&b==1)) win = 1;  // 石頭 > 剪刀
    else if ((a==2&&b==3)||(a==3&&b==2)) win = 2;  // 剪刀 > 布
    else                                 win = 3;  // 布 > 石頭
    std::vector<dpp::snowflake> ws;
    for (auto& [uid, c] : choices) if (c == win) ws.push_back(uid);
    return ws;
}

// ── 統計字串（供 wallet.h 用） ────────────────────────────────────────────────
static std::string rps_stats_line(dpp::snowflake uid) {
    int w = 0, l = 0; int64_t profit = 0;
    {
        auto it = rps_stats_data.find(uid);
        if (it != rps_stats_data.end()) {
            w = it->second.wins; l = it->second.losses; profit = it->second.profit;
        }
    }
    int total = w + l;
    if (total == 0) return "尚無紀錄";
    char buf[16]; snprintf(buf, sizeof(buf), "%.1f%%", w * 100.0 / total);
    return "勝/負 **" + std::to_string(w) + "/" + std::to_string(l) + "**"
         + "　勝率 **" + buf + "**"
         + "\n盈虧 **" + (profit >= 0 ? "+" : "") + std::to_string(profit) + " 碼**";
}

// ── 訊息建構 ──────────────────────────────────────────────────────────────────
static dpp::message make_rps_lobby_msg(const RpsGame& g) {
    dpp::embed e;
    e.set_title("✊✌️🖐️  猜拳 — 等待玩家").set_color(0x3498DB);
    std::string desc = "**房主：** <@" + std::to_string((uint64_t)g.host_uid) + ">\n";
    desc += "**賭注：** " + std::to_string(g.bet) + " 碼 / 人\n\n";
    desc += "**玩家（" + std::to_string(g.players.size()) + "/5）：**\n";
    for (auto& [uid, name] : g.players) desc += "• " + name + "\n";
    desc += "\n房主點「開始遊戲」（需 2~5 人）。";
    e.set_description(desc);

    std::string ch_s = std::to_string((uint64_t)g.channel_id);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("加入").set_id("rps_join_" + ch_s).set_style(dpp::cos_success)
        .set_disabled((int)g.players.size() >= 5));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("離開").set_id("rps_leave_" + ch_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("開始遊戲").set_id("rps_start_" + ch_s).set_style(dpp::cos_primary)
        .set_disabled((int)g.players.size() < 2));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 重整").set_id("rps_refresh_" + ch_s).set_style(dpp::cos_secondary));
    dpp::message msg; msg.add_embed(e); msg.add_component(row); return msg;
}

static dpp::message make_rps_choosing_msg(const RpsGame& g) {
    dpp::embed e;
    e.set_title("✊✌️🖐️  出拳中…").set_color(0xF39C12);
    std::string desc = "**賭注：** " + std::to_string(g.bet) + " 碼 / 人\n\n";
    desc += "**出拳狀況：**\n";
    for (auto& [uid, name] : g.players) {
        bool done = (g.choices.count(uid) && g.choices.at(uid) != 0);
        desc += (done ? "✅ " : "⏳ ") + name + "\n";
    }
    desc += "\n點「出拳」秘密選擇，所有人選完後同時公布！";
    e.set_description(desc);

    std::string ch_s = std::to_string((uint64_t)g.channel_id);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✊ 出拳").set_id("rps_pick_" + ch_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 重整").set_id("rps_refresh_" + ch_s).set_style(dpp::cos_secondary));
    dpp::message msg; msg.add_embed(e); msg.add_component(row); return msg;
}

static dpp::message make_rps_draw_msg(const RpsGame& g) {
    dpp::embed e;
    e.set_title("✊✌️🖐️  平手！").set_color(0x95A5A6);
    std::string desc = "**賭注：** " + std::to_string(g.bet) + " 碼 / 人\n\n";
    desc += "**出拳結果：**\n";
    for (auto& [uid, name] : g.players) {
        int c = (g.choices.count(uid) ? g.choices.at(uid) : 0);
        desc += std::string(c > 0 ? RPS_EMO[c] : "❓") + " **" + name + "**\n";
    }
    desc += "\n**投票狀況：**\n";
    for (auto& [uid, name] : g.players) {
        auto vi = g.draw_votes.find(uid);
        if (vi == g.draw_votes.end()) desc += "⏳ " + name + "\n";
        else desc += (vi->second ? "🔄 " : "🚪 ") + name + "\n";
    }
    desc += "\n點「投票」決定是否再來一把。";
    e.set_description(desc);

    std::string ch_s = std::to_string((uint64_t)g.channel_id);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("投票").set_id("rps_vote_" + ch_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 重整").set_id("rps_refresh_" + ch_s).set_style(dpp::cos_secondary));
    dpp::message msg; msg.add_embed(e); msg.add_component(row); return msg;
}

static dpp::message make_rps_settled_msg(
    const RpsGame& g,
    const std::vector<dpp::snowflake>& winners,
    int64_t per_winner)
{
    dpp::embed e;
    e.set_title("✊✌️🖐️  結算").set_color(0x2ECC71);
    std::string desc = "**賭注：** " + std::to_string(g.bet) + " 碼 / 人\n\n";
    desc += "**出拳結果：**\n";
    for (auto& [uid, name] : g.players) {
        int c = (g.choices.count(uid) ? g.choices.at(uid) : 0);
        bool is_w = (std::find(winners.begin(), winners.end(), uid) != winners.end());
        desc += std::string(c > 0 ? RPS_EMO[c] : "❓") + " ";
        if (is_w) desc += "👑 **" + name + "**";
        else      desc += name;
        desc += "\n";
    }
    if (!winners.empty()) {
        int wc = (g.choices.count(winners[0]) ? g.choices.at(winners[0]) : 0);
        if (wc > 0) {
            desc += "\n勝出：" + std::string(RPS_EMO[wc]) + " **" + RPS_NAME[wc] + "**\n";
        }
        desc += "🏆 每位贏家獲得 **" + std::to_string(per_winner) + "** 碼！";
    }
    e.set_description(desc);
    e.set_footer(dpp::embed_footer().set_text("平台抽成 2%"));

    std::string ch_s = std::to_string((uint64_t)g.channel_id);
    int64_t dbl = g.bet * 2;
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("再來一局（" + std::to_string(g.bet) + " 碼）")
        .set_id("rps_again_" + ch_s + "_" + std::to_string(g.bet))
        .set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("雙倍下注（" + std::to_string(dbl) + " 碼）")
        .set_id("rps_again_" + ch_s + "_" + std::to_string(dbl))
        .set_style(dpp::cos_danger));
    dpp::message msg; msg.add_embed(e); msg.add_component(row); return msg;
}

// ── 結算（所有人選完後呼叫，在 data_mutex 外） ────────────────────────────────
static void rps_do_reveal(dpp::snowflake ch_id) {
    RpsGame g;
    { std::lock_guard<std::mutex> lk(data_mutex); g = rps_games[ch_id]; }

    // 舊的「出拳中」訊息標記過期，結果另開新訊息
    dpp::embed stale_e;
    stale_e.set_title("✊✌️🖐️  結果已公布").set_color(0x808080);
    stale_e.set_description("請往下滑查看結果！");
    dpp::message stale; stale.id = g.message_id; stale.channel_id = g.channel_id;
    stale.add_embed(stale_e);
    g_bot->message_edit(stale);

    auto winners = rps_winners(g.choices);
    bool is_draw = winners.empty();

    if (!is_draw) {
        int64_t n_players = (int64_t)g.players.size();
        int64_t n_winners = (int64_t)winners.size();
        int64_t per_winner = (int64_t)std::ceil(g.bet * n_players * 0.98 / n_winners);

        {
            std::lock_guard<std::mutex> lk(data_mutex);
            for (auto uid : winners) {
                chip_data[uid].chips += per_winner;
                rps_stats_data[uid].wins++;
                rps_stats_data[uid].profit += (per_winner - g.bet);
            }
            for (auto& [uid, name] : g.players) {
                if (std::find(winners.begin(), winners.end(), uid) == winners.end()) {
                    rps_stats_data[uid].losses++;
                    rps_stats_data[uid].profit -= g.bet;
                }
            }
            rps_games.erase(ch_id);
        }
        save_chips();
        save_rps_stats();

        auto pub = make_rps_settled_msg(g, winners, per_winner);
        pub.channel_id = g.channel_id;
        g_bot->message_create(pub);
    } else {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            rps_games[ch_id].draw_state = true;
        }
        RpsGame gu;
        { std::lock_guard<std::mutex> lk(data_mutex); gu = rps_games[ch_id]; }
        auto pub = make_rps_draw_msg(gu);
        pub.channel_id = g.channel_id;
        g_bot->message_create(pub, [ch_id](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = rps_games.find(ch_id);
                if (it != rps_games.end())
                    it->second.message_id = std::get<dpp::message>(cb.value).id;
            }
        });
    }
}

// ── 平手投票結果（所有人投完後呼叫，在 data_mutex 外） ────────────────────────
static void rps_process_draw_result(dpp::snowflake ch_id) {
    RpsGame g;
    { std::lock_guard<std::mutex> lk(data_mutex); g = rps_games[ch_id]; }

    // 舊的投票訊息標記過期，結果另開新訊息
    dpp::embed stale_e;
    stale_e.set_title("✊✌️🖐️  投票結束").set_color(0x808080);
    stale_e.set_description("請往下滑查看最新狀態！");
    dpp::message stale; stale.id = g.message_id; stale.channel_id = g.channel_id;
    stale.add_embed(stale_e);
    g_bot->message_edit(stale);

    dpp::message pub; pub.channel_id = g.channel_id;
    bool continues = false;

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& game = rps_games[ch_id];
        // Refund everyone first
        for (auto& [uid, name] : game.players)
            chip_data[uid].chips += game.bet;
        // Remove exit voters
        for (auto& [uid, vote] : game.draw_votes)
            if (!vote) { game.players.erase(uid); game.avatars.erase(uid); game.choices.erase(uid); }
        game.draw_votes.clear();

        if ((int)game.players.size() >= 2) {
            // Re-deduct for rematch players and start new round
            for (auto& [uid, name] : game.players)
                chip_data[uid].chips -= game.bet;
            game.choices.clear();
            game.draw_state = false;
            auto cm = make_rps_choosing_msg(game);
            for (auto& em : cm.embeds)     pub.add_embed(em);
            for (auto& co : cm.components) pub.add_component(co);
            continues = true;
        } else {
            // Not enough players — end game
            rps_games.erase(ch_id);
            dpp::embed e;
            e.set_title("✊✌️🖐️  遊戲結束").set_color(0x808080);
            e.set_description("剩餘玩家不足，遊戲結束，籌碼已退還。");
            pub.add_embed(e);
        }
    }
    save_chips();
    if (continues) {
        g_bot->message_create(pub, [ch_id](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = rps_games.find(ch_id);
                if (it != rps_games.end())
                    it->second.message_id = std::get<dpp::message>(cb.value).id;
            }
        });
    } else {
        g_bot->message_create(pub);
    }
}

// ── 按鈕主路由 ────────────────────────────────────────────────────────────────
static void handle_rps_button(const dpp::button_click_t& ev) {
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;

    auto ephem = [&](const std::string& txt) {
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message(txt).set_flags(dpp::m_ephemeral));
    };

    // ── rps_join ──────────────────────────────────────────────────────────────
    if (cid.rfind("rps_join_", 0) == 0) {
        dpp::snowflake ch_id(std::stoull(cid.substr(9)));
        std::string dn = ev.command.member.get_nickname();
        if (dn.empty()) dn = ev.command.get_issuing_user().username;
        std::string av = ev.command.get_issuing_user().get_avatar_url();

        int64_t paid_bet = 0;
        dpp::message pub;
        bool ok = false;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto gi = rps_games.find(ch_id);
            if (gi == rps_games.end())         { err = "❌ 遊戲不存在或已結束。"; }
            else if (gi->second.started)        { err = "❌ 遊戲已開始，無法加入。"; }
            else if (gi->second.players.count(uid)) { err = "❌ 你已在房間內。"; }
            else if ((int)gi->second.players.size() >= 5) { err = "❌ 房間已滿（最多 5 人）。"; }
            else {
                auto ci = chip_data.find(uid);
                int64_t chips = (ci != chip_data.end()) ? ci->second.chips : 0;
                if (chips < gi->second.bet) {
                    err = "❌ 籌碼不足，需要 " + std::to_string(gi->second.bet) + " 碼才能加入。";
                } else {
                    paid_bet = gi->second.bet;
                    chip_data[uid].chips -= paid_bet;
                    gi->second.players[uid] = dn;
                    gi->second.avatars[uid] = av;
                    auto lm = make_rps_lobby_msg(gi->second);
                    pub.id = gi->second.message_id; pub.channel_id = ch_id;
                    for (auto& em : lm.embeds)     pub.add_embed(em);
                    for (auto& co : lm.components) pub.add_component(co);
                    ok = true;
                }
            }
        }
        if (!ok) { ephem(err); return; }
        save_chips();
        ephem("✅ 已加入，扣除 " + std::to_string(paid_bet) + " 碼。");
        g_bot->message_edit(pub);
        return;
    }

    // ── rps_leave ─────────────────────────────────────────────────────────────
    if (cid.rfind("rps_leave_", 0) == 0) {
        dpp::snowflake ch_id(std::stoull(cid.substr(10)));
        int64_t refund = 0;
        dpp::message pub;
        bool need_edit = false;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto gi = rps_games.find(ch_id);
            if (gi == rps_games.end())                { err = "❌ 遊戲不存在或已結束。"; }
            else if (gi->second.started)              { err = "❌ 遊戲已開始，無法離開。"; }
            else if (!gi->second.players.count(uid))  { err = "❌ 你不在此房間內。"; }
            else {
                refund = gi->second.bet;
                chip_data[uid].chips += refund;
                bool was_host = (gi->second.host_uid == uid);
                gi->second.players.erase(uid);
                gi->second.avatars.erase(uid);
                if (gi->second.players.empty()) {
                    rps_games.erase(ch_id);
                } else {
                    if (was_host) gi->second.host_uid = gi->second.players.begin()->first;
                    auto lm = make_rps_lobby_msg(gi->second);
                    pub.id = gi->second.message_id; pub.channel_id = ch_id;
                    for (auto& em : lm.embeds)     pub.add_embed(em);
                    for (auto& co : lm.components) pub.add_component(co);
                    need_edit = true;
                }
            }
        }
        if (!err.empty()) { ephem(err); return; }
        save_chips();
        ephem("↩️ 已離開，退還 " + std::to_string(refund) + " 碼。");
        if (need_edit) g_bot->message_edit(pub);
        return;
    }

    // ── rps_start ─────────────────────────────────────────────────────────────
    if (cid.rfind("rps_start_", 0) == 0) {
        dpp::snowflake ch_id(std::stoull(cid.substr(10)));
        dpp::message pub;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto gi = rps_games.find(ch_id);
            if (gi == rps_games.end())               { err = "❌ 遊戲不存在或已結束。"; }
            else if (gi->second.started)             { err = "❌ 遊戲已開始。"; }
            else if (gi->second.host_uid != uid)     { err = "❌ 只有房主可以開始遊戲。"; }
            else if ((int)gi->second.players.size() < 2) { err = "❌ 至少需要 2 人才能開始。"; }
            else {
                gi->second.started = true;
                auto cm = make_rps_choosing_msg(gi->second);
                pub.id = gi->second.message_id; pub.channel_id = ch_id;
                for (auto& em : cm.embeds)     pub.add_embed(em);
                for (auto& co : cm.components) pub.add_component(co);
            }
        }
        if (!err.empty()) { ephem(err); return; }
        ev.reply(dpp::ir_update_message, pub);
        return;
    }

    // ── rps_pick（秘密出拳選單） ──────────────────────────────────────────────
    if (cid.rfind("rps_pick_", 0) == 0) {
        dpp::snowflake ch_id(std::stoull(cid.substr(9)));
        int cur_choice = -1;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto gi = rps_games.find(ch_id);
            if (gi == rps_games.end())                   { err = "❌ 遊戲不存在或已結束。"; }
            else if (!gi->second.started || gi->second.draw_state) { err = "❌ 現在不是出拳階段。"; }
            else if (!gi->second.players.count(uid))     { err = "❌ 你不在此遊戲中。"; }
            else {
                auto ci = gi->second.choices.find(uid);
                cur_choice = (ci != gi->second.choices.end()) ? ci->second : 0;
            }
        }
        if (!err.empty()) { ephem(err); return; }
        if (cur_choice != 0) {
            ephem("✅ 你已選擇 " + std::string(RPS_EMO[cur_choice]) + " **" + RPS_NAME[cur_choice] + "**，請等待其他人…");
            return;
        }
        std::string ch_s = std::to_string((uint64_t)ch_id);
        dpp::embed e; e.set_title("✊✌️🖐️  秘密出拳").set_color(0x9B59B6);
        e.set_description("選擇後只有你看得到，等所有人選完再同時公布！");
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("✊ 石頭").set_id("rps_choose_" + ch_s + "_1").set_style(dpp::cos_secondary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("✌️ 剪刀").set_id("rps_choose_" + ch_s + "_2").set_style(dpp::cos_secondary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🖐️ 布").set_id("rps_choose_" + ch_s + "_3").set_style(dpp::cos_secondary));
        dpp::message m; m.add_embed(e); m.add_component(row);
        m.set_flags(dpp::m_ephemeral);
        ev.reply(dpp::ir_channel_message_with_source, m);
        return;
    }

    // ── rps_choose（記錄選擇） ────────────────────────────────────────────────
    if (cid.rfind("rps_choose_", 0) == 0) {
        std::string rest = cid.substr(11);
        auto last_ = rest.rfind('_');
        dpp::snowflake ch_id(std::stoull(rest.substr(0, last_)));
        int choice = std::stoi(rest.substr(last_ + 1));

        bool all_done = false;
        dpp::message pub_update;
        bool need_pub_update = false;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto gi = rps_games.find(ch_id);
            if (gi == rps_games.end())               { err = "❌ 遊戲已結束。"; }
            else if (!gi->second.players.count(uid)) { err = "❌ 你不在此遊戲中。"; }
            else if (gi->second.choices.count(uid) && gi->second.choices[uid] != 0) {
                err = "✅ 你已選過了，等待其他人…";
            } else {
                gi->second.choices[uid] = choice;
                all_done = true;
                for (auto& [puid, name] : gi->second.players)
                    if (!gi->second.choices.count(puid) || gi->second.choices[puid] == 0)
                        { all_done = false; break; }
                if (!all_done) {
                    auto cm = make_rps_choosing_msg(gi->second);
                    pub_update.id = gi->second.message_id;
                    pub_update.channel_id = ch_id;
                    for (auto& em : cm.embeds)     pub_update.add_embed(em);
                    for (auto& co : cm.components) pub_update.add_component(co);
                    need_pub_update = true;
                }
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_update_message,
                dpp::message(err).set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message,
            dpp::message("✅ 你選了 " + std::string(RPS_EMO[choice]) + " **" + RPS_NAME[choice] + "**，等待其他人…")
                .set_flags(dpp::m_ephemeral));
        if (need_pub_update) g_bot->message_edit(pub_update);
        if (all_done) rps_do_reveal(ch_id);
        return;
    }

    // ── rps_vote（平手投票彈出） ──────────────────────────────────────────────
    if (cid.rfind("rps_vote_", 0) == 0) {
        dpp::snowflake ch_id(std::stoull(cid.substr(9)));
        std::string err;
        bool already_voted = false; bool my_vote = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto gi = rps_games.find(ch_id);
            if (gi == rps_games.end())             { err = "❌ 遊戲已結束。"; }
            else if (!gi->second.draw_state)        { err = "❌ 目前不是平手投票階段。"; }
            else if (!gi->second.players.count(uid)){ err = "❌ 你不在此遊戲中。"; }
            else if (gi->second.draw_votes.count(uid)) {
                already_voted = true; my_vote = gi->second.draw_votes[uid];
            }
        }
        if (!err.empty()) { ephem(err); return; }
        if (already_voted) {
            ephem("✅ 你已投票：" + std::string(my_vote ? "🔄 再來一把" : "🚪 安全離場") + "。");
            return;
        }
        std::string ch_s = std::to_string((uint64_t)ch_id);
        dpp::embed e; e.set_title("平手投票").set_color(0x95A5A6);
        e.set_description("你要？");
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🔄 再來一把").set_id("rps_drawvote_" + ch_s + "_r").set_style(dpp::cos_success));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🚪 安全離場").set_id("rps_drawvote_" + ch_s + "_e").set_style(dpp::cos_danger));
        dpp::message m; m.add_embed(e); m.add_component(row);
        m.set_flags(dpp::m_ephemeral);
        ev.reply(dpp::ir_channel_message_with_source, m);
        return;
    }

    // ── rps_drawvote ──────────────────────────────────────────────────────────
    if (cid.rfind("rps_drawvote_", 0) == 0) {
        std::string rest = cid.substr(13);
        auto last_ = rest.rfind('_');
        dpp::snowflake ch_id(std::stoull(rest.substr(0, last_)));
        bool is_rematch = (rest[last_ + 1] == 'r');

        bool all_voted = false;
        dpp::message pub_update;
        bool need_pub_update = false;
        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto gi = rps_games.find(ch_id);
            if (gi == rps_games.end())             { err = "❌ 遊戲已結束。"; }
            else if (!gi->second.draw_state)        { err = "❌ 不在投票階段。"; }
            else if (!gi->second.players.count(uid)){ err = "❌ 你不在此遊戲中。"; }
            else if (gi->second.draw_votes.count(uid)){ err = "✅ 你已投票了。"; }
            else {
                gi->second.draw_votes[uid] = is_rematch;
                all_voted = ((int)gi->second.draw_votes.size() == (int)gi->second.players.size());
                if (!all_voted) {
                    auto dm = make_rps_draw_msg(gi->second);
                    pub_update.id = gi->second.message_id;
                    pub_update.channel_id = ch_id;
                    for (auto& em : dm.embeds)     pub_update.add_embed(em);
                    for (auto& co : dm.components) pub_update.add_component(co);
                    need_pub_update = true;
                }
            }
        }
        if (!err.empty()) {
            ev.reply(dpp::ir_update_message,
                dpp::message(err).set_flags(dpp::m_ephemeral)); return;
        }
        ev.reply(dpp::ir_update_message,
            dpp::message("✅ 已投票：" + std::string(is_rematch ? "🔄 再來一把" : "🚪 安全離場") + "。")
                .set_flags(dpp::m_ephemeral));
        if (need_pub_update) g_bot->message_edit(pub_update);
        if (all_voted) rps_process_draw_result(ch_id);
        return;
    }

    // ── rps_again（再來一局 / 雙倍下注） ─────────────────────────────────────
    if (cid.rfind("rps_again_", 0) == 0) {
        std::string rest = cid.substr(10);
        auto us = rest.find('_');
        dpp::snowflake ch_id(std::stoull(rest.substr(0, us)));
        int64_t new_bet = std::stoll(rest.substr(us + 1));

        std::string dn = ev.command.member.get_nickname();
        if (dn.empty()) dn = ev.command.get_issuing_user().username;
        std::string av = ev.command.get_issuing_user().get_avatar_url();

        std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (rps_games.count(ch_id)) { err = "❌ 此頻道已有進行中的猜拳遊戲！"; }
            else {
                auto ci = chip_data.find(uid);
                int64_t chips = (ci != chip_data.end()) ? ci->second.chips : 0;
                if (chips < new_bet) {
                    err = "❌ 籌碼不足 " + std::to_string(new_bet) + " 碼。";
                } else {
                    chip_data[uid].chips -= new_bet;
                    RpsGame ng;
                    ng.host_uid = uid; ng.channel_id = ch_id; ng.bet = new_bet;
                    ng.players[uid] = dn; ng.avatars[uid] = av;
                    rps_games[ch_id] = ng;
                }
            }
        }
        if (!err.empty()) { ephem(err); return; }
        save_chips();

        // Update the old result message to prevent re-clicking
        dpp::embed old_e;
        old_e.set_title("✊✌️🖐️  新遊戲已開啟").set_color(0x808080);
        old_e.set_description("<@" + std::to_string((uint64_t)uid) + "> 發起了新一局（" + std::to_string(new_bet) + " 碼）！");
        ev.reply(dpp::ir_update_message, dpp::message().add_embed(old_e));

        // Send new lobby message
        RpsGame g_snap;
        { std::lock_guard<std::mutex> lk(data_mutex); g_snap = rps_games[ch_id]; }
        auto lobby = make_rps_lobby_msg(g_snap);
        lobby.channel_id = ch_id;
        g_bot->message_create(lobby, [ch_id](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (rps_games.count(ch_id))
                    rps_games[ch_id].message_id = std::get<dpp::message>(cb.value).id;
            }
        });
        return;
    }

    // ── rps_refresh ───────────────────────────────────────────────────────────
    if (cid.rfind("rps_refresh_", 0) == 0) {
        dpp::snowflake ch_id(std::stoull(cid.substr(12)));
        dpp::message fresh;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto gi = rps_games.find(ch_id);
            if (gi == rps_games.end()) { ephem("❌ 遊戲不存在或已結束。"); return; }
            auto& g = gi->second;
            if (!g.started)        fresh = make_rps_lobby_msg(g);
            else if (g.draw_state) fresh = make_rps_draw_msg(g);
            else                   fresh = make_rps_choosing_msg(g);
        }
        dpp::embed se; se.set_title("🔄 訊息已刷新").set_color(0x95A5A6)
                          .set_description("請往下滑查看最新狀態！");
        ev.reply(dpp::ir_update_message, dpp::message().add_embed(se));
        fresh.channel_id = ch_id;
        g_bot->message_create(fresh, [ch_id](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = rps_games.find(ch_id);
                if (it != rps_games.end())
                    it->second.message_id = std::get<dpp::message>(cb.value).id;
            }
        });
        return;
    }
}

// ── !猜拳 訊息指令 ────────────────────────────────────────────────────────────
static void handle_rps_message(const dpp::message_create_t& ev,
                                const std::string& content,
                                dpp::snowflake uid, dpp::snowflake ch)
{
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (rps_games.count(ch)) {
            dpp::message m; m.channel_id = ch;
            m.set_content("❌ 此頻道已有進行中的猜拳遊戲！");
            g_bot->message_create(m); return;
        }
    }
    // 解析金額（支援 !猜拳 / ！猜拳 兩種前綴）
    std::string rest = content;
    // strip 全形/半形 !猜拳
    for (auto& pfx : {"!猜拳", "！猜拳"}) {
        if (rest.rfind(pfx, 0) == 0) { rest = rest.substr(strlen(pfx)); break; }
    }
    while (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
    if (rest.empty()) {
        dpp::message m; m.channel_id = ch;
        m.set_content("用法：`!猜拳 [籌碼]`　例：`!猜拳 1000`");
        g_bot->message_create(m); return;
    }
    int64_t bet = 0;
    try { bet = std::stoll(rest); } catch (...) {}
    if (bet <= 0) {
        dpp::message m; m.channel_id = ch;
        m.set_content("❌ 籌碼金額必須是正整數！"); g_bot->message_create(m); return;
    }
    std::string dn = ev.msg.member.get_nickname();
    if (dn.empty()) dn = ev.msg.author.username;
    std::string av = ev.msg.author.get_avatar_url();

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ci = chip_data.find(uid);
        int64_t chips = (ci != chip_data.end()) ? ci->second.chips : 0;
        if (chips < bet) {
            dpp::message m; m.channel_id = ch;
            m.set_content("❌ 籌碼不足 " + std::to_string(bet) + " 碼！");
            g_bot->message_create(m); return;
        }
        chip_data[uid].chips -= bet;
        RpsGame ng;
        ng.host_uid = uid; ng.channel_id = ch; ng.bet = bet;
        ng.players[uid] = dn; ng.avatars[uid] = av;
        rps_games[ch] = ng;
    }
    save_chips();

    RpsGame g_snap;
    { std::lock_guard<std::mutex> lk(data_mutex); g_snap = rps_games[ch]; }
    auto lobby = make_rps_lobby_msg(g_snap);
    lobby.channel_id = ch;
    g_bot->message_create(lobby, [ch](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (rps_games.count(ch))
                rps_games[ch].message_id = std::get<dpp::message>(cb.value).id;
        }
    });
}

// ── /猜拳 slash ───────────────────────────────────────────────────────────────
static void handle_rps_slash(const dpp::slashcommand_t& ev,
                              dpp::snowflake uid, dpp::snowflake ch)
{
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (rps_games.count(ch)) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 此頻道已有進行中的猜拳遊戲！").set_flags(dpp::m_ephemeral));
            return;
        }
    }
    int64_t bet = 0;
    try { bet = std::get<int64_t>(ev.get_parameter("籌碼")); } catch (...) {}
    if (bet <= 0) {
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 籌碼金額必須是正整數！").set_flags(dpp::m_ephemeral));
        return;
    }
    std::string dn = ev.command.member.get_nickname();
    if (dn.empty()) dn = ev.command.get_issuing_user().username;
    std::string av = ev.command.get_issuing_user().get_avatar_url();

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ci = chip_data.find(uid);
        int64_t chips = (ci != chip_data.end()) ? ci->second.chips : 0;
        if (chips < bet) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 籌碼不足 " + std::to_string(bet) + " 碼！").set_flags(dpp::m_ephemeral));
            return;
        }
        chip_data[uid].chips -= bet;
        RpsGame ng;
        ng.host_uid = uid; ng.channel_id = ch; ng.bet = bet;
        ng.players[uid] = dn; ng.avatars[uid] = av;
        rps_games[ch] = ng;
    }
    save_chips();

    RpsGame g_snap;
    { std::lock_guard<std::mutex> lk(data_mutex); g_snap = rps_games[ch]; }
    ev.reply(dpp::ir_channel_message_with_source, make_rps_lobby_msg(g_snap));
    ev.get_original_response([ch](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (rps_games.count(ch))
                rps_games[ch].message_id = std::get<dpp::message>(cb.value).id;
        }
    });
}
