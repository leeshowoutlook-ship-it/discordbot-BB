#pragma once
#include "types.h"
#include "helpers.h"
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <functional>
#include <deque>
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
// 支援：M/D HH:MM[:SS]（指定日期時間）、HH:MM（今天幾點）、Xm（X分鐘後）、Xh（X小時後）；無法解析回傳 0

static time_t parse_si_deadline(const std::string& s) {
    if (s.empty()) return 0;
    time_t now = time(nullptr);

    // M/D HH:MM 或 M/D HH:MM:SS（例：9/14 23:59 或 9/14 23:59:59）
    {
        size_t sp = s.find(' ');
        size_t sl = s.find('/');
        if (sp != std::string::npos && sl != std::string::npos && sl < sp) {
            try {
                int mon = std::stoi(s.substr(0, sl));
                int day = std::stoi(s.substr(sl + 1, sp - sl - 1));
                std::string tpart = s.substr(sp + 1);
                int h = 0, m = 0, sec = 0;
                size_t c1 = tpart.find(':'), c2 = tpart.rfind(':');
                if (c1 == std::string::npos) return 0;
                h = std::stoi(tpart.substr(0, c1));
                if (c1 == c2) {
                    m = std::stoi(tpart.substr(c1 + 1));
                } else {
                    m   = std::stoi(tpart.substr(c1 + 1, c2 - c1 - 1));
                    sec = std::stoi(tpart.substr(c2 + 1));
                }
                if (mon < 1 || mon > 12 || day < 1 || day > 31) return 0;
                if (h < 0 || h > 23 || m < 0 || m > 59 || sec < 0 || sec > 59) return 0;
                struct tm lt{}; localtime_s(&lt, &now);
                lt.tm_mon = mon - 1; lt.tm_mday = day;
                lt.tm_hour = h; lt.tm_min = m; lt.tm_sec = sec;
                time_t t = mktime(&lt);
                if (t <= 0) return 0;
                return t;
            } catch (...) { return 0; }
        }
    }

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
    return "\n⏰ 截止時間：**<t:" + std::to_string((int64_t)g_signin.deadline) + ":t>**";
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
        j["unsigned_role_id"] = (uint64_t)g_signin.unsigned_role_id;
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
        g_signin.unsigned_role_id = dpp::snowflake(j.value("unsigned_role_id", (uint64_t)0));
        if (j.contains("signed_in"))
            for (auto& [k, v] : j["signed_in"].items())
                g_signin.signed_in[dpp::snowflake(std::stoull(k))] = v.get<std::string>();
        if (j.contains("not_signed"))
            for (auto& [k, v] : j["not_signed"].items())
                g_signin.not_signed[dpp::snowflake(std::stoull(k))] = v.get<std::string>();
    } catch (...) {}
}

// ─── 「未簽到人員」身分組 ─────────────────────────────────────────────────────
// 角色 id 建立一次後存進 g_signin.unsigned_role_id（存檔），之後直接重複使用，不用每次都搜尋。

static void ensure_unsigned_role(dpp::cluster& bot, dpp::snowflake gid, std::function<void(dpp::snowflake)> then) {
    dpp::snowflake existing;
    { std::lock_guard<std::mutex> lk(data_mutex); existing = g_signin.unsigned_role_id; }
    if (existing != 0) { then(existing); return; }
    dpp::role r;
    r.guild_id = gid;
    r.name     = "未簽到人員";
    r.colour   = 0xE74C3C;
    bot.role_create(r, [then](const dpp::confirmation_callback_t& cb) {
        if (cb.is_error()) {
            auto err = cb.get_error();
            std::ofstream lf("cmd_register_log.txt", std::ios::app);
            lf << "[" << time(nullptr) << "] role_create(未簽到人員) 失敗！HTTP "
               << cb.http_info.status << "，code=" << err.code
               << "，message=" << err.message
               << "，human_readable=" << err.human_readable << "\n";
            then(0); return;
        }
        dpp::snowflake rid = std::get<dpp::role>(cb.value).id;
        { std::lock_guard<std::mutex> lk(data_mutex); g_signin.unsigned_role_id = rid; }
        save_signin();
        then(rid);
    });
}

// 把「未簽到人員」身分組發給目前 not_signed 名單裡的每一個人（已經有的話 Discord 端會自動忽略）。
// 用 timer 一秒發一個（比之前的每秒3個更保守，因為那個速度實測還是會被 Discord rate limit 擋掉），
// 被限速（429）的話排回佇列尾端稍後重試，其他錯誤（例如權限不足）記一次log就跳過、不重試。
// done(count)：count >= 0 為排入處理的人數；count == -1 代表身分組建立/取得失敗（例如機器人缺少「管理身分組」權限）。
static void grant_unsigned_role_to_all(dpp::cluster& bot, dpp::snowflake gid, std::function<void(int)> done = nullptr) {
    ensure_unsigned_role(bot, gid, [&bot, gid, done](dpp::snowflake rid) {
        if (rid == 0) { if (done) done(-1); return; }
        auto queue = std::make_shared<std::deque<dpp::snowflake>>();
        { std::lock_guard<std::mutex> lk(data_mutex);
          for (auto& [uid, name] : g_signin.not_signed) queue->push_back(uid); }
        int total = (int)queue->size();
        if (total > 0) {
            bot.start_timer([&bot, gid, rid, queue](dpp::timer t) {
                if (queue->empty()) { bot.stop_timer(t); return; }
                dpp::snowflake uid = queue->front(); queue->pop_front();
                bot.guild_member_add_role(gid, uid, rid, [queue, uid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) return;
                    if (cb.http_info.status == 429) { queue->push_back(uid); return; } // 被限速，排回佇列尾端稍後重試
                    auto err = cb.get_error();
                    std::ofstream lf("cmd_register_log.txt", std::ios::app);
                    lf << "[" << time(nullptr) << "] guild_member_add_role 失敗！uid=" << (uint64_t)uid
                       << "，HTTP " << cb.http_info.status << "，message=" << err.message
                       << "，human_readable=" << err.human_readable << "\n";
                });
            }, 1);
        }
        if (done) done(total);
    });
}

