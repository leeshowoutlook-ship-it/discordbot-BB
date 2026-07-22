#pragma once
#include "bjstats.h"
#include <random>
#include <algorithm>

// ─── Card helpers ─────────────────────────────────────────────────────────────
// suit: 0=♠黑桃  1=♥愛心  2=♦方片  3=♣梅花
// rank: 1=A, 2-10, 11=J, 12=Q, 13=K

// suit: 0=♠(B)  1=♥(H)  2=♦(C)  3=♣(M)
// rank: 1=A … 10 … 11=J 12=Q 13=K
static std::string card_str(const BJCard& c) {
    static const char* PREFIX[4] = { "B", "H", "C", "M" };
    static const char* RANK[14]  = { "","A","2","3","4","5","6","7","8","9","10","J","Q","K" };
    std::string ename = std::string(PREFIX[c.suit]) + RANK[c.rank];
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = emoji_name_map.find(ename);
        if (it != emoji_name_map.end()) return it->second;
    }
    static const char* S[4] = { "♠","♥","♦","♣" };
    return std::string(S[c.suit]) + RANK[c.rank];
}

static bool is_soft(const std::vector<BJCard>& hand) {
    int base = 0, aces = 0;
    for (auto& c : hand) {
        if (c.rank == 1) { aces++; base += 1; }
        else              base += std::min(c.rank, 10);
    }
    return aces > 0 && (base + 10) <= 21;
}

static int hand_value(const std::vector<BJCard>& hand) {
    int total = 0, aces = 0;
    for (auto& c : hand) {
        if (c.rank == 1) { aces++; total += 11; }
        else              total += std::min(c.rank, 10);
    }
    while (total > 21 && aces > 0) { total -= 10; aces--; }
    return total;
}

static bool is_blackjack(const std::vector<BJCard>& hand) {
    return hand.size() == 2 && hand_value(hand) == 21;
}

static std::string hand_str(const std::vector<BJCard>& hand, bool hide_second = false) {
    std::ostringstream oss;
    for (size_t i = 0; i < hand.size(); i++) {
        if (i > 0) oss << " ";
        oss << (i == 1 && hide_second ? "🃏" : card_str(hand[i]));
    }
    return oss.str();
}

// ─── Deck ─────────────────────────────────────────────────────────────────────

static std::vector<BJCard> make_deck() {
    std::vector<BJCard> deck; deck.reserve(52);
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            deck.push_back({r, s});
    std::mt19937 rng(std::random_device{}());
    std::shuffle(deck.begin(), deck.end(), rng);
    return deck;
}

static BJCard draw_card(BJGame& g) {
    if (g.deck.empty()) g.deck = make_deck();
    BJCard c = g.deck.back(); g.deck.pop_back();
    return c;
}

// ─── Game embed (called WITHOUT data_mutex) ───────────────────────────────────

