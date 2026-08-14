#pragma once
#include "helpers.h"
#include <fstream>
#include <set>
#include <nlohmann/json.hpp>
#include <cmath>
#include <random>
#include <algorithm>

inline dpp::cluster* g_bot = nullptr;

static const std::string CHIPS_FILE      = "chips.json";
static const std::string INVENTORY_FILE  = "inventory.json";
static const int64_t     CLAIM_AMOUNT  = 500;
static const int64_t     WEEKLY_AMOUNT = 2000;

// ─── Persistence ──────────────────────────────────────────────────────────────

inline void save_inventory() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, inv] : inventory_data) {
            nlohmann::json inv_j;
            for (auto& [key, cnt] : inv) if (cnt > 0) inv_j[key] = cnt;
            if (!inv_j.empty()) j[std::to_string((uint64_t)uid)] = inv_j;
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(INVENTORY_FILE, j.dump(2));
}

static void load_chips() {
    std::ifstream f(CHIPS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& cd = chip_data[uid];
            cd.chips              = v["chips"].get<int64_t>();
            cd.last_claim         = (time_t)v.value("last_claim",         (int64_t)0);
            cd.last_weekly        = (time_t)v.value("last_weekly",        (int64_t)0);
            cd.last_hunt_daily    = (time_t)v.value("last_hunt_daily",    (int64_t)0);
            cd.last_weekly_scroll = (time_t)v.value("last_weekly_scroll", (int64_t)0);
            cd.vip_until          = (time_t)v.value("vip_until",          (int64_t)0);
            cd.vip_last_claim     = (time_t)v.value("vip_last_claim",     (int64_t)0);
            cd.supervisor_until   = (time_t)v.value("supervisor_until",   (int64_t)0);
            cd.insurance_until    = (time_t)v.value("insurance_until",    (int64_t)0);
            cd.free_xfer          = v.value("free_xfer",                  0);
            cd.risk_dice_day      = (time_t)v.value("risk_dice_day",  (int64_t)0);
            cd.risk_dice_uses     = v.value("risk_dice_uses",            0);
            cd.claim_fail_streak  = v.value("claim_fail_streak",         0);
        }
    } catch (...) {}
}

static void save_chips() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, cd] : chip_data)
            j[std::to_string((uint64_t)uid)] = {
                {"chips",              cd.chips},
                {"last_claim",         (int64_t)cd.last_claim},
                {"last_weekly",        (int64_t)cd.last_weekly},
                {"last_hunt_daily",    (int64_t)cd.last_hunt_daily},
                {"last_weekly_scroll", (int64_t)cd.last_weekly_scroll},
                {"vip_until",          (int64_t)cd.vip_until},
                {"vip_last_claim",     (int64_t)cd.vip_last_claim},
                {"supervisor_until",   (int64_t)cd.supervisor_until},
                {"insurance_until",    (int64_t)cd.insurance_until},
                {"free_xfer",          cd.free_xfer},
                {"risk_dice_day",      (int64_t)cd.risk_dice_day},
                {"risk_dice_uses",     cd.risk_dice_uses},
                {"claim_fail_streak",  cd.claim_fail_streak}
            };
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
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

// ─── 領取按鈕驗證（防固定排程腳本）──────────────────────────────────────────────

static std::mt19937& claim_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}
static int claim_rand(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(claim_rng());
}

// 答錯／逾時要鎖幾小時：基礎時數 × 連續失敗次數，上限 CLAIM_PENALTY_MAX_HRS，
// 讓玩家每多失敗一次，鎖定時間就再延長一次，對固定重試的腳本殺傷力遞增。
// 只要中間成功領取一次就會歸零（見 handle_claim / handle_claim_verify 的 granted 分支）。
static int claim_penalty_hours(int fail_streak) {
    return std::min(CLAIM_PENALTY_MAX_HRS, CLAIM_PENALTY_HOURS * std::max(1, fail_streak));
}

