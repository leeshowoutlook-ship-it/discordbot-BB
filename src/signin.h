#pragma once
#include "types.h"
#include "helpers.h"
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <nlohmann/json.hpp>

// ─── Permission: admin / 副會長 / 會長 ────────────────────────────────────────

static bool si_perm(dpp::snowflake uid, const std::vector<dpp::snowflake>& roles) {
    if (!cfg.notify_user_id.empty() && std::to_string(uid) == cfg.notify_user_id)
        return true;
    for (auto& rid : roles) {
        const dpp::role* r = dpp::find_role(rid);
        if (r && r->name.find("會長") != std::string::npos)
            return true;
    }
    return false;
}
static bool si_perm(const dpp::interaction& cmd) {
    return si_perm(cmd.member.user_id, cmd.member.get_roles());
}

// ─── 截止時間解析 ─────────────────────────────────────────────────────────────
// 支援：HH:MM（今天幾點）、Xm（X分鐘後）、Xh（X小時後）；無法解析回傳 0

static time_t parse_si_deadline(const std::string& s) {
    if (s.empty()) return 0;
    time_t now = time(nullptr);

    // HH:MM
    if (s.size() == 5 && s[2] == ':') {
        try {
            int h = std::stoi(s.substr(0, 2));
            int m = std::stoi(s.substr(3, 2));
            if (h < 0 || h > 23 || m < 0 || m > 59) return 0;
            struct tm lt{}; localtime_s(&lt, &now);
            lt.tm_hour = h; lt.tm_min = m; lt.tm_sec = 0;
            time_t t = mktime(&lt);
            if (t <= now) t += 86400; // 已過則改為明天
            return t;
        } catch (...) { return 0; }
    }

    // Xm（分鐘）
    if (!s.empty() && (s.back() == 'm' || s.back() == 'M')) {
        try { int v = std::stoi(s.substr(0, s.size() - 1)); return v > 0 ? now + v * 60 : 0; }
        catch (...) { return 0; }
    }

    // Xh（小時）
    if (!s.empty() && (s.back() == 'h' || s.back() == 'H')) {
        try { int v = std::stoi(s.substr(0, s.size() - 1)); return v > 0 ? now + v * 3600 : 0; }
        catch (...) { return 0; }
    }

    return 0;
}

static std::string si_deadline_str() {
    if (g_signin.deadline == 0) return "";
    return "\n⏰ 截止時間：<t:" + std::to_string((int64_t)g_signin.deadline) + ":t>（<t:"
         + std::to_string((int64_t)g_signin.deadline) + ":R>）";
}

// ─── 主簽到訊息 ───────────────────────────────────────────────────────────────

static dpp::message make_si_start_msg(int total) {
    dpp::embed e;
    e.set_title("📋 簽到開始！").set_color(0x3498DB);
    e.set_description(
        "共 **" + std::to_string(total) + "** 位成員需要簽到。\n\n"
        "✅ 已簽到：**0** 人\n"
        "❌ 未簽到：**" + std::to_string(total) + "** 人"
        + si_deadline_str() + "\n\n"
        "請點擊下方按鈕進行簽到驗證！"
    );
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 我要簽到").set_id("si_btn").set_style(dpp::cos_success));
    dpp::message m; m.add_embed(e); m.add_component(row);
    return m;
}

// 更新主簽到訊息（呼叫前必須已持有 data_mutex）
static dpp::message make_si_status_msg() {
    int signed_n = (int)g_signin.signed_in.size();
    int unsign_n = (int)g_signin.not_signed.size();
    dpp::embed e;
    e.set_title("📋 簽到進行中").set_color(0x3498DB);
    e.set_description(
        "總人數：**" + std::to_string(signed_n + unsign_n) + "** 人\n\n"
        "✅ 已簽到：**" + std::to_string(signed_n) + "** 人\n"
        "❌ 未簽到：**" + std::to_string(unsign_n) + "** 人"
        + si_deadline_str() + "\n\n"
        "請點擊下方按鈕進行簽到驗證！"
    );
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 我要簽到").set_id("si_btn").set_style(dpp::cos_success));
    dpp::message m; m.add_embed(e); m.add_component(row);
    return m;
}

