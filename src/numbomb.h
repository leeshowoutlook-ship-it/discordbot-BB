#pragma once
// ─── 數字炸彈（團康，多人）────────────────────────────────────────────────────
// 1～100 藏一顆炸彈，玩家輪流猜數字，沒中就把範圍縮小，猜中炸彈的人輸。
// 入場費：每個人加入時先付，踩到炸彈的人的入場費歸其他人，獎池（入場費×人數）由其他人平分。
// 每回合 60 秒，超時系統幫他亂猜；範圍只剩炸彈時，輪到的人直接踩到。
// 一個頻道同時只能有一局。大廳 15 分鐘沒開始自動取消退費。
// 所有狀態存在 numbomb.json，重開 bot 會接著玩（計時器在 on_ready 重新掛上）。
// 討論串規則：摸彩房、ALLIN 房不能開（團康沒辦法每個人都 ALLIN）；最低下注房入場費至少 5,000。
#include "chips.h"
#include <random>

static const std::string NB_FILE = "numbomb.json";
static const int NB_TURN_SECS   = 60;
static const int NB_LOBBY_SECS  = 900;
static const int NB_MAX_PLAYERS = 10;
static const int NB_MIN_PLAYERS = 2;

struct NbGame {
    uint64_t ch = 0, msg_id = 0, host = 0;
    int64_t  fee = 0;
    int      state = 0;                  // 0 大廳／1 遊戲中
    std::vector<uint64_t> players;       // 遊戲中＝回合順序
    std::map<uint64_t, std::string> names, avatars;
    int lo = 0, hi = 101, bomb = 0;      // 可以猜的範圍是 lo+1 ～ hi-1
    int turn = 0, seq = 0;               // seq：每換一次回合 +1，計時器／表單用來判斷是不是舊的
    time_t created = 0, deadline = 0;
    std::vector<std::string> log;
};
struct NbStats { int games = 0, bombs = 0; int64_t profit = 0; };
inline std::map<uint64_t, NbGame>  nb_games;   // 頻道 → 遊戲
inline std::map<uint64_t, NbStats> nb_stats;

static std::mt19937& nb_rng() { static std::mt19937 g(std::random_device{}()); return g; }

// ─── 存檔 ─────────────────────────────────────────────────────────────────────
static void save_numbomb() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        nlohmann::json gs = nlohmann::json::array();
        for (auto& [ch, g] : nb_games) {
            nlohmann::json nm = nlohmann::json::object(), av = nlohmann::json::object();
            for (auto& [u, n] : g.names) nm[std::to_string(u)] = n;
            for (auto& [u, a] : g.avatars) av[std::to_string(u)] = a;
            std::vector<std::string> ps; for (auto u : g.players) ps.push_back(std::to_string(u));
            gs.push_back({{"ch", std::to_string(g.ch)}, {"msg_id", std::to_string(g.msg_id)}, {"host", std::to_string(g.host)},
                          {"fee", g.fee}, {"state", g.state}, {"players", ps}, {"names", nm}, {"avatars", av},
                          {"lo", g.lo}, {"hi", g.hi}, {"bomb", g.bomb}, {"turn", g.turn}, {"seq", g.seq},
                          {"created", (int64_t)g.created}, {"deadline", (int64_t)g.deadline}, {"log", g.log}});
        }
        j["games"] = gs;
        nlohmann::json st = nlohmann::json::object();
        for (auto& [u, s] : nb_stats) st[std::to_string(u)] = {{"games", s.games}, {"bombs", s.bombs}, {"profit", s.profit}};
        j["stats"] = st;
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(NB_FILE, j.dump(2));
}
static void load_numbomb() {
    std::ifstream f(NB_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        auto u64 = [](const nlohmann::json& v, const char* k) -> uint64_t {
            try { return std::stoull(v.value(k, std::string("0"))); } catch (...) { return 0; }
        };
        if (j.contains("games"))
            for (auto& v : j["games"]) {
                NbGame g;
                g.ch = u64(v, "ch"); g.msg_id = u64(v, "msg_id"); g.host = u64(v, "host");
                g.fee = v.value("fee", (int64_t)0); g.state = v.value("state", 0);
                if (v.contains("players")) for (auto& p : v["players"]) g.players.push_back(std::stoull(p.get<std::string>()));
                if (v.contains("names"))   for (auto& [k, n] : v["names"].items())   g.names[std::stoull(k)] = n.get<std::string>();
                if (v.contains("avatars")) for (auto& [k, a] : v["avatars"].items()) g.avatars[std::stoull(k)] = a.get<std::string>();
                g.lo = v.value("lo", 0); g.hi = v.value("hi", 101); g.bomb = v.value("bomb", 0);
                g.turn = v.value("turn", 0); g.seq = v.value("seq", 0);
                g.created = (time_t)v.value("created", (int64_t)0); g.deadline = (time_t)v.value("deadline", (int64_t)0);
                if (v.contains("log")) g.log = v["log"].get<std::vector<std::string>>();
                if (g.ch) nb_games[g.ch] = g;
            }
        if (j.contains("stats"))
            for (auto& [k, v] : j["stats"].items()) {
                auto& s = nb_stats[std::stoull(k)];
                s.games = v.value("games", 0); s.bombs = v.value("bombs", 0); s.profit = v.value("profit", (int64_t)0);
            }
    } catch (...) {}
}