// 產生「點正確表情符號」的驗證題目
static ClaimChallenge build_claim_challenge_emoji() {
    static const std::vector<std::string> POOL = {"🍎","🍌","🍇","🍊","🍉","🍓","🍒","🥝"};
    std::vector<std::string> pool = POOL;
    std::shuffle(pool.begin(), pool.end(), claim_rng());
    ClaimChallenge c;
    c.options.assign(pool.begin(), pool.begin() + 4);
    c.correct_idx = claim_rand(0, 3);
    c.prompt = "請點擊寫著 " + c.options[c.correct_idx] + " 的按鈕";
    return c;
}

// 產生「十以內加法」的驗證題目（1+1=? 這種）
static ClaimChallenge build_claim_challenge_math() {
    int a = claim_rand(1, 9);
    int b = claim_rand(1, 10 - a);
    int answer = a + b;
    std::set<int> used = {answer};
    std::vector<int> nums = {answer};
    while (nums.size() < 4) {
        int wrong = claim_rand(0, 10);
        if (used.count(wrong)) continue;
        used.insert(wrong); nums.push_back(wrong);
    }
    std::shuffle(nums.begin(), nums.end(), claim_rng());
    ClaimChallenge c;
    for (int n : nums) c.options.push_back(std::to_string(n));
    c.correct_idx = (int)(std::find(nums.begin(), nums.end(), answer) - nums.begin());
    c.prompt = "請計算 " + std::to_string(a) + " + " + std::to_string(b) + " = ?　點擊正確答案的按鈕";
    return c;
}

static dpp::message make_claim_challenge_msg(dpp::snowflake uid, const ClaimChallenge& c) {
    // 題目可能是重新顯示的舊挑戰（expires_at 不會因此延後），顯示實際剩餘秒數而不是固定的驗證時限
    int secs_left = (int)std::max((time_t)0, c.expires_at - time(nullptr));
    dpp::embed e;
    e.set_title("🔒  領取驗證").set_color(0x3498DB);
    e.set_description(
        "為防止腳本自動化，這次領取需要驗證，" + c.prompt +
        "（" + std::to_string(secs_left) + " 秒內有效，逾時或按錯本次不會發放）：");
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    std::string uid_s = std::to_string((uint64_t)uid);
    for (int i = 0; i < (int)c.options.size(); i++) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(c.options[i]).set_id("claim_verify_" + uid_s + "_" + std::to_string(i))
            .set_style(dpp::cos_secondary));
    }
    msg.add_component(row);
    return msg;
}

// ─── Claim handler — 每整點可領一次 ──────────────────────────────────────────

// challenged_out：這次是否改發出按鈕驗證（真正發放要等玩家按對按鈕，見 handle_claim_verify）
// token_out：這次挑戰的識別碼，呼叫端送出訊息後要傳給 schedule_claim_verify_timeout 排逾時鎖定
static dpp::message handle_claim(dpp::snowflake uid, bool* claimed_out = nullptr,
                                  bool* challenged_out = nullptr, uint64_t* token_out = nullptr) {
    time_t now = time(nullptr);
    int64_t now_hour  = now / 3600;
    int64_t balance; bool success = false; bool challenged = false; int secs_left = 0;
    ClaimChallenge pending;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& cd = chip_data[uid];

        // 已經有一個尚未過期的驗證在跑，不能重新輸入指令繞過去（重新roll有機率直接不驗證就發放）
        // ——直接重新顯示同一題，不重新roll、不動 last_claim
        auto cit = claim_challenges.find(uid);
        if (cit != claim_challenges.end() && now <= cit->second.expires_at) {
            pending = cit->second;
            challenged = true;
            balance = cd.chips;
        } else {
            int64_t last_hour = cd.last_claim / 3600;
            if (now_hour > last_hour) {
                // 每次手動領取都有機率觸發驗證，不看是否連續整點——避免玩家/腳本靠跳過整點來規避
                if (claim_rand(1, 100) <= CLAIM_VERIFY_CHANCE) {
                    pending = (claim_rand(0, 1) == 0) ? build_claim_challenge_emoji() : build_claim_challenge_math();
                    pending.expires_at = now + CLAIM_VERIFY_SECS;
                    pending.token      = claim_challenge_token_seq++;
                    claim_challenges[uid] = pending;
                    challenged = true; balance = cd.chips;
                } else {
                    cd.chips += CLAIM_AMOUNT;
                    cd.last_claim = now;
                    cd.claim_fail_streak = 0;
                    success = true;
                    balance = cd.chips;
                }
            } else {
                secs_left = (int)(((now / 3600) + 1) * 3600 - now);
                balance = cd.chips;
            }
        }
    }
    if (claimed_out)    *claimed_out    = success;
    if (challenged_out) *challenged_out = challenged;
    if (token_out)       *token_out     = pending.token;
    if (challenged) return make_claim_challenge_msg(uid, pending);
    save_chips();
    return make_claim_msg(uid, success, balance, secs_left);
}