// 簽到結束訊息（移除按鈕；呼叫前持有 data_mutex）
static dpp::message make_si_closed_msg() {
    int signed_n = (int)g_signin.signed_in.size();
    int unsign_n = (int)g_signin.not_signed.size();
    dpp::embed e;
    e.set_title("📋 簽到已結束").set_color(0x95A5A6);
    e.set_description(
        "✅ 已簽到：**" + std::to_string(signed_n) + "** 人\n"
        "❌ 未簽到：**" + std::to_string(unsign_n) + "** 人\n"
        "（簽到時間已截止）"
    );
    dpp::message m; m.add_embed(e); // 不附加 component，移除簽到按鈕
    return m;
}

// ─── 驗證挑戰（ephemeral） ────────────────────────────────────────────────────

struct SiQuestion {
    std::string prompt;
    std::vector<std::string> options; // 4 個選項
    int correct_idx;                  // 0-based 正確答案
};

static const std::vector<SiQuestion>& si_questions() {
    static const std::vector<SiQuestion> Q = {
        { "我們公會名稱是", { "BigBase", "SmallBase", "GayBar", "GuyBand" }, 0 },
    };
    return Q;
}

static dpp::message make_si_verify_msg(dpp::snowflake uid) {
    static std::mt19937 rng(std::random_device{}());

    const auto& q = si_questions()[0];

    // 打亂選項順序，同時追蹤正確答案落在哪個位置
    std::vector<int> order(q.options.size());
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);

    int shuffled_correct = 0;
    for (int i = 0; i < (int)order.size(); i++)
        if (order[i] == q.correct_idx) { shuffled_correct = i; break; }

    dpp::embed e;
    e.set_title("🔐 簽到驗證").set_color(0xF39C12);
    e.set_description(q.prompt);

    dpp::message msg;
    msg.set_flags(dpp::m_ephemeral);
    msg.add_embed(e);

    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (int i = 0; i < (int)order.size(); i++) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(std::to_string(i + 1) + ". " + q.options[order[i]])
            .set_id("si_v_" + uid_s + "_" + std::to_string(i) + "_" + std::to_string(shuffled_correct))
            .set_style(dpp::cos_secondary));
    }
    msg.add_component(row);
    return msg;
}

// ─── 名單總覽（呼叫前持有 data_mutex） ────────────────────────────────────────

static dpp::message make_si_overview_msg() {
    int signed_n = (int)g_signin.signed_in.size();
    int unsign_n = (int)g_signin.not_signed.size();

    dpp::embed e;
    e.set_title("📋 簽到名單").set_color(0x2C3E50);
    e.set_description(
        "📊 **目前狀況**\n"
        "✅ 已簽到：**" + std::to_string(signed_n) + "** 人\n"
        "❌ 未簽到：**" + std::to_string(unsign_n) + "** 人\n"
        "📌 總計：**" + std::to_string(signed_n + unsign_n) + "** 人"
    );

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 已簽到（" + std::to_string(signed_n) + "）")
        .set_id("si_checked").set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 未簽到（" + std::to_string(unsign_n) + "）")
        .set_id("si_unc_0").set_style(dpp::cos_danger)
        .set_disabled(unsign_n == 0));

    dpp::message m; m.add_embed(e); m.add_component(row);
    return m;
}

// ─── 已簽到名單（呼叫前持有 data_mutex） ─────────────────────────────────────

static dpp::message make_si_checked_msg() {
    std::string desc;
    int i = 1;
    for (auto& [uid, name] : g_signin.signed_in)
        desc += std::to_string(i++) + ". " + name + "\n";
    if (desc.empty()) desc = "（尚無人簽到）";

    dpp::embed e;
    e.set_title("✅ 已簽到名單（" + std::to_string(g_signin.signed_in.size()) + " 人）");
    e.set_color(0x27AE60);
    e.set_description(desc);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔙 返回").set_id("si_back").set_style(dpp::cos_secondary));

    dpp::message m; m.add_embed(e); m.add_component(row);
    return m;
}

// ─── 未簽到名單分頁（10人/頁、有踢出按鈕；呼叫前持有 data_mutex） ────────────