// 呼叫端持有 data_mutex
static std::string nb_stats_line_locked(uint64_t uid) {
    auto it = nb_stats.find(uid);
    if (it == nb_stats.end() || it->second.games == 0) return "尚無紀錄";
    auto& s = it->second;
    int surv = (s.games - s.bombs) * 100 / s.games;
    return "存活率 **" + std::to_string(surv) + "%**　盈虧 **" + std::string(s.profit >= 0 ? "+" : "") + std::to_string(s.profit) + "** 碼";
}
static std::string nb_stats_line(dpp::snowflake uid) {
    std::lock_guard<std::mutex> lk(data_mutex);
    return nb_stats_line_locked((uint64_t)uid);
}

// ─── 畫面 ─────────────────────────────────────────────────────────────────────
static std::string nb_name(const NbGame& g, uint64_t u) {
    auto it = g.names.find(u);
    return it != g.names.end() && !it->second.empty() ? it->second : std::to_string(u);
}
static dpp::component nb_btn(const std::string& label, const std::string& id, dpp::component_style st, bool dis = false) {
    return dpp::component().set_type(dpp::cot_button).set_label(label).set_id(id).set_style(st).set_disabled(dis);
}
static dpp::message make_nb_lobby_msg(const NbGame& g) {
    std::string cs = std::to_string(g.ch);
    dpp::embed e;
    e.set_title("💣  數字炸彈").set_color(0xE67E22);
    std::string d = "入場費：**" + std::to_string(g.fee) + "** 碼" + (g.fee > 0 ? "（踩到炸彈的人的入場費歸其他人平分）" : "（純娛樂）") + "\n";
    d += "人數：**" + std::to_string(g.players.size()) + "/" + std::to_string(NB_MAX_PLAYERS) + "**（至少 " + std::to_string(NB_MIN_PLAYERS) + " 人才能開始）\n\n";
    for (size_t i = 0; i < g.players.size(); i++)
        d += std::to_string(i + 1) + ". <@" + std::to_string(g.players[i]) + ">" + (g.players[i] == g.host ? "　👑" : "") + "\n";
    d += "\n📜 1～100 之間藏了一顆炸彈，大家輪流猜數字，沒中就會把範圍縮小，**猜中炸彈的人就輸了**！\n";
    d += "每人每回合 " + std::to_string(NB_TURN_SECS) + " 秒，超時系統幫你亂猜。\n";
    d += "-# 大廳 <t:" + std::to_string((int64_t)(g.created + NB_LOBBY_SECS)) + ":R> 沒開始會自動取消並退費。";
    e.set_description(d);
    auto av = g.avatars.find(g.host);
    if (av != g.avatars.end() && !av->second.empty()) e.set_thumbnail(av->second);
    e.set_footer(dpp::embed_footer().set_text("👤 主持：" + nb_name(g, g.host)));
    dpp::message m("💣 數字炸彈大廳開好了，按「加入」一起玩！");
    m.add_embed(e);
    dpp::component r; r.set_type(dpp::cot_action_row);
    r.add_component(nb_btn("加入（" + std::to_string(g.fee) + " 碼）", "nb_join_" + cs, dpp::cos_success, (int)g.players.size() >= NB_MAX_PLAYERS));
    r.add_component(nb_btn("離開", "nb_leave_" + cs, dpp::cos_secondary));
    r.add_component(nb_btn("▶ 開始", "nb_start_" + cs, dpp::cos_primary, (int)g.players.size() < NB_MIN_PLAYERS));
    r.add_component(nb_btn("取消", "nb_cancel_" + cs, dpp::cos_danger));
    m.add_component(r);
    return m;
}
static dpp::message make_nb_play_msg(const NbGame& g) {
    std::string cs = std::to_string(g.ch);
    uint64_t cur = g.players[g.turn];
    dpp::embed e;
    e.set_title("💣  數字炸彈 — 遊戲中").set_color(0xE74C3C);
    std::string d = "## 範圍：" + std::to_string(g.lo + 1) + " ～ " + std::to_string(g.hi - 1) + "\n";
    d += "▶ 輪到 <@" + std::to_string(cur) + ">　⏰ <t:" + std::to_string((int64_t)g.deadline) + ":R> 超時自動亂猜\n\n";
    if (!g.log.empty()) {
        size_t from = g.log.size() > 6 ? g.log.size() - 6 : 0;
        for (size_t i = from; i < g.log.size(); i++) d += g.log[i] + "\n";
        d += "\n";
    }
    d += "順序：";
    for (size_t i = 0; i < g.players.size(); i++) d += std::string(i ? " → " : "") + ((int)i == g.turn ? "**" + nb_name(g, g.players[i]) + "**" : nb_name(g, g.players[i]));
    d += "\n獎池：**" + std::to_string(g.fee * (int64_t)g.players.size()) + "** 碼";
    e.set_description(d);
    auto av = g.avatars.find(cur);
    if (av != g.avatars.end() && !av->second.empty()) e.set_thumbnail(av->second);
    e.set_footer(dpp::embed_footer().set_text("👤 " + nb_name(g, cur)));
    dpp::message m("輪到 <@" + std::to_string(cur) + "> 猜！");
    m.add_embed(e);
    dpp::component r; r.set_type(dpp::cot_action_row);
    r.add_component(nb_btn("💣 猜數字", "nb_guess_" + cs, dpp::cos_danger));
    r.add_component(nb_btn("🎲 隨便猜", "nb_rand_" + cs, dpp::cos_secondary));
    m.add_component(r);
    return m;
}
// 結算畫面要的資料（遊戲結束後 nb_games 裡已經沒有這局了）
struct NbResult { NbGame g; uint64_t loser = 0; int64_t share = 0; std::map<uint64_t, std::string> lines; };
static dpp::message make_nb_result_msg(const NbResult& r) {
    const NbGame& g = r.g;
    std::string cs = std::to_string(g.ch);
    dpp::embed e;
    e.set_title("💥  BOOM！炸彈是 " + std::to_string(g.bomb)).set_color(0x992D22);
    std::string d;
    size_t from = g.log.size() > 8 ? g.log.size() - 8 : 0;
    for (size_t i = from; i < g.log.size(); i++) d += g.log[i] + "\n";
    d += "\n💀 <@" + std::to_string(r.loser) + "> 踩到炸彈了！\n\n";
    for (auto u : g.players) {
        int64_t net = (u == r.loser) ? -g.fee : r.share - g.fee;
        d += (u == r.loser ? "💀 " : "🎉 ") + std::string("<@") + std::to_string(u) + ">　" + (net >= 0 ? "+" : "") + std::to_string(net) + " 碼";
        auto it = r.lines.find(u);
        if (it != r.lines.end()) d += "　｜　" + it->second;
        d += "\n";
    }
    e.set_description(d);
    auto av = g.avatars.find(r.loser);
    if (av != g.avatars.end() && !av->second.empty()) e.set_thumbnail(av->second);
    e.set_footer(dpp::embed_footer().set_text("👤 踩到炸彈：" + nb_name(g, r.loser)));
    dpp::message m("💥 BOOM！數字炸彈結束");
    m.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(nb_btn("再來一局", "nb_again_" + cs + "_" + std::to_string(g.fee), dpp::cos_primary));
    row.add_component(nb_btn("雙倍入場費（" + std::to_string(g.fee * 2) + "）", "nb_again_" + cs + "_" + std::to_string(g.fee * 2),
                             dpp::cos_success, g.fee == 0));
    m.add_component(row);
    return m;
}