static dpp::message make_bj_msg(const BJGame& g, const std::string& status = "") {
    dpp::embed e;
    e.set_title("🃏  21點").set_color(0x1A8754);

    bool hide = !g.game_over;

    // ── Content: only card emoji so Discord renders them jumbo ────────────
    std::ostringstream cards;

    // Dealer row
    for (size_t i = 0; i < g.dealer_cards.size(); i++) {
        if (i > 0) cards << "  ";
        cards << (i == 1 && hide ? "🃏" : card_str(g.dealer_cards[i]));
    }
    cards << "\n\n";

    // Main hand row
    for (size_t i = 0; i < g.main_hand.cards.size(); i++) {
        if (i > 0) cards << "  ";
        cards << card_str(g.main_hand.cards[i]);
    }

    // Split hand row
    if (g.has_split) {
        cards << "\n\n";
        for (size_t i = 0; i < g.split_hand.cards.size(); i++) {
            if (i > 0) cards << "  ";
            cards << card_str(g.split_hand.cards[i]);
        }
    }

    // ── Embed: game info ──────────────────────────────────────────────────
    std::ostringstream desc;

    // Dealer value
    int dv = hand_value(g.dealer_cards);
    desc << "🏦  莊家：" << (hide ? "**?**" : "**" + std::to_string(dv) + "**");
    if (!hide && is_blackjack(g.dealer_cards)) desc << "  🌟 BJ";
    desc << "\n\n";

    // Main hand
    {
        const BJHand& h = g.main_hand;
        int v = hand_value(h.cards);
        std::string label = g.has_split
            ? (!g.split_active ? "🂡  手牌 1 ▶" : "🂡  手牌 1")
            : "🂡  你的手牌";
        desc << label << "：**" << v << "**";
        if (h.doubled)                             desc << "  ⚡ 加倍";
        if (is_blackjack(h.cards)) desc << "  🌟 BJ！";
        if (v > 21)                                desc << "  💥 爆牌";
        desc << "\n";
    }

    // Split hand
    if (g.has_split) {
        const BJHand& h = g.split_hand;
        int v = hand_value(h.cards);
        std::string label = g.split_active ? "🂡  手牌 2 ▶" : "🂡  手牌 2";
        desc << label << "：**" << v << "**";
        if (h.doubled)          desc << "  ⚡ 加倍";
        if (is_blackjack(h.cards)) desc << "  🌟 BJ！";
        if (v > 21)             desc << "  💥 爆牌";
        desc << "\n";
    }

    desc << "\n💰 賭注 **" << g.bet << "** 碼　　💼 持有 **" << get_chips(g.user_id) << "** 碼";
    if (!status.empty()) desc << "\n\n" << status;
    if (g.game_over) {
        int bj_w = 0, bj_l = 0, bj_p = 0; int64_t bj_profit = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = bj_stats_data.find(g.user_id);
            if (it != bj_stats_data.end()) {
                bj_w = it->second.wins; bj_l = it->second.losses;
                bj_p = it->second.pushes; bj_profit = it->second.profit;
            }
        }
        int bj_total = bj_w + bj_l + bj_p;
        if (bj_total > 0) {
            char r[16]; snprintf(r, sizeof(r), "%.1f%%", bj_w * 100.0 / bj_total);
            desc << "\n📊 勝/負/平 " << bj_w << "/" << bj_l << "/" << bj_p
                 << "　勝率 " << r
                 << "　盈虧 " << (bj_profit >= 0 ? "+" : "") << bj_profit << " 碼";
        }
    }
    desc << "\n\n👤 <@" << (uint64_t)g.user_id << ">";

    e.set_description(desc.str());
    if (!g.avatar_url.empty())
        e.set_thumbnail(g.avatar_url);

    dpp::message msg;
    msg.set_content(cards.str());
    msg.add_embed(e);

    if (!g.game_over) {
        const BJHand& cur = g.split_active ? g.split_hand : g.main_hand;
        bool first = cur.cards.size() == 2;
        bool can_split  = first && !g.has_split && cur.cards[0].rank == cur.cards[1].rank;
        bool can_double = first && !cur.doubled;

        dpp::component row; row.set_type(dpp::cot_action_row);
        auto btn = [&](const std::string& lbl, const std::string& id,
                        dpp::component_style sty, bool dis = false) {
            dpp::component b;
            b.set_type(dpp::cot_button).set_label(lbl).set_id(id)
             .set_style(sty).set_disabled(dis);
            row.add_component(b);
        };
        std::string gid = std::to_string(g.id);
        btn("🃏 要牌", "bj_hit_"    + gid, dpp::cos_primary);
        btn("✋ 停牌", "bj_stand_"  + gid, dpp::cos_secondary);
        btn("⚡ 加倍", "bj_double_" + gid, dpp::cos_success,  !can_double);
        btn("✂️ 分牌", "bj_split_"  + gid, dpp::cos_danger,   !can_split);
        msg.add_component(row);
    } else {
        // Post-game restart buttons
        dpp::component row; row.set_type(dpp::cot_action_row);
        std::string uid_s = std::to_string((uint64_t)g.user_id);
        std::string bet_s = std::to_string(g.bet);
        dpp::component again, dbl;
        again.set_type(dpp::cot_button)
              .set_label("🔄 再來一局（" + bet_s + " 碼）")
              .set_id("bj_again_" + uid_s + "_" + bet_s)
              .set_style(dpp::cos_success);
        dbl.set_type(dpp::cot_button)
            .set_label("💰 雙倍（" + std::to_string(g.bet * 2) + " 碼）")
            .set_id("bj_again_" + uid_s + "_" + std::to_string(g.bet * 2))
            .set_style(dpp::cos_danger);
        row.add_component(again); row.add_component(dbl);
        msg.add_component(row);
    }
    return msg;
}

// ─── Dealer play ──────────────────────────────────────────────────────────────

static void dealer_play(BJGame& g) {
    while (hand_value(g.dealer_cards) < 17)
        g.dealer_cards.push_back(draw_card(g));
}

