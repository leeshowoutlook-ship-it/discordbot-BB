#pragma once
// ─── 英雄聯盟世界賽下注（固定賠率）────────────────────────────────────────────
// 三種盤：單場勝負、單場比分、冠軍隊伍。押中拿回「下注 × 下注當下的賠率」，之後改賠率不影響已押的；
// 系統當莊家，不抽水（賠率＝1／機率）。比賽取消就全部退回。
// 機率來源：冠軍機率用 Polymarket（2026/11/1 查到的數字），單場勝率用冠軍機率推算（KC vs T1 用查到的 1.8／1.8），
// 比分機率用單場勝率反推每一小局的勝率再算出來。管理員可以用 !世界賽 賠率 … 覆寫。
// 賽程／結果從 Leaguepedia（lol.fandom.com 的 cargoquery，免金鑰）每 15 分鐘抓一次：
//   有對戰隊伍就自動開盤 → 開賽時間到自動封盤 → 有勝方就自動結算；冠軍盤在八強四場都排好時自動開、
//   第一場八強開打封盤、決賽結算時一起結算。抓不到時管理員可用 !世界賽 結算 … 手動處理。
// 所有資料存在 lolbet.json；資料用 data_mutex 保護（跟籌碼一起），存檔前要先放掉鎖。
#include "chips.h"
#include <ctime>
#include <tuple>
#include <iomanip>

static const std::string LOLBET_FILE        = "lolbet.json";
static const std::string LOL_OVERVIEW_PAGE  = "2026 Season World Championship/Main Event";
static const std::string LOL_TITLE          = "2026 英雄聯盟世界賽";
static const int64_t     LOL_MIN_BET        = 100;
static const int64_t     LOL_MAX_BET_POOL   = 500000; // 每個人在每一個盤最多押這麼多

struct LolBet  { std::string pick; int64_t amount = 0; int64_t to_win = 0; int64_t payout = -1; }; // to_win＝押中會拿回的總額（含本金）；payout -1＝還沒結算
struct LolPool { std::map<uint64_t, LolBet> bets; };
struct LolMatch {
    int         no = 0;          // 顯示用編號（管理員指令用這個）
    std::string key;             // Leaguepedia MatchId；手動開的是 manual_<no>
    std::string stage;           // 八強 1／四強 2／決賽…
    std::string team1, team2;
    time_t      start = 0;       // 0＝沒有開賽時間（手動開盤，要手動封盤）
    int         bestof = 5;
    int         status = 0;      // 0 下注中／1 已封盤／2 已結算／3 已取消
    int         score1 = 0, score2 = 0;
    double      odds1 = 0, odds2 = 0; // 管理員手動設定的勝負賠率；0＝用估計機率推算
    LolPool     win_pool, score_pool;
};
struct LolChamp {
    bool exists = false;
    std::vector<std::string> teams;
    time_t lock_time = 0;
    int    status = 0;           // 同 LolMatch
    std::string winner;
    std::vector<double> odds;    // 跟 teams 一一對應
    LolPool pool;
};
struct LolStat { int bets = 0, wins = 0; int64_t profit = 0; };

inline std::vector<LolMatch>          lol_matches;
inline LolChamp                       lol_champ;
inline int                            lol_next_no = 1;
inline uint64_t                       lol_announce_ch = 0;
inline std::map<uint64_t, LolStat>    lol_stats;

// ─── 小工具 ───────────────────────────────────────────────────────────────────
static std::string lol_fmt(int64_t v) {
    bool neg = v < 0; uint64_t u = neg ? (uint64_t)(-v) : (uint64_t)v;
    std::string s = std::to_string(u), out;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) { out.insert(out.begin(), s[i]); if (++c % 3 == 0 && i > 0) out.insert(out.begin(), ','); }
    return (neg ? "-" : "") + out;
}
// Leaguepedia 隊名去掉括號註記，例如「LYON (2024 American Team)」→「LYON」
static std::string lol_clean_team(std::string s) {
    size_t p = s.find(" (");
    if (p != std::string::npos) s = s.substr(0, p);
    return s;
}
static std::string lol_short(const std::string& name) {
    static const std::map<std::string, std::string> M = {
        {"Top Esports","TES"}, {"Invictus Gaming","IG"}, {"Gen.G","GEN"}, {"Dplus Kia","DK"},
        {"Bilibili Gaming","BLG"}, {"Karmine Corp","KC"}, {"Hanwha Life Esports","HLE"},
        {"Anyone's Legend","AL"}, {"JD Gaming","JDG"}, {"Weibo Gaming","WBG"}, {"KT Rolster","KT"},
        {"Movistar KOI","MKOI"}, {"G2 Esports","G2"}, {"Fnatic","FNC"}, {"Team Liquid","TL"},
        {"FlyQuest","FLY"}, {"Cloud9","C9"}, {"100 Thieves","100T"}, {"CTBC Flying Oyster","CFO"},
        {"PSG Talon","PSG"}, {"GAM Esports","GAM"}, {"Vivo Keyd Stars","VKS"}, {"paiN Gaming","PNG"},
    };
    auto it = M.find(name);
    return it != M.end() ? it->second : name;
}
static const char* lol_status_name(int s) {
    switch (s) { case 0: return "🟢 下注中"; case 1: return "🔒 已封盤"; case 2: return "✅ 已結算"; default: return "❌ 已取消"; }
}
static int64_t lol_pool_total(const LolPool& p) { int64_t t = 0; for (auto& [u, b] : p.bets) t += b.amount; return t; }
static int64_t lol_pick_total(const LolPool& p, const std::string& pick) {
    int64_t t = 0; for (auto& [u, b] : p.bets) if (b.pick == pick) t += b.amount; return t;
}
static int lol_pick_count(const LolPool& p, const std::string& pick) {
    int n = 0; for (auto& [u, b] : p.bets) if (b.pick == pick) n++; return n;
}
// 比分盤的選項：「隊1分-隊2分」，BO5 就是 3-0 3-1 3-2 0-3 1-3 2-3
static std::vector<std::string> lol_score_picks(const LolMatch& m) {
    int w = (m.bestof + 1) / 2;
    std::vector<std::string> v;
    for (int l = 0; l < w; l++) v.push_back(std::to_string(w) + "-" + std::to_string(l));
    for (int l = 0; l < w; l++) v.push_back(std::to_string(l) + "-" + std::to_string(w));
    return v;
}
static std::string lol_pick_label(const LolMatch& m, char pool, const std::string& pick) {
    if (pool == 'w') return pick == "1" ? lol_short(m.team1) : lol_short(m.team2);
    size_t d = pick.find('-');
    if (d == std::string::npos) return pick;
    int a = atoi(pick.c_str()), b = atoi(pick.c_str() + d + 1);
    return (a > b ? lol_short(m.team1) : lol_short(m.team2)) + " " + std::to_string(std::max(a, b)) + ":" + std::to_string(std::min(a, b));
}
static LolMatch* lol_find_match_locked(int no) {
    for (auto& m : lol_matches) if (m.no == no) return &m;
    return nullptr;
}
static LolMatch* lol_find_match_key_locked(const std::string& key) {
    for (auto& m : lol_matches) if (m.key == key) return &m;
    return nullptr;
}
static bool lol_match_open(const LolMatch& m) { return m.status == 0 && (m.start == 0 || time(nullptr) < m.start); }
static bool lol_champ_open() { return lol_champ.exists && lol_champ.status == 0 && (lol_champ.lock_time == 0 || time(nullptr) < lol_champ.lock_time); }
// 已經輸掉某場的隊伍（冠軍盤顯示「已淘汰」用）
static std::set<std::string> lol_eliminated_locked() {
    std::set<std::string> out;
    for (auto& m : lol_matches) if (m.status == 2) out.insert(m.score1 > m.score2 ? m.team2 : m.team1);
    return out;
}