// ─── 遊戲流程（_locked＝呼叫端持有 data_mutex）──────────────────────────────
static dpp::message nb_closed_msg(const std::string& text) {
    dpp::message m("💣 數字炸彈");
    m.add_embed(dpp::embed().set_title("💣  數字炸彈").set_description(text).set_color(0x95A5A6));
    return m;
}
static void nb_refund_all_locked(NbGame& g) {
    for (auto u : g.players) chip_data[dpp::snowflake(u)].chips += g.fee;
}
// 開局檢查（頻道規則）；回傳錯誤訊息或空字串
static std::string nb_room_error(dpp::snowflake ch, int64_t fee) {
    if (is_lottery_room(ch)) return "❌ 此討論串只能 `!摸彩`，不能玩遊戲！";
    if (is_allin_room(ch))   return "❌ ALLIN 房不能開數字炸彈（團康遊戲沒辦法讓每個人都 ALLIN）。";
    if (fee < 0) return "❌ 入場費不能是負的。";
    if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && fee < 5000)
        return "❌ 此討論串最低下注為 **5,000** 碼！";
    return "";
}
// 踩到炸彈：結算、刪掉這局。回傳結算資料。
static NbResult nb_boom_locked(NbGame& g, uint64_t loser) {
    NbResult r; r.loser = loser;
    int n = (int)g.players.size();
    int64_t pot = g.fee * n;
    r.share = n > 1 ? pot / (n - 1) : pot;
    for (auto u : g.players) {
        auto& st = nb_stats[u];
        st.games++;
        if (u == loser) { st.bombs++; st.profit -= g.fee; }
        else { chip_data[dpp::snowflake(u)].chips += r.share; st.profit += r.share - g.fee; }
    }
    for (auto u : g.players) r.lines[u] = nb_stats_line_locked(u);
    r.g = g;
    nb_games.erase(g.ch);
    return r;
}
// 處理一次猜測。回傳 true＝爆炸（result 有值，遊戲已刪除）；false＝繼續（g 已換到下一個人）。
static bool nb_guess_locked(NbGame& g, int n, bool auto_pick, NbResult& result) {
    uint64_t cur = g.players[g.turn];
    std::string who = nb_name(g, cur);
    if (n == g.bomb) {
        g.log.push_back("💥 " + who + (auto_pick ? "（超時／隨便猜）" : "") + " 猜了 **" + std::to_string(n) + "**……");
        result = nb_boom_locked(g, cur);
        return true;
    }
    if (n < g.bomb) g.lo = n; else g.hi = n;
    g.log.push_back("✅ " + who + (auto_pick ? "（超時／隨便猜）" : "") + " 猜 **" + std::to_string(n) + "** → 範圍 " +
                    std::to_string(g.lo + 1) + "～" + std::to_string(g.hi - 1));
    g.turn = (g.turn + 1) % (int)g.players.size();
    g.seq++;
    g.deadline = time(nullptr) + NB_TURN_SECS;
    if (g.hi - g.lo == 2) { // 只剩炸彈了，輪到的人只能踩
        uint64_t next = g.players[g.turn];
        g.log.push_back("😱 只剩 **" + std::to_string(g.bomb) + "** 了，" + nb_name(g, next) + " 只能踩下去……");
        result = nb_boom_locked(g, next);
        return true;
    }
    return false;
}
static int nb_random_pick(const NbGame& g) {
    return std::uniform_int_distribution<int>(g.lo + 1, g.hi - 1)(nb_rng());
}