static dpp::message make_claim_verify_timeout_msg(int hours) {
    dpp::embed e;
    e.set_title("⌛  逾時未回應").set_color(0xE74C3C);
    e.set_description("你沒有在時限內完成驗證，這次沒有領到，接下來 **" +
                       std::to_string(hours) + "** 小時無法領取。");
    dpp::message msg; msg.add_embed(e); return msg;
}

// 逾時計時器共用邏輯：處罰並回傳「要編輯哪則訊息」跟這次鎖了幾小時；
// 找不到/已被處理過就回傳 false（不編輯、不處罰，代表答案已經在別處被消化掉了）
static bool claim_verify_timeout_penalize(dpp::snowflake uid, uint64_t token, ClaimChallenge* out, int* hours_out) {
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = claim_challenges.find(uid);
    if (it == claim_challenges.end() || it->second.token != token) return false;
    if (out) *out = it->second;
    auto& cd = chip_data[uid];
    cd.claim_fail_streak++;
    int hours = claim_penalty_hours(cd.claim_fail_streak);
    cd.last_claim = time(nullptr) + (time_t)hours * 3600;
    if (hours_out) *hours_out = hours;
    claim_challenges.erase(it);
    return true;
}

// 送出驗證訊息後呼叫（一般頻道訊息，!領取）：記錄訊息位置、並排一個逾時計時器，
// 時間到了如果玩家完全沒按，視同驗證失敗一併鎖住（時數隨連續失敗次數遞增）。
static void schedule_claim_verify_timeout(dpp::snowflake uid, dpp::snowflake ch,
                                           dpp::snowflake mid, uint64_t token) {
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = claim_challenges.find(uid);
        if (it != claim_challenges.end() && it->second.token == token) {
            it->second.channel_id = ch;
            it->second.message_id = mid;
        }
    }
    g_bot->start_timer([uid, token](dpp::timer t) {
        g_bot->stop_timer(t);
        ClaimChallenge c; int hours = 0;
        if (!claim_verify_timeout_penalize(uid, token, &c, &hours)) return;
        save_chips();
        if (!c.channel_id || !c.message_id) return;
        dpp::message edit = make_claim_verify_timeout_msg(hours);
        edit.id = c.message_id; edit.channel_id = c.channel_id;
        g_bot->message_edit(edit);
    }, CLAIM_VERIFY_SECS);
}

// 送出驗證訊息後呼叫（ephemeral 互動回覆，/領取）：ephemeral 訊息只能靠 interaction token 編輯。
static void schedule_claim_verify_timeout_interaction(dpp::snowflake uid, const std::string& itoken, uint64_t token) {
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = claim_challenges.find(uid);
        if (it != claim_challenges.end() && it->second.token == token)
            it->second.interaction_token = itoken;
    }
    g_bot->start_timer([uid, itoken, token](dpp::timer t) {
        g_bot->stop_timer(t);
        ClaimChallenge c; int hours = 0;
        if (!claim_verify_timeout_penalize(uid, token, &c, &hours)) return;
        save_chips();
        if (c.interaction_token.empty()) return;
        g_bot->interaction_response_edit(itoken, make_claim_verify_timeout_msg(hours));
    }, CLAIM_VERIFY_SECS);
}

