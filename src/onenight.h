#pragma once
#include "types.h"
#include "chips.h"
#include "onwstats.h"
#include <random>
#include <algorithm>
#include <sstream>

// ─── Constants ────────────────────────────────────────────────────────────────

static const std::vector<std::string> ONW_ROLE_ORDER = {
    "狼人","頭狼","皮革匠","預言家","強盜","搗蛋鬼","女巫","村子白痴","酒鬼","失眠者","村民"
};
static const std::map<std::string,std::string> ONW_EMOJI = {
    {"狼人","🐺"},{"頭狼","🐾"},{"皮革匠","🩱"},{"預言家","🔮"},{"強盜","🗡️"},
    {"搗蛋鬼","😈"},{"女巫","🧙"},{"酒鬼","🍺"},{"村子白痴","🃏"},{"失眠者","😴"},{"村民","🏘️"}
};

static bool is_wolf_role(const std::string& r) { return r == "狼人" || r == "頭狼"; }
static const int ONW_REWARD_WIN  = 100;
static const int ONW_REWARD_LOSE = 30;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static std::mt19937& onw_rng() {
    static std::mt19937 r(std::random_device{}());
    return r;
}
static ONWPlayer* onw_find(ONWGame& g, dpp::snowflake uid) {
    for (auto& p : g.players) if (p.uid == uid) return &p;
    return nullptr;
}
static int onw_count_original(const ONWGame& g, const std::string& role) {
    int n = 0;
    for (auto& p : g.players) if (p.original_role == role) n++;
    return n;
}
static std::string onw_emoji(const std::string& role) {
    auto it = ONW_EMOJI.find(role);
    return it != ONW_EMOJI.end() ? it->second : "❓";
}

// ─── Forward declarations ─────────────────────────────────────────────────────

static void onw_start_wolves(dpp::cluster&, uint64_t);
static void onw_start_seer(dpp::cluster&, uint64_t);
static void onw_start_robber(dpp::cluster&, uint64_t);
static void onw_start_troublemaker(dpp::cluster&, uint64_t);
static void onw_start_village_idiot(dpp::cluster&, uint64_t);
static void onw_start_witch(dpp::cluster&, uint64_t);
static void onw_start_drunk(dpp::cluster&, uint64_t);
static void onw_start_insomniac(dpp::cluster&, uint64_t);
static void onw_start_day(dpp::cluster&, uint64_t);
static void onw_resolve_vote(dpp::cluster&, uint64_t);

// ─── Lobby Message ────────────────────────────────────────────────────────────

static dpp::message make_onw_lobby_msg(const ONWGame& g) {
    int pc = (int)g.players.size();
    int total = 0;
    for (auto& [r,c] : g.role_counts) total += c;
    int needed = pc + 3;
    std::string gid = std::to_string(g.id);

    dpp::embed e;
    e.set_title("🌙  一夜狼人 — 等待中").set_color(0x2C3E50);

    std::string plist;
    for (int i = 0; i < pc; i++)
        plist += std::to_string(i+1) + ". " + g.players[i].display_name + "\n";
    if (plist.empty()) plist = "尚無玩家";
    e.add_field("👥 玩家（" + std::to_string(pc) + "/8）", plist, true);

    std::string rlist;
    for (auto& role : ONW_ROLE_ORDER) {
        int cnt = g.role_counts.count(role) ? g.role_counts.at(role) : 0;
        if (role == "狼人" || role == "村民")
            rlist += onw_emoji(role) + " " + role + " ×" + std::to_string(cnt) + "\n";
        else
            rlist += (cnt ? "✅ " : "☐ ") + onw_emoji(role) + " " + role + "\n";
    }
    e.add_field("🃏 角色池（共 " + std::to_string(total) + " 張）", rlist, true);

    std::string status;
    if (pc < 3)
        status = "⚠️ 至少需要 3 位玩家";
    else if (total != needed)
        status = "⚠️ 角色數需為 " + std::to_string(needed) + "（" + std::to_string(pc) + " 人 + 3 中央）目前 " + std::to_string(total) + " 張";
    else
        status = "✅ 可以開始（共 " + std::to_string(needed) + " 張牌）";
    e.add_field("狀態", status, false);
    e.set_footer(dpp::embed_footer().set_text("皮革匠被投死時獨自獲勝 | 狼人若全在中央，村民平票獲勝"));

    bool can_start = (pc >= 3 && total == needed);

    dpp::message msg; msg.add_embed(e);

    // Row 1: 狼人 and 村民 +/-
    {
        int cw = g.role_counts.count("狼人") ? g.role_counts.at("狼人") : 0;
        int cv = g.role_counts.count("村民") ? g.role_counts.at("村民") : 0;
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🐺狼人("+std::to_string(cw)+") −")
            .set_id("onw_dec_"+gid+"_狼人").set_style(dpp::cos_secondary).set_disabled(cw==0));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🐺狼人 +").set_id("onw_inc_"+gid+"_狼人").set_style(dpp::cos_secondary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏘️村民("+std::to_string(cv)+") −")
            .set_id("onw_dec_"+gid+"_村民").set_style(dpp::cos_secondary).set_disabled(cv==0));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏘️村民 +").set_id("onw_inc_"+gid+"_村民").set_style(dpp::cos_secondary));
        msg.add_component(row);
    }

    // Rows 2-3: toggle buttons for all other roles (night order, 5 per row)
    static const std::vector<std::string> tog_roles = {
        "頭狼","皮革匠","預言家","強盜","搗蛋鬼",
        "女巫","村子白痴","酒鬼","失眠者"
    };
    for (int i = 0; i < (int)tog_roles.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)tog_roles.size(), i+5); j++) {
            const auto& role = tog_roles[j];
            int ct = g.role_counts.count(role) ? g.role_counts.at(role) : 0;
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(onw_emoji(role)+" "+role)
                .set_id("onw_tog_"+gid+"_"+role)
                .set_style(ct ? dpp::cos_success : dpp::cos_secondary));
        }
        msg.add_component(row);
    }

    // Row 4: action buttons
    dpp::component row5; row5.set_type(dpp::cot_action_row);
    row5.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✋ 加入").set_id("onw_join_"+gid).set_style(dpp::cos_success));
    row5.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚪 離開").set_id("onw_leave_"+gid).set_style(dpp::cos_secondary));
    row5.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🌙 開始遊戲").set_id("onw_start_"+gid)
        .set_style(dpp::cos_danger).set_disabled(!can_start));
    row5.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💥 解散遊戲").set_id("onw_dissolve_"+gid)
        .set_style(dpp::cos_secondary));
    msg.add_component(row5);

    return msg;
}

