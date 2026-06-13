#pragma once
#include "helpers.h"

// ─── Step 1：選王 ─────────────────────────────────────────────────────────────

static dpp::message make_boss_msg(const dpp::user& user) {
    dpp::message msg;
    msg.add_embed(base_embed("⚔️  王團報名", "請選擇要報名的王：", 0x5865F2, user));
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& [label, style] : std::vector<std::pair<std::string, dpp::component_style>>{
        {"困難拉圖斯", dpp::cos_danger},
        {"普通拉圖斯", dpp::cos_primary},
        {"殘暴炎魔",   dpp::cos_danger}
    }) {
        dpp::component btn;
        btn.set_type(dpp::cot_button).set_label(label)
           .set_id("boss_" + label).set_style(style);
        row.add_component(btn);
    }
    msg.add_component(row);
    return msg;
}

// ─── Step 2：選時間 ───────────────────────────────────────────────────────────

static dpp::message make_time_msg(const std::string& boss, const dpp::user& user,
                                   int view_day = 0,
                                   const std::set<std::pair<std::string,std::string>>& slots = {}) {
    auto week = get_game_week();
    const std::string& cur_label = week[view_day].second;
    auto is_sel = [&](const std::string& t) { return slots.count({cur_label, t}) > 0; };

    // Compact description
    std::ostringstream desc;
    desc << "<@" << user.id << ">  ⚔️ **" << boss << "**  ·  📌 **" << cur_label << "**\n\n";
    desc << "請選擇**連七的出發時間點**（綠色=已選）\n\n";

    bool any = false;
    for (auto& [k, label] : week) {
        std::vector<std::string> ts;
        for (auto& [d, t] : slots)
            if (d == label) ts.push_back(t);
        if (ts.empty()) continue;
        std::sort(ts.begin(), ts.end());
        desc << "✅  **" << label << "**";
        for (auto& t : ts) desc << "  · " << t;
        desc << "\n";
        any = true;
    }
    if (!any) desc << "*尚未選擇任何時段*";

    dpp::embed e;
    e.set_title("🗓️  選擇時間").set_color(0x3498DB);
    e.set_description(desc.str());
    e.set_footer(dpp::embed_footer().set_text("王團報名系統"));
    if (!get_boss_img(boss).empty()) e.set_thumbnail(get_boss_img(boss));
    dpp::message msg; msg.add_embed(e);

    // Row 1：日期選單
    {
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component sel;
        sel.set_type(dpp::cot_selectmenu).set_id("day_select").set_placeholder("選擇日期");
        for (int i = 0; i < 7; i++) {
            dpp::select_option opt(week[i].second, std::to_string(i), week[i].second);
            opt.set_default(i == view_day);
            sel.add_select_option(opt);
        }
        row.add_component(sel);
        msg.add_component(row);
    }
    // Row 2：12:00~20:00
    {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (const char* v : {"12:00","14:00","16:00","18:00","20:00"}) {
            dpp::component btn;
            btn.set_type(dpp::cot_button).set_label(v)
               .set_id(std::string("slot_") + v)
               .set_style(is_sel(v) ? dpp::cos_success : dpp::cos_secondary);
            row.add_component(btn);
        }
        msg.add_component(row);
    }
    // Row 3：22:00, 00:00 + 返回 + 確定
    {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (const char* v : {"22:00","00:00"}) {
            dpp::component btn;
            btn.set_type(dpp::cot_button).set_label(v)
               .set_id(std::string("slot_") + v)
               .set_style(is_sel(v) ? dpp::cos_success : dpp::cos_secondary);
            row.add_component(btn);
        }
        dpp::component back, conf;
        back.set_type(dpp::cot_button).set_label("↩ 返回")
            .set_id("back_to_boss").set_style(dpp::cos_secondary);
        conf.set_type(dpp::cot_button).set_label("✅ 確定")
            .set_id("confirm_time").set_style(dpp::cos_success).set_disabled(slots.empty());
        row.add_component(back); row.add_component(conf);
        msg.add_component(row);
    }
    return msg;
}

// ─── Step 3：選位置 ───────────────────────────────────────────────────────────

static dpp::message make_position_msg(const std::string& boss, const dpp::user& user) {
    dpp::message msg;
    msg.add_embed(base_embed("🎭  選擇職業",
        "⚔️ **" + boss + "**\n請選擇職業位置：",
        0xE67E22, user, get_boss_img(boss)));
    auto positions = get_positions(boss);
    dpp::component row; row.set_type(dpp::cot_action_row);
    int i = 0;
    for (auto& pos : positions) {
        if (i > 0 && i % 5 == 0) {
            msg.add_component(row);
            row = dpp::component(); row.set_type(dpp::cot_action_row);
        }
        dpp::component btn;
        btn.set_type(dpp::cot_button).set_label(pos)
           .set_id("pos_" + pos).set_style(dpp::cos_secondary);
        row.add_component(btn); ++i;
    }
    if (i > 0) msg.add_component(row);
    {
        dpp::component brow; brow.set_type(dpp::cot_action_row);
        dpp::component back;
        back.set_type(dpp::cot_button).set_label("↩ 返回")
            .set_id("back_to_time").set_style(dpp::cos_secondary);
        brow.add_component(back);
        msg.add_component(brow);
    }
    return msg;
}

