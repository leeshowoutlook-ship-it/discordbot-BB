#pragma once
#include "chips.h"
#include <random>
#include <sstream>
#include <algorithm>

static const std::string SCRATCH_STATS_FILE = "scratchstats.json";

// ─── Card template ────────────────────────────────────────────────────────────
// sq values: -1=炸彈, 0=空格, 10=1x, 15=1.5x, 20=2x
// RTP ≈ 98% for "always scratch exactly 3" strategy

static const std::array<int, 9> SK_TEMPLATE = {-1, -1, 0, 0, 0, 10, 10, 15, 20};

static std::mt19937& sk_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

static std::string sk_label(int v) {
    if (v == -1) return "💣";
    if (v ==  0) return "⬜";
    if (v == 10) return "💰 1x";
    if (v == 15) return "✨1.5x";
    if (v == 20) return "🌟 2x";
    return "?";
}

static std::string sk_emoji(int v) {
    if (v == -1) return "💣";
    if (v ==  0) return "⬜";
    if (v == 10) return "💰";
    if (v == 15) return "✨";
    if (v == 20) return "🌟";
    return "？";
}

// ─── Persistence ──────────────────────────────────────────────────────────────

static void load_scratchstats() {
    std::ifstream f(SCRATCH_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = scratch_stats_data[uid];
            s.wins   = v.value("wins",   0);
            s.losses = v.value("losses", 0);
            s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}

static void save_scratchstats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : scratch_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"wins",   s.wins},
                {"losses", s.losses},
                {"profit", s.profit}};
    }
    atomic_write(SCRATCH_STATS_FILE, j.dump(2));
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

static int64_t sk_payout(const ScratchGame& g) {
    int64_t sum = 0;
    for (int i = 0; i < 9; i++)
        if (((g.revealed >> i) & 1) && g.sq[i] > 0) sum += g.sq[i];
    return sum * g.bet / 10;
}

static std::string sk_stats_line(dpp::snowflake uid) {
    ScratchStats st;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = scratch_stats_data.find(uid);
        if (it != scratch_stats_data.end()) st = it->second;
    }
    int total = st.wins + st.losses;
    std::string wr = (total > 0) ? std::to_string(st.wins * 100 / total) + "%" : "—";
    std::string ps = (st.profit >= 0) ? "+" + std::to_string(st.profit) : std::to_string(st.profit);
    return "勝率 **" + wr + "**　盈虧 **" + ps + "** 碼";
}

static void sk_set_user(dpp::embed& e, const ScratchGame& g) {
    if (!g.avatar_url.empty()) e.set_thumbnail(g.avatar_url);
    dpp::embed_footer footer;
    footer.text = "👤 " + (g.display_name.empty() ? std::to_string((uint64_t)g.uid) : g.display_name);
    if (!g.avatar_url.empty()) footer.icon_url = g.avatar_url;
    e.set_footer(footer);
}

// Current state (hidden squares = 🔲)
static std::string sk_grid_text(const ScratchGame& g) {
    std::ostringstream s;
    for (int i = 0; i < 9; i++) {
        if (i > 0 && i % 3 == 0) s << "\n";
        if (i % 3 > 0) s << "  ";
        s << (((g.revealed >> i) & 1) ? sk_emoji(g.sq[i]) : "🔲");
    }
    return s.str();
}

// Full reveal (all squares shown — for result screens)
static std::string sk_full_grid(const ScratchGame& g) {
    std::ostringstream s;
    for (int i = 0; i < 9; i++) {
        if (i > 0 && i % 3 == 0) s << "\n";
        if (i % 3 > 0) s << "  ";
        s << sk_emoji(g.sq[i]);
    }
    return s.str();
}