// 開始新一場簽到前的確認視窗：會覆蓋掉上一場（不管是還在進行中還是已結束）的紀錄，先跟管理員確認一次。
static dpp::message make_si_start_confirm_msg(time_t deadline, bool was_active) {
    std::string warn = was_active
        ? "⚠️ **目前有正在進行的簽到！** 開啟新的簽到會結束並覆蓋掉目前的簽到紀錄，確定要這麼做嗎？"
        : "⚠️ 開啟新的簽到會覆蓋掉上一次的簽到紀錄，確定要開啟嗎？";
    if (deadline > 0) warn += "\n截止時間：<t:" + std::to_string((int64_t)deadline) + ":f>";
    dpp::message m; m.set_content(warn);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確定開啟").set_id("si_startok_" + std::to_string((int64_t)deadline)).set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("si_startno").set_style(dpp::cos_secondary));
    m.add_component(row);
    return m;
}

// 真正開始一場簽到（guild_get_members → 建立 g_signin → 發公告 → 設截止 timer）。
// on_success(total)：建立成功後回呼，帶未簽到總人數；on_error(msg)：取得成員列表失敗時回呼。
static void si_do_start(dpp::cluster& bot, dpp::snowflake gid, dpp::snowflake ch, time_t deadline,
                        std::function<void(int)> on_success, std::function<void(const std::string&)> on_error) {
    bot.guild_get_members(gid, 1000, 0, [&bot, ch, gid, deadline, on_success, on_error](const dpp::confirmation_callback_t& cc) {
        if (cc.is_error()) { if (on_error) on_error("❌ 無法取得伺服器成員列表！"); return; }
        auto& gmap = std::get<dpp::guild_member_map>(cc.value);
        int total;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            g_signin = SignInSession{};
            g_signin.active     = true;
            g_signin.guild_id   = gid;
            g_signin.channel_id = ch;
            g_signin.deadline   = deadline;
            for (auto& [muid, gm] : gmap) {
                const dpp::user* user = dpp::find_user(muid);
                if (user && user->is_bot()) continue;
                std::string name;
                if (!gm.get_nickname().empty()) {
                    name = gm.get_nickname();
                } else if (user) {
                    name = user->global_name.empty() ? user->username : user->global_name;
                } else {
                    name = "<@" + std::to_string((uint64_t)muid) + ">";
                }
                g_signin.not_signed[muid] = name;
            }
            total = (int)g_signin.not_signed.size();
        }
        grant_unsigned_role_to_all(bot, gid);
        dpp::message msg = make_si_start_msg(total);
        msg.channel_id = ch;
        bot.message_create(msg, [&bot, deadline](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                dpp::snowflake mid = std::get<dpp::message>(cb.value).id;
                { std::lock_guard<std::mutex> lk(data_mutex); g_signin.message_id = mid; }
                if (deadline > 0) {
                    long long secs = (long long)deadline - (long long)time(nullptr);
                    if (secs > 0) {
                        dpp::timer tid = bot.start_timer([&bot](dpp::timer t) {
                            dpp::snowflake m_id = 0, m_ch = 0;
                            dpp::message closed;
                            {
                                std::lock_guard<std::mutex> lk(data_mutex);
                                if (!g_signin.active) { bot.stop_timer(t); return; }
                                g_signin.active = false;
                                m_id = g_signin.message_id;
                                m_ch = g_signin.channel_id;
                                closed = make_si_closed_msg();
                            }
                            save_signin();
                            if (m_id != 0) { closed.id = m_id; closed.channel_id = m_ch; bot.message_edit(closed); }
                            bot.stop_timer(t);
                        }, (uint64_t)secs);
                        { std::lock_guard<std::mutex> lk(data_mutex); g_signin.timer_id = tid; }
                    }
                }
                save_signin();
            }
        });
        if (on_success) on_success(total);
    });
}

// 單一玩家簽到成功時拿掉「未簽到人員」身分組。
static void remove_unsigned_role(dpp::cluster& bot, dpp::snowflake gid, dpp::snowflake uid) {
    dpp::snowflake rid;
    { std::lock_guard<std::mutex> lk(data_mutex); rid = g_signin.unsigned_role_id; }
    if (rid != 0) bot.guild_member_remove_role(gid, uid, rid);
}
