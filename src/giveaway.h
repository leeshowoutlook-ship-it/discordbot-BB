#pragma once
#include "helpers.h"
#include <random>

// ─── Giveaway embed (active) ──────────────────────────────────────────────────

static dpp::message make_giveaway_msg(const Giveaway& gw) {
    dpp::embed e;
    // Prize name as title — immediately visible without reading a label
    e.set_title("🎉  " + gw.prize).set_color(0xF39C12);

    // Row 1: 3 inline fields — winner count, end time, participants
    e.add_field("🏆  中獎人數", std::to_string(gw.winner_count) + " 人",             true);
    e.add_field("⏰  結束時間", "<t:" + std::to_string(gw.end_time) + ":R>",          true);
    e.add_field("👥  參與人數", std::to_string(gw.participants.size()) + " 人",       true);

    // Row 2: optional fields (only shown when set)
    if (!gw.provider.empty())  e.add_field("💝  提供者",    gw.provider,    true);
    if (!gw.role_name.empty()) e.add_field("🔒  限制身分組", gw.role_name,  true);

    // Note (full width)
    if (!gw.note.empty()) e.add_field("📝  備註", gw.note, false);

    e.set_footer(dpp::embed_footer().set_text("點下方按鈕參加 / 再按一次取消"));

    std::string content = gw.mention.empty() ? "" : (gw.mention + " ");
    dpp::message msg(gw.channel_id, content);
    msg.add_embed(e);

    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component bjoin, bleave;
    bjoin.set_type(dpp::cot_button)
         .set_label("🎉 加入抽獎 (" + std::to_string(gw.participants.size()) + ")")
         .set_id("giveaway_join_" + std::to_string(gw.id))
         .set_style(dpp::cos_success);
    bleave.set_type(dpp::cot_button)
          .set_label("↩ 取消報名")
          .set_id("giveaway_leave_" + std::to_string(gw.id))
          .set_style(dpp::cos_secondary);
    row.add_component(bjoin);
    row.add_component(bleave);
    msg.add_component(row);
    return msg;
}

// ─── Giveaway embed (ended) ───────────────────────────────────────────────────

static dpp::message make_giveaway_ended_msg(const Giveaway& gw,
                                             const std::vector<dpp::snowflake>& winners) {
    dpp::embed e;
    e.set_title("🎊  " + gw.prize).set_color(0x2ECC71);

    if (winners.empty()) {
        e.add_field("結果", "😢 沒有人參與", false);
    } else {
        std::ostringstream w_oss;
        for (auto& w : winners) w_oss << "<@" << w << ">  ";
        e.add_field("🏆  中獎者", w_oss.str(), false);
    }
    if (!gw.provider.empty()) e.add_field("💝  提供者", gw.provider, true);

    e.set_footer(dpp::embed_footer().set_text("抽獎已結束"));

    dpp::message msg(gw.channel_id, "");
    msg.add_embed(e);

    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component btn, btn2;
    btn.set_type(dpp::cot_button).set_label("🎊 抽獎已結束")
       .set_id("giveaway_ended").set_style(dpp::cos_secondary).set_disabled(true);
    btn2.set_type(dpp::cot_button).set_label("↩ 取消報名")
        .set_id("giveaway_ended2").set_style(dpp::cos_secondary).set_disabled(true);
    row.add_component(btn);
    row.add_component(btn2);
    msg.add_component(row);
    return msg;
}

// ─── End giveaway ─────────────────────────────────────────────────────────────

static void end_giveaway(dpp::cluster& bot, uint64_t gid) {
    Giveaway gw;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = giveaways.find(gid);
        if (it == giveaways.end() || it->second.ended) return;
        it->second.ended = true;
        gw = it->second;
    }

    std::vector<dpp::snowflake> pool(gw.participants.begin(), gw.participants.end());
    std::mt19937 rng(std::random_device{}());
    std::shuffle(pool.begin(), pool.end(), rng);
    int cnt = std::min((int)pool.size(), gw.winner_count);
    std::vector<dpp::snowflake> winners(pool.begin(), pool.begin() + cnt);

    if (gw.msg_id) {
        dpp::message ended = make_giveaway_ended_msg(gw, winners);
        ended.id = gw.msg_id; ended.channel_id = gw.channel_id;
        bot.message_edit(ended);
    }

    // Announce: ping mention + winners
    std::ostringstream ann;
    if (!gw.mention.empty()) ann << gw.mention << "  ";
    if (!winners.empty()) {
        ann << "🎊 **恭喜中獎！** 獎品：**" << gw.prize << "**\n";
        for (auto& w : winners) ann << "<@" << w << ">  ";
    } else {
        ann << "😢 **" << gw.prize << "** 抽獎結束，但沒有人參與。";
    }
    bot.message_create(dpp::message(gw.channel_id, ann.str()));
}

// ─── Periodic check ───────────────────────────────────────────────────────────

static void check_giveaways(dpp::cluster& bot) {
    time_t now = time(nullptr);
    std::vector<uint64_t> to_end;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [id, gw] : giveaways)
            if (!gw.ended && now >= gw.end_time)
                to_end.push_back(id);
    }
    for (auto gid : to_end) end_giveaway(bot, gid);
}