// Result button grid: scratched squares keep their style, unscratched squares revealed
static void sk_add_result_grid(dpp::message& msg, const ScratchGame& g) {
    for (int row = 0; row < 3; row++) {
        dpp::component ar; ar.set_type(dpp::cot_action_row);
        for (int col = 0; col < 3; col++) {
            int idx = row * 3 + col;
            bool scratched = (g.revealed >> idx) & 1;
            int val = g.sq[idx];
            dpp::component btn;
            btn.set_type(dpp::cot_button).set_disabled(true);
            btn.set_label(sk_label(val));
            btn.set_id("sc9_done_" + std::to_string(idx));
            if (val == -1)
                btn.set_style(dpp::cos_danger);                          // 炸彈：紅
            else if (!scratched && val > 0)
                btn.set_style(dpp::cos_primary);                         // 未刮到的倍率：藍
            else if (scratched && val > 0)
                btn.set_style(dpp::cos_success);                         // 刮到的倍率：綠
            else
                btn.set_style(dpp::cos_secondary);                       // 空格：灰
            ar.add_component(btn);
        }
        msg.add_component(ar);
    }
}

// ─── Playing message ──────────────────────────────────────────────────────────

static const int SK_MAX_EXTRA = 4; // 3 mandatory + 4 extra = 7 total max

static dpp::message make_scratch_play_msg(const ScratchGame& g) {
    std::string sid = std::to_string((uint64_t)g.uid);
    bool choosing   = (g.safe_scratches >= 3 && !g.extra_mode);
    bool early_exit = (g.safe_scratches >= 1 && g.safe_scratches < 3 && !g.extra_mode);

    dpp::embed e;
    e.set_color(0x9B59B6);

    std::ostringstream desc;
    desc << "下注：**" << g.bet << "** 碼";
    if (g.total_paid > g.bet)
        desc << "　（含追加費 **" << (g.total_paid - g.bet) << "** 碼）";
    desc << "\n";

    int64_t pay = sk_payout(g);
    if (pay > 0) {
        std::string parts;
        for (int i = 0; i < 9; i++) {
            if (((g.revealed >> i) & 1) && g.sq[i] > 0) {
                if (!parts.empty()) parts += " + ";
                parts += sk_emoji(g.sq[i]);
            }
        }
        desc << "已刮到：" << parts << " → **" << pay << "** 碼\n";
    } else {
        desc << "已刮到：（尚無倍率）\n";
    }

    if (g.safe_scratches < 3 && !early_exit) {
        desc << "尚需刮 **" << (3 - g.safe_scratches) << "** 格才能收手";
        e.set_title("🎴  刮刮樂");
    } else if (early_exit) {
        double fee_mult = (g.safe_scratches == 1) ? 0.6 : 0.3;
        int64_t fee = std::max((int64_t)1, (int64_t)(g.bet * fee_mult));
        int64_t net_early = pay - g.total_paid - fee;
        std::string net_str = (net_early >= 0 ? "+" : "") + std::to_string(net_early);
        desc << "尚需刮 **" << (3 - g.safe_scratches) << "** 格才能免費收手\n";
        desc << "⛑️ 提前出場費 **" << fee << "** 碼（淨 " << net_str << " 碼）";
        e.set_title("🎴  刮刮樂");
    } else if (g.extra_mode) {
        desc << "✅ 已付費，請點擊任一格多刮一格";
        e.set_title("🎴  刮刮樂 — 選格子！");
    } else {
        e.set_title("🎴  刮刮樂 — 收手或繼續？");
    }

    e.set_description(desc.str());
    sk_set_user(e, g);

    dpp::message msg;
    msg.set_content(sk_grid_text(g));
    msg.add_embed(e);

    // 3×3 button grid — disabled only in choosing state (safe_scratches>=3, not extra_mode)
    for (int row = 0; row < 3; row++) {
        dpp::component ar; ar.set_type(dpp::cot_action_row);
        for (int col = 0; col < 3; col++) {
            int idx = row * 3 + col;
            bool rev = (g.revealed >> idx) & 1;
            dpp::component btn;
            btn.set_type(dpp::cot_button);
            if (rev) {
                btn.set_label(sk_label(g.sq[idx]));
                btn.set_id("sc9_done_" + std::to_string(idx));
                btn.set_style(g.sq[idx] > 0 ? dpp::cos_success : dpp::cos_secondary);
                btn.set_disabled(true);
            } else {
                btn.set_label("🔲");
                btn.set_id("sc9_rev_" + sid + "_" + std::to_string(idx));
                btn.set_style(dpp::cos_primary);
                btn.set_disabled(choosing);
            }
            ar.add_component(btn);
        }
        msg.add_component(ar);
    }

    // Row 4: early exit (1 or 2 scratches done)
    if (early_exit) {
        double fee_mult = (g.safe_scratches == 1) ? 0.6 : 0.3;
        int64_t fee = std::max((int64_t)1, (int64_t)(g.bet * fee_mult));
        int64_t net_early = pay - g.total_paid - fee;
        std::string net_str = (net_early >= 0 ? "+" : "") + std::to_string(net_early);
        int64_t chips = get_chips(g.uid);

        dpp::component ar4; ar4.set_type(dpp::cot_action_row);
        ar4.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("⛑️ 提前出場（費 " + std::to_string(fee) + " 碼，淨 " + net_str + "）")
            .set_id("sc9_early_" + sid).set_style(dpp::cos_secondary)
            .set_disabled(chips < fee));
        msg.add_component(ar4);
    }

    // Row 4: collect or extra scratch (3+ scratches done)
    if (choosing) {
        int64_t net = pay - g.total_paid;
        std::string net_str = (net >= 0 ? "+" : "") + std::to_string(net);
        int64_t chips = get_chips(g.uid);
        int64_t extra_cost = std::max((int64_t)1, g.bet / 2);

        dpp::component ar4; ar4.set_type(dpp::cot_action_row);
        ar4.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("💰 收手（淨 " + net_str + " 碼）")
            .set_id("sc9_cash_" + sid).set_style(dpp::cos_success));
        if (g.extra_count < SK_MAX_EXTRA) {
            ar4.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🔓 多刮一格（花費 " + std::to_string(extra_cost) + " 碼）")
                .set_id("sc9_extra_" + sid).set_style(dpp::cos_danger)
                .set_disabled(chips < extra_cost));
        }
        msg.add_component(ar4);
    }

    return msg;
}