// ─── Vote Message ─────────────────────────────────────────────────────────────

static dpp::message make_onw_vote_msg(const ONWGame& g) {
    std::string gid = std::to_string(g.id);
    int voted = 0;
    for (auto& p : g.players) if (p.vote_target != 0) voted++;

    dpp::embed e;
    e.set_title("☀️  投票 — 誰是狼人？").set_color(0xF39C12);
    e.set_description("每人投一票，票數最多的人出局。\n所有人投完或主持人強制結算後公布結果。");

    std::map<dpp::snowflake, std::string> uid_name;
    for (auto& p : g.players) uid_name[p.uid] = p.display_name;

    std::string status;
    for (auto& p : g.players) {
        bool has_voted = (p.vote_target != 0);
        status += (has_voted ? "✅ " : "⏳ ") + p.display_name;
        if (has_voted)
            status += " → **" + (uid_name.count(p.vote_target) ? uid_name.at(p.vote_target) : "?") + "**";
        status += "\n";
    }
    e.add_field("投票狀況（" + std::to_string(voted) + "/" + std::to_string((int)g.players.size()) + "）", status, false);
    e.set_footer(dpp::embed_footer().set_text("可投自己 | 同票則無人出局"));

    dpp::message msg; msg.add_embed(e);

    // Player vote buttons (up to 5 per row)
    for (int i = 0; i < (int)g.players.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)g.players.size(), i+5); j++) {
            auto& p = g.players[j];
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(j+1)+". "+p.display_name)
                .set_id("onw_vote_"+gid+"_"+std::to_string((uint64_t)p.uid))
                .set_style(dpp::cos_danger));
        }
        msg.add_component(row);
    }

    dpp::component ctrl; ctrl.set_type(dpp::cot_action_row);
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 強制結算（主持人）").set_id("onw_vote_resolve_"+gid)
        .set_style(dpp::cos_secondary));
    msg.add_component(ctrl);

    return msg;
}

// ─── Role Assignment ──────────────────────────────────────────────────────────

static void onw_assign_roles(ONWGame& g) {
    std::vector<std::string> deck;
    for (auto& role : ONW_ROLE_ORDER) {
        int cnt = g.role_counts.count(role) ? g.role_counts.at(role) : 0;
        for (int i = 0; i < cnt; i++) deck.push_back(role);
    }
    std::shuffle(deck.begin(), deck.end(), onw_rng());
    for (int i = 0; i < (int)g.players.size(); i++) {
        g.players[i].original_role = deck[i];
        g.players[i].current_role  = deck[i];
    }
    for (int i = 0; i < 3; i++)
        g.center[i] = deck[(int)g.players.size() + i];
}