// ─── 賠率 ─────────────────────────────────────────────────────────────────────
// 2026/11/1 Polymarket 冠軍機率：Gen.G 43.5%、T1 16.5%、DK 12.2%、BLG 11%；
// 剩下 16.8% 依博彩網站的排序分給 IG／TES／LYON／KC（KC 在 Polymarket 約 2%）。
static double lol_champ_prob(const std::string& team) {
    static const std::map<std::string, double> P = {
        {"Gen.G", 0.435}, {"T1", 0.165}, {"Dplus Kia", 0.122}, {"Bilibili Gaming", 0.110},
        {"Invictus Gaming", 0.060}, {"Top Esports", 0.050}, {"LYON", 0.038}, {"Karmine Corp", 0.020},
    };
    auto it = P.find(team);
    return it != P.end() ? it->second : 0.03;
}
static double lol_floor2(double v) { return std::floor(v * 100.0 + 1e-9) / 100.0; }
// 隊1 贏這個系列賽的機率
static double lol_series_p1(const LolMatch& m) {
    if (m.odds1 > 1.0 && m.odds2 > 1.0) return (1.0 / m.odds1) / (1.0 / m.odds1 + 1.0 / m.odds2);
    // 查到的單場賠率（不抽水後）
    auto pair_is = [&](const char* a, const char* b) { return (m.team1 == a && m.team2 == b) || (m.team1 == b && m.team2 == a); };
    if (pair_is("Karmine Corp", "T1")) return 0.5;
    // 用冠軍機率推算：奪冠要連贏 3 個系列賽，所以強度取立方根再比
    double a = std::cbrt(lol_champ_prob(m.team1)), b = std::cbrt(lol_champ_prob(m.team2));
    return a / (a + b);
}
static double lol_comb(int n, int k) { double r = 1; for (int i = 1; i <= k; i++) r = r * (n - k + i) / i; return r; }
// 每一小局勝率 q → 系列賽勝率（先拿 w 局）
static double lol_series_from_game(double q, int w) {
    double s = 0;
    for (int l = 0; l < w; l++) s += lol_comb(w - 1 + l, l) * std::pow(q, w) * std::pow(1 - q, l);
    return s;
}
static double lol_game_q(double p, int w) {
    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < 60; i++) { double mid = (lo + hi) / 2; if (lol_series_from_game(mid, w) < p) lo = mid; else hi = mid; }
    return (lo + hi) / 2;
}
// 某個選項的賠率（不抽水＝1／機率，小數點第二位以下捨去）
static double lol_pick_odds(const LolMatch& m, char pool, const std::string& pick) {
    double p1 = lol_series_p1(m);
    if (pool == 'w') {
        if (pick == "1") return m.odds1 > 1.0 ? m.odds1 : lol_floor2(1.0 / p1);
        return m.odds2 > 1.0 ? m.odds2 : lol_floor2(1.0 / (1.0 - p1));
    }
    size_t d = pick.find('-');
    if (d == std::string::npos) return 0;
    int a = atoi(pick.c_str()), b = atoi(pick.c_str() + d + 1);
    int w = (m.bestof + 1) / 2;
    double q = lol_game_q(p1, w);
    double pr = a > b ? lol_comb(w - 1 + b, b) * std::pow(q, w) * std::pow(1 - q, b)
                      : lol_comb(w - 1 + a, a) * std::pow(1 - q, w) * std::pow(q, a);
    return pr > 0 ? lol_floor2(1.0 / pr) : 0;
}
static std::string lol_odds_str(double o) { std::ostringstream os; os << std::fixed << std::setprecision(2) << o; return os.str(); }
// 冠軍盤賠率：8 隊的冠軍機率重新加總成 100% 再換算
static std::vector<double> lol_default_champ_odds(const std::vector<std::string>& teams) {
    double tot = 0; for (auto& t : teams) tot += lol_champ_prob(t);
    std::vector<double> v;
    for (auto& t : teams) v.push_back(lol_floor2(tot / lol_champ_prob(t)));
    return v;
}
static double lol_champ_odds_at(int idx) {
    if (idx < 0 || idx >= (int)lol_champ.teams.size()) return 0;
    if (lol_champ.odds.size() != lol_champ.teams.size()) lol_champ.odds = lol_default_champ_odds(lol_champ.teams);
    return lol_champ.odds[idx];
}

// ─── 存檔／讀檔 ───────────────────────────────────────────────────────────────
static nlohmann::json lol_pool_to_json(const LolPool& p) {
    nlohmann::json j = nlohmann::json::object();
    for (auto& [u, b] : p.bets) j[std::to_string(u)] = {{"pick", b.pick}, {"amount", b.amount}, {"to_win", b.to_win}, {"payout", b.payout}};
    return j;
}
static void lol_pool_from_json(LolPool& p, const nlohmann::json& j) {
    if (!j.is_object()) return;
    for (auto& [k, v] : j.items()) {
        LolBet b; b.pick = v.value("pick", ""); b.amount = v.value("amount", (int64_t)0); b.payout = v.value("payout", (int64_t)-1);
        b.to_win = v.value("to_win", (int64_t)0);
        p.bets[std::stoull(k)] = b;
    }
}
static void save_lolbet() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        j["next_no"] = lol_next_no;
        j["announce_ch"] = std::to_string(lol_announce_ch);
        nlohmann::json ms = nlohmann::json::array();
        for (auto& m : lol_matches)
            ms.push_back({{"no", m.no}, {"key", m.key}, {"stage", m.stage}, {"team1", m.team1}, {"team2", m.team2},
                          {"start", (int64_t)m.start}, {"bestof", m.bestof}, {"status", m.status},
                          {"score1", m.score1}, {"score2", m.score2}, {"odds1", m.odds1}, {"odds2", m.odds2},
                          {"win_pool", lol_pool_to_json(m.win_pool)}, {"score_pool", lol_pool_to_json(m.score_pool)}});
        j["matches"] = ms;
        j["champ"] = {{"exists", lol_champ.exists}, {"teams", lol_champ.teams}, {"lock_time", (int64_t)lol_champ.lock_time},
                      {"status", lol_champ.status}, {"winner", lol_champ.winner}, {"odds", lol_champ.odds}, {"pool", lol_pool_to_json(lol_champ.pool)}};
        nlohmann::json st = nlohmann::json::object();
        for (auto& [u, s] : lol_stats) st[std::to_string(u)] = {{"bets", s.bets}, {"wins", s.wins}, {"profit", s.profit}};
        j["stats"] = st;
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(LOLBET_FILE, j.dump(2));
}
static void load_lolbet() {
    std::ifstream f(LOLBET_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        lol_next_no = j.value("next_no", 1);
        try { lol_announce_ch = std::stoull(j.value("announce_ch", std::string("0"))); } catch (...) { lol_announce_ch = 0; }
        lol_matches.clear();
        if (j.contains("matches"))
            for (auto& v : j["matches"]) {
                LolMatch m;
                m.no = v.value("no", 0); m.key = v.value("key", ""); m.stage = v.value("stage", "");
                m.team1 = v.value("team1", ""); m.team2 = v.value("team2", "");
                m.start = (time_t)v.value("start", (int64_t)0); m.bestof = v.value("bestof", 5);
                m.status = v.value("status", 0); m.score1 = v.value("score1", 0); m.score2 = v.value("score2", 0);
                m.odds1 = v.value("odds1", 0.0); m.odds2 = v.value("odds2", 0.0);
                if (v.contains("win_pool"))   lol_pool_from_json(m.win_pool, v["win_pool"]);
                if (v.contains("score_pool")) lol_pool_from_json(m.score_pool, v["score_pool"]);
                lol_matches.push_back(m);
            }
        if (j.contains("champ")) {
            auto& c = j["champ"];
            lol_champ.exists = c.value("exists", false);
            if (c.contains("teams")) lol_champ.teams = c["teams"].get<std::vector<std::string>>();
            lol_champ.lock_time = (time_t)c.value("lock_time", (int64_t)0);
            lol_champ.status = c.value("status", 0);
            lol_champ.winner = c.value("winner", "");
            if (c.contains("odds") && c["odds"].is_array()) lol_champ.odds = c["odds"].get<std::vector<double>>();
            if (c.contains("pool")) lol_pool_from_json(lol_champ.pool, c["pool"]);
        }
        if (j.contains("stats"))
            for (auto& [k, v] : j["stats"].items()) {
                auto& s = lol_stats[std::stoull(k)];
                s.bets = v.value("bets", 0); s.wins = v.value("wins", 0); s.profit = v.value("profit", (int64_t)0);
            }
    } catch (...) {}
}