// ─── 領取驗證按鈕結果 ──────────────────────────────────────────────────────────

// granted_out：驗證是否成功並實際發放了籌碼（銀行自動還款等後續處理交給呼叫端）
static dpp::message handle_claim_verify(dpp::snowflake uid, int idx, bool* granted_out = nullptr) {
    bool granted = false, no_record = false, wrong = false, timed_out = false;
    int64_t balance = 0; int penalty_hours = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = claim_challenges.find(uid);
        if (it == claim_challenges.end()) {
            // 沒有進行中的驗證（可能已經處理過、或被新的一次取代），不處罰
            no_record = true;
        } else {
            time_t now = time(nullptr);
            bool late = now > it->second.expires_at;
            if (!late && idx == it->second.correct_idx) {
                auto& cd = chip_data[uid];
                cd.chips += CLAIM_AMOUNT;
                cd.last_claim = now;
                cd.claim_fail_streak = 0;
                balance = cd.chips;
                granted = true;
            } else {
                // 答錯或逾時才按：這次不能領，接下來鎖住的時數 = 基礎時數 × 連續失敗次數
                // （直接把 last_claim 往後推，沿用既有的整點冷卻判斷，不用額外欄位）
                auto& cd = chip_data[uid];
                cd.claim_fail_streak++;
                penalty_hours = claim_penalty_hours(cd.claim_fail_streak);
                cd.last_claim = now + (time_t)penalty_hours * 3600;
                if (late) timed_out = true; else wrong = true;
            }
            claim_challenges.erase(it);
        }
    }
    if (granted_out) *granted_out = granted;
    dpp::embed e;
    if (granted) {
        save_chips();
        e.set_title("🪙  驗證成功，領取成功！").set_color(0xF1C40F);
        e.add_field("💰  獲得",     std::to_string(CLAIM_AMOUNT) + " 碼", true);
        e.add_field("💼  目前持有", std::to_string(balance) + " 碼",      true);
    } else if (no_record) {
        save_chips();
        e.set_title("⌛  驗證已過期").set_color(0x808080);
        e.set_description("請重新輸入 `!領取` 或 `/領取` 再試一次。");
    } else {
        save_chips();
        e.set_title("❌  驗證失敗").set_color(0xE74C3C);
        e.set_description(
            std::string(timed_out ? "逾時沒有按" : "按錯按鈕了") +
            "，這次沒有領到，接下來 **" + std::to_string(penalty_hours) + "** 小時無法領取。");
    }
    dpp::message msg; msg.add_embed(e);
    return msg;
}

// ─── 小黑屋（管理員專用）：查看／解除領取驗證的鎖定 ─────────────────────────────

static dpp::message make_claim_jail_msg() {
    time_t now = time(nullptr);
    std::vector<std::pair<dpp::snowflake, time_t>> locked; // uid, 剩餘秒數
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, cd] : chip_data)
            if (cd.last_claim > now) locked.push_back({uid, cd.last_claim - now});
    }
    std::sort(locked.begin(), locked.end(), [](auto& a, auto& b) { return a.second > b.second; });

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2C, 0x3E, 0x50));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content("## 🔒 小黑屋"));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    const int MAX_ROWS = 20; // 每行一個 section，避免訊息過長
    if (locked.empty()) {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("目前沒有人被鎖 🎉"));
    } else {
        int n = 0;
        for (auto& [uid, secs] : locked) {
            if (n >= MAX_ROWS) break;
            int h = (int)(secs / 3600), m = (int)((secs % 3600) / 60), s = (int)(secs % 60);
            char buf[16]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
            std::string text = "<@" + std::to_string((uint64_t)uid) + ">　剩餘 " + std::string(buf);
            container.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label("🔓 解鎖").set_id("claimjail_unlock_" + std::to_string((uint64_t)uid))
                    .set_style(dpp::cos_danger)));
            n++;
        }
        if ((int)locked.size() > MAX_ROWS)
            container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
                .set_content("-# 還有 " + std::to_string(locked.size() - MAX_ROWS) + " 人沒有顯示"));
    }
    msg.add_component_v2(container);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📋 名單").set_id("claimjail_list").set_style(dpp::cos_secondary));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 重整").set_id("claimjail_home").set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