// ─── DM Helpers ───────────────────────────────────────────────────────────────

// Build player pick buttons for a DM (exclude self if self_uid != 0)
static dpp::message onw_pick_player_msg(
    const ONWGame& g, const std::string& title, const std::string& desc,
    const std::string& btn_prefix, dpp::snowflake self_uid = 0,
    bool add_skip = true, const std::string& skip_id = "")
{
    dpp::embed e;
    e.set_title(title).set_color(0x5865F2);
    e.set_description(desc);
    dpp::message msg; msg.add_embed(e);

    for (int i = 0; i < (int)g.players.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)g.players.size(), i+5); j++) {
            auto& p = g.players[j];
            if (p.uid == self_uid) continue;
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(j+1)+". "+p.display_name)
                .set_id(btn_prefix+std::to_string((uint64_t)p.uid))
                .set_style(dpp::cos_primary));
        }
        if (!row.components.empty()) msg.add_component(row);
    }
    if (add_skip) {
        dpp::component skip_row; skip_row.set_type(dpp::cot_action_row);
        skip_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("略過").set_id(skip_id.empty() ? btn_prefix+"skip" : skip_id)
            .set_style(dpp::cos_secondary));
        msg.add_component(skip_row);
    }
    return msg;
}

// ─── Night: Wolves ────────────────────────────────────────────────────────────

static void onw_start_wolves(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    std::vector<ONWPlayer> wolves;     // 所有狼（狼人＋頭狼）
    std::vector<ONWPlayer> reg_wolves; // 只有 狼人
    std::vector<ONWPlayer> all_players;
    ONWPlayer alpha_wolf;
    bool has_alpha = false, has_center_wolf = false;
    std::string gs = std::to_string(gid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::NIGHT_WOLVES;
        ch = g.channel_id;
        all_players = g.players;
        for (auto& p : g.players) {
            if (p.original_role == "狼人")      { wolves.push_back(p); reg_wolves.push_back(p); }
            else if (p.original_role == "頭狼") { wolves.push_back(p); alpha_wolf = p; has_alpha = true; }
        }
        for (auto& c : g.center) if (c == "狼人") { has_center_wolf = true; break; }
        // wolf_done: 若無一般狼人則預設完成
        g.wolf_done = reg_wolves.empty();
        // alpha_done: 若無頭狼則預設完成
        g.alpha_done = !has_alpha;
    }

    if (wolves.empty()) {
        { std::lock_guard<std::mutex> lk(data_mutex);
          onw_games[gid].wolf_done = true;
          onw_games[gid].alpha_done = true; }
        int delay = 3 + (int)(onw_rng()() % 5);
        bot.start_timer([&bot, gid](dpp::timer t){ bot.stop_timer(t); onw_start_seer(bot, gid); }, delay);
        return;
    }

    // ── DM 一般狼人 ──
    if (wolves.size() == 1 && !has_alpha) {
        // 孤狼：可偷看中央牌
        auto& wolf = reg_wolves[0];
        dpp::embed e;
        e.set_title("🐺 你是唯一的狼人！").set_color(0x8B0000);
        e.set_description("你可以偷看一張中央牌，或選擇略過。");
        dpp::message dm; dm.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int i = 0; i < 3; i++)
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("中央牌 " + std::to_string(i+1))
                .set_id("onw_wolf_peek_"+gs+"_"+std::to_string(i))
                .set_style(dpp::cos_primary));
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("略過").set_id("onw_wolf_skip_"+gs).set_style(dpp::cos_secondary));
        dm.add_component(row);
        bot.direct_message_create(wolf.uid, dm);
    } else {
        // 多狼（含頭狼在內）：各自確認
        for (auto& wolf : reg_wolves) {
            std::string others;
            for (auto& w2 : wolves)
                if (w2.uid != wolf.uid) others += "• **" + w2.display_name + "**\n";
            dpp::embed e;
            e.set_title("🐺 你的狼人同伴").set_color(0x8B0000);
            e.set_description(others + "\n確認後即可讓其他人繼續。");
            dpp::message dm; dm.add_embed(e);
            dpp::component row; row.set_type(dpp::cot_action_row);
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("✅ 確認").set_id("onw_wolf_skip_"+gs).set_style(dpp::cos_secondary));
            dm.add_component(row);
            bot.direct_message_create(wolf.uid, dm);
        }
    }

    // ── DM 頭狼 ──
    if (has_alpha) {
        std::string others;
        for (auto& w : reg_wolves) others += "• **" + w.display_name + "**（狼人）\n";
        if (!others.empty()) others += "\n";

        dpp::embed e; e.set_title("🐾 你是頭狼！").set_color(0x8B0000);
        dpp::message dm;

        if (has_center_wolf) {
            e.set_description(others + "中央有狼人牌，你可以把它換給一名玩家讓他成為狼人（或略過）。");
            dm.add_embed(e);
            // 排除所有狼的玩家按鈕
            std::set<dpp::snowflake> wolf_uids;
            for (auto& w : wolves) wolf_uids.insert(w.uid);
            for (int i = 0; i < (int)all_players.size(); i += 5) {
                dpp::component row; row.set_type(dpp::cot_action_row);
                for (int j = i; j < std::min((int)all_players.size(), i+5); j++) {
                    if (wolf_uids.count(all_players[j].uid)) continue;
                    row.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label(std::to_string(j+1)+". "+all_players[j].display_name)
                        .set_id("onw_alpha_inf_"+gs+"_"+std::to_string((uint64_t)all_players[j].uid))
                        .set_style(dpp::cos_danger));
                }
                if (!row.components.empty()) dm.add_component(row);
            }
        } else {
            e.set_description(others + "中央沒有狼人牌，無法感染玩家。");
            dm.add_embed(e);
        }
        dpp::component skip_row; skip_row.set_type(dpp::cot_action_row);
        skip_row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(has_center_wolf ? "略過（不感染）" : "✅ 確認")
            .set_id("onw_alpha_skip_"+gs).set_style(dpp::cos_secondary));
        dm.add_component(skip_row);
        bot.direct_message_create(alpha_wolf.uid, dm);
    }
}