// ─── 計時器 ───────────────────────────────────────────────────────────────────
static void nb_edit_game_msg(dpp::message m, uint64_t ch, uint64_t msg_id) {
    if (!g_bot || !msg_id) { if (g_bot) { m.channel_id = ch; g_bot->message_create(m); } return; }
    m.id = msg_id; m.channel_id = ch;
    g_bot->message_edit(m);
}
static void nb_arm_turn_timer(uint64_t ch, int seq, int secs);
// 回合超時：幫目前的人亂猜
static void nb_on_turn_timeout(uint64_t ch, int seq) {
    dpp::message out; uint64_t msg_id = 0; bool boom = false, ok = false; int next_seq = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = nb_games.find(ch);
        if (it == nb_games.end() || it->second.state != 1 || it->second.seq != seq) return;
        NbGame& g = it->second;
        msg_id = g.msg_id;
        NbResult r;
        boom = nb_guess_locked(g, nb_random_pick(g), true, r);
        if (boom) out = make_nb_result_msg(r);
        else { out = make_nb_play_msg(g); next_seq = g.seq; }
        ok = true;
    }
    if (!ok) return;
    if (boom) save_chips();
    save_numbomb();
    nb_edit_game_msg(out, ch, msg_id);
    if (!boom) nb_arm_turn_timer(ch, next_seq, NB_TURN_SECS);
}
static void nb_arm_turn_timer(uint64_t ch, int seq, int secs) {
    if (!g_bot) return;
    g_bot->start_timer([ch, seq](dpp::timer t) { g_bot->stop_timer(t); nb_on_turn_timeout(ch, seq); }, std::max(3, secs));
}
// 大廳逾時：退費取消
static void nb_on_lobby_timeout(uint64_t ch, time_t created) {
    uint64_t msg_id = 0; bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = nb_games.find(ch);
        if (it == nb_games.end() || it->second.state != 0 || it->second.created != created) return;
        msg_id = it->second.msg_id;
        nb_refund_all_locked(it->second);
        nb_games.erase(it);
        ok = true;
    }
    if (!ok) return;
    save_chips(); save_numbomb();
    dpp::message m = nb_closed_msg("⌛ 大廳太久沒開始，已自動取消，入場費全部退回。");
    nb_edit_game_msg(m, ch, msg_id);
}
static void nb_arm_lobby_timer(uint64_t ch, time_t created) {
    if (!g_bot) return;
    int secs = (int)std::max<int64_t>(3, (int64_t)(created + NB_LOBBY_SECS - time(nullptr)));
    g_bot->start_timer([ch, created](dpp::timer t) { g_bot->stop_timer(t); nb_on_lobby_timeout(ch, created); }, secs);
}
// bot 上線後呼叫：把進行中的回合計時器、大廳逾時重新掛上
static void nb_resume_all() {
    std::vector<std::tuple<uint64_t, int, int, int, time_t>> todo; // ch, state, seq, secs, created
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [ch, g] : nb_games)
            todo.push_back({ch, g.state, g.seq, (int)std::max<int64_t>(5, (int64_t)(g.deadline - time(nullptr))), g.created});
    }
    for (auto& [ch, st, seq, secs, created] : todo) {
        if (st == 1) nb_arm_turn_timer(ch, seq, secs);
        else nb_arm_lobby_timer(ch, created);
    }
}