// ─── Step 4：報名成功 ─────────────────────────────────────────────────────────

static dpp::message make_success_msg(const Registration& reg) {
    dpp::embed e;
    e.set_title("✅  報名成功！").set_color(0x2ECC71);
    if (!get_boss_img(reg.boss).empty()) e.set_thumbnail(get_boss_img(reg.boss));
    e.add_field("⚔️  王",   reg.boss,                    true);
    e.add_field("🎭  職業", reg.position,                 true);
    e.add_field("📅  時段", format_slots(reg.slots),      false);
    e.set_footer(dpp::embed_footer().set_text("報名者：" + reg.username));
    dpp::message msg; msg.add_embed(e);
    return msg;
}

// ─── 紀錄：選擇查看方式 ────────────────────────────────────────────────────────

static dpp::message make_records_select_msg(const dpp::user& user) {
    dpp::embed e;
    e.set_title("📋  王團報名紀錄").set_color(0xE74C3C);
    e.set_description("<@" + std::to_string(user.id) + "> 請選擇查看方式：");
    e.set_footer(dpp::embed_footer().set_text("王團報名系統"));
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component sel;
    sel.set_type(dpp::cot_selectmenu).set_id("records_view").set_placeholder("選擇查看方式");
    sel.add_select_option(dpp::select_option("👤 我的報名",     "mine",           "查看個人報名紀錄"));
    sel.add_select_option(dpp::select_option("⚔️ 困難拉圖斯",  "boss_困難拉圖斯", "困難拉圖斯報名名單"));
    sel.add_select_option(dpp::select_option("🗡️ 普通拉圖斯",  "boss_普通拉圖斯", "普通拉圖斯報名名單"));
    sel.add_select_option(dpp::select_option("🔥 殘暴炎魔",    "boss_殘暴炎魔",   "殘暴炎魔報名名單"));
    row.add_component(sel); msg.add_component(row);
    return msg;
}

// ─── 紀錄：顯示篩選結果 ───────────────────────────────────────────────────────

static dpp::message make_records_view_msg(const std::string& filter,
                                           dpp::snowflake viewer_id, bool admin) {
    std::vector<Registration> list;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& r : registrations) {
            if (filter == "mine" && r.user_id == viewer_id) list.push_back(r);
            else if (filter.rfind("boss_", 0) == 0 && r.boss == filter.substr(5)) list.push_back(r);
        }
    }
    dpp::embed e;
    std::string title = (filter == "mine") ? "👤 我的報名" : "【" + filter.substr(5) + "】 報名名單";
    e.set_title("📋  " + title).set_color(0xE74C3C);
    e.set_footer(dpp::embed_footer().set_text("王團報名系統"));
    const size_t MAX_SHOW = 20;
    bool truncated = list.size() > MAX_SHOW;
    if (truncated) list.resize(MAX_SHOW);
    if (list.empty()) {
        e.set_description("目前沒有紀錄。");
    } else {
        if (truncated) e.set_description("⚠️ 僅顯示前 20 筆。");
        for (size_t i = 0; i < list.size(); i++) {
            auto& r = list[i];
            // Compact: emoji labels, no verbose text
            e.add_field(std::to_string(i+1) + ".  " + r.username,
                        "🎭 " + r.position + "\n📅 " + format_slots(r.slots), false);
        }
    }
    dpp::message msg; msg.add_embed(e);
    if (!list.empty()) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        int cnt = 0;
        for (size_t i = 0; i < list.size(); i++) {
            if (cnt > 0 && cnt % 5 == 0) {
                msg.add_component(row);
                row = dpp::component(); row.set_type(dpp::cot_action_row);
            }
            bool can_del = (list[i].user_id == viewer_id) || admin;
            dpp::component btn;
            btn.set_type(dpp::cot_button)
               .set_label("🗑️ " + std::to_string(i+1))
               .set_id("del_" + std::to_string(list[i].id))
               .set_style(can_del ? dpp::cos_danger : dpp::cos_secondary)
               .set_disabled(!can_del);
            row.add_component(btn); ++cnt;
        }
        if (cnt > 0) msg.add_component(row);
    }
    {
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component btn;
        btn.set_type(dpp::cot_button).set_label("↩ 返回")
           .set_id("records_back").set_style(dpp::cos_secondary);
        row.add_component(btn); msg.add_component(row);
    }
    return msg;
}