// ─── Night: Seer ─────────────────────────────────────────────────────────────

static void onw_start_seer(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, seer_uid = 0;
    std::string gs = std::to_string(gid);
    const ONWGame* gptr = nullptr;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::NIGHT_SEER;
        ch = g.channel_id;
        for (auto& p : g.players)
            if (p.original_role == "預言家") { seer_uid = p.uid; break; }
    }

    if (!seer_uid) {
        { std::lock_guard<std::mutex> lk(data_mutex); onw_games[gid].seer_done = true; }
        int delay = 3 + (int)(onw_rng()() % 5);
        bot.start_timer([&bot, gid](dpp::timer t){ bot.stop_timer(t); onw_start_robber(bot, gid); }, delay);
        return;
    }

    // Build seer DM with two options: look at a player OR look at two center cards
    ONWGame g_copy;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        g_copy = onw_games[gid];
    }

    dpp::message dm = onw_pick_player_msg(
        g_copy,
        "🔮 預言家 — 查驗",
        "選擇查看一名玩家的身份，或查看兩張中央牌。",
        "onw_seer_p_"+gs+"_",
        seer_uid, false
    );
    // Add center peek button
    dpp::component crow; crow.set_type(dpp::cot_action_row);
    crow.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🃏 查看兩張中央牌").set_id("onw_seer_center_"+gs)
        .set_style(dpp::cos_primary));
    crow.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("略過").set_id("onw_seer_skip_"+gs).set_style(dpp::cos_secondary));
    dm.add_component(crow);
    bot.direct_message_create(seer_uid, dm);
}

// ─── Night: Robber ────────────────────────────────────────────────────────────

static void onw_start_robber(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, robber_uid = 0;
    std::string gs = std::to_string(gid);
    ONWGame g_copy;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::NIGHT_ROBBER;
        ch = g.channel_id;
        for (auto& p : g.players)
            if (p.original_role == "強盜") { robber_uid = p.uid; break; }
        g_copy = g;
    }

    if (!robber_uid) {
        { std::lock_guard<std::mutex> lk(data_mutex); onw_games[gid].robber_done = true; }
        int delay = 3 + (int)(onw_rng()() % 5);
        bot.start_timer([&bot, gid](dpp::timer t){ bot.stop_timer(t); onw_start_troublemaker(bot, gid); }, delay);
        return;
    }

    dpp::message dm = onw_pick_player_msg(
        g_copy,
        "🗡️ 強盜 — 交換",
        "選擇一名玩家，與其交換身份牌（你會看到新牌）。\n略過則保持原本身份。",
        "onw_robber_"+gs+"_",
        robber_uid, true, "onw_robber_skip_"+gs
    );
    bot.direct_message_create(robber_uid, dm);
}

// ─── Night: Troublemaker ──────────────────────────────────────────────────────