static dpp::message make_claim_jail_list_msg() {
    std::vector<std::pair<dpp::snowflake, int>> fails;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, cd] : chip_data)
            if (cd.claim_fail_streak > 0) fails.push_back({uid, cd.claim_fail_streak});
    }
    std::sort(fails.begin(), fails.end(), [](auto& a, auto& b) { return a.second > b.second; });

    std::string content = "## 📋 答錯／逾時次數\n";
    if (fails.empty()) content += "目前沒有人答錯過。";
    else for (auto& [uid, cnt] : fails)
        content += "<@" + std::to_string((uint64_t)uid) + ">　**" + std::to_string(cnt) + "** 次\n";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2C, 0x3E, 0x50));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回小黑屋").set_id("claimjail_home").set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

// 解除某玩家的領取鎖定（清空鎖定時間跟連續失敗次數，等於完全赦免）
static void claim_jail_unlock(dpp::snowflake target) {
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& cd = chip_data[target];
        cd.last_claim        = 0;
        cd.claim_fail_streak = 0;
    }
    save_chips();
}

// ─── Weekly claim — 每週四中午12:00(UTC+8) = 週四04:00 UTC ────────────────────
// 1970-01-01 was a Thursday, so week_id = (t - 4*3600) / 604800

static int64_t weekly_id(time_t t) {
    return ((int64_t)t - 4 * 3600) / (7 * 86400);
}

static dpp::message handle_weekly_claim(dpp::snowflake uid, bool* claimed_out = nullptr) {
    time_t now = time(nullptr);
    int64_t cur_week = weekly_id(now);
    int64_t balance; bool success; bool scroll_given = false;
    time_t next_thu = (time_t)((cur_week + 1) * (int64_t)(7 * 86400) + 4 * 3600);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& cd = chip_data[uid];
        if (cur_week > weekly_id(cd.last_weekly)) {
            cd.chips += WEEKLY_AMOUNT;
            cd.free_xfer = 2; // 上限 2 次，不累積
            cd.last_weekly = now;
            success = true;
            balance = cd.chips;
        } else {
            success = false;
            balance = cd.chips;
        }
        // Top up weekly raid scroll to max (1) if not yet done this week
        static const int MAX_WEEKLY_SCROLL = 1;
        if (cur_week > weekly_id(cd.last_weekly_scroll)) {
            int cur = inventory_data[uid].count("weekly_hunt_scroll")
                      ? inventory_data[uid]["weekly_hunt_scroll"] : 0;
            int to_give = std::max(0, MAX_WEEKLY_SCROLL - cur);
            if (to_give > 0) {
                inventory_data[uid]["weekly_hunt_scroll"] += to_give;
                scroll_given = true;
            }
            cd.last_weekly_scroll = now;
        }
    }
    if (claimed_out) *claimed_out = success;
    save_chips();
    if (scroll_given) save_inventory();

    dpp::embed e;
    if (success) {
        e.set_title("🎁  每週領取成功！").set_color(0xF1C40F);
        e.add_field("💰  獲得",     std::to_string(WEEKLY_AMOUNT) + " 碼", true);
        e.add_field("💼  目前持有", std::to_string(balance)        + " 碼", true);
        e.add_field("🎟️  免手續費轉帳", "+2 次（累計可使用）", false);
        if (scroll_given)
            e.add_field("🎫  組隊王挑戰卷", "獲得 **每週怪物狩獵卷** ×1", false);
        e.set_footer(dpp::embed_footer().set_text("每週四中午12:00（UTC+8）更新"));
    } else {
        e.set_title("⏳  本週已領取").set_color(0x808080);
        e.add_field("⏰  下次領取", "<t:" + std::to_string((int64_t)next_thu) + ":F>", true);
        e.add_field("💼  目前持有", std::to_string(balance) + " 碼",                  true);
        if (scroll_given)
            e.add_field("🎫  組隊王挑戰卷", "獲得 **每週怪物狩獵卷** ×1（本週首次）", false);
    }
    dpp::message msg; msg.add_embed(e); return msg;
}