// ─── 結算（呼叫端要持有 data_mutex，放掉後再 save_chips()／save_lolbet()）──────
// 固定賠率：押中的人拿回 to_win（下注 × 當時賠率），沒押中拿 0；refund=true 全部退回本金。回傳一行摘要。
static std::string lol_settle_pool_locked(LolPool& p, const std::string& win_pick, bool refund, const std::string& win_label) {
    if (p.bets.empty()) return "沒有人下注";
    if (refund) {
        for (auto& [u, b] : p.bets) { b.payout = b.amount; chip_data[dpp::snowflake(u)].chips += b.amount; }
        return "全部退回";
    }
    int winners = 0; int64_t paid = 0, staked = 0;
    for (auto& [u, b] : p.bets) {
        auto& st = lol_stats[u];
        staked += b.amount;
        if (b.pick == win_pick) {
            b.payout = b.to_win > 0 ? b.to_win : b.amount;
            chip_data[dpp::snowflake(u)].chips += b.payout;
            paid += b.payout; st.wins++; winners++;
        } else b.payout = 0;
        st.profit += b.payout - b.amount;
    }
    return std::to_string(winners) + "／" + std::to_string(p.bets.size()) + " 人押中 " + win_label
         + "，總押注 " + lol_fmt(staked) + " 碼、賠出 " + lol_fmt(paid) + " 碼";
}
// 這一盤賺最多的前幾名（公告用）
static std::string lol_top_winners(const std::vector<const LolPool*>& pools, int n = 5) {
    std::map<uint64_t, int64_t> net;
    for (auto* p : pools) for (auto& [u, b] : p->bets) if (b.payout >= 0) net[u] += b.payout - b.amount;
    std::vector<std::pair<uint64_t, int64_t>> v(net.begin(), net.end());
    v.erase(std::remove_if(v.begin(), v.end(), [](auto& x) { return x.second <= 0; }), v.end());
    std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; });
    std::string s;
    for (int i = 0; i < (int)v.size() && i < n; i++) s += "　" + std::to_string(i + 1) + ". <@" + std::to_string(v[i].first) + "> +" + lol_fmt(v[i].second) + " 碼\n";
    return s;
}
static std::string lol_settle_match_locked(LolMatch& m, int s1, int s2) {
    m.status = 2; m.score1 = s1; m.score2 = s2;
    std::string wp = s1 > s2 ? "1" : "2";
    std::string sp = std::to_string(s1) + "-" + std::to_string(s2);
    std::string t = "🏁 **" + m.stage + "** " + lol_short(m.team1) + " **" + std::to_string(s1) + ":" + std::to_string(s2) + "** " + lol_short(m.team2) + " 結算完成\n";
    t += "　🎯 勝負盤：" + lol_settle_pool_locked(m.win_pool, wp, false, lol_pick_label(m, 'w', wp)) + "\n";
    t += "　🔢 比分盤：" + lol_settle_pool_locked(m.score_pool, sp, false, lol_pick_label(m, 's', sp)) + "\n";
    std::string top = lol_top_winners({&m.win_pool, &m.score_pool});
    if (!top.empty()) t += "💰 這場賺最多\n" + top;
    return t;
}
static std::string lol_cancel_match_locked(LolMatch& m) {
    m.status = 3;
    lol_settle_pool_locked(m.win_pool, "", true, "");
    lol_settle_pool_locked(m.score_pool, "", true, "");
    return "❌ **" + m.stage + "** " + lol_short(m.team1) + " vs " + lol_short(m.team2) + " 已取消，下注全部退回。";
}
static std::string lol_settle_champ_locked(const std::string& winner) {
    lol_champ.status = 2; lol_champ.winner = winner;
    int idx = -1;
    for (int i = 0; i < (int)lol_champ.teams.size(); i++) if (lol_champ.teams[i] == winner) idx = i;
    std::string t = "🏆 **" + LOL_TITLE + " 冠軍：" + winner + "**！冠軍盤結算完成\n";
    t += "　" + lol_settle_pool_locked(lol_champ.pool, std::to_string(idx), false, lol_short(winner)) + "\n";
    std::string top = lol_top_winners({&lol_champ.pool});
    if (!top.empty()) t += "💰 冠軍盤賺最多\n" + top;
    return t;
}
static std::string lol_cancel_champ_locked() {
    lol_champ.status = 3;
    lol_settle_pool_locked(lol_champ.pool, "", true, "");
    return "❌ 冠軍盤已取消，下注全部退回。";
}

// 公告到設定的頻道（不 ping 人）
static void lol_announce(const std::vector<std::string>& texts) {
    if (!g_bot || texts.empty()) return;
    uint64_t ch;
    { std::lock_guard<std::mutex> lk(data_mutex); ch = lol_announce_ch; }
    if (!ch) return;
    for (auto& t : texts) {
        dpp::message m(dpp::snowflake(ch), t);
        m.set_allowed_mentions(false, false, false, false, {}, {});
        g_bot->message_create(m);
    }
}