static void onw_start_troublemaker(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, tm_uid = 0;
    std::string gs = std::to_string(gid);
    ONWGame g_copy;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::NIGHT_TROUBLEMAKER;
        g.tm_first = 0;
        ch = g.channel_id;
        for (auto& p : g.players)
            if (p.original_role == "搗蛋鬼") { tm_uid = p.uid; break; }
        g_copy = g;
    }

    if (!tm_uid) {
        { std::lock_guard<std::mutex> lk(data_mutex); onw_games[gid].troublemaker_done = true; }
        int delay = 3 + (int)(onw_rng()() % 5);
        bot.start_timer([&bot, gid](dpp::timer t){ bot.stop_timer(t); onw_start_witch(bot, gid); }, delay);
        return;
    }

    dpp::message dm = onw_pick_player_msg(
        g_copy,
        "😈 搗蛋鬼 — 第一個玩家",
        "選擇**第一名**要互換的玩家（兩人互換，你不會看到牌）。\n略過則不做任何事。",
        "onw_tm1_"+gs+"_",
        tm_uid, true, "onw_tm_skip_"+gs
    );
    bot.direct_message_create(tm_uid, dm);
}

// ─── Night: Village Idiot ─────────────────────────────────────────────────────

static void onw_start_village_idiot(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake vi_uid = 0;
    std::vector<ONWPlayer> all_players;
    std::string gs = std::to_string(gid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::NIGHT_VILLAGE_IDIOT;
        all_players = g.players;
        for (auto& p : g.players)
            if (p.original_role == "村子白痴") { vi_uid = p.uid; break; }
    }
    if (!vi_uid) {
        { std::lock_guard<std::mutex> lk(data_mutex); onw_games[gid].vi_done = true; }
        int delay = 3 + (int)(onw_rng()() % 5);
        bot.start_timer([&bot, gid](dpp::timer t){ bot.stop_timer(t); onw_start_drunk(bot, gid); }, delay);
        return;
    }
    // 建立座位清單
    std::string seat_list;
    for (int i = 0; i < (int)all_players.size(); i++)
        seat_list += std::to_string(i+1) + ". " + all_players[i].display_name + "\n";
    dpp::embed e;
    e.set_title("🃏 村子白痴 — 移動所有人的牌").set_color(0x3498DB);
    e.set_description("選擇方向，所有玩家的牌會循環移動一格。\n你的牌（原始身份）會公開在桌上。\n\n**座位順序：**\n" + seat_list);
    dpp::message dm; dm.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⬅ 左移（2號牌→1號）").set_id("onw_vi_left_"+gs).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("➡ 右移（1號牌→2號）").set_id("onw_vi_right_"+gs).set_style(dpp::cos_primary));
    dm.add_component(row);
    bot.direct_message_create(vi_uid, dm);
}

// ─── Night: Witch ─────────────────────────────────────────────────────────────

static void onw_start_witch(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake witch_uid = 0;
    std::string gs = std::to_string(gid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::NIGHT_WITCH;
        for (auto& p : g.players)
            if (p.original_role == "女巫") { witch_uid = p.uid; break; }
    }
    if (!witch_uid) {
        { std::lock_guard<std::mutex> lk(data_mutex); onw_games[gid].witch_done = true; }
        int delay = 3 + (int)(onw_rng()() % 5);
        bot.start_timer([&bot, gid](dpp::timer t){ bot.stop_timer(t); onw_start_village_idiot(bot, gid); }, delay);
        return;
    }
    dpp::embed e;
    e.set_title("🧙 女巫 — 偷看中央牌").set_color(0x9B59B6);
    e.set_description("選擇一張中央牌偷看，之後可以決定要不要把它換給某人。");
    dpp::message dm; dm.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (int i = 0; i < 3; i++)
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("中央牌 " + std::to_string(i+1))
            .set_id("onw_witch_c_"+gs+"_"+std::to_string(i))
            .set_style(dpp::cos_primary));
    dm.add_component(row);
    bot.direct_message_create(witch_uid, dm);
}

// ─── Night: Drunk ─────────────────────────────────────────────────────────────

static void onw_start_drunk(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, drunk_uid = 0;
    std::string gs = std::to_string(gid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::NIGHT_DRUNK;
        ch = g.channel_id;
        for (auto& p : g.players)
            if (p.original_role == "酒鬼") { drunk_uid = p.uid; break; }
    }

    if (!drunk_uid) {
        { std::lock_guard<std::mutex> lk(data_mutex); onw_games[gid].drunk_done = true; }
        int delay = 3 + (int)(onw_rng()() % 5);
        bot.start_timer([&bot, gid](dpp::timer t){ bot.stop_timer(t); onw_start_insomniac(bot, gid); }, delay);
        return;
    }

    dpp::embed e;
    e.set_title("🍺 酒鬼 — 交換中央牌").set_color(0xF39C12);
    e.set_description("選擇一張中央牌與自己的身份牌交換。\n**你不會看到換來的是什麼。**");
    dpp::message dm; dm.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (int i = 0; i < 3; i++)
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("中央牌 " + std::to_string(i+1))
            .set_id("onw_drunk_"+gs+"_"+std::to_string(i))
            .set_style(dpp::cos_primary));
    dm.add_component(row);
    bot.direct_message_create(drunk_uid, dm);
}