// ─── Bomb result ──────────────────────────────────────────────────────────────

static dpp::message make_scratch_bomb_msg(ScratchGame g, int bomb_idx) {
    // Reveal the bomb square
    g.revealed |= (1 << bomb_idx);

    int64_t lost = g.total_paid;
    add_chips(g.uid, -lost);
    int64_t new_chips = get_chips(g.uid);
    if (new_chips <= 0) announce_bankrupt(g.uid, g.channel_id);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = scratch_stats_data[g.uid];
        st.losses++;
        st.profit -= lost;
    }
    save_scratchstats();

    dpp::embed e;
    e.set_title("💣  爆炸了！").set_color(0xFF0000);

    std::ostringstream desc;
    desc << "下注：**" << g.bet << "** 碼\n";
    desc << "💸 損失 **-" << lost << "** 碼\n";
    desc << "餘額：**" << new_chips << "** 碼\n";
    desc << sk_stats_line(g.uid);

    e.set_description(desc.str());
    sk_set_user(e, g);

    dpp::message msg;
    msg.set_content(sk_full_grid(g));
    msg.add_embed(e);

    sk_add_result_grid(msg, g);

    // Replay row
    std::string sid = std::to_string((uint64_t)g.uid);
    int64_t dbl = g.bet * 2;
    dpp::component ar4; ar4.set_type(dpp::cot_action_row);
    ar4.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("再來一張").set_id("sc9_again_" + sid + "_" + std::to_string(g.bet))
        .set_style(dpp::cos_primary));
    ar4.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("雙倍（" + std::to_string(dbl) + "）")
        .set_id("sc9_again_" + sid + "_" + std::to_string(dbl))
        .set_style(dpp::cos_success).set_disabled(new_chips < dbl));
    msg.add_component(ar4);

    return msg;
}

