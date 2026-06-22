#pragma once
#include "chips.h"
#include <random>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <cstdlib>

// ─── Persistence ──────────────────────────────────────────────────────────────

static const std::string SHOOT_STATS_FILE = "shootstats.json";

static void load_shootstats() {
    std::ifstream f(SHOOT_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = shoot_stats_data[uid];
            s.wins   = v.value("wins",   0);
            s.losses = v.value("losses", 0);
            s.bumps  = v.value("bumps",  0);
            s.passes = v.value("passes", 0);
            s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}

static void save_shootstats() {
    nlohmann::json j;
    std::lock_guard<std::mutex> lk(data_mutex);
    for (auto& [uid, s] : shoot_stats_data)
        j[std::to_string((uint64_t)uid)] = {
            {"wins",   s.wins},   {"losses", s.losses},
            {"bumps",  s.bumps},  {"passes", s.passes},
            {"profit", s.profit}};
    atomic_write(SHOOT_STATS_FILE, j.dump(2));
}

// ─── Card display ─────────────────────────────────────────────────────────────

static const char* SH_SUITS[] = {"♠","♥","♦","♣"};

static std::string sh_rank(int r) {
    if (r == 1)  return "A";
    if (r == 11) return "J";
    if (r == 12) return "Q";
    if (r == 13) return "K";
    return std::to_string(r);
}

// Emoji card (same encoding as blackjack: suit prefix B/H/C/M + rank A-K)
static std::string sh_card(int c) {
    static const char* PREFIX[4] = { "B", "H", "C", "M" };
    static const char* RANK[14]  = { "","A","2","3","4","5","6","7","8","9","10","J","Q","K" };
    std::string ename = std::string(PREFIX[c%4]) + RANK[c/4+1];
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = emoji_name_map.find(ename);
        if (it != emoji_name_map.end()) return it->second;
    }
    return sh_rank(c/4+1) + SH_SUITS[c%4]; // fallback
}

// ─── Payout by gap ────────────────────────────────────────────────────────────
// gap=0 same rank (射上/射下), gap=1 auto re-deal, gap>=2 normal shoot

static int sh_payout(int gap) {
    switch (gap) {
        case 0: return 2;
        case 2: return 12;
        case 3: return 6;
        case 4: return 4;
        case 5: return 3;
        default: return 2;  // gap>=6 (gap==1 never reaches here)
    }
}

// ─── Dealing ──────────────────────────────────────────────────────────────────

static std::mt19937& sh_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

// Draw 2 cards; auto re-deal if gap==1 (adjacent, no winning card possible)
static void sh_deal(ShootGame& sg) {
    std::vector<int> deck(52);
    std::iota(deck.begin(), deck.end(), 0);
    do {
        std::shuffle(deck.begin(), deck.end(), sh_rng());
        sg.c1 = deck[0];
        sg.c2 = deck[1];
    } while (std::abs((sg.c1/4+1) - (sg.c2/4+1)) == 1);
}

static int sh_draw_third(int c1, int c2) {
    std::vector<int> rem;
    rem.reserve(50);
    for (int i = 0; i < 52; i++)
        if (i != c1 && i != c2) rem.push_back(i);
    return rem[std::uniform_int_distribution<int>(0, 49)(sh_rng())];
}

// ─── Stats summary line ───────────────────────────────────────────────────────

static std::string sh_stats_line(dpp::snowflake uid) {
    ShootStats st;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = shoot_stats_data.find(uid);
        if (it != shoot_stats_data.end()) st = it->second;
    }
    int total = st.wins + st.losses + st.bumps;
    std::string wr = (total > 0)
        ? std::to_string(st.wins * 100 / total) + "%"
        : "—";
    std::string profit_s = (st.profit >= 0)
        ? "+" + std::to_string(st.profit)
        : std::to_string(st.profit);
    return "勝率 **" + wr + "**　盈虧 **" + profit_s + "** 碼";
}

// ─── Start message ────────────────────────────────────────────────────────────

static void sh_set_user(dpp::embed& e, const ShootGame& sg) {
    if (!sg.avatar_url.empty()) e.set_thumbnail(sg.avatar_url);
    e.set_footer(dpp::embed_footer().set_text("👤 " + (sg.display_name.empty() ? std::to_string((uint64_t)sg.uid) : sg.display_name)));
}

