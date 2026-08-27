#include "team.h"
#include "giveaway.h"
#include "bjstats.h"
// blackjack.h → moved to handlers_bj.cpp
#include "dice.h"
#include "pet.h"
#include "shop.h"
#include "help.h"
#include "warn.h"
#include "persistence.h"
#include "wolfplayerstats.h"
#include "onwstats.h"
// monster.h moved to handlers_hunt.cpp
#include "shoot.h"
#include "rocket.h"
#include "scroll.h"
#include "scratch.h"
#include "wallet.h"
#include "bank.h"
// raid.h, darkdragon.h, undercover.h → moved to handlers_raid.cpp / handlers_uc.cpp
#include "ucstats.h"
#include "guess.h"
#include "rl_stats.h"
#include "adventure.h"
#include "rps.h"
#include "enhance.h"
#include "stock.h"
#include "announcement.h"
#include "handler_decls.h"

// ─── Trade helpers ────────────────────────────────────────────────────────────

// Unified item lookup by numeric ID — checks virtual items, gacha equipment, then stocks
static std::pair<std::string,std::string> trade_item_info(int id) {
    if (!id) return {"",""};
    if (auto* vi = find_virtual_item_by_id(id)) return {vi->key, vi->name};
    if (auto* gi = find_gacha_item_by_id(id))   return {gi->key, gi->name};
    if (auto* sd = find_stock_def_by_id(id))    return {sd->key, sd->name};
    return {"",""};
}

static bool trade_is_stock(const std::string& key) { return key.rfind("stock_", 0) == 0; }

// 是否持有至少 qty 個道具／股數。呼叫前不可持有 data_mutex（自己上鎖）。
static bool trade_has_item(dpp::snowflake uid, const std::string& key, int64_t qty) {
    std::lock_guard<std::mutex> lk(data_mutex);
    if (trade_is_stock(key)) {
        auto pit = player_stocks.find(uid);
        if (pit == player_stocks.end()) return false;
        auto hit = pit->second.find(key);
        return hit != pit->second.end() && hit->second.shares >= qty;
    }
    auto it = inventory_data.find(uid);
    return it != inventory_data.end() && it->second.count(key) && it->second.at(key) >= qty;
}

// 執行道具／股票的轉移。呼叫前必須持有 data_mutex。
static void trade_transfer_item(dpp::snowflake from_uid, dpp::snowflake to_uid, const std::string& key, int64_t qty) {
    if (trade_is_stock(key)) {
        auto& fh = player_stocks[from_uid][key];
        auto& th = player_stocks[to_uid][key];
        int64_t total_cost = th.avg_cost * th.shares + fh.avg_cost * qty;
        th.shares += qty;
        th.avg_cost = th.shares > 0 ? total_cost / th.shares : 0;
        fh.shares -= qty;
        if (fh.shares <= 0) { fh.shares = 0; fh.avg_cost = 0; }
        return;
    }
    inventory_data[from_uid][key] -= qty;
    inventory_data[to_uid][key]   += qty;
    if (key == "col_rd_lovebook") inventory_data[from_uid]["_lovebook_unlocked"] = 0;
    if (key == "recover_fatigue_cursed") { // 交易出去後變回一般高級強效咖啡
        inventory_data[to_uid]["recover_fatigue_cursed"] -= qty;
        inventory_data[to_uid]["recover_fatigue"]         += qty;
    }
}

// 是否禁止交易此道具。貓哥的戀愛教典預設不可交易，需先在背包「特殊」分頁付 2000 碼解鎖一次。
// 呼叫前不可持有 data_mutex（自己上鎖）。
static bool trade_item_blocked(dpp::snowflake uid, const std::string& key) {
    if (key == "orb_ticket") return true;
    if (key == "col_rd_lovebook") {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid);
        if (it != inventory_data.end()) {
            auto jt = it->second.find("_lovebook_unlocked");
            if (jt != it->second.end() && jt->second > 0) return false;
        }
        return true;
    }
    return false;
}

static dpp::message make_trade_msg(const TradeOffer& t,
                                   const std::string& from_name,
                                   const std::string& to_name,
                                   const std::string& status = "") {
    auto item_desc = [](int id, int64_t qty) -> std::string {
        auto [key, name] = trade_item_info(id);
        std::string s = std::string("`") + std::to_string(id) + "` " + (name.empty() ? "未知道具" : name);
        if (qty > 1) s += " ×" + std::to_string(qty);
        return s;
    };

    std::string desc;
    desc += from_name + " 向 " + to_name + " 提出交易\n\n";

    auto chips_with_fee = [](int64_t chips, dpp::snowflake payer_uid) -> std::string {
        if (chips <= 0) return "";
        bool has_lovebook = false;
        { std::lock_guard<std::mutex> lk(data_mutex); has_lovebook = col_has_lovebook(payer_uid); }
        if (has_lovebook)
            return "• 💰 " + std::to_string(chips) + " 籌碼（戀愛教典：免手續費）\n";
        int64_t fee = (chips + 99) / 100;
        return "• 💰 " + std::to_string(chips) + " 籌碼（含 1% 手續費 " + std::to_string(fee) + " 碼，實付 " + std::to_string(chips + fee) + " 碼）\n";
    };

    desc += "**" + from_name + " 提供：**\n";
    bool from_empty = (!t.from_item_id && t.from_chips <= 0);
    if (t.from_item_id) {
        desc += "• " + item_desc(t.from_item_id, t.from_qty) + "\n";
        auto [from_key, from_iname2] = trade_item_info(t.from_item_id);
        if (col_would_break_set(t.from_uid, from_key))
            desc += "　⚠️ 交易後 " + from_name + " 的收藏套組加成將會失效！\n";
    }
    if (t.from_chips > 0) desc += chips_with_fee(t.from_chips, t.from_uid);
    if (from_empty) desc += "• （無）\n";

    desc += "\n**" + to_name + " 提供：**\n";
    bool to_empty = (!t.to_item_id && t.to_chips <= 0);
    if (t.to_item_id) {
        desc += "• " + item_desc(t.to_item_id, t.to_qty) + "\n";
        auto [to_key, to_iname2] = trade_item_info(t.to_item_id);
        if (col_would_break_set(t.to_uid, to_key))
            desc += "　⚠️ 交易後 " + to_name + " 的收藏套組加成將會失效！\n";
    }
    if (t.to_chips > 0) desc += chips_with_fee(t.to_chips, t.to_uid);
    if (to_empty) desc += "• （無）\n";

    if (status.empty()) desc += "\n⏳ 等待 " + to_name + " 確認...";
    else if (status == "ok") desc += "\n✅ 交易完成！";
    else if (status == "rej") desc += "\n❌ 交易已拒絕。";
    else if (!status.empty()) desc += "\n❌ " + status;

    uint32_t color = status.empty() ? 0x3498DB : (status == "ok" ? 0x2ECC71 : 0xE74C3C);

    dpp::embed e;
    e.set_title("🔄  交易提案").set_color(color).set_description(desc);
    dpp::message msg; msg.add_embed(e);

    if (status.empty()) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        std::string tid_s = std::to_string(t.id);
        dpp::component acc, rej;
        acc.set_type(dpp::cot_button).set_label("✅ 接受")
           .set_id("trade_acc_" + tid_s).set_style(dpp::cos_success);
        rej.set_type(dpp::cot_button).set_label("❌ 拒絕")
           .set_id("trade_rej_" + tid_s).set_style(dpp::cos_danger);
        row.add_component(acc); row.add_component(rej);
        msg.add_component(row);
    }
    return msg;
}

// ─── 管理員：查詢／強制中斷玩家卡住的遊戲 ───────────────────────────────────────
// 掃過幾個比較容易卡住的長時遊戲系統（怪物狩獵、村落挑戰、探險、組隊房間/戰鬥、暗黑龍王）。
// 不含 21點/骰子/刮刮樂/猜拳等單次互動就會結束的小遊戲——那些不太會卡住，暫時不列入。
static dpp::message make_admin_kill_report_msg(dpp::snowflake target, const std::string& notice = "") {
    std::string uid_s = std::to_string((uint64_t)target);
    struct Found { std::string btn_id, desc; };
    std::vector<Found> found;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto hit = monster_hunt_games.find(target);
        if (hit != monster_hunt_games.end())
            found.push_back({"adminkill_hunt_" + uid_s,
                "🗡️ 怪物狩獵：" + hit->second.monster_name + "（" + hit->second.difficulty + "）"});
        auto vit = village_games.find(target);
        if (vit != village_games.end())
            found.push_back({"adminkill_village_" + uid_s, "🏘️ 村落挑戰：" + vit->second.group_key});
        // 探險不列入：跟寵物打工一樣是背景進行，沒有卡住的互動視窗，不需要強制中斷
        for (auto& [rch, room] : raid_rooms) {
            bool in_room = room.host_uid == target ||
                std::find(room.member_uids.begin(), room.member_uids.end(), target) != room.member_uids.end();
            if (in_room)
                found.push_back({"adminkill_raidroom_" + uid_s + "_" + std::to_string((uint64_t)rch),
                    "🚪 組隊房間（" + room.boss_key + "），頻道 <#" + std::to_string((uint64_t)rch) + ">"});
        }
        for (auto& [rch, g] : raid_games) {
            bool in = false; for (auto& p : g.players) if (p.uid == target) { in = true; break; }
            if (in) found.push_back({"adminkill_raid_" + uid_s + "_" + std::to_string((uint64_t)rch),
                "⚔️ 組隊戰鬥（" + g.boss_name + "），頻道 <#" + std::to_string((uint64_t)rch) + ">"});
        }
        for (auto& [rch, g] : dd_games) {
            bool in = false; for (auto& p : g.players) if (p.uid == target) { in = true; break; }
            if (in) found.push_back({"adminkill_dd_" + uid_s + "_" + std::to_string((uint64_t)rch),
                "🐉 暗黑龍王戰鬥，頻道 <#" + std::to_string((uint64_t)rch) + ">"});
        }
        // 小遊戲：單次互動很快結束，但沒有逾時清理機制，放著不理一樣會累積成沒人在用的視窗
        if (auto ubj = user_bj.find(target); ubj != user_bj.end()) {
            auto git = bj_games.find(ubj->second);
            if (git != bj_games.end())
                found.push_back({"adminkill_bj_" + uid_s, "🃏 21點，賭注 " + std::to_string(git->second.bet) + " 碼"});
        }
        if (auto udc = user_dice.find(target); udc != user_dice.end()) {
            auto git = dice_games.find(udc->second);
            if (git != dice_games.end())
                found.push_back({"adminkill_dice_" + uid_s, "🎲 骰子，賭注 " + std::to_string(git->second.bet) + " 碼"});
        }
        if (auto sit = shoot_games.find(target); sit != shoot_games.end())
            found.push_back({"adminkill_shoot_" + uid_s, "🎯 射龍門，賭注 " + std::to_string(sit->second.bet) + " 碼"});
        if (auto rit = rocket_games.find(target); rit != rocket_games.end())
            found.push_back({"adminkill_rocket_" + uid_s, "🚀 火箭升空，賭注 " + std::to_string(rit->second.bet) + " 碼"});
        if (auto scit = scratch_games.find(target); scit != scratch_games.end())
            found.push_back({"adminkill_scratch_" + uid_s, "🎫 刮刮樂，花費 " + std::to_string(scit->second.total_paid) + " 碼"});
        if (auto git = guess_games.find(target); git != guess_games.end())
            found.push_back({"adminkill_guess_" + uid_s, "🔢 猜數字，已猜 " + std::to_string(git->second.attempts) + " 次"});
        for (auto& [rch, g] : rps_games) {
            if (g.players.count(target))
                found.push_back({"adminkill_rps_" + uid_s + "_" + std::to_string((uint64_t)rch),
                    "✊ 猜拳，頻道 <#" + std::to_string((uint64_t)rch) + ">"});
        }
        for (auto& [rch, room] : roulette_rooms) {
            if (room.p1_uid == target || room.p2_uid == target || room.invited_uid == target)
                found.push_back({"adminkill_roulette_" + uid_s + "_" + std::to_string((uint64_t)rch),
                    "🔫 俄羅斯輪盤，頻道 <#" + std::to_string((uint64_t)rch) + ">"});
        }
        for (auto& [rch, gid] : channel_onw_game) {
            auto git = onw_games.find(gid); if (git == onw_games.end()) continue;
            bool in = git->second.host_id == target;
            for (auto& p : git->second.players) if (p.uid == target) { in = true; break; }
            if (in) found.push_back({"adminkill_onw_" + uid_s + "_" + std::to_string((uint64_t)rch),
                "🐺 一夜狼人，頻道 <#" + std::to_string((uint64_t)rch) + ">"});
        }
        for (auto& [rch, gid] : channel_uc_game) {
            auto git = uc_games.find(gid); if (git == uc_games.end()) continue;
            bool in = git->second.host_id == target;
            for (auto& p : git->second.players) if (p.uid == target) { in = true; break; }
            if (in) found.push_back({"adminkill_uc_" + uid_s + "_" + std::to_string((uint64_t)rch),
                "🕵️ 誰是臥底，頻道 <#" + std::to_string((uint64_t)rch) + ">"});
        }
    }

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    std::string header = "## 🔍 <@" + uid_s + "> 目前的遊戲狀態";
    if (!notice.empty()) header = "✅ " + notice + "\n\n" + header;
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(header));

    dpp::message m; m.set_flags(dpp::m_using_components_v2);
    if (found.empty()) {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("沒有找到任何進行中的遊戲。"));
    } else {
        for (auto& f : found) {
            container.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(f.desc))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label("🛑 中斷").set_id(f.btn_id).set_style(dpp::cos_danger)));
        }
    }
    m.add_component_v2(container);
    return m;
}

// 清掉某頻道／討論串裡全部進行中的遊戲（怪物狩獵、村落挑戰、組隊房間、組隊戰鬥、暗黑龍王、
// 21點、骰子、射龍門、火箭升空、刮刮樂、猜數字、猜拳、俄羅斯輪盤、一夜狼人、誰是臥底），回傳清了幾筆。
// 只清「進行中的那一局」，不會動到累計勝負/籌碼盈虧等統計資料。
// 探險不列入：跟寵物打工一樣是背景進行、沒有卡住的互動視窗，讓它繼續跑。
static int admin_kill_channel(dpp::snowflake ch) {
    int n = 0;
    std::vector<dpp::timer> timers;
    bool touched_scratch = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto it = monster_hunt_games.begin(); it != monster_hunt_games.end(); ) {
            if (it->second.channel_id == ch) {
                if (it->second.timer_id) timers.push_back(it->second.timer_id);
                it = monster_hunt_games.erase(it); n++;
            } else ++it;
        }
        for (auto it = village_games.begin(); it != village_games.end(); ) {
            if (it->second.channel_id == ch) {
                if (it->second.timer_id) timers.push_back(it->second.timer_id);
                it = village_games.erase(it); n++;
            } else ++it;
        }
        if (auto it = raid_rooms.find(ch); it != raid_rooms.end()) {
            if (it->second.timer_id) timers.push_back(it->second.timer_id);
            raid_rooms.erase(it); n++;
        }
        if (auto it = raid_games.find(ch); it != raid_games.end()) {
            if (it->second.timer_id) timers.push_back(it->second.timer_id);
            raid_games.erase(it); n++;
        }
        if (auto it = dd_games.find(ch); it != dd_games.end()) {
            if (it->second.timer_id) timers.push_back(it->second.timer_id);
            dd_games.erase(it); n++;
        }
        for (auto it = bj_games.begin(); it != bj_games.end(); ) {
            if (it->second.channel_id == ch) { user_bj.erase(it->second.user_id); it = bj_games.erase(it); n++; }
            else ++it;
        }
        for (auto it = dice_games.begin(); it != dice_games.end(); ) {
            if (it->second.ch == ch) { user_dice.erase(it->second.uid); it = dice_games.erase(it); n++; }
            else ++it;
        }
        for (auto it = shoot_games.begin(); it != shoot_games.end(); ) {
            if (it->second.channel_id == ch) { it = shoot_games.erase(it); n++; } else ++it;
        }
        for (auto it = rocket_games.begin(); it != rocket_games.end(); ) {
            if (it->second.channel_id == ch) { it = rocket_games.erase(it); n++; } else ++it;
        }
        for (auto it = scratch_games.begin(); it != scratch_games.end(); ) {
            if (it->second.channel_id == ch) { it = scratch_games.erase(it); n++; touched_scratch = true; } else ++it;
        }
        for (auto it = guess_games.begin(); it != guess_games.end(); ) {
            if (it->second.channel_id == ch) { it = guess_games.erase(it); n++; } else ++it;
        }
        if (rps_games.erase(ch)) n++;
        if (auto it = roulette_rooms.find(ch); it != roulette_rooms.end()) {
            if (it->second.timer_id) timers.push_back(it->second.timer_id);
            roulette_rooms.erase(it); n++;
        }
        if (auto it = channel_onw_game.find(ch); it != channel_onw_game.end()) {
            onw_games.erase(it->second); channel_onw_game.erase(it); n++;
        }
        if (auto it = channel_uc_game.find(ch); it != channel_uc_game.end()) {
            uc_games.erase(it->second); channel_uc_game.erase(it); n++;
        }
    }
    for (auto t : timers) g_bot->stop_timer(t);
    if (touched_scratch) save_scratch_games();
    return n;
}