// ─── Cash-out result ──────────────────────────────────────────────────────────

static dpp::message make_scratch_cash_msg(const ScratchGame& g) {
    int64_t pay = sk_payout(g);
    int64_t net = pay - g.total_paid;

    add_chips(g.uid, net);
    int64_t new_chips = get_chips(g.uid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = scratch_stats_data[g.uid];
        if (pay >= g.total_paid) st.wins++; else st.losses++;
        st.profit += net;
    }
    save_scratchstats();

    dpp::embed e;
    std::ostringstream desc;
    desc << "下注：**" << g.bet << "** 碼";
    if (g.total_paid > g.bet)
        desc << "　（含追加費 **" << (g.total_paid - g.bet) << "** 碼）";
    desc << "\n";

    if (pay > 0) {
        // List what was found
        std::string parts;
        for (int i = 0; i < 9; i++) {
            if (((g.revealed >> i) & 1) && g.sq[i] > 0) {
                if (!parts.empty()) parts += " + ";
                parts += sk_emoji(g.sq[i]);
            }
        }
        desc << "刮到：" << parts << " → **" << pay << "** 碼\n\n";
    }

    if (net >= 0) {
        e.set_title("💰  成功收手！").set_color(0x2ECC71);
        desc << "🎉 淨賺 **+" << net << "** 碼\n";
    } else {
        e.set_title("😔  收手（沒中）").set_color(0x95A5A6);
        desc << "💸 損失 **" << net << "** 碼\n";
    }
    desc << "餘額：**" << new_chips << "** 碼\n";
    desc << sk_stats_line(g.uid);

    e.set_description(desc.str());
    sk_set_user(e, g);

    dpp::message msg;
    msg.set_content(sk_full_grid(g));
    msg.add_embed(e);

    sk_add_result_grid(msg, g);

    // Replay row
    std::string sid = std::to_string((uint64_t)g.uid);
    int64_t dbl = g.bet * 2;
    dpp::component ar4; ar4.set_type(dpp::cot_action_row);
    ar4.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("再來一張").set_id("sc9_again_" + sid + "_" + std::to_string(g.bet))
        .set_style(dpp::cos_primary));
    ar4.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("雙倍（" + std::to_string(dbl) + "）")
        .set_id("sc9_again_" + sid + "_" + std::to_string(dbl))
        .set_style(dpp::cos_success).set_disabled(new_chips < dbl));
    msg.add_component(ar4);

    return msg;
}

// ─── Start ────────────────────────────────────────────────────────────────────

static dpp::message handle_scratch_start(dpp::snowflake uid, dpp::snowflake ch, int64_t bet,
                                          const std::string& avatar_url = "",
                                          const std::string& display_name = "") {
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (scratch_games.count(uid))
            return dpp::message().add_embed(
                dpp::embed().set_title("⚠️  你有未完成的刮刮樂！").set_color(0xE67E22)
                    .set_description("請先刮完或點「💰 收手」結算後再買新的。"));
    }

    int64_t bal = get_chips(uid);
    if (bal < bet)
        return dpp::message().add_embed(
            dpp::embed().set_title("❌  籌碼不足").set_color(0xE74C3C)
                .set_description("持有 **" + std::to_string(bal) + "** 碼，無法下注 **" + std::to_string(bet) + "** 碼"));

    // Generate shuffled card
    ScratchGame g;
    g.uid          = uid;
    g.channel_id   = ch;
    g.bet          = bet;
    g.total_paid   = bet;
    g.revealed     = 0;
    g.safe_scratches = 0;
    g.extra_mode   = false;
    g.avatar_url   = avatar_url;
    g.display_name = display_name;
    g.sq = SK_TEMPLATE;
    std::shuffle(g.sq.begin(), g.sq.end(), sk_rng());

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        scratch_games[uid] = g;
    }
    return make_scratch_play_msg(g);
}