// ─── Night: Insomniac ─────────────────────────────────────────────────────────

static void onw_start_insomniac(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, ins_uid = 0;
    std::string current_role;
    std::string gs = std::to_string(gid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::NIGHT_INSOMNIAC;
        ch = g.channel_id;
        for (auto& p : g.players)
            if (p.original_role == "失眠者") {
                ins_uid = p.uid;
                current_role = p.current_role;
                break;
            }
    }

    if (!ins_uid) {
        { std::lock_guard<std::mutex> lk(data_mutex); onw_games[gid].insomniac_done = true; }
        int delay = 3 + (int)(onw_rng()() % 5);
        bot.start_timer([&bot, gid](dpp::timer t){ bot.stop_timer(t); onw_start_day(bot, gid); }, delay);
        return;
    }

    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& g = onw_games[gid];
        auto* ip = onw_find(g, ins_uid);
        if (ip) changed = (ip->original_role != ip->current_role);
    }

    dpp::embed e;
    e.set_title("😴 失眠者 — 查看當下身份").set_color(0x9B59B6);
    if (changed)
        e.set_description("你的身份在夜晚被換掉了！\n你現在的身份是：" + onw_emoji(current_role) + " **" + current_role + "**");
    else
        e.set_description("你的身份沒有改變。\n你現在仍是：" + onw_emoji(current_role) + " **" + current_role + "**");
    dpp::message dm; dm.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確認").set_id("onw_insomniac_ok_"+gs).set_style(dpp::cos_secondary));
    dm.add_component(row);
    bot.direct_message_create(ins_uid, dm);
}

// ─── Day Phase ────────────────────────────────────────────────────────────────

static void onw_start_day(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    std::string gs = std::to_string(gid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::DAY_DISCUSS;
        ch = g.channel_id;
    }

    dpp::embed e;
    e.set_title("☀️  天亮了！").set_color(0xF1C40F);
    e.set_description("請開始討論，找出狼人在哪裡！\n\n主持人認為討論結束時，按下「開始投票」進入投票環節。");
    dpp::message msg; msg.channel_id = ch; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗳️ 開始投票（主持人）").set_id("onw_begin_vote_"+gs)
        .set_style(dpp::cos_danger));
    msg.add_component(row);

    bot.message_create(msg);
}

// ─── Vote Resolution ──────────────────────────────────────────────────────────

