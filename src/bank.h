#pragma once
#include "chips.h"
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

static const double  DEPOSIT_RATE = 0.005;   // 0.5% 每日最低餘額
static const double  LOAN_RATE    = 0.05;
static const int64_t MAX_LOAN     = 10000;
static const std::string BANK_FILE = "bank.json";

// UTC+8 的「天」編號（每天 00:00 CST = UTC 16:00 前一天）
static int64_t utc8_day_number() {
    return ((int64_t)time(nullptr) + 8 * 3600) / 86400;
}

// 下一個 UTC+8 午夜的 Unix 時間（用來顯示 Discord timestamp）
static time_t next_utc8_midnight() {
    int64_t today = utc8_day_number();
    return (time_t)((today + 1) * 86400 - 8 * 3600);
}

// ─── Persistence ──────────────────────────────────────────────────────────────

static void load_bank() {
    std::ifstream f(BANK_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        int64_t today = utc8_day_number();
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& bd = bank_data[uid];
            bd.deposited         = v.value("deposited",         (int64_t)0);
            bd.deposit_time      = v.value("deposit_time",      (int64_t)0);
            bd.loan              = v.value("loan",              (int64_t)0);
            bd.loan_time         = v.value("loan_time",         (int64_t)0);
            bd.daily_min         = v.value("daily_min",         (int64_t)0);
            bd.last_interest_day = v.value("last_interest_day", (int64_t)0);
            // 舊資料移轉：初始化 daily_min 和 last_interest_day
            if (bd.deposited > 0 && bd.daily_min == 0)
                bd.daily_min = bd.deposited;
            if (bd.deposited > 0 && bd.last_interest_day == 0)
                bd.last_interest_day = today;
        }
    } catch (...) {}
}

static void save_bank() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, bd] : bank_data) {
            if (bd.deposited == 0 && bd.loan == 0) continue;
            j[std::to_string((uint64_t)uid)] = {
                {"deposited",         bd.deposited},
                {"deposit_time",      bd.deposit_time},
                {"loan",              bd.loan},
                {"loan_time",         bd.loan_time},
                {"daily_min",         bd.daily_min},
                {"last_interest_day", bd.last_interest_day}
            };
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(BANK_FILE, j.dump(2));
}

// ─── Calculations ─────────────────────────────────────────────────────────────

static int64_t calc_loan_with_interest(int64_t principal, int64_t loan_time_val) {
    if (principal <= 0 || loan_time_val <= 0) return principal;
    int64_t days = (time(nullptr) - loan_time_val) / 86400;
    if (days <= 0) return principal;
    return (int64_t)(principal * std::pow(1.0 + LOAN_RATE, (double)days) + 0.5);
}

// ─── Daily interest — 每天 UTC+8 午夜由背景 timer 呼叫 ────────────────────────

static void apply_daily_interest() {
    int64_t today = utc8_day_number();
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, bd] : bank_data) {
            if (bd.deposited <= 0) continue;
            if (bd.last_interest_day >= today) continue;
            // 補齊離線期間每天的利息
            while (bd.last_interest_day < today) {
                int64_t interest = (int64_t)(bd.daily_min * DEPOSIT_RATE + 0.5);
                bd.deposited += interest;
                bd.daily_min  = bd.deposited;
                bd.last_interest_day++;
            }
            changed = true;
        }
    }
    if (changed) save_bank();
}

// ─── Auto-repay: 領取時自動扣還借款 ──────────────────────────────────────────

static int64_t bank_auto_repay(dpp::snowflake uid, int64_t max_repay) {
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = bank_data.find(uid);
    if (it == bank_data.end() || it->second.loan == 0) return 0;

    auto& bd = it->second;
    int64_t total_owed = calc_loan_with_interest(bd.loan, bd.loan_time);

    auto& cd = chip_data[uid];
    int64_t repaid = std::min({cd.chips, total_owed, max_repay});
    if (repaid <= 0) return 0;

    int64_t days_elapsed = (time(nullptr) - bd.loan_time) / 86400;
    cd.chips   -= repaid;
    total_owed -= repaid;

    if (total_owed <= 0) {
        bd.loan = 0; bd.loan_time = 0;
    } else {
        bd.loan = total_owed;
        bd.loan_time += days_elapsed * 86400;
    }
    return repaid;
}

