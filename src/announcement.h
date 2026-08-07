#pragma once
#include "types.h"
#include "helpers.h"
#include <nlohmann/json.hpp>
#include <fstream>

// ─── 大廳公告：管理員可編輯，大廳訊息一定會顯示最新一則 ───────────────────────

static const std::string ANNOUNCEMENT_FILE = "announcement.json";
static const size_t      ANNOUNCEMENT_MAX_LEN = 300;

static void save_announcement() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        j = {
            {"text",       g_announcement.text},
            {"updated_by", g_announcement.updated_by},
            {"updated_at", (int64_t)g_announcement.updated_at},
        };
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(ANNOUNCEMENT_FILE, j.dump(2));
}

static void load_announcement() {
    std::ifstream f(ANNOUNCEMENT_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        g_announcement.text       = j.value("text", "");
        g_announcement.updated_by = j.value("updated_by", "");
        g_announcement.updated_at = (time_t)j.value("updated_at", (int64_t)0);
    } catch (...) {}
}

// 呼叫前不可持有 data_mutex
static std::string set_announcement(const std::string& text, const std::string& by) {
    if (text.size() > ANNOUNCEMENT_MAX_LEN)
        return "❌ 公告內容過長（上限 " + std::to_string(ANNOUNCEMENT_MAX_LEN) + " 字）！";
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        g_announcement.text       = text;
        g_announcement.updated_by = by;
        g_announcement.updated_at = time(nullptr);
    }
    save_announcement();
    return "✅ 公告已更新，大廳會顯示這則內容。";
}

// 大廳用：組出「📢 最新更新」那一行文字，沒有公告時回傳空字串
static std::string announcement_lobby_line() {
    std::lock_guard<std::mutex> lk(data_mutex);
    if (g_announcement.text.empty()) return "";
    return "📢 **最新更新**：" + g_announcement.text + "\n\n";
}

// !公告／/公告 查看用（含更新者與時間）
static dpp::message make_announcement_view_msg() {
    Announcement a;
    { std::lock_guard<std::mutex> lk(data_mutex); a = g_announcement; }
    dpp::embed e;
    e.set_title("📢  最新更新").set_color(0x5865F2);
    if (a.text.empty()) {
        e.set_description("目前沒有公告內容。");
    } else {
        e.set_description(a.text);
        if (!a.updated_by.empty()) {
            char buf[32]; struct tm tm_{}; localtime_s(&tm_, &a.updated_at);
            snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d", tm_.tm_mon + 1, tm_.tm_mday, tm_.tm_hour, tm_.tm_min);
            e.set_footer(dpp::embed_footer().set_text("由 " + a.updated_by + " 更新於 " + std::string(buf)));
        }
    }
    dpp::message m; m.add_embed(e);
    return m;
}