static void onw_resolve_vote(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    std::vector<ONWPlayer> players;
    std::vector<std::string> night_log;
    std::string gs = std::to_string(gid);

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        g.phase = ONWPhase::GAME_OVER;
        ch = g.channel_id;
        players = g.players;
        night_log = g.night_log;
    }

    // Tally votes
    std::map<dpp::snowflake, int> tally;
    for (auto& p : players)
        tally[p.uid] = 0;
    for (auto& p : players)
        if (p.vote_target != 0) tally[p.vote_target]++;

    int max_votes = 0;
    for (auto& [uid, cnt] : tally) if (cnt > max_votes) max_votes = cnt;

    std::vector<dpp::snowflake> eliminated;
    if (max_votes > 0) {
        for (auto& [uid, cnt] : tally)
            if (cnt == max_votes) eliminated.push_back(uid);
        if (eliminated.size() > 1) eliminated.clear(); // 同票 → 無人出局
    }

    // Build role reveal
    std::string reveal;
    for (auto& p : players) {
        bool dead = std::find(eliminated.begin(), eliminated.end(), p.uid) != eliminated.end();
        reveal += (dead ? "💀 " : "🔵 ") + p.display_name
               + " — 原始：" + onw_emoji(p.original_role) + p.original_role
               + "　當下：" + onw_emoji(p.current_role) + p.current_role + "\n";
    }

    // Center reveal
    std::string center_str;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& g = onw_games[gid];
        for (int i = 0; i < 3; i++)
            center_str += "中央" + std::to_string(i+1) + ": " + onw_emoji(g.center[i]) + g.center[i] + "　";
    }

    // Determine winner
    // Check if any eliminated player is Tanner
    std::vector<dpp::snowflake> tanner_wins, wolf_killed;
    for (auto uid : eliminated) {
        for (auto& p : players) {
            if (p.uid != uid) continue;
            if (p.current_role == "皮革匠")          tanner_wins.push_back(uid);
            if (is_wolf_role(p.current_role)) wolf_killed.push_back(uid);
        }
    }

    // Check if any player currently holds wolf role
    bool any_player_wolf = false;
    for (auto& p : players) if (is_wolf_role(p.current_role)) { any_player_wolf = true; break; }

    std::string winner_title, winner_desc;
    std::vector<dpp::snowflake> winners, losers;

    if (!tanner_wins.empty()) {
        // Tanner killed — tanner wins alone
        winner_title = "🩱  皮革匠獨自獲勝！";
        winner_desc  = "皮革匠被投死，皮革匠勝出！";
        for (auto& p : players) {
            if (p.current_role == "皮革匠") winners.push_back(p.uid);
            else losers.push_back(p.uid);
        }
    } else if (!wolf_killed.empty()) {
        // A werewolf was eliminated — village wins
        winner_title = "🏘️  村民陣營獲勝！";
        winner_desc  = "成功找出狼人！";
        for (auto& p : players) {
            if (!is_wolf_role(p.current_role)) winners.push_back(p.uid);
            else losers.push_back(p.uid);
        }
    } else if (!any_player_wolf) {
        // No wolves among players (all in center)
        if (eliminated.empty()) {
            // Nobody died — village wins (no wolf to kill)
            winner_title = "🏘️  村民陣營獲勝！";
            winner_desc  = "場上沒有狼人，平票無人死亡，村民獲勝！";
            for (auto& p : players) winners.push_back(p.uid);
        } else {
            // Killed someone but no wolf in play — no winner
            winner_title = "💀  無人獲勝！";
            winner_desc  = "場上沒有狼人，卻有人被投死，雙方皆輸！";
            for (auto& p : players) losers.push_back(p.uid);
        }
    } else {
        // Wolves exist but not killed — wolf wins
        winner_title = "🐺  狼人陣營獲勝！";
        winner_desc  = "沒有狼人被投死，狼人獲勝！";
        for (auto& p : players) {
            if (is_wolf_role(p.current_role)) winners.push_back(p.uid);
            else losers.push_back(p.uid);
        }
    }

    // Give rewards
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto uid : winners) chip_data[uid].chips += ONW_REWARD_WIN;
        for (auto uid : losers)  chip_data[uid].chips += ONW_REWARD_LOSE;
    }
    save_chips();

    // Update ONW stats
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& p : players) {
            auto& st = onw_stats_data[p.uid];
            bool won = std::find(winners.begin(), winners.end(), p.uid) != winners.end();
            if (is_wolf_role(p.current_role)) {
                st.wolf_games++; if (won) st.wolf_wins++;
            } else if (p.current_role == "皮革匠") {
                st.tanner_games++; if (won) st.tanner_wins++;
            } else {
                st.village_games++; if (won) st.village_wins++;
            }
        }
    }
    save_onw_stats();

    // Cleanup game
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        channel_onw_game.erase(onw_games[gid].channel_id);
        onw_games.erase(gid);
    }

    // Post result
    dpp::embed e;
    int result_color = !wolf_killed.empty() ? 0x2ECC71 : (winners.empty() ? 0x95A5A6 : 0x8B0000);
    e.set_title(winner_title).set_color(result_color);
    e.set_description(winner_desc);
    e.add_field("🃏 角色揭曉", reveal, false);

    // Vote breakdown
    {
        std::map<dpp::snowflake, std::string> uid_name;
        for (auto& p : players) uid_name[p.uid] = p.display_name;

        std::string vote_str;
        for (auto& p : players) {
            vote_str += "▸ **" + p.display_name + "** 投給 ";
            if (p.vote_target == 0) vote_str += "（棄票）";
            else vote_str += uid_name.count(p.vote_target) ? "**" + uid_name[p.vote_target] + "**" : "?";
            vote_str += "\n";
        }
        // Tally (show players who got any votes)
        std::vector<std::pair<int,dpp::snowflake>> sorted_tally;
        for (auto& [u, cnt] : tally) if (cnt > 0) sorted_tally.push_back({cnt, u});
        std::sort(sorted_tally.rbegin(), sorted_tally.rend());
        if (!sorted_tally.empty()) {
            vote_str += "\n票數：";
            for (auto& [cnt, u] : sorted_tally) {
                bool dead = std::find(eliminated.begin(), eliminated.end(), u) != eliminated.end();
                vote_str += uid_name[u] + " " + std::to_string(cnt) + "票" + (dead ? "💀" : "") + "　";
            }
        }
        e.add_field("🗳️ 投票明細", vote_str, false);
    }

    e.add_field("🎴 中央牌", center_str, false);
    if (!night_log.empty()) {
        std::string log_str;
        for (auto& line : night_log) log_str += line + "\n";
        e.add_field("🌙 夜晚行動復盤", log_str, false);
    }
    e.add_field("💰 獎勵", "獲勝方 **+" + std::to_string(ONW_REWARD_WIN) + "** 碼，落敗方 **+" + std::to_string(ONW_REWARD_LOSE) + "** 碼", false);
    {
        dpp::message m; m.channel_id = ch; m.add_embed(e);
        std::string ch_s = std::to_string((uint64_t)ch);
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("再來一場").set_id("onw_again_" + ch_s)
            .set_style(dpp::cos_primary).set_emoji("🌙", 0));
        m.add_component(row);
        bot.message_create(m);
    }
}