// ─── 下注 ────────────────────────────────────────────────────────────────────
// target：比賽編號，或 0＝冠軍盤；pool：'w' 勝負／'s' 比分／'c' 冠軍。amount_str 可以是數字或「全押」。
// 成功回傳 "" 並把結果放進 ok_text。
static std::string lol_place_bet(dpp::snowflake uid, int target, char pool, const std::string& pick,
                                 std::string amount_str, std::string& ok_text) {
    amount_str.erase(std::remove_if(amount_str.begin(), amount_str.end(), [](char ch) { return ch == ',' || ch == ' '; }), amount_str.end());
    bool allin = (amount_str == "全押" || amount_str == "allin" || amount_str == "ALLIN" || amount_str == "全部");
    int64_t amount = 0;
    if (!allin) {
        if (amount_str.empty() || amount_str.size() > 15 || !std::all_of(amount_str.begin(), amount_str.end(), ::isdigit))
            return "請輸入數字（或「全押」）。";
        amount = std::stoll(amount_str);
    }
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        LolPool* p = nullptr; std::string label, where; double odds = 0;
        if (pool == 'c') {
            if (!lol_champ_open()) return "冠軍盤已經封盤了。";
            int idx = atoi(pick.c_str());
            if (idx < 0 || idx >= (int)lol_champ.teams.size()) return "找不到這支隊伍。";
            p = &lol_champ.pool; label = lol_champ.teams[idx]; where = "冠軍盤"; odds = lol_champ_odds_at(idx);
        } else {
            LolMatch* m = lol_find_match_locked(target);
            if (!m) return "找不到這場比賽。";
            if (!lol_match_open(*m)) return "這場已經封盤了。";
            if (pool == 'w') {
                if (pick != "1" && pick != "2") return "選項錯誤。";
                p = &m->win_pool; where = m->stage + " 勝負盤";
            } else {
                auto v = lol_score_picks(*m);
                if (std::find(v.begin(), v.end(), pick) == v.end()) return "選項錯誤。";
                p = &m->score_pool; where = m->stage + " 比分盤";
            }
            label = lol_pick_label(*m, pool, pick);
            odds = lol_pick_odds(*m, pool, pick);
        }
        if (odds <= 1.0) return "這個選項目前沒有賠率。";
        auto bit = p->bets.find((uint64_t)uid);
        int64_t already = 0;
        if (bit != p->bets.end()) {
            if (bit->second.pick != pick) return "你在" + where + "已經押了別的選項，同一個盤只能加碼同一個選項。";
            already = bit->second.amount;
        }
        int64_t have = chip_data[uid].chips;
        int64_t room = LOL_MAX_BET_POOL - already;
        if (room <= 0) return "你在" + where + "已經押到上限 " + lol_fmt(LOL_MAX_BET_POOL) + " 碼了。";
        if (allin) amount = std::min(have, room);
        if (amount < LOL_MIN_BET) return "最少要押 " + lol_fmt(LOL_MIN_BET) + " 碼。";
        if (amount > room) return "每個盤最多押 " + lol_fmt(LOL_MAX_BET_POOL) + " 碼，你還能再押 " + lol_fmt(room) + " 碼。";
        if (amount > have) return "籌碼不足（你有 " + lol_fmt(have) + " 碼）。";
        chip_data[uid].chips -= amount;
        auto& b = p->bets[(uint64_t)uid];
        if (b.amount == 0) lol_stats[(uint64_t)uid].bets++;
        b.pick = pick; b.amount += amount;
        int64_t win_now = (int64_t)std::floor((long double)amount * odds);
        b.to_win += win_now;
        ok_text = "✅ 在" + where + "押 **" + label + "** " + lol_fmt(amount) + " 碼（賠率 " + lol_odds_str(odds)
                + "，押中拿回 " + lol_fmt(win_now) + " 碼）"
                + (already ? "\n　加碼後共押 " + lol_fmt(b.amount) + " 碼，押中共拿回 " + lol_fmt(b.to_win) + " 碼" : "") + "。";
    }
    save_chips();
    save_lolbet();
    return "";
}

// ─── 畫面（Components V2，沒開嵌入內容的人也看得到）──────────────────────────
// eph=true：給私人訊息用的版本（按鈕會原地更新）；false：公開看板（按鈕開一則只有自己看得到的訊息）
static dpp::component lol_btn(const std::string& label, const std::string& id, dpp::component_style st = dpp::cos_secondary, bool disabled = false) {
    return dpp::component().set_type(dpp::cot_button).set_label(label).set_id(id).set_style(st).set_disabled(disabled);
}
static dpp::message lol_v2_msg(const std::string& text, uint32_t accent) {
    dpp::message msg; msg.set_flags(dpp::m_using_components_v2);
    dpp::component c; c.set_type(dpp::cot_container).set_accent(accent);
    c.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text));
    msg.add_component_v2(c);
    return msg;
}
static std::string lol_time_text(time_t t) {
    if (!t) return "開賽時間未定";
    std::string s = std::to_string((int64_t)t);
    return "<t:" + s + ":f>（<t:" + s + ":R>）";
}

static dpp::message make_lol_board_msg(bool eph, const std::string& notice = "") {
    std::string t = "## 🏆 " + LOL_TITLE + " 下注\n"
                    "固定賠率：押中拿回「下注 × 賠率」，賠率在你下注那一刻就固定。開賽就封盤，結果出來自動結算。\n";
    std::vector<std::pair<int, std::string>> open_btns;
    bool champ_exists;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        std::vector<const LolMatch*> order;
        for (auto& m : lol_matches) order.push_back(&m);
        std::stable_sort(order.begin(), order.end(), [](const LolMatch* a, const LolMatch* b) {
            time_t ta = a->start ? a->start : (time_t)1 << 40, tb = b->start ? b->start : (time_t)1 << 40;
            return ta != tb ? ta < tb : a->no < b->no;
        });
        for (auto* mp : order) {
            const LolMatch& m = *mp;
            if (m.status == 3) continue;
            t += "\n**" + m.stage + "**　" + m.team1 + " vs " + m.team2 + "\n　";
            if (m.status == 2)
                t += "✅ " + lol_short(m.team1) + " " + std::to_string(m.score1) + ":" + std::to_string(m.score2) + " " + lol_short(m.team2);
            else {
                bool open = lol_match_open(m);
                t += std::string(open ? "🟢 下注中" : "🔒 比賽中，已封盤") + "｜" + lol_time_text(m.start);
                t += "\n　賠率 " + lol_short(m.team1) + " **" + lol_odds_str(lol_pick_odds(m, 'w', "1")) + "**／" + lol_short(m.team2)
                   + " **" + lol_odds_str(lol_pick_odds(m, 'w', "2")) + "**｜已押 " + lol_fmt(lol_pool_total(m.win_pool) + lol_pool_total(m.score_pool)) + " 碼";
                if (open) open_btns.push_back({m.no, m.stage + " " + lol_short(m.team1) + " vs " + lol_short(m.team2)});
            }
        }
        if (lol_matches.empty()) t += "\n目前還沒有開盤的比賽。";
        champ_exists = lol_champ.exists && lol_champ.status != 3;
        if (champ_exists) {
            t += "\n\n**🏆 冠軍盤**　";
            if (lol_champ.status == 2) t += "✅ 冠軍：" + lol_champ.winner;
            else t += std::string(lol_champ_open() ? "🟢 下注中｜" + lol_time_text(lol_champ.lock_time) + " 封盤" : "🔒 已封盤，決賽打完結算");
            t += "｜已押 " + lol_fmt(lol_pool_total(lol_champ.pool)) + " 碼";
        }
    }
    if (!notice.empty()) t += "\n\n" + notice;
    dpp::message msg = lol_v2_msg(t, dpp::utility::rgb(0xC8, 0x9B, 0x3C));
    std::string e = eph ? "e" : "";
    for (size_t i = 0; i < open_btns.size(); i += 5) {
        dpp::component r; r.set_type(dpp::cot_action_row);
        for (size_t k = i; k < open_btns.size() && k < i + 5; k++)
            r.add_component(lol_btn("🎮 " + open_btns[k].second, "lol_m" + e + "_" + std::to_string(open_btns[k].first), dpp::cos_primary));
        msg.add_component_v2(r);
        if (i >= 10) break;
    }
    dpp::component r; r.set_type(dpp::cot_action_row);
    r.add_component(lol_btn("🏆 冠軍盤", "lol_c" + e, dpp::cos_success, !champ_exists));
    r.add_component(lol_btn("📋 我的下注", "lol_my" + e));
    r.add_component(lol_btn("🔄 重新整理", "lol_b" + e));
    msg.add_component_v2(r);
    if (eph) msg.set_flags(msg.flags | dpp::m_ephemeral);
    return msg;
}