// 清掉全伺服器、每個人正在進行的遊戲，回傳清了幾筆。範圍跟 admin_kill_channel 一樣，只是不限頻道。
// 同樣只清「進行中的那一局」，不會動到累計勝負/籌碼盈虧等統計資料。
// 探險不列入：跟寵物打工一樣是背景進行、沒有卡住的互動視窗，讓它繼續跑。
static int admin_kill_everything() {
    int n = 0;
    std::vector<dpp::timer> timers;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, g] : monster_hunt_games) if (g.timer_id) timers.push_back(g.timer_id);
        n += (int)monster_hunt_games.size(); monster_hunt_games.clear();
        for (auto& [k, g] : village_games) if (g.timer_id) timers.push_back(g.timer_id);
        n += (int)village_games.size(); village_games.clear();
        for (auto& [k, room] : raid_rooms) if (room.timer_id) timers.push_back(room.timer_id);
        n += (int)raid_rooms.size(); raid_rooms.clear();
        for (auto& [k, g] : raid_games) if (g.timer_id) timers.push_back(g.timer_id);
        n += (int)raid_games.size(); raid_games.clear();
        for (auto& [k, g] : dd_games) if (g.timer_id) timers.push_back(g.timer_id);
        n += (int)dd_games.size(); dd_games.clear();
        n += (int)bj_games.size(); bj_games.clear(); user_bj.clear();
        n += (int)dice_games.size(); dice_games.clear(); user_dice.clear();
        n += (int)shoot_games.size(); shoot_games.clear();
        n += (int)rocket_games.size(); rocket_games.clear();
        n += (int)scratch_games.size(); scratch_games.clear();
        n += (int)guess_games.size(); guess_games.clear();
        n += (int)rps_games.size(); rps_games.clear();
        for (auto& [k, room] : roulette_rooms) if (room.timer_id) timers.push_back(room.timer_id);
        n += (int)roulette_rooms.size(); roulette_rooms.clear();
        n += (int)channel_onw_game.size(); channel_onw_game.clear(); onw_games.clear();
        n += (int)channel_uc_game.size(); channel_uc_game.clear(); uc_games.clear();
    }
    for (auto t : timers) g_bot->stop_timer(t);
    save_scratch_games(); // 刮刮樂有落地存檔，清空後要記得存檔
    return n;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Always use the exe's directory so data files go to the same place regardless of launch path
    if (argc > 0) std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());

    cfg = load_config();
    if (cfg.token.empty()) { fprintf(stderr, "找不到 BOT_TOKEN\n"); return 1; }
    load_chips();
    load_bank();
    load_warns();
    load_bjstats();
    load_dicestats();
    load_shootstats();
    load_rocketstats();
    load_scratchstats();
    load_wolf_player_stats();
    load_onw_stats();
    load_bj_games();
    load_dice_games();
    load_shop();
    load_purchases();
    load_pet_data();
    load_inventory();
    load_registrations();
    load_proposed_teams();
    load_giveaways();
    load_equipped();
    load_hunt_clear();
    load_adv_games();
    load_dogbook();
    load_gacha_pity();
    load_gacha_hero_pity();
    load_uc_stats();
    load_guess_stats();
    load_roulettestats();
    load_rps_stats();
    load_scratch_games();
    load_stock_market();
    load_stock_holdings();
    load_announcement();

    dpp::cluster bot(cfg.token, dpp::i_default_intents | dpp::i_message_content);
    g_bot = &bot;
    bot.on_log(dpp::utility::cout_logger());

    // ── 訊息指令 ──────────────────────────────────────────────────────────────
    bot.on_message_create([&bot](const dpp::message_create_t& ev) {
        // 擋掉所有機器人帳號（含其他 bot／webhook），避免被拿來自動洗 !領取 等指令
        if (ev.msg.author.is_bot()) return;

        // Normalize full-width ！ (U+FF01, UTF-8: EF BC 81) to ASCII !
        std::string content = ev.msg.content;
        { const std::string fw = "\xEF\xBC\x81"; size_t p;
          while ((p = content.find(fw)) != std::string::npos) content.replace(p, 3, "!"); }
        dpp::snowflake     uid     = ev.msg.author.id;
        dpp::snowflake     ch      = ev.msg.channel_id;

        if (content.empty() || content[0] != '!') return;

        // Only delete messages that are recognized bot commands
        auto is_our_cmd = [&]() -> bool {
            static const std::vector<std::string> EXACT = {
                "!王團報名","!王團紀錄","!富豪榜","!虧損榜","!領取","!每週領取",
                "!錢包","!幫助","!help","!寵物","!背包","!寵物圖鑑","!商店","!大廳",
                "!管理員權限","!警告榜單","!記帳","!狼人殺","!狼人殺榜單","!銀行",
                "!一夜狼人","!一夜狼人規則","!狼人殺規則",
                "!臥底","!誰是臥底",
                "!臥底 遊玩成人內容","!誰是臥底 遊玩成人內容",
                "!貓","!笑話","!轉蛋","!裝備","!怪物狩獵","!狩獵規則",
                "!道具圖鑑","!裝備圖鑑","!合成","!收藏","!輪盤","!探險","!猜拳","！猜拳","!強化","!股票",
                "!公告","！公告","!小黑屋","！小黑屋"
            };
            for (auto& s : EXACT) if (content == s) return true;
            // Secret owner-only command
            if (content == "!偷看" && !cfg.notify_user_id.empty() &&
                std::to_string(uid) == cfg.notify_user_id) return true;
            // Prefix-match commands (with args)
            static const std::vector<std::string> PREFIX = {
                "!21 ","!骰子 ","!射 ","!火箭 ","!刮 ","!猜 ",
                "!幸運頻道 ","!警告 ","!轉帳 ","!交易 ","!卷軸使用 ","!輪盤 ","!猜拳 ","！猜拳 ",
                "!公告 ","！公告 ",
            };
            for (auto& s : PREFIX) if (content.rfind(s, 0) == 0) return true;
            // standalone (no args)
            if (content == "!21" || content == "!骰子" || content == "!射" ||
                content == "!火箭" || content == "!刮" || content == "!猜" ||
                content == "!幸運頻道" || content == "!警告" || content == "!轉帳" ||
                content == "!交易" || content == "!卷軸使用") return true;
            return false;
        };
        if (!is_our_cmd()) return;
        if (ev.msg.guild_id != 0) bot.message_delete(ev.msg.id, ch); // 隱藏使用者輸入的 ! 指令（DM 不刪）

        if (content == "!王團報名") {
            start_cmd(bot, uid, ch, make_boss_msg(ev.msg.author), ev.msg.id);
        }
        else if (content == "!王團紀錄") {
            start_cmd(bot, uid, ch, make_records_select_msg(ev.msg.author), ev.msg.id);
        }
        else if (content == "!ping") {
            ev.reply("Pong! 🏓");
        }
        else if (content == "!emoji偵錯") {
            std::ostringstream oss;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                oss << "emoji_tag_map 共 **" << emoji_tag_map.size() << "** 筆\n";
                int shown = 0;
                for (auto& [id, tag] : emoji_tag_map) {
                    if (shown++ >= 20) { oss << "...（只顯示前20筆）"; break; }
                    oss << "`" << tag << "`\n";
                }
            }
            dpp::message m; m.set_content(oss.str());
            m.set_reference(ev.msg.id); m.channel_id = ch;
            bot.message_create(m);
        }
        else if (content == "!幫助") {
            dpp::message m = make_help_msg(0);
            m.set_reference(ev.msg.id); m.channel_id = ch;
            bot.message_create(m);
        }
        else if (content == "!錢包") {
            dpp::message m = make_wallet_home_msg(uid);
            m.channel_id = ch;
            bot.message_create(m);
        }
        else if (content == "!富豪榜") {
            dpp::message m = handle_leaderboard(0);
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!虧損榜") {
            dpp::message m = handle_losers_board(0, "");
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!銀行") {
            dpp::message m = make_bank_msg(uid);
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!領取") {
            bool claimed = false, challenged = false; uint64_t claim_token = 0;
            dpp::message m = handle_claim(uid, &claimed, &challenged, &claim_token);
            if (claimed) {
                int64_t repaid = bank_auto_repay(uid, CLAIM_AMOUNT / 2);
                if (repaid > 0) {
                    save_chips(); save_bank();
                    if (!m.embeds.empty())
                        m.embeds[0].add_field("💳 自動還款", std::to_string(repaid) + " 碼已從本次領取扣除", false);
                }
            }
            // Daily hunt scroll grant — independent of chip claim result
            {
                bool gave_scrolls = false;
                bool updated_daily = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto& cd = chip_data[uid];
                    time_t now2 = time(nullptr);
                    struct tm now_tm{}, last_tm{};
                    localtime_s(&now_tm, &now2);
                    localtime_s(&last_tm, &cd.last_hunt_daily);
                    bool new_day = (now_tm.tm_year != last_tm.tm_year || now_tm.tm_yday != last_tm.tm_yday);
                    if (new_day) {
                        int cur_scrolls = inventory_data[uid].count("hunt_scroll")
                                          ? inventory_data[uid]["hunt_scroll"] : 0;
                        int max_scrolls = 2;
                        // 呀呀的懸賞令：每日+1狩獵卷上限
                        if (inventory_data[uid].count("col_yaya_bounty") && inventory_data[uid]["col_yaya_bounty"] > 0)
                            max_scrolls = 3;
                        int to_give = std::max(0, max_scrolls - cur_scrolls);
                        if (to_give > 0) {
                            inventory_data[uid]["hunt_scroll"] += to_give;
                            gave_scrolls = true;
                        }
                        cd.last_hunt_daily = now2;
                        updated_daily = true;
                    }
                }
                if (gave_scrolls) {
                    save_chips(); save_inventory();
                    int cur_after = 0;
                    { std::lock_guard<std::mutex> lk2(data_mutex);
                      cur_after = inventory_data[uid].count("hunt_scroll") ? inventory_data[uid]["hunt_scroll"] : 0; }
                    std::string scroll_msg = "已補滿至 **2 張**（上限）";
                    if (!m.embeds.empty()) {
                        m.embeds[0].add_field("📜 每日狩獵卷", scroll_msg, false);
                    } else {
                        dpp::embed se; se.set_color(0x27AE60);
                        se.set_description("📜 " + scroll_msg);
                        m.embeds.push_back(se);
                    }
                } else if (updated_daily) {
                    save_chips(); // persist last_hunt_daily even when player already had max scrolls
                }
            }
            m.channel_id = ch;
            if (challenged) {
                // 驗證按鈕還沒按，先不要自動刪除訊息；改排一個逾時鎖定計時器
                bot.message_create(m, [uid, ch, claim_token](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        dpp::snowflake mid = std::get<dpp::message>(cb.value).id;
                        { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[mid] = uid; }
                        schedule_claim_verify_timeout(uid, ch, mid, claim_token);
                    }
                });
            } else {
                bot.message_create(m, [&bot, ch](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        dpp::snowflake mid = std::get<dpp::message>(cb.value).id;
                        bot.start_timer([&bot, mid, ch](dpp::timer t) {
                            bot.message_delete(mid, ch); bot.stop_timer(t);
                        }, 15);
                    }
                });
            }
        }
        else if (content == "!每週領取") {
            bool claimed = false;
            dpp::message m = handle_weekly_claim(uid, &claimed);
            if (claimed) {
                int64_t repaid = bank_auto_repay(uid, WEEKLY_AMOUNT / 2);
                if (repaid > 0) {
                    save_chips(); save_bank();
                    if (!m.embeds.empty())
                        m.embeds[0].add_field("💳 自動還款", std::to_string(repaid) + " 碼已從本次領取扣除", false);
                }
            }
            m.channel_id = ch;
            bot.message_create(m, [&bot, ch](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    dpp::snowflake mid = std::get<dpp::message>(cb.value).id;
                    bot.start_timer([&bot, mid, ch](dpp::timer t) {
                        bot.message_delete(mid, ch); bot.stop_timer(t);
                    }, 15);
                }
            });
        }
        else if (content == "!商店") {
            dpp::message m = make_shop_main_msg(std::to_string((uint64_t)uid));
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!記帳") {
            bool is_adm = is_draw_authorized_msg(uid, ev.msg.member.get_roles());
            dpp::message m = is_adm ? make_ledger_msg(0) : make_my_ledger_msg(uid);
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!寵物") {
            dpp::message msg = make_pet_view_msg(uid,
                ev.msg.author.get_avatar_url(),
                ev.msg.member.get_nickname());
            msg.channel_id = ch;
            bot.message_create(msg, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!大廳") {
            dpp::message msg = make_lobby_msg(uid,
                ev.msg.author.get_avatar_url(),
                ev.msg.member.get_nickname());
            msg.channel_id = ch;
            bot.message_create(msg, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!收藏") {
            std::string dn_ = ev.msg.member.get_nickname().empty() ? ev.msg.author.global_name : ev.msg.member.get_nickname();
            std::string av_ = ev.msg.author.get_avatar_url();
            dpp::message cm = make_collection_msg(uid, dn_, av_);
            cm.channel_id = ch;
            bot.message_create(cm, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[std::get<dpp::message>(cb.value).id] = uid; }
            });
        }
        else if (content == "!探險" || content == "！探險") {
            std::string dn_ = ev.msg.member.get_nickname().empty() ? ev.msg.author.global_name : ev.msg.member.get_nickname();
            std::string av_ = ev.msg.author.get_avatar_url();
            dpp::message am = make_adv_main_msg(uid, dn_, av_);
            am.channel_id = ch;
            bot.message_create(am, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[std::get<dpp::message>(cb.value).id] = uid; }
            });
        }
        else if (content == "!強化") {
            std::string dn_ = ev.msg.member.get_nickname().empty() ? ev.msg.author.global_name : ev.msg.member.get_nickname();
            std::string av_ = ev.msg.author.get_avatar_url();
            dpp::message em = make_enhance_main_msg(uid, dn_, av_);
            em.channel_id = ch;
            bot.message_create(em, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[std::get<dpp::message>(cb.value).id] = uid; }
            });
        }
        else if (content == "!股票") {
            std::string dn_ = ev.msg.member.get_nickname().empty() ? ev.msg.author.global_name : ev.msg.member.get_nickname();
            std::string av_ = ev.msg.author.get_avatar_url();
            dpp::message sm = make_stock_home_msg(uid, dn_, av_);
            sm.channel_id = ch;
            bot.message_create(sm, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[std::get<dpp::message>(cb.value).id] = uid; }
            });
        }
        else if (content == "!背包") {
            std::string dn_ = ev.msg.member.get_nickname();
            dpp::message msg = make_bag_home_msg(uid, dn_, ev.msg.author.get_avatar_url());
            msg.channel_id = ch;
            bot.message_create(msg, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!寵物圖鑑") {
            dpp::message msg = make_petdex_msg("嫩寶");
            msg.channel_id = ch;
            bot.message_create(msg, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!管理員權限") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                dpp::message m; m.set_content("❌ 沒有權限！"); m.channel_id = ch;
                bot.message_create(m); return;
            }
            dpp::component row; row.set_type(dpp::cot_action_row);
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("💰 調整碼數").set_id("admin_chip_modal_btn").set_style(dpp::cos_primary));
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🎒 給道具").set_id("admin_item_btn").set_style(dpp::cos_secondary));
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🛑 中斷遊戲").set_id("admin_kill_btn").set_style(dpp::cos_danger));
            dpp::component row2; row2.set_type(dpp::cot_action_row);
            row2.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🧹 清除本頻道遊戲").set_id("admin_clear_channel_btn").set_style(dpp::cos_danger));
            row2.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🧨 清除全部遊戲").set_id("admin_clear_all_btn").set_style(dpp::cos_danger));
            dpp::component row3; row3.set_type(dpp::cot_action_row);
            row3.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(g_claim_verify_enabled ? "🔒 領取驗證：開啟中" : "🔓 領取驗證：已關閉")
                .set_id("admin_toggle_verify_btn")
                .set_style(g_claim_verify_enabled ? dpp::cos_success : dpp::cos_secondary));
            dpp::message m;
            m.set_content("🔑 **管理員面板**\n請選擇操作：");
            m.add_component(row); m.add_component(row2); m.add_component(row3); m.channel_id = ch;
            bot.message_create(m);
        }
        // !小黑屋／！小黑屋：查看／解除領取驗證的鎖定（限管理員）
        else if (content == "!小黑屋" || content == "！小黑屋") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                dpp::message m; m.set_content("❌ 沒有權限！"); m.channel_id = ch;
                bot.message_create(m); return;
            }
            dpp::message m = make_claim_jail_msg();
            m.channel_id = ch;
            bot.message_create(m);
        }
        // !公告／！公告：不帶參數＝查看，帶參數＝設定（限管理員/副會長）
        else if (content == "!公告" || content == "！公告" ||
                 content.rfind("!公告 ", 0) == 0 || content.rfind("！公告 ", 0) == 0) {
            size_t sp = content.find(' ');
            std::string text = (sp == std::string::npos) ? "" : content.substr(sp + 1);
            dpp::message m;
            if (text.empty()) {
                m = make_announcement_view_msg();
            } else if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                m.set_content("❌ 沒有權限設定公告！");
            } else {
                std::string dn = ev.msg.member.get_nickname().empty()
                               ? ev.msg.author.username : ev.msg.member.get_nickname();
                m.set_content(set_announcement(text, dn));
            }
            m.channel_id = ch; bot.message_create(m);
        }
        // ── 骰子/射/火箭/卷軸/刮刮樂/猜數字 → handlers_games.cpp ───────────
        else if (content.rfind("!骰子", 0) == 0 ||
                 (content.rfind("!射", 0) == 0 && (content.size() == 4 || content[4] == ' ')) ||
                 (content.rfind("!火箭", 0) == 0 && (content.size() == 7 || content[7] == ' ')) ||
                 (content.rfind("!卷軸使用", 0) == 0 && (content.size() == 13 || content[13] == ' ')) ||
                 (content.rfind("!刮", 0) == 0 && (content.size() == 4 || content[4] == ' ')) ||
                 ((content.rfind("!猜", 0) == 0 || content.rfind("！猜", 0) == 0)
                  && content.rfind("!猜拳", 0) != 0 && content.rfind("！猜拳", 0) != 0)) {
            handle_games_message(ev, content, uid, ch); return;
        }
        // !轉帳 @mention <碼>
        else if (content.rfind("!轉帳", 0) == 0 && content.size() > 7) {
            std::string rest = content.substr(content.find(' ') + 1);
            dpp::snowflake to_uid = parse_mention(rest);
            size_t gt = rest.find('>');
            int64_t amount = 0;
            if (gt != std::string::npos) {
                try { amount = std::stoll(rest.substr(gt + 1)); } catch (...) {}
            }
            if (!to_uid || amount <= 0) {
                dpp::message m; m.set_content("用法：`!轉帳 @對象 籌碼量`  例：`!轉帳 @王小明 200`");
                m.channel_id = ch; bot.message_create(m);
            } else {
                bool has_loan_block = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = bank_data.find(uid);
                    has_loan_block = (it != bank_data.end() && it->second.loan > 0);
                }
                if (has_loan_block) {
                    dpp::message m; m.set_content("❌ 你有未還清的借款，無法轉帳！請先至 `!銀行` 還清借款。");
                    m.channel_id = ch; bot.message_create(m);
                } else {
                    std::string from_name = "<@" + std::to_string((uint64_t)uid)    + ">";
                    std::string to_name   = "<@" + std::to_string((uint64_t)to_uid) + ">";
                    dpp::message m = handle_transfer_request(uid, from_name, to_uid, to_name, amount);
                    m.channel_id = ch; bot.message_create(m);
                }
            }
        }
        // !警告 @mention [原因]
        else if (content.rfind("!警告", 0) == 0 && content.find('@') != std::string::npos) {
            dpp::snowflake target = parse_mention(content);
            if (!target) {
                dpp::message m; m.set_content("用法：`!警告 @對象`  或  `!警告 @對象 原因`");
                m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m);
            } else {
                size_t gt = content.find('>');
                std::string reason;
                if (gt != std::string::npos && gt + 2 < content.size())
                    reason = content.substr(gt + 2);
                std::string target_name = "<@" + std::to_string((uint64_t)target) + ">";
                dpp::message m = handle_warn(target, target_name, reason);
                m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m);
            }
        }
        else if (content == "!警告榜單") {
            dpp::message m = handle_warn_board();
            m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m);
        }
        // !幸運頻道 <最大頻道數>
        else if (content.rfind("!幸運頻道", 0) == 0) {
            std::string rest = content.size() > 9 ? content.substr(content.find(' ') + 1) : "";
            int max_ch = rest.empty() ? 0 : std::atoi(rest.c_str());
            if (max_ch < 1) {
                dpp::message m; m.set_content("用法：`!幸運頻道 最大頻道數`  例：`!幸運頻道 8`");
                m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m);
            } else {
                std::mt19937 rng(std::random_device{}());
                int lucky = std::uniform_int_distribution<int>(1, max_ch)(rng);
                dpp::embed e;
                e.set_title("🎰  幸運頻道").set_color(0xF39C12);
                e.set_description("🍀  本次幸運頻道是 **頻道 " + std::to_string(lucky) + "**！");
                dpp::message m; m.add_embed(e);
                m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m);
            }
        }
        // ── 21點 → handlers_bj.cpp ──────────────────────────────────────────
        else if (content.rfind("!21", 0) == 0 && (content.size() == 3 || content[3] == ' ')) {
            handle_bj_message(ev, content, uid, ch); return;
        }
        // ── 狼人殺 / 一夜狼人 → handlers_wolf.cpp
        else if (content == "!狼人殺" || content == "!偷看" || content == "!狼人殺榜單" ||
                 content == "!一夜狼人" || content == "!一夜狼人規則" || content == "!狼人殺規則") {
            handle_wolf_message(ev, content, uid, ch); return;
        }
        // !交易 @對象 我的道具ID 我的道具數量 我的籌碼 對方道具ID 對方道具數量 對方籌碼  (ID=0 表示不出，數量預設1)
        else if (content.rfind("!交易", 0) == 0) {
            auto trade_usage = [&]() {
                dpp::message m; m.channel_id = ch;
                m.set_content("用法：`!交易 @對象 我的道具ID 我的道具數量 我出的籌碼 對方道具ID 對方道具數量 對方出的籌碼`（不出填 0，數量預設1）\n例：`!交易 @小明 50001 1 0 50002 1 500`");
                bot.message_create(m);
            };
            dpp::snowflake target = parse_mention(content);
            size_t gt = content.find('>');
            if (!target || gt == std::string::npos) { trade_usage(); return; }
            if (target == uid) {
                dpp::message m; m.channel_id = ch;
                m.set_content("❌ 不能和自己交易！");
                bot.message_create(m); return;
            }
            std::istringstream iss(content.substr(gt + 1));
            int from_item_id = 0, to_item_id = 0;
            int64_t from_qty = 1, to_qty = 1, from_chips = 0, to_chips = 0;
            iss >> from_item_id >> from_qty >> from_chips >> to_item_id >> to_qty >> to_chips;
            if (from_qty <= 0) from_qty = 1;
            if (to_qty   <= 0) to_qty   = 1;

            // Validate sender's side
            auto [from_key, from_iname] = trade_item_info(from_item_id);
            if (from_item_id && from_key.empty()) {
                dpp::message m; m.channel_id = ch;
                m.set_content("❌ 道具 ID `" + std::to_string(from_item_id) + "` 不存在！");
                bot.message_create(m); return;
            }
            if (!from_key.empty()) {
                if (trade_item_blocked(uid, from_key)) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ **" + from_iname + "** 不可交易！");
                    bot.message_create(m); return;
                }
                if (!trade_has_item(uid, from_key, from_qty)) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ 你沒有 **" + std::to_string(from_qty) + "** 個/股 **" + from_iname + "**！");
                    bot.message_create(m); return;
                }
            }
            if (from_chips < 0) from_chips = 0;
            if (from_chips > 0) {
                bool has_lovebook_chk = false;
                { std::lock_guard<std::mutex> lk(data_mutex); has_lovebook_chk = col_has_lovebook(uid); }
                int64_t from_fee_chk = has_lovebook_chk ? 0 : (from_chips + 99) / 100;
                if (get_chips(uid) < from_chips + from_fee_chk) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ 你的籌碼不足（含 1% 手續費 " + std::to_string(from_fee_chk) + " 碼，需 " + std::to_string(from_chips + from_fee_chk) + " 碼）！");
                    bot.message_create(m); return;
                }
            }
            if (to_chips < 0) to_chips = 0;
            auto [to_key_chk, to_iname_chk] = trade_item_info(to_item_id);
            if (to_item_id && to_key_chk.empty()) {
                dpp::message m; m.channel_id = ch;
                m.set_content("❌ 對方道具 ID `" + std::to_string(to_item_id) + "` 不存在！");
                bot.message_create(m); return;
            }
            if (!to_key_chk.empty()) {
                if (trade_item_blocked(target, to_key_chk)) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ **" + to_iname_chk + "** 不可交易！");
                    bot.message_create(m); return;
                }
            }

            TradeOffer t;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                t.id = trade_counter++;
                t.from_uid = uid; t.to_uid = target; t.channel_id = ch;
                t.from_item_id = from_item_id; t.from_qty = from_qty; t.from_chips = from_chips;
                t.to_item_id   = to_item_id;   t.to_qty   = to_qty;   t.to_chips   = to_chips;
                t.created_at   = time(nullptr);
                trade_offers[t.id] = t;
            }
            std::string from_name = ev.msg.author.username;
            std::string to_name = "<@" + std::to_string((uint64_t)target) + ">";
            dpp::message m = make_trade_msg(t, from_name, to_name);
            m.channel_id = ch;
            bot.message_create(m, [uid, tid = t.id](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        // ── 誰是臥底 → handlers_uc.cpp
        else if (content == "!臥底" || content == "！臥底" ||
                 content == "!誰是臥底" || content == "！誰是臥底" ||
                 content == "!臥底 遊玩成人內容" || content == "！臥底 遊玩成人內容" ||
                 content == "!誰是臥底 遊玩成人內容" || content == "！誰是臥底 遊玩成人內容") {
            handle_uc_message(ev, content, uid, ch); return;
        }
        else if (content == "!轉蛋") {
            std::string dn = ev.msg.member.get_nickname().empty()
                           ? ev.msg.author.username : ev.msg.member.get_nickname();
            std::string av = ev.msg.author.get_avatar_url();
            dpp::message m = make_gacha_main_msg(uid, dn, av);
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!裝備") {
            std::string dn = ev.msg.member.get_nickname().empty()
                           ? ev.msg.author.username : ev.msg.member.get_nickname();
            std::string av = ev.msg.author.get_avatar_url();
            Pet pet;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = pet_data.find(uid); if (it != pet_data.end()) pet = it->second; }
            dpp::message m = make_equip_msg(uid, pet, dn, av);
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        // ── 怪物狩獵 → handlers_hunt.cpp
        else if (content == "!怪物狩獵" || content == "!狩獵規則") {
            handle_hunt_message(ev, content, uid, ch); return;
        }
        else if (content == "!道具圖鑑") {
            dpp::message m = make_itemdex_main_msg(uid);
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!裝備圖鑑") {
            dpp::message m = make_equipdex_main_msg(uid);
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        // ── 輪盤 → handlers_roulette.cpp
        else if (content.rfind("!輪盤", 0) == 0) {
            handle_roulette_message(ev, content, uid, ch); return;
        }
        // ── 猜拳
        else if (content.rfind("!猜拳", 0) == 0 || content.rfind("！猜拳", 0) == 0) {
            handle_rps_message(ev, content, uid, ch); return;
        }
        else if (content == "!合成") {
            dpp::message msg = make_craft_msg(uid);
            msg.channel_id = ch;
            bot.message_create(msg, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[std::get<dpp::message>(cb.value).id] = uid; }
            });
        }
        else if (content == "!貓") {
            bot.request("https://api.thecatapi.com/v1/images/search", dpp::m_get,
                [&bot, ch](const dpp::http_request_completion_t& res) {
                    auto fail = [&]() {
                        dpp::message m; m.channel_id = ch;
                        m.set_content("❌ 抓不到貓貓 QQ 請稍後再試");
                        bot.message_create(m);
                    };
                    if (res.status != 200) { fail(); return; }
                    try {
                        auto j = nlohmann::json::parse(res.body);
                        std::string url = j[0]["url"].get<std::string>();
                        dpp::embed e;
                        e.set_title("🐱 隨機貓貓").set_color(0xFFB6C1).set_image(url);
                        dpp::message m; m.channel_id = ch; m.add_embed(e);
                        bot.message_create(m);
                    } catch (...) { fail(); }
                });
        }
        else if (content == "!笑話") {
            bot.request("https://v2.jokeapi.dev/joke/Any?safe-mode", dpp::m_get,
                [&bot, ch](const dpp::http_request_completion_t& res) {
                    auto fail = [&]() {
                        dpp::message m; m.channel_id = ch;
                        m.set_content("❌ 抓不到笑話 QQ 請稍後再試");
                        bot.message_create(m);
                    };
                    if (res.status != 200) { fail(); return; }
                    try {
                        auto j = nlohmann::json::parse(res.body);
                        if (j.value("error", false)) { fail(); return; }
                        std::string type     = j["type"].get<std::string>();
                        std::string category = j["category"].get<std::string>();
                        dpp::embed e;
                        e.set_title("😂 隨機笑話").set_color(0xF1C40F);
                        e.set_footer(dpp::embed_footer().set_text("分類：" + category + "　|　來源：JokeAPI (safe-mode)"));
                        if (type == "single") {
                            e.set_description(j["joke"].get<std::string>());
                        } else {
                            std::string setup    = j["setup"].get<std::string>();
                            std::string delivery = j["delivery"].get<std::string>();
                            e.set_description("**" + setup + "**\n\n||" + delivery + "||");
                        }
                        dpp::message m; m.channel_id = ch; m.add_embed(e);
                        bot.message_create(m);
                    } catch (...) { fail(); }
                });
        }
        // !抽獎 時間 人數 獎品
        else {
            const std::string prefix = "!抽獎";
            if (content.rfind(prefix, 0) == 0) {
                bool authorized = is_draw_authorized_msg(uid, ev.msg.member.get_roles());
                if (!authorized) {
                    ev.reply("❌ 只有管理員或副會長才能開抽獎！");
                    return;
                }
                std::string rest = content.size() > prefix.size()
                                   ? content.substr(prefix.size() + 1) : "";
                if (rest.empty()) {
                    ev.reply("用法：`!抽獎 時間 人數 獎品`　例：`!抽獎 5h 4 3張突襲`");
                    return;
                }
                // Parse:  time_str  count  prize...
                std::istringstream iss(rest);
                std::string time_str, count_str;
                iss >> time_str >> count_str;
                std::string prize;
                std::getline(iss >> std::ws, prize);
                if (prize.empty() || count_str.empty()) {
                    ev.reply("用法：`!抽獎 時間 人數 獎品`　例：`!抽獎 5h 4 3張突襲`");
                    return;
                }
                int duration = parse_duration(time_str);
                int winner_count = std::max(1, std::atoi(count_str.c_str()));

                Giveaway gw;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    gw.id           = giveaway_counter++;
                    gw.channel_id   = ch;
                    gw.host_id      = uid;
                    gw.prize        = prize;
                    gw.winner_count = winner_count;
                    gw.end_time     = time(nullptr) + duration;
                    giveaways[gw.id] = gw;
                }
                save_giveaways();
                dpp::message gw_msg = make_giveaway_msg(gw);
                gw_msg.set_reference(ev.msg.id);
                bot.message_create(gw_msg, [gid = gw.id](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        auto& m = std::get<dpp::message>(cb.value);
                        std::lock_guard<std::mutex> lk(data_mutex);
                        giveaways[gid].msg_id     = m.id;
                        giveaways[gid].channel_id = m.channel_id;
                    }
                    save_giveaways();
                });
            }
        }
    });

    // ── 按鈕 ──────────────────────────────────────────────────────────────────
    bot.on_button_click([&bot](const dpp::button_click_t& ev) {
        const std::string& cid  = ev.custom_id;
        const dpp::user&   user = ev.command.get_issuing_user();
        dpp::snowflake     uid  = user.id;
        bool               adm  = is_admin(ev.command);

        // 報名流程按鈕需要 owner 驗證
        if (cid.rfind("boss_",0)==0 || cid.rfind("slot_",0)==0 ||
            cid == "confirm_time"   || cid.rfind("pos_",0)==0   ||
            cid == "back_to_boss"   || cid == "back_to_time") {
            if (!check_owner(ev, uid)) return;
        }

        // ── 幫助翻頁 ──────────────────────────────────────────────────────────
        if (cid.rfind("help_prev_", 0) == 0 || cid.rfind("help_next_", 0) == 0) {
            bool is_prev = cid.rfind("help_prev_", 0) == 0;
            int cur_page = std::stoi(cid.substr(is_prev ? 10 : 10));
            int new_page = is_prev ? cur_page - 1 : cur_page + 1;
            ev.reply(dpp::ir_update_message, make_help_msg(new_page));
        }
        // ── 領取驗證按鈕 ─────────────────────────────────────────────────────
        else if (cid.rfind("claim_verify_", 0) == 0) {
            std::string rest = cid.substr(13);
            size_t sep = rest.find('_'); if (sep == std::string::npos) return;
            dpp::snowflake bu(std::stoull(rest.substr(0, sep)));
            if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 這不是你的驗證！").set_flags(dpp::m_ephemeral)); return; }
            int idx = 0;
            try { idx = std::stoi(rest.substr(sep + 1)); } catch (...) {}
            bool granted = false;
            dpp::message m = handle_claim_verify(uid, idx, &granted);
            if (granted) {
                int64_t repaid = bank_auto_repay(uid, CLAIM_AMOUNT / 2);
                if (repaid > 0) {
                    save_chips(); save_bank();
                    if (!m.embeds.empty())
                        m.embeds[0].add_field("💳 自動還款", std::to_string(repaid) + " 碼已從本次領取扣除", false);
                }
            }
            ev.reply(dpp::ir_update_message, m);
        }
        // ── 小黑屋按鈕（限管理員）────────────────────────────────────────────────
        else if (cid.rfind("claimjail_", 0) == 0) {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
            }
            if (cid == "claimjail_home") {
                ev.reply(dpp::ir_update_message, make_claim_jail_msg());
            } else if (cid == "claimjail_list") {
                ev.reply(dpp::ir_update_message, make_claim_jail_list_msg());
            } else if (cid.rfind("claimjail_unlock_", 0) == 0) {
                dpp::snowflake target(std::stoull(cid.substr(std::string("claimjail_unlock_").size())));
                claim_jail_unlock(target);
                ev.reply(dpp::ir_update_message, make_claim_jail_msg());
            }
        }
        // ── 中斷卡住的遊戲（限管理員）────────────────────────────────────────────
        else if (cid.rfind("adminkill_", 0) == 0) {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
            }
            dpp::snowflake target = 0;
            std::string notice;
            if (cid.rfind("adminkill_hunt_", 0) == 0) {
                target = dpp::snowflake(std::stoull(cid.substr(std::string("adminkill_hunt_").size())));
                dpp::timer tid = 0;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = monster_hunt_games.find(target);
                  if (it != monster_hunt_games.end()) { tid = it->second.timer_id; monster_hunt_games.erase(it); }
                }
                if (tid) g_bot->stop_timer(tid);
                notice = "已中斷怪物狩獵";
            } else if (cid.rfind("adminkill_village_", 0) == 0) {
                target = dpp::snowflake(std::stoull(cid.substr(std::string("adminkill_village_").size())));
                dpp::timer tid = 0;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = village_games.find(target);
                  if (it != village_games.end()) { tid = it->second.timer_id; village_games.erase(it); }
                }
                if (tid) g_bot->stop_timer(tid);
                notice = "已中斷村落挑戰";
            } else if (cid.rfind("adminkill_raidroom_", 0) == 0) {
                std::string rest = cid.substr(std::string("adminkill_raidroom_").size());
                size_t sep = rest.find('_'); if (sep == std::string::npos) return;
                target = dpp::snowflake(std::stoull(rest.substr(0, sep)));
                dpp::snowflake rch(std::stoull(rest.substr(sep + 1)));
                dpp::timer rtid = 0;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = raid_rooms.find(rch);
                  if (it != raid_rooms.end()) { rtid = it->second.timer_id; raid_rooms.erase(it); }
                }
                if (rtid) g_bot->stop_timer(rtid);
                notice = "已解散組隊房間";
            } else if (cid.rfind("adminkill_raid_", 0) == 0) {
                std::string rest = cid.substr(std::string("adminkill_raid_").size());
                size_t sep = rest.find('_'); if (sep == std::string::npos) return;
                target = dpp::snowflake(std::stoull(rest.substr(0, sep)));
                dpp::snowflake rch(std::stoull(rest.substr(sep + 1)));
                dpp::timer tid = 0;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = raid_games.find(rch);
                  if (it != raid_games.end()) { tid = it->second.timer_id; raid_games.erase(it); }
                }
                if (tid) g_bot->stop_timer(tid);
                notice = "已中斷組隊戰鬥";
            } else if (cid.rfind("adminkill_dd_", 0) == 0) {
                std::string rest = cid.substr(std::string("adminkill_dd_").size());
                size_t sep = rest.find('_'); if (sep == std::string::npos) return;
                target = dpp::snowflake(std::stoull(rest.substr(0, sep)));
                dpp::snowflake rch(std::stoull(rest.substr(sep + 1)));
                dpp::timer tid = 0;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = dd_games.find(rch);
                  if (it != dd_games.end()) { tid = it->second.timer_id; dd_games.erase(it); }
                }
                if (tid) g_bot->stop_timer(tid);
                notice = "已中斷暗黑龍王戰鬥";
            } else if (cid.rfind("adminkill_bj_", 0) == 0) {
                target = dpp::snowflake(std::stoull(cid.substr(std::string("adminkill_bj_").size())));
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto uit = user_bj.find(target);
                  if (uit != user_bj.end()) { bj_games.erase(uit->second); user_bj.erase(uit); }
                }
                notice = "已中斷21點";
            } else if (cid.rfind("adminkill_dice_", 0) == 0) {
                target = dpp::snowflake(std::stoull(cid.substr(std::string("adminkill_dice_").size())));
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto uit = user_dice.find(target);
                  if (uit != user_dice.end()) { dice_games.erase(uit->second); user_dice.erase(uit); }
                }
                notice = "已中斷骰子";
            } else if (cid.rfind("adminkill_shoot_", 0) == 0) {
                target = dpp::snowflake(std::stoull(cid.substr(std::string("adminkill_shoot_").size())));
                { std::lock_guard<std::mutex> lk(data_mutex); shoot_games.erase(target); }
                notice = "已中斷射龍門";
            } else if (cid.rfind("adminkill_rocket_", 0) == 0) {
                target = dpp::snowflake(std::stoull(cid.substr(std::string("adminkill_rocket_").size())));
                { std::lock_guard<std::mutex> lk(data_mutex); rocket_games.erase(target); }
                notice = "已中斷火箭升空";
            } else if (cid.rfind("adminkill_scratch_", 0) == 0) {
                target = dpp::snowflake(std::stoull(cid.substr(std::string("adminkill_scratch_").size())));
                { std::lock_guard<std::mutex> lk(data_mutex); scratch_games.erase(target); }
                save_scratch_games(); // 刮刮樂有落地存檔，清掉要記得存檔
                notice = "已中斷刮刮樂";
            } else if (cid.rfind("adminkill_guess_", 0) == 0) {
                target = dpp::snowflake(std::stoull(cid.substr(std::string("adminkill_guess_").size())));
                { std::lock_guard<std::mutex> lk(data_mutex); guess_games.erase(target); }
                notice = "已中斷猜數字";
            } else if (cid.rfind("adminkill_rps_", 0) == 0) {
                std::string rest = cid.substr(std::string("adminkill_rps_").size());
                size_t sep = rest.find('_'); if (sep == std::string::npos) return;
                target = dpp::snowflake(std::stoull(rest.substr(0, sep)));
                dpp::snowflake rch(std::stoull(rest.substr(sep + 1)));
                { std::lock_guard<std::mutex> lk(data_mutex); rps_games.erase(rch); }
                notice = "已中斷猜拳";
            } else if (cid.rfind("adminkill_roulette_", 0) == 0) {
                std::string rest = cid.substr(std::string("adminkill_roulette_").size());
                size_t sep = rest.find('_'); if (sep == std::string::npos) return;
                target = dpp::snowflake(std::stoull(rest.substr(0, sep)));
                dpp::snowflake rch(std::stoull(rest.substr(sep + 1)));
                dpp::timer tid = 0;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = roulette_rooms.find(rch);
                  if (it != roulette_rooms.end()) { tid = it->second.timer_id; roulette_rooms.erase(it); }
                }
                if (tid) g_bot->stop_timer(tid);
                notice = "已中斷俄羅斯輪盤";
            } else if (cid.rfind("adminkill_onw_", 0) == 0) {
                std::string rest = cid.substr(std::string("adminkill_onw_").size());
                size_t sep = rest.find('_'); if (sep == std::string::npos) return;
                target = dpp::snowflake(std::stoull(rest.substr(0, sep)));
                dpp::snowflake rch(std::stoull(rest.substr(sep + 1)));
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = channel_onw_game.find(rch);
                  if (it != channel_onw_game.end()) { onw_games.erase(it->second); channel_onw_game.erase(it); }
                }
                notice = "已中斷一夜狼人";
            } else if (cid.rfind("adminkill_uc_", 0) == 0) {
                std::string rest = cid.substr(std::string("adminkill_uc_").size());
                size_t sep = rest.find('_'); if (sep == std::string::npos) return;
                target = dpp::snowflake(std::stoull(rest.substr(0, sep)));
                dpp::snowflake rch(std::stoull(rest.substr(sep + 1)));
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = channel_uc_game.find(rch);
                  if (it != channel_uc_game.end()) { uc_games.erase(it->second); channel_uc_game.erase(it); }
                }
                notice = "已中斷誰是臥底";
            } else return;
            ev.reply(dpp::ir_update_message, make_admin_kill_report_msg(target, notice));
        }
        // ── 清除本頻道／清除全部進行中的遊戲（限管理員）─────────────────────────
        else if (cid == "admin_clear_channel_btn") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
            }
            int n = admin_kill_channel(ev.command.channel_id);
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 已清除本頻道 **" + std::to_string(n) + "** 筆進行中的遊戲。").set_flags(dpp::m_ephemeral));
        }
        else if (cid == "admin_clear_all_btn") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
            }
            dpp::component row; row.set_type(dpp::cot_action_row);
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("✅ 確認清除全部").set_id("admin_clear_all_confirm").set_style(dpp::cos_danger));
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("❌ 取消").set_id("admin_clear_all_cancel").set_style(dpp::cos_secondary));
            dpp::message m("⚠️ 這會清除**全伺服器、每個人**正在進行的怪物狩獵／村落挑戰／組隊房間／組隊戰鬥／暗黑龍王／21點／骰子／射龍門／火箭升空／刮刮樂／猜數字／猜拳／俄羅斯輪盤／一夜狼人／誰是臥底，直接中斷、不補償，且無法復原（不會動到累計勝負等統計資料）。\n（探險不受影響，會繼續在背景進行，跟寵物打工一樣）確定嗎？");
            m.add_component(row);
            ev.reply(dpp::ir_channel_message_with_source, m.set_flags(dpp::m_ephemeral));
        }
        else if (cid == "admin_clear_all_confirm") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
            }
            int n = admin_kill_everything();
            ev.reply(dpp::ir_update_message,
                dpp::message("✅ 已清除全伺服器 **" + std::to_string(n) + "** 筆進行中的遊戲。").set_flags(dpp::m_ephemeral));
        }
        else if (cid == "admin_clear_all_cancel") {
            ev.reply(dpp::ir_update_message, dpp::message("已取消。").set_flags(dpp::m_ephemeral));
        }
        else if (cid == "admin_toggle_verify_btn") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
            }
            g_claim_verify_enabled = !g_claim_verify_enabled;
            std::string state = g_claim_verify_enabled ? "🔒 **開啟**" : "🔓 **關閉**";
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 領取驗證已切換為 " + state + "。").set_flags(dpp::m_ephemeral));
        }
        // ── 富豪榜翻頁 ────────────────────────────────────────────────────────
        else if (cid.rfind("lb_", 0) == 0) {
            if (!page_is_mine(ev.command.message_id, uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的頁面！").set_flags(dpp::m_ephemeral)); return;
            }
            int page = std::stoi(cid.substr(3));
            ev.reply(dpp::ir_update_message, handle_leaderboard(page));
        }
        // ── 虧損榜翻頁 ───────────────────────────────────────────────────────
        else if (cid.rfind("losers_", 0) == 0) {
            if (!page_is_mine(ev.command.message_id, uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的頁面！").set_flags(dpp::m_ephemeral)); return;
            }
            // id format: losers_{page}_{dir}_{game}  dir='a'|'d'
            std::string rest = cid.substr(7);
            size_t p1 = rest.find('_');
            int page = p1 != std::string::npos ? std::stoi(rest.substr(0, p1)) : 0;
            size_t p2 = (p1 != std::string::npos) ? rest.find('_', p1+1) : std::string::npos;
            bool asc = (p1 != std::string::npos) && (rest[p1+1] == 'a');
            std::string game = (p2 != std::string::npos) ? rest.substr(p2+1) : "";
            ev.reply(dpp::ir_update_message, handle_losers_board(page, game, asc));
        }
        // ── 轉帳確認/取消 ─────────────────────────────────────────────────────
        else if (cid.rfind("xfer_ok_", 0) == 0) {
            uint64_t tid = std::stoull(cid.substr(8));
            ev.reply(dpp::ir_update_message, handle_transfer_confirm(tid, uid, false));
        }
        else if (cid.rfind("xfer_free_", 0) == 0) {
            uint64_t tid = std::stoull(cid.substr(10));
            ev.reply(dpp::ir_update_message, handle_transfer_confirm(tid, uid, true));
        }
        else if (cid.rfind("xfer_cancel_", 0) == 0) {
            uint64_t tid = std::stoull(cid.substr(12));
            ev.reply(dpp::ir_update_message, handle_transfer_cancel(tid, uid));
        }
        // ── 交易按鈕 ─────────────────────────────────────────────────────────
        else if (cid.rfind("trade_", 0) == 0) {
            bool accepting = (cid.rfind("trade_acc_", 0) == 0);
            uint64_t tid = std::stoull(cid.substr(cid.rfind('_') + 1));

            TradeOffer t;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = trade_offers.find(tid);
                if (it == trade_offers.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 交易已過期或不存在！").set_flags(dpp::m_ephemeral)); return;
                }
                t = it->second;
            }
            std::string from_name = "<@" + std::to_string((uint64_t)t.from_uid) + ">";
            std::string to_name   = "<@" + std::to_string((uint64_t)t.to_uid)   + ">";

            if (accepting && uid != t.to_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 只有 " + to_name + " 才能接受這筆交易！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!accepting && uid != t.from_uid && uid != t.to_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你不是這筆交易的一方！").set_flags(dpp::m_ephemeral)); return;
            }

            if (!accepting) {
                { std::lock_guard<std::mutex> lk(data_mutex); trade_offers.erase(tid); }
                ev.reply(dpp::ir_update_message, make_trade_msg(t, from_name, to_name, "rej")); return;
            }

            // Accept: validate + execute under lock（fail_reason 決定後才在鎖外呼叫 make_trade_msg，
            // 避免 make_trade_msg 內部的 col_would_break_set 再次鎖 data_mutex 造成死鎖）
            std::string fail_reason;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto [fkey, fname2] = trade_item_info(t.from_item_id);
                auto [tkey, tname2] = trade_item_info(t.to_item_id);
                auto has_enough = [](dpp::snowflake u, const std::string& key, int64_t qty) {
                    if (key.empty()) return true;
                    if (trade_is_stock(key)) {
                        auto pit = player_stocks.find(u);
                        if (pit == player_stocks.end()) return false;
                        auto hit = pit->second.find(key);
                        return hit != pit->second.end() && hit->second.shares >= qty;
                    }
                    auto it2 = inventory_data[u].find(key);
                    return it2 != inventory_data[u].end() && it2->second >= qty;
                };
                if (!has_enough(t.from_uid, fkey, t.from_qty))
                    fail_reason = "提案方已沒有足夠的該道具！交易取消。";
                int64_t from_fee = (t.from_chips > 0 && !col_has_lovebook(t.from_uid)) ? (t.from_chips + 99) / 100 : 0;
                int64_t to_fee   = (t.to_chips   > 0 && !col_has_lovebook(t.to_uid))   ? (t.to_chips   + 99) / 100 : 0;
                if (fail_reason.empty() && t.from_chips > 0 && chip_data[t.from_uid].chips < t.from_chips + from_fee)
                    fail_reason = "提案方籌碼不足（含手續費）！交易取消。";
                if (fail_reason.empty() && !has_enough(t.to_uid, tkey, t.to_qty))
                    fail_reason = "你沒有足夠的對方要求的道具！交易取消。";
                if (fail_reason.empty() && t.to_chips > 0 && chip_data[t.to_uid].chips < t.to_chips + to_fee)
                    fail_reason = "你的籌碼不足（含手續費）！交易取消。";

                if (!fail_reason.empty()) {
                    trade_offers.erase(tid);
                } else {
                    // Execute
                    if (!fkey.empty()) trade_transfer_item(t.from_uid, t.to_uid, fkey, t.from_qty);
                    if (!tkey.empty()) trade_transfer_item(t.to_uid, t.from_uid, tkey, t.to_qty);
                    if (t.from_chips > 0) {
                        chip_data[t.from_uid].chips -= t.from_chips + from_fee;  // 手續費燒掉
                        chip_data[t.to_uid].chips   += t.from_chips;
                    }
                    if (t.to_chips > 0) {
                        chip_data[t.to_uid].chips   -= t.to_chips + to_fee;      // 手續費燒掉
                        chip_data[t.from_uid].chips += t.to_chips;
                    }
                    trade_offers.erase(tid);
                }
            }
            if (!fail_reason.empty()) {
                ev.reply(dpp::ir_update_message, make_trade_msg(t, from_name, to_name, fail_reason)); return;
            }
            save_chips();
            save_inventory();
            if (trade_is_stock(trade_item_info(t.from_item_id).first) || trade_is_stock(trade_item_info(t.to_item_id).first))
                save_stock_holdings();
            ev.reply(dpp::ir_update_message, make_trade_msg(t, from_name, to_name, "ok"));
        }
        // ── 組隊房間開始：依 boss 分流 ────────────────────────────────────────
        else if (cid.rfind("rroom_start_", 0) == 0) {
            dpp::snowflake rch(std::stoull(cid.substr(12)));
            bool is_dd = false;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = raid_rooms.find(rch);
              if (it != raid_rooms.end() && it->second.boss_key == "dark_dragon") is_dd = true;
            }
            if (is_dd) handle_dd_button(ev); else handle_raid_button(ev); return;
        }
        // ── 組隊 / 多人 Raid → handlers_raid.cpp ─────────────────────────────
        else if (cid.rfind("hunt_team_", 0) == 0 || cid.rfind("hunt_boss_", 0) == 0 ||
                 cid.rfind("rroom_", 0) == 0 || cid.rfind("raid_", 0) == 0) {
            handle_raid_button(ev); return;
        }
        // ── 暗黑龍王 → handlers_dd.cpp ───────────────────────────────────────
        else if (cid.rfind("dd_", 0) == 0) {
            handle_dd_button(ev); return;
        }
        // ── 探險系統 ──────────────────────────────────────────────────────────
        else if (cid.rfind("adv_", 0) == 0) {
            handle_adv_button(ev); return;
        }
        // ── 寵物強化 ──────────────────────────────────────────────────────────
        else if (cid.rfind("enh_", 0) == 0) {
            handle_enhance_button(ev); return;
        }
        // ── 股票市場 ──────────────────────────────────────────────────────────
        else if (cid.rfind("stock_", 0) == 0) {
            handle_stock_button(ev); return;
        }
        // ── 單人怪物狩獵 / 村落按鈕 → handlers_hunt.cpp ──────────────────────
        else if (cid.rfind("hunt_", 0) == 0 || cid.rfind("village_", 0) == 0) {
            handle_hunt_button(ev); return;
        }
        // ── 21點按鈕 → handlers_bj.cpp ──────────────────────────────────────
        else if (cid.rfind("bj_", 0) == 0) {
            handle_bj_button(ev); return;
        }
        // ── 這局不算 ──────────────────────────────────────────────────────────
        else if (cid.rfind("game_cancel_", 0) == 0) {
            // game_cancel_{uid}_{type}_{refund}
            std::string rest = cid.substr(12);
            size_t s1 = rest.find('_');
            if (s1 == std::string::npos) return;
            dpp::snowflake gc_uid(std::stoull(rest.substr(0, s1)));
            if (uid != gc_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的道具！").set_flags(dpp::m_ephemeral)); return; }
            std::string after = rest.substr(s1+1);
            size_t s2 = after.find('_');
            if (s2 == std::string::npos) return;
            std::string gc_type = after.substr(0, s2);
            int64_t gc_refund = std::stoll(after.substr(s2+1));
            // Consume item
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = inventory_data.find(uid);
              if (it == inventory_data.end() || it->second.count("game_cancel") == 0 || it->second.at("game_cancel") <= 0) {
                  ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你沒有這局不算道具！").set_flags(dpp::m_ephemeral)); return;
              }
              it->second["game_cancel"]--;
            }
            // Refund chips
            add_chips(uid, gc_refund);
            // Undo stats
            if (gc_type == "rk") {
                { std::lock_guard<std::mutex> lk(data_mutex); auto& s = rocket_stats_data[uid]; if (s.losses > 0) s.losses--; s.profit += gc_refund; }
                save_rocketstats();
            } else if (gc_type == "di") {
                { std::lock_guard<std::mutex> lk(data_mutex); auto& s = dice_stats_data[uid]; if (s.losses > 0) s.losses--; s.profit += gc_refund; }
                save_dicestats();
            } else if (gc_type == "shl") {
                { std::lock_guard<std::mutex> lk(data_mutex); auto& s = shoot_stats_data[uid]; if (s.losses > 0) s.losses--; s.profit += gc_refund; }
                save_shootstats();
            } else if (gc_type == "shb") {
                { std::lock_guard<std::mutex> lk(data_mutex); auto& s = shoot_stats_data[uid]; if (s.bumps > 0) s.bumps--; s.profit += gc_refund; }
                save_shootstats();
            } else if (gc_type == "bj") {
                { std::lock_guard<std::mutex> lk(data_mutex); auto& s = bj_stats_data[uid]; if (s.losses > 0) s.losses--; s.profit += gc_refund; }
                save_bjstats();
            }
            save_inventory(); save_chips();
            dpp::embed ge;
            ge.set_title("🎴  這局不算！！").set_color(0x2ECC71);
            ge.set_description("已消耗 **這局不算** ×1\n退還 **+" + std::to_string(gc_refund) + "** 碼\n本局結果取消！");
            ev.reply(dpp::ir_update_message, dpp::message().add_embed(ge));
        }
        // ── 對不起我錯了 ──────────────────────────────────────────────────────────
        else if (cid.rfind("half_refund_", 0) == 0) {
            // half_refund_{uid}_{type}_{loss}
            std::string rest = cid.substr(12);
            size_t s1 = rest.find('_');
            if (s1 == std::string::npos) return;
            dpp::snowflake hr_uid(std::stoull(rest.substr(0, s1)));
            if (uid != hr_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的道具！").set_flags(dpp::m_ephemeral)); return; }
            std::string after = rest.substr(s1+1);
            size_t s2 = after.find('_');
            if (s2 == std::string::npos) return;
            int64_t hr_loss = std::stoll(after.substr(s2+1));
            int64_t hr_refund = hr_loss / 2;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = inventory_data.find(uid);
              if (it == inventory_data.end() || it->second.count("half_refund") == 0 || it->second.at("half_refund") <= 0) {
                  ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你沒有對不起我錯了道具！").set_flags(dpp::m_ephemeral)); return;
              }
              it->second["half_refund"]--;
            }
            add_chips(uid, hr_refund);
            save_inventory(); save_chips();
            dpp::embed he;
            he.set_title("💸  對不起我錯了！！").set_color(0x3498DB);
            he.set_description("已消耗 **對不起我錯了** ×1\n退還損失的一半 **+" + std::to_string(hr_refund) + "** 碼");
            ev.reply(dpp::ir_update_message, dpp::message().add_embed(he));
        }
        // ── 寶珠合成 ──────────────────────────────────────────────────────────
        else if (cid.rfind("craft_orb_", 0) == 0) {
            // craft_orb_{type}_{uid}
            std::string rest = cid.substr(10);
            size_t s1 = rest.find('_');
            if (s1 == std::string::npos) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 無效合成請求").set_flags(dpp::m_ephemeral)); return; }
            std::string orb_type = rest.substr(0, s1);
            dpp::snowflake bu(std::stoull(rest.substr(s1 + 1)));
            if (uid != bu) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
            }
            struct OrbCraftInfo { std::string shard_key, orb_key, name; };
            static const std::map<std::string, OrbCraftInfo> CRAFT_MAP = {
                {"speed",      {"orb_shard_speed",      "EQ_K_SPEED",      "迅捷狼王的寶珠"}},
                {"athena",     {"orb_shard_athena",     "EQ_K_ATHENA",     "雅典娜的寶珠"}},
                {"bear",       {"orb_shard_bear",       "EQ_K_BEAR",       "巨山狂熊的寶珠"}},
                {"viking",     {"orb_shard_viking",     "EQ_K_VIKING",     "維京的寶珠"}},
                {"wargod",     {"orb_shard_wargod",     "EQ_K_WARGOD",     "狂怒戰神的寶珠"}},
                {"latus",      {"orb_shard_latus",      "EQ_K_LATUS",      "拉圖斯的寶珠"}},
                {"darkdragon", {"orb_shard_darkdragon", "EQ_K_DARKDRAGON", "暗黑龍王的寶珠"}},
            };
            auto cit = CRAFT_MAP.find(orb_type);
            if (cit == CRAFT_MAP.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 未知寶珠類型").set_flags(dpp::m_ephemeral)); return; }
            const auto& ci = cit->second;
            bool ok = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& inv = inventory_data[uid];
                if (inv.count(ci.shard_key) && inv[ci.shard_key] >= 10) {
                    inv[ci.shard_key] -= 10;
                    if (inv[ci.shard_key] == 0) inv.erase(ci.shard_key);
                    inv[ci.orb_key]++;
                    ok = true;
                }
            }
            if (!ok) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 碎片不足！需要 10 個才能合成。").set_flags(dpp::m_ephemeral)); return;
            }
            save_inventory();
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 合成成功！獲得 **" + ci.name + "** ×1！\n"
                             "前往 `!裝備` → 靈魂寶珠欄位裝備它。").set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("craft_bb_wig_", 0) == 0 || cid.rfind("craft_bb_undies_", 0) == 0) {
            bool is_wig = cid.rfind("craft_bb_wig_", 0) == 0;
            dpp::snowflake bu(std::stoull(cid.substr(is_wig ? 13 : 16)));
            if (uid != bu) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string broken_key = is_wig ? "col_bb_wig_broken" : "col_bb_undies_broken";
            std::string full_key   = is_wig ? "col_bb_wig_full"   : "col_bb_undies_full";
            std::string full_name  = is_wig ? "Zoey散發氣味的秀髮" : "皮包遺失的粉紅內衣";
            bool ok = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& inv = inventory_data[uid];
                if (inv.count(broken_key) && inv[broken_key] >= 5) {
                    inv[broken_key] -= 5;
                    if (inv[broken_key] == 0) inv.erase(broken_key);
                    inv[full_key]++;
                    ok = true;
                }
            }
            if (!ok) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 戰損版數量不足！需要 5 個才能合成。").set_flags(dpp::m_ephemeral)); return;
            }
            save_inventory();
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 合成成功！獲得 **" + full_name + "** ×1！").set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("craft_main_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(11)));
            if (uid != bu) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_craft_msg(uid));
        }
        // ── 背包 / 商店 / 轉蛋 / 裝備按鈕 → handlers_shop.cpp ──────────────
        else if (cid.rfind("shop_", 0) == 0 || cid.rfind("gacha_", 0) == 0 ||
                 cid.rfind("equip_", 0) == 0 || cid.rfind("bag_", 0) == 0) {
            handle_shop_button(ev); return;
        }
        // ── 寵物 / 大廳 / 天賦按鈕 ────────────────────────────────────────────
        else if (cid.rfind("lobby_", 0) == 0 || cid.rfind("pet_", 0) == 0 || cid.rfind("talent_pick_", 0) == 0 || cid.rfind("talent_slot_", 0) == 0) {
            handle_pet_button(ev); return;
        }
        // ── 銀行按鈕 ──────────────────────────────────────────────────────────
        else if (cid.rfind("bank_refresh_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(13)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_bank_msg(uid));
        }
        else if (cid.rfind("bank_deposit_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(13)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            int64_t chips;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                chips = chip_data[uid].chips;
            }
            dpp::interaction_modal_response modal(
                "bank_deposit_modal_" + std::to_string((uint64_t)uid), "💰 存款");
            modal.add_component(dpp::component().set_type(dpp::cot_text)
                .set_label("存款金額（錢包 " + std::to_string(chips) + " 碼）")
                .set_id("amount").set_text_style(dpp::text_short)
                .set_min_length(1).set_max_length(15)
                .set_placeholder("輸入要存入的碼數"));
            ev.dialog(modal);
        }
        else if (cid.rfind("bank_withdraw_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(14)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            int64_t dep_total;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                dep_total = bank_data[uid].deposited;
            }
            dpp::interaction_modal_response modal(
                "bank_withdraw_modal_" + std::to_string((uint64_t)uid), "📤 提款");
            modal.add_component(dpp::component().set_type(dpp::cot_text)
                .set_label("提款金額（最多 " + std::to_string(dep_total) + " 碼）")
                .set_id("amount").set_text_style(dpp::text_short)
                .set_min_length(1).set_max_length(15)
                .set_placeholder("輸入要提出的碼數（或 all）"));
            ev.dialog(modal);
        }
        else if (cid.rfind("bank_borrow_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(12)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            dpp::interaction_modal_response modal(
                "bank_borrow_modal_" + std::to_string((uint64_t)uid), "💸 借款");
            modal.add_component(dpp::component().set_type(dpp::cot_text)
                .set_label("借款金額（上限 " + std::to_string(effective_max_loan(uid)) + " 碼，利率 " + std::to_string((int)(effective_loan_rate(uid)*100)) + "%/天）")
                .set_id("amount").set_text_style(dpp::text_short)
                .set_min_length(1).set_max_length(15)
                .set_placeholder("輸入要借的碼數"));
            ev.dialog(modal);
        }
        else if (cid.rfind("bank_repay_", 0) == 0) {
            dpp::snowflake btn_uid(std::stoull(cid.substr(11)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            int64_t total_owed;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& bd = bank_data[uid];
                total_owed = calc_loan_with_interest(bd.loan, bd.loan_time);
            }
            dpp::interaction_modal_response modal(
                "bank_repay_modal_" + std::to_string((uint64_t)uid), "💳 還款");
            modal.add_component(dpp::component().set_type(dpp::cot_text)
                .set_label("還款金額（欠款 " + std::to_string(total_owed) + " 碼，可分次還）")
                .set_id("amount").set_text_style(dpp::text_short)
                .set_min_length(1).set_max_length(15)
                .set_placeholder("輸入要還的碼數（或 all 全額還款）"));
            ev.dialog(modal);
        }
        // ── 誰是臥底按鈕 → handlers_uc.cpp ──────────────────────────────────
        else if (cid.rfind("uc_", 0) == 0) {
            handle_uc_button(ev); return;
        }
        // ── onw / wolf buttons → handlers_wolf.cpp ────────────────────────────
        else if (cid.rfind("onw_again_", 0) == 0 || cid.rfind("onw_", 0) == 0 ||
                 cid.rfind("wolf_", 0) == 0) {
            handle_wolf_button(ev); return;
        }
        // ── 警告榜單按鈕 ──────────────────────────────────────────────────────
        else if (cid == "warn_board") {
            ev.reply(dpp::ir_update_message, handle_warn_board());
        }
        else if (cid.rfind("warn_detail_", 0) == 0) {
            dpp::snowflake target(std::stoull(cid.substr(12)));
            ev.reply(dpp::ir_update_message, handle_warn_detail(target));
        }
        // ── 管理員面板 Modal 觸發 ─────────────────────────────────────────────
        else if (cid == "admin_chip_modal_btn" || cid == "admin_item_btn" || cid == "admin_kill_btn") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
            }
            if (cid == "admin_chip_modal_btn") {
                dpp::interaction_modal_response modal("admin_chips_modal", "管理員調整碼數");
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("目標 User ID").set_id("target_uid")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(20)
                    .set_placeholder("例：457478323665240065"));
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("碼數（負數可扣除）").set_id("chip_amount")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(15));
                ev.dialog(modal);
            } else if (cid == "admin_item_btn") {
                dpp::interaction_modal_response modal("admin_item_modal", "管理員給道具");
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("目標 User ID").set_id("target_uid")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(20)
                    .set_placeholder("例：457478323665240065"));
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("道具代碼").set_id("item_key")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(20)
                    .set_placeholder("inc_100 / grow_1 / evo_1 ..."));
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("數量（負數＝沒收）").set_id("item_qty")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(5)
                    .set_placeholder("1"));
                ev.dialog(modal);
            } else { // admin_kill_btn
                dpp::interaction_modal_response modal("admin_kill_lookup_modal", "查詢／中斷玩家遊戲");
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("目標 User ID").set_id("target_uid")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(20)
                    .set_placeholder("例：457478323665240065"));
                ev.dialog(modal);
            }
        }
        // ── 記帳：Tab 切換 ────────────────────────────────────────────────────
        else if (cid.rfind("ledger_tab_", 0) == 0) {
            if (!adm) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 只有管理員可以使用記帳本！").set_flags(dpp::m_ephemeral)); return; }
            std::string filter = cid.substr(11);
            ev.reply(dpp::ir_update_message, make_ledger_msg(0, filter));
        }
        // ── 記帳：刪除單筆 ────────────────────────────────────────────────────
        else if (cid.rfind("ledger_del_", 0) == 0) {
            if (!page_is_mine(ev.command.message_id, uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的頁面！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!adm) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 只有管理員可以刪除帳單！").set_flags(dpp::m_ephemeral)); return;
            }
            // format: ledger_del_{filter}_{page}_{record_id}
            std::string rest = cid.substr(11);
            size_t s1 = rest.find('_');
            if (s1 == std::string::npos) return;
            std::string filter = rest.substr(0, s1);
            rest = rest.substr(s1 + 1);
            size_t s2 = rest.find('_');
            if (s2 == std::string::npos) return;
            int page        = std::stoi(rest.substr(0, s2));
            uint64_t rec_id = std::stoull(rest.substr(s2 + 1));
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = std::find_if(purchase_records.begin(), purchase_records.end(),
                    [rec_id](const PurchaseRecord& r){ return r.id == rec_id; });
                if (it != purchase_records.end()) purchase_records.erase(it);
            }
            save_purchases();
            ev.reply(dpp::ir_update_message, make_ledger_msg(page, filter));
        }
        // ── 記帳：翻頁 ────────────────────────────────────────────────────────
        else if (cid.rfind("ledger_page_", 0) == 0) {
            if (!page_is_mine(ev.command.message_id, uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的頁面！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!adm) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 只有管理員可以使用記帳本！").set_flags(dpp::m_ephemeral)); return;
            }
            // format: ledger_page_{filter}_{page}
            std::string rest = cid.substr(12);
            size_t sep = rest.rfind('_');
            if (sep == std::string::npos) return;
            std::string filter = rest.substr(0, sep);
            int page = std::stoi(rest.substr(sep + 1));
            ev.reply(dpp::ir_update_message, make_ledger_msg(page, filter));
        }
        // ── 個人記帳：Tab 切換 ────────────────────────────────────────────────
        else if (cid.rfind("my_ledger_tab_", 0) == 0) {
            // my_ledger_tab_{uid}_{filter}
            std::string rest = cid.substr(14);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的帳本！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string filter = rest.substr(sep + 1);
            ev.reply(dpp::ir_update_message, make_my_ledger_msg(uid, 0, filter));
        }
        // ── 個人記帳：翻頁 ────────────────────────────────────────────────────
        else if (cid.rfind("my_ledger_page_", 0) == 0) {
            // my_ledger_page_{uid}_{filter}_{page}
            std::string rest = cid.substr(15);
            size_t s1 = rest.find('_');
            if (s1 == std::string::npos) return;
            dpp::snowflake btn_uid(std::stoull(rest.substr(0, s1)));
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的帳本！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string rest2 = rest.substr(s1 + 1);
            size_t s2 = rest2.rfind('_');
            if (s2 == std::string::npos) return;
            std::string filter = rest2.substr(0, s2);
            int page = std::stoi(rest2.substr(s2 + 1));
            ev.reply(dpp::ir_update_message, make_my_ledger_msg(uid, page, filter));
        }
        // ── 遊戲按鈕 (骰子/猜數字/射/火箭/卷軸/刮刮樂) → handlers_games.cpp ─
        else if (cid.rfind("dc_", 0) == 0 || cid.rfind("guess_", 0) == 0 ||
                 cid.rfind("shoot_", 0) == 0 || cid.rfind("rocket_", 0) == 0 ||
                 cid.rfind("scroll_", 0) == 0 || cid.rfind("sc9_", 0) == 0) {
            handle_games_button(ev); return;
        }
        // ── 錢包分頁按鈕 ──────────────────────────────────────────────────────
        else if (cid.rfind("wallet_home_", 0) == 0 || cid.rfind("wallet_games_", 0) == 0 ||
                 cid.rfind("wallet_wolf_", 0) == 0 || cid.rfind("wallet_onw_", 0) == 0 ||
                 cid.rfind("wallet_bank_", 0) == 0) {
            dpp::snowflake owner(std::stoull(cid.substr(cid.rfind('_') + 1)));
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的錢包！").set_flags(dpp::m_ephemeral)); return;
            }
            if (cid.rfind("wallet_games_", 0) == 0)
                ev.reply(dpp::ir_update_message, make_wallet_games_msg(uid));
            else if (cid.rfind("wallet_onw_", 0) == 0)
                ev.reply(dpp::ir_update_message, make_wallet_onw_msg(uid));
            else if (cid.rfind("wallet_wolf_", 0) == 0)
                ev.reply(dpp::ir_update_message, make_wallet_wolf_msg(uid));
            else if (cid.rfind("wallet_bank_", 0) == 0)
                ev.reply(dpp::ir_update_message, make_bank_msg(uid));
            else
                ev.reply(dpp::ir_update_message, make_wallet_home_msg(uid));
        }
        // ── 抽獎：加入 ────────────────────────────────────────────────────────
        else if (cid.rfind("giveaway_join_", 0) == 0 || cid.rfind("giveaway_leave_", 0) == 0) {
            bool is_join = cid.rfind("giveaway_join_", 0) == 0;
            uint64_t gid = std::stoull(cid.substr(is_join ? 14 : 15));
            bool role_ok = true, over = false, already_in = false, no_chips = false;
            int64_t entry_cost = 0;
            Giveaway gw;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = giveaways.find(gid);
                if (it == giveaways.end()) {
                    dpp::embed e; e.set_title("⚠️  抽獎不存在").set_color(0x808080);
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message().add_embed(e).set_flags(dpp::m_ephemeral)); return;
                }
                if (it->second.ended) { over = true; }
                else {
                    entry_cost = it->second.entry_cost;
                    if (it->second.role_restriction) {
                        role_ok = false;
                        for (auto& rid : ev.command.member.get_roles())
                            if (rid == it->second.role_restriction) { role_ok = true; break; }
                    }
                    if (role_ok) {
                        auto& p = it->second.participants;
                        already_in = p.count(uid) > 0;
                        if (is_join && !already_in) {
                            // Check chips
                            if (entry_cost > 0 && chip_data[uid].chips < entry_cost) {
                                no_chips = true;
                            } else {
                                if (entry_cost > 0) chip_data[uid].chips -= entry_cost;
                                p.insert(uid);
                                gw = it->second;
                            }
                        } else if (!is_join && already_in) {
                            p.erase(uid);
                            if (entry_cost > 0) chip_data[uid].chips += entry_cost;
                            gw = it->second;
                        } else {
                            gw = it->second;
                        }
                    }
                }
            }
            if (over) {
                dpp::embed e; e.set_title("⚠️  抽獎已結束").set_color(0x808080);
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral)); return;
            }
            if (!role_ok) {
                dpp::embed e; e.set_title("❌  你沒有參加此抽獎的資格").set_color(0xE74C3C);
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral)); return;
            }
            if (no_chips) {
                dpp::embed e; e.set_title("❌  籌碼不足").set_color(0xE74C3C);
                e.set_description("報名此抽獎需要 **" + std::to_string(entry_cost) + "** 碼，你的餘額不足。");
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral)); return;
            }
            if (is_join && already_in) {
                dpp::embed e; e.set_title("ℹ️  你已在報名名單中").set_color(0x3498DB);
                e.set_description("如要取消報名請按 **↩ 取消報名** 按鈕。");
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral)); return;
            }
            if (!is_join && !already_in) {
                dpp::embed e; e.set_title("ℹ️  你尚未報名此抽獎").set_color(0x3498DB);
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral)); return;
            }
            if (entry_cost > 0) save_chips();
            save_giveaways();
            dpp::message updated = make_giveaway_msg(gw);
            updated.id = gw.msg_id; updated.channel_id = gw.channel_id;
            bot.message_edit(updated);
            dpp::embed fb;
            if (is_join) {
                fb.set_title("✅  報名成功！").set_color(0x2ECC71);
                std::string desc = "你已加入抽獎 **" + gw.prize + "**！";
                if (entry_cost > 0) desc += "\n已扣除報名費 **" + std::to_string(entry_cost) + "** 碼。";
                desc += "\n抽獎結束時會 @通知中獎者。";
                fb.set_description(desc);
            } else {
                fb.set_title("↩  已取消報名").set_color(0x808080);
                std::string desc = "你已從抽獎 **" + gw.prize + "** 中退出。";
                if (entry_cost > 0) desc += "\n已退還報名費 **" + std::to_string(entry_cost) + "** 碼。";
                fb.set_description(desc);
            }
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message().add_embed(fb).set_flags(dpp::m_ephemeral));
        }
        // ── 輪盤賭 → handlers_roulette.cpp ────────────────────────────────────
        else if (cid.rfind("rl_", 0) == 0) {
            handle_roulette_button(ev); return;
        }
        // ── 猜拳 ────────────────────────────────────────────────────────────────
        else if (cid.rfind("rps_", 0) == 0) {
            handle_rps_button(ev); return;
        }
        else {
            // Unknown / unhandled button — must still reply to prevent Discord 3-second timeout
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 按鈕已失效（可能在 bot 重啟前建立）").set_flags(dpp::m_ephemeral));
        }
    });

    // ── Modal 送出（管理員操作）─────────────────────────────────────────────
    bot.on_form_submit([](const dpp::form_submit_t& ev) {
        const std::string& cid = ev.custom_id;
        dpp::snowflake issuer = ev.command.get_issuing_user().id;

        // Pet rename modal
        if (cid.rfind("pet_rename_modal_", 0) == 0 || cid.rfind("pet_release_modal_", 0) == 0) {
            handle_pet_modal(ev); return;
        }

        // Bank deposit modal
        if (cid.rfind("bank_deposit_modal_", 0) == 0) {
            dpp::snowflake modal_uid(std::stoull(cid.substr(19)));
            if (issuer != modal_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string input;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    input = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        input = std::get<std::string>(sub.value);
            }
            int64_t amount = 0;
            try { amount = std::stoll(input); } catch (...) {}
            std::string notice; bool saved = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& bd = bank_data[modal_uid];
                if (amount <= 0) {
                    notice = "❌ 請輸入有效的碼數！";
                } else if (bd.loan > 0) {
                    notice = "❌ 有未還清的借款，無法存款！";
                } else if (chip_data[modal_uid].chips < amount) {
                    notice = "❌ 錢包餘額不足！";
                } else {
                    // 初始化 daily_min（若尚未設定）
                    if (bd.daily_min == 0) bd.daily_min = bd.deposited;
                    if (bd.last_interest_day == 0) bd.last_interest_day = utc8_day_number();
                    chip_data[modal_uid].chips -= amount;
                    bd.deposited += amount;
                    // 存款增加餘額，daily_min 不變（min 不可能因增加而降低）
                    notice = "✅ 已存入 **" + std::to_string(amount) + "** 碼！";
                    saved = true;
                }
            }
            if (saved) { save_chips(); save_bank(); }
            ev.reply(dpp::ir_update_message, make_bank_msg(modal_uid, notice));
            return;
        }

        // Bank withdraw modal
        if (cid.rfind("bank_withdraw_modal_", 0) == 0) {
            dpp::snowflake modal_uid(std::stoull(cid.substr(20)));
            if (issuer != modal_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string input;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    input = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        input = std::get<std::string>(sub.value);
            }
            std::string lo = input;
            for (auto& c2 : lo) c2 = (char)std::tolower((unsigned char)c2);
            bool all_flag = (lo == "all" || lo == "全部");
            int64_t amount = 0;
            if (!all_flag) { try { amount = std::stoll(input); } catch (...) {} }
            std::string notice; bool saved = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& bd = bank_data[modal_uid];
                int64_t dep_total = bd.deposited;
                if (dep_total <= 0) {
                    notice = "❌ 沒有存款可提！";
                } else {
                    if (all_flag) amount = dep_total;
                    if (amount <= 0 || amount > dep_total) {
                        notice = "❌ 金額無效！存款餘額為 " + std::to_string(dep_total) + " 碼。";
                    } else {
                        // 初始化 daily_min（若尚未設定）
                        if (bd.daily_min == 0) bd.daily_min = bd.deposited;
                        chip_data[modal_uid].chips += amount;
                        bd.deposited -= amount;
                        // 提款降低餘額，更新 daily_min
                        bd.daily_min = std::min(bd.daily_min, bd.deposited);
                        if (bd.deposited <= 0) {
                            bd.daily_min = 0; bd.last_interest_day = 0;
                        }
                        notice = "✅ 已提款 **" + std::to_string(amount) + "** 碼！";
                        saved = true;
                    }
                }
            }
            if (saved) { save_chips(); save_bank(); }
            ev.reply(dpp::ir_update_message, make_bank_msg(modal_uid, notice));
            return;
        }

        // Bank borrow modal
        if (cid.rfind("bank_borrow_modal_", 0) == 0) {
            dpp::snowflake modal_uid(std::stoull(cid.substr(18)));
            if (issuer != modal_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string input;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    input = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        input = std::get<std::string>(sub.value);
            }
            int64_t amount = 0;
            try { amount = std::stoll(input); } catch (...) {}
            int64_t mloan = effective_max_loan(modal_uid);
            double  mrate = effective_loan_rate(modal_uid);
            std::string notice; bool saved = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& bd = bank_data[modal_uid];
                if (bd.loan > 0) {
                    notice = "❌ 已有未還清的借款！請先還款。";
                } else if (bd.deposited > 0) {
                    notice = "❌ 有存款時不可借款！請先提款。";
                } else {
                    if (amount <= 0 || amount > mloan) {
                        notice = "❌ 借款金額需在 1 ~ " + std::to_string(mloan) + " 碼之間！";
                    } else {
                    chip_data[modal_uid].chips += amount;
                    bd.loan      = (int64_t)(amount * (1.0 + mrate) + 0.5);
                    bd.loan_time = time(nullptr);
                    int rate_pct = (int)(mrate * 100);
                    notice = "✅ 已借入 **" + std::to_string(amount) + "** 碼！（立即計息，需還款 **" + std::to_string(bd.loan) + "** 碼起）每日再加 " + std::to_string(rate_pct) + "% 利息。";
                    saved = true;
                    } // inner else
                } // outer else
            }
            if (saved) { save_chips(); save_bank(); }
            ev.reply(dpp::ir_update_message, make_bank_msg(modal_uid, notice));
            return;
        }

        // Bank repay modal
        if (cid.rfind("bank_repay_modal_", 0) == 0) {
            dpp::snowflake modal_uid(std::stoull(cid.substr(17)));
            if (issuer != modal_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的銀行！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string input;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    input = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        input = std::get<std::string>(sub.value);
            }
            std::string notice; bool saved = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& bd = bank_data[modal_uid];
                auto& cd = chip_data[modal_uid];
                int64_t total_owed = calc_loan_with_interest(bd.loan, bd.loan_time);
                if (total_owed <= 0) {
                    notice = "❌ 你目前沒有借款！";
                } else {
                    int64_t amount = 0;
                    std::string inp_lower = input;
                    for (auto& c : inp_lower) c = (char)std::tolower((unsigned char)c);
                    if (inp_lower == "all") {
                        amount = std::min(cd.chips, total_owed);
                    } else {
                        try { amount = std::stoll(input); } catch (...) {}
                    }
                    if (amount <= 0) {
                        notice = "❌ 請輸入有效金額！";
                    } else if (amount > cd.chips) {
                        notice = "❌ 錢包餘額不足！（目前 " + std::to_string(cd.chips) + " 碼）";
                    } else if (amount > total_owed) {
                        notice = "❌ 還款金額超過欠款！（欠款 " + std::to_string(total_owed) + " 碼）";
                    } else {
                        int64_t days_elapsed = (time(nullptr) - bd.loan_time) / 86400;
                        cd.chips -= amount;
                        total_owed -= amount;
                        if (total_owed <= 0) {
                            bd.loan = 0; bd.loan_time = 0;
                            notice = "✅ 已全額還款 **" + std::to_string(amount) + "** 碼！借款已清零。";
                        } else {
                            bd.loan = total_owed;
                            bd.loan_time += days_elapsed * 86400;
                            notice = "✅ 已還款 **" + std::to_string(amount) + "** 碼，尚餘欠款 **" + std::to_string(total_owed) + "** 碼。";
                        }
                        saved = true;
                    }
                }
            }
            if (saved) { save_chips(); save_bank(); }
            ev.reply(dpp::ir_update_message, make_bank_msg(modal_uid, notice));
            return;
        }

        // 猜數字 modal → handlers_games.cpp
        if (cid.rfind("guess_modal_", 0) == 0) {
            handle_games_modal(ev); return;
        }

        // Undercover modals → handlers_uc.cpp
        if (cid.rfind("uc_answer_modal_", 0) == 0 || cid.rfind("uc_guess_modal_", 0) == 0) {
            handle_uc_modal(ev); return;
        }

        // ── 輪盤賭 Modal → handlers_roulette.cpp ──────────────────────────────
        if (cid.rfind("rl_stake_m_", 0) == 0 || cid.rfind("rl_bet_m_", 0) == 0) {
            handle_roulette_modal(ev); return;
        }

        // Adventure funds modal
        if (cid.rfind("adv_funds_modal_", 0) == 0) {
            handle_adv_modal(ev); return;
        }

        // Stock buy/sell/mood modal
        if (cid.rfind("stock_", 0) == 0) {
            handle_stock_modal(ev); return;
        }

        if (cid != "admin_chips_modal" && cid != "admin_item_modal" && cid != "admin_kill_lookup_modal") return;
        if (cfg.notify_user_id.empty() || std::to_string(issuer) != cfg.notify_user_id) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
        }
        // Extract all string fields in order
        auto trim = [](std::string s) {
            while (!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
            while (!s.empty() && (s.back()==' '||s.back()=='\r'||s.back()=='\n')) s.pop_back();
            return s;
        };
        std::vector<std::string> fields;
        for (auto& row : ev.components) {
            if (std::holds_alternative<std::string>(row.value))
                fields.push_back(trim(std::get<std::string>(row.value)));
            for (auto& sub : row.components)
                if (std::holds_alternative<std::string>(sub.value))
                    fields.push_back(trim(std::get<std::string>(sub.value)));
        }
        // Parse target user ID (first field)
        if (fields.empty()) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 請填寫所有欄位！").set_flags(dpp::m_ephemeral)); return;
        }
        dpp::snowflake target_uid = 0;
        try { target_uid = dpp::snowflake(std::stoull(fields[0])); } catch (...) {}
        if (target_uid == 0) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ User ID 格式錯誤！").set_flags(dpp::m_ephemeral)); return;
        }

        if (cid == "admin_chips_modal") {
            // fields: [target_uid, amount]
            if (fields.size() < 2) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 請填寫碼數！").set_flags(dpp::m_ephemeral)); return;
            }
            int64_t amount = 0;
            try { amount = std::stoll(fields[1]); } catch (...) {}
            if (amount == 0) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 請輸入有效數字！").set_flags(dpp::m_ephemeral)); return;
            }
            add_chips(target_uid, amount);
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message(
                    "✅ 已為 <@" + std::to_string((uint64_t)target_uid) + "> " +
                    (amount > 0 ? "新增" : "扣除") + " **" + std::to_string(std::abs(amount)) +
                    "** 碼！\n目前餘額：**" + std::to_string(get_chips(target_uid)) + "** 碼。"
                ).set_flags(dpp::m_ephemeral));

        } else if (cid == "admin_item_modal") {
            // fields: [target_uid, item_key_or_id, qty]（qty 負數＝沒收）
            if (fields.size() < 3) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 請填寫道具代碼和數量！").set_flags(dpp::m_ephemeral)); return;
            }
            // Accept item_id (numeric) or key (string)
            const std::string& raw = fields[1];
            const VirtualShopItem* vi = nullptr;
            bool is_num = !raw.empty() && std::all_of(raw.begin(), raw.end(), ::isdigit);
            if (is_num) vi = find_virtual_item_by_id(std::stoi(raw));
            else        vi = find_virtual_item(raw);
            if (!vi) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 找不到道具：`" + raw + "`\n可輸入道具 ID 數字（如 96001）或 key（如 weekly_hunt_scroll）").set_flags(dpp::m_ephemeral)); return;
            }
            const std::string& key = vi->key;
            int qty = 1;
            try { qty = std::stoi(fields[2]); } catch (...) {}
            if (qty == 0 || qty < -999 || qty > 999) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 數量必須在 -999~999 之間（不可為 0，負數＝沒收）！").set_flags(dpp::m_ephemeral)); return;
            }
            int64_t actual = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& cur = inventory_data[target_uid][key];
                int64_t before = cur;
                cur += qty;
                if (cur < 0) cur = 0; // 沒收上限就是玩家現有的數量，不會扣成負的
                actual = cur - before;
            }
            save_inventory();
            std::string verb = actual >= 0 ? "給予" : "沒收";
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 已" + verb + " <@" + std::to_string((uint64_t)target_uid) +
                    "> **" + vi->name + "** × " + std::to_string(std::abs(actual)) + "！").set_flags(dpp::m_ephemeral));

        } else if (cid == "admin_kill_lookup_modal") {
            ev.reply(dpp::ir_channel_message_with_source, make_admin_kill_report_msg(target_uid));
        }
    });

    // ── 選單 ──────────────────────────────────────────────────────────────────
    bot.on_select_click([](const dpp::select_click_t& ev) {
        const std::string& cid  = ev.custom_id;
        const dpp::user&   user = ev.command.get_issuing_user();
        dpp::snowflake     uid  = user.id;

        if (cid == "day_select") {
            if (!check_owner(ev, uid)) return;
            int new_day = std::stoi(ev.values[0]);
            std::string boss;
            std::set<std::pair<std::string,std::string>> slots;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = user_states.find(uid);
                if (it == user_states.end()) return;
                it->second.view_day = new_day;
                boss  = it->second.boss;
                slots = it->second.slots;
            }
            ev.reply(dpp::ir_update_message, make_time_msg(boss, user, new_day, slots));
        }
        else if (cid == "records_view") {
            if (!check_owner(ev, uid)) return;
            const std::string& filter = ev.values[0];
            bool adm = is_admin(ev.command);
            { std::lock_guard<std::mutex> lk(data_mutex); view_filters[uid] = filter; }
            ev.reply(dpp::ir_update_message, make_records_view_msg(filter, uid, adm));
        }
        else if (cid == "losers_game_sel") {
            if (!page_is_mine(ev.command.message_id, uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的頁面！").set_flags(dpp::m_ephemeral)); return;
            }
            // value format: losers_{page}_{dir}_{game}
            std::string val = ev.values[0];
            std::string rest = val.substr(7);
            size_t p1 = rest.find('_');
            int page = p1 != std::string::npos ? std::stoi(rest.substr(0, p1)) : 0;
            size_t p2 = (p1 != std::string::npos) ? rest.find('_', p1+1) : std::string::npos;
            bool asc = (p1 != std::string::npos) && (rest[p1+1] == 'a');
            std::string game = (p2 != std::string::npos) ? rest.substr(p2+1) : "";
            ev.reply(dpp::ir_update_message, handle_losers_board(page, game, asc));
        }
        else if (cid == "petdex_chain_sel") {
            ev.reply(dpp::ir_update_message, make_petdex_msg(ev.values[0]));
        }
        // ── 誰是臥底 select → handlers_uc.cpp ───────────────────────────────
        else if (cid.rfind("uc_pool_", 0) == 0) {
            handle_uc_select(ev, uid); return;
        }
        else if (cid.rfind("itemdex_cat_", 0) == 0) {
            if (ev.values.empty()) return;
            dpp::snowflake bu(std::stoull(cid.substr(12)));
            if (uid != bu) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的圖鑑！").set_flags(dpp::m_ephemeral));
                return;
            }
            ev.reply(dpp::ir_update_message, make_itemdex_cat_msg(uid, ev.values[0]));
        }
        else if (cid.rfind("equipdex_set_", 0) == 0) {
            if (ev.values.empty()) return;
            dpp::snowflake bu(std::stoull(cid.substr(13)));
            if (uid != bu) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的圖鑑！").set_flags(dpp::m_ephemeral));
                return;
            }
            ev.reply(dpp::ir_update_message, make_equipdex_set_msg(uid, ev.values[0]));
        }
        // ── 輪盤賭 select → handlers_roulette.cpp ─────────────────────────────
        else if (cid.rfind("rl_ch_sel_", 0) == 0) {
            handle_roulette_select(ev); return;
        }
    });

    // ── Autocomplete ──────────────────────────────────────────────────────────
    bot.on_autocomplete([](const dpp::autocomplete_t& ev) {
        std::string cmd = ev.name;
        if (cmd != "交易" && cmd != "trade") return;
        dpp::snowflake uid = ev.command.get_issuing_user().id;
        for (auto& opt : ev.options) {
            if (opt.name != "我的道具" || !opt.focused) continue;
            std::string filter;
            if (opt.value.index() != 0) try { filter = std::get<std::string>(opt.value); } catch (...) {}
            std::map<std::string,int> inv;
            std::map<std::string,StockHolding> holdings;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = inventory_data.find(uid);
              if (it != inventory_data.end()) inv = it->second;
              auto sit = player_stocks.find(uid);
              if (sit != player_stocks.end()) holdings = sit->second;
            }
            std::vector<dpp::command_option_choice> choices;
            for (auto& [key, cnt] : inv) {
                if (cnt <= 0 || key.empty() || key[0] == '_') continue;
                std::string name; int item_id = 0;
                if (auto* vi = find_virtual_item(key))  { name = vi->name; item_id = vi->item_id; }
                else if (auto* gi = find_gacha_item(key)) { name = gi->name; item_id = gi->item_id; }
                else continue;
                if (item_id == 0) continue;
                std::string label = name + " ×" + std::to_string(cnt) + "（ID: " + std::to_string(item_id) + "）";
                if (!filter.empty()) {
                    // match by name or ID
                    if (label.find(filter) == std::string::npos &&
                        std::to_string(item_id).find(filter) == std::string::npos) continue;
                }
                choices.push_back(dpp::command_option_choice(label, std::to_string(item_id)));
                if (choices.size() >= 25) break;
            }
            for (auto& [key, h] : holdings) {
                if (h.shares <= 0 || choices.size() >= 25) continue;
                auto* sd = find_stock_def(key);
                if (!sd || sd->item_id == 0) continue;
                std::string label = sd->name + " ×" + std::to_string(h.shares) + " 股（ID: " + std::to_string(sd->item_id) + "）";
                if (!filter.empty()) {
                    if (label.find(filter) == std::string::npos &&
                        std::to_string(sd->item_id).find(filter) == std::string::npos) continue;
                }
                choices.push_back(dpp::command_option_choice(label, std::to_string(sd->item_id)));
            }
            dpp::interaction_response res(dpp::ir_autocomplete_reply);
            for (auto& c : choices) res.add_autocomplete_choice(c);
            ev.from()->creator->interaction_response_create(ev.command.id, ev.command.token, res);
            return;
        }
    });

    // ── 斜線指令 ──────────────────────────────────────────────────────────────
    bot.on_slashcommand([&bot](const dpp::slashcommand_t& ev) {
        const std::string  cmd_name = ev.command.get_command_name();
        const dpp::user&   user     = ev.command.get_issuing_user();
        dpp::snowflake     uid      = user.id;
        dpp::snowflake     ch       = ev.command.channel_id;

        if (cmd_name == "王團報名" || cmd_name == "王團紀錄" ||
            cmd_name == "raid" || cmd_name == "raidlog") {
            invalidate_old_msg(bot, uid);
            dpp::message m = (cmd_name == "王團報名" || cmd_name == "raid")
                             ? make_boss_msg(user) : make_records_select_msg(user);
            ev.reply(dpp::ir_channel_message_with_source, m);
            ev.get_original_response([uid, ch](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    const auto& m = std::get<dpp::message>(cb.value);
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[m.id] = uid;
                    user_active_msg[uid] = {m.id, ch};
                }
            });
        }
        else if (cmd_name == "ping") {
            ev.reply("Pong! 🏓");
        }
        else if (cmd_name == "公告" || cmd_name == "announcement") {
            std::string text;
            auto tp = ev.get_parameter("內容");
            if (std::holds_alternative<std::string>(tp)) text = std::get<std::string>(tp);
            if (text.empty()) {
                ev.reply(dpp::ir_channel_message_with_source, make_announcement_view_msg());
            } else if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限設定公告！").set_flags(dpp::m_ephemeral));
            } else {
                std::string dn = ev.command.member.get_nickname().empty() ? user.username : ev.command.member.get_nickname();
                ev.reply(dpp::ir_channel_message_with_source, dpp::message(set_announcement(text, dn)));
            }
        }
        else if (cmd_name == "小黑屋") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 沒有權限！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_channel_message_with_source, make_claim_jail_msg());
        }
        else if (cmd_name == "幫助" || cmd_name == "help") {
            ev.reply(dpp::ir_channel_message_with_source, make_help_msg(0));
        }
        else if (cmd_name == "領取" || cmd_name == "claim") {
            bool claimed = false, challenged = false; uint64_t claim_token = 0;
            dpp::message m = handle_claim(uid, &claimed, &challenged, &claim_token);
            if (claimed) {
                int64_t repaid = bank_auto_repay(uid, CLAIM_AMOUNT / 2);
                if (repaid > 0) {
                    save_chips(); save_bank();
                    if (!m.embeds.empty())
                        m.embeds[0].add_field("💳 自動還款", std::to_string(repaid) + " 碼已從本次領取扣除", false);
                }
            }
            // Daily hunt scroll grant — same logic as !領取
            {
                bool gave_scrolls = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto& cd = chip_data[uid];
                    time_t now2 = time(nullptr);
                    struct tm now_tm{}, last_tm{};
                    localtime_s(&now_tm, &now2);
                    localtime_s(&last_tm, &cd.last_hunt_daily);
                    bool new_day = (now_tm.tm_year != last_tm.tm_year || now_tm.tm_yday != last_tm.tm_yday);
                    if (new_day) {
                        int cur_scrolls = inventory_data[uid].count("hunt_scroll")
                                          ? inventory_data[uid]["hunt_scroll"] : 0;
                        int to_give = std::max(0, 2 - cur_scrolls);
                        if (to_give > 0) {
                            inventory_data[uid]["hunt_scroll"] += to_give;
                            gave_scrolls = true;
                        }
                        cd.last_hunt_daily = now2;
                    }
                }
                if (gave_scrolls) {
                    save_chips(); save_inventory();
                    if (!m.embeds.empty())
                        m.embeds[0].add_field("📜 每日狩獵卷", "已補滿至 **2 張**（上限）", false);
                }
            }
            ev.reply(dpp::ir_channel_message_with_source, m.set_flags(dpp::m_ephemeral));
            if (challenged) schedule_claim_verify_timeout_interaction(uid, ev.command.token, claim_token);
        }
        else if (cmd_name == "每週領取" || cmd_name == "weekly") {
            bool claimed = false;
            dpp::message m = handle_weekly_claim(uid, &claimed);
            if (claimed) {
                int64_t repaid = bank_auto_repay(uid, WEEKLY_AMOUNT / 2);
                if (repaid > 0) {
                    save_chips(); save_bank();
                    if (!m.embeds.empty())
                        m.embeds[0].add_field("💳 自動還款", std::to_string(repaid) + " 碼已從本次領取扣除", false);
                }
            }
            ev.reply(dpp::ir_channel_message_with_source, m.set_flags(dpp::m_ephemeral));
        }
        else if (cmd_name == "銀行" || cmd_name == "bank") {
            ev.reply(dpp::ir_channel_message_with_source, make_bank_msg(uid));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (cmd_name == "錢包" || cmd_name == "wallet") {
            ev.reply(dpp::ir_channel_message_with_source, make_wallet_home_msg(uid));
        }
        else if (cmd_name == "富豪榜" || cmd_name == "leaderboard") {
            ev.reply(dpp::ir_channel_message_with_source, handle_leaderboard(0));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (cmd_name == "商店" || cmd_name == "shop") {
            ev.reply(dpp::ir_channel_message_with_source, make_shop_main_msg(std::to_string((uint64_t)uid)));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (cmd_name == "記帳" || cmd_name == "ledger") {
            bool adm_ledger = is_admin(ev.command);
            ev.reply(dpp::ir_channel_message_with_source,
                adm_ledger ? make_ledger_msg(0) : make_my_ledger_msg(uid));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (cmd_name == "大廳" || cmd_name == "lobby" || cmd_name == "寵物" || cmd_name == "pet") {
            handle_pet_slash(ev, cmd_name);
        }
        else if (cmd_name == "收藏" || cmd_name == "collect" ||
                 cmd_name == "探險" || cmd_name == "adventure" ||
                 cmd_name == "強化" || cmd_name == "enhance") {
            handle_adv_slash(ev, cmd_name, uid, ch); return;
        }
        else if (cmd_name == "猜拳" || cmd_name == "janken") {
            handle_rps_slash(ev, uid, ch);
        }
        else if (cmd_name == "股票" || cmd_name == "stock") {
            std::string dn = ev.command.member.get_nickname();
            ev.reply(dpp::ir_channel_message_with_source,
                make_stock_home_msg(uid, dn, user.get_avatar_url()));
        }
        else if (cmd_name == "背包" || cmd_name == "bag" || cmd_name == "petuse" || cmd_name == "寵物圖鑑" || cmd_name == "petdex") {
            handle_pet_slash(ev, cmd_name);
        }
        else if (cmd_name == "虧損榜" || cmd_name == "lossboard") {
            ev.reply(dpp::ir_channel_message_with_source, handle_losers_board(0, ""));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (cmd_name == "射" || cmd_name == "inbetween" ||
                 cmd_name == "火箭" || cmd_name == "rocket" ||
                 cmd_name == "刮刮樂" || cmd_name == "scratch" ||
                 cmd_name == "骰子" || cmd_name == "dice") {
            handle_games_slash(ev, cmd_name, uid, ch); return;
        }
        // ── 21點 slash → handlers_bj.cpp ─────────────────────────────────────
        else if (cmd_name == "21" || cmd_name == "blackjack") {
            handle_bj_slash(ev, cmd_name, uid, ch); return;
        }
        else if (cmd_name == "警告" || cmd_name == "warn") {
            dpp::snowflake target = std::get<dpp::snowflake>(ev.get_parameter("對象"));
            std::string reason;
            auto rp = ev.get_parameter("原因");
            if (std::holds_alternative<std::string>(rp)) reason = std::get<std::string>(rp);
            std::string target_name = "<@" + std::to_string((uint64_t)target) + ">";
            ev.reply(dpp::ir_channel_message_with_source, handle_warn(target, target_name, reason));
        }
        else if (cmd_name == "警告榜單" || cmd_name == "warnboard") {
            ev.reply(dpp::ir_channel_message_with_source, handle_warn_board());
        }
        else if (cmd_name == "幸運頻道" || cmd_name == "lucky") {
            int64_t max_ch = std::get<int64_t>(ev.get_parameter("最大頻道數"));
            if (max_ch < 1) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 最大頻道數必須大於 0").set_flags(dpp::m_ephemeral));
                return;
            }
            std::mt19937 rng(std::random_device{}());
            int lucky = std::uniform_int_distribution<int>(1, (int)max_ch)(rng);
            dpp::embed e;
            e.set_title("🎰  幸運頻道").set_color(0xF39C12);
            e.set_description("🍀  本次幸運頻道是 **頻道 " + std::to_string(lucky) + "**！");
            ev.reply(dpp::ir_channel_message_with_source, dpp::message().add_embed(e));
        }
        else if (cmd_name == "轉帳" || cmd_name == "transfer") {
            dpp::snowflake to_uid = std::get<dpp::snowflake>(ev.get_parameter("對象"));
            int64_t amount        = std::get<int64_t>(ev.get_parameter("金額"));
            bool has_loan_block = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = bank_data.find(uid);
                has_loan_block = (it != bank_data.end() && it->second.loan > 0);
            }
            if (has_loan_block) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你有未還清的借款，無法轉帳！請先至 `!銀行` 還清借款。").set_flags(dpp::m_ephemeral));
            } else {
                std::string from_name = "<@" + std::to_string((uint64_t)uid)    + ">";
                std::string to_name   = "<@" + std::to_string((uint64_t)to_uid) + ">";
                ev.reply(dpp::ir_channel_message_with_source,
                    handle_transfer_request(uid, from_name, to_uid, to_name, amount));
            }
        }
        else if (cmd_name == "交易" || cmd_name == "trade") {
            dpp::snowflake target = std::get<dpp::snowflake>(ev.get_parameter("對象"));
            auto get_int = [&](const std::string& name) -> int64_t {
                auto p = ev.get_parameter(name);
                return p.index() == 0 ? 0 : std::get<int64_t>(p);
            };
            // 我的道具 is now string (autocomplete) — parse as integer ID
            int from_item_id = 0;
            { auto p = ev.get_parameter("我的道具");
              if (p.index() != 0) try { from_item_id = std::stoi(std::get<std::string>(p)); } catch (...) {} }
            int64_t from_qty   = get_int("我的道具數量"); if (from_qty <= 0) from_qty = 1;
            int64_t from_chips = get_int("我的籌碼");
            int to_item_id   = (int)get_int("對方道具");
            int64_t to_qty   = get_int("對方道具數量"); if (to_qty <= 0) to_qty = 1;
            int64_t to_chips = get_int("對方籌碼");
            if (target == uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不能和自己交易！").set_flags(dpp::m_ephemeral)); return;
            }
            auto [from_key2, from_iname2] = trade_item_info(from_item_id);
            if (from_item_id && from_key2.empty()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 道具 ID `" + std::to_string(from_item_id) + "` 不存在！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!from_key2.empty()) {
                if (trade_item_blocked(uid, from_key2)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ **" + from_iname2 + "** 不可交易！").set_flags(dpp::m_ephemeral)); return;
                }
                if (!trade_has_item(uid, from_key2, from_qty)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你沒有 **" + std::to_string(from_qty) + "** 個/股 **" + from_iname2 + "**！").set_flags(dpp::m_ephemeral)); return;
                }
            }
            if (from_chips < 0) from_chips = 0;
            if (from_chips > 0) {
                bool has_lovebook_chk2 = false;
                { std::lock_guard<std::mutex> lk(data_mutex); has_lovebook_chk2 = col_has_lovebook(uid); }
                int64_t from_fee_chk2 = has_lovebook_chk2 ? 0 : (from_chips + 99) / 100;
                if (get_chips(uid) < from_chips + from_fee_chk2) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你的籌碼不足（含 1% 手續費 " + std::to_string(from_fee_chk2) + " 碼，需 " + std::to_string(from_chips + from_fee_chk2) + " 碼）！").set_flags(dpp::m_ephemeral)); return;
                }
            }
            if (to_chips < 0) to_chips = 0;
            auto [to_key2, to_iname2] = trade_item_info(to_item_id);
            if (to_item_id && to_key2.empty()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 對方道具 ID `" + std::to_string(to_item_id) + "` 不存在！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!to_key2.empty() && trade_item_blocked(target, to_key2)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ **" + to_iname2 + "** 不可交易！").set_flags(dpp::m_ephemeral)); return;
            }
            TradeOffer t;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                t.id = trade_counter++;
                t.from_uid = uid; t.to_uid = target; t.channel_id = ch;
                t.from_item_id = from_item_id; t.from_qty = from_qty; t.from_chips = from_chips;
                t.to_item_id   = to_item_id;   t.to_qty   = to_qty;   t.to_chips   = to_chips;
                t.created_at   = time(nullptr);
                trade_offers[t.id] = t;
            }
            std::string from_name = ev.command.get_issuing_user().username;
            std::string to_name   = "<@" + std::to_string((uint64_t)target) + ">";
            ev.reply(dpp::ir_channel_message_with_source, make_trade_msg(t, from_name, to_name));
            ev.get_original_response([tid = t.id, uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (cmd_name == "輪盤" || cmd_name == "roulette") {
            handle_roulette_slash(ev, uid, ch); return;
        }
        // ── wolf/onenight slash → handlers_wolf.cpp ───────────────────────────
        else if (cmd_name == "狼人殺" || cmd_name == "werewolf" ||
                 cmd_name == "一夜狼人" || cmd_name == "onenight") {
            handle_wolf_slash(ev, cmd_name, uid, ch); return;
        }
        // ── 誰是臥底 slash → handlers_uc.cpp ─────────────────────────────────
        else if (cmd_name == "臥底" || cmd_name == "誰是臥底" || cmd_name == "undercover") {
            handle_uc_slash(ev, cmd_name, uid, ch); return;
        }
        else if (cmd_name == "抽獎" || cmd_name == "giveaway") {
            if (!is_draw_authorized(ev.command)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 只有管理員或副會長才能開抽獎！").set_flags(dpp::m_ephemeral));
                return;
            }

            // Required parameters
            std::string time_str    = std::get<std::string>(ev.get_parameter("時間"));
            int64_t     winner_cnt  = std::get<int64_t>(ev.get_parameter("獲獎人數"));
            std::string prize       = std::get<std::string>(ev.get_parameter("獎品名稱"));

            // Target channel (defaults to current channel)
            dpp::snowflake target_ch = ch;
            auto ch_param = ev.get_parameter("抽獎頻道");
            if (std::holds_alternative<dpp::snowflake>(ch_param))
                target_ch = std::get<dpp::snowflake>(ch_param);

            // Optional parameters
            std::string provider, mention, note;
            dpp::snowflake role_restriction = 0;
            std::string role_name;

            auto prov = ev.get_parameter("提供者");
            if (std::holds_alternative<dpp::snowflake>(prov))
                provider = "<@" + std::to_string(std::get<dpp::snowflake>(prov)) + ">";

            auto ment = ev.get_parameter("提及");
            if (std::holds_alternative<dpp::snowflake>(ment))
                mention = "<@" + std::to_string(std::get<dpp::snowflake>(ment)) + ">";

            auto note_p = ev.get_parameter("備註");
            if (std::holds_alternative<std::string>(note_p))
                note = std::get<std::string>(note_p);

            auto role_p = ev.get_parameter("限制身分組");
            if (std::holds_alternative<dpp::snowflake>(role_p)) {
                role_restriction = std::get<dpp::snowflake>(role_p);
                const dpp::role* r = dpp::find_role(role_restriction);
                role_name = r ? ("<@&" + std::to_string(role_restriction) + ">")
                              : std::to_string(role_restriction);
            }

            int64_t entry_cost = 0;
            auto cost_p = ev.get_parameter("報名費");
            if (std::holds_alternative<int64_t>(cost_p))
                entry_cost = std::max(int64_t(0), std::get<int64_t>(cost_p));

            Giveaway gw;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                gw.id               = giveaway_counter++;
                gw.channel_id       = target_ch;
                gw.host_id          = uid;
                gw.prize            = prize;
                gw.winner_count     = (int)std::max(int64_t(1), winner_cnt);
                gw.provider         = provider;
                gw.mention          = mention;
                gw.note             = note;
                gw.role_restriction = role_restriction;
                gw.role_name        = role_name;
                gw.end_time         = time(nullptr) + parse_duration(time_str);
                gw.entry_cost       = entry_cost;
                giveaways[gw.id]    = gw;
            }

            // Confirm to caller (ephemeral)
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 抽獎已建立！").set_flags(dpp::m_ephemeral));

            // Post the giveaway in the target channel
            save_giveaways();
            bot.message_create(make_giveaway_msg(gw),
                [gid = gw.id](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        auto& m = std::get<dpp::message>(cb.value);
                        std::lock_guard<std::mutex> lk(data_mutex);
                        giveaways[gid].msg_id     = m.id;
                        giveaways[gid].channel_id = m.channel_id;
                    }
                    save_giveaways();
                });
        }
        else if (cmd_name == "合成" || cmd_name == "craft") {
            ev.reply(dpp::ir_channel_message_with_source, make_craft_msg(uid));
        }
        // ── 怪物狩獵 slash → handlers_hunt.cpp ───────────────────────────────
        else if (cmd_name == "怪物狩獵" || cmd_name == "hunt" ||
                 cmd_name == "狩獵規則" || cmd_name == "huntrules") {
            handle_hunt_slash(ev, cmd_name, uid, ch); return;
        }
        else if (cmd_name == "裝備" || cmd_name == "equip") {
            Pet pet2;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = pet_data.find(uid); if (it != pet_data.end()) pet2 = it->second; }
            std::string _dn = ev.command.member.get_nickname().empty() ? user.username : ev.command.member.get_nickname();
            std::string _av = user.get_avatar_url();
            ev.reply(dpp::ir_channel_message_with_source, make_equip_msg(uid, pet2, _dn, _av));
        }
        else if (cmd_name == "裝備圖鑑" || cmd_name == "equipdex") {
            ev.reply(dpp::ir_channel_message_with_source, make_equipdex_main_msg(uid));
        }
        else if (cmd_name == "道具圖鑑" || cmd_name == "itemdex") {
            ev.reply(dpp::ir_channel_message_with_source, make_itemdex_main_msg(uid));
        }
        else if (cmd_name == "轉蛋" || cmd_name == "gacha") {
            std::string _dn = ev.command.member.get_nickname().empty() ? user.username : ev.command.member.get_nickname();
            std::string _av = user.get_avatar_url();
            ev.reply(dpp::ir_channel_message_with_source, make_gacha_main_msg(uid, _dn, _av));
        }
        else if (cmd_name == "卷軸使用" || cmd_name == "scroll") {
            int pct = 0, cnt = 1;
            auto pp = ev.get_parameter("成功率");
            auto cp2 = ev.get_parameter("張數");
            if (std::holds_alternative<int64_t>(pp)) pct = (int)std::get<int64_t>(pp);
            if (std::holds_alternative<int64_t>(cp2)) cnt = (int)std::get<int64_t>(cp2);
            if (pct != 10 && pct != 30 && pct != 60 && pct != 70) pct = 0;
            if (cnt <= 0) cnt = 1; if (cnt > 100) cnt = 100;
            if (pct == 0)
                ev.reply(dpp::ir_channel_message_with_source, make_scroll_sel_msg(uid));
            else
                ev.reply(dpp::ir_channel_message_with_source, make_scroll_result_msg(uid, pct, cnt));
        }
        else if (cmd_name == "一夜狼人規則" || cmd_name == "onwrules") {
            dpp::embed e;
            e.set_title("🌙  一夜終極狼人 — 規則").set_color(0x2C3E50);
            e.set_description("3～8 人遊玩。每人有一張身份牌，另有 3 張中央牌（不屬於任何人）。\n"
                "遊戲分為**夜晚階段**（依序行動）和**白天階段**（討論+投票）。");
            e.add_field("🌙 夜晚行動順序",
                "1️⃣ **狼人**：互相確認身份（有頭狼時可換中央牌）\n"
                "2️⃣ **預言家**：查驗 1 名玩家，或翻 2 張中央牌\n"
                "3️⃣ **盜賊**：與另一名玩家換牌\n"
                "4️⃣ **捣蛋鬼**：交換兩名其他玩家的牌\n"
                "5️⃣ **村莊白痴**：查看自己的牌（確認是白痴）\n"
                "6️⃣ **女巫**：偷看 1 張中央牌，可選擇與任意玩家交換\n"
                "7️⃣ **醉漢**：與中央牌盲換（不看新牌）\n"
                "8️⃣ **失眠者**：確認自己的牌是否被換", false);
            e.add_field("☀️ 白天", "所有人討論後投票，得票最多者出局（平票則無人出局）。", false);
            e.add_field("🏆 勝負", "狼人出局 → 好人勝｜好人出局 → 狼人勝｜丹寧匠被投死 → 丹寧匠勝", false);
            e.add_field("💰 獎勵", "獲勝方 **+300** 碼，落敗方 **+100** 碼", false);
            e.set_footer(dpp::embed_footer().set_text("使用 !一夜狼人 開始遊戲"));
            ev.reply(dpp::ir_channel_message_with_source, dpp::message().add_embed(e));
        }
        else if (cmd_name == "狼人殺規則" || cmd_name == "wwrules") {
            dpp::embed e;
            e.set_title("🐺  狼人殺 — 規則").set_color(0x8B0000);
            e.set_description("**固定 9 人**遊玩。角色：狼人×3、村民×3、預言家×1、女巫×1、獵人×1。");
            e.add_field("🐺 狼人 ×3", "夜晚商議投票，目標：狼人數 ≥ 好人或屠滅所有村民/神職。", false);
            e.add_field("🏘️ 村民 ×3", "無特殊技能，靠白天討論推理。目標：活到狼人全滅。", false);
            e.add_field("🔮 預言家 ×1", "每夜查驗一名玩家，獲知其陣營。", false);
            e.add_field("🧪 女巫 ×1", "解藥×1（救今晚被殺的人）、毒藥×1（毒死任意一人），各用一次。被女巫毒死的獵人不能開槍。", false);
            e.add_field("🏹 獵人 ×1", "死亡時帶走一名玩家（被女巫毒死時不能開槍）。", false);
            e.add_field("🌙 夜晚", "狼人投票 → 預言家查驗 → 女巫選擇", false);
            e.add_field("☀️ 白天", "公布死亡 → 討論 → 投票放逐（平票 PK）", false);
            e.add_field("🏆 勝負", "好人勝：所有狼人死亡 ｜ 狼人勝：狼數≥好人 或 屠滅村民/神職", false);
            e.set_footer(dpp::embed_footer().set_text("使用 !狼人殺 開始遊戲（需滿 9 人）"));
            ev.reply(dpp::ir_channel_message_with_source, dpp::message().add_embed(e));
        }
        else if (cmd_name == "狼人殺榜單" || cmd_name == "wwboard") {
            ev.reply(dpp::ir_channel_message_with_source, make_wolf_leaderboard_msg());
        }
        else if (cmd_name == "猜數字" || cmd_name == "guess") {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (guess_games.count(uid)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你已有進行中的猜數字遊戲！").set_flags(dpp::m_ephemeral)); return;
                }
                GuessGame g;
                g.uid          = uid;
                g.channel_id   = ch;
                g.secret       = guess_gen_secret();
                g.avatar_url   = user.get_avatar_url();
                g.display_name = ev.command.member.get_nickname().empty()
                                 ? user.username : ev.command.member.get_nickname();
                guess_games[uid] = g;
            }
            GuessGame snap; { std::lock_guard<std::mutex> lk(data_mutex); snap = guess_games[uid]; }
            ev.reply(dpp::ir_channel_message_with_source, make_guess_msg(snap));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (guess_games.count(uid))
                        guess_games[uid].msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
    });

    // ── 伺服器連線：抓 emoji 名稱 + 即時註冊 guild slash 指令 ────────────────
    bot.on_guild_create([&bot](const dpp::guild_create_t& ev) {
        dpp::snowflake gid = ev.created.id;

        // 抓 emoji（try-catch 防止型別不符崩潰）
        bot.guild_emojis_get(gid, [](const dpp::confirmation_callback_t& cb) {
            if (cb.is_error()) return;
            try {
                auto& emojis = std::get<dpp::emoji_map>(cb.value);
                std::lock_guard<std::mutex> lk(data_mutex);
                for (auto& [id, emoji] : emojis) {
                    std::string tag = "<:" + emoji.name + ":" + std::to_string(id) + ">";
                    emoji_tag_map[id]          = tag;
                    emoji_name_map[emoji.name] = tag;
                }
            } catch (...) {}
        });

        // 用 bulk_create 一次取代所有 guild commands（自動移除舊指令如 /狀態）
        if (dpp::run_once<struct register_guild_cmds>()) {
            dpp::slashcommand bj("21", "用籌碼玩 21 點", bot.me.id);
            bj.add_option(dpp::command_option(dpp::co_string, "籌碼", "下注籌碼量（數字 或 ALL）", true));

            dpp::slashcommand draw("抽獎", "開始抽獎（需要管理員或副會長）", bot.me.id);
            draw.add_option(dpp::command_option(dpp::co_string,  "時間",      "抽獎時長，例: 5h 30m", true))
                .add_option(dpp::command_option(dpp::co_integer, "獲獎人數",  "中獎人數",             true))
                .add_option(dpp::command_option(dpp::co_string,  "獎品名稱",  "獎品名稱",             true))
                .add_option(dpp::command_option(dpp::co_channel, "抽獎頻道",  "在哪個頻道發起抽獎",   true))
                .add_option(dpp::command_option(dpp::co_role,    "限制身分組","限制特定身分組才能參加",false))
                .add_option(dpp::command_option(dpp::co_user,    "提供者",    "獎品提供者",           false))
                .add_option(dpp::command_option(dpp::co_user,    "提及",      "特別提及的對象",        false))
                .add_option(dpp::command_option(dpp::co_string,  "備註",      "備註說明",             false))
                .add_option(dpp::command_option(dpp::co_integer, "報名費",    "參加需花費的籌碼數（0為免費）", false));

            dpp::slashcommand warn_cmd("警告", "警告成員並記錄次數", bot.me.id);
            warn_cmd.add_option(dpp::command_option(dpp::co_user,   "對象", "要警告的成員",       true))
                    .add_option(dpp::command_option(dpp::co_string, "原因", "警告原因（可省略）", false));

            dpp::slashcommand announce_cmd("公告", "查看／設定大廳最新更新（設定限管理員/副會長）", bot.me.id);
            announce_cmd.add_option(dpp::command_option(dpp::co_string, "內容", "留空＝查看；填寫＝設定新公告（限管理員/副會長）", false));

            dpp::slashcommand claimjail_cmd("小黑屋", "查看／解除領取驗證的鎖定（限管理員）", bot.me.id);

            dpp::slashcommand announce_en("announcement", "View or set the lobby announcement (admin/officer to set)", bot.me.id);
            announce_en.add_option(dpp::command_option(dpp::co_string, "內容", "Leave empty to view; fill in to set (admin/officer only)", false));

            dpp::slashcommand lucky("幸運頻道", "隨機抽出幸運頻道號碼", bot.me.id);
            lucky.add_option(dpp::command_option(dpp::co_integer, "最大頻道數", "頻道總數（抽 1 到此數）", true));

            dpp::slashcommand transfer("轉帳", "轉移籌碼給其他人", bot.me.id);
            transfer.add_option(dpp::command_option(dpp::co_user,    "對象", "收款人",   true))
                    .add_option(dpp::command_option(dpp::co_integer, "金額", "轉帳碼數", true));

            dpp::slashcommand dice_cmd("骰子", "擲骰子押注", bot.me.id);
            dice_cmd.add_option(dpp::command_option(dpp::co_string, "籌碼", "下注碼數（數字 或 ALL）", true));

            dpp::slashcommand dice_en("dice", "Roll dice to bet chips", bot.me.id);
            dice_en.add_option(dpp::command_option(dpp::co_string, "籌碼", "Bet amount (number or ALL)", true));

            dpp::slashcommand bj_en("blackjack", "Play 21/Blackjack with chips", bot.me.id);
            bj_en.add_option(dpp::command_option(dpp::co_string, "籌碼", "Bet amount (number or ALL)", true));

            dpp::slashcommand draw_en("giveaway", "Start a giveaway (admin/officer only)", bot.me.id);
            draw_en.add_option(dpp::command_option(dpp::co_string,  "時間",      "Duration, e.g. 5h 30m",     true))
                   .add_option(dpp::command_option(dpp::co_integer, "獲獎人數",  "Number of winners",          true))
                   .add_option(dpp::command_option(dpp::co_string,  "獎品名稱",  "Prize name",                 true))
                   .add_option(dpp::command_option(dpp::co_channel, "抽獎頻道",  "Channel to post giveaway",   true))
                   .add_option(dpp::command_option(dpp::co_role,    "限制身分組","Restrict to a role",         false))
                   .add_option(dpp::command_option(dpp::co_user,    "提供者",    "Prize provider",             false))
                   .add_option(dpp::command_option(dpp::co_user,    "提及",      "Mention target",             false))
                   .add_option(dpp::command_option(dpp::co_string,  "備註",      "Extra notes",                false))
                   .add_option(dpp::command_option(dpp::co_integer, "報名費",    "Entry fee in chips (0=free)",false));

            dpp::slashcommand shoot_cmd("射", "射龍門 — 猜中間牌贏籌碼", bot.me.id);
            shoot_cmd.add_option(dpp::command_option(dpp::co_string, "籌碼", "下柱碼數（數字 或 ALL）", true));

            dpp::slashcommand shoot_en("inbetween", "In-Between card game", bot.me.id);
            shoot_en.add_option(dpp::command_option(dpp::co_string, "籌碼", "Bet amount (number or ALL)", true));

            dpp::slashcommand rocket_cmd("火箭", "火箭升空 — 每次 1.35x，小心爆炸！", bot.me.id);
            rocket_cmd.add_option(dpp::command_option(dpp::co_string, "籌碼", "下注碼數（數字 或 ALL）", true));

            dpp::slashcommand rocket_en("rocket", "Rocket Launch — each press x1.35, beware of explosion!", bot.me.id);
            rocket_en.add_option(dpp::command_option(dpp::co_string, "籌碼", "Bet amount (number or ALL)", true));

            dpp::slashcommand scratch_cmd("刮刮樂", "九格刮刮樂 — 刮開格子找倍率，小心炸彈！", bot.me.id);
            scratch_cmd.add_option(dpp::command_option(dpp::co_string, "籌碼", "下注碼數（數字 或 ALL）", true));

            dpp::slashcommand scratch_en("scratch", "Scratch Card — reveal squares for multipliers, avoid bombs!", bot.me.id);
            scratch_en.add_option(dpp::command_option(dpp::co_string, "籌碼", "Bet amount (number or ALL)", true));

            // English aliases with parameters
            dpp::slashcommand warn_en("warn", "Warn a member", bot.me.id);
            warn_en.add_option(dpp::command_option(dpp::co_user,   "對象", "Target member", true))
                   .add_option(dpp::command_option(dpp::co_string, "原因", "Reason (optional)", false));

            dpp::slashcommand lucky_en("lucky", "Draw a lucky channel number", bot.me.id);
            lucky_en.add_option(dpp::command_option(dpp::co_integer, "最大頻道數", "Max channel count", true));

            dpp::slashcommand transfer_en("transfer", "Transfer chips to another user", bot.me.id);
            transfer_en.add_option(dpp::command_option(dpp::co_user,    "對象", "Recipient",       true))
                       .add_option(dpp::command_option(dpp::co_integer, "金額", "Amount to send",  true));

            dpp::slashcommand scroll_cmd("卷軸使用", "使用成長卷軸（不帶參數=選擇介面）", bot.me.id);
            scroll_cmd.add_option(dpp::command_option(dpp::co_integer, "成功率", "卷軸成功率：10/30/60/70", false))
                      .add_option(dpp::command_option(dpp::co_integer, "張數",   "要使用幾張（最多100）",   false));

            dpp::slashcommand scroll_en("scroll", "Use growth scrolls", bot.me.id);
            scroll_en.add_option(dpp::command_option(dpp::co_integer, "成功率", "Scroll success rate: 10/30/60/70", false))
                     .add_option(dpp::command_option(dpp::co_integer, "張數",   "How many to use (max 100)",         false));

            dpp::slashcommand trade_cmd("交易", "向另一名玩家發起道具/籌碼交易", bot.me.id);
            trade_cmd.add_option(dpp::command_option(dpp::co_user, "對象", "交易對象", true));
            { auto p = dpp::command_option(dpp::co_string, "我的道具", "我出的道具（從清單選擇或手動輸入ID）", false);
              p.set_auto_complete(true); trade_cmd.add_option(p); }
            trade_cmd.add_option(dpp::command_option(dpp::co_integer, "我的道具數量", "我出的道具數量（預設1，股票可填股數）", false))
                     .add_option(dpp::command_option(dpp::co_integer, "我的籌碼", "我出的籌碼（0=無）",  false))
                     .add_option(dpp::command_option(dpp::co_integer, "對方道具", "要對方出的道具ID（0=無）", false))
                     .add_option(dpp::command_option(dpp::co_integer, "對方道具數量", "要對方出的道具數量（預設1）", false))
                     .add_option(dpp::command_option(dpp::co_integer, "對方籌碼", "要對方出的籌碼（0=無）",  false));

            dpp::slashcommand trade_en("trade", "Propose an item/chip trade with another player", bot.me.id);
            trade_en.add_option(dpp::command_option(dpp::co_user, "對象", "Trade target", true));
            { auto p = dpp::command_option(dpp::co_string, "我的道具", "Your item (pick from list or type ID)", false);
              p.set_auto_complete(true); trade_en.add_option(p); }
            trade_en.add_option(dpp::command_option(dpp::co_integer, "我的道具數量", "Your item quantity (default 1, shares for stocks)", false))
                    .add_option(dpp::command_option(dpp::co_integer, "我的籌碼", "Your chips (0=none)",   false))
                    .add_option(dpp::command_option(dpp::co_integer, "對方道具", "Their item ID (0=none)",false))
                    .add_option(dpp::command_option(dpp::co_integer, "對方道具數量", "Their item quantity (default 1)", false))
                    .add_option(dpp::command_option(dpp::co_integer, "對方籌碼", "Their chips (0=none)",  false));

            dpp::slashcommand roulette_cmd("輪盤", "向玩家發起俄羅斯輪盤（賭注輸贏）", bot.me.id);
            roulette_cmd.add_option(dpp::command_option(dpp::co_integer, "籌碼", "下注籌碼數量", true))
                        .add_option(dpp::command_option(dpp::co_user,    "對象", "邀請特定玩家（選填）", false));

            dpp::slashcommand roulette_en("roulette", "Challenge someone to a roulette duel", bot.me.id);
            roulette_en.add_option(dpp::command_option(dpp::co_integer, "籌碼", "Bet amount", true))
                       .add_option(dpp::command_option(dpp::co_user,    "對象", "Invite a specific player (optional)", false));

            dpp::slashcommand rps_cmd("猜拳", "開 2~5 人猜拳房間，同時出拳結算", bot.me.id);
            rps_cmd.add_option(dpp::command_option(dpp::co_integer, "籌碼", "下注籌碼數量", true));

            dpp::slashcommand rps_en("janken", "Open a 2–5 player rock-paper-scissors room", bot.me.id);
            rps_en.add_option(dpp::command_option(dpp::co_integer, "籌碼", "Bet amount", true));

            bot.guild_bulk_command_create({
                dpp::slashcommand("ping",      "測試機器人是否在線",            bot.me.id),
                dpp::slashcommand("王團報名",  "王團報名",                      bot.me.id),
                dpp::slashcommand("王團紀錄",  "查看王團報名紀錄",              bot.me.id),
                dpp::slashcommand("幫助",      "查看所有指令說明",              bot.me.id),
                dpp::slashcommand("領取",      "每整點領取 500 碼",             bot.me.id),
                dpp::slashcommand("每週領取",  "每週四可領取 2000 碼",          bot.me.id),
                dpp::slashcommand("錢包",      "查看籌碼量與21點統計",          bot.me.id),
                dpp::slashcommand("富豪榜",    "查看全伺服器籌碼排行榜",        bot.me.id),
                dpp::slashcommand("商店",      "瀏覽並購買道具",                bot.me.id),
                dpp::slashcommand("記帳",      "查看購買記帳本（管理員）",      bot.me.id),
                dpp::slashcommand("警告榜單",  "查看警告次數排行榜",            bot.me.id),
                dpp::slashcommand("大廳",      "進入大廳（寵物/背包/裝備/商店）",bot.me.id),
                dpp::slashcommand("寵物",      "查看你的寵物狀態",              bot.me.id),
                dpp::slashcommand("背包",      "查看背包道具，點選使用",         bot.me.id),
                dpp::slashcommand("寵物圖鑑",  "查看所有寵物進化路線",          bot.me.id),
                dpp::slashcommand("虧損榜",    "查看全伺服器虧損排行榜",        bot.me.id),
                dpp::slashcommand("狼人殺",    "開始狼人殺遊戲（需要9名玩家）", bot.me.id),
                dpp::slashcommand("銀行",      "存款/借款/還款，查看利息",      bot.me.id),
                // English aliases (no-parameter ones)
                dpp::slashcommand("shop",      "Open the shop",                 bot.me.id),
                dpp::slashcommand("wallet",    "Check your chip balance",       bot.me.id),
                dpp::slashcommand("leaderboard","View chip leaderboard",        bot.me.id),
                dpp::slashcommand("lossboard", "View loss leaderboard",         bot.me.id),
                dpp::slashcommand("claim",     "Claim hourly 500 chips",        bot.me.id),
                dpp::slashcommand("weekly",    "Claim weekly 2000 chips",       bot.me.id),
                dpp::slashcommand("help",      "Show command list",             bot.me.id),
                dpp::slashcommand("lobby",     "Open lobby (pet/bag/equip/shop)",bot.me.id),
                dpp::slashcommand("pet",       "View your pet status",          bot.me.id),
                dpp::slashcommand("bag",       "View backpack and use items",   bot.me.id),
                dpp::slashcommand("petdex",    "View pet evolution chart",      bot.me.id),
                dpp::slashcommand("ledger",    "View purchase log (admin)",     bot.me.id),
                dpp::slashcommand("warnboard", "View warning leaderboard",      bot.me.id),
                dpp::slashcommand("werewolf",  "Start a werewolf game",         bot.me.id),
                dpp::slashcommand("一夜狼人",  "開始一夜終極狼人遊戲",            bot.me.id),
                dpp::slashcommand("臥底",      "開始誰是臥底遊戲",                bot.me.id),
                dpp::slashcommand("誰是臥底",  "開始誰是臥底遊戲",                bot.me.id),
                dpp::slashcommand("bank",      "Deposit/borrow/repay chips",    bot.me.id),
                dpp::slashcommand("raid",      "Sign up for raid",              bot.me.id),
                dpp::slashcommand("raidlog",   "View raid sign-up records",     bot.me.id),
                dpp::slashcommand("合成",      "查看並合成寶珠（需要碎片×10）", bot.me.id),
                dpp::slashcommand("craft",     "Craft orbs from shards (×10)",  bot.me.id),
                dpp::slashcommand("怪物狩獵",  "開始怪物狩獵",                  bot.me.id),
                dpp::slashcommand("hunt",      "Start monster hunt",             bot.me.id),
                dpp::slashcommand("狩獵規則",  "查看怪物狩獵規則說明",          bot.me.id),
                dpp::slashcommand("huntrules", "View monster hunt rules",        bot.me.id),
                dpp::slashcommand("裝備",      "查看並管理裝備",                 bot.me.id),
                dpp::slashcommand("equip",     "View and manage equipment",      bot.me.id),
                dpp::slashcommand("裝備圖鑑",  "查看所有裝備列表",               bot.me.id),
                dpp::slashcommand("equipdex",  "View equipment catalog",         bot.me.id),
                dpp::slashcommand("道具圖鑑",  "查看所有道具列表",               bot.me.id),
                dpp::slashcommand("itemdex",   "View item catalog",              bot.me.id),
                dpp::slashcommand("轉蛋",      "開啟轉蛋（抽裝備）",             bot.me.id),
                dpp::slashcommand("gacha",     "Open gacha (draw equipment)",    bot.me.id),
                dpp::slashcommand("一夜狼人規則", "查看一夜終極狼人遊戲規則",    bot.me.id),
                dpp::slashcommand("onwrules",  "View One Night Werewolf rules",  bot.me.id),
                dpp::slashcommand("狼人殺規則", "查看狼人殺遊戲規則",            bot.me.id),
                dpp::slashcommand("wwrules",   "View Werewolf game rules",       bot.me.id),
                dpp::slashcommand("狼人殺榜單", "查看狼人殺勝率排行",            bot.me.id),
                dpp::slashcommand("wwboard",   "View Werewolf leaderboard",      bot.me.id),
                dpp::slashcommand("收藏",      "查看收藏",                        bot.me.id),
                dpp::slashcommand("collect",   "View collection",                bot.me.id),
                dpp::slashcommand("探險",      "前往探險頁面",                    bot.me.id),
                dpp::slashcommand("adventure", "Go to adventure page",           bot.me.id),
                dpp::slashcommand("強化",      "強化寵物攻擊力／防禦力／生命值", bot.me.id),
                dpp::slashcommand("enhance",   "Enhance pet ATK/DEF/HP",         bot.me.id),
                dpp::slashcommand("股票",      "查看並買賣股票",                 bot.me.id),
                dpp::slashcommand("stock",     "View and trade stocks",          bot.me.id),
                dpp::slashcommand("onenight",  "Start One Night Werewolf game",  bot.me.id),
                dpp::slashcommand("undercover","Start Undercover (Who is spy?)", bot.me.id),
                dpp::slashcommand("猜數字",    "猜四位不重複數字（1A2B）",       bot.me.id),
                dpp::slashcommand("guess",     "Guess the 4-digit number (1A2B)",bot.me.id),
                bj, draw, warn_cmd, lucky, transfer, dice_cmd, shoot_cmd, shoot_en,
                rocket_cmd, rocket_en, scratch_cmd, scratch_en,
                warn_en, lucky_en, transfer_en, trade_cmd, trade_en,
                scroll_cmd, scroll_en,
                dice_en, bj_en, draw_en,
                roulette_cmd, roulette_en,
                rps_cmd, rps_en,
                announce_cmd, announce_en,
                claimjail_cmd,
            }, gid);
        }
    });

    // ── on_ready ──────────────────────────────────────────────────────────────
    // 斷線後若 session 過期（無法 resume），DPP 會重新 IDENTIFY，on_ready 會再次觸發。
    // 整個 body 用 run_once 包起來，避免 timer（股價、抽獎、備份...）被重複註冊。
    bot.on_ready([&bot](const dpp::ready_t& event) {
      if (dpp::run_once<struct on_ready_once>()) {
        // 清除舊的 global commands（避免與 guild commands 重複顯示）
        bot.global_bulk_command_create({});

        // 從 bot 應用程式抓 application emoji（需手動帶 Authorization header）
        std::string app_id   = std::to_string((uint64_t)bot.me.id);
        std::string auth_hdr = "Bot " + bot.token;
        bot.request(
            "https://discord.com/api/v10/applications/" + app_id + "/emojis",
            dpp::m_get,
            [](const dpp::http_request_completion_t& result) {
                { FILE* f = fopen("C:\\bot_debug.txt","a");
                  if (f) { fprintf(f,"app_emoji status=%d\n", result.status); fclose(f); } }
                if (result.status != 200) {
                    FILE* f = fopen("C:\\bot_debug.txt","a");
                    if (f) { fprintf(f,"app_emoji error body=%.500s\n", result.body.c_str()); fclose(f); }
                    return;
                }
                try {
                    auto j = nlohmann::json::parse(result.body);
                    if (!j.contains("items")) return;
                    { FILE* f = fopen("C:\\bot_debug.txt","a");
                      if (f) { fprintf(f,"app_emoji loaded %zu items:\n", j["items"].size());
                               for (auto& ej : j["items"])
                                   fprintf(f,"  id=%-22s name=%s\n",
                                           ej["id"].get<std::string>().c_str(),
                                           ej["name"].get<std::string>().c_str());
                               fclose(f); } }
                    std::lock_guard<std::mutex> lk(data_mutex);
                    for (auto& ej : j["items"]) {
                        uint64_t eid = std::stoull(ej["id"].get<std::string>());
                        std::string name = ej["name"].get<std::string>();
                        bool animated   = ej.value("animated", false);
                        emoji_tag_map[eid] = (animated ? "<a:" : "<:")
                                           + name + ":" + std::to_string(eid) + ">";
                        emoji_name_map[name] = emoji_tag_map[eid];
                    }
                } catch (...) {}
            },
            "", "application/json",
            { {"Authorization", auth_hdr} });

        cleanup_expired();
        apply_daily_interest(); // 啟動時補算可能錯過的利息
        bot.start_timer([](dpp::timer)     { cleanup_expired(); },  3600);
        bot.start_timer([&bot](dpp::timer) { check_giveaways(bot); save_giveaways(); }, 30);
        bot.start_timer([](dpp::timer)     { apply_daily_interest(); }, 300); // 每 5 分鐘檢查是否跨日
        start_stock_price_timer(); // 開機立即抓一次股價，之後每 5 分鐘更新

        // ── 每小時自動備份所有 JSON 資料（覆蓋同一份，不累積） ──────────────
        bot.start_timer([](dpp::timer) {
            namespace fs = std::filesystem;
            try {
                fs::create_directories("backup");
                static const char* FILES[] = {
                    "chips.json","bank.json","inventory.json","pets.json",
                    "hunt_clear.json","equipped.json","purchases.json",
                    "rlstats.json","bjstats.json","dicestats.json",
                    "shootstats.json","scratchstats.json","warnings.json", nullptr
                };
                for (int i = 0; FILES[i]; i++) {
                    fs::path src(FILES[i]);
                    if (fs::exists(src))
                        fs::copy_file(src, fs::path("backup") / FILES[i],
                                      fs::copy_options::overwrite_existing);
                }
            } catch (...) {}
        }, 3600);

        // ── 尊爵VIP 自動領取 (每 5 分鐘掃一次) ──────────────────────────────
        bot.start_timer([](dpp::timer) {
            time_t now = time(nullptr);
            int64_t now_hour = now / 3600;
            bool changed = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                for (auto& [uid, cd] : chip_data) {
                    if (cd.vip_until <= now) continue;       // VIP 未啟用或已過期
                    int64_t last_hour = cd.vip_last_claim / 3600;
                    int64_t hours_missed = std::min(now_hour - last_hour, (int64_t)24);
                    if (hours_missed > 0) {
                        cd.chips          += CLAIM_AMOUNT * hours_missed;
                        cd.vip_last_claim  = now;
                        cd.last_claim      = now; // 防止同小時重複手動領取
                        changed            = true;
                    }
                }
            }
            if (changed) save_chips();
        }, 300);

        // ── 寵物監工 自動再派 (每 5 分鐘掃一次) ─────────────────────────────
        bot.start_timer([](dpp::timer) {
            time_t now = time(nullptr);
            bool changed_pet = false, changed_chips = false;
            std::vector<dpp::snowflake> to_redispatch;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                for (auto& [uid, pet] : pet_data) {
                    if (pet.stage == 0) continue;                           // 蛋不能打工
                    if (pet.work_task == 0 || pet.work_end > now) continue; // 無打工或打工中
                    if (pet.work_task == 24) continue;                      // 自願留營表：不觸發監工代打
                    // 探險中不自動再派
                    { auto ai = adv_games.find(uid); if (ai != adv_games.end() && ai->second.pet_along && ai->second.end_time > now) continue; }
                    if (pet.onsen_end > now) continue;                      // 泡溫泉中
                    auto ci = chip_data.find(uid);
                    if (ci == chip_data.end() || ci->second.supervisor_until <= now) continue;
                    // 打工完成超過 10 分鐘才自動再派
                    if (now - pet.work_end < 600) continue;
                    to_redispatch.push_back(uid);
                }
                for (auto uid : to_redispatch) {
                    auto& pet = pet_data[uid];
                    int task = pet.work_task;
                    // 先自動領取上次打工報酬（全額）
                    auto opts = work_options(pet.stage);
                    int idx = (task == 1) ? 0 : (task == 4) ? 1 : 2;
                    int64_t reward   = opts[idx].pay;
                    int     exp_gain = opts[idx].exp_gain;
                    for (auto& s : pet.statuses) {
                        if (s == "受傷") reward = (int64_t)(reward * 0.9);
                        if (s == "憂鬱") reward = (int64_t)(reward * 0.8);
                    }
                    if (pet.talent == "招人喜歡" || (pet.talent2_unlocked && pet.talent2 == "招人喜歡")) reward = (int64_t)(reward * 1.1);
                    if (pet.is_supervisor_work) reward = (int64_t)(reward * 0.6);
                    chip_data[uid].chips += reward;
                    changed_chips = true;
                    if (pet.stage < 3)
                        pet.exp = std::min(pet.exp + exp_gain, exp_needed(pet.stage));
                    else
                        pet.exp += exp_gain;
                    // 再派（監工出勤，領取時收益 ×0.6）
                    int dur_sec = task * 3600;
                    if (pet.talent == "迅捷" || (pet.talent2_unlocked && pet.talent2 == "迅捷")) dur_sec = (int)(dur_sec * 0.9);
                    for (auto& s : pet.statuses) if (s == "疲勞") { dur_sec = (int)(dur_sec * 1.3); break; }
                    pet.work_end           = now + dur_sec;
                    pet.is_supervisor_work = true;
                    pet.work_notified      = false;  // 重派後重新計算通知
                    changed_pet = true;
                }
            }
            if (changed_pet)   save_pet_data();
            if (changed_chips) save_chips();

            // ── 打工完成通知（私訊）──────────────────────────────────────────
            std::vector<dpp::snowflake> to_notify_work;
            std::vector<dpp::snowflake> to_notify_onsen;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                for (auto& [uid, pet] : pet_data) {
                    if (!pet.notify_after_work) continue;
                    // 打工完成
                    if (!pet.work_notified && pet.work_task != 0 && pet.work_end <= now) {
                        pet.work_notified = true;
                        to_notify_work.push_back(uid);
                    }
                    // 溫泉完成
                    if (!pet.onsen_notified && pet.onsen_end > 0 && pet.onsen_end <= now) {
                        pet.onsen_notified = true;
                        to_notify_onsen.push_back(uid);
                    }
                }
            }
            if (!to_notify_work.empty() || !to_notify_onsen.empty()) {
                save_pet_data();
                for (auto nuid : to_notify_work)
                    g_bot->direct_message_create(nuid, dpp::message("🐾 你的寵物打工完成了！快輸入 `!寵物` 去領取獎勵吧！"));
                for (auto nuid : to_notify_onsen)
                    g_bot->direct_message_create(nuid, dpp::message("🛀 你的寵物溫泉回來了！負面狀態已全部清除，快輸入 `!寵物` 去看看牠吧！"));
            }

            // ── 探險完成通知（私訊）──────────────────────────────────────────
            std::vector<dpp::snowflake> to_notify_adv;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                for (auto& [auid, g] : adv_games) {
                    if (!g.notify_on_finish || g.finish_notified) continue;
                    if (g.end_time > now) continue;
                    g.finish_notified = true;
                    to_notify_adv.push_back(auid);
                }
            }
            if (!to_notify_adv.empty()) {
                save_adv_games();
                for (auto nuid : to_notify_adv)
                    g_bot->direct_message_create(nuid, dpp::message("🗺️ 你的探險完成了！快輸入 `!探險` 去收取結果吧！"));
            }
        }, 300);
      } // run_once<on_ready_once>

        printf("Bot 已上線：%s\n", bot.me.username.c_str());
    });

    bot.start(dpp::st_wait);
    return 0;
}
