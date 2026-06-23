#pragma once
#include "helpers.h"
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string CHIPS_FILE    = "chips.json";
static const int64_t     CLAIM_AMOUNT  = 500;
static const int64_t     WEEKLY_AMOUNT = 2000;

// ─── Persistence ──────────────────────────────────────────────────────────────

static void load_chips() {
    std::ifstream f(CHIPS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            chip_data[uid] = {
                v["chips"].get<int64_t>(),
                (time_t)v.value("last_claim",  (int64_t)0),
                (time_t)v.value("last_weekly", (int64_t)0)
            };
        }
    } catch (...) {}
}

static void save_chips() {
    nlohmann::json j;
    // Caller must hold data_mutex or be the only writer
    for (auto& [uid, cd] : chip_data)
        j[std::to_string((uint64_t)uid)] = {
            {"chips", cd.chips},
            {"last_claim",  (int64_t)cd.last_claim},
            {"last_weekly", (int64_t)cd.last_weekly}
        };
    atomic_write(CHIPS_FILE, j.dump(2));
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

static int64_t get_chips(dpp::snowflake uid) {
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = chip_data.find(uid);
    return it == chip_data.end() ? 0 : it->second.chips;
}

static void add_chips(dpp::snowflake uid, int64_t delta) {
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        chip_data[uid].chips += delta;
    }
    save_chips();
}

// ─── Claim embed ──────────────────────────────────────────────────────────────

static dpp::message make_claim_msg(dpp::snowflake uid, bool success,
                                   int64_t balance, int secs_left = 0) {
    dpp::embed e;
    if (success) {
        e.set_title("🪙  領取成功！").set_color(0xF1C40F);
        e.add_field("💰  獲得",     std::to_string(CLAIM_AMOUNT) + " 碼", true);
        e.add_field("💼  目前持有", std::to_string(balance) + " 碼",      true);
        e.set_footer(dpp::embed_footer().set_text("每小時可領取一次"));
    } else {
        int h = secs_left / 3600, m = (secs_left % 3600) / 60, s = secs_left % 60;
        char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
        e.set_title("⏳  還沒到領取時間").set_color(0x808080);
        e.add_field("⏰  距下次領取", std::string(buf),            true);
        e.add_field("💼  目前持有",   std::to_string(balance) + " 碼", true);
    }
    dpp::message msg; msg.add_embed(e);
    return msg;
}

// ─── Claim handler — 每整點可領一次 ──────────────────────────────────────────

static dpp::message handle_claim(dpp::snowflake uid) {
    time_t now = time(nullptr);
    int64_t now_hour  = now / 3600;
    int64_t balance; bool success; int secs_left = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& cd = chip_data[uid];
        int64_t last_hour = cd.last_claim / 3600;
        if (now_hour > last_hour) {
            cd.chips += CLAIM_AMOUNT;
            cd.last_claim = now;
            success = true;
            balance = cd.chips;
        } else {
            secs_left = (int)(((now / 3600) + 1) * 3600 - now);
            success = false;
            balance = cd.chips;
        }
    }
    save_chips();
    return make_claim_msg(uid, success, balance, secs_left);
}

// ─── Weekly claim — 每週四中午12:00(UTC+8) = 週四04:00 UTC ────────────────────
// 1970-01-01 was a Thursday, so week_id = (t - 4*3600) / 604800

static int64_t weekly_id(time_t t) {
    return ((int64_t)t - 4 * 3600) / (7 * 86400);
}

static dpp::message handle_weekly_claim(dpp::snowflake uid) {
    time_t now = time(nullptr);
    int64_t cur_week = weekly_id(now);
    int64_t balance; bool success;
    time_t next_thu = (time_t)((cur_week + 1) * (int64_t)(7 * 86400) + 4 * 3600);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& cd = chip_data[uid];
        if (cur_week > weekly_id(cd.last_weekly)) {
            cd.chips += WEEKLY_AMOUNT;
            cd.last_weekly = now;
            success = true;
            balance = cd.chips;
        } else {
            success = false;
            balance = cd.chips;
        }
    }
    save_chips();

    dpp::embed e;
    if (success) {
        e.set_title("🎁  每週領取成功！").set_color(0xF1C40F);
        e.add_field("💰  獲得",     std::to_string(WEEKLY_AMOUNT) + " 碼", true);
        e.add_field("💼  目前持有", std::to_string(balance)        + " 碼", true);
        e.set_footer(dpp::embed_footer().set_text("每週四中午12:00（UTC+8）更新"));
    } else {
        e.set_title("⏳  本週已領取").set_color(0x808080);
        e.add_field("⏰  下次領取", "<t:" + std::to_string((int64_t)next_thu) + ":F>", true);
        e.add_field("💼  目前持有", std::to_string(balance) + " 碼",                  true);
    }
    dpp::message msg; msg.add_embed(e); return msg;
}