static dpp::message make_lol_match_msg(dpp::snowflake uid, int no, const std::string& notice = "") {
    std::string t; bool open = false; std::vector<std::string> picks; std::string t1, t2, o1s, o2s;
    bool found = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        LolMatch* m = lol_find_match_locked(no);
        if (m) {
        found = true;
        open = lol_match_open(*m);
        t1 = lol_short(m->team1); t2 = lol_short(m->team2);
        o1s = lol_odds_str(lol_pick_odds(*m, 'w', "1")); o2s = lol_odds_str(lol_pick_odds(*m, 'w', "2"));
        picks = lol_score_picks(*m);
        t = "## 🎮 " + m->stage + "：" + m->team1 + " vs " + m->team2 + "\n";
        t += std::string(lol_status_name(open ? 0 : (m->status == 0 ? 1 : m->status))) + "｜" + lol_time_text(m->start) + "\n";
        int64_t wt = lol_pool_total(m->win_pool), st = lol_pool_total(m->score_pool);
        t += "\n**🎯 勝負盤**（已押 " + lol_fmt(wt) + " 碼）\n";
        for (std::string p : {"1", "2"})
            t += "　" + lol_pick_label(*m, 'w', p) + "：賠率 **" + lol_odds_str(lol_pick_odds(*m, 'w', p)) + "**　（"
               + std::to_string(lol_pick_count(m->win_pool, p)) + " 人押 " + lol_fmt(lol_pick_total(m->win_pool, p)) + " 碼）\n";
        t += "\n**🔢 比分盤**（已押 " + lol_fmt(st) + " 碼）\n";
        for (auto& p : picks)
            t += "　" + lol_pick_label(*m, 's', p) + "：賠率 **" + lol_odds_str(lol_pick_odds(*m, 's', p)) + "**　（"
               + std::to_string(lol_pick_count(m->score_pool, p)) + " 人）\n";
        t += "\n**你的下注**\n";
        auto show = [&](const LolPool& p, char pool, const char* name) {
            auto it = p.bets.find((uint64_t)uid);
            t += std::string("　") + name + "：";
            if (it == p.bets.end()) t += "還沒押\n";
            else {
                t += lol_pick_label(*m, pool, it->second.pick) + " " + lol_fmt(it->second.amount) + " 碼";
                if (it->second.payout >= 0) t += "　→ 拿回 " + lol_fmt(it->second.payout) + " 碼";
                else t += "　押中拿回 " + lol_fmt(it->second.to_win) + " 碼";
                t += "\n";
            }
        };
        show(m->win_pool, 'w', "勝負盤");
        show(m->score_pool, 's', "比分盤");
        t += "💰 你的籌碼：" + lol_fmt(chip_data[uid].chips) + " 碼\n-# 每個盤最少 " + lol_fmt(LOL_MIN_BET) + "、最多 "
           + lol_fmt(LOL_MAX_BET_POOL) + " 碼；同一個盤只能押一個選項（可以加碼）。";
        // 比分選項要帶隊名，先在鎖內算好（格式：pick|顯示文字）
        if (!open) picks.clear();
        for (auto& p : picks) p = p + "|" + lol_pick_label(*m, 's', p) + "（賠率 " + lol_odds_str(lol_pick_odds(*m, 's', p)) + "）";
        }
    }
    if (!found) return make_lol_board_msg(true, "❌ 找不到這場比賽。");
    if (!notice.empty()) t += "\n\n" + notice;
    dpp::message msg = lol_v2_msg(t, dpp::utility::rgb(0x34, 0x98, 0xDB));
    std::string ns = std::to_string(no);
    dpp::component r1; r1.set_type(dpp::cot_action_row);
    r1.add_component(lol_btn("押 " + t1 + " 贏（" + o1s + "）", "lol_bw_" + ns + "_1", dpp::cos_primary, !open));
    r1.add_component(lol_btn("押 " + t2 + " 贏（" + o2s + "）", "lol_bw_" + ns + "_2", dpp::cos_danger, !open));
    msg.add_component_v2(r1);
    if (open && !picks.empty()) {
        dpp::component r2; r2.set_type(dpp::cot_action_row);
        dpp::component sel; sel.set_type(dpp::cot_selectmenu).set_id("lol_ss_" + ns).set_placeholder("🔢 押比分…");
        for (auto& pl : picks) {
            size_t bar = pl.find('|');
            sel.add_select_option(dpp::select_option(pl.substr(bar + 1), pl.substr(0, bar)));
        }
        r2.add_component(sel);
        msg.add_component_v2(r2);
    }
    dpp::component r3; r3.set_type(dpp::cot_action_row);
    r3.add_component(lol_btn("🔄 重新整理", "lol_me_" + ns));
    r3.add_component(lol_btn("↩ 返回", "lol_be"));
    msg.add_component_v2(r3);
    msg.set_flags(msg.flags | dpp::m_ephemeral);
    return msg;
}

static dpp::message make_lol_champ_msg(dpp::snowflake uid, const std::string& notice = "") {
    std::string t; bool open = false; std::vector<std::pair<int, std::string>> opts;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (!lol_champ.exists) {
            t = "## 🏆 冠軍盤\n冠軍盤還沒開。";
        } else {
            open = lol_champ_open();
            auto out = lol_eliminated_locked();
            int64_t tot = lol_pool_total(lol_champ.pool);
            t = "## 🏆 冠軍盤：誰會拿下 " + LOL_TITLE + "？\n";
            t += std::string(lol_status_name(open ? 0 : (lol_champ.status == 0 ? 1 : lol_champ.status)));
            if (lol_champ.status == 0) t += "｜" + lol_time_text(lol_champ.lock_time) + " 封盤";
            if (lol_champ.status == 2) t += "｜冠軍：**" + lol_champ.winner + "**";
            t += "\n已押 " + lol_fmt(tot) + " 碼\n\n";
            for (int i = 0; i < (int)lol_champ.teams.size(); i++) {
                std::string p = std::to_string(i);
                const std::string& nm = lol_champ.teams[i];
                t += "　" + nm + (out.count(nm) ? "（已淘汰）" : "") + "：賠率 **" + lol_odds_str(lol_champ_odds_at(i)) + "**　（"
                   + std::to_string(lol_pick_count(lol_champ.pool, p)) + " 人押 " + lol_fmt(lol_pick_total(lol_champ.pool, p)) + " 碼）\n";
                if (!out.count(nm)) opts.push_back({i, nm + "（賠率 " + lol_odds_str(lol_champ_odds_at(i)) + "）"});
            }
            auto it = lol_champ.pool.bets.find((uint64_t)uid);
            t += "\n**你的下注**：";
            if (it == lol_champ.pool.bets.end()) t += "還沒押";
            else {
                int idx = atoi(it->second.pick.c_str());
                t += (idx >= 0 && idx < (int)lol_champ.teams.size() ? lol_champ.teams[idx] : "?") + " " + lol_fmt(it->second.amount) + " 碼";
                if (it->second.payout >= 0) t += "　→ 拿回 " + lol_fmt(it->second.payout) + " 碼";
                else t += "　押中拿回 " + lol_fmt(it->second.to_win) + " 碼";
            }
            t += "\n💰 你的籌碼：" + lol_fmt(chip_data[uid].chips) + " 碼";
        }
    }
    if (!notice.empty()) t += "\n\n" + notice;
    dpp::message msg = lol_v2_msg(t, dpp::utility::rgb(0xF1, 0xC4, 0x0F));
    if (open && !opts.empty()) {
        dpp::component r; r.set_type(dpp::cot_action_row);
        dpp::component sel; sel.set_type(dpp::cot_selectmenu).set_id("lol_cs").set_placeholder("🏆 押冠軍隊伍…");
        for (auto& [i, nm] : opts) sel.add_select_option(dpp::select_option(nm, std::to_string(i)));
        r.add_component(sel);
        msg.add_component_v2(r);
    }
    dpp::component r3; r3.set_type(dpp::cot_action_row);
    r3.add_component(lol_btn("🔄 重新整理", "lol_ce"));
    r3.add_component(lol_btn("↩ 返回", "lol_be"));
    msg.add_component_v2(r3);
    msg.set_flags(msg.flags | dpp::m_ephemeral);
    return msg;
}