// ─── Leaderboard with pagination ──────────────────────────────────────────────

static dpp::message handle_leaderboard(int page = 0) {
    std::map<std::string, int64_t> stock_prices;
    { std::lock_guard<std::mutex> lk(stock_mutex);
      for (auto& [k, s] : stock_market) stock_prices[k] = s.price; }

    std::map<dpp::snowflake, int64_t> wealth_map;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, cd] : chip_data) wealth_map[uid] += cd.chips;
        for (auto& [uid, bd] : bank_data)
            if (bd.deposited > 0) wealth_map[uid] += bd.deposited;
        for (auto& [uid, holdings] : player_stocks) {
            int64_t sv = 0;
            for (auto& [key, h] : holdings) {
                auto pit = stock_prices.find(key);
                if (pit != stock_prices.end() && pit->second > 0) sv += h.shares * pit->second;
            }
            if (sv > 0) wealth_map[uid] += sv;
        }
    }
    std::vector<std::pair<dpp::snowflake, int64_t>> sorted;
    for (auto& [uid, w] : wealth_map) if (w > 0) sorted.push_back({uid, w});
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
        "第 " + std::to_string(page+1) + "/" + std::to_string(total_pages) + " 頁  共 " + std::to_string(total) + " 人（含銀行存款、股票市值）"));

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
                                    const std::string& from_name = "",
                                    bool use_free = false) {
    dpp::embed e;
    if (from_uid == to_uid) {
        e.set_title("❌  無法轉帳給自己").set_color(0xE74C3C);
        dpp::message msg; msg.add_embed(e); return msg;
    }
    if (amount <= 0) {
        e.set_title("❌  金額必須大於 0").set_color(0xE74C3C);
        dpp::message msg; msg.add_embed(e); return msg;
    }
    bool has_lovebook = false;
    { std::lock_guard<std::mutex> lk(data_mutex); has_lovebook = col_has_lovebook(from_uid); }
    int64_t fee   = (use_free || has_lovebook) ? 0 : (amount + 99) / 100;
    int64_t total = amount + fee;
    int64_t from_bal = 0, to_bal = 0;
    bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        int64_t have = chip_data[from_uid].chips;
        if (have >= total) {
            chip_data[from_uid].chips -= total;
            chip_data[to_uid].chips   += amount;
            from_bal = chip_data[from_uid].chips;
            to_bal   = chip_data[to_uid].chips;
            ok = true;
        }
    }
    if (!ok) {
        e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        e.set_description(use_free
            ? "你的籌碼不足 **" + std::to_string(amount) + "** 碼"
            : "你的籌碼不足 **" + std::to_string(total) + "** 碼（含手續費 " + std::to_string(fee) + " 碼）");
        dpp::message msg; msg.add_embed(e); return msg;
    }
    save_chips();
    e.set_title("💸  轉帳成功").set_color(0x2ECC71);
    if (!from_name.empty())
        e.add_field("👤  轉帳者",    from_name,                        true);
    e.add_field("👤  收款人",    to_name,                           true);
    e.add_field("💰  轉帳金額",  std::to_string(amount) + " 碼",    false);
    if (has_lovebook) {
        e.add_field("💕  手續費",   "免費（貓哥的戀愛教典）",       false);
    } else if (use_free) {
        e.add_field("🎟️  手續費",   "免費（使用 1 次免手續費）",       false);
    } else {
        e.add_field("💳  手續費（1%）", std::to_string(fee) + " 碼",  true);
        e.add_field("💸  實際支付",  std::to_string(total)  + " 碼",  true);
    }
    e.add_field("💼  轉帳者餘額", std::to_string(from_bal) + " 碼",  true);
    e.add_field("💼  收款人餘額", std::to_string(to_bal)   + " 碼",  true);
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
    int64_t bal      = get_chips(from_uid);
    int      free_n  = 0;
    bool     has_lovebook = false;
    { std::lock_guard<std::mutex> lk(data_mutex);
      free_n = chip_data[from_uid].free_xfer;
      has_lovebook = col_has_lovebook(from_uid);
    }
    int64_t fee   = has_lovebook ? 0 : (amount + 99) / 100;
    int64_t total = amount + fee;
    if (bal < total && (free_n == 0 || bal < amount)) {
        // 付費模式也不夠，且沒有免費次數（或免費模式也不夠）
        e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        e.set_description("你持有 **" + std::to_string(bal) + "** 碼，轉帳 " +
            std::to_string(amount) + " 碼需支付 **" + std::to_string(total) + "** 碼（含手續費 " + std::to_string(fee) + " 碼）");
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
    e.add_field("👤  轉帳者",      from_name,                           true);
    e.add_field("👤  收款人",      to_name,                             true);
    e.add_field("💰  轉帳金額",    std::to_string(amount) + " 碼",      false);
    e.add_field("💳  手續費（1%）", std::to_string(fee)   + " 碼",      true);
    e.add_field("💸  付費模式支付", std::to_string(total)  + " 碼",      true);
    e.add_field("💼  你的餘額",    std::to_string(bal)    + " 碼",      true);
    e.add_field("🎟️  免手續費次數", std::to_string(free_n) + " 次",     true);
    e.set_footer(dpp::embed_footer().set_text("請在 60 秒內確認"));

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    std::string tid_s = std::to_string(tid);
    dpp::component ok_btn, free_btn, cancel_btn;
    ok_btn.set_type(dpp::cot_button).set_label("✅ 確認（付手續費）")
          .set_id("xfer_ok_" + tid_s).set_style(dpp::cos_success)
          .set_disabled(bal < total);
    free_btn.set_type(dpp::cot_button).set_label("🎟️ 免費轉帳（" + std::to_string(free_n) + " 次）")
            .set_id("xfer_free_" + tid_s).set_style(dpp::cos_primary)
            .set_disabled(free_n <= 0 || bal < amount);
    cancel_btn.set_type(dpp::cot_button).set_label("❌ 取消")
              .set_id("xfer_cancel_" + tid_s).set_style(dpp::cos_danger);
    row.add_component(ok_btn); row.add_component(free_btn); row.add_component(cancel_btn);
    msg.add_component(row);
    return msg;
}