// ─── 開大廳（!數字炸彈／/數字炸彈／再來一局）──────────────────────────────────
// 成功回傳 true，msg 是大廳畫面；失敗回傳 false，msg 是錯誤文字。發出去後要呼叫 nb_set_msg_id。
static bool nb_create(dpp::snowflake ch, dpp::snowflake host, int64_t fee, const std::string& name, const std::string& avatar, dpp::message& msg) {
    std::string err = nb_room_error(ch, fee);
    if (!err.empty()) { msg = dpp::message(err); return false; }
    NbGame g;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (nb_games.count((uint64_t)ch)) { msg = dpp::message("❌ 這個頻道已經有一局數字炸彈了，按上面的「加入」就可以。"); return false; }
        if (chip_data[host].chips < fee) { msg = dpp::message("❌ 籌碼不足，入場費要 " + std::to_string(fee) + " 碼。"); return false; }
        chip_data[host].chips -= fee;
        g.ch = (uint64_t)ch; g.host = (uint64_t)host; g.fee = fee; g.created = time(nullptr);
        g.players.push_back((uint64_t)host); g.names[(uint64_t)host] = name; g.avatars[(uint64_t)host] = avatar;
        nb_games[g.ch] = g;
    }
    save_chips(); save_numbomb();
    nb_arm_lobby_timer((uint64_t)ch, g.created);
    msg = make_nb_lobby_msg(g);
    return true;
}
static void nb_set_msg_id(uint64_t ch, uint64_t msg_id) {
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = nb_games.find(ch);
        if (it == nb_games.end() || it->second.msg_id) return;
        it->second.msg_id = msg_id;
    }
    save_numbomb();
}
static std::string nb_display_name(const dpp::interaction& cmd) {
    const dpp::user& u = cmd.get_issuing_user();
    std::string n = cmd.member.get_nickname();
    if (n.empty()) n = u.global_name;
    if (n.empty()) n = u.username;
    return n;
}

