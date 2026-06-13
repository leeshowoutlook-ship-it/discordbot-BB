#pragma once
#include "messages.h"
#include <optional>

// ─── Slot check ───────────────────────────────────────────────────────────────

static bool has_slot(const Registration& r, const std::string& day, const std::string& time) {
    for (auto& [d, t] : r.slots)
        if (d == day && t == time) return true;
    return false;
}

// ─── 組隊演算法 ───────────────────────────────────────────────────────────────

// 普通拉圖斯：法師×1 + 火×1 + 敏職輸出×1 + 任意×3
static std::optional<std::vector<Registration>> try_normal_latus(
    const std::vector<Registration>& avail) {
    if (avail.size() < 6) return std::nullopt;
    std::vector<Registration> huo, fa, min_out, rest;
    for (auto& r : avail) {
        if      (r.position == "火")       huo.push_back(r);
        else if (r.position == "法師")     fa.push_back(r);
        else if (r.position == "敏職輸出") min_out.push_back(r);
        else                               rest.push_back(r);
    }
    if (huo.empty() || fa.empty() || min_out.empty()) return std::nullopt;
    std::vector<Registration> team = {huo[0], fa[0], min_out[0]};
    std::vector<Registration> pool;
    for (size_t i=1; i<huo.size();     i++) pool.push_back(huo[i]);
    for (size_t i=1; i<fa.size();      i++) pool.push_back(fa[i]);
    for (size_t i=1; i<min_out.size(); i++) pool.push_back(min_out[i]);
    for (auto& r : rest) pool.push_back(r);
    for (size_t i=0; i<pool.size() && team.size()<6; i++) team.push_back(pool[i]);
    if (team.size() < 6) return std::nullopt;
    return team;
}

// 殘暴炎魔：(兩刀/三刀法師)×1 + 火×1(可選) + 填滿6人
static std::optional<std::vector<Registration>> try_flame_demon(
    const std::vector<Registration>& avail) {
    if (avail.size() < 6) return std::nullopt;
    std::vector<Registration> sword, huo, need_fire, no_fire;
    for (auto& r : avail) {
        if      (r.position == "兩刀法師" || r.position == "三刀法師") sword.push_back(r);
        else if (r.position == "火")             huo.push_back(r);
        else if (r.position == "需要火的輸出")   need_fire.push_back(r);
        else if (r.position == "不需要火的輸出") no_fire.push_back(r);
    }
    if (sword.empty()) return std::nullopt;
    // 方案A：1 sword + 1 火 + 4 任意
    if (!huo.empty()) {
        std::vector<Registration> team = {sword[0], huo[0]};
        std::vector<Registration> pool;
        for (size_t i=1; i<sword.size(); i++) pool.push_back(sword[i]);
        for (size_t i=1; i<huo.size();   i++) pool.push_back(huo[i]);
        for (auto& r : need_fire) pool.push_back(r);
        for (auto& r : no_fire)   pool.push_back(r);
        for (size_t i=0; i<pool.size() && team.size()<6; i++) team.push_back(pool[i]);
        if (team.size() >= 6) return team;
    }
    // 方案B：1 sword + 5 不需要火的輸出
    if (no_fire.size() >= 5) {
        std::vector<Registration> team = {sword[0]};
        for (int i=0; i<5; i++) team.push_back(no_fire[i]);
        return team;
    }
    return std::nullopt;
}