static dpp::message handle_transfer_confirm(uint64_t tid, dpp::snowflake uid, bool use_free = false) {
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
        if (use_free) {
            auto& cd = chip_data[uid];
            if (cd.free_xfer > 0) cd.free_xfer--;
            else use_free = false; // 次數用完，退回付費模式
        }
    }
    return handle_transfer(pt.from_uid, pt.to_uid, pt.amount, pt.to_name, pt.from_name, use_free);
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

inline void announce_bankrupt(dpp::snowflake uid, dpp::snowflake channel_id) {
    if (!g_bot || !channel_id) return;
    static const std::string IMG =
        "https://media.discordapp.net/attachments/1514918524164898966/1518545976535814144/flipped-image.webp";
    bool has_loan = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = bank_data.find(uid);
        has_loan = (it != bank_data.end() && it->second.loan > 0);
    }
    std::string text = "<@" + std::to_string((uint64_t)uid) + "> ";
    text += has_loan
        ? "你都有欠債了還敢賭博? 快去好好工作!"
        : "哎呀破產了阿 要不要試試看我們的銀行系統呢? 輸入 !銀行";
    g_bot->start_timer([channel_id, text](dpp::timer t) {
        g_bot->stop_timer(t);
        g_bot->message_create(dpp::message(channel_id, IMG),
            [channel_id, text](const dpp::confirmation_callback_t&) {
                g_bot->message_create(dpp::message(channel_id, text));
            });
    }, 5);
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