// ─── Resolve one hand ─────────────────────────────────────────────────────────
// Returns {text, payout, bet_paid}.
// payout = chips to ADD back (0 = lost, ab = push, ab*2 = win).
// Bet was already deducted upfront; payout is what the player receives back.

struct HandResult { std::string text; int64_t payout; int64_t bet_paid; };

static HandResult resolve_hand(
    const BJHand& h, const std::vector<BJCard>& dealer, int64_t bet, bool is_split)
{
    int pv = hand_value(h.cards), dv = hand_value(dealer);
    bool pbj = is_blackjack(h.cards);
    bool dbj = is_blackjack(dealer);
    int64_t ab = h.doubled ? bet * 2 : bet;
    bool fcc = ((int)h.cards.size() >= 5 && pv <= 21); // 過五關

    if (pv > 21)     return { "💥 爆牌 — 莊家贏",                              0,     ab };
    if (fcc)         return { "🎖️ 過五關！+**" + std::to_string(ab) + "** 碼", ab*2,  ab };
    if (dbj && !pbj) return { "莊家 BJ — 莊家贏",                              0,     ab };
    if (pbj && dbj)  return { "雙 BJ — 平局",                                  ab,    ab };
    if (pbj) {
        int64_t w = (int64_t)(ab * 1.5);
        return { "🌟 Blackjack！+**" + std::to_string(w) + "** 碼", ab + w, ab };
    }
    if (dv > 21)    return { "莊家爆牌 +**" + std::to_string(ab) + "** 碼",   ab*2,  ab };
    if (pv > dv)    return { "你贏 +**" + std::to_string(ab) + "** 碼",        ab*2,  ab };
    if (pv < dv)    return { "莊家贏 -**" + std::to_string(ab) + "** 碼",      0,     ab };
    return { "平局",                                                             ab,    ab };
}

// ─── Resolve full game (called UNDER data_mutex — uses chip_data directly) ────

static std::string resolve_bj(BJGame& g, int64_t& out_net) {
    // #9: only play dealer if at least one player hand didn't bust
    bool all_bust = (hand_value(g.main_hand.cards) > 21);
    if (g.has_split) all_bust = all_bust && (hand_value(g.split_hand.cards) > 21);
    if (!all_bust) dealer_play(g);
    g.game_over = true;

    std::ostringstream oss;
    auto r1 = resolve_hand(g.main_hand, g.dealer_cards, g.bet, false);
    int64_t total_payout = r1.payout;
    int64_t total_bet    = r1.bet_paid;

    if (g.has_split) {
        auto r2 = resolve_hand(g.split_hand, g.dealer_cards, g.bet, true);
        oss << "手牌 1：" << r1.text << "\n手牌 2：" << r2.text;
        total_payout += r2.payout;
        total_bet    += r2.bet_paid;
    } else {
        oss << r1.text;
    }

    chip_data[g.user_id].chips += total_payout;
    int64_t net = total_payout - total_bet;

    // Track BJ stats
    auto& stats = bj_stats_data[g.user_id];
    if (net > 0) stats.wins++;
    else if (net < 0) stats.losses++;
    else stats.pushes++;
    stats.profit += net;

    out_net = net;
    if (net > 0)      oss << "\n\n✅ 總計 +**" << net << "** 碼";
    else if (net < 0) oss << "\n\n❌ 總計 **"  << net << "** 碼";
    else              oss << "\n\n⚖️ 平局，退回籌碼";
    return oss.str();
}

// ─── Start game ───────────────────────────────────────────────────────────────

static BJGame start_bj(dpp::snowflake uid, dpp::snowflake ch, int64_t bet,
                       const std::string& avatar_url = "",
                       const std::string& display_name = "") {
    BJGame g;
    { std::lock_guard<std::mutex> lk(data_mutex); g.id = bj_counter++; }
    g.user_id = uid; g.channel_id = ch; g.bet = bet;
    g.avatar_url = avatar_url; g.display_name = display_name;
    g.deck = make_deck();
    g.main_hand.cards.push_back(draw_card(g));
    g.dealer_cards.push_back(draw_card(g));
    g.main_hand.cards.push_back(draw_card(g));
    g.dealer_cards.push_back(draw_card(g));
    return g;
}

// ─── Button handler ───────────────────────────────────────────────────────────
// Holds data_mutex once and modifies chip_data directly — no nested locking.
// Calls save_chips() AFTER releasing the lock.