static dpp::message make_si_unchecked_msg(int page) {
    std::vector<std::pair<dpp::snowflake, std::string>> members(
        g_signin.not_signed.begin(), g_signin.not_signed.end());

    int total = (int)members.size();
    int pages = std::max(1, (total + 9) / 10);
    if (page < 0)       page = 0;
    if (page >= pages)  page = pages - 1;

    int start = page * 10;
    int end   = std::min(start + 10, total);

    std::string desc;
    for (int i = start; i < end; i++)
        desc += std::to_string(i + 1) + ". " + members[i].second + "\n";
    if (desc.empty()) desc = "（所有人已簽到！）";

    dpp::embed e;
    e.set_title("❌ 未簽到名單（" + std::to_string(total) + " 人）— 第 " +
                std::to_string(page + 1) + " / " + std::to_string(pages) + " 頁");
    e.set_color(0xE74C3C);
    e.set_description(desc);

    dpp::message m; m.add_embed(e);

    // 每行最多 5 個踢出按鈕，最多 2 行（共 10 人）
    std::string page_s = std::to_string(page);
    for (int row_i = 0; row_i < 2; row_i++) {
        int row_start = start + row_i * 5;
        if (row_start >= end) break;
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int col = 0; col < 5 && (row_start + col) < end; col++) {
            auto& [tuid, name] = members[row_start + col];
            std::string label;
            if (name.rfind("<@", 0) == 0) {
                label = "#" + std::to_string(row_start + col + 1);
            } else {
                label = name.size() > 10 ? name.substr(0, 9) + "…" : name;
            }
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("踢 " + label)
                .set_id("si_kick_" + std::to_string((uint64_t)tuid) + "_" + page_s)
                .set_style(dpp::cos_danger));
        }
        m.add_component(row);
    }

    // 導航列
    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⬅ 上一頁").set_id("si_unc_" + std::to_string(page - 1))
        .set_style(dpp::cos_secondary).set_disabled(page == 0));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔙 返回").set_id("si_back")
        .set_style(dpp::cos_secondary));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("下一頁 ➡").set_id("si_unc_" + std::to_string(page + 1))
        .set_style(dpp::cos_secondary).set_disabled(page >= pages - 1));
    m.add_component(nav);

    return m;
}

// ─── 踢出確認畫面（呼叫前持有 data_mutex） ────────────────────────────────────

static dpp::message make_si_kick_confirm_msg(dpp::snowflake tuid,
                                              const std::string& name, int page) {
    dpp::embed e;
    e.set_title("⚠️ 確認踢出").set_color(0xE67E22);
    e.set_description("確定要將 **" + name + "** 踢出伺服器嗎？\n此操作無法復原！");

    std::string ts = std::to_string((uint64_t)tuid);
    std::string ps = std::to_string(page);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確認踢出").set_id("si_kcf_" + ts + "_" + ps)
        .set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("si_kno_" + ps)
        .set_style(dpp::cos_secondary));

    dpp::message m; m.add_embed(e); m.add_component(row);
    return m;
}

// ─── 持久化 ───────────────────────────────────────────────────────────────────

static const std::string SIGNIN_FILE = "signin.json";

static void save_signin() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        j["active"]     = g_signin.active;
        j["guild_id"]   = (uint64_t)g_signin.guild_id;
        j["channel_id"] = (uint64_t)g_signin.channel_id;
        j["message_id"] = (uint64_t)g_signin.message_id;
        j["deadline"]   = (int64_t)g_signin.deadline;
        nlohmann::json si = nlohmann::json::object();
        for (auto& [uid, name] : g_signin.signed_in)
            si[std::to_string((uint64_t)uid)] = name;
        nlohmann::json ns = nlohmann::json::object();
        for (auto& [uid, name] : g_signin.not_signed)
            ns[std::to_string((uint64_t)uid)] = name;
        j["signed_in"]  = si;
        j["not_signed"] = ns;
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(SIGNIN_FILE, j.dump(2));
}

static void load_signin() {
    std::ifstream f(SIGNIN_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        g_signin = SignInSession{};
        g_signin.active     = j.value("active",     false);
        g_signin.guild_id   = dpp::snowflake(j.value("guild_id",   (uint64_t)0));
        g_signin.channel_id = dpp::snowflake(j.value("channel_id", (uint64_t)0));
        g_signin.message_id = dpp::snowflake(j.value("message_id", (uint64_t)0));
        g_signin.deadline   = (time_t)j.value("deadline", (int64_t)0);
        if (j.contains("signed_in"))
            for (auto& [k, v] : j["signed_in"].items())
                g_signin.signed_in[dpp::snowflake(std::stoull(k))] = v.get<std::string>();
        if (j.contains("not_signed"))
            for (auto& [k, v] : j["not_signed"].items())
                g_signin.not_signed[dpp::snowflake(std::stoull(k))] = v.get<std::string>();
    } catch (...) {}
}