// ─── Game Start ───────────────────────────────────────────────────────────────

static void onw_begin_game(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_games.find(gid);
        if (it == onw_games.end()) return;
        auto& g = it->second;
        onw_assign_roles(g);
        ch = g.channel_id;
    }

    // Send role DMs
    std::vector<std::pair<dpp::snowflake, std::string>> role_dms;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& p : onw_games[gid].players)
            role_dms.push_back({p.uid, p.original_role});
    }
    for (auto& [uid, role] : role_dms) {
        dpp::embed e;
        e.set_title(onw_emoji(role) + "  你的起始身份：" + role).set_color(0x5865F2);
        std::string desc;
        if (role == "狼人")     desc = "你是**狼人**！夜晚與同伴確認彼此，若你是孤狼可偷看一張中央牌。\n目標：不被投死。";
        if (role == "頭狼")     desc = "你是**頭狼**！與其他狼人互認，若中央有狼人牌可以把它換給一名玩家讓他變成狼人。\n目標：不被投死。";
        if (role == "皮革匠")   desc = "你是**皮革匠**！你不想活著。\n目標：讓自己被投死，即可獨自獲勝。";
        if (role == "預言家")   desc = "你是**預言家**！可查看一名玩家的身份，或兩張中央牌。\n目標：找出並殺死狼人。";
        if (role == "強盜")     desc = "你是**強盜**！可與一名玩家交換身份（你會看到新身份）。\n目標：你換來的身份就是你的陣營。";
        if (role == "搗蛋鬼")   desc = "你是**搗蛋鬼**！可交換兩名其他玩家的身份（你不會看到）。\n目標：幫助好人找出狼人。";
        if (role == "女巫")     desc = "你是**女巫**！偷看一張中央牌，可選擇把它換給任意玩家（含自己），或略過。\n目標：根據最終身份決定陣營。";
        if (role == "酒鬼")     desc = "你是**酒鬼**！必須從中央取一張牌換掉自己的（你不會看到）。\n目標：你不知道自己最終是什麼身份。";
        if (role == "村子白痴") desc = "你是**村子白痴**！選擇左移或右移，所有玩家的牌循環移動一格。你的原始牌會翻面公開。\n目標：不被投死（好人陣營）。";
        if (role == "失眠者")   desc = "你是**失眠者**！最後會看到自己當下的身份。\n目標：根據最終身份決定陣營。";
        if (role == "村民")     desc = "你是**村民**！沒有特殊能力，靠推理與說服找出狼人。\n目標：投死狼人。";
        e.set_description(desc);
        bot.direct_message_create(uid, dpp::message().add_embed(e));
    }

    // Build role distribution from players + center
    {
        std::map<std::string,int> role_cnt;
        for (auto& [u, r] : role_dms) role_cnt[r]++;
        std::array<std::string,3> center_roles;
        { std::lock_guard<std::mutex> lk(data_mutex); center_roles = onw_games[gid].center; }
        for (auto& r : center_roles) role_cnt[r]++;

        static const std::vector<std::string> role_order = {
            "狼人","頭狼","皮革匠","預言家","強盜","搗蛋鬼","女巫","酒鬼","村子白痴","失眠者","村民"
        };
        std::string role_list;
        for (auto& r : role_order)
            if (role_cnt.count(r) && role_cnt[r] > 0)
                role_list += onw_emoji(r) + " **" + r + "**" + (role_cnt[r] > 1 ? " ×" + std::to_string(role_cnt[r]) : "") + "\n";

        dpp::embed e;
        e.set_title("🌙 夜晚開始").set_color(0x2C3E50);
        e.set_description("角色已發放，請查看私訊！");
        e.add_field("本局角色一覽（共 " + std::to_string((int)role_dms.size()) + " 人 + 3 中央）", role_list, false);
        dpp::message m; m.channel_id = ch; m.add_embed(e);
        bot.message_create(m);
    }

    // Start night
    onw_start_wolves(bot, gid);
}