// ─── 按鈕／表單 ───────────────────────────────────────────────────────────────
static bool nb_handle_button(const dpp::button_click_t& ev) {
    const std::string& cid = ev.custom_id;
    if (cid.rfind("nb_", 0) != 0) return false;
    const dpp::user& user = ev.command.get_issuing_user();
    uint64_t uid = (uint64_t)user.id;
    auto eph = [&](const std::string& t) { ev.reply(dpp::ir_channel_message_with_source, dpp::message(t).set_flags(dpp::m_ephemeral)); };

    if (cid.rfind("nb_again_", 0) == 0) { // nb_again_<ch>_<fee>
        std::string rest = cid.substr(9);
        size_t us = rest.find('_');
        if (us == std::string::npos) return true;
        int64_t fee = std::atoll(rest.c_str() + us + 1);
        dpp::message m;
        if (!nb_create(ev.command.channel_id, user.id, fee, nb_display_name(ev.command), user.get_avatar_url(), m)) { eph(m.content); return true; }
        uint64_t ch = (uint64_t)ev.command.channel_id;
        ev.reply(dpp::ir_channel_message_with_source, m);
        ev.get_original_response([ch](const dpp::confirmation_callback_t& c2) {
            if (!c2.is_error()) nb_set_msg_id(ch, std::get<dpp::message>(c2.value).id);
        });
        return true;
    }

    // 其他按鈕：nb_<動作>_<ch>
    size_t us = cid.rfind('_');
    uint64_t ch = std::stoull(cid.substr(us + 1));
    std::string act = cid.substr(3, us - 3);

    if (act == "guess") { // 開表單（只有輪到的人）
        int seq = 0; int lo = 0, hi = 0; std::string err;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = nb_games.find(ch);
            if (it == nb_games.end() || it->second.state != 1) err = "這局已經結束了。";
            else if (it->second.players[it->second.turn] != uid) err = "還沒輪到你喔！";
            else { seq = it->second.seq; lo = it->second.lo; hi = it->second.hi; }
        }
        if (!err.empty()) { eph("❌ " + err); return true; }
        dpp::interaction_modal_response modal("nb_gm_" + std::to_string(ch) + "_" + std::to_string(seq), "💣 數字炸彈");
        modal.add_component(dpp::component().set_type(dpp::cot_text)
            .set_label("猜一個 " + std::to_string(lo + 1) + "～" + std::to_string(hi - 1) + " 的數字")
            .set_id("n").set_text_style(dpp::text_short).set_min_length(1).set_max_length(3)
            .set_placeholder(std::to_string((lo + hi) / 2)));
        ev.dialog(modal);
        return true;
    }

    dpp::message out; bool chips_changed = false; std::string err;
    int arm_seq = -1; bool boom = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = nb_games.find(ch);
        if (it == nb_games.end()) err = "這局已經結束了。";
        else {
            NbGame& g = it->second;
            bool in = std::find(g.players.begin(), g.players.end(), uid) != g.players.end();
            if (act == "join") {
                if (g.state != 0) err = "遊戲已經開始了。";
                else if (in) err = "你已經在裡面了。";
                else if ((int)g.players.size() >= NB_MAX_PLAYERS) err = "人數已滿。";
                else if (chip_data[user.id].chips < g.fee) err = "籌碼不足，入場費要 " + std::to_string(g.fee) + " 碼。";
                else {
                    chip_data[user.id].chips -= g.fee; chips_changed = true;
                    g.players.push_back(uid); g.names[uid] = nb_display_name(ev.command); g.avatars[uid] = user.get_avatar_url();
                    out = make_nb_lobby_msg(g);
                }
            } else if (act == "leave") {
                if (g.state != 0) err = "遊戲已經開始，不能離開。";
                else if (!in) err = "你不在這局裡。";
                else {
                    chip_data[user.id].chips += g.fee; chips_changed = true;
                    g.players.erase(std::find(g.players.begin(), g.players.end(), uid));
                    if (g.players.empty()) { nb_games.erase(it); out = nb_closed_msg("🚪 大家都離開了，這局已取消。"); }
                    else { if (g.host == uid) g.host = g.players[0]; out = make_nb_lobby_msg(g); }
                }
            } else if (act == "cancel") {
                if (g.state != 0) err = "遊戲已經開始，不能取消。";
                else if (g.host != uid) err = "只有主持人（👑）可以取消。";
                else { nb_refund_all_locked(g); chips_changed = true; nb_games.erase(it); out = nb_closed_msg("❌ 主持人取消了這局，入場費全部退回。"); }
            } else if (act == "start") {
                if (g.state != 0) err = "遊戲已經開始了。";
                else if (g.host != uid) err = "只有主持人（👑）可以開始。";
                else if ((int)g.players.size() < NB_MIN_PLAYERS) err = "至少要 " + std::to_string(NB_MIN_PLAYERS) + " 人。";
                else {
                    g.state = 1;
                    std::shuffle(g.players.begin(), g.players.end(), nb_rng());
                    g.bomb = std::uniform_int_distribution<int>(1, 100)(nb_rng());
                    g.lo = 0; g.hi = 101; g.turn = 0; g.seq++;
                    g.deadline = time(nullptr) + NB_TURN_SECS;
                    g.log.push_back("🎬 遊戲開始！炸彈藏在 1～100 之間");
                    out = make_nb_play_msg(g); arm_seq = g.seq;
                }
            } else if (act == "rand") {
                if (g.state != 1) err = "遊戲還沒開始。";
                else if (g.players[g.turn] != uid) err = "還沒輪到你喔！";
                else {
                    NbResult r;
                    boom = nb_guess_locked(g, nb_random_pick(g), true, r);
                    if (boom) { out = make_nb_result_msg(r); chips_changed = true; }
                    else { out = make_nb_play_msg(g); arm_seq = g.seq; }
                }
            } else err = "未知的按鈕。";
        }
    }
    if (!err.empty()) { eph("❌ " + err); return true; }
    if (chips_changed) save_chips();
    save_numbomb();
    ev.reply(dpp::ir_update_message, out);
    if (arm_seq >= 0) nb_arm_turn_timer(ch, arm_seq, NB_TURN_SECS);
    return true;
}

