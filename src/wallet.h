#pragma once
// Must be included after pet.h, shoot.h, wolfplayerstats.h, and guess.h
#include "wolfplayerstats.h"
#include "guess.h"
#include "rl_stats.h"

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
    ChipData cd;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end() && it->second.stage > 0) {
            pet = it->second;
            has_pet = true;
        }
        auto ci = chip_data.find(uid);
        if (ci != chip_data.end()) cd = ci->second;
    }

    std::string content = "## 💼 我的錢包\n<@" + std::to_string((uint64_t)uid) + ">\n\n";
    content += "**💰 持有籌碼** " + std::to_string(chips) + " 碼\n";

    if (has_pet) {
        std::string name = pet_name(pet.chain, pet.stage, pet.variant);
        if (!pet.custom_name.empty()) name = pet.custom_name + "（" + name + "）";
        std::string status = wallet_work_status(pet);
        content += "**🐾 寵物** " + name + "\n" + status + "\n";
    } else {
        content += "**🐾 寵物** 尚無寵物（可至商店購買）\n";
    }

    {
        time_t now = time(nullptr);
        auto fmt_remain = [now](time_t until) -> std::string {
            if (until == 0 || until <= now) return "";
            int rem = (int)(until - now);
            int d = rem / 86400, h = (rem % 86400) / 3600, m = (rem % 3600) / 60;
            if (d > 0) return "剩 " + std::to_string(d) + " 天 " + std::to_string(h) + " 小時";
            if (h > 0) return "剩 " + std::to_string(h) + " 小時 " + std::to_string(m) + " 分";
            return "剩 " + std::to_string(m) + " 分鐘";
        };
        std::string priv;
        std::string v = fmt_remain(cd.vip_until);
        std::string s = fmt_remain(cd.supervisor_until);
        std::string ins = fmt_remain(cd.insurance_until);
        if (!v.empty()) {
            std::string last_str;
            if (cd.vip_last_claim > 0) {
                int mins = (int)((now - cd.vip_last_claim) / 60);
                if (mins < 60) last_str = "（" + std::to_string(mins) + " 分鐘前自動領取）";
                else           last_str = "（" + std::to_string(mins / 60) + " 小時前自動領取）";
            } else {
                last_str = "（尚未自動領取）";
            }
            priv += "👑 **尊爵VIP** — " + v + " " + last_str + "\n";
        }
        if (!s.empty())   priv += "🏭 **寵物監工** — " + s + "\n";
        if (!ins.empty()) priv += "💊 **醫療保險** — " + ins + "\n";
        if (!priv.empty()) content += "\n**✨ 特權狀態**\n" + priv;
    }

    std::string sid = std::to_string((uint64_t)uid);
    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x58, 0x65, 0xF2));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📊 遊戲統計").set_id("wallet_games_" + sid).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏦 銀行").set_id("wallet_bank_" + sid).set_style(dpp::cos_secondary));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);
    msg.add_component_v2(row);
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

    auto fmt_profit = [](int64_t p) -> std::string {
        return (p >= 0 ? "+" : "") + std::to_string(p) + " 碼";
    };
    auto fmt_rate = [](int w, int total) -> std::string {
        if (total == 0) return "—";
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f%%", w * 100.0 / total);
        return buf;
    };

    std::string content = "## 📊 遊戲統計\n<@" + std::to_string((uint64_t)uid) + ">\n\n";

    // 21點
    int bj_total = bj_w + bj_l + bj_p;
    if (bj_total > 0) {
        content += "**🃏 21點** 勝/負/平 **" + std::to_string(bj_w) + "/" + std::to_string(bj_l) + "/" + std::to_string(bj_p) + "**"
            + "　勝率 **" + fmt_rate(bj_w, bj_total) + "**　盈虧 **" + fmt_profit(bj_profit) + "**\n";
    } else {
        content += "**🃏 21點** 尚無紀錄\n";
    }

    // 骰子
    int d_total = d_w + d_l;
    if (d_total > 0) {
        content += "**🎲 骰子** 勝/負 **" + std::to_string(d_w) + "/" + std::to_string(d_l) + "**"
            + "　勝率 **" + fmt_rate(d_w, d_total) + "**　盈虧 **" + fmt_profit(d_profit) + "**\n";
    } else {
        content += "**🎲 骰子** 尚無紀錄\n";
    }

    // 射龍門
    int sh_decided = sh_w + sh_l + sh_b;
    if (sh_decided > 0 || sh_pass > 0) {
        content += "**🃏 射龍門** 射中/射偏/撞柱/棄牌 **"
            + std::to_string(sh_w) + "/" + std::to_string(sh_l) + "/"
            + std::to_string(sh_b) + "/" + std::to_string(sh_pass) + "**"
            + "　勝率 **" + fmt_rate(sh_w, sh_decided) + "**　盈虧 **" + fmt_profit(sh_profit) + "**\n";
    } else {
        content += "**🃏 射龍門** 尚無紀錄\n";
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
        content += "**🚀 火箭升空** 收手/爆炸 **" + std::to_string(rk_w) + "/" + std::to_string(rk_l) + "**"
            + "　勝率 **" + fmt_rate(rk_w, rk_total) + "**　盈虧 **" + fmt_profit(rk_profit_v) + "**\n";
    } else {
        content += "**🚀 火箭升空** 尚無紀錄\n";
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
        content += "**🎴 刮刮樂** 中獎/未中 **" + std::to_string(sk_w) + "/" + std::to_string(sk_l) + "**"
            + "　勝率 **" + fmt_rate(sk_w, sk_total) + "**　盈虧 **" + fmt_profit(sk_profit_v) + "**\n";
    } else {
        content += "**🎴 刮刮樂** 尚無紀錄\n";
    }

    // 一夜狼人
    int onw_w = 0, onw_v = 0, onw_t = 0, onw_ww = 0, onw_vw = 0, onw_tw = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_stats_data.find(uid);
        if (it != onw_stats_data.end()) {
            onw_w  = it->second.wolf_games;    onw_ww = it->second.wolf_wins;
            onw_v  = it->second.village_games; onw_vw = it->second.village_wins;
            onw_t  = it->second.tanner_games;  onw_tw = it->second.tanner_wins;
        }
    }
    int onw_total = onw_w + onw_v + onw_t;
    int onw_total_wins = onw_ww + onw_vw + onw_tw;
    if (onw_total > 0) {
        content += "**🌙 一夜狼人** 總場次 **" + std::to_string(onw_total) + "**　"
            "勝場 **" + std::to_string(onw_total_wins) + "**　"
            "勝率 **" + fmt_rate(onw_total_wins, onw_total) + "**\n";
    } else {
        content += "**🌙 一夜狼人** 尚無紀錄\n";
    }

    // 猜數字
    content += "**🔢 猜數字** " + guess_stats_line(uid) + "\n";

    // 誰是臥底
    int uc_cg = 0, uc_cw = 0, uc_sg = 0, uc_sw = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_stats_data.find(uid);
        if (it != uc_stats_data.end()) {
            uc_cg = it->second.civ_games; uc_cw = it->second.civ_wins;
            uc_sg = it->second.spy_games; uc_sw = it->second.spy_wins;
        }
    }
    int uc_total = uc_cg + uc_sg, uc_wins = uc_cw + uc_sw;
    if (uc_total > 0) {
        content += "**🕵️ 誰是臥底** 總場次 **" + std::to_string(uc_total) + "**　"
            "勝場 **" + std::to_string(uc_wins) + "**　"
            "勝率 **" + fmt_rate(uc_wins, uc_total) + "**\n";
    } else {
        content += "**🕵️ 誰是臥底** 尚無紀錄\n";
    }

    // 俄羅斯輪盤
    int rl_w = 0, rl_l = 0; int64_t rl_profit = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = roulette_stats_data.find(uid);
        if (it != roulette_stats_data.end()) {
            rl_w = it->second.wins; rl_l = it->second.losses; rl_profit = it->second.profit;
        }
    }
    int rl_total = rl_w + rl_l;
    if (rl_total > 0) {
        content += "**🎲 俄羅斯輪盤** 勝/負 **" + std::to_string(rl_w) + "/" + std::to_string(rl_l) + "**"
            + "　勝率 **" + fmt_rate(rl_w, rl_total) + "**　盈虧 **" + fmt_profit(rl_profit) + "**\n";
    } else {
        content += "**🎲 俄羅斯輪盤** 尚無紀錄\n";
    }

    // 猜拳
    int rps_w = 0, rps_l = 0; int64_t rps_profit = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = rps_stats_data.find(uid);
        if (it != rps_stats_data.end()) {
            rps_w = it->second.wins; rps_l = it->second.losses; rps_profit = it->second.profit;
        }
    }
    int rps_total = rps_w + rps_l;
    if (rps_total > 0) {
        content += "**✊ 猜拳** 勝/負 **" + std::to_string(rps_w) + "/" + std::to_string(rps_l) + "**"
            + "　勝率 **" + fmt_rate(rps_w, rps_total) + "**　盈虧 **" + fmt_profit(rps_profit) + "**\n";
    } else {
        content += "**✊ 猜拳** 尚無紀錄\n";
    }

    std::string sid = std::to_string((uint64_t)uid);
    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("← 返回").set_id("wallet_home_" + sid).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🌙 一夜狼人").set_id("wallet_onw_" + sid).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐺 狼人殺").set_id("wallet_wolf_" + sid).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏦 銀行").set_id("wallet_bank_" + sid).set_style(dpp::cos_secondary));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);
    msg.add_component_v2(row);
    return msg;
}
