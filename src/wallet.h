#pragma once
// Must be included after pet.h and shoot.h

static std::string wallet_work_status(const Pet& pet) {
    time_t now = time(nullptr);
    if (pet.work_task > 0 && pet.work_end > now) {
        int rem = (int)(pet.work_end - now);
        int h = rem / 3600, m = (rem % 3600) / 60;
        std::string t = (h > 0 ? std::to_string(h) + "h " : "") + std::to_string(m) + "m";
        return "打工中 ⏳ 剩 " + t;
    }
    if (pet.work_task > 0 && pet.work_end <= now)
        return "打工完成 🎉 記得領回！";
    return "閒置中";
}

// ─── Page 1: chips + pet ─────────────────────────────────────────────────────

static dpp::message make_wallet_home_msg(dpp::snowflake uid) {
    int64_t chips = get_chips(uid);

    Pet pet;
    bool has_pet = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end() && it->second.stage > 0) {
            pet = it->second;
            has_pet = true;
        }
    }

    dpp::embed e;
    e.set_title("💼  我的錢包").set_color(0x5865F2);
    e.set_description("<@" + std::to_string((uint64_t)uid) + ">");
    e.add_field("💰  持有籌碼", std::to_string(chips) + " 碼", false);

    if (has_pet) {
        std::string name = pet_name(pet.chain, pet.stage, pet.variant);
        if (!pet.custom_name.empty()) name = pet.custom_name + "（" + name + "）";
        std::string status = wallet_work_status(pet);
        e.add_field("🐾  寵物", name + "\n" + status, false);
        std::string img = pet_image_url(pet.chain, pet.stage, pet.variant);
        if (!img.empty()) e.set_thumbnail(img);
    } else {
        e.add_field("🐾  寵物", "尚無寵物（可至商店購買）", false);
    }

    std::string sid = std::to_string((uint64_t)uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📊 遊戲統計").set_id("wallet_games_" + sid).set_style(dpp::cos_secondary));

    dpp::message msg; msg.add_embed(e); msg.add_component(row);
    return msg;
}

// ─── Page 2: all game stats ───────────────────────────────────────────────────

static dpp::message make_wallet_games_msg(dpp::snowflake uid) {
    // 21點
    int bj_w = 0, bj_l = 0, bj_p = 0; int64_t bj_profit = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = bj_stats_data.find(uid);
        if (it != bj_stats_data.end()) {
            bj_w = it->second.wins; bj_l = it->second.losses;
            bj_p = it->second.pushes; bj_profit = it->second.profit;
        }
    }
    // 骰子
    int d_w = 0, d_l = 0; int64_t d_profit = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = dice_stats_data.find(uid);
        if (it != dice_stats_data.end()) {
            d_w = it->second.wins; d_l = it->second.losses; d_profit = it->second.profit;
        }
    }
    // 射龍門
    int sh_w = 0, sh_l = 0, sh_b = 0, sh_pass = 0; int64_t sh_profit = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = shoot_stats_data.find(uid);
        if (it != shoot_stats_data.end()) {
            sh_w    = it->second.wins;   sh_l    = it->second.losses;
            sh_b    = it->second.bumps;  sh_pass = it->second.passes;
            sh_profit = it->second.profit;
        }
    }

    dpp::embed e;
    e.set_title("📊  遊戲統計").set_color(0x2ECC71);
    e.set_description("<@" + std::to_string((uint64_t)uid) + ">");

    auto fmt_profit = [](int64_t p) -> std::string {
        return (p >= 0 ? "+" : "") + std::to_string(p) + " 碼";
    };
    auto fmt_rate = [](int w, int total) -> std::string {
        if (total == 0) return "—";
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f%%", w * 100.0 / total);
        return buf;
    };

    // 21點
    int bj_total = bj_w + bj_l + bj_p;
    if (bj_total > 0) {
        e.add_field("🃏  21點",
            "勝/負/平 **" + std::to_string(bj_w) + "/" + std::to_string(bj_l) + "/" + std::to_string(bj_p) + "**"
            + "　勝率 **" + fmt_rate(bj_w, bj_w + bj_l) + "**"
            + "\n盈虧 **" + fmt_profit(bj_profit) + "**", false);
    } else {
        e.add_field("🃏  21點", "尚無紀錄", false);
    }

    // 骰子
    int d_total = d_w + d_l;
    if (d_total > 0) {
        e.add_field("🎲  骰子",
            "勝/負 **" + std::to_string(d_w) + "/" + std::to_string(d_l) + "**"
            + "　勝率 **" + fmt_rate(d_w, d_total) + "**"
            + "\n盈虧 **" + fmt_profit(d_profit) + "**", false);
    } else {
        e.add_field("🎲  骰子", "尚無紀錄", false);
    }

    // 射龍門
    int sh_decided = sh_w + sh_l + sh_b;
    if (sh_decided > 0 || sh_pass > 0) {
        e.add_field("🃏  射龍門",
            "射中/射偏/撞柱/棄牌 **"
            + std::to_string(sh_w) + "/" + std::to_string(sh_l) + "/"
            + std::to_string(sh_b) + "/" + std::to_string(sh_pass) + "**"
            + "　勝率 **" + fmt_rate(sh_w, sh_decided) + "**"
            + "\n盈虧 **" + fmt_profit(sh_profit) + "**", false);
    } else {
        e.add_field("🃏  射龍門", "尚無紀錄", false);
    }

    // 火箭升空
    int rk_w = 0, rk_l = 0; int64_t rk_profit_v = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = rocket_stats_data.find(uid);
        if (it != rocket_stats_data.end()) {
            rk_w = it->second.wins; rk_l = it->second.losses; rk_profit_v = it->second.profit;
        }
    }
    int rk_total = rk_w + rk_l;
    if (rk_total > 0) {
        e.add_field("🚀  火箭升空",
            "收手/爆炸 **" + std::to_string(rk_w) + "/" + std::to_string(rk_l) + "**"
            + "　勝率 **" + fmt_rate(rk_w, rk_total) + "**"
            + "\n盈虧 **" + fmt_profit(rk_profit_v) + "**", false);
    } else {
        e.add_field("🚀  火箭升空", "尚無紀錄", false);
    }

    // 刮刮樂
    int sk_w = 0, sk_l = 0; int64_t sk_profit_v = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = scratch_stats_data.find(uid);
        if (it != scratch_stats_data.end()) {
            sk_w = it->second.wins; sk_l = it->second.losses; sk_profit_v = it->second.profit;
        }
    }
    int sk_total = sk_w + sk_l;
    if (sk_total > 0) {
        e.add_field("🎴  刮刮樂",
            "中獎/未中 **" + std::to_string(sk_w) + "/" + std::to_string(sk_l) + "**"
            + "　勝率 **" + fmt_rate(sk_w, sk_total) + "**"
            + "\n盈虧 **" + fmt_profit(sk_profit_v) + "**", false);
    } else {
        e.add_field("🎴  刮刮樂", "尚無紀錄", false);
    }

    std::string sid = std::to_string((uint64_t)uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("← 返回").set_id("wallet_home_" + sid).set_style(dpp::cos_secondary));

    dpp::message msg; msg.add_embed(e); msg.add_component(row);
    return msg;
}
