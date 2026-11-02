#pragma once
#include "helpers.h"
#include "guild_members.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>

static const std::string WARN_FILE = "warnings.json";

// ─── Persistence ──────────────────────────────────────────────────────────────

static void load_warns() {
    std::ifstream f(WARN_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            if (v.is_number_integer()) {
                // Backward compat: old format was just a count
                int cnt = v.get<int>();
                for (int i = 0; i < cnt; i++)
                    warn_data[uid].push_back({0, ""});
            } else if (v.is_array()) {
                for (auto& rec : v) {
                    WarnRecord wr;
                    wr.timestamp = (time_t)rec.value("ts", (int64_t)0);
                    wr.reason    = rec.value("reason", std::string{});
                    warn_data[uid].push_back(wr);
                }
            }
        }
    } catch (...) {}
}

static void save_warns() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, records] : warn_data) {
            if (records.empty()) continue;
            nlohmann::json arr = nlohmann::json::array();
            for (auto& r : records)
                arr.push_back({{"ts", (int64_t)r.timestamp}, {"reason", r.reason}});
            j[std::to_string((uint64_t)uid)] = arr;
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(WARN_FILE, j.dump(2));
}

// ─── Parse @mention → snowflake ───────────────────────────────────────────────

static dpp::snowflake parse_mention(const std::string& s) {
    size_t at = s.find("<@");
    if (at == std::string::npos) return 0;
    size_t i = at + 2;
    if (i < s.size() && s[i] == '!') i++;
    size_t end = s.find('>', i);
    if (end == std::string::npos || end == i) return 0;
    try { return dpp::snowflake(std::stoull(s.substr(i, end - i))); }
    catch (...) { return 0; }
}

// ─── Warn command ─────────────────────────────────────────────────────────────

static dpp::message handle_warn(dpp::snowflake target_id,
                                 const std::string& target_name,
                                 const std::string& reason) {
    int count = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        WarnRecord wr;
        wr.timestamp = std::time(nullptr);
        wr.reason    = reason;
        warn_data[target_id].push_back(wr);
        count = (int)warn_data[target_id].size();
    }
    save_warns();

    dpp::embed e;
    e.set_title("⚠️  警告").set_color(0xE67E22);
    e.add_field("👤  對象", target_name,                       true);
    e.add_field("🔢  累計", std::to_string(count) + " 次",    true);
    if (!reason.empty()) e.add_field("📝  原因", reason, false);
    dpp::message msg; msg.add_embed(e);
    return msg;
}

// ─── Warn detail view ─────────────────────────────────────────────────────────

// scoped：從「本伺服器」榜單點進來的，返回時回到本伺服器榜單
static dpp::message handle_warn_detail(dpp::snowflake uid, bool scoped = false) {
    std::vector<WarnRecord> records;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = warn_data.find(uid);
        if (it != warn_data.end()) records = it->second;
    }

    dpp::embed e;
    e.set_title("📋  警告詳情").set_color(0xE67E22);
    e.set_description("<@" + std::to_string((uint64_t)uid) + "> 共 **" +
                      std::to_string(records.size()) + "** 次警告");

    int start = std::max(0, (int)records.size() - 10); // show latest 10
    for (int i = start; i < (int)records.size(); i++) {
        auto& r = records[i];
        std::string title = "第 " + std::to_string(i + 1) + " 次";
        std::string body;
        if (r.timestamp > 0) {
            struct tm tm_buf{}; localtime_s(&tm_buf, &r.timestamp);
            char buf[32]; strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M", &tm_buf);
            body += std::string(buf);
        } else {
            body += "（時間不明）";
        }
        if (!r.reason.empty()) body += "\n原因：" + r.reason;
        else                    body += "\n原因：（無）";
        e.add_field(title, body, true);
    }
    if (records.size() > 10)
        e.set_footer(dpp::embed_footer().set_text("僅顯示最近 10 筆，共 " + std::to_string(records.size()) + " 筆"));

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回榜單")
        .set_id(scoped ? "warn_boardg" : "warn_board").set_style(dpp::cos_secondary);
    row.add_component(back); msg.add_component(row);
    return msg;
}

// ─── Warn leaderboard ─────────────────────────────────────────────────────────

// scope_guild：0＝全部；非 0＝只列該伺服器成員。current_guild：目前所在伺服器（顯示切換按鈕用，私訊為 0）
static dpp::message handle_warn_board(dpp::snowflake scope_guild = 0, dpp::snowflake current_guild = 0) {
    std::set<uint64_t> members;
    bool members_ok = scope_guild == 0 || guild_members_get(scope_guild, members);
    std::vector<std::pair<int, dpp::snowflake>> sorted;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, records] : warn_data) {
            if (records.empty()) continue;
            if (scope_guild != 0 && !members.count((uint64_t)uid)) continue;
            sorted.push_back({(int)records.size(), uid});
        }
    }
    std::sort(sorted.begin(), sorted.end(), std::greater<>());

    // 切換「全部／本伺服器」
    auto add_scope_row = [&](dpp::message& msg) {
        if (current_guild == 0) return;
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🌐 全部")
            .set_id("warn_board").set_style(scope_guild == 0 ? dpp::cos_primary : dpp::cos_secondary)
            .set_disabled(scope_guild == 0));
        row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🏠 本伺服器")
            .set_id("warn_boardg").set_style(scope_guild != 0 ? dpp::cos_primary : dpp::cos_secondary)
            .set_disabled(scope_guild != 0));
        msg.add_component(row);
    };

    dpp::embed e;
    e.set_title(scope_guild != 0 ? "🏴  警告榜單（本伺服器）" : "🏴  警告榜單（全部）").set_color(0xC0392B);

    if (!members_ok) {
        e.set_description("⏳ 這個伺服器的成員名單還在讀取中，請稍後再試。");
        dpp::message msg; msg.add_embed(e); add_scope_row(msg); return msg;
    }
    if (sorted.empty()) {
        e.set_description("目前沒有警告紀錄。");
        dpp::message msg; msg.add_embed(e); add_scope_row(msg); return msg;
    }

    std::ostringstream oss;
    int show = std::min((int)sorted.size(), 10);
    for (int i = 0; i < show; i++)
        oss << (i + 1) << ". <@" << sorted[i].second << "> — **" << sorted[i].first << "** 次\n";
    e.set_description(oss.str());

    dpp::message msg; msg.add_embed(e);

    // Buttons: up to 2 rows × 5 = 10 entries
    int n = 0;
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    for (int i = 0; i < show; i++) {
        dpp::component btn;
        btn.set_type(dpp::cot_button)
           .set_label("第 " + std::to_string(i + 1) + " 名詳情")
           .set_id("warn_detail_" + std::to_string((uint64_t)sorted[i].second) + (scope_guild != 0 ? "_g" : ""))
           .set_style(dpp::cos_secondary);
        if (i < 5) row1.add_component(btn);
        else        row2.add_component(btn);
        n++;
    }
    msg.add_component(row1);
    if (n > 5) msg.add_component(row2);
    add_scope_row(msg);
    return msg;
}