static dpp::message handle_bj_button(const std::string& action, uint64_t gid,
                                      dpp::snowflake uid)
{
    BJGame snap;
    std::string status;
    bool do_save = false;
    int64_t bj_net = 0;

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = bj_games.find(gid);
        if (it == bj_games.end() || it->second.game_over) return {};
        BJGame& g = it->second;
        if (g.user_id != uid) return {};

        BJHand& cur = g.split_active ? g.split_hand : g.main_hand;

        auto advance = [&]() {
            // Move to split hand, or resolve
            if (!g.split_active && g.has_split && !g.split_hand.done) {
                g.split_active = true;
                if (g.split_hand.cards.size() == 1) // deal 2nd card to split
                    g.split_hand.cards.push_back(draw_card(g));
            } else {
                status  = resolve_bj(g, bj_net); // modifies chip_data under lock
                do_save = true;
            }
        };

        if (action == "hit") {
            cur.cards.push_back(draw_card(g));
            int val = hand_value(cur.cards);
            bool fcc      = ((int)cur.cards.size() >= 5 && val <= 21);
            bool hard_stop = (val > 21) || (val == 21 && !is_soft(cur.cards));
            if (hard_stop || fcc) { cur.done = true; advance(); }
        }
        else if (action == "stand") {
            cur.done = true;
            advance();
        }
        else if (action == "double") {
            if (cur.cards.size() == 2 && !cur.doubled) {
                if (chip_data[uid].chips < g.bet) {
                    status = "⚠️ 籌碼不足，無法加倍！";
                } else {
                    chip_data[uid].chips -= g.bet; // deduct extra bet under lock
                    cur.doubled = true;
                    cur.cards.push_back(draw_card(g));
                    cur.done = true;
                    do_save = true;
                    advance();
                }
            }
        }
        else if (action == "split") {
            if (cur.cards.size() == 2 && !g.has_split
                && cur.cards[0].rank == cur.cards[1].rank)
            {
                if (chip_data[uid].chips < g.bet) {
                    status = "⚠️ 籌碼不足，無法分牌！";
                } else {
                    chip_data[uid].chips -= g.bet; // deduct extra bet under lock
                    g.has_split = true;
                    g.split_hand.cards.push_back(cur.cards[1]); // move card
                    cur.cards.pop_back();
                    cur.cards.push_back(draw_card(g)); // deal new card to main
                    // split hand's 2nd card dealt when we advance to it
                    do_save = true;
                }
            }
        }

        snap = g; // snapshot for message building (outside lock)
    } // mutex released here

    if (do_save) { save_chips(); save_bjstats(); }
    save_bj_games(); // always persist current state (finished games excluded inside)

    if (do_save && snap.game_over && get_chips(uid) <= 0)
        announce_bankrupt(uid, snap.channel_id);

    dpp::message bj_msg = make_bj_msg(snap, status); // get_chips locks briefly — no conflict
    if (bj_net < 0) {
        int gc = 0, hr = 0;
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (inventory_data.count(uid)) {
              auto& inv = inventory_data[uid];
              gc = inv.count("game_cancel") ? inv["game_cancel"] : 0;
              hr = inv.count("half_refund") ? inv["half_refund"] : 0;
          }
        }
        if (gc > 0 || hr > 0) {
            std::string bj_uid_s = std::to_string((uint64_t)uid);
            std::string bj_loss_s = std::to_string(-bj_net);
            dpp::component gc_row; gc_row.set_type(dpp::cot_action_row);
            if (gc > 0) gc_row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("這局不算!!")
                .set_id("game_cancel_" + bj_uid_s + "_bj_" + bj_loss_s)
                .set_style(dpp::cos_success));
            if (hr > 0) gc_row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("對不起我錯了！！")
                .set_id("half_refund_" + bj_uid_s + "_bj_" + bj_loss_s)
                .set_style(dpp::cos_primary));
            bj_msg.add_component(gc_row);
        }
    }
    return bj_msg;
}

// Disable all buttons on an old game message so stale clicks don't confuse players
static void bj_disable_old_msg(dpp::cluster& bot, const BJGame& g) {
    if ((uint64_t)g.msg_id == 0) return;
    dpp::message dm = make_bj_msg(g, "");
    for (auto& row : dm.components)
        for (auto& btn : row.components)
            btn.disabled = true;
    dm.id = g.msg_id;
    dm.channel_id = g.channel_id;
    bot.message_edit(dm);
}
