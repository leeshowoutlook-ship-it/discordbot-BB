#pragma once
#include "types.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <filesystem>

// Atomic JSON save: write to .tmp then rename to avoid corruption on forced kill.
// Uses std::filesystem::rename which replaces existing files on Windows (unlike std::rename).
static inline void atomic_write(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    { std::ofstream f(tmp); if (f) f << data; else return; }
    std::filesystem::rename(tmp, path);
}

// ─── Config loader ────────────────────────────────────────────────────────────

static Config load_config() {
    Config c;
    std::ifstream f(".env");
    std::string line;
    while (std::getline(f, line)) {
        auto strip = [](std::string s) {
            while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
            return s;
        };
        auto parse = [&](const std::string& key, std::string& out) {
            if (line.rfind(key + "=", 0) == 0) out = strip(line.substr(key.size() + 1));
        };
        parse("BOT_TOKEN",          c.token);
        parse("NOTIFY_USER_ID",     c.notify_user_id);
        parse("IMG_NORMAL",         c.img_normal);
        parse("IMG_HARD",           c.img_hard);
        parse("IMG_FLAME",          c.img_flame);
        parse("MIN_BET_THREAD_ID",  c.min_bet_thread_id);
        parse("ALLIN_THREAD_ID",    c.allin_thread_id);
    }
    return c;
}

// ─── Boss metadata ────────────────────────────────────────────────────────────

static std::string get_boss_img(const std::string& boss) {
    if (boss == "普通拉圖斯") return cfg.img_normal;
    if (boss == "困難拉圖斯") return cfg.img_hard;
    if (boss == "殘暴炎魔")   return cfg.img_flame;
    return "";
}

static std::vector<std::string> get_positions(const std::string& boss) {
    if (boss == "普通拉圖斯") return {"法師", "火", "敏職輸出", "力職輸出"};
    if (boss == "殘暴炎魔")   return {"兩刀法師", "三刀法師", "火", "需要火的輸出", "不需要火的輸出"};
    if (boss == "困難拉圖斯") return {"主控法", "清球兩刀法", "主控清球都可以的法", "火", "時間副控", "力職輸出", "敏職輸出"};
    return {};
}

// ─── Game week (rolling, starts Thursday) ────────────────────────────────────
//
// Each weekday that has already passed today is advanced by 7 days so users
// always see upcoming dates.  Slots use date_label ("6/12(四)") as key.