// ─── Leaderboard with pagination ──────────────────────────────────────────────

static dpp::message handle_leaderboard(int page = 0) {
    std::vector<std::pair<dpp::snowflake, int64_t>> sorted;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, cd] : chip_data)
            if (cd.chips > 0 && (cfg.notify_user_id.empty() || std::to_string((uint64_t)uid) != cfg.notify_user_id))
                sorted.push_back({uid, cd.chips});
    }
    std::sort(sorted.begin(), sorted.end(),
              [](auto& a, auto& b) { return a.second > b.second; });

    const int PAGE_SIZE = 10;
    int total = (int)sorted.size();
    int total_pages = std::max(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    page = std::max(0, std::min(page, total_pages - 1));

    dpp::embed e;
    e.set_title("💰  富豪榜").set_color(0xF1C40F);
    if (sorted.empty()) {
        e.set_description("目前沒有人有籌碼");
        dpp::message msg; msg.add_embed(e); return msg;
    }
    static const char* MEDALS[] = {"🥇", "🥈", "🥉"};
    std::ostringstream oss;
    int start = page * PAGE_SIZE;
    int end   = std::min(start + PAGE_SIZE, total);
    for (int i = start; i < end; i++) {
        std::string rank = (i < 3) ? MEDALS[i] : (std::to_string(i + 1) + ".");
        oss << rank << " <@" << (uint64_t)sorted[i].first
            << ">  **" << sorted[i].second << "** 碼\n";
    }
    e.set_description(oss.str());
    e.set_footer(dpp::embed_footer().set_text(
        "第 " + std::to_string(page+1) + "/" + std::to_string(total_pages) + " 頁  共 " + std::to_string(total) + " 人"));

    dpp::message msg; msg.add_embed(e);
    if (total_pages > 1) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component prev, next;
        prev.set_type(dpp::cot_button).set_label("◀").set_id("lb_" + std::to_string(page-1))
            .set_style(dpp::cos_secondary).set_disabled(page == 0);
        next.set_type(dpp::cot_button).set_label("▶").set_id("lb_" + std::to_string(page+1))
            .set_style(dpp::cos_secondary).set_disabled(page == total_pages-1);
        row.add_component(prev); row.add_component(next);
        msg.add_component(row);
    }
    return msg;
}

// ─── Wallet (formerly status) ─────────────────────────────────────────────────

static dpp::message handle_wallet(dpp::snowflake uid) {
    int64_t chips = get_chips(uid);
    int wins = 0, losses = 0, pushes = 0; int64_t profit = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = bj_stats_data.find(uid);
        if (it != bj_stats_data.end()) {
            wins = it->second.wins; losses = it->second.losses;
            pushes = it->second.pushes; profit = it->second.profit;
        }
    }
    int total_games = wins + losses + pushes;
    double win_rate = total_games > 0 ? (wins * 100.0 / total_games) : 0.0;

    int d_wins = 0, d_losses = 0; int64_t d_profit = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = dice_stats_data.find(uid);
        if (it != dice_stats_data.end()) {
            d_wins = it->second.wins; d_losses = it->second.losses;
            d_profit = it->second.profit;
        }
    }
    int d_total = d_wins + d_losses;
    double d_win_rate = d_total > 0 ? (d_wins * 100.0 / d_total) : 0.0;

    dpp::embed e;
    e.set_title("💼  我的錢包").set_color(0x5865F2);
    e.set_description("<@" + std::to_string((uint64_t)uid) + ">");
    e.add_field("💰  目前持有", std::to_string(chips) + " 碼", false);
    if (total_games > 0) {
        char rate_buf[16]; snprintf(rate_buf, sizeof(rate_buf), "%.1f%%", win_rate);
        e.add_field("🃏  21點  勝/負/平",
            std::to_string(wins) + " / " + std::to_string(losses) + " / " + std::to_string(pushes), true);
        e.add_field("📈  勝率", rate_buf, true);
        e.add_field("💹  21點盈虧",
            (profit >= 0 ? "+" : "") + std::to_string(profit) + " 碼", false);
    }
    if (d_total > 0) {
        char dr_buf[16]; snprintf(dr_buf, sizeof(dr_buf), "%.1f%%", d_win_rate);
        e.add_field("🎲  骰子  勝/負",
            std::to_string(d_wins) + " / " + std::to_string(d_losses), true);
        e.add_field("📈  勝率", dr_buf, true);
        e.add_field("💹  骰子盈虧",
            (d_profit >= 0 ? "+" : "") + std::to_string(d_profit) + " 碼", false);
    }
    dpp::message msg; msg.add_embed(e); return msg;
}