static bool nb_handle_form(const dpp::form_submit_t& ev) {
    const std::string& cid = ev.custom_id;
    if (cid.rfind("nb_gm_", 0) != 0) return false;
    std::string rest = cid.substr(6);
    size_t us = rest.find('_');
    if (us == std::string::npos) return true;
    uint64_t ch = std::stoull(rest.substr(0, us));
    int seq = atoi(rest.c_str() + us + 1);
    std::string input;
    for (auto& row : ev.components) {
        if (std::holds_alternative<std::string>(row.value)) input = std::get<std::string>(row.value);
        for (auto& sub : row.components) if (std::holds_alternative<std::string>(sub.value)) input = std::get<std::string>(sub.value);
    }
    uint64_t uid = (uint64_t)ev.command.get_issuing_user().id;
    auto eph = [&](const std::string& t) { ev.reply(dpp::ir_channel_message_with_source, dpp::message(t).set_flags(dpp::m_ephemeral)); };
    dpp::message out; std::string err; bool boom = false; int arm_seq = -1;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = nb_games.find(ch);
        if (it == nb_games.end() || it->second.state != 1) err = "這局已經結束了。";
        else {
            NbGame& g = it->second;
            if (g.seq != seq || g.players[g.turn] != uid) err = "已經不是你的回合了（可能超時被系統猜掉了）。";
            else {
                bool digits = !input.empty() && std::all_of(input.begin(), input.end(), ::isdigit);
                int n = digits ? atoi(input.c_str()) : -1;
                if (n <= g.lo || n >= g.hi) err = "要猜 " + std::to_string(g.lo + 1) + "～" + std::to_string(g.hi - 1) + " 之間的數字，請再按一次「猜數字」。";
                else {
                    NbResult r;
                    boom = nb_guess_locked(g, n, false, r);
                    if (boom) out = make_nb_result_msg(r);
                    else { out = make_nb_play_msg(g); arm_seq = g.seq; }
                }
            }
        }
    }
    if (!err.empty()) { eph("❌ " + err); return true; }
    if (boom) save_chips();
    save_numbomb();
    ev.reply(dpp::ir_update_message, out);
    if (arm_seq >= 0) nb_arm_turn_timer(ch, arm_seq, NB_TURN_SECS);
    return true;
}