static dpp::message make_lol_my_msg(dpp::snowflake uid) {
    std::string t = "## 📋 我的世界賽下注\n";
    int n = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& m : lol_matches) {
            for (auto [pool, pp, name] : {std::tuple<char, const LolPool*, const char*>{'w', &m.win_pool, "勝負"},
                                          std::tuple<char, const LolPool*, const char*>{'s', &m.score_pool, "比分"}}) {
                auto it = pp->bets.find((uint64_t)uid);
                if (it == pp->bets.end()) continue;
                n++;
                t += "　**" + m.stage + "** " + name + "：" + lol_pick_label(m, pool, it->second.pick) + " " + lol_fmt(it->second.amount) + " 碼　";
                if (it->second.payout < 0) t += std::string(lol_status_name(lol_match_open(m) ? 0 : 1)) + "，押中拿回 " + lol_fmt(it->second.to_win);
                else if (m.status == 3) t += "❌ 取消，已退回";
                else t += it->second.payout > 0 ? "✅ 拿回 " + lol_fmt(it->second.payout) : "💸 沒押中";
                t += "\n";
            }
        }
        auto it = lol_champ.pool.bets.find((uint64_t)uid);
        if (lol_champ.exists && it != lol_champ.pool.bets.end()) {
            n++;
            int idx = atoi(it->second.pick.c_str());
            t += "　**冠軍盤**：" + (idx >= 0 && idx < (int)lol_champ.teams.size() ? lol_champ.teams[idx] : "?") + " " + lol_fmt(it->second.amount) + " 碼　";
            if (it->second.payout < 0) t += std::string(lol_status_name(lol_champ_open() ? 0 : 1)) + "，押中拿回 " + lol_fmt(it->second.to_win);
            else if (lol_champ.status == 3) t += "❌ 取消，已退回";
            else t += it->second.payout > 0 ? "✅ 拿回 " + lol_fmt(it->second.payout) : "💸 沒押中";
            t += "\n";
        }
        if (n == 0) t += "你還沒有下注。\n";
        auto sit = lol_stats.find((uint64_t)uid);
        if (sit != lol_stats.end() && sit->second.bets > 0)
            t += "\n📊 累計：下注 " + std::to_string(sit->second.bets) + " 次、押中 " + std::to_string(sit->second.wins)
               + " 次、盈虧 " + (sit->second.profit >= 0 ? "+" : "") + lol_fmt(sit->second.profit) + " 碼";
    }
    dpp::message msg = lol_v2_msg(t, dpp::utility::rgb(0x95, 0xA5, 0xA6));
    dpp::component r; r.set_type(dpp::cot_action_row);
    r.add_component(lol_btn("↩ 返回", "lol_be"));
    msg.add_component_v2(r);
    msg.set_flags(msg.flags | dpp::m_ephemeral);
    return msg;
}

// 錢包「遊戲統計」用的一行
static std::string lol_stats_line(dpp::snowflake uid) {
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = lol_stats.find((uint64_t)uid);
    if (it == lol_stats.end() || it->second.bets == 0) return "尚無紀錄";
    auto& s = it->second;
    return "下注 **" + std::to_string(s.bets) + "** 次　押中 **" + std::to_string(s.wins) + "** 次　盈虧 **"
         + (s.profit >= 0 ? "+" : "") + lol_fmt(s.profit) + "**";
}

static dpp::interaction_modal_response lol_amount_modal(const std::string& id, const std::string& title, const std::string& label) {
    dpp::interaction_modal_response modal(id, title.substr(0, 45));
    modal.add_component(dpp::component().set_type(dpp::cot_text)
        .set_label(label.size() > 45 ? label.substr(0, 45) : label)
        .set_id("amt").set_text_style(dpp::text_short)
        .set_min_length(1).set_max_length(12).set_placeholder("輸入籌碼數量，或打「全押」"));
    return modal;
}

// ─── 按鈕／選單／表單（回傳 true＝已處理）──────────────────────────────────────
static bool lol_handle_button(const dpp::button_click_t& ev) {
    const std::string& cid = ev.custom_id;
    if (cid.rfind("lol_", 0) != 0) return false;
    dpp::snowflake uid = ev.command.get_issuing_user().id;
    if (cid == "lol_b")  { ev.reply(dpp::ir_update_message, make_lol_board_msg(false)); return true; }
    if (cid == "lol_be") { ev.reply(dpp::ir_update_message, make_lol_board_msg(true)); return true; }
    if (cid == "lol_c")  { ev.reply(dpp::ir_channel_message_with_source, make_lol_champ_msg(uid)); return true; }
    if (cid == "lol_ce") { ev.reply(dpp::ir_update_message, make_lol_champ_msg(uid)); return true; }
    if (cid == "lol_my") { ev.reply(dpp::ir_channel_message_with_source, make_lol_my_msg(uid)); return true; }
    if (cid == "lol_mye"){ ev.reply(dpp::ir_update_message, make_lol_my_msg(uid)); return true; }
    if (cid.rfind("lol_me_", 0) == 0) { ev.reply(dpp::ir_update_message, make_lol_match_msg(uid, atoi(cid.c_str() + 7))); return true; }
    if (cid.rfind("lol_m_", 0) == 0)  { ev.reply(dpp::ir_channel_message_with_source, make_lol_match_msg(uid, atoi(cid.c_str() + 6))); return true; }
    if (cid.rfind("lol_bw_", 0) == 0) { // lol_bw_<no>_<1|2>
        std::string rest = cid.substr(7);
        size_t us = rest.find('_');
        if (us == std::string::npos) return true;
        int no = atoi(rest.c_str()); std::string pick = rest.substr(us + 1);
        std::string title, label;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            LolMatch* m = lol_find_match_locked(no);
            if (m) { title = m->stage + " 勝負盤"; label = "押 " + lol_pick_label(*m, 'w', pick) + " 贏，要押多少碼？"; }
        }
        if (title.empty()) { ev.reply(dpp::ir_update_message, make_lol_board_msg(true, "❌ 找不到這場比賽。")); return true; }
        ev.dialog(lol_amount_modal("lol_bm_" + std::to_string(no) + "_w_" + pick, title, label));
        return true;
    }
    return true;
}

static bool lol_handle_select(const dpp::select_click_t& ev) {
    const std::string& cid = ev.custom_id;
    if (cid.rfind("lol_", 0) != 0) return false;
    if (ev.values.empty()) return true;
    const std::string& v = ev.values[0];
    if (cid.rfind("lol_ss_", 0) == 0) {
        int no = atoi(cid.c_str() + 7);
        std::string title, label;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            LolMatch* m = lol_find_match_locked(no);
            if (m) { title = m->stage + " 比分盤"; label = "押 " + lol_pick_label(*m, 's', v) + "，要押多少碼？"; }
        }
        if (title.empty()) { ev.reply(dpp::ir_update_message, make_lol_board_msg(true, "❌ 找不到這場比賽。")); return true; }
        ev.dialog(lol_amount_modal("lol_bm_" + std::to_string(no) + "_s_" + v, title, label));
        return true;
    }
    if (cid == "lol_cs") {
        std::string label;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            int idx = atoi(v.c_str());
            if (idx >= 0 && idx < (int)lol_champ.teams.size()) label = "押 " + lol_short(lol_champ.teams[idx]) + " 拿冠軍，要押多少碼？";
        }
        if (label.empty()) return true;
        ev.dialog(lol_amount_modal("lol_bm_0_c_" + v, "冠軍盤", label));
        return true;
    }
    return true;
}