// ─── Transfer (execute immediately) ───────────────────────────────────────────

static dpp::message handle_transfer(dpp::snowflake from_uid,
                                    dpp::snowflake to_uid,
                                    int64_t amount,
                                    const std::string& to_name,
                                    const std::string& from_name = "") {
    dpp::embed e;
    if (from_uid == to_uid) {
        e.set_title("❌  無法轉帳給自己").set_color(0xE74C3C);
        dpp::message msg; msg.add_embed(e); return msg;
    }
    if (amount <= 0) {
        e.set_title("❌  金額必須大於 0").set_color(0xE74C3C);
        dpp::message msg; msg.add_embed(e); return msg;
    }
    int64_t from_bal = 0, to_bal = 0;
    bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        int64_t have = chip_data[from_uid].chips;
        if (have >= amount) {
            chip_data[from_uid].chips -= amount;
            chip_data[to_uid].chips   += amount;
            from_bal = chip_data[from_uid].chips;
            to_bal   = chip_data[to_uid].chips;
            ok = true;
        }
    }
    if (!ok) {
        e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        e.set_description("你的籌碼不足 **" + std::to_string(amount) + "** 碼");
        dpp::message msg; msg.add_embed(e); return msg;
    }
    save_chips();
    e.set_title("💸  轉帳成功").set_color(0x2ECC71);
    if (!from_name.empty())
        e.add_field("👤  轉帳者",    from_name,                         true);
    e.add_field("👤  收款人",    to_name,                           true);
    e.add_field("💰  金額",      std::to_string(amount)   + " 碼", false);
    e.add_field("💼  轉帳者餘額", std::to_string(from_bal) + " 碼", true);
    e.add_field("💼  收款人餘額", std::to_string(to_bal)   + " 碼", true);
    dpp::message msg; msg.add_embed(e); return msg;
}

// ─── Transfer with confirmation ───────────────────────────────────────────────

static dpp::message handle_transfer_request(
    dpp::snowflake from_uid, const std::string& from_name,
    dpp::snowflake to_uid,   const std::string& to_name, int64_t amount) {

    dpp::embed e;
    if (from_uid == to_uid) {
        e.set_title("❌  無法轉帳給自己").set_color(0xE74C3C);
        dpp::message msg; msg.add_embed(e); return msg;
    }
    if (amount <= 0) {
        e.set_title("❌  金額必須大於 0").set_color(0xE74C3C);
        dpp::message msg; msg.add_embed(e); return msg;
    }
    int64_t bal = get_chips(from_uid);
    if (bal < amount) {
        e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        e.set_description("你持有 **" + std::to_string(bal) + "** 碼，不足 **" + std::to_string(amount) + "** 碼");
        dpp::message msg; msg.add_embed(e); return msg;
    }
    uint64_t tid;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        tid = transfer_counter++;
        PendingTransfer pt{from_uid, to_uid, amount, from_name, to_name, time(nullptr)};
        pending_transfers[tid] = pt;
    }
    e.set_title("💸  轉帳確認").set_color(0xF39C12);
    e.add_field("👤  轉帳者",   from_name,                        true);
    e.add_field("👤  收款人",   to_name,                          true);
    e.add_field("💰  金額",     std::to_string(amount) + " 碼",   false);
    e.add_field("💼  你的餘額", std::to_string(bal)    + " 碼",   false);
    e.set_footer(dpp::embed_footer().set_text("請在 60 秒內確認"));

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component ok_btn, cancel_btn;
    ok_btn.set_type(dpp::cot_button).set_label("✅ 確認轉帳")
          .set_id("xfer_ok_" + std::to_string(tid)).set_style(dpp::cos_success);
    cancel_btn.set_type(dpp::cot_button).set_label("❌ 取消")
              .set_id("xfer_cancel_" + std::to_string(tid)).set_style(dpp::cos_danger);
    row.add_component(ok_btn); row.add_component(cancel_btn);
    msg.add_component(row);
    return msg;
}