static dpp::message make_shoot_start_msg(const ShootGame& sg) {
    int r1 = sg.c1/4+1, r2 = sg.c2/4+1;
    int lo_c = (r1 <= r2) ? sg.c1 : sg.c2;
    int hi_c = (r1 <= r2) ? sg.c2 : sg.c1;
    int lo = lo_c/4+1, hi = hi_c/4+1;
    int gap = hi - lo;
    int pay = sh_payout(gap);

    dpp::embed e;
    e.set_title("🃏  射龍門").set_color(0xE67E22);

    std::ostringstream desc;
    desc << "下柱：**" << sg.bet << "** 碼\n";
    if (gap == 0) desc << "同點牌 — 請選擇方向\n";
    int64_t pass_cost = std::max((int64_t)1, sg.bet / 5);
    desc << "賠率 **" << pay << "x**\n";
    desc << "✅ 射中  **+" << sg.bet*(pay-1) << "** 碼\n";
    desc << "❌ 射偏  **-" << sg.bet << "** 碼\n";
    desc << "💥 撞柱  **-" << sg.bet*2 << "** 碼\n";
    desc << "🙅 PASS  **-" << pass_cost << "** 碼（退還 80%）";
    e.set_description(desc.str());
    sh_set_user(e, sg);

    std::string sid = std::to_string((uint64_t)sg.uid);
    dpp::component row;
    row.set_type(dpp::cot_action_row);

    if (gap == 0) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("射上 ↑").set_id("shoot_up_" + sid).set_style(dpp::cos_primary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("射下 ↓").set_id("shoot_dn_" + sid).set_style(dpp::cos_primary));
    } else {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("射！🎯").set_id("shoot_go_" + sid).set_style(dpp::cos_danger));
    }
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("PASS").set_id("shoot_pass_" + sid).set_style(dpp::cos_secondary));

    dpp::message msg;
    msg.set_content(sh_card(lo_c) + "  " + sh_card(hi_c));
    msg.add_embed(e);
    msg.add_component(row);
    return msg;
}

// ─── After-game buttons ───────────────────────────────────────────────────────

static void sh_add_replay_row(dpp::message& msg, uint64_t uid_u, int64_t bet, int64_t cur_chips) {
    int64_t dbl = bet * 2;
    std::string sid = std::to_string(uid_u);
    dpp::component row;
    row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("再來一局").set_id("shoot_again_" + sid + "_" + std::to_string(bet))
        .set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("雙倍下柱（" + std::to_string(dbl) + "）")
        .set_id("shoot_again_" + sid + "_" + std::to_string(dbl))
        .set_style(dpp::cos_success)
        .set_disabled(cur_chips < dbl));
    msg.add_component(row);
}

// ─── Shoot result ─────────────────────────────────────────────────────────────
// direction: 0 = normal shoot (g>=2), 1 = up (g=0), -1 = down (g=0)
// Caller must NOT hold data_mutex (add_chips acquires it internally)