// ─── Bank UI ──────────────────────────────────────────────────────────────────

static dpp::message make_bank_msg(dpp::snowflake uid, const std::string& notice = "") {
    int64_t deposited, daily_min, last_day, loan, loan_time_v, chips;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& bd = bank_data[uid];
        deposited  = bd.deposited;
        daily_min  = bd.daily_min;
        last_day   = bd.last_interest_day;
        loan       = bd.loan;
        loan_time_v = bd.loan_time;
        chips      = chip_data[uid].chips;
    }

    int64_t loan_total = calc_loan_with_interest(loan, loan_time_v);
    time_t  next_mid   = next_utc8_midnight();

    dpp::embed e;
    e.set_title("🏦  銀行").set_color(0x27AE60);
    if (!notice.empty()) e.set_description(notice);

    if (deposited > 0) {
        e.add_field("💰  存款餘額", "**" + std::to_string(deposited) + "** 碼", false);
        std::string min_s = "**" + std::to_string(daily_min) + "** 碼";
        min_s += "（今日計息基礎，利息 +" + std::to_string((int64_t)(daily_min * DEPOSIT_RATE + 0.5)) + " 碼）";
        e.add_field("📉  今日最低餘額", min_s, false);
        e.add_field("⏰  下次計息",
            "<t:" + std::to_string((int64_t)next_mid) + ":f>（<t:" + std::to_string((int64_t)next_mid) + ":R>）", false);
    }
    if (loan_total > 0) {
        std::string s = "**" + std::to_string(loan_total) + "** 碼";
        if (loan_total > loan)
            s += "（本金 " + std::to_string(loan) + "，利息 +" + std::to_string(loan_total - loan) + "）";
        e.add_field("💸  借款餘額", s, false);
        int64_t loan_days      = (time(nullptr) - loan_time_v) / 86400;
        int64_t next_loan_tick = loan_time_v + (loan_days + 1) * 86400;
        e.add_field("⏰  借款下次跳息",
            "<t:" + std::to_string(next_loan_tick) + ":f>（<t:" + std::to_string(next_loan_tick) + ":R>）", false);
    }
    if (deposited == 0 && loan_total == 0)
        e.add_field("📊  目前狀態", "無存款、無借款", false);

    e.add_field("💼  錢包持有", "**" + std::to_string(chips) + "** 碼", false);
    e.set_footer(dpp::embed_footer().set_text(
        "存款利率 0.5%/天（每日最低餘額計息，午夜 12:00 結算）· 借款利率 5%/天（複利）· 借款上限 10,000 碼"));

    std::string sid = std::to_string((uint64_t)uid);
    bool has_dep   = deposited > 0;
    bool has_loan_b = loan_total > 0;

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 存款").set_id("bank_deposit_" + sid)
        .set_style(dpp::cos_success).set_disabled(has_loan_b));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📤 提款").set_id("bank_withdraw_" + sid)
        .set_style(dpp::cos_success).set_disabled(!has_dep));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💸 借款").set_id("bank_borrow_" + sid)
        .set_style(dpp::cos_danger).set_disabled(has_loan_b || has_dep));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💳 還款").set_id("bank_repay_" + sid)
        .set_style(dpp::cos_primary).set_disabled(!has_loan_b));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 重整").set_id("bank_refresh_" + sid)
        .set_style(dpp::cos_secondary));

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("← 返回錢包").set_id("wallet_home_" + sid).set_style(dpp::cos_secondary));

    dpp::message msg; msg.add_embed(e); msg.add_component(row); msg.add_component(row2);
    return msg;
}