static dpp::message handle_transfer_confirm(uint64_t tid, dpp::snowflake uid) {
    PendingTransfer pt;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pending_transfers.find(tid);
        if (it == pending_transfers.end()) {
            dpp::embed e; e.set_title("❌  轉帳已過期或不存在").set_color(0xE74C3C);
            dpp::message msg; msg.add_embed(e); return msg;
        }
        if (it->second.from_uid != uid) {
            dpp::embed e; e.set_title("❌  只有發起人才能確認").set_color(0xE74C3C);
            dpp::message msg; msg.add_embed(e); return msg;
        }
        pt = it->second;
        pending_transfers.erase(it);
    }
    return handle_transfer(pt.from_uid, pt.to_uid, pt.amount, pt.to_name, pt.from_name);
}

static dpp::message handle_transfer_cancel(uint64_t tid, dpp::snowflake uid) {
    bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pending_transfers.find(tid);
        if (it != pending_transfers.end() && it->second.from_uid == uid) {
            pending_transfers.erase(it); ok = true;
        }
    }
    dpp::embed e;
    e.set_title(ok ? "🚫  已取消轉帳" : "❌  找不到轉帳").set_color(ok ? 0x808080 : 0xE74C3C);
    dpp::message msg; msg.add_embed(e); return msg;
}

// ─── Bankruptcy announcement ──────────────────────────────────────────────────

inline dpp::cluster* g_bot = nullptr;

inline void announce_bankrupt(dpp::snowflake uid, dpp::snowflake channel_id) {
    if (!g_bot || !channel_id) return;
    static const std::string IMG =
        "https://media.discordapp.net/attachments/1514918524164898966/1518545976535814144/flipped-image.webp";
    g_bot->start_timer([channel_id, uid](dpp::timer t) {
        g_bot->stop_timer(t);
        g_bot->message_create(dpp::message(channel_id, IMG),
            [channel_id, uid](const dpp::confirmation_callback_t&) {
                g_bot->message_create(dpp::message(channel_id,
                    "<@" + std::to_string(uid) + "> 哈哈這人又破產了"));
            });
    }, 1);
}

// ─── Loss leaderboard ─────────────────────────────────────────────────────────
// game: "" = all, "21" "骰子" "射龍門" "火箭" "刮刮樂"