static dpp::message make_shoot_result_msg(const ShootGame& sg, int direction) {
    int r1 = sg.c1/4+1, r2 = sg.c2/4+1;
    int lo_c = (r1 <= r2) ? sg.c1 : sg.c2;
    int hi_c = (r1 <= r2) ? sg.c2 : sg.c1;
    int lo = lo_c/4+1, hi = hi_c/4+1;
    int gap = hi - lo;
    int pay = sh_payout(gap);

    int c3 = sh_draw_third(sg.c1, sg.c2);
    int r3 = c3/4+1;

    bool bump, win;
    if (gap == 0) {
        bump = (r3 == lo);
        win  = !bump && (direction == 1 ? r3 > lo : r3 < lo);
    } else {
        bump = (r3 == lo || r3 == hi);
        win  = !bump && (r3 > lo && r3 < hi);
    }

    int64_t delta;
    std::string title;
    uint32_t color;
    if (win) {
        delta = sg.bet * (pay - 1);
        title = "✅  射中！";
        color = 0x2ECC71;
    } else if (bump) {
        delta = -(sg.bet * 2);
        title = "💥  撞柱！";
        color = 0xFF6B00;
    } else {
        delta = -sg.bet;
        title = "❌  射偏！";
        color = 0x95A5A6;
    }

    add_chips(sg.uid, delta);
    int64_t new_chips = get_chips(sg.uid);
    if (delta < 0 && new_chips <= 0) announce_bankrupt(sg.uid, sg.channel_id);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = shoot_stats_data[sg.uid];
        if (win)       { st.wins++;   st.profit += delta; }
        else if (bump) { st.bumps++;  st.profit += delta; }
        else           { st.losses++; st.profit += delta; }
    }
    save_shootstats();

    dpp::embed e;
    e.set_title(title).set_color(color);

    std::string cards;
    if (gap == 0) {
        std::string dir_label = (direction == 1) ? "↑" : "↓";
        cards = sh_card(lo_c) + "  " + dir_label + "  " + sh_card(c3);
    } else {
        cards = sh_card(lo_c) + "  " + sh_card(c3) + "  " + sh_card(hi_c);
    }

    std::ostringstream desc;
    desc << "下柱：**" << sg.bet << "** 碼\n";
    if (delta > 0)      desc << "💰 贏得 **+" << delta << "** 碼";
    else if (bump)      desc << "💥 撞柱 **" << delta << "** 碼";
    else                desc << "💸 **" << delta << "** 碼";
    desc << "　餘額：**" << new_chips << "** 碼\n";
    desc << sh_stats_line(sg.uid);
    e.set_description(desc.str());
    sh_set_user(e, sg);

    dpp::message msg;
    msg.set_content(cards);
    msg.add_embed(e);
    sh_add_replay_row(msg, (uint64_t)sg.uid, sg.bet, new_chips);
    return msg;
}

// ─── Pass result ──────────────────────────────────────────────────────────────

static dpp::message make_shoot_pass_msg(const ShootGame& sg) {
    int r1 = sg.c1/4+1, r2 = sg.c2/4+1;
    int lo_c = (r1 <= r2) ? sg.c1 : sg.c2;
    int hi_c = (r1 <= r2) ? sg.c2 : sg.c1;

    // PASS 扣 20%，退還 80%
    int64_t cost = std::max((int64_t)1, sg.bet / 5);
    add_chips(sg.uid, -cost);
    int64_t new_chips = get_chips(sg.uid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = shoot_stats_data[sg.uid];
        st.passes++;
        st.profit -= cost;
    }
    save_shootstats();

    dpp::embed e;
    e.set_title("🙅  棄牌").set_color(0x7F8C8D);
    std::ostringstream desc;
    desc << "下柱：**" << sg.bet << "** 碼\n";
    desc << "棄牌手續費 **-" << cost << "** 碼（退還 80%）\n";
    desc << "餘額：**" << new_chips << "** 碼\n";
    desc << sh_stats_line(sg.uid);
    e.set_description(desc.str());
    sh_set_user(e, sg);

    dpp::message msg;
    msg.set_content(sh_card(lo_c) + "  " + sh_card(hi_c));
    msg.add_embed(e);
    sh_add_replay_row(msg, (uint64_t)sg.uid, sg.bet, new_chips);
    return msg;
}

// ─── Start helper (called from main.cpp) ─────────────────────────────────────

static dpp::message handle_shoot_start(dpp::snowflake uid, dpp::snowflake ch, int64_t bet,
                                        const std::string& avatar_url = "",
                                        const std::string& display_name = "") {
    int64_t bal = get_chips(uid);
    if (bal < bet) {
        dpp::embed e;
        e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        e.set_description("持有 **" + std::to_string(bal) + "** 碼，無法下柱 **" + std::to_string(bet) + "** 碼");
        return dpp::message().add_embed(e);
    }

    ShootGame sg;
    sg.uid = uid;
    sg.channel_id = ch;
    sg.bet = bet;
    sg.avatar_url = avatar_url;
    sg.display_name = display_name;
    sh_deal(sg);

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        shoot_games[uid] = sg;
    }
    return make_shoot_start_msg(sg);
}
