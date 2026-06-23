#include "team.h"
#include "giveaway.h"
#include "blackjack.h"
#include "dice.h"
#include "pet.h"
#include "shop.h"
#include "help.h"
#include "warn.h"
#include "persistence.h"
#include "werewolf.h"
#include "shoot.h"
#include "rocket.h"
#include "scroll.h"
#include "scratch.h"
#include "wallet.h"

// ─── Helpers shared by slash + message command handlers ───────────────────────

// Send the initial message for a command (bot reply), track ownership.
static void start_cmd(dpp::cluster& bot, dpp::snowflake uid,
                       dpp::snowflake channel_id, dpp::message msg,
                       dpp::snowflake reply_to = 0) {
    invalidate_old_msg(bot, uid);
    if (reply_to) msg.set_reference(reply_to);
    msg.channel_id = channel_id;
    bot.message_create(msg, [uid, channel_id](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            auto& m = std::get<dpp::message>(cb.value);
            std::lock_guard<std::mutex> lk(data_mutex);
            msg_owner[m.id] = uid;
            user_active_msg[uid] = {m.id, channel_id};
        }
    });
}

// ─── Trade helpers ────────────────────────────────────────────────────────────

static dpp::message make_trade_msg(const TradeOffer& t,
                                   const std::string& from_name,
                                   const std::string& to_name,
                                   const std::string& status = "") {
    auto item_desc = [](int id) -> std::string {
        auto vi = find_virtual_item_by_id(id);
        return std::string("`") + std::to_string(id) + "` " + (vi ? vi->name : "未知道具");
    };

    std::string desc;
    desc += from_name + " 向 " + to_name + " 提出交易\n\n";

    desc += "**我方提供：**\n";
    bool from_empty = (!t.from_item_id && t.from_chips <= 0);
    if (t.from_item_id) desc += "• " + item_desc(t.from_item_id) + "\n";
    if (t.from_chips > 0) desc += "• 💰 " + std::to_string(t.from_chips) + " 籌碼\n";
    if (from_empty) desc += "• （無）\n";

    desc += "\n**對方提供：**\n";
    bool to_empty = (!t.to_item_id && t.to_chips <= 0);
    if (t.to_item_id) desc += "• " + item_desc(t.to_item_id) + "\n";
    if (t.to_chips > 0) desc += "• 💰 " + std::to_string(t.to_chips) + " 籌碼\n";
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

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Always use the exe's directory so data files go to the same place regardless of launch path
    if (argc > 0) std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());

    cfg = load_config();
    if (cfg.token.empty()) { fprintf(stderr, "找不到 BOT_TOKEN\n"); return 1; }
    load_chips();
    load_warns();
    load_bjstats();
    load_dicestats();
    load_shootstats();
    load_rocketstats();
    load_scratchstats();
    load_wolf_player_stats();
    load_bj_games();
    load_dice_games();
    load_shop();
    load_purchases();
    load_pet_data();
    load_inventory();
    load_registrations();
    load_proposed_teams();
    load_giveaways();

    dpp::cluster bot(cfg.token, dpp::i_default_intents | dpp::i_message_content);
    g_bot = &bot;
    bot.on_log(dpp::utility::cout_logger());

    // ── 訊息指令 ──────────────────────────────────────────────────────────────
    bot.on_message_create([&bot](const dpp::message_create_t& ev) {
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
                "!錢包","!幫助","!help","!寵物","!背包","!寵物圖鑑","!商店",
                "!管理員權限","!警告榜單","!記帳","!狼人殺","!狼人殺榜單"
            };
            for (auto& s : EXACT) if (content == s) return true;
            // Secret owner-only command
            if (content == "!偷看" && !cfg.notify_user_id.empty() &&
                std::to_string(uid) == cfg.notify_user_id) return true;
            // Prefix-match commands (with args)
            static const std::vector<std::string> PREFIX = {
                "!21 ","!骰子 ","!射 ","!火箭 ","!刮 ",
                "!幸運頻道 ","!警告 ","!轉帳 ","!交易 ","!卷軸使用 ",
            };
            for (auto& s : PREFIX) if (content.rfind(s, 0) == 0) return true;
            // standalone (no args)
            if (content == "!21" || content == "!骰子" || content == "!射" ||
                content == "!火箭" || content == "!刮" || content == "!幸運頻道" ||
                content == "!警告" || content == "!轉帳" || content == "!交易" ||
                content == "!卷軸使用") return true;
            return false;
        };
        if (!is_our_cmd()) return;
        bot.message_delete(ev.msg.id, ch); // 隱藏使用者輸入的 ! 指令

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
        else if (content == "!領取") {
            dpp::message m = handle_claim(uid);
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
        else if (content == "!每週領取") {
            dpp::message m = handle_weekly_claim(uid);
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
            dpp::message m = make_shop_main_msg();
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
        else if (content == "!背包") {
            dpp::message msg = make_pet_use_msg(uid);
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
                .set_label("🥚 給蛋").set_id("admin_egg_btn").set_style(dpp::cos_success));
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🎒 給道具").set_id("admin_item_btn").set_style(dpp::cos_secondary));
            dpp::message m;
            m.set_content("🔑 **管理員面板**\n請選擇操作：");
            m.add_component(row); m.channel_id = ch;
            bot.message_create(m, [&bot, ch](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    dpp::snowflake mid = std::get<dpp::message>(cb.value).id;
                    bot.start_timer([&bot, mid, ch](dpp::timer t) {
                        bot.message_delete(mid, ch); bot.stop_timer(t);
                    }, 30);
                }
            });
        }
        // !骰子 <碼|ALL>
        else if (content.rfind("!骰子", 0) == 0) {
            size_t sp = content.find(' ');
            std::string rest = (sp != std::string::npos) ? content.substr(sp + 1) : "";
            std::string rest_lo = rest; for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (rest_lo == "all");
            int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) {
                    dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                    m.channel_id = ch; bot.message_create(m); return;
                }
            } else {
                if (bet <= 0) {
                    dpp::message m; m.set_content("用法：`!骰子 <籌碼量>`  例：`!骰子 100` 或 `!骰子 ALL`");
                    m.channel_id = ch; bot.message_create(m); return;
                }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    dpp::message m; m.set_content("❌ 此討論串最低下注為 **1,000** 碼！");
                    m.channel_id = ch; bot.message_create(m); return;
                }
            }
            if (get_chips(uid) < bet) {
                dpp::embed e; e.set_title("❌  籌碼不足").set_color(0xE74C3C);
                dpp::message m; m.add_embed(e); m.channel_id = ch; bot.message_create(m); return;
            }
            dpp::message m = start_dice(uid, ch, bet,
                ev.msg.author.get_avatar_url(), ev.msg.author.username);
            m.channel_id = ch; bot.message_create(m);
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
                std::string from_name = "<@" + std::to_string((uint64_t)uid)     + ">";
                std::string to_name   = "<@" + std::to_string((uint64_t)to_uid)  + ">";
                dpp::message m = handle_transfer_request(uid, from_name, to_uid, to_name, amount);
                m.channel_id = ch; bot.message_create(m);
            }
        }
        // !警告 @mention [原因]（原因可省略，所有人可用）
        else if (content.rfind("!警告", 0) == 0 && content.find('@') != std::string::npos) {
            dpp::snowflake target = parse_mention(content);
            if (!target) {
                dpp::message m; m.set_content("用法：`!警告 @對象`  或  `!警告 @對象 原因`");
                m.set_reference(ev.msg.id); m.channel_id = ch;
                bot.message_create(m);
            } else {
                size_t gt = content.find('>');
                std::string reason;
                if (gt != std::string::npos && gt + 2 < content.size())
                    reason = content.substr(gt + 2);
                std::string target_name = "<@" + std::to_string((uint64_t)target) + ">";
                dpp::message m = handle_warn(target, target_name, reason);
                m.set_reference(ev.msg.id); m.channel_id = ch;
                bot.message_create(m);
            }
        }
        else if (content == "!警告榜單") {
            dpp::message m = handle_warn_board();
            m.set_reference(ev.msg.id); m.channel_id = ch;
            bot.message_create(m);
        }
        // !幸運頻道 <最大頻道數>
        else if (content.rfind("!幸運頻道", 0) == 0) {
            std::string rest = content.size() > 9 ? content.substr(content.find(' ') + 1) : "";
            int max_ch = rest.empty() ? 0 : std::atoi(rest.c_str());
            if (max_ch < 1) {
                dpp::message m; m.set_content("用法：`!幸運頻道 最大頻道數`  例：`!幸運頻道 8`");
                m.set_reference(ev.msg.id); m.channel_id = ch;
                bot.message_create(m);
            } else {
                std::mt19937 rng(std::random_device{}());
                int lucky = std::uniform_int_distribution<int>(1, max_ch)(rng);
                dpp::embed e;
                e.set_title("🎰  幸運頻道").set_color(0xF39C12);
                e.set_description("🍀  本次幸運頻道是 **頻道 " + std::to_string(lucky) + "**！");
                dpp::message m; m.add_embed(e);
                m.set_reference(ev.msg.id); m.channel_id = ch;
                bot.message_create(m);
            }
        }
        // !21 <碼|ALL>
        else if (content.rfind("!21", 0) == 0 && (content.size() == 3 || content[3] == ' ')) {
            std::string rest = content.size() > 4 ? content.substr(4) : "";
            std::string rest_lo = rest; for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (rest_lo == "all");
            int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) {
                    dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
            } else {
                if (bet <= 0) {
                    dpp::message m; m.set_content("用法：`!21 <籌碼量>`  例：`!21 100` 或 `!21 ALL`");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    dpp::message m; m.set_content("❌ 此討論串最低下注為 **1,000** 碼！");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
            }
            int64_t bal = get_chips(uid);
            if (bal < bet) {
                dpp::embed e; e.set_title("❌  籌碼不足").set_color(0xE74C3C);
                e.set_description("你持有 **" + std::to_string(bal) + "** 碼，無法下注 **" + std::to_string(bet) + "** 碼。");
                dpp::message m; m.add_embed(e);
                m.set_reference(ev.msg.id); m.channel_id = ch;
                bot.message_create(m); return;
            }
            // Kill any existing game
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = user_bj.find(uid);
                if (it != user_bj.end()) { bj_games.erase(it->second); user_bj.erase(it); }
            }
            add_chips(uid, -bet);
            BJGame g = start_bj(uid, ch, bet,
                ev.msg.author.get_avatar_url(), ev.msg.author.username);
            // Check immediate BJ
            std::string status;
            if (is_blackjack(g.main_hand.cards)) {
                bool dbj = is_blackjack(g.dealer_cards);
                g.game_over = true;
                if (dbj) { add_chips(uid, bet); status = "雙 BJ — 平局！"; }
                else {
                    int64_t win = (int64_t)(bet * 1.5);
                    add_chips(uid, bet + win);
                    status = "🌟 Blackjack！贏得 **" + std::to_string(win) + "** 碼！";
                }
            }
            dpp::message bj_msg = make_bj_msg(g, status);
            bj_msg.set_reference(ev.msg.id); bj_msg.channel_id = ch;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                uint64_t gid = g.id;
                bj_games[gid] = g;
                user_bj[uid]  = gid;
            }
            save_bj_games();
            bot.message_create(bj_msg, [uid, gid = g.id](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    auto& m = std::get<dpp::message>(cb.value);
                    { std::lock_guard<std::mutex> lk(data_mutex);
                      auto it = bj_games.find(gid);
                      if (it != bj_games.end()) it->second.msg_id = m.id; }
                    save_bj_games(); // save with msg_id filled in
                }
            });
        }
        // !射 <碼|ALL>
        else if (content.rfind("!射", 0) == 0 && (content.size() == 4 || content[4] == ' ')) {
            size_t sp = content.find(' ');
            std::string rest = (sp != std::string::npos) ? content.substr(sp + 1) : "";
            // 轉小寫比較
            std::string rest_lo = rest;
            for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (rest_lo == "all");
            int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) {
                    dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
            } else {
                if (bet <= 0) {
                    dpp::message m; m.set_content("用法：`!射 <籌碼量>`  例：`!射 100` 或 `!射 ALL`");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    dpp::message m; m.set_content("❌ 此討論串最低下柱為 **1,000** 碼！");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
            }
            dpp::message start_msg = handle_shoot_start(uid, ch, bet,
                ev.msg.author.get_avatar_url(), ev.msg.member.get_nickname());
            start_msg.set_reference(ev.msg.id); start_msg.channel_id = ch;
            bot.message_create(start_msg, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = shoot_games.find(uid);
                    if (it != shoot_games.end())
                        it->second.msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
        // !火箭 <碼|ALL>
        else if (content.rfind("!火箭", 0) == 0 && (content.size() == 7 || content[7] == ' ')) {
            size_t sp = content.find(' ');
            std::string rest = (sp != std::string::npos) ? content.substr(sp + 1) : "";
            std::string rest_lo = rest;
            for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (rest_lo == "all");
            int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) {
                    dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
            } else {
                if (bet <= 0) {
                    dpp::message m; m.set_content("用法：`!火箭 <籌碼量>`  例：`!火箭 100` 或 `!火箭 ALL`");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    dpp::message m; m.set_content("❌ 此討論串最低下注為 **1,000** 碼！");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
            }
            std::string av  = ev.msg.author.get_avatar_url();
            std::string dn  = ev.msg.member.get_nickname();
            start_cmd(bot, uid, ch, handle_rocket_start(uid, ch, bet, av, dn), ev.msg.id);
        }
        // !卷軸使用 [成功率%] [張數]
        else if (content.rfind("!卷軸使用", 0) == 0 && (content.size() == 13 || content[13] == ' ')) {
            std::string rest = (content.size() > 14) ? content.substr(14) : "";
            // parse optional: pct count
            int pct = 0, cnt = 1;
            if (!rest.empty()) {
                std::istringstream iss(rest);
                iss >> pct >> cnt;
                if (pct != 10 && pct != 30 && pct != 60 && pct != 70) pct = 0;
                if (cnt <= 0) cnt = 1;
                if (cnt > 100) cnt = 100;
            }
            dpp::message m;
            if (pct == 0) {
                m = make_scroll_sel_msg(uid);
            } else {
                m = make_scroll_result_msg(uid, pct, cnt);
            }
            m.set_reference(ev.msg.id); m.channel_id = ch;
            bot.message_create(m);
        }
        // !刮 <碼|ALL>  ("!刮" = 4 bytes)
        else if (content.rfind("!刮", 0) == 0 && (content.size() == 4 || content[4] == ' ')) {
            size_t sp = content.find(' ');
            std::string rest = (sp != std::string::npos) ? content.substr(sp + 1) : "";
            std::string rest_lo = rest;
            for (auto& c2 : rest_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (rest_lo == "all");
            int64_t bet = is_all ? get_chips(uid) : (rest.empty() ? 0 : std::atoll(rest.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) {
                    dpp::message m; m.set_content("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
            } else {
                if (bet <= 0) {
                    dpp::message m; m.set_content("用法：`!刮 <籌碼量>`  例：`!刮 100` 或 `!刮 ALL`");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    dpp::message m; m.set_content("❌ 此討論串最低下注為 **1,000** 碼！");
                    m.set_reference(ev.msg.id); m.channel_id = ch; bot.message_create(m); return;
                }
            }
            std::string av = ev.msg.author.get_avatar_url();
            std::string dn = ev.msg.member.get_nickname();
            start_cmd(bot, uid, ch, handle_scratch_start(uid, ch, bet, av, dn), ev.msg.id);
        }
        // !狼人殺
        else if (content == "!狼人殺") {
            bool already = false;
            uint64_t gid = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (channel_wolf_game.count(ch)) { already = true; }
                else {
                    gid = wolf_counter++;
                    WolfGame g;
                    g.id = gid; g.channel_id = ch;
                    g.guild_id = ev.msg.guild_id; g.host_id = uid;
                    wolf_games[gid] = g;
                    channel_wolf_game[ch] = gid;
                }
            }
            if (already) {
                dpp::message m; m.set_content("❌ 此頻道已有進行中的狼人殺遊戲！");
                m.channel_id = ch; bot.message_create(m); return;
            }
            dpp::message m;
            { std::lock_guard<std::mutex> lk(data_mutex); m = make_wolf_lobby_msg(wolf_games[gid]); }
            m.channel_id = ch; bot.message_create(m);
        }
        // !交易 @對象 我的道具ID 我的籌碼 對方道具ID 對方籌碼  (ID=0 表示不出)
        else if (content.rfind("!交易", 0) == 0) {
            auto trade_usage = [&]() {
                dpp::message m; m.channel_id = ch;
                m.set_content("用法：`!交易 @對象 我的道具ID 我出的籌碼 對方道具ID 對方出的籌碼`（不出填 0）\n例：`!交易 @小明 50001 0 50002 500`");
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
            int64_t from_chips = 0, to_chips = 0;
            iss >> from_item_id >> from_chips >> to_item_id >> to_chips;

            // Validate sender's side
            const VirtualShopItem* from_vi = find_virtual_item_by_id(from_item_id);
            if (from_item_id && !from_vi) {
                dpp::message m; m.channel_id = ch;
                m.set_content("❌ 道具 ID `" + std::to_string(from_item_id) + "` 不存在！");
                bot.message_create(m); return;
            }
            if (from_vi) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& inv = inventory_data[uid];
                auto it = inv.find(from_vi->key);
                if (it == inv.end() || it->second <= 0) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ 你沒有道具 **" + from_vi->name + "**！");
                    bot.message_create(m); return;
                }
            }
            if (from_chips < 0) from_chips = 0;
            if (from_chips > 0 && get_chips(uid) < from_chips) {
                dpp::message m; m.channel_id = ch;
                m.set_content("❌ 你的籌碼不足！");
                bot.message_create(m); return;
            }
            if (to_chips < 0) to_chips = 0;
            const VirtualShopItem* to_vi = find_virtual_item_by_id(to_item_id);
            if (to_item_id && !to_vi) {
                dpp::message m; m.channel_id = ch;
                m.set_content("❌ 對方道具 ID `" + std::to_string(to_item_id) + "` 不存在！");
                bot.message_create(m); return;
            }

            TradeOffer t;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                t.id = trade_counter++;
                t.from_uid = uid; t.to_uid = target; t.channel_id = ch;
                t.from_item_id = from_item_id; t.from_chips = from_chips;
                t.to_item_id   = to_item_id;   t.to_chips   = to_chips;
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
        // !偷看 (owner only, secret — not in help, not a slash command)
        else if (content == "!偷看") {
            if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) return;
            bot.message_delete(ev.msg.id, ch); // silently erase owner's message
            auto phase_str = [](WolfPhase p) -> std::string {
                switch (p) {
                    case WolfPhase::WAITING:           return "等待玩家";
                    case WolfPhase::SHERIFF_NOMINATE:  return "競選警長";
                    case WolfPhase::SHERIFF_SPEECH:    return "警長發言";
                    case WolfPhase::SHERIFF_VOTE:      return "警長投票";
                    case WolfPhase::NIGHT_WOLVES:      return "夜晚－狼殺";
                    case WolfPhase::NIGHT_SEER:        return "夜晚－預言家";
                    case WolfPhase::NIGHT_WITCH:       return "夜晚－女巫";
                    case WolfPhase::DAY_ANNOUNCE:      return "白天公告";
                    case WolfPhase::LAST_WORDS:        return "遺言";
                    case WolfPhase::SHERIFF_SPEAK_DIR: return "警長選方向";
                    case WolfPhase::DAY_SPEAK:         return "白天發言";
                    case WolfPhase::BADGE_TRANSFER:    return "傳遞警徽";
                    case WolfPhase::DAY_VOTE:          return "白天投票";
                    case WolfPhase::DAY_VOTE_PK:       return "PK投票";
                    case WolfPhase::HUNTER_SHOOT:      return "獵人開槍";
                    case WolfPhase::GAME_OVER:         return "遊戲結束";
                    default:                           return "未知";
                }
            };
            std::string dm;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (wolf_games.empty()) {
                    dm = "🔍 目前沒有進行中的狼人殺對局。";
                } else {
                    for (auto& [gid, g] : wolf_games) {
                        dm += "═══════════════\n";
                        dm += "📍 頻道：<#" + std::to_string((uint64_t)g.channel_id) + ">\n";
                        dm += "📅 第 " + std::to_string(g.day) + " 天　階段：**" + phase_str(g.phase) + "**\n";
                        if (g.wolf_victim)
                            dm += "🐺 狼選目標：<@" + std::to_string((uint64_t)g.wolf_victim) + ">\n";
                        if (!g.wolf_vote_map.empty()) {
                            dm += "🐺 狼票：";
                            for (auto& [wolf, tgt] : g.wolf_vote_map)
                                dm += "<@" + std::to_string((uint64_t)wolf) + ">→<@" + std::to_string((uint64_t)tgt) + "> ";
                            dm += "\n";
                        }
                        dm += "🧙 女巫：解藥" + std::string(g.witch_has_antidote ? "✅" : "❌")
                            + " 毒藥" + std::string(g.witch_has_poison ? "✅" : "❌") + "\n";
                        if (g.witch_save_target)
                            dm += "　└ 今晚救：<@" + std::to_string((uint64_t)g.witch_save_target) + ">\n";
                        if (g.witch_poison_target)
                            dm += "　└ 今晚毒：<@" + std::to_string((uint64_t)g.witch_poison_target) + ">\n";
                        dm += "\n**玩家身份：**\n";
                        for (auto& p : g.players) {
                            if (p.seat == 0) continue;
                            dm += std::to_string(p.seat) + ". ";
                            dm += p.alive ? "🟢 " : "💀 ";
                            dm += p.display_name + " ── **" + p.role + "**";
                            if (p.is_sheriff) dm += " 🎖️";
                            dm += "\n";
                        }
                    }
                }
            }
            bot.direct_message_create(uid, dpp::message(dm));
        }
        // !狼人殺榜單
        else if (content == "!狼人殺榜單") {
            ev.reply(make_wolf_leaderboard_msg());
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
            ev.reply(dpp::ir_update_message, handle_transfer_confirm(tid, uid));
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

            // Accept: validate + execute under lock
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto from_vi = find_virtual_item_by_id(t.from_item_id);
                auto to_vi   = find_virtual_item_by_id(t.to_item_id);
                if (from_vi) {
                    auto it2 = inventory_data[t.from_uid].find(from_vi->key);
                    if (it2 == inventory_data[t.from_uid].end() || it2->second <= 0) {
                        trade_offers.erase(tid);
                        ev.reply(dpp::ir_update_message,
                            make_trade_msg(t, from_name, to_name, "提案方已沒有該道具！交易取消。")); return;
                    }
                }
                if (t.from_chips > 0 && chip_data[t.from_uid].chips < t.from_chips) {
                    trade_offers.erase(tid);
                    ev.reply(dpp::ir_update_message,
                        make_trade_msg(t, from_name, to_name, "提案方籌碼不足！交易取消。")); return;
                }
                if (to_vi) {
                    auto it2 = inventory_data[t.to_uid].find(to_vi->key);
                    if (it2 == inventory_data[t.to_uid].end() || it2->second <= 0) {
                        trade_offers.erase(tid);
                        ev.reply(dpp::ir_update_message,
                            make_trade_msg(t, from_name, to_name, "你沒有對方要求的道具！交易取消。")); return;
                    }
                }
                if (t.to_chips > 0 && chip_data[t.to_uid].chips < t.to_chips) {
                    trade_offers.erase(tid);
                    ev.reply(dpp::ir_update_message,
                        make_trade_msg(t, from_name, to_name, "你的籌碼不足！交易取消。")); return;
                }
                // Execute
                if (from_vi) {
                    inventory_data[t.from_uid][from_vi->key]--;
                    inventory_data[t.to_uid][from_vi->key]++;
                }
                if (to_vi) {
                    inventory_data[t.to_uid][to_vi->key]--;
                    inventory_data[t.from_uid][to_vi->key]++;
                }
                if (t.from_chips > 0) {
                    chip_data[t.from_uid].chips -= t.from_chips;
                    chip_data[t.to_uid].chips   += t.from_chips;
                }
                if (t.to_chips > 0) {
                    chip_data[t.to_uid].chips   -= t.to_chips;
                    chip_data[t.from_uid].chips += t.to_chips;
                }
                trade_offers.erase(tid);
            }
            save_chips();
            save_inventory();
            ev.reply(dpp::ir_update_message, make_trade_msg(t, from_name, to_name, "ok"));
        }
        // ── 商店按鈕（楓之谷 + 虛擬商店）────────────────────────────────────
        else if (cid.rfind("shop_", 0) == 0) {
            if (!page_is_mine(ev.command.message_id, uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的頁面！").set_flags(dpp::m_ephemeral)); return;
            }
            if (cid == "shop_main") {
                ev.reply(dpp::ir_update_message, make_shop_main_msg());
            } else if (cid == "shop_virtual" || cid == "shop_vback") {
                ev.reply(dpp::ir_update_message, make_virtual_shop_msg());
            } else if (cid.rfind("shop_vcat_", 0) == 0) {
                ev.reply(dpp::ir_update_message, make_vcat_shop_msg(uid, cid.substr(10)));
            } else if (cid.rfind("shop_vbuy_", 0) == 0) {
                ev.reply(dpp::ir_update_message, make_vbuy_confirm_msg(uid, cid.substr(10)));
            } else if (cid.rfind("shop_vconfirm_", 0) == 0) {
                ev.reply(dpp::ir_update_message, handle_vbuy(uid, user.username, cid.substr(14)));
            } else if (cid.rfind("shop_maple_", 0) == 0) {
                int page = std::stoi(cid.substr(11));
                ev.reply(dpp::ir_update_message, make_maple_shop_msg(page));
            } else if (cid.rfind("shop_buy_", 0) == 0) {
                int idx = std::stoi(cid.substr(9));
                ev.reply(dpp::ir_update_message, make_buy_confirm_msg(uid, idx));
            } else if (cid.rfind("shop_confirm_", 0) == 0) {
                int idx = std::stoi(cid.substr(13));
                ev.reply(dpp::ir_update_message, handle_buy(uid, user.username, idx));
            }
        }
        // ── 寵物按鈕 ──────────────────────────────────────────────────────────
        else if (cid.rfind("pet_", 0) == 0) {
            // All pet buttons embed the owner uid in the button ID
            if (cid.rfind("pet_work_", 0) == 0) {
                std::string rest = cid.substr(9);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
                int task = std::stoi(rest.substr(sep + 1));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_work_start(uid, task));
            } else if (cid.rfind("pet_claim_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(10)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_work_claim(uid));
            } else if (cid.rfind("pet_refresh_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(12)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_pet_view_msg(uid,
                    user.get_avatar_url(),
                    ev.command.member.get_nickname()));
            } else if (cid.rfind("pet_open_use_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(13)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_pet_use_msg(uid));
            } else if (cid.rfind("talent_pick_", 0) == 0) {
                // talent_pick_{talent}_{uid}
                std::string rest = cid.substr(12);
                size_t last = rest.rfind('_');
                if (last == std::string::npos) return;
                std::string talent = rest.substr(0, last);
                dpp::snowflake btn_uid(std::stoull(rest.substr(last + 1)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的背包！").set_flags(dpp::m_ephemeral)); return;
                }
                // Validate still has scroll and no talent
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto& inv = inventory_data[uid];
                    auto it = inv.find("talent_scroll");
                    if (it == inv.end() || it->second <= 0) {
                        ev.reply(dpp::ir_update_message,
                            dpp::message().add_embed(dpp::embed().set_title("❌  失敗").set_color(0xE74C3C)
                                .set_description("你已沒有天賦賦予卷軸！"))); return;
                    }
                    auto& pet = pet_data[uid];
                    if (!pet.talent.empty()) {
                        ev.reply(dpp::ir_update_message,
                            dpp::message().add_embed(dpp::embed().set_title("❌  失敗").set_color(0xE74C3C)
                                .set_description("寵物已擁有天賦：**" + pet.talent + "**"))); return;
                    }
                    inv["talent_scroll"]--;
                    pet.talent = talent;
                }
                save_inventory();
                save_pet_data();
                auto talent_desc = [](const std::string& t) -> std::string {
                    if (t == "迅捷")      return "打工時間縮短 10%！";
                    if (t == "招人喜歡")  return "打工報酬提升 10%！";
                    if (t == "幸運")      return "打工有 5% 機率獲得雙倍報酬！";
                    if (t == "天然呆")    return "使用道具時有 5% 機率不消耗道具！";
                    if (t == "喜歡作夢")  return "每次打工完有 0.1% 機率將現有籌碼翻倍！";
                    return "";
                };
                dpp::embed re;
                re.set_title("🌟  天賦覺醒！").set_color(0xF39C12)
                  .set_description("✨ 天賦賦予成功！\n**" + talent + "** — " + talent_desc(talent));
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(re));
            } else if (cid.rfind("pet_use_", 0) == 0) {
                // pet_use_{uid}_{item_key}  — uid has no underscores
                std::string rest = cid.substr(8);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
                std::string item_key = rest.substr(sep + 1);
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_use_item(uid, item_key));
            } else if (cid.rfind("pet_discard_mode_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(17)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_pet_discard_mode_msg(uid));
            } else if (cid.rfind("pet_discard_confirm_", 0) == 0) {
                // pet_discard_confirm_{uid}_{item_key}
                std::string rest = cid.substr(20);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
                std::string item_key = rest.substr(sep + 1);
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_pet_discard_confirm_msg(uid, item_key));
            } else if (cid.rfind("pet_discard_do_", 0) == 0) {
                // pet_discard_do_{uid}_{item_key}
                std::string rest = cid.substr(15);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
                std::string item_key = rest.substr(sep + 1);
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_discard_item(uid, item_key));
            } else if (cid.rfind("pet_rename_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(11)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                dpp::interaction_modal_response modal(
                    "pet_rename_modal_" + std::to_string((uint64_t)uid), "為寵物改名");
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("寵物暱稱（留空則清除暱稱）").set_id("new_name")
                    .set_text_style(dpp::text_short).set_min_length(0).set_max_length(20)
                    .set_placeholder("輸入新名字，或留空清除"));
                ev.dialog(modal);
            } else if (cid.rfind("pet_refine_star_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(16)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                {
                    std::string sid = std::to_string((uint64_t)uid);
                    dpp::embed ce; ce.set_title("✨  提煉星星").set_color(0xF1C40F);
                    ce.set_description("確定要消耗 **50 exp** 提煉星星嗎？\n成功率：**90%**");
                    dpp::component row; row.set_type(dpp::cot_action_row);
                    row.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label("✅ 確認提煉").set_id("pet_refine_confirm_" + sid).set_style(dpp::cos_success));
                    row.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label("❌ 取消").set_id("pet_refine_cancel_" + sid).set_style(dpp::cos_secondary));
                    dpp::message cm; cm.add_embed(ce); cm.add_component(row);
                    cm.set_flags(dpp::m_ephemeral);
                    ev.reply(dpp::ir_channel_message_with_source, cm);
                }
            } else if (cid.rfind("pet_refine_confirm_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(19)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_refine_star(uid));
            } else if (cid.rfind("pet_refine_cancel_", 0) == 0) {
                ev.reply(dpp::ir_update_message,
                    dpp::message("❌ 已取消提煉。").set_flags(dpp::m_ephemeral));
            } else if (cid.rfind("pet_release_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(12)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                dpp::interaction_modal_response modal(
                    "pet_release_modal_" + std::to_string((uint64_t)uid), "🕊️ 放生確認");
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("請輸入「放生」以確認").set_id("confirm_text")
                    .set_text_style(dpp::text_short).set_min_length(2).set_max_length(4)
                    .set_placeholder("放生"));
                ev.dialog(modal);
            }
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
        else if (cid == "admin_chip_modal_btn" || cid == "admin_egg_btn" || cid == "admin_item_btn") {
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
            } else if (cid == "admin_egg_btn") {
                dpp::interaction_modal_response modal("admin_egg_modal", "管理員給蛋");
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("目標 User ID").set_id("target_uid")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(20)
                    .set_placeholder("例：457478323665240065"));
                modal.add_component(dpp::component().set_type(dpp::cot_text)
                    .set_label("蛋的種類").set_id("egg_chain")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(10)
                    .set_placeholder("嫩寶 / 菇菇仔 / 肥肥"));
                ev.dialog(modal);
            } else {
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
                    .set_label("數量").set_id("item_qty")
                    .set_text_style(dpp::text_short).set_min_length(1).set_max_length(5)
                    .set_placeholder("1"));
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
        // ── 骰子押注選擇 ──────────────────────────────────────────────────────
        else if (cid.rfind("dc_", 0) == 0 && cid.rfind("dc_again_", 0) != 0) {
            // format: dc_{gid}_{choice}
            std::string rest = cid.substr(3);
            size_t sep = rest.rfind('_');
            if (sep == std::string::npos) return;
            uint64_t gid = std::stoull(rest.substr(0, sep));
            int choice   = std::stoi(rest.substr(sep + 1));
            dpp::message res = handle_dice_pick(gid, choice, uid);
            if (res.embeds.empty()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, res);
        }
        // ── 骰子再來一局 ──────────────────────────────────────────────────────
        else if (cid.rfind("dc_again_", 0) == 0) {
            std::string rest = cid.substr(9);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake orig_uid(std::stoull(rest.substr(0, sep)));
            int64_t bet = std::stoll(rest.substr(sep + 1));
            if (uid != orig_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ev.command.channel_id) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
            } else if (get_chips(uid) < bet) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 籌碼不足 " + std::to_string(bet) + " 碼！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_channel_message_with_source, start_dice(uid, ev.command.channel_id, bet,
                user.get_avatar_url(), user.username));
        }
        // ── 21點再來一局 ──────────────────────────────────────────────────────
        else if (cid.rfind("bj_again_", 0) == 0) {
            std::string rest = cid.substr(9);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            dpp::snowflake orig_uid(std::stoull(rest.substr(0, sep)));
            int64_t bet = std::stoll(rest.substr(sep + 1));
            if (uid != orig_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ev.command.channel_id) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
            } else if (get_chips(uid) < bet) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 籌碼不足 " + std::to_string(bet) + " 碼！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = user_bj.find(uid);
                if (it != user_bj.end()) { bj_games.erase(it->second); user_bj.erase(it); }
            }
            const dpp::user& u = ev.command.get_issuing_user();
            add_chips(uid, -bet);
            BJGame g = start_bj(uid, ev.command.channel_id, bet, u.get_avatar_url(), u.username);
            std::string status;
            if (is_blackjack(g.main_hand.cards)) {
                bool dbj = is_blackjack(g.dealer_cards);
                g.game_over = true;
                if (dbj) { add_chips(uid, bet); status = "雙 BJ — 平局！"; }
                else {
                    int64_t win = (int64_t)(bet * 1.5);
                    add_chips(uid, bet + win); status = "🌟 Blackjack！贏得 **" + std::to_string(win) + "** 碼！";
                }
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                uint64_t gid = g.id;
                bj_games[gid] = g; user_bj[uid] = gid;
            }
            save_bj_games();
            ev.reply(dpp::ir_channel_message_with_source, make_bj_msg(g, status));
            ev.get_original_response([uid, gid = g.id](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    auto& m = std::get<dpp::message>(cb.value);
                    { std::lock_guard<std::mutex> lk(data_mutex);
                      auto it = bj_games.find(gid);
                      if (it != bj_games.end()) it->second.msg_id = m.id;
                      msg_owner[m.id] = uid; }
                    save_bj_games();
                }
            });
        }
        // ── 21點按鈕 ──────────────────────────────────────────────────────────
        else if (cid.rfind("bj_", 0) == 0) {
            // cid format: bj_{action}_{game_id}
            size_t p1 = cid.find('_', 3);
            if (p1 == std::string::npos) return;
            std::string action = cid.substr(3, p1 - 3);
            uint64_t gid = std::stoull(cid.substr(p1 + 1));
            // Ownership check
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = bj_games.find(gid);
                if (it == bj_games.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 遊戲已結束。").set_flags(dpp::m_ephemeral));
                    return;
                }
                if (it->second.user_id != uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的遊戲！").set_flags(dpp::m_ephemeral));
                    return;
                }
            }
            dpp::message updated = handle_bj_button(action, gid, uid);
            if (updated.embeds.empty()) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("⚠️ 遊戲不存在。").set_flags(dpp::m_ephemeral));
                return;
            }
            // Clean up finished game
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = bj_games.find(gid);
                if (it != bj_games.end() && it->second.game_over) {
                    user_bj.erase(it->second.user_id);
                    bj_games.erase(it);
                }
            }
            ev.reply(dpp::ir_update_message, updated);
        }
        // ── 選王 ──────────────────────────────────────────────────────────────
        else if (cid.rfind("boss_", 0) == 0) {
            std::string boss = cid.substr(5);
            { std::lock_guard<std::mutex> lk(data_mutex); user_states[uid] = RegState{boss, 0, {}}; }
            ev.reply(dpp::ir_update_message, make_time_msg(boss, user, 0, {}));
        }
        // ── 時段切換 ──────────────────────────────────────────────────────────
        else if (cid.rfind("slot_", 0) == 0) {
            std::string tval = cid.substr(5);
            std::string boss; int view_day;
            std::set<std::pair<std::string,std::string>> slots;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = user_states.find(uid);
                if (it == user_states.end()) return;
                auto& state = it->second;
                auto week = get_game_week();
                std::string cur_label = week[state.view_day].second;
                auto key = std::make_pair(cur_label, tval);
                if (state.slots.count(key)) state.slots.erase(key);
                else state.slots.insert(key);
                boss = state.boss; view_day = state.view_day; slots = state.slots;
            }
            ev.reply(dpp::ir_update_message, make_time_msg(boss, user, view_day, slots));
        }
        // ── 返回：時間 → 選王 ──────────────────────────────────────────────────
        else if (cid == "back_to_boss") {
            { std::lock_guard<std::mutex> lk(data_mutex); user_states.erase(uid); }
            ev.reply(dpp::ir_update_message, make_boss_msg(user));
        }
        // ── 返回：位置 → 時間 ──────────────────────────────────────────────────
        else if (cid == "back_to_time") {
            std::string boss; int view_day;
            std::set<std::pair<std::string,std::string>> slots;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = user_states.find(uid);
                if (it == user_states.end()) return;
                boss = it->second.boss; view_day = it->second.view_day; slots = it->second.slots;
            }
            ev.reply(dpp::ir_update_message, make_time_msg(boss, user, view_day, slots));
        }
        // ── 確定時段 ──────────────────────────────────────────────────────────
        else if (cid == "confirm_time") {
            std::string boss;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = user_states.find(uid);
                if (it == user_states.end() || it->second.slots.empty()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 請先選擇至少一個時段！").set_flags(dpp::m_ephemeral));
                    return;
                }
                boss = it->second.boss;
            }
            ev.reply(dpp::ir_update_message, make_position_msg(boss, user));
        }
        // ── 選職業 → 報名完成 ──────────────────────────────────────────────────
        else if (cid.rfind("pos_", 0) == 0) {
            std::string pos = cid.substr(4);
            Registration reg;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = user_states.find(uid);
                if (it == user_states.end()) return;
                reg.id         = reg_counter++;
                reg.user_id    = uid;
                reg.channel_id = ev.command.channel_id;
                reg.username   = user.username;
                reg.boss       = it->second.boss;
                reg.slots      = std::vector<std::pair<std::string,std::string>>(
                                     it->second.slots.begin(), it->second.slots.end());
                reg.position   = pos;
                registrations.push_back(reg);
                user_states.erase(it);
                user_active_msg.erase(uid);
                msg_owner.erase(ev.command.message_id);
            }
            save_registrations();
            ev.reply(dpp::ir_update_message, make_success_msg(reg));
            check_team_formation(bot, reg.boss, reg.channel_id);
            save_proposed_teams();
        }
        // ── 紀錄：刪除 ────────────────────────────────────────────────────────
        else if (cid.rfind("del_", 0) == 0) {
            if (!adm && !check_owner(ev, uid)) return;
            uint64_t rid = std::stoull(cid.substr(4));
            bool ok = false; std::string cur_filter;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = std::find_if(registrations.begin(), registrations.end(),
                    [rid](const Registration& r){ return r.id == rid; });
                if (it != registrations.end() && (it->user_id == uid || adm)) {
                    ok = true; registrations.erase(it);
                }
                auto vf = view_filters.find(uid);
                cur_filter = (vf != view_filters.end()) ? vf->second : "mine";
            }
            if (!ok) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你只能刪除自己的報名！").set_flags(dpp::m_ephemeral));
                return;
            }
            save_registrations();
            ev.reply(dpp::ir_update_message, make_records_view_msg(cur_filter, uid, adm));
        }
        // ── 紀錄：返回 ────────────────────────────────────────────────────────
        else if (cid == "records_back") {
            { std::lock_guard<std::mutex> lk(data_mutex); view_filters.erase(uid); }
            ev.reply(dpp::ir_update_message, make_records_select_msg(user));
        }
        // ── 組隊確認 ──────────────────────────────────────────────────────────
        else if (cid.rfind("team_confirm_", 0) == 0 || cid.rfind("team_cancel_", 0) == 0) {
            bool is_confirm = cid.rfind("team_confirm_", 0) == 0;
            uint64_t tid = std::stoull(cid.substr(is_confirm ? 13 : 12));
            bool authorized = adm ||
                (!cfg.notify_user_id.empty() && std::to_string(uid) == cfg.notify_user_id);
            if (!authorized) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 只有管理員可以操作！").set_flags(dpp::m_ephemeral));
                return;
            }
            ProposedTeam pt; bool found = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = proposed_teams.find(tid);
                if (it != proposed_teams.end()) {
                    found = true; pt = it->second;
                    proposed_teams.erase(it);
                    proposed_slots.erase({pt.boss, pt.day, pt.time_slot});
                }
            }
            if (!found) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("⚠️ 此組隊通知已失效。").set_flags(dpp::m_ephemeral));
                return;
            }
            if (is_confirm) {
                // #2: Remove ALL registrations of confirmed members
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    std::set<dpp::snowflake> member_ids;
                    for (auto& m : pt.members) member_ids.insert(m.user_id);
                    registrations.erase(
                        std::remove_if(registrations.begin(), registrations.end(),
                            [&](const Registration& r){ return member_ids.count(r.user_id); }),
                        registrations.end());
                }

                dpp::embed done_e;
                done_e.set_title("✅  組隊已確認！").set_color(0x2ECC71);
                done_e.add_field("⚔️  王",   pt.boss,                       true);
                done_e.add_field("🕐  時間", pt.day + "  " + pt.time_slot,  true);
                done_e.set_footer(dpp::embed_footer().set_text("王團報名系統"));
                dpp::message done_msg; done_msg.add_embed(done_e);
                ev.reply(dpp::ir_update_message, done_msg);

                std::ostringstream ann;
                for (auto& m : pt.members) ann << "<@" << m.user_id << "> ";
                dpp::embed ann_e;
                ann_e.set_title("🎉  組隊成功！").set_color(0x2ECC71);
                if (!get_boss_img(pt.boss).empty()) ann_e.set_thumbnail(get_boss_img(pt.boss));
                ann_e.add_field("⚔️  王",   pt.boss,                       true);
                ann_e.add_field("🕐  時間", pt.day + "  " + pt.time_slot,  true);
                std::ostringstream mem_oss;
                for (size_t i=0; i<pt.members.size(); i++)
                    mem_oss << std::to_string(i+1) << ". **" << pt.members[i].username
                            << "** · " << pt.members[i].position << "\n";
                ann_e.add_field("👥  成員", mem_oss.str(), false);
                ann_e.set_footer(dpp::embed_footer().set_text("王團報名系統"));
                dpp::message ann_msg(ev.command.channel_id, ann.str());
                ann_msg.add_embed(ann_e);
                save_registrations(); save_proposed_teams();
                bot.message_create(ann_msg);
            } else {
                save_proposed_teams();
                dpp::embed cancel_e;
                cancel_e.set_title("❌  組隊已撤銷").set_color(0x808080);
                cancel_e.add_field("⚔️  王",   pt.boss,                       true);
                cancel_e.add_field("🕐  時間", pt.day + "  " + pt.time_slot,  true);
                dpp::message cancel_msg; cancel_msg.add_embed(cancel_e);
                ev.reply(dpp::ir_update_message, cancel_msg);
            }
        }
        // ── 狼人殺按鈕 ───────────────────────────────────────────────────────────
        else if (cid.rfind("wolf_", 0) == 0) {
            // ── wolf_join / wolf_leave / wolf_start (大廳) ─────────────────────
            if (cid.rfind("wolf_join_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(10));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 遊戲不存在！").set_flags(dpp::m_ephemeral)); return; }
                auto& g = it->second;
                if (g.phase != WolfPhase::WAITING) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 遊戲已開始！").set_flags(dpp::m_ephemeral)); return; }
                if (wfind(g, uid)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 你已加入！").set_flags(dpp::m_ephemeral)); return; }
                if ((int)g.players.size() >= 9) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 已滿 9 人！").set_flags(dpp::m_ephemeral)); return; }
                WolfPlayer p; p.uid = uid;
                {
                    std::string dn = ev.command.member.get_nickname();
                    p.display_name = dn;
                }
                g.players.push_back(p);
                ev.reply(dpp::ir_update_message, make_wolf_lobby_msg(g));
            }
            else if (cid.rfind("wolf_leave_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(11));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                auto& g = it->second;
                if (g.phase != WolfPhase::WAITING) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 遊戲已開始！").set_flags(dpp::m_ephemeral)); return; }
                auto& pl = g.players;
                pl.erase(std::remove_if(pl.begin(), pl.end(), [uid](auto& p){ return p.uid == uid; }), pl.end());
                ev.reply(dpp::ir_update_message, make_wolf_lobby_msg(g));
            }
            else if (cid.rfind("wolf_start_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(11));
                bool ok = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (uid != g.host_id) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以開始！").set_flags(dpp::m_ephemeral)); return; }
                    if ((int)g.players.size() != 9) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 需要 9 名玩家！").set_flags(dpp::m_ephemeral)); return; }
                    ok = true;
                }
                if (ok) {
                    dpp::embed e; e.set_title("🐺  遊戲即將開始！").set_color(0x8B0000).set_description("角色分配中，請查收私訊...");
                    ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                    begin_wolf_game(bot, gid);
                }
            }
            // ── 警長競選 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_nominate_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(14));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                auto& g = it->second;
                if (g.phase != WolfPhase::SHERIFF_NOMINATE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 報名已結束！").set_flags(dpp::m_ephemeral)); return; }
                auto* p = wfind(g, uid);
                if (!p || !p->alive) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中！").set_flags(dpp::m_ephemeral)); return; }
                auto cit = std::find(g.candidates.begin(), g.candidates.end(), uid);
                if (cit != g.candidates.end()) {
                    // Already a candidate → withdraw
                    g.candidates.erase(cit);
                    ev.reply(dpp::ir_update_message, make_sheriff_nominate_msg(g)); return;
                }
                g.candidates.push_back(uid);
                // If all alive players nominated → auto tear badge
                int alive_cnt = 0;
                for (auto& pl : g.players) if (pl.alive) alive_cnt++;
                bool all_nominated = ((int)g.candidates.size() == alive_cnt);
                if (all_nominated) {
                    g.phase = WolfPhase::DAY_ANNOUNCE;
                    dpp::embed te;
                    te.set_title("🗑️  全員參選！撕毀警徽").set_color(0x808080);
                    te.set_description("所有玩家均參選警長，依規則直接撕毀警徽，本局無警長。");
                    ev.reply(dpp::ir_update_message, dpp::message().add_embed(te));
                    announce_night_and_start_day(bot, gid);
                } else {
                    ev.reply(dpp::ir_update_message, make_sheriff_nominate_msg(g));
                }
            }
            else if (cid.rfind("wolf_sheriff_vote_start_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(24));
                bool ok = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (uid != g.host_id) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以操作！").set_flags(dpp::m_ephemeral)); return; }
                    if (g.candidates.empty()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有候選人！").set_flags(dpp::m_ephemeral)); return; }
                    // Build speak_seats from candidates in their seat order
                    g.speak_seats.clear();
                    g.speak_idx = 0;
                    for (auto cuid : g.candidates) {
                        auto* cp = wfind(g, cuid);
                        if (cp) g.speak_seats.push_back(cp->seat);
                    }
                    ok = true;
                }
                if (ok) {
                    dpp::embed e; e.set_title("🎤  候選人開始競選發言").set_color(0xF39C12)
                        .set_description("候選人依序發言，發言完畢按「結束發言」；也可按「不競選」退出。");
                    ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                    start_sheriff_speech(bot, gid);
                }
            }
            else if (cid.rfind("wolf_skip_sheriff_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(18));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    if (uid != it->second.host_id) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以操作！").set_flags(dpp::m_ephemeral)); return; }
                }
                dpp::embed e; e.set_title("⏭  本局無警長").set_color(0x808080).set_description("跳過警長競選，遊戲開始！");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                announce_night_and_start_day(bot, gid);
            }
            // ── 警長投票 棄票 ──────────────────────────────────────────────────
            else if (cid.rfind("wolf_svote_abstain_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(19));
                bool auto_resolve = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::SHERIFF_VOTE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 投票已結束！").set_flags(dpp::m_ephemeral)); return; }
                    bool is_cand = std::find(g.candidates.begin(), g.candidates.end(), uid) != g.candidates.end();
                    bool withdrew = std::find(g.withdrawn_candidates.begin(), g.withdrawn_candidates.end(), uid) != g.withdrawn_candidates.end();
                    if (is_cand || withdrew) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 候選人不能投票！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid); if (!p || !p->alive) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中！").set_flags(dpp::m_ephemeral)); return; }
                    g.sheriff_votes[uid] = dpp::snowflake(0);
                    if (!g.sheriff_vote_msg_id) g.sheriff_vote_msg_id = ev.command.message_id;
                    int eligible = 0, voted_cnt = 0;
                    for (auto& p2 : g.players) {
                        if (!p2.alive) continue;
                        bool c = std::find(g.candidates.begin(), g.candidates.end(), p2.uid) != g.candidates.end();
                        bool w = std::find(g.withdrawn_candidates.begin(), g.withdrawn_candidates.end(), p2.uid) != g.withdrawn_candidates.end();
                        if (c || w) continue;
                        eligible++; if (g.sheriff_votes.count(p2.uid)) voted_cnt++;
                    }
                    auto_resolve = (eligible > 0 && voted_cnt == eligible);
                    if (!auto_resolve) ev.reply(dpp::ir_update_message, make_sheriff_vote_msg(g));
                }
                if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 所有人已投票，自動結算！").set_flags(dpp::m_ephemeral)); resolve_sheriff_vote(bot, gid); }
            }
            // ── 競選報名期間退出 / 不競選 ──────────────────────────────────────
            else if (cid.rfind("wolf_withdraw_nominate_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(23));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                auto& g = it->second;
                if (g.phase != WolfPhase::SHERIFF_NOMINATE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 報名階段已結束！").set_flags(dpp::m_ephemeral)); return; }
                auto& p = *wfind(g, uid); // may crash if not found
                auto* pfound = wfind(g, uid);
                if (!pfound) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中！").set_flags(dpp::m_ephemeral)); return; }
                bool is_cand = std::find(g.candidates.begin(), g.candidates.end(), uid) != g.candidates.end();
                bool not_run = std::find(g.not_running.begin(), g.not_running.end(), uid) != g.not_running.end();
                if (is_cand) {
                    // Candidate withdrawing
                    g.candidates.erase(std::remove(g.candidates.begin(), g.candidates.end(), uid), g.candidates.end());
                    g.withdrawn_candidates.push_back(uid);
                } else if (!not_run) {
                    // Non-candidate opting out
                    g.not_running.push_back(uid);
                }
                ev.reply(dpp::ir_update_message, make_sheriff_nominate_msg(g));
            }
            // ── 警長投票 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_svote_", 0) == 0) {
                std::string rest = cid.substr(11);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake target(std::stoull(rest.substr(sep+1)));
                bool auto_resolve = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::SHERIFF_VOTE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 投票已結束！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || !p->alive) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中！").set_flags(dpp::m_ephemeral)); return; }
                    bool is_cand = std::find(g.candidates.begin(), g.candidates.end(), uid) != g.candidates.end();
                    bool withdrew = std::find(g.withdrawn_candidates.begin(), g.withdrawn_candidates.end(), uid) != g.withdrawn_candidates.end();
                    if (is_cand || withdrew) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 候選人不能投票！").set_flags(dpp::m_ephemeral)); return; }
                    g.sheriff_votes[uid] = target;
                    if (!g.sheriff_vote_msg_id) g.sheriff_vote_msg_id = ev.command.message_id;
                    int eligible = 0, voted_cnt = 0;
                    for (auto& p2 : g.players) {
                        if (!p2.alive) continue;
                        bool c = std::find(g.candidates.begin(), g.candidates.end(), p2.uid) != g.candidates.end();
                        bool w = std::find(g.withdrawn_candidates.begin(), g.withdrawn_candidates.end(), p2.uid) != g.withdrawn_candidates.end();
                        if (c || w) continue;
                        eligible++; if (g.sheriff_votes.count(p2.uid)) voted_cnt++;
                    }
                    auto_resolve = (eligible > 0 && voted_cnt == eligible);
                    if (!auto_resolve) ev.reply(dpp::ir_update_message, make_sheriff_vote_msg(g));
                }
                if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 所有人已投票，自動結算！").set_flags(dpp::m_ephemeral)); resolve_sheriff_vote(bot, gid); }
            }
            else if (cid.rfind("wolf_sheriff_resolve_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(21));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    if (it->second.host_id != uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以結算！").set_flags(dpp::m_ephemeral)); return; }
                    if (it->second.phase != WolfPhase::SHERIFF_VOTE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是投票階段！").set_flags(dpp::m_ephemeral)); return; }
                }
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("⏳ 結算中...").set_flags(dpp::m_ephemeral));
                resolve_sheriff_vote(bot, gid);
            }
            // ── 狼人投票（在討論串）─────────────────────────────────────────────
            else if (cid.rfind("wolf_wvote_", 0) == 0) {
                std::string rest = cid.substr(11);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake target(std::stoull(rest.substr(sep+1)));
                bool all_voted = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::NIGHT_WOLVES) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是投票時間！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || p->role != "狼人" || !p->alive) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是狼人！").set_flags(dpp::m_ephemeral)); return; }
                    g.wolf_vote_map[uid] = target;
                    int alive_wolves = 0, voted = 0;
                    for (auto& wp : g.players) if (wp.role == "狼人" && wp.alive) { alive_wolves++; if (g.wolf_vote_map.count(wp.uid)) voted++; }
                    all_voted = (voted == alive_wolves);
                    if (g.wolf_vote_msg_id) {
                        dpp::message upd = make_wolf_vote_msg(g);
                        upd.id = g.wolf_vote_msg_id; upd.channel_id = g.wolf_thread_id;
                        bot.message_edit(upd);
                    }
                }
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票！").set_flags(dpp::m_ephemeral));
                if (all_voted) proceed_to_seer(bot, gid);
            }
            // ── 預言家查驗（私訊按鈕）──────────────────────────────────────────
            else if (cid.rfind("wolf_seer_", 0) == 0) {
                std::string rest = cid.substr(10);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake target(std::stoull(rest.substr(sep+1)));
                std::string result, target_name;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::NIGHT_SEER) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || p->role != "預言家") { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是預言家！").set_flags(dpp::m_ephemeral)); return; }
                    auto* t = wfind(g, target);
                    if (!t) return;
                    target_name = std::to_string(t->seat) + ". " + t->display_name;
                    result = (t->role == "狼人") ? "🐺 **狼人**！" : "✅ **好人**（非狼人）";
                }
                dpp::embed e; e.set_title("🔮  查驗結果").set_color(0x9B59B6);
                e.set_description("**" + target_name + "** 的身份是：" + result);
                ev.reply(dpp::ir_channel_message_with_source, dpp::message().add_embed(e));
                proceed_to_witch(bot, gid);
            }
            // ── 女巫藥水（私訊按鈕）────────────────────────────────────────────
            else if (cid.rfind("wolf_witch_save_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(16));
                bool ok = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::NIGHT_WITCH) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || p->role != "女巫") { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是女巫！").set_flags(dpp::m_ephemeral)); return; }
                    if (!g.witch_has_antidote || g.witch_used_tonight) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 解藥已用完或今晚已用過！").set_flags(dpp::m_ephemeral)); return; }
                    g.witch_save_target = g.wolf_victim;
                    g.witch_has_antidote = false; g.witch_used_tonight = true;
                    ok = true;
                }
                if (ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("💊 已使用解藥！").set_flags(dpp::m_ephemeral)); resolve_night(bot, gid); }
            }
            else if (cid.rfind("wolf_witch_skip_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(16));
                bool ok = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    if (it->second.phase != WolfPhase::NIGHT_WITCH) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(it->second, uid);
                    if (!p || p->role != "女巫") { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是女巫！").set_flags(dpp::m_ephemeral)); return; }
                    ok = true;
                }
                if (ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⏭ 跳過！").set_flags(dpp::m_ephemeral)); resolve_night(bot, gid); }
            }
            else if (cid.rfind("wolf_witch_poison_", 0) == 0) {
                std::string rest = cid.substr(18);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake target(std::stoull(rest.substr(sep+1)));
                bool ok = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::NIGHT_WITCH) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || p->role != "女巫") { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是女巫！").set_flags(dpp::m_ephemeral)); return; }
                    if (!g.witch_has_poison || g.witch_used_tonight) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 毒藥已用完或今晚已用過！").set_flags(dpp::m_ephemeral)); return; }
                    g.witch_poison_target = target; g.witch_has_poison = false; g.witch_used_tonight = true;
                    ok = true;
                }
                if (ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("☠️ 已使用毒藥！").set_flags(dpp::m_ephemeral)); resolve_night(bot, gid); }
            }
            // ── 警長選發言方向 ─────────────────────────────────────────────────
            else if (cid.rfind("wolf_dir_cw_", 0) == 0 || cid.rfind("wolf_dir_ccw_", 0) == 0) {
                bool is_cw = cid.rfind("wolf_dir_cw_", 0) == 0;
                uint64_t gid = std::stoull(cid.substr(is_cw ? 12 : 13));
                dpp::snowflake ch2;
                std::vector<int> speak;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (uid != g.sheriff_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有警長可以選擇！").set_flags(dpp::m_ephemeral)); return; }
                    if (g.phase != WolfPhase::SHERIFF_SPEAK_DIR) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    ch2 = g.channel_id;
                    compute_speak_order(g, g.night_deaths, true, is_cw);
                    speak = g.speak_seats;
                }
                std::string dir_label = is_cw ? "順時針 ▶" : "逆時針 ◀";
                dpp::embed e; e.set_title("🎤  今日發言順序（" + dir_label + "）").set_color(0x3498DB)
                    .set_description("開始依序發言...");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                start_day_speak(bot, gid);
            }
            // ── 白天投票 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_dvote_abstain_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(19));
                bool auto_resolve = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::DAY_VOTE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是投票時間！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || !p->alive) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中或已死亡！").set_flags(dpp::m_ephemeral)); return; }
                    g.day_votes[uid] = dpp::snowflake(0); // 棄票
                    int alive_cnt = 0, voted_cnt = 0;
                    for (auto& p2 : g.players) { if (p2.alive) { alive_cnt++; if (g.day_votes.count(p2.uid)) voted_cnt++; } }
                    auto_resolve = (alive_cnt > 0 && voted_cnt == alive_cnt);
                    if (!auto_resolve && g.day_vote_msg_id) {
                        dpp::message upd = make_day_vote_msg(g);
                        upd.id = g.day_vote_msg_id; upd.channel_id = g.channel_id;
                        bot.message_edit(upd);
                    }
                }
                if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("🚫 已棄票，所有人已投完，自動結算！").set_flags(dpp::m_ephemeral)); resolve_day_vote(bot, gid); }
                else ev.reply(dpp::ir_channel_message_with_source, dpp::message("🚫 已棄票！").set_flags(dpp::m_ephemeral));
            }
            else if (cid.rfind("wolf_dvote_resolve_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(19));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    if (uid != it->second.host_id) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以結算！").set_flags(dpp::m_ephemeral)); return; }
                    if (it->second.phase != WolfPhase::DAY_VOTE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是投票階段！").set_flags(dpp::m_ephemeral)); return; }
                }
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("⏳ 結算中...").set_flags(dpp::m_ephemeral));
                resolve_day_vote(bot, gid);
            }
            else if (cid.rfind("wolf_dvote_", 0) == 0) {
                std::string rest = cid.substr(11);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake target(std::stoull(rest.substr(sep+1)));
                std::string tname;
                bool auto_resolve = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::DAY_VOTE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是投票時間！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || !p->alive) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中或已死亡！").set_flags(dpp::m_ephemeral)); return; }
                    g.day_votes[uid] = target;
                    auto* t = wfind(g, target); if (t) tname = t->display_name;
                    int alive_cnt = 0, voted_cnt = 0;
                    for (auto& p2 : g.players) { if (p2.alive) { alive_cnt++; if (g.day_votes.count(p2.uid)) voted_cnt++; } }
                    auto_resolve = (alive_cnt > 0 && voted_cnt == alive_cnt);
                    if (!auto_resolve && g.day_vote_msg_id) {
                        dpp::message upd = make_day_vote_msg(g);
                        upd.id = g.day_vote_msg_id; upd.channel_id = g.channel_id;
                        bot.message_edit(upd);
                    }
                }
                if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票，所有人已投完，自動結算！").set_flags(dpp::m_ephemeral)); resolve_day_vote(bot, gid); }
                else ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票給 **" + tname + "**！").set_flags(dpp::m_ephemeral));
            }
            // ── 獵人不開槍 ─────────────────────────────────────────────────────
            else if (cid.rfind("wolf_hunter_skip_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(17));
                WolfPhase after;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::HUNTER_SHOOT) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    if (uid != g.hunter_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是獵人！").set_flags(dpp::m_ephemeral)); return; }
                    g.hunter_pending = false;
                    after = g.after_hunter;
                }
                dpp::embed e; e.set_title("🏹  獵人選擇不開槍").set_color(0x808080);
                e.set_description("獵人選擇不帶走任何人。");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                continue_after_hunter(bot, gid);
            }
            // ── 候選人退選（任何時候）──────────────────────────────────────────
            else if (cid.rfind("wolf_candidate_withdraw_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(24));
                std::string pname;
                bool ok = false;
                bool is_speech = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    bool in_speech = (g.phase == WolfPhase::SHERIFF_SPEECH);
                    bool in_vote   = (g.phase == WolfPhase::SHERIFF_VOTE);
                    if (!in_speech && !in_vote) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是競選階段！").set_flags(dpp::m_ephemeral)); return; }
                    bool is_cand = std::find(g.candidates.begin(), g.candidates.end(), uid) != g.candidates.end();
                    if (!is_cand) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是候選人！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid); if (p) pname = std::to_string(p->seat) + ". " + p->display_name;
                    g.candidates.erase(std::remove(g.candidates.begin(), g.candidates.end(), uid), g.candidates.end());
                    g.withdrawn_candidates.push_back(uid);
                    // Remove from speak order
                    g.speak_seats.erase(std::remove(g.speak_seats.begin(), g.speak_seats.end(), p ? p->seat : -1), g.speak_seats.end());
                    ok = true; is_speech = in_speech;
                }
                if (ok) {
                    bot.message_create(dpp::message(ev.command.channel_id, "📢 **" + pname + "** 退出了警長競選！"));
                    ev.reply(dpp::ir_channel_message_with_source, dpp::message("🚪 已退出候選！").set_flags(dpp::m_ephemeral));
                    if (is_speech) advance_speaker(bot, gid);
                }
            }
            // ── MVP 投票 ────────────────────────────────────────────────────────
            else if (cid.rfind("wolf_mvp_", 0) == 0) {
                std::string rest = cid.substr(9);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake target(std::stoull(rest.substr(sep+1)));
                bool auto_resolve = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::MVP_VOTE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是 MVP 投票階段！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在本場遊戲中！").set_flags(dpp::m_ephemeral)); return; }
                    if (target == uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 不能投自己！").set_flags(dpp::m_ephemeral)); return; }
                    g.mvp_votes[uid] = target;
                    int total = 0, voted = 0;
                    for (auto& p2 : g.players) { if (p2.seat > 0) { total++; if (g.mvp_votes.count(p2.uid)) voted++; } }
                    auto_resolve = (total > 0 && voted == total);
                    if (!auto_resolve && g.mvp_vote_msg_id) {
                        dpp::message upd = make_mvp_vote_msg(g);
                        upd.id = g.mvp_vote_msg_id; upd.channel_id = g.channel_id;
                        bot.message_edit(upd);
                    }
                }
                if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票，所有人投完，自動結算！").set_flags(dpp::m_ephemeral)); resolve_mvp_vote(bot, gid); }
                else ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票！").set_flags(dpp::m_ephemeral));
            }
            // ── 獵人射擊 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_hunter_", 0) == 0) {
                std::string rest = cid.substr(12);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake target(std::stoull(rest.substr(sep+1)));
                std::string tname;
                WolfPhase after;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::HUNTER_SHOOT) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    if (uid != g.hunter_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是獵人！").set_flags(dpp::m_ephemeral)); return; }
                    auto* t = wfind(g, target);
                    if (t && t->alive) { t->alive = false; tname = std::to_string(t->seat) + ". " + t->display_name; }
                    g.hunter_pending = false;
                    after = g.after_hunter;
                }
                dpp::embed e; e.set_title("🏹  獵人射擊！").set_color(0xE67E22);
                e.set_description("獵人帶走了 **" + tname + "**！");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                continue_after_hunter(bot, gid);
            }
            // ── 警長傳徽 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_badge_destroy_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(19));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::BADGE_TRANSFER) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    if (uid != g.badge_from) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有死亡警長可以操作！").set_flags(dpp::m_ephemeral)); return; }
                    g.sheriff_uid = 0;
                    for (auto& p : g.players) p.is_sheriff = false;
                }
                dpp::embed e; e.set_title("🗑️  警徽撕毀").set_color(0x808080).set_description("本局之後無警長。");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                continue_after_badge(bot, gid);
            }
            else if (cid.rfind("wolf_badge_", 0) == 0) {
                std::string rest = cid.substr(11);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake new_sheriff(std::stoull(rest.substr(sep+1)));
                std::string ns_name;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::BADGE_TRANSFER) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    if (uid != g.badge_from) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有死亡警長可以操作！").set_flags(dpp::m_ephemeral)); return; }
                    for (auto& p : g.players) p.is_sheriff = false;
                    g.sheriff_uid = new_sheriff;
                    auto* np = wfind(g, new_sheriff);
                    if (np) { np->is_sheriff = true; ns_name = np->display_name; }
                }
                dpp::embed e; e.set_title("🏅  警徽傳遞").set_color(0xF39C12);
                e.set_description("**" + ns_name + "** 成為新警長！（投票計 1.5 票）");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                continue_after_badge(bot, gid);
            }
            // ── 遺言結束 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_last_words_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(16));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::LAST_WORDS) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                    // 只有遺言者本人或主持人可以結束
                    if (uid != g.lw_current_victim && uid != g.host_id) {
                        ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有遺言者本人或主持人可以操作！").set_flags(dpp::m_ephemeral)); return;
                    }
                }
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(
                    dpp::embed().set_title("💬  遺言結束").set_color(0x7F8C8D).set_description("遺言時間結束。")));
                continue_last_words(bot, gid);
            }
            // ── 發言完成 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_speak_done_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(16));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::SHERIFF_SPEECH && g.phase != WolfPhase::DAY_SPEAK) {
                        ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return;
                    }
                    if (g.speak_idx < (int)g.speak_seats.size()) {
                        int cur_seat = g.speak_seats[g.speak_idx];
                        dpp::snowflake cur_uid = 0;
                        for (auto& p : g.players) if (p.seat == cur_seat) { cur_uid = p.uid; break; }
                        if (uid != cur_uid && uid != g.host_id) {
                            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有當前發言者或主持人可以操作！").set_flags(dpp::m_ephemeral)); return;
                        }
                    }
                }
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 發言結束！").set_flags(dpp::m_ephemeral));
                advance_speaker(bot, gid);
            }
            // ── 競選者退出 ─────────────────────────────────────────────────────
            else if (cid.rfind("wolf_withdraw_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(14));
                bool ok = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::SHERIFF_SPEECH) {
                        ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return;
                    }
                    if (g.speak_idx < (int)g.speak_seats.size()) {
                        int cur_seat = g.speak_seats[g.speak_idx];
                        dpp::snowflake cur_uid = 0;
                        for (auto& p : g.players) if (p.seat == cur_seat) { cur_uid = p.uid; break; }
                        if (uid != cur_uid) {
                            ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有當前發言者可以退出競選！").set_flags(dpp::m_ephemeral)); return;
                        }
                        auto& cands = g.candidates;
                        cands.erase(std::remove(cands.begin(), cands.end(), uid), cands.end());
                        ok = true;
                    }
                }
                if (ok) {
                    ev.reply(dpp::ir_channel_message_with_source, dpp::message("🚪 已退出競選！").set_flags(dpp::m_ephemeral));
                    advance_speaker(bot, gid);
                }
            }
            // ── PK 投票 ────────────────────────────────────────────────────────
            else if (cid.rfind("wolf_dvote_pk_resolve_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(22));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    if (it->second.host_id != uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以結算！").set_flags(dpp::m_ephemeral)); return; }
                    if (it->second.phase != WolfPhase::DAY_VOTE_PK) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是 PK 階段！").set_flags(dpp::m_ephemeral)); return; }
                }
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 結算中...").set_flags(dpp::m_ephemeral));
                resolve_pk_vote(bot, gid);
            }
            else if (cid.rfind("wolf_dvote_pk_", 0) == 0) {
                std::string rest = cid.substr(14);
                size_t sep = rest.find('_');
                if (sep == std::string::npos) return;
                uint64_t gid = std::stoull(rest.substr(0, sep));
                dpp::snowflake target(std::stoull(rest.substr(sep+1)));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::DAY_VOTE_PK) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是 PK 時間！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || !p->alive) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中！").set_flags(dpp::m_ephemeral)); return; }
                    if (std::find(g.pk_candidates.begin(), g.pk_candidates.end(), target) == g.pk_candidates.end()) {
                        ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只能投 PK 候選人！").set_flags(dpp::m_ephemeral)); return;
                    }
                    g.pk_votes[uid] = target;
                    if (g.day_vote_msg_pk_id) {
                        dpp::message upd = make_day_pk_vote_msg(g);
                        upd.id = g.day_vote_msg_pk_id; upd.channel_id = g.channel_id;
                        bot.message_edit(upd);
                    }
                }
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票！").set_flags(dpp::m_ephemeral));
            }
        }
        // ── 射龍門按鈕 ────────────────────────────────────────────────────────
        else if (cid.rfind("shoot_", 0) == 0) {
            // 解析 uid（所有射龍門按鈕 ID 格式：shoot_XXX_{uid} 或 shoot_again_{uid}_{bet}）
            auto sh_get_uid = [&]() -> dpp::snowflake {
                if (cid.rfind("shoot_again_", 0) == 0) {
                    std::string rest = cid.substr(12);
                    size_t sep = rest.rfind('_');
                    return dpp::snowflake(std::stoull(rest.substr(0, sep)));
                }
                size_t us = cid.rfind('_');
                return dpp::snowflake(std::stoull(cid.substr(us + 1)));
            };
            dpp::snowflake owner = sh_get_uid();
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }

            if (cid.rfind("shoot_again_", 0) == 0) {
                // shoot_again_{uid}_{bet}  → 開新訊息
                std::string rest = cid.substr(12);
                size_t sep = rest.rfind('_');
                int64_t bet = std::stoll(rest.substr(sep + 1));
                if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 無效下柱！").set_flags(dpp::m_ephemeral)); return; }
                if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ev.command.channel_id) == cfg.allin_thread_id) {
                    bet = get_chips(uid);
                    if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
                } else if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ev.command.channel_id) == cfg.min_bet_thread_id && bet < 1000) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 此討論串最低下柱為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return;
                }
                dpp::message start_msg = handle_shoot_start(uid, ev.command.channel_id, bet,
                    user.get_avatar_url(), ev.command.member.get_nickname());
                ev.reply(dpp::ir_channel_message_with_source, start_msg);
                ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = shoot_games.find(uid);
                        if (it != shoot_games.end())
                            it->second.msg_id = std::get<dpp::message>(cb.value).id;
                    }
                });
            } else {
                // shoot_go_ / shoot_up_ / shoot_dn_ / shoot_pass_
                ShootGame sg;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = shoot_games.find(uid);
                    if (it == shoot_games.end()) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("⚠️ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                    }
                    sg = it->second;
                    shoot_games.erase(it);
                }
                dpp::message result;
                if (cid.rfind("shoot_pass_", 0) == 0) {
                    result = make_shoot_pass_msg(sg);
                } else {
                    int direction = 0;
                    if      (cid.rfind("shoot_up_", 0) == 0) direction =  1;
                    else if (cid.rfind("shoot_dn_", 0) == 0) direction = -1;
                    result = make_shoot_result_msg(sg, direction);
                }
                ev.reply(dpp::ir_update_message, result);
            }
        }
        // ── 火箭升空按鈕 ──────────────────────────────────────────────────────
        else if (cid.rfind("rocket_", 0) == 0) {
            // 所有火箭按鈕都以 rocket_XXX_{uid} 或 rocket_again_{uid}_{bet} 結尾
            auto rk_get_uid = [&]() -> dpp::snowflake {
                if (cid.rfind("rocket_again_", 0) == 0) {
                    std::string rest = cid.substr(13);
                    size_t sep = rest.rfind('_');
                    return dpp::snowflake(std::stoull(rest.substr(0, sep)));
                }
                size_t us = cid.rfind('_');
                return dpp::snowflake(std::stoull(cid.substr(us + 1)));
            };
            dpp::snowflake owner = rk_get_uid();
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }

            if (cid.rfind("rocket_again_", 0) == 0) {
                // rocket_again_{uid}_{bet} → 開新局
                std::string rest = cid.substr(13);
                size_t sep = rest.rfind('_');
                int64_t bet = std::stoll(rest.substr(sep + 1));
                if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 無效下注！").set_flags(dpp::m_ephemeral)); return; }
                ev.reply(dpp::ir_channel_message_with_source,
                    handle_rocket_start(uid, ev.command.channel_id, bet,
                        user.get_avatar_url(),
                        ev.command.member.get_nickname()));
            } else {
                // rocket_up_ / rocket_cash_
                RocketGame rg;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = rocket_games.find(uid);
                    if (it == rocket_games.end()) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("⚠️ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                    }
                    rg = it->second;
                }

                if (cid.rfind("rocket_cash_", 0) == 0) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        rocket_games.erase(uid);
                    }
                    ev.reply(dpp::ir_update_message, make_rocket_cash_msg(rg));
                } else {
                    // rocket_up_
                    rg.presses++;
                    if (rk_explodes()) {
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            rocket_games.erase(uid);
                        }
                        ev.reply(dpp::ir_update_message, make_rocket_explode_msg(rg));
                    } else if (rg.presses >= 10) {
                        // 成功登月
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            rocket_games.erase(uid);
                        }
                        ev.reply(dpp::ir_update_message, make_rocket_moon_msg(rg));
                    } else {
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            rocket_games[uid] = rg;
                        }
                        ev.reply(dpp::ir_update_message, make_rocket_play_msg(rg));
                    }
                }
            }
        }
        // ── 卷軸按鈕 ──────────────────────────────────────────────────────────
        else if (cid.rfind("scroll_", 0) == 0) {
            // scroll_sel_{uid}  or  scroll_go_{uid}_{pct}_{count}
            auto sc_get_uid = [&]() -> dpp::snowflake {
                // after "scroll_sel_" or "scroll_go_" the next token is uid
                size_t prefix_end = cid.find('_', 7);  // skip "scroll_"
                size_t uid_start  = prefix_end + 1;
                size_t uid_end    = cid.find('_', uid_start);
                std::string uid_s = (uid_end == std::string::npos)
                    ? cid.substr(uid_start)
                    : cid.substr(uid_start, uid_end - uid_start);
                return dpp::snowflake(std::stoull(uid_s));
            };
            dpp::snowflake owner = sc_get_uid();
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的模擬器！").set_flags(dpp::m_ephemeral)); return;
            }

            if (cid.rfind("scroll_sel_", 0) == 0) {
                ev.reply(dpp::ir_update_message, make_scroll_sel_msg(uid));
            } else {
                // scroll_go_{uid}_{pct}_{count}
                // parse after "scroll_go_"
                std::string rest = cid.substr(10); // "scroll_go_" = 10 chars
                size_t p1 = rest.find('_');
                size_t p2 = rest.find('_', p1 + 1);
                int pct   = std::stoi(rest.substr(p1 + 1, p2 - p1 - 1));
                int count = std::stoi(rest.substr(p2 + 1));
                if (count < 1) count = 1;
                if (count > 100) count = 100;
                ev.reply(dpp::ir_update_message, make_scroll_result_msg(uid, pct, count));
            }
        }
        // ── 刮刮樂按鈕 ────────────────────────────────────────────────────────
        else if (cid.rfind("sc9_", 0) == 0) {
            // sc9_rev_{uid}_{idx}  sc9_cash_{uid}  sc9_extra_{uid}  sc9_again_{uid}_{bet}
            auto sk_get_uid = [&]() -> dpp::snowflake {
                if (cid.rfind("sc9_again_", 0) == 0) {
                    std::string rest = cid.substr(10);
                    size_t sep = rest.rfind('_');
                    return dpp::snowflake(std::stoull(rest.substr(0, sep)));
                }
                size_t us = cid.rfind('_');
                // sc9_rev_{uid}_{idx}: last token is idx, second-to-last is uid
                if (cid.rfind("sc9_rev_", 0) == 0) {
                    std::string rest = cid.substr(8); // after "sc9_rev_"
                    size_t sep = rest.rfind('_');
                    return dpp::snowflake(std::stoull(rest.substr(0, sep)));
                }
                return dpp::snowflake(std::stoull(cid.substr(us + 1)));
            };
            dpp::snowflake owner = sk_get_uid();
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的刮刮樂！").set_flags(dpp::m_ephemeral)); return;
            }

            if (cid.rfind("sc9_again_", 0) == 0) {
                // Start new game
                std::string rest = cid.substr(10);
                size_t sep = rest.rfind('_');
                int64_t bet = std::stoll(rest.substr(sep + 1));
                if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 無效下注！").set_flags(dpp::m_ephemeral)); return; }
                ev.reply(dpp::ir_channel_message_with_source,
                    handle_scratch_start(uid, ev.command.channel_id, bet,
                        user.get_avatar_url(), ev.command.member.get_nickname()));
            } else {
                // Get the current game
                ScratchGame g;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = scratch_games.find(uid);
                    if (it == scratch_games.end()) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("⚠️ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                    }
                    g = it->second;
                }

                if (cid.rfind("sc9_cash_", 0) == 0) {
                    // Collect
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        scratch_games.erase(uid);
                    }
                    ev.reply(dpp::ir_update_message, make_scratch_cash_msg(g));

                } else if (cid.rfind("sc9_early_", 0) == 0) {
                    // Early exit before 3-scratch minimum; pay penalty fee
                    double fee_mult = (g.safe_scratches == 1) ? 0.6 : 0.3;
                    int64_t fee = std::max((int64_t)1, (int64_t)(g.bet * fee_mult));
                    int64_t chips = get_chips(uid);
                    if (chips < fee) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 籌碼不足，無法提前出場！").set_flags(dpp::m_ephemeral)); return;
                    }
                    g.total_paid += fee;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        scratch_games.erase(uid);
                    }
                    ev.reply(dpp::ir_update_message, make_scratch_cash_msg(g));

                } else if (cid.rfind("sc9_extra_", 0) == 0) {
                    // Pay half-bet for one more scratch
                    if (g.extra_count >= SK_MAX_EXTRA) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 已達追加上限！").set_flags(dpp::m_ephemeral)); return;
                    }
                    int64_t extra_cost = std::max((int64_t)1, g.bet / 2);
                    int64_t chips = get_chips(uid);
                    if (chips < extra_cost) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 籌碼不足，無法追加刮格！").set_flags(dpp::m_ephemeral)); return;
                    }
                    g.total_paid += extra_cost;
                    g.extra_mode = true;
                    g.extra_count++;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        scratch_games[uid] = g;
                    }
                    ev.reply(dpp::ir_update_message, make_scratch_play_msg(g));

                } else if (cid.rfind("sc9_rev_", 0) == 0) {
                    // Reveal a square
                    std::string rest = cid.substr(8);
                    size_t sep = rest.rfind('_');
                    int idx = std::stoi(rest.substr(sep + 1));
                    if (idx < 0 || idx > 8 || ((g.revealed >> idx) & 1)) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("⚠️ 無效操作！").set_flags(dpp::m_ephemeral)); return;
                    }
                    // Check state: only allow if safe_scratches < 3 OR extra_mode
                    if (g.safe_scratches >= 3 && !g.extra_mode) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("⚠️ 請先選擇收手或付費多刮！").set_flags(dpp::m_ephemeral)); return;
                    }

                    g.revealed |= (1 << idx);
                    if (g.extra_mode) g.extra_mode = false;

                    if (g.sq[idx] == -1) {
                        // Bomb!
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            scratch_games.erase(uid);
                        }
                        ev.reply(dpp::ir_update_message, make_scratch_bomb_msg(g, idx));
                    } else {
                        g.safe_scratches++;
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            scratch_games[uid] = g;
                        }
                        ev.reply(dpp::ir_update_message, make_scratch_play_msg(g));
                    }
                }
            }
        }
        // ── 錢包分頁按鈕 ──────────────────────────────────────────────────────
        else if (cid.rfind("wallet_home_", 0) == 0 || cid.rfind("wallet_games_", 0) == 0 || cid.rfind("wallet_wolf_", 0) == 0) {
            dpp::snowflake owner(std::stoull(cid.substr(cid.rfind('_') + 1)));
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的錢包！").set_flags(dpp::m_ephemeral)); return;
            }
            if (cid.rfind("wallet_games_", 0) == 0)
                ev.reply(dpp::ir_update_message, make_wallet_games_msg(uid));
            else if (cid.rfind("wallet_wolf_", 0) == 0)
                ev.reply(dpp::ir_update_message, make_wallet_wolf_msg(uid));
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
    });

    // ── Modal 送出（管理員操作）─────────────────────────────────────────────
    bot.on_form_submit([](const dpp::form_submit_t& ev) {
        const std::string& cid = ev.custom_id;
        dpp::snowflake issuer = ev.command.get_issuing_user().id;

        // Pet rename modal
        if (cid.rfind("pet_rename_modal_", 0) == 0) {
            dpp::snowflake modal_uid(std::stoull(cid.substr(17)));
            if (issuer != modal_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string new_name;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    new_name = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        new_name = std::get<std::string>(sub.value);
            }
            // Trim whitespace
            while (!new_name.empty() && (new_name.front()==' '||new_name.front()=='\t')) new_name.erase(new_name.begin());
            while (!new_name.empty() && (new_name.back()==' '||new_name.back()=='\r'||new_name.back()=='\n')) new_name.pop_back();
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (!pet_data.count(modal_uid)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你沒有寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                pet_data[modal_uid].custom_name = new_name;
            }
            save_pet_data();
            std::string msg_text = new_name.empty() ? "✅ 已清除寵物暱稱！" : "✅ 已將寵物改名為 **" + new_name + "**！";
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message(msg_text).set_flags(dpp::m_ephemeral));
            return;
        }

        // Pet release modal
        if (cid.rfind("pet_release_modal_", 0) == 0) {
            dpp::snowflake modal_uid(std::stoull(cid.substr(18)));
            if (issuer != modal_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string input;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    input = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        input = std::get<std::string>(sub.value);
            }
            if (input != "放生") {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 輸入錯誤，放生已取消。").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_channel_message_with_source,
                handle_pet_release(modal_uid).set_flags(dpp::m_ephemeral));
            return;
        }

        if (cid != "admin_chips_modal" && cid != "admin_egg_modal" && cid != "admin_item_modal") return;
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

        } else if (cid == "admin_egg_modal") {
            // fields: [target_uid, chain_name]
            if (fields.size() < 2) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 請填寫蛋的種類！").set_flags(dpp::m_ephemeral)); return;
            }
            const std::string& chain = fields[1];
            if (chain != "嫩寶" && chain != "菇菇仔" && chain != "肥肥" && chain != "小企鵝") {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 蛋的種類必須是：嫩寶 / 菇菇仔 / 肥肥 / 小企鵝").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (pet_data.count(target_uid) && pet_data[target_uid].stage >= 0 && !pet_data[target_uid].chain.empty()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 該玩家已經有寵物了！").set_flags(dpp::m_ephemeral)); return;
                }
                Pet p; p.chain = chain; p.stage = 0;
                pet_data[target_uid] = p;
            }
            save_pet_data();
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 已給予 <@" + std::to_string((uint64_t)target_uid) +
                    "> 一顆 **" + chain + "的蛋**！").set_flags(dpp::m_ephemeral));

        } else if (cid == "admin_item_modal") {
            // fields: [target_uid, item_key, qty]
            if (fields.size() < 3) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 請填寫道具代碼和數量！").set_flags(dpp::m_ephemeral)); return;
            }
            const std::string& key = fields[1];
            if (!find_virtual_item(key)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 找不到道具代碼：`" + key + "`\n可用：inc_100/inc_60/inc_30/inc_10\ngrow_1~6\nevo_1~5").set_flags(dpp::m_ephemeral)); return;
            }
            int qty = 1;
            try { qty = std::stoi(fields[2]); } catch (...) {}
            if (qty <= 0 || qty > 999) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 數量必須在 1~999 之間！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[target_uid][key] += qty;
            }
            save_inventory();
            const VirtualShopItem* vi = find_virtual_item(key);
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 已給予 <@" + std::to_string((uint64_t)target_uid) +
                    "> **" + vi->name + "** × " + std::to_string(qty) + "！").set_flags(dpp::m_ephemeral));
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
        else if (cmd_name == "幫助" || cmd_name == "help") {
            ev.reply(dpp::ir_channel_message_with_source, make_help_msg(0));
        }
        else if (cmd_name == "領取" || cmd_name == "claim") {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message(handle_claim(uid)).set_flags(dpp::m_ephemeral));
        }
        else if (cmd_name == "每週領取" || cmd_name == "weekly") {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message(handle_weekly_claim(uid)).set_flags(dpp::m_ephemeral));
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
            ev.reply(dpp::ir_channel_message_with_source, make_shop_main_msg());
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
        else if (cmd_name == "寵物" || cmd_name == "pet") {
            ev.reply(dpp::ir_channel_message_with_source, make_pet_view_msg(uid,
                user.get_avatar_url(),
                ev.command.member.get_nickname()));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (cmd_name == "背包" || cmd_name == "bag" || cmd_name == "petuse") {
            ev.reply(dpp::ir_channel_message_with_source, make_pet_use_msg(uid));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (cmd_name == "寵物圖鑑" || cmd_name == "petdex") {
            ev.reply(dpp::ir_channel_message_with_source, make_petdex_msg("嫩寶"));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
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
        else if (cmd_name == "射" || cmd_name == "inbetween") {
            int64_t bet = 0;
            auto bp = ev.get_parameter("籌碼");
            std::string bet_str = std::holds_alternative<std::string>(bp) ? std::get<std::string>(bp) : "";
            std::string bet_lo = bet_str; for (auto& c2 : bet_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (bet_lo == "all");
            bet = is_all ? get_chips(uid) : (bet_str.empty() ? 0 : std::atoll(bet_str.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
            } else {
                if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("用法：`/射 籌碼:100` 或 `/射 籌碼:ALL`").set_flags(dpp::m_ephemeral)); return; }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 此討論串最低下柱為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return; }
            }
            ev.reply(dpp::ir_channel_message_with_source, handle_shoot_start(uid, ch, bet,
                user.get_avatar_url(), ev.command.member.get_nickname()));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = shoot_games.find(uid);
                    if (it != shoot_games.end())
                        it->second.msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
        else if (cmd_name == "火箭" || cmd_name == "rocket") {
            int64_t bet = 0;
            auto bp = ev.get_parameter("籌碼");
            std::string bet_str = std::holds_alternative<std::string>(bp) ? std::get<std::string>(bp) : "";
            std::string bet_lo = bet_str; for (auto& c2 : bet_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (bet_lo == "all");
            bet = is_all ? get_chips(uid) : (bet_str.empty() ? 0 : std::atoll(bet_str.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
            } else {
                if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("用法：`/火箭 籌碼:100` 或 `/火箭 籌碼:ALL`").set_flags(dpp::m_ephemeral)); return; }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 此討論串最低下注為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return; }
            }
            ev.reply(dpp::ir_channel_message_with_source,
                handle_rocket_start(uid, ch, bet,
                    user.get_avatar_url(),
                    ev.command.member.get_nickname()));
        }
        else if (cmd_name == "刮刮樂" || cmd_name == "scratch") {
            int64_t bet = 0;
            auto bp = ev.get_parameter("籌碼");
            std::string bet_str = std::holds_alternative<std::string>(bp) ? std::get<std::string>(bp) : "";
            std::string bet_lo = bet_str; for (auto& c2 : bet_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (bet_lo == "all");
            bet = is_all ? get_chips(uid) : (bet_str.empty() ? 0 : std::atoll(bet_str.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
            } else {
                if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("用法：`/刮刮樂 籌碼:100` 或 `/刮刮樂 籌碼:ALL`").set_flags(dpp::m_ephemeral)); return; }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 此討論串最低下注為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return; }
            }
            ev.reply(dpp::ir_channel_message_with_source,
                handle_scratch_start(uid, ch, bet,
                    user.get_avatar_url(),
                    ev.command.member.get_nickname()));
        }
        else if (cmd_name == "骰子") {
            int64_t bet = 0;
            auto bp = ev.get_parameter("籌碼");
            std::string bet_str = std::holds_alternative<std::string>(bp) ? std::get<std::string>(bp) : "";
            std::string bet_lo = bet_str; for (auto& c2 : bet_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (bet_lo == "all");
            bet = is_all ? get_chips(uid) : (bet_str.empty() ? 0 : std::atoll(bet_str.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
            } else {
                if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("用法：`/骰子 籌碼:100` 或 `/骰子 籌碼:ALL`").set_flags(dpp::m_ephemeral)); return; }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 此討論串最低下注為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return; }
            }
            if (get_chips(uid) < bet) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 籌碼不足").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_channel_message_with_source, start_dice(uid, ch, bet,
                user.get_avatar_url(), user.username));
        }
        else if (cmd_name == "21") {
            int64_t bet = 0;
            auto bp = ev.get_parameter("籌碼");
            std::string bet_str = std::holds_alternative<std::string>(bp) ? std::get<std::string>(bp) : "";
            std::string bet_lo = bet_str; for (auto& c2 : bet_lo) c2 = (char)std::tolower((unsigned char)c2);
            bool is_all = (bet_lo == "all");
            bet = is_all ? get_chips(uid) : (bet_str.empty() ? 0 : std::atoll(bet_str.c_str()));
            if (!cfg.allin_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.allin_thread_id) {
                bet = get_chips(uid);
                if (bet < 5000) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此房間需持有至少 **5,000** 碼才能 ALLIN！").set_flags(dpp::m_ephemeral)); return; }
            } else {
                if (bet <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("用法：`/21 籌碼:100` 或 `/21 籌碼:ALL`").set_flags(dpp::m_ephemeral)); return; }
                if (!cfg.min_bet_thread_id.empty() && std::to_string((uint64_t)ch) == cfg.min_bet_thread_id && bet < 1000) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 此討論串最低下注為 **1,000** 碼！").set_flags(dpp::m_ephemeral)); return; }
            }
            int64_t bal = get_chips(uid);
            if (bal < bet) {
                dpp::embed e; e.set_title("❌  籌碼不足").set_color(0xE74C3C);
                e.set_description("你持有 **" + std::to_string(bal) + "** 碼，無法下注 **" + std::to_string(bet) + "** 碼。");
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral));
                return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = user_bj.find(uid);
                if (it != user_bj.end()) { bj_games.erase(it->second); user_bj.erase(it); }
            }
            add_chips(uid, -bet);
            {
                const dpp::user& u = ev.command.get_issuing_user();
                BJGame g = start_bj(uid, ch, bet, u.get_avatar_url(), u.username);
                std::string status;
                if (is_blackjack(g.main_hand.cards)) {
                    bool dbj = is_blackjack(g.dealer_cards);
                    g.game_over = true;
                    if (dbj) { add_chips(uid, bet); status = "雙 BJ — 平局！"; }
                    else {
                        int64_t win = (int64_t)(bet * 1.5);
                        add_chips(uid, bet + win);
                        status = "🌟 Blackjack！贏得 **" + std::to_string(win) + "** 碼！";
                    }
                }
                ev.reply(dpp::ir_channel_message_with_source, make_bj_msg(g, status));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    uint64_t gid = g.id;
                    bj_games[gid] = g;
                    user_bj[uid]  = gid;
                }
                save_bj_games();
                ev.get_original_response([uid, gid = g.id](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        auto& m = std::get<dpp::message>(cb.value);
                        { std::lock_guard<std::mutex> lk(data_mutex);
                          auto it = bj_games.find(gid);
                          if (it != bj_games.end()) it->second.msg_id = m.id; }
                        save_bj_games();
                    }
                });
            } // end BJGame block
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
            std::string from_name = "<@" + std::to_string((uint64_t)uid)    + ">";
            std::string to_name   = "<@" + std::to_string((uint64_t)to_uid) + ">";
            ev.reply(dpp::ir_channel_message_with_source,
                handle_transfer_request(uid, from_name, to_uid, to_name, amount));
        }
        else if (cmd_name == "交易" || cmd_name == "trade") {
            dpp::snowflake target = std::get<dpp::snowflake>(ev.get_parameter("對象"));
            auto get_int = [&](const std::string& name) -> int64_t {
                auto p = ev.get_parameter(name);
                return p.index() == 0 ? 0 : std::get<int64_t>(p);
            };
            int from_item_id = (int)get_int("我的道具");
            int64_t from_chips = get_int("我的籌碼");
            int to_item_id   = (int)get_int("對方道具");
            int64_t to_chips = get_int("對方籌碼");
            if (target == uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不能和自己交易！").set_flags(dpp::m_ephemeral)); return;
            }
            const VirtualShopItem* from_vi = find_virtual_item_by_id(from_item_id);
            if (from_item_id && !from_vi) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 道具 ID `" + std::to_string(from_item_id) + "` 不存在！").set_flags(dpp::m_ephemeral)); return;
            }
            if (from_vi) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = inventory_data[uid].find(from_vi->key);
                if (it == inventory_data[uid].end() || it->second <= 0) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你沒有道具 **" + from_vi->name + "**！").set_flags(dpp::m_ephemeral)); return;
                }
            }
            if (from_chips < 0) from_chips = 0;
            if (from_chips > 0 && get_chips(uid) < from_chips) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你的籌碼不足！").set_flags(dpp::m_ephemeral)); return;
            }
            if (to_chips < 0) to_chips = 0;
            const VirtualShopItem* to_vi = find_virtual_item_by_id(to_item_id);
            if (to_item_id && !to_vi) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 對方道具 ID `" + std::to_string(to_item_id) + "` 不存在！").set_flags(dpp::m_ephemeral)); return;
            }
            TradeOffer t;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                t.id = trade_counter++;
                t.from_uid = uid; t.to_uid = target; t.channel_id = ch;
                t.from_item_id = from_item_id; t.from_chips = from_chips;
                t.to_item_id   = to_item_id;   t.to_chips   = to_chips;
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
        else if (cmd_name == "狼人殺" || cmd_name == "werewolf") {
            bool already = false;
            uint64_t gid = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (channel_wolf_game.count(ch)) { already = true; }
                else {
                    gid = wolf_counter++;
                    WolfGame g;
                    g.id = gid; g.channel_id = ch;
                    g.guild_id = ev.command.guild_id; g.host_id = uid;
                    wolf_games[gid] = g;
                    channel_wolf_game[ch] = gid;
                }
            }
            if (already) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此頻道已有進行中的狼人殺遊戲！").set_flags(dpp::m_ephemeral));
                return;
            }
            dpp::message m;
            { std::lock_guard<std::mutex> lk(data_mutex); m = make_wolf_lobby_msg(wolf_games[gid]); }
            ev.reply(dpp::ir_channel_message_with_source, m);
        }
        else if (cmd_name == "抽獎") {
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

            dpp::slashcommand lucky("幸運頻道", "隨機抽出幸運頻道號碼", bot.me.id);
            lucky.add_option(dpp::command_option(dpp::co_integer, "最大頻道數", "頻道總數（抽 1 到此數）", true));

            dpp::slashcommand transfer("轉帳", "轉移籌碼給其他人", bot.me.id);
            transfer.add_option(dpp::command_option(dpp::co_user,    "對象", "收款人",   true))
                    .add_option(dpp::command_option(dpp::co_integer, "金額", "轉帳碼數", true));

            dpp::slashcommand dice_cmd("骰子", "擲骰子押注", bot.me.id);
            dice_cmd.add_option(dpp::command_option(dpp::co_string, "籌碼", "下注碼數（數字 或 ALL）", true));

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

            dpp::slashcommand trade_cmd("交易", "向另一名玩家發起道具/籌碼交易", bot.me.id);
            trade_cmd.add_option(dpp::command_option(dpp::co_user,    "對象",     "交易對象",           true))
                     .add_option(dpp::command_option(dpp::co_integer, "我的道具", "我出的道具ID（0=無）", false))
                     .add_option(dpp::command_option(dpp::co_integer, "我的籌碼", "我出的籌碼（0=無）",  false))
                     .add_option(dpp::command_option(dpp::co_integer, "對方道具", "要對方出的道具ID（0=無）", false))
                     .add_option(dpp::command_option(dpp::co_integer, "對方籌碼", "要對方出的籌碼（0=無）",  false));

            dpp::slashcommand trade_en("trade", "Propose an item/chip trade with another player", bot.me.id);
            trade_en.add_option(dpp::command_option(dpp::co_user,    "對象",     "Trade target",         true))
                    .add_option(dpp::command_option(dpp::co_integer, "我的道具", "Your item ID (0=none)", false))
                    .add_option(dpp::command_option(dpp::co_integer, "我的籌碼", "Your chips (0=none)",   false))
                    .add_option(dpp::command_option(dpp::co_integer, "對方道具", "Their item ID (0=none)",false))
                    .add_option(dpp::command_option(dpp::co_integer, "對方籌碼", "Their chips (0=none)",  false));

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
                dpp::slashcommand("寵物",      "查看你的寵物狀態",              bot.me.id),
                dpp::slashcommand("背包",      "查看背包道具，點選使用",         bot.me.id),
                dpp::slashcommand("寵物圖鑑",  "查看所有寵物進化路線",          bot.me.id),
                dpp::slashcommand("虧損榜",    "查看全伺服器虧損排行榜",        bot.me.id),
                dpp::slashcommand("狼人殺",    "開始狼人殺遊戲（需要9名玩家）", bot.me.id),
                // English aliases (no-parameter ones)
                dpp::slashcommand("shop",      "Open the shop",                 bot.me.id),
                dpp::slashcommand("wallet",    "Check your chip balance",       bot.me.id),
                dpp::slashcommand("leaderboard","View chip leaderboard",        bot.me.id),
                dpp::slashcommand("lossboard", "View loss leaderboard",         bot.me.id),
                dpp::slashcommand("claim",     "Claim hourly 500 chips",        bot.me.id),
                dpp::slashcommand("weekly",    "Claim weekly 2000 chips",       bot.me.id),
                dpp::slashcommand("help",      "Show command list",             bot.me.id),
                dpp::slashcommand("pet",       "View your pet status",          bot.me.id),
                dpp::slashcommand("bag",       "View backpack and use items",   bot.me.id),
                dpp::slashcommand("petdex",    "View pet evolution chart",      bot.me.id),
                dpp::slashcommand("ledger",    "View purchase log (admin)",     bot.me.id),
                dpp::slashcommand("warnboard", "View warning leaderboard",      bot.me.id),
                dpp::slashcommand("werewolf",  "Start a werewolf game",         bot.me.id),
                dpp::slashcommand("raid",      "Sign up for raid",              bot.me.id),
                dpp::slashcommand("raidlog",   "View raid sign-up records",     bot.me.id),
                bj, draw, warn_cmd, lucky, transfer, dice_cmd, shoot_cmd, shoot_en,
                rocket_cmd, rocket_en, scratch_cmd, scratch_en,
                warn_en, lucky_en, transfer_en, trade_cmd, trade_en,
            }, gid);
        }
    });

    // ── on_ready ──────────────────────────────────────────────────────────────
    bot.on_ready([&bot](const dpp::ready_t& event) {
        // 清除舊的 global commands（避免與 guild commands 重複顯示）
        if (dpp::run_once<struct clear_global_cmds>())
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
        bot.start_timer([](dpp::timer)     { cleanup_expired(); },  3600);
        bot.start_timer([&bot](dpp::timer) { check_giveaways(bot); save_giveaways(); }, 30);
        printf("Bot 已上線：%s\n", bot.me.username.c_str());
    });

    bot.start(dpp::st_wait);
    return 0;
}