// asc=true: most negative first (虧損排序), asc=false: most positive first (盈利排序)
static dpp::message handle_losers_board(int page = 0, const std::string& game = "", bool asc = true) {
    std::vector<std::pair<dpp::snowflake, int64_t>> sorted;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        std::set<dpp::snowflake> all_uids;
        for (auto& [u,_] : bj_stats_data)     all_uids.insert(u);
        for (auto& [u,_] : dice_stats_data)    all_uids.insert(u);
        for (auto& [u,_] : shoot_stats_data)   all_uids.insert(u);
        for (auto& [u,_] : rocket_stats_data)  all_uids.insert(u);
        for (auto& [u,_] : scratch_stats_data) all_uids.insert(u);

        for (auto uid : all_uids) {
            if (!cfg.notify_user_id.empty() && std::to_string((uint64_t)uid) == cfg.notify_user_id) continue;
            int64_t total = 0;
            if (game == "" || game == "21") {
                auto it = bj_stats_data.find(uid);
                if (it != bj_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "骰子") {
                auto it = dice_stats_data.find(uid);
                if (it != dice_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "射龍門") {
                auto it = shoot_stats_data.find(uid);
                if (it != shoot_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "火箭") {
                auto it = rocket_stats_data.find(uid);
                if (it != rocket_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "刮刮樂") {
                auto it = scratch_stats_data.find(uid);
                if (it != scratch_stats_data.end()) total += it->second.profit;
            }
            sorted.push_back({uid, total});
        }
    }
    if (asc)
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second < b.second; });
    else
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });

    std::string game_label = game.empty() ? "全遊戲" : game;
    std::string dir_c = asc ? "a" : "d";
    const int PAGE_SIZE = 10;
    int total = (int)sorted.size();
    int total_pages = std::max(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    page = std::max(0, std::min(page, total_pages - 1));

    dpp::embed e;
    std::string title = asc ? ("📉  虧損榜 — " + game_label) : ("📈  盈利榜 — " + game_label);
    e.set_title(title).set_color(asc ? 0xE74C3C : 0x2ECC71);
    if (sorted.empty()) {
        e.set_description("目前沒有記錄");
    } else {
        static const char* MEDALS[] = {"🥇", "🥈", "🥉"};
        std::ostringstream oss;
        int start = page * PAGE_SIZE;
        int end   = std::min(start + PAGE_SIZE, total);
        for (int i = start; i < end; i++) {
            std::string rank = (i < 3) ? MEDALS[i] : (std::to_string(i + 1) + ".");
            int64_t val = sorted[i].second;
            std::string val_str = (val >= 0 ? "+" : "") + std::to_string(val);
            oss << rank << " <@" << (uint64_t)sorted[i].first
                << ">  **" << val_str << "** 碼\n";
        }
        e.set_description(oss.str());
    }
    e.set_footer(dpp::embed_footer().set_text(
        "第 " + std::to_string(page+1) + "/" + std::to_string(total_pages) + " 頁  共 " + std::to_string(total) + " 人"));

    dpp::message msg; msg.add_embed(e);

    // Dropdown: preserve current sort direction in values
    dpp::component sel_row; sel_row.set_type(dpp::cot_action_row);
    dpp::component sel; sel.set_type(dpp::cot_selectmenu)
        .set_id("losers_game_sel")
        .set_placeholder("選擇遊戲篩選");
    sel.add_select_option(dpp::select_option("全遊戲",  "losers_0_" + dir_c + "_",       "所有遊戲加總").set_default(game == ""));
    sel.add_select_option(dpp::select_option("21點",    "losers_0_" + dir_c + "_21",     "只看21點"));
    sel.add_select_option(dpp::select_option("骰子",    "losers_0_" + dir_c + "_骰子",   "只看骰子"));
    sel.add_select_option(dpp::select_option("射龍門",  "losers_0_" + dir_c + "_射龍門", "只看射龍門"));
    sel.add_select_option(dpp::select_option("火箭升空","losers_0_" + dir_c + "_火箭",   "只看火箭升空"));
    sel.add_select_option(dpp::select_option("刮刮樂",  "losers_0_" + dir_c + "_刮刮樂", "只看刮刮樂"));
    sel_row.add_component(sel);
    msg.add_component(sel_row);

    // Navigation + sort toggle row (always shown)
    std::string flip_c = asc ? "d" : "a";
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    dpp::component prev, sort_btn, next;
    prev.set_type(dpp::cot_button).set_label("◀")
        .set_id("losers_" + std::to_string(page-1) + "_" + dir_c + "_" + game)
        .set_style(dpp::cos_secondary).set_disabled(page == 0);
    sort_btn.set_type(dpp::cot_button)
        .set_label(asc ? "📈 切換為盈利排序" : "📉 切換為虧損排序")
        .set_id("losers_0_" + flip_c + "_" + game)
        .set_style(dpp::cos_primary);
    next.set_type(dpp::cot_button).set_label("▶")
        .set_id("losers_" + std::to_string(page+1) + "_" + dir_c + "_" + game)
        .set_style(dpp::cos_secondary).set_disabled(page == total_pages-1);
    nav_row.add_component(prev); nav_row.add_component(sort_btn); nav_row.add_component(next);
    msg.add_component(nav_row);

    return msg;
}