// 困難拉圖斯：主控法×1 + 清球兩刀法×1 + 火×1 + 時間副控×1 + 輸出×2
static std::optional<std::vector<Registration>> try_hard_latus(
    const std::vector<Registration>& avail) {
    if (avail.size() < 6) return std::nullopt;
    std::vector<Registration> main_c, ball_c, both_c, huo, time_c, output;
    for (auto& r : avail) {
        if      (r.position == "主控法")             main_c.push_back(r);
        else if (r.position == "清球兩刀法")          ball_c.push_back(r);
        else if (r.position == "主控清球都可以的法")  both_c.push_back(r);
        else if (r.position == "火")                  huo.push_back(r);
        else if (r.position == "時間副控")            time_c.push_back(r);
        else output.push_back(r); // 力職輸出, 敏職輸出
    }
    if (huo.empty() || time_c.empty() || output.size() < 2) return std::nullopt;
    std::vector<Registration*> zhu_cands, qiu_cands;
    for (auto& r : main_c) zhu_cands.push_back(&r);
    for (auto& r : both_c) zhu_cands.push_back(&r);
    for (auto& r : ball_c) qiu_cands.push_back(&r);
    for (auto& r : both_c) qiu_cands.push_back(&r);
    for (auto* zhu : zhu_cands) {
        for (auto* qiu : qiu_cands) {
            if (zhu == qiu) continue;
            std::vector<Registration> team = {*zhu, *qiu, huo[0], time_c[0]};
            int added = 0;
            for (auto& r : output) {
                if (added >= 2) break;
                bool dup = false;
                for (auto& t : team) if (t.id == r.id) { dup = true; break; }
                if (!dup) { team.push_back(r); added++; }
            }
            if (team.size() >= 6) return team;
        }
    }
    return std::nullopt;
}

// ─── 組隊偵測 ─────────────────────────────────────────────────────────────────

static void check_team_formation(dpp::cluster& bot, const std::string& boss,
                                  dpp::snowflake channel_id) {
    std::set<std::pair<std::string,std::string>> all_slots;
    std::vector<Registration> boss_regs;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& r : registrations) {
            if (r.boss != boss) continue;
            boss_regs.push_back(r);
            for (auto& [d, t] : r.slots)
                all_slots.insert({d, t});
        }
    }

    for (auto& [day, time] : all_slots) {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (proposed_slots.count({boss, day, time})) continue;
        }
        std::vector<Registration> avail;
        for (auto& r : boss_regs)
            if (has_slot(r, day, time)) avail.push_back(r);

        std::optional<std::vector<Registration>> team;
        if      (boss == "普通拉圖斯") team = try_normal_latus(avail);
        else if (boss == "殘暴炎魔")   team = try_flame_demon(avail);
        else if (boss == "困難拉圖斯") team = try_hard_latus(avail);
        if (!team) continue;

        ProposedTeam pt;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            pt.id = team_counter++;
            pt.boss = boss; pt.day = day; pt.time_slot = time;
            pt.members = *team;
            proposed_teams[pt.id] = pt;
            proposed_slots.insert({boss, day, time});
        }

        dpp::embed e;
        e.set_title("🎯  偵測到可組隊！").set_color(0xF1C40F);
        if (!get_boss_img(boss).empty()) e.set_thumbnail(get_boss_img(boss));
        e.add_field("⚔️  王",   boss,             true);
        e.add_field("🕐  時間", day + "  " + time, true);
        std::ostringstream mem_oss;
        for (size_t i = 0; i < pt.members.size(); i++)
            mem_oss << std::to_string(i+1) << ". **" << pt.members[i].username
                    << "** · " << pt.members[i].position << "\n";
        e.add_field("👥  成員", mem_oss.str(), false);
        e.set_footer(dpp::embed_footer().set_text("王團報名系統"));

        std::string content;
        if (!cfg.notify_user_id.empty())
            content = "<@" + cfg.notify_user_id + "> 找到可組隊，請確認是否成團！";

        dpp::message notify_msg(channel_id, content);
        notify_msg.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component cb, cx;
        cb.set_type(dpp::cot_button).set_label("✅ 成功組團")
          .set_id("team_confirm_" + std::to_string(pt.id)).set_style(dpp::cos_success);
        cx.set_type(dpp::cot_button).set_label("❌ 組團撤銷")
          .set_id("team_cancel_" + std::to_string(pt.id)).set_style(dpp::cos_danger);
        row.add_component(cb); row.add_component(cx);
        notify_msg.add_component(row);
        bot.message_create(notify_msg);
        return; // 一次只通知一個時段
    }
}
