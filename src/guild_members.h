#pragma once
#include <dpp/dpp.h>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <cstdio>

// ─── 各伺服器成員名單快取 ────────────────────────────────────────────────────
// 用途：富豪榜「本伺服器」篩選、限定收藏只給本家伺服器成員。
// bot 加入／連上伺服器時（on_guild_create）抓一次，之後每 10 分鐘全部重抓（main.cpp 的 timer）。
inline std::mutex                                   g_guild_members_mtx;
inline std::map<uint64_t, std::set<uint64_t>>       g_guild_members; // guild_id → 成員 uid
inline std::set<uint64_t>                           g_known_guilds;  // bot 所在的伺服器（on_guild_create 登記）

// 取得某伺服器的成員名單；還沒抓到回傳 false
static bool guild_members_get(dpp::snowflake guild_id, std::set<uint64_t>& out) {
    std::lock_guard<std::mutex> lk(g_guild_members_mtx);
    auto it = g_guild_members.find((uint64_t)guild_id);
    if (it == g_guild_members.end()) return false;
    out = it->second;
    return true;
}

// 是否為某伺服器成員；名單還沒抓到時回傳 unknown_default
static bool guild_has_member(dpp::snowflake guild_id, dpp::snowflake uid, bool unknown_default) {
    std::lock_guard<std::mutex> lk(g_guild_members_mtx);
    auto it = g_guild_members.find((uint64_t)guild_id);
    if (it == g_guild_members.end()) return unknown_default;
    return it->second.count((uint64_t)uid) > 0;
}

// 分頁抓完整成員名單（每次 1000 人），全部抓完才替換，抓失敗就保留舊名單
static void refresh_guild_members(dpp::cluster& bot, dpp::snowflake guild_id,
                                  std::shared_ptr<std::set<uint64_t>> acc = nullptr, uint64_t after = 0) {
    if (!acc) {
        acc = std::make_shared<std::set<uint64_t>>();
        std::lock_guard<std::mutex> lk(g_guild_members_mtx);
        g_known_guilds.insert((uint64_t)guild_id);
    }
    bot.guild_get_members(guild_id, 1000, after, [&bot, guild_id, acc](const dpp::confirmation_callback_t& cc) {
        if (cc.is_error()) {
            fprintf(stderr, "[guild_members] %llu 取得成員名單失敗：%s\n",
                    (unsigned long long)guild_id, cc.get_error().message.c_str()); fflush(stderr);
            return;
        }
        auto& gmap = std::get<dpp::guild_member_map>(cc.value);
        uint64_t last = 0;
        for (auto& [muid, gm] : gmap) { acc->insert((uint64_t)muid); last = std::max(last, (uint64_t)muid); }
        if (gmap.size() >= 1000) { refresh_guild_members(bot, guild_id, acc, last); return; }
        size_t n = acc->size();
        {
            std::lock_guard<std::mutex> lk(g_guild_members_mtx);
            g_guild_members[(uint64_t)guild_id] = std::move(*acc);
        }
        fprintf(stderr, "[guild_members] %llu 成員名單更新：%zu 人\n", (unsigned long long)guild_id, n); fflush(stderr);
    });
}

// 重抓所有 bot 所在的伺服器
static void refresh_all_guild_members(dpp::cluster& bot) {
    std::vector<uint64_t> ids;
    {
        std::lock_guard<std::mutex> lk(g_guild_members_mtx);
        ids.assign(g_known_guilds.begin(), g_known_guilds.end());
    }
    for (auto gid : ids) refresh_guild_members(bot, gid);
}