static bool lol_handle_form(const dpp::form_submit_t& ev) {
    const std::string& cid = ev.custom_id;
    if (cid.rfind("lol_bm_", 0) != 0) return false;
    // lol_bm_<no>_<pool>_<pick>
    std::string rest = cid.substr(7);
    size_t a = rest.find('_');
    if (a == std::string::npos || a + 2 >= rest.size()) return true;
    int no = atoi(rest.c_str());
    char pool = rest[a + 1];
    std::string pick = rest.substr(a + 3);
    std::string input;
    for (auto& row : ev.components) {
        if (std::holds_alternative<std::string>(row.value)) input = std::get<std::string>(row.value);
        for (auto& sub : row.components) if (std::holds_alternative<std::string>(sub.value)) input = std::get<std::string>(sub.value);
    }
    dpp::snowflake uid = ev.command.get_issuing_user().id;
    std::string ok;
    std::string err = lol_place_bet(uid, no, pool, pick, input, ok);
    std::string notice = err.empty() ? ok : "❌ " + err;
    ev.reply(dpp::ir_update_message, pool == 'c' ? make_lol_champ_msg(uid, notice) : make_lol_match_msg(uid, no, notice));
    return true;
}

// ─── 自動封盤（每分鐘）──────────────────────────────────────────────────────
static void lol_tick() {
    bool changed = false;
    time_t now = time(nullptr);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& m : lol_matches) if (m.status == 0 && m.start > 0 && now >= m.start) { m.status = 1; changed = true; }
        if (lol_champ.exists && lol_champ.status == 0 && lol_champ.lock_time > 0 && now >= lol_champ.lock_time) { lol_champ.status = 1; changed = true; }
    }
    if (changed) save_lolbet();
}

// ─── Leaguepedia 同步 ────────────────────────────────────────────────────────
static std::string lol_urlenc(const std::string& s) {
    std::ostringstream o;
    for (unsigned char ch : s) {
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') o << ch;
        else o << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)ch << std::nouppercase << std::dec;
    }
    return o.str();
}
static time_t lol_parse_utc(const std::string& s) {
    int Y, M, D, h, m, sec = 0;
    if (sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &sec) < 5) return 0;
    std::tm tm{}; tm.tm_year = Y - 1900; tm.tm_mon = M - 1; tm.tm_mday = D; tm.tm_hour = h; tm.tm_min = m; tm.tm_sec = sec;
#ifdef _WIN32
    return _mkgmtime(&tm);
#else
    return timegm(&tm);
#endif
}
// 把一份 Leaguepedia 回傳套用到資料上；回傳要公告的訊息。呼叫端持有 data_mutex。
static std::vector<std::string> lol_apply_schedule_locked(const nlohmann::json& rows, bool& changed) {
    std::vector<std::string> ann;
    std::string finals_winner;
    for (auto& r : rows) {
        if (!r.contains("title")) continue;
        auto& v = r["title"];
        auto gs = [&](const char* k) { return v.contains(k) && v[k].is_string() ? v[k].get<std::string>() : std::string(); };
        std::string tab = gs("Tab"), key = gs("MatchId");
        std::string stage;
        std::string num; { size_t p = key.rfind('_'); if (p != std::string::npos) num = key.substr(p + 1); }
        if (tab == "Quarterfinals") stage = "八強 " + num;
        else if (tab == "Semifinals") stage = "四強 " + num;
        else if (tab == "Finals") stage = "決賽";
        else continue;
        std::string t1 = lol_clean_team(gs("Team1")), t2 = lol_clean_team(gs("Team2"));
        if (t1.empty() || t2.empty() || t1 == "TBD" || t2 == "TBD") continue;
        time_t start = lol_parse_utc(gs("DateTime UTC"));
        int bo = atoi(gs("BestOf").c_str()); if (bo <= 0) bo = 5;
        std::string winner = gs("Winner");
        int s1 = atoi(gs("Team1Score").c_str()), s2 = atoi(gs("Team2Score").c_str());

        LolMatch* m = lol_find_match_key_locked(key);
        if (!m) {
            if (winner == "1" || winner == "2") continue; // 已經打完才第一次看到的就不開盤
            LolMatch nm; nm.no = lol_next_no++; nm.key = key; nm.stage = stage; nm.team1 = t1; nm.team2 = t2;
            nm.start = start; nm.bestof = bo;
            lol_matches.push_back(nm); changed = true;
            ann.push_back("🟢 **" + stage + "** " + t1 + " vs " + t2 + " 開盤了！" + lol_time_text(start) + " 開打，用 `/世界賽` 下注。");
            continue;
        }
        if (m->status == 0 && start && m->start != start) { m->start = start; changed = true; }
        if ((m->status == 0 || m->status == 1) && (winner == "1" || winner == "2")) {
            int need = (m->bestof + 1) / 2;
            bool valid = std::max(s1, s2) == need && std::min(s1, s2) < need && ((winner == "1") == (s1 > s2));
            if (valid) { ann.push_back(lol_settle_match_locked(*m, s1, s2)); changed = true; }
        }
        if (tab == "Finals" && m->status == 2) finals_winner = m->score1 > m->score2 ? m->team1 : m->team2;
    }
    // 冠軍盤：八強四場都排好、第一場還沒開打就自動開
    if (!lol_champ.exists) {
        std::vector<LolMatch*> qf;
        for (auto& m : lol_matches) if (m.stage.rfind("八強", 0) == 0) qf.push_back(&m);
        if (qf.size() == 4) {
            time_t first = 0;
            for (auto* m : qf) if (m->start && (!first || m->start < first)) first = m->start;
            if (first && time(nullptr) < first) {
                lol_champ = LolChamp{};
                lol_champ.exists = true; lol_champ.lock_time = first;
                for (auto* m : qf) { lol_champ.teams.push_back(m->team1); lol_champ.teams.push_back(m->team2); }
                lol_champ.odds = lol_default_champ_odds(lol_champ.teams);
                changed = true;
                ann.push_back("🏆 **冠軍盤開盤了！** 八強 8 隊任你押，" + lol_time_text(first) + " 封盤，用 `/世界賽` 下注。");
            }
        }
    }
    if (lol_champ.exists && (lol_champ.status == 0 || lol_champ.status == 1) && !finals_winner.empty()) {
        if (std::find(lol_champ.teams.begin(), lol_champ.teams.end(), finals_winner) != lol_champ.teams.end()) {
            ann.push_back(lol_settle_champ_locked(finals_winner)); changed = true;
        }
    }
    return ann;
}

// done(msg)：手動同步時回報結果；自動同步傳 nullptr
static void lol_sync(dpp::cluster& bot, std::function<void(const std::string&)> done = nullptr) {
    std::string where = "OverviewPage=\"" + LOL_OVERVIEW_PAGE + "\" AND Tab IN (\"Quarterfinals\",\"Semifinals\",\"Finals\")";
    std::string url = "https://lol.fandom.com/api.php?action=cargoquery&format=json&limit=50"
                      "&tables=MatchSchedule"
                      "&fields=" + lol_urlenc("Team1,Team2,Team1Score,Team2Score,Winner,DateTime_UTC,Tab,MatchId,BestOf") +
                      "&where=" + lol_urlenc(where) +
                      "&order_by=" + lol_urlenc("DateTime_UTC");
    bot.request(url, dpp::m_get, [done](const dpp::http_request_completion_t& r) {
        std::string err;
        std::vector<std::string> ann;
        bool changed = false, settled = false;
        if (r.status != 200) err = "HTTP " + std::to_string(r.status);
        else {
            try {
                auto j = nlohmann::json::parse(r.body);
                if (j.contains("error")) err = j["error"].value("info", std::string("未知錯誤"));
                else if (!j.contains("cargoquery")) err = "回傳格式不對";
                else {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    ann = lol_apply_schedule_locked(j["cargoquery"], changed);
                    for (auto& a : ann) if (a.find("結算完成") != std::string::npos) settled = true;
                }
            } catch (const std::exception& e) { err = std::string("解析失敗：") + e.what(); }
        }
        if (!err.empty()) { fprintf(stderr, "[lolbet] Leaguepedia 同步失敗：%s\n", err.c_str()); fflush(stderr); }
        if (settled) save_chips();
        if (changed) save_lolbet();
        lol_announce(ann);
        if (done) done(err.empty() ? ("✅ 同步完成" + (ann.empty() ? std::string("，沒有變動。") : "，" + std::to_string(ann.size()) + " 項更新（已公告）。"))
                                   : "❌ 同步失敗：" + err);
    }, "", "application/json", {{"User-Agent", "discordbot-BB/1.0 (worlds betting; contact via Discord)"}});
}

