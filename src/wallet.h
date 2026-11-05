#pragma once
// Must be included after pet.h, shoot.h, wolfplayerstats.h, and guess.h
#include "wolfplayerstats.h"
#include "guess.h"
#include "rl_stats.h"
#include "lolbet.h"
#include "niuniu.h"
#include "numbomb.h"

// ─── Loss leaderboard ─────────────────────────────────────────────────────────
// game: "" = all, "21" "骰子" "射龍門" "火箭" "刮刮樂" "轉盤" "俄羅斯輪盤" "猜拳" "妞妞" "數字炸彈"
// 放在 wallet.h 而不是 chips.h：妞妞／數字炸彈的統計表定義在 niuniu.h／numbomb.h 裡，
// 而那兩個檔案本身會 include chips.h，chips.h 若要用到它們會造成循環 include，
// 所以挪到已經 include 了這些檔案的這裡。

// asc=true: most negative first (虧損排序), asc=false: most positive first (盈利排序)
// scope_guild != 0：只看該伺服器成員；current_guild != 0 時顯示「全部／本伺服器」切換列
// id 的方向字元：a/d＝全部，A/D＝本伺服器（伺服器取按鈕所在的伺服器）
static dpp::message handle_losers_board(int page = 0, const std::string& game = "", bool asc = true,
                                        dpp::snowflake scope_guild = 0, dpp::snowflake current_guild = 0) {
    std::vector<std::pair<dpp::snowflake, int64_t>> sorted;
    std::set<uint64_t> members;
    bool members_ok = (scope_guild == 0) || guild_members_get(scope_guild, members);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        std::set<dpp::snowflake> all_uids;
        for (auto& [u,_] : bj_stats_data)      all_uids.insert(u);
        for (auto& [u,_] : dice_stats_data)    all_uids.insert(u);
        for (auto& [u,_] : shoot_stats_data)   all_uids.insert(u);
        for (auto& [u,_] : rocket_stats_data)  all_uids.insert(u);
        for (auto& [u,_] : scratch_stats_data) all_uids.insert(u);
        for (auto& [u,_] : euroulette_stats_data) all_uids.insert(u);
        for (auto& [u,_] : roulette_stats_data)   all_uids.insert(u);
        for (auto& [u,_] : rps_stats_data)        all_uids.insert(u);
        for (auto& [u,_] : nn_stats_data)  all_uids.insert(dpp::snowflake(u));
        for (auto& [u,_] : nb_stats)       all_uids.insert(dpp::snowflake(u));

        for (auto uid : all_uids) {
            if (scope_guild != 0 && !members.count((uint64_t)uid)) continue;
            int64_t total = 0;
            if (game == "" || game == "21") {
                auto it = bj_stats_data.find(uid);
                if (it != bj_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "骰子") {
                auto it = dice_stats_data.find(uid);
                if (it != dice_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "射龍門") {
                auto it = shoot_stats_data.find(uid);
                if (it != shoot_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "火箭") {
                auto it = rocket_stats_data.find(uid);
                if (it != rocket_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "刮刮樂") {
                auto it = scratch_stats_data.find(uid);
                if (it != scratch_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "轉盤") {
                auto it = euroulette_stats_data.find(uid);
                if (it != euroulette_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "俄羅斯輪盤") {
                auto it = roulette_stats_data.find(uid);
                if (it != roulette_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "猜拳") {
                auto it = rps_stats_data.find(uid);
                if (it != rps_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "妞妞") {
                auto it = nn_stats_data.find((uint64_t)uid);
                if (it != nn_stats_data.end()) total += it->second.profit;
            }
            if (game == "" || game == "數字炸彈") {
                auto it = nb_stats.find((uint64_t)uid);
                if (it != nb_stats.end()) total += it->second.profit;
            }
            sorted.push_back({uid, total});
        }
    }
    if (asc)
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second < b.second; });
    else
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });

    std::string game_label = game.empty() ? "全遊戲" : game;
    bool scoped = (scope_guild != 0);
    std::string dir_c = scoped ? (asc ? "A" : "D") : (asc ? "a" : "d");
    const int PAGE_SIZE = 10;
    int total = (int)sorted.size();
    int total_pages = std::max(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    page = std::max(0, std::min(page, total_pages - 1));

    dpp::embed e;
    std::string title = asc ? ("📉  虧損榜 — " + game_label) : ("📈  盈利榜 — " + game_label);
    title += scoped ? "（本伺服器）" : "（全部）";
    e.set_title(title).set_color(asc ? 0xE74C3C : 0x2ECC71);
    if (!members_ok) {
        e.set_description("⏳ 這個伺服器的成員名單還在讀取中，請稍後再試。");
    } else if (sorted.empty()) {
        e.set_description("目前沒有記錄");
    } else {
        static const char* MEDALS[] = {"🥇", "🥈", "🥉"};
        std::ostringstream oss;
        int start = page * PAGE_SIZE;
        int end   = std::min(start + PAGE_SIZE, total);
        for (int i = start; i < end; i++) {
            std::string rank = (i < 3) ? MEDALS[i] : (std::to_string(i + 1) + ".");
            int64_t val = sorted[i].second;
            std::string val_str = (val >= 0 ? "+" : "") + std::to_string(val);
            oss << rank << " <@" << (uint64_t)sorted[i].first
                << ">  **" << val_str << "** 碼\n";
        }
        e.set_description(oss.str());
    }
    e.set_footer(dpp::embed_footer().set_text(
        "第 " + std::to_string(page+1) + "/" + std::to_string(total_pages) + " 頁  共 " + std::to_string(total) + " 人"));

    dpp::message msg; msg.add_embed(e);

    // Dropdown: preserve current sort direction in values
    dpp::component sel_row; sel_row.set_type(dpp::cot_action_row);
    dpp::component sel; sel.set_type(dpp::cot_selectmenu)
        .set_id("losers_game_sel")
        .set_placeholder("選擇遊戲篩選");
    sel.add_select_option(dpp::select_option("全遊戲",    "losers_0_" + dir_c + "_",         "所有遊戲加總").set_default(game == ""));
    sel.add_select_option(dpp::select_option("21點",      "losers_0_" + dir_c + "_21",       "只看21點"));
    sel.add_select_option(dpp::select_option("骰子",      "losers_0_" + dir_c + "_骰子",     "只看骰子"));
    sel.add_select_option(dpp::select_option("射龍門",    "losers_0_" + dir_c + "_射龍門",   "只看射龍門"));
    sel.add_select_option(dpp::select_option("火箭升空",  "losers_0_" + dir_c + "_火箭",     "只看火箭升空"));
    sel.add_select_option(dpp::select_option("刮刮樂",    "losers_0_" + dir_c + "_刮刮樂",   "只看刮刮樂"));
    sel.add_select_option(dpp::select_option("轉盤",      "losers_0_" + dir_c + "_轉盤",     "只看轉盤"));
    sel.add_select_option(dpp::select_option("俄羅斯輪盤","losers_0_" + dir_c + "_俄羅斯輪盤","只看俄羅斯輪盤"));
    sel.add_select_option(dpp::select_option("猜拳",      "losers_0_" + dir_c + "_猜拳",     "只看猜拳"));
    sel.add_select_option(dpp::select_option("妞妞",      "losers_0_" + dir_c + "_妞妞",     "只看妞妞"));
    sel.add_select_option(dpp::select_option("數字炸彈",  "losers_0_" + dir_c + "_數字炸彈", "只看數字炸彈"));
    sel_row.add_component(sel);
    msg.add_component(sel_row);

    // Navigation + sort toggle row (always shown)
    std::string flip_c = scoped ? (asc ? "D" : "A") : (asc ? "d" : "a");
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    dpp::component prev, sort_btn, next;
    prev.set_type(dpp::cot_button).set_label("◀")
        .set_id("losers_" + std::to_string(page-1) + "_" + dir_c + "_" + game)
        .set_style(dpp::cos_secondary).set_disabled(page == 0);
    sort_btn.set_type(dpp::cot_button)
        .set_label(asc ? "📈 切換為盈利排序" : "📉 切換為虧損排序")
        .set_id("losers_0_" + flip_c + "_" + game)
        .set_style(dpp::cos_primary);
    next.set_type(dpp::cot_button).set_label("▶")
        .set_id("losers_" + std::to_string(page+1) + "_" + dir_c + "_" + game)
        .set_style(dpp::cos_secondary).set_disabled(page == total_pages-1);
    nav_row.add_component(prev); nav_row.add_component(sort_btn); nav_row.add_component(next);
    msg.add_component(nav_row);

    // 範圍切換：🌐 全部 / 🏠 本伺服器（私訊時不顯示）
    // 注意：id 用獨立的 "lsc_" 前綴，不可重用 "losers_0_<dir>_<game>"——
    // 翻到第 2 頁時，上一頁按鈕的 id 正好也是 "losers_0_<dir>_<game>"，
    // 兩個按鈕 id 撞在一起會被 Discord 拒絕，整個翻頁就會顯示「互動失敗」。
    if (current_guild != 0) {
        std::string a = asc ? "a" : "d", A = asc ? "A" : "D";
        dpp::component scope_row; scope_row.set_type(dpp::cot_action_row);
        scope_row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🌐 全部")
            .set_id("lsc_" + a + "_" + game)
            .set_style(!scoped ? dpp::cos_primary : dpp::cos_secondary).set_disabled(!scoped));
        scope_row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🏠 本伺服器")
            .set_id("lsc_" + A + "_" + game)
            .set_style(scoped ? dpp::cos_primary : dpp::cos_secondary).set_disabled(scoped));
        msg.add_component(scope_row);
    }

    return msg;
}

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
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + sid).set_style(dpp::cos_secondary));

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

    // 轉盤
    int er_w = 0, er_l = 0; int64_t er_profit_v = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = euroulette_stats_data.find(uid);
        if (it != euroulette_stats_data.end()) {
            er_w = it->second.wins; er_l = it->second.losses; er_profit_v = it->second.profit;
        }
    }
    int er_total = er_w + er_l;
    if (er_total > 0) {
        content += "**🎡 轉盤** 中獎/未中 **" + std::to_string(er_w) + "/" + std::to_string(er_l) + "**"
            + "　勝率 **" + fmt_rate(er_w, er_total) + "**　盈虧 **" + fmt_profit(er_profit_v) + "**\n";
    } else {
        content += "**🎡 轉盤** 尚無紀錄\n";
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
    content += "**🏆 世界賽下注** " + lol_stats_line(uid) + "\n";
    content += "**🐂 妞妞** " + nn_stats_line(uid) + "\n";
    content += "**💣 數字炸彈** " + nb_stats_line(uid) + "\n";

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

    // 摸彩
    LotteryStats lot;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = lottery_stats_data.find(uid);
        if (it != lottery_stats_data.end()) lot = it->second;
    }
    if (lot.draws > 0) {
        content += "**🎰 摸彩** 摸彩/中獎 **" + std::to_string(lot.draws) + "/" + std::to_string(lot.wins) + "**"
            + "　中獎率 **" + fmt_rate(lot.wins, lot.draws) + "**　累積獲得 **+" + std::to_string(lot.chips_won) + " 碼**\n";
    } else {
        content += "**🎰 摸彩** 尚無紀錄\n";
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