static std::vector<std::pair<std::string,std::string>> get_game_week() {
    time_t now = time(nullptr);
    struct tm tm_now{};
    localtime_s(&tm_now, &now);

    struct tm today_cmp = tm_now;
    today_cmp.tm_hour = 0; today_cmp.tm_min = 0; today_cmp.tm_sec = 0;
    time_t today_t0 = mktime(&today_cmp);

    int days_since_thu = (tm_now.tm_wday - 4 + 7) % 7;
    time_t thu_t = now - (time_t)days_since_thu * 86400;

    const char* keys[] = {"四","五","六","日","一","二","三"};
    std::vector<std::pair<std::string,std::string>> result;
    for (int i = 0; i < 7; i++) {
        time_t day_t = thu_t + (time_t)i * 86400;
        struct tm dm{}; localtime_s(&dm, &day_t);

        struct tm day_cmp = dm;
        day_cmp.tm_hour = 0; day_cmp.tm_min = 0; day_cmp.tm_sec = 0;
        if (mktime(&day_cmp) < today_t0) {
            day_t += 7 * 86400;
            localtime_s(&dm, &day_t);
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%d/%d(%s)", dm.tm_mon + 1, dm.tm_mday, keys[i]);
        result.push_back({keys[i], buf});
    }
    return result;
}

// ─── Permission helpers ───────────────────────────────────────────────────────

static bool is_admin(const dpp::interaction& cmd) {
    for (auto& rid : cmd.member.get_roles()) {
        const dpp::role* r = dpp::find_role(rid);
        if (r && (r->name == "管理員" || r->name.find("副會長") != std::string::npos))
            return true;
    }
    return false;
}

// Checks if uid or their roles qualify to run giveaway/admin commands.
static bool is_draw_authorized(const dpp::interaction& cmd) {
    dpp::snowflake uid = cmd.member.user_id;
    if (!cfg.notify_user_id.empty() && std::to_string(uid) == cfg.notify_user_id)
        return true;
    return is_admin(cmd);
}

// Variant for message-command context (roles may be empty if not cached).
static bool is_draw_authorized_msg(dpp::snowflake uid,
                                    const std::vector<dpp::snowflake>& roles) {
    if (!cfg.notify_user_id.empty() && std::to_string(uid) == cfg.notify_user_id)
        return true;
    for (auto& rid : roles) {
        const dpp::role* r = dpp::find_role(rid);
        if (r && (r->name == "管理員" || r->name.find("副會長") != std::string::npos))
            return true;
    }
    return false;
}

// Returns true if uid owns the page message (or no tracked owner).
static bool page_is_mine(dpp::snowflake msg_id, dpp::snowflake uid) {
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = msg_owner.find(msg_id);
    return it == msg_owner.end() || it->second == uid;
}

// Returns true if uid owns the message (or no tracked owner). Sends ephemeral error otherwise.
static bool check_owner(const dpp::interaction_create_t& ev, dpp::snowflake uid) {
    dpp::snowflake msg_id = ev.command.message_id;
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = msg_owner.find(msg_id);
    if (it != msg_owner.end() && it->second != uid) {
        ev.reply(dpp::ir_channel_message_with_source,
            dpp::message("❌ 這不是你的操作！").set_flags(dpp::m_ephemeral));
        return false;
    }
    return true;
}

// ─── Duration parsing ─────────────────────────────────────────────────────────

// Parses "5h", "30m", "90s" and returns seconds.  Defaults to 1 hour.
static int parse_duration(const std::string& s) {
    if (s.empty()) return 3600;
    int val = 0;
    char unit = 'h';
    sscanf_s(s.c_str(), "%d%c", &val, &unit, 1);
    if (val <= 0) return 3600;
    switch (unit) {
        case 'm': case 'M': return val * 60;
        case 's': case 'S': return val;
        default:            return val * 3600;
    }
}

// ─── Embed helpers ────────────────────────────────────────────────────────────

static dpp::embed base_embed(const std::string& title, const std::string& desc,
                              uint32_t color, const dpp::user& user,
                              const std::string& img_url = "") {
    dpp::embed e;
    e.set_title(title);
    e.set_description("<@" + std::to_string(user.id) + "> " + desc);
    e.set_color(color);
    e.set_footer(dpp::embed_footer().set_text("王團報名系統"));
    if (!img_url.empty()) e.set_thumbnail(img_url);
    return e;
}

static dpp::message make_expired_msg() {
    dpp::embed e;
    e.set_title("⚠️  操作已失效").set_color(0x808080);
    e.set_description("你已開始新的指令，此操作已關閉。");
    dpp::message msg;
    msg.add_embed(e);
    return msg;
}

// Invalidates the user's current active message (edits it to expired state).
static void invalidate_old_msg(dpp::cluster& bot, dpp::snowflake uid) {
    ActiveMsg old{};
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = user_active_msg.find(uid);
        if (it != user_active_msg.end()) { old = it->second; user_active_msg.erase(it); }
    }
    if (old.msg_id) {
        dpp::message expired = make_expired_msg();
        expired.id = old.msg_id;
        expired.channel_id = old.channel_id;
        bot.message_edit(expired);
        std::lock_guard<std::mutex> lk(data_mutex);
        msg_owner.erase(old.msg_id);
    }
}

// ─── Slot formatting ──────────────────────────────────────────────────────────

// e.g. "**6/12(五)** · 18:00 · 20:00\n**6/13(六)** · 12:00"
static std::string format_slots(const std::vector<std::pair<std::string,std::string>>& slots) {
    auto week = get_game_week();
    std::ostringstream oss; bool first = true;
    for (auto& [k, label] : week) {
        std::vector<std::string> ts;
        for (auto& [d, t] : slots)
            if (d == label) ts.push_back(t);
        if (ts.empty()) continue;
        if (!first) oss << "\n";
        std::sort(ts.begin(), ts.end());
        oss << "**" << label << "**";
        for (auto& t : ts) oss << " · " << t;
        first = false;
    }
    return first ? "（尚未選擇）" : oss.str();
}

// ─── Expired data cleanup ─────────────────────────────────────────────────────

static bool is_date_past(const std::string& date_label) {
    time_t now = time(nullptr);
    struct tm tm_now{};
    localtime_s(&tm_now, &now);
    int mon, day;
    if (sscanf_s(date_label.c_str(), "%d/%d", &mon, &day) != 2) return false;
    struct tm today_cmp = tm_now;
    today_cmp.tm_hour = 0; today_cmp.tm_min = 0; today_cmp.tm_sec = 0;
    time_t today_t0 = mktime(&today_cmp);
    struct tm dm = tm_now;
    dm.tm_mon = mon - 1; dm.tm_mday = day;
    dm.tm_hour = 0; dm.tm_min = 0; dm.tm_sec = 0;
    return mktime(&dm) < today_t0;
}

static void cleanup_expired() {
    std::lock_guard<std::mutex> lk(data_mutex);
    registrations.erase(
        std::remove_if(registrations.begin(), registrations.end(),
            [](const Registration& r) {
                if (r.slots.empty()) return true;
                for (auto& [d, t] : r.slots)
                    if (!is_date_past(d)) return false;
                return true;
            }),
        registrations.end()
    );
    for (auto it = proposed_teams.begin(); it != proposed_teams.end(); ) {
        if (is_date_past(it->second.day)) {
            proposed_slots.erase({it->second.boss, it->second.day, it->second.time_slot});
            it = proposed_teams.erase(it);
        } else ++it;
    }
}