// ─── 管理員指令：!世界賽 <子指令> ...（呼叫端已確認權限）────────────────────────
static std::string lol_admin_cmd(dpp::cluster& bot, dpp::snowflake ch, const std::vector<std::string>& a,
                                 std::function<void(const std::string&)> reply_later) {
    const std::string usage =
        "**世界賽管理指令**\n"
        "`!世界賽 開盤 隊1 隊2 [名稱]`：手動開一場（沒有開賽時間，要手動封盤）\n"
        "`!世界賽 封盤 編號`　`!世界賽 結算 編號 3:1`（隊1:隊2）　`!世界賽 取消 編號`\n"
        "`!世界賽 冠軍結算 隊名`　`!世界賽 冠軍取消`\n"
        "`!世界賽 賠率 編號 隊1賠率 隊2賠率`：改勝負賠率（比分賠率跟著重算；`!世界賽 賠率 編號 自動` 改回估計值）\n"
        "`!世界賽 冠軍賠率 隊名 賠率`：改冠軍盤某隊的賠率（已押的不受影響）\n"
        "`!世界賽 同步`：馬上從 Leaguepedia 抓一次　`!世界賽 頻道`：開盤／結算公告發在這個頻道";
    if (a.empty()) return usage;
    const std::string& sub = a[0];
    std::vector<std::string> ann; std::string res; bool chips_changed = false;
    if (sub == "同步") { lol_sync(bot, reply_later); return "⏳ 同步中…"; }
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (sub == "頻道") { lol_announce_ch = (uint64_t)ch; res = "✅ 之後世界賽的開盤／結算公告會發在這個頻道。"; }
        else if (sub == "開盤") {
            if (a.size() < 3) return usage;
            LolMatch m; m.no = lol_next_no++; m.key = "manual_" + std::to_string(m.no);
            m.team1 = a[1]; m.team2 = a[2];
            m.stage = "第 " + std::to_string(m.no) + " 場";
            if (a.size() >= 4) { m.stage = a[3]; for (size_t i = 4; i < a.size(); i++) m.stage += " " + a[i]; }
            lol_matches.push_back(m);
            res = "✅ 開盤：#" + std::to_string(m.no) + " " + m.stage + " " + m.team1 + " vs " + m.team2 + "（記得開打前 `!世界賽 封盤 " + std::to_string(m.no) + "`）";
        }
        else if (sub == "封盤" || sub == "結算" || sub == "取消") {
            if (a.size() < 2) return usage;
            LolMatch* m = lol_find_match_locked(atoi(a[1].c_str()));
            if (!m) return "❌ 找不到編號 " + a[1] + "。";
            if (m->status >= 2) return "❌ 這場已經" + std::string(m->status == 2 ? "結算" : "取消") + "過了。";
            if (sub == "封盤") { m->status = 1; res = "🔒 已封盤：" + m->stage; }
            else if (sub == "取消") { ann.push_back(lol_cancel_match_locked(*m)); chips_changed = true; res = "✅ 已取消並退款。"; }
            else {
                if (a.size() < 3) return usage;
                int s1 = -1, s2 = -1;
                if (sscanf(a[2].c_str(), "%d:%d", &s1, &s2) != 2 && sscanf(a[2].c_str(), "%d-%d", &s1, &s2) != 2) return "❌ 比分格式：3:1（隊1:隊2）";
                int need = (m->bestof + 1) / 2;
                if (std::max(s1, s2) != need || std::min(s1, s2) < 0 || std::min(s1, s2) >= need)
                    return "❌ BO" + std::to_string(m->bestof) + " 的比分要像 " + std::to_string(need) + ":1 這樣。";
                ann.push_back(lol_settle_match_locked(*m, s1, s2)); chips_changed = true; res = "✅ 已結算。";
            }
        }
        else if (sub == "賠率") {
            if (a.size() < 3) return usage;
            LolMatch* m = lol_find_match_locked(atoi(a[1].c_str()));
            if (!m) return "❌ 找不到編號 " + a[1] + "。";
            if (a[2] == "自動") { m->odds1 = m->odds2 = 0; }
            else {
                if (a.size() < 4) return usage;
                double o1 = atof(a[2].c_str()), o2 = atof(a[3].c_str());
                if (o1 <= 1.0 || o2 <= 1.0 || o1 > 100 || o2 > 100) return "❌ 賠率要大於 1、小於 100，例如 `!世界賽 賠率 1 1.85 1.95`";
                m->odds1 = o1; m->odds2 = o2;
            }
            res = "✅ " + m->stage + " 賠率：" + lol_short(m->team1) + " " + lol_odds_str(lol_pick_odds(*m, 'w', "1")) + "／"
                + lol_short(m->team2) + " " + lol_odds_str(lol_pick_odds(*m, 'w', "2")) + "（已押的不受影響）";
        }
        else if (sub == "冠軍賠率") {
            if (!lol_champ.exists || lol_champ.status >= 2) return "❌ 冠軍盤不存在或已經結算／取消。";
            if (a.size() < 3) return usage;
            double o = atof(a.back().c_str());
            std::string q = a[1]; for (size_t i = 2; i + 1 < a.size(); i++) q += " " + a[i];
            if (o <= 1.0 || o > 1000) return "❌ 賠率要大於 1、小於 1000。";
            int idx = -1;
            for (int i = 0; i < (int)lol_champ.teams.size(); i++) if (lol_champ.teams[i] == q || lol_short(lol_champ.teams[i]) == q) idx = i;
            if (idx < 0) return "❌ 冠軍盤裡沒有「" + q + "」。";
            lol_champ_odds_at(idx); // 確保 odds 陣列已建立
            lol_champ.odds[idx] = o;
            res = "✅ 冠軍盤 " + lol_champ.teams[idx] + " 賠率改成 " + lol_odds_str(o) + "（已押的不受影響）";
        }
        else if (sub == "冠軍結算") {
            if (!lol_champ.exists || lol_champ.status >= 2) return "❌ 冠軍盤不存在或已經結算／取消。";
            if (a.size() < 2) return usage;
            std::string q = a[1]; for (size_t i = 2; i < a.size(); i++) q += " " + a[i];
            std::string found;
            for (auto& t : lol_champ.teams) if (t == q || lol_short(t) == q) found = t;
            if (found.empty()) return "❌ 冠軍盤裡沒有「" + q + "」。";
            ann.push_back(lol_settle_champ_locked(found)); chips_changed = true; res = "✅ 冠軍盤已結算。";
        }
        else if (sub == "冠軍取消") {
            if (!lol_champ.exists || lol_champ.status >= 2) return "❌ 冠軍盤不存在或已經結算／取消。";
            ann.push_back(lol_cancel_champ_locked()); chips_changed = true; res = "✅ 冠軍盤已取消並退款。";
        }
        else return usage;
    }
    if (chips_changed) save_chips();
    save_lolbet();
    lol_announce(ann);
    return res;
}
