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
#include "handler_decls.h"

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

// Unified item lookup by numeric ID — checks virtual items then gacha equipment
static std::pair<std::string,std::string> trade_item_info(int id) {
    if (!id) return {"",""};
    if (auto* vi = find_virtual_item_by_id(id)) return {vi->key, vi->name};
    if (auto* gi = find_gacha_item_by_id(id))   return {gi->key, gi->name};
    return {"",""};
}

static dpp::message make_trade_msg(const TradeOffer& t,
                                   const std::string& from_name,
                                   const std::string& to_name,
                                   const std::string& status = "") {
    auto item_desc = [](int id) -> std::string {
        auto [key, name] = trade_item_info(id);
        return std::string("`") + std::to_string(id) + "` " + (name.empty() ? "未知道具" : name);
    };

    std::string desc;
    desc += from_name + " 向 " + to_name + " 提出交易\n\n";

    auto chips_with_fee = [](int64_t chips) -> std::string {
        if (chips <= 0) return "";
        int64_t fee = (chips + 99) / 100;
        return "• 💰 " + std::to_string(chips) + " 籌碼（含 1% 手續費 " + std::to_string(fee) + " 碼，實付 " + std::to_string(chips + fee) + " 碼）\n";
    };

    desc += "**" + from_name + " 提供：**\n";
    bool from_empty = (!t.from_item_id && t.from_chips <= 0);
    if (t.from_item_id) {
        desc += "• " + item_desc(t.from_item_id) + "\n";
        auto [from_key, from_iname2] = trade_item_info(t.from_item_id);
        if (col_would_break_set(t.from_uid, from_key))
            desc += "　⚠️ 交易後 " + from_name + " 的收藏套組加成將會失效！\n";
    }
    if (t.from_chips > 0) desc += chips_with_fee(t.from_chips);
    if (from_empty) desc += "• （無）\n";

    desc += "\n**" + to_name + " 提供：**\n";
    bool to_empty = (!t.to_item_id && t.to_chips <= 0);
    if (t.to_item_id) {
        desc += "• " + item_desc(t.to_item_id) + "\n";
        auto [to_key, to_iname2] = trade_item_info(t.to_item_id);
        if (col_would_break_set(t.to_uid, to_key))
            desc += "　⚠️ 交易後 " + to_name + " 的收藏套組加成將會失效！\n";
    }
    if (t.to_chips > 0) desc += chips_with_fee(t.to_chips);
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
    load_gacha_pity();
    load_uc_stats();
    load_guess_stats();
    load_roulettestats();
    load_rps_stats();
    load_scratch_games();

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
                "!錢包","!幫助","!help","!寵物","!背包","!寵物圖鑑","!商店","!大廳",
                "!管理員權限","!警告榜單","!記帳","!狼人殺","!狼人殺榜單","!銀行",
                "!一夜狼人","!一夜狼人規則","!狼人殺規則",
                "!臥底","!誰是臥底",
                "!臥底 遊玩成人內容","!誰是臥底 遊玩成人內容",
                "!貓","!笑話","!轉蛋","!裝備","!怪物狩獵","!狩獵規則",
                "!道具圖鑑","!裝備圖鑑","!合成","!收藏","!輪盤","!探險","!猜拳","！猜拳","!強化"
            };
            for (auto& s : EXACT) if (content == s) return true;
            // Secret owner-only command
            if (content == "!偷看" && !cfg.notify_user_id.empty() &&
                std::to_string(uid) == cfg.notify_user_id) return true;
            // Prefix-match commands (with args)
            static const std::vector<std::string> PREFIX = {
                "!21 ","!骰子 ","!射 ","!火箭 ","!刮 ","!猜 ",
                "!幸運頻道 ","!警告 ","!轉帳 ","!交易 ","!卷軸使用 ","!輪盤 ","!猜拳 ","！猜拳 ",
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
            bool claimed = false;
            dpp::message m = handle_claim(uid, &claimed);
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
                    std::string from_name = "<@" + std::to_string((uint64_t)uid)     + ">";
                    std::string to_name   = "<@" + std::to_string((uint64_t)to_uid)  + ">";
                    dpp::message m = handle_transfer_request(uid, from_name, to_uid, to_name, amount);
                    m.channel_id = ch; bot.message_create(m);
                }
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
        // ── 21點 → handlers_bj.cpp ──────────────────────────────────────────
        else if (content.rfind("!21", 0) == 0 && (content.size() == 3 || content[3] == ' ')) {
            handle_bj_message(ev, content, uid, ch); return;
        }
#if 0 // ── BJ message handler moved to handlers_bj.cpp ────────────────────────
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
            // Kill any existing game, disabling its message buttons so stale clicks are prevented
            {
                BJGame old_g; bool had_old = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = user_bj.find(uid);
                    if (it != user_bj.end()) {
                        auto git = bj_games.find(it->second);
                        if (git != bj_games.end()) { old_g = git->second; had_old = true; }
                        bj_games.erase(it->second); user_bj.erase(it);
                    }
                }
                if (had_old) bj_disable_old_msg(bot, old_g);
            }
            add_chips(uid, -bet);
            BJGame g = start_bj(uid, ch, bet,
                ev.msg.author.get_avatar_url(), ev.msg.author.username);
            // Check immediate BJ
            std::string status;
            if (is_blackjack(g.main_hand.cards)) {
                bool dbj = is_blackjack(g.dealer_cards);
                g.game_over = true;
                if (dbj) {
                    add_chips(uid, bet);
                    status = "雙 BJ — 平局！";
                    { std::lock_guard<std::mutex> lk(data_mutex); bj_stats_data[uid].pushes++; }
                } else {
                    int64_t win = (int64_t)(bet * 1.5);
                    add_chips(uid, bet + win);
                    status = "🌟 Blackjack！贏得 **" + std::to_string(win) + "** 碼！";
                    { std::lock_guard<std::mutex> lk(data_mutex); bj_stats_data[uid].wins++; bj_stats_data[uid].profit += win; }
                }
                save_bjstats();
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
#endif // ── end BJ message handler ─────────────────────────────────────────────
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
        // !猜（排除 !猜拳，避免被更長的指令名搶先攔截）
        else if ((content.rfind("!猜", 0) == 0 || content.rfind("！猜", 0) == 0)
                 && content.rfind("!猜拳", 0) != 0 && content.rfind("！猜拳", 0) != 0) {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (guess_games.count(uid)) {
                    dpp::message m; m.set_content("❌ 你已有進行中的猜數字遊戲！");
                    m.channel_id = ch; bot.message_create(m); return;
                }
                GuessGame g;
                g.uid = uid; g.channel_id = ch;
                g.secret = guess_gen_secret();
                g.avatar_url   = ev.msg.author.get_avatar_url();
                g.display_name = ev.msg.member.get_nickname().empty()
                                 ? ev.msg.author.username : ev.msg.member.get_nickname();
                guess_games[uid] = g;
            }
            GuessGame snap; { std::lock_guard<std::mutex> lk(data_mutex); snap = guess_games[uid]; }
            auto msg = make_guess_msg(snap); msg.channel_id = ch;
            bot.message_create(msg, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (guess_games.count(uid))
                        guess_games[uid].msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
        // ── 狼人殺 / 一夜狼人 → handlers_wolf.cpp
        else if (content == "!狼人殺" || content == "!偷看" || content == "!狼人殺榜單" ||
                 content == "!一夜狼人" || content == "!一夜狼人規則" || content == "!狼人殺規則") {
            handle_wolf_message(ev, content, uid, ch); return;
        }
#if 0 // ── wolf/onenight message blocks moved to handlers_wolf.cpp ──────────
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
#endif // end old !狼人殺 block
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
            auto [from_key, from_iname] = trade_item_info(from_item_id);
            if (from_item_id && from_key.empty()) {
                dpp::message m; m.channel_id = ch;
                m.set_content("❌ 道具 ID `" + std::to_string(from_item_id) + "` 不存在！");
                bot.message_create(m); return;
            }
            if (!from_key.empty()) {
                static const std::set<std::string> NO_TRADE_KEYS = {"orb_ticket"};
                if (NO_TRADE_KEYS.count(from_key)) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ **" + from_iname + "** 不可交易！");
                    bot.message_create(m); return;
                }
                std::lock_guard<std::mutex> lk(data_mutex);
                auto& inv = inventory_data[uid];
                auto it = inv.find(from_key);
                if (it == inv.end() || it->second <= 0) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ 你沒有 **" + from_iname + "**！");
                    bot.message_create(m); return;
                }
            }
            if (from_chips < 0) from_chips = 0;
            if (from_chips > 0) {
                int64_t from_fee_chk = (from_chips + 99) / 100;
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
                static const std::set<std::string> NO_TRADE_KEYS2 = {"orb_ticket"};
                if (NO_TRADE_KEYS2.count(to_key_chk)) {
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
                t.from_item_id = from_item_id; t.from_chips = from_chips;
                t.to_item_id   = to_item_id;   t.to_chips   = to_chips;
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
#if 0 // remaining wolf/onenight message handlers (disabled)
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
        else if (content == "!一夜狼人") {
            bool already = false;
            uint64_t gid = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (channel_onw_game.count(ch)) { already = true; }
                else {
                    gid = onw_counter++;
                    ONWGame g;
                    g.id = gid; g.channel_id = ch;
                    g.guild_id = ev.msg.guild_id; g.host_id = uid;
                    g.role_counts = {{"狼人",2},{"預言家",1},{"強盜",1},{"搗蛋鬼",1},{"酒鬼",1},{"村民",1}};
                    ONWPlayer host;
                    host.uid = uid;
                    host.display_name = ev.msg.member.get_nickname().empty()
                        ? ev.msg.author.username : ev.msg.member.get_nickname();
                    g.players.push_back(host);
                    onw_games[gid] = g;
                    channel_onw_game[ch] = gid;
                }
            }
            if (already) {
                dpp::message m; m.set_content("❌ 此頻道已有進行中的一夜狼人遊戲！");
                m.channel_id = ch; bot.message_create(m); return;
            }
            dpp::message m;
            { std::lock_guard<std::mutex> lk(data_mutex); m = make_onw_lobby_msg(onw_games[gid]); }
            m.channel_id = ch;
            bot.message_create(m, [gid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = onw_games.find(gid);
                    if (it != onw_games.end())
                        it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
        else if (content == "!一夜狼人規則") {
            dpp::embed e;
            e.set_title("🌙  一夜終極狼人 — 規則").set_color(0x2C3E50);
            e.set_description("3～8 人遊玩。每人有一張身份牌，另有 3 張中央牌（不屬於任何人）。\n"
                "夜晚各角色依序在**私訊**進行行動，天亮後所有人討論並投票放逐一人。");
            e.add_field("🐺 狼人",
                "夜晚睜眼確認同伴。若為孤狼可偷看一張中央牌。\n**目標：** 不被投死。", false);
            e.add_field("🐾 頭狼",
                "與其他狼人互認。若中央有狼人牌，可感染一名玩家讓他成為狼人（或略過）。\n**目標：** 不被投死。", false);
            e.add_field("🩱 皮革匠",
                "無夜晚行動。\n**目標：** 讓自己被投死，即可**獨自獲勝**（其他人全輸）。", false);
            e.add_field("🔮 預言家",
                "可查看一名玩家的身份，或查看兩張中央牌（可略過）。\n**目標：** 找出並投死狼人。", false);
            e.add_field("🗡️ 強盜",
                "可與一名玩家互換身份（立刻看到新身份），或略過。\n**目標：** 依換來的身份決定陣營。", false);
            e.add_field("😈 搗蛋鬼",
                "可交換兩名其他玩家的身份（自己看不到），或略過。\n**目標：** 幫助好人找出狼人。", false);
            e.add_field("🧙 女巫",
                "偷看一張中央牌，可選擇把它換給任意玩家（含自己），或略過。\n**目標：** 根據最終身份決定陣營。", false);
            e.add_field("🃏 村子白痴",
                "選擇左移或右移，所有玩家的牌循環移動一格。你的原始身份牌會翻面公開。\n**目標：** 不被投死（好人陣營）。", false);
            e.add_field("🍺 酒鬼",
                "必須從中央取一張牌換掉自己（不看是什麼）。\n**目標：** 你自己也不知道最終身份是什麼。", false);
            e.add_field("😴 失眠者",
                "夜晚結束後可看到自己當下的最終身份。\n**目標：** 根據最終身份決定陣營。", false);
            e.add_field("🏘️ 村民",
                "無夜晚行動，靠白天討論與說服。\n**目標：** 投死狼人。", false);
            e.add_field("☀️ 勝負條件（優先順序）",
                "🩱 皮革匠被投死 → **皮革匠獨贏**，其他人全輸\n"
                "🏘️ 有狼人被投死 → **村民陣營獲勝**\n"
                "🏘️ 場上無狼人 + 平票無人出局 → **村民獲勝**\n"
                "💀 場上無狼人 + 有人出局 → **無人獲勝**\n"
                "🐺 狼人存活未被投死 → **狼人陣營獲勝**", false);
            e.add_field("💰 獎勵", "獲勝方 **+300** 碼，落敗方 **+100** 碼", false);
            e.set_footer(dpp::embed_footer().set_text("使用 !一夜狼人 開始遊戲"));
            dpp::message m; m.add_embed(e); m.channel_id = ch;
            bot.message_create(m);
        }
        else if (content == "!狼人殺規則") {
            dpp::embed e;
            e.set_title("🐺  狼人殺 — 規則").set_color(0x8B0000);
            e.set_description("**固定 9 人**遊玩。角色：狼人×3、村民×3、預言家×1、女巫×1、獵人×1。");
            e.add_field("🐺 狼人 ×3",
                "夜晚在私人討論串商議，投票選定一名玩家殺害。\n"
                "**目標：** 狼人數 ≥ 好人存活數，或屠滅所有村民，或屠滅所有神職。", false);
            e.add_field("🏘️ 村民 ×3",
                "無特殊技能，靠白天討論推理。\n**目標：** 活到狼人全滅。", false);
            e.add_field("🔮 預言家 ×1",
                "每天夜晚可查驗一名玩家，獲知其是「好人」或「狼人」。\n**目標：** 引導好人找出所有狼人。", false);
            e.add_field("🧪 女巫 ×1",
                "有**解藥×1**（救今晚被殺的人）和**毒藥×1**（毒死任意一人），各用一次。\n"
                "**注意：** 被女巫毒死的獵人不能開槍。\n**目標：** 幫助好人陣營獲勝。", false);
            e.add_field("🏹 獵人 ×1",
                "死亡時（被投票或被狼殺）可帶走一名玩家。被女巫毒死時不能開槍。\n**目標：** 臨死拉走狼人。", false);
            e.add_field("🌙 夜晚流程",
                "1️⃣ 狼人在私人討論串投票選目標\n"
                "2️⃣ 預言家查驗一名玩家\n"
                "3️⃣ 女巫決定是否使用解藥／毒藥", false);
            e.add_field("☀️ 白天流程",
                "公布昨晚死亡情況 → 存活玩家討論 → 投票放逐一人\n"
                "（票數相同則進入 PK，PK 票最多者出局）", false);
            e.add_field("🏆 勝負條件",
                "🏘️ **好人勝：** 所有狼人死亡\n"
                "🐺 **狼人勝：** 狼人數 ≥ 好人存活數，或所有村民死亡，或所有神職死亡", false);
            e.set_footer(dpp::embed_footer().set_text("使用 !狼人殺 開始遊戲（需滿 9 人）"));
            dpp::message m; m.add_embed(e); m.channel_id = ch;
            bot.message_create(m);
        }
#endif // ── end wolf/onenight message blocks ──────────────────────────────────
        // ── 誰是臥底 → handlers_uc.cpp
        else if (content == "!臥底" || content == "！臥底" ||
                 content == "!誰是臥底" || content == "！誰是臥底" ||
                 content == "!臥底 遊玩成人內容" || content == "！臥底 遊玩成人內容" ||
                 content == "!誰是臥底 遊玩成人內容" || content == "！誰是臥底 遊玩成人內容") {
            handle_uc_message(ev, content, uid, ch); return;
        }
#if 0 // ── UC message blocks moved to handlers_uc.cpp ────────────────────────
        else if (content == "!臥底" || content == "！臥底" ||
                 content == "!誰是臥底" || content == "！誰是臥底" ||
                 content == "!臥底 遊玩成人內容" || content == "！臥底 遊玩成人內容" ||
                 content == "!誰是臥底 遊玩成人內容" || content == "！誰是臥底 遊玩成人內容") {
            bool adult_game = (content.find("遊玩成人內容") != std::string::npos);
            bool already = false;
            uint64_t gid = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (channel_uc_game.count(ch)) { already = true; }
                else {
                    gid = uc_counter++;
                    UCGame g;
                    g.id = gid; g.channel_id = ch;
                    g.guild_id = ev.msg.guild_id; g.host_id = uid;
                    g.adult_allowed = adult_game;
                    if (adult_game) g.word_pool = "adult";
                    UCPlayer host;
                    host.uid = uid; host.seat = 0;
                    host.display_name = ev.msg.member.get_nickname().empty()
                        ? ev.msg.author.username : ev.msg.member.get_nickname();
                    g.players.push_back(host);
                    uc_games[gid] = g;
                    channel_uc_game[ch] = gid;
                }
            }
            if (already) {
                dpp::message m; m.set_content("❌ 此頻道已有進行中的誰是臥底遊戲！");
                m.channel_id = ch; bot.message_create(m); return;
            }
            dpp::message m;
            { std::lock_guard<std::mutex> lk(data_mutex); m = uc_lobby_msg(uc_games[gid]); }
            m.channel_id = ch;
            bot.message_create(m, [gid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = uc_games.find(gid);
                    if (it != uc_games.end())
                        it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
#endif // ── end UC message blocks ──────────────────────────────────────────────
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
#if 0 // ── raid/hunt message blocks moved to handlers_raid.cpp ─────────────────
        else if (content == "!怪物狩獵") {
            std::string dn = ev.msg.member.get_nickname().empty()
                           ? ev.msg.author.username : ev.msg.member.get_nickname();
            std::string av = ev.msg.author.get_avatar_url();
            Pet pet;
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = pet_data.find(uid); if (it != pet_data.end()) pet = it->second; }
            dpp::message m = make_hunt_main_msg(uid, pet, dn, av);
            m.channel_id = ch;
            bot.message_create(m, [uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    msg_owner[std::get<dpp::message>(cb.value).id] = uid;
                }
            });
        }
        else if (content == "!狩獵規則") {
            dpp::embed e;
            e.set_title("⚔️  怪物狩獵 — 系統規則").set_color(0xC0392B);
            e.add_field("📜 狩獵卷",
                "每天第一次 `!領取` 會獲得 **2 張狩獵卷**。\n"
                "也可在 **商店 → 虛擬商店 → 特殊道具** 購買（3000 碼/張）。", false);
            e.add_field("⭐ 難度解鎖",
                "**簡單**：直接開放\n"
                "**普通**：需通關所有簡單怪物（各至少一次）\n"
                "**困難**：需通關所有普通怪物\n"
                "**怪物之王**：需通關所有困難怪物", false);
            e.add_field("🎮 戰鬥流程",
                "選擇怪物 → 消耗 1 張狩獵卷 → 開始回合制戰鬥\n"
                "**75% 機率**玩家先手（裝備**迅捷狼王的寶珠**則必定先手）。\n"
                "每回合選擇：⚔️ 普通攻擊 或 💥 耗費氣力的攻擊", false);
            e.add_field("⚔️ 攻擊說明",
                "**普通攻擊**：傷害 = 寵物 ATK − 怪物 DEF（最小 0）\n"
                "**氣力攻擊**：傷害 = 隨機 0.5~1.5 倍 ATK − 怪物 DEF（高風險高報酬）\n"
                "怪物每回合反擊：傷害 = 怪物 ATK − 寵物 DEF（最小 0）", false);
            e.add_field("🏆 通關獎勵",
                "通關後獲得 **隨機範圍的籌碼**，首次通關有**額外獎勵**。\n"
                "有機率掉落**成長道具**（可在背包或商店查看）。", false);
            e.add_field("🩹 負面狀態影響",
                "**受傷**：無法進行狩獵\n"
                "**肌肉緊繃**：每回合攻擊有 30% 機率失敗（怪物仍反擊）", false);
            e.add_field("⚔️ 裝備系統",
                "使用 `!轉蛋` 抽取裝備，有武器・手套・衣服・鞋子・靈魂寶珠五個部位。\n"
                "使用 `!裝備` 管理與更換裝備，裝備會提升寵物的 ATK / HP / DEF。", false);
            e.add_field("⏰ 注意事項",
                "戰鬥有 **20 分鐘**限時，超時視為失敗。\n"
                "失敗（或逾時）寵物會獲得「**受傷**」狀態，需使用**高級傷藥**恢復。", false);
            dpp::message m; m.channel_id = ch; m.add_embed(e);
            bot.message_create(m);
        }
#endif // ── end raid/hunt message blocks ────────────────────────────────────────
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
#if 0 // ── roulette message block moved to handlers_roulette.cpp ──────────────
        else if (content.rfind("!輪盤賭", 0) == 0) {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (roulette_rooms.count(ch)) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ 此頻道已有進行中的輪盤賭！");
                    bot.message_create(m); return;
                }
            }
            // Parse args after "!輪盤賭"
            const std::string rl_prefix = "!輪盤賭";
            std::string rest;
            if (content.size() > rl_prefix.size() && content[rl_prefix.size()] == ' ')
                rest = content.substr(rl_prefix.size() + 1);
            if (rest.empty()) {
                dpp::message m; m.channel_id = ch;
                m.set_content("用法：`!輪盤賭 [籌碼]` 或 `!輪盤賭 [籌碼] @玩家`\n例：`!輪盤賭 1000` 或 `!輪盤賭 5000 @someone`");
                bot.message_create(m); return;
            }
            std::istringstream iss_rl(rest);
            std::string amount_str, mention_str;
            iss_rl >> amount_str >> mention_str;
            int64_t stake_rl = 0;
            try { stake_rl = std::stoll(amount_str); } catch (...) {}
            if (stake_rl <= 0) {
                dpp::message m; m.channel_id = ch;
                m.set_content("❌ 籌碼金額必須是正整數！"); bot.message_create(m); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (get_chips(uid) < stake_rl) {
                    dpp::message m; m.channel_id = ch;
                    m.set_content("❌ 你的籌碼不足 " + std::to_string(stake_rl) + " 碼！");
                    bot.message_create(m); return;
                }
            }
            // Parse optional invite mention <@uid> or <@!uid>
            dpp::snowflake invited_rl = 0;
            if (!mention_str.empty() && mention_str.size() > 3 && mention_str[0] == '<' && mention_str[1] == '@') {
                std::string id_s = mention_str.substr(2);
                if (id_s.back() == '>') id_s.pop_back();
                if (!id_s.empty() && id_s[0] == '!') id_s = id_s.substr(1);
                try { invited_rl = dpp::snowflake(std::stoull(id_s)); } catch (...) {}
            }
            std::string dn_rl = ev.msg.member.get_nickname();
            if (dn_rl.empty()) dn_rl = ev.msg.author.username;
            RouletteRoom rr;
            rr.channel_id = ch;
            rr.p1_uid = uid; rr.p1_name = dn_rl;
            rr.p1_avatar = ev.msg.author.get_avatar_url();
            rr.stake = stake_rl;
            rr.invited_uid = invited_rl;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                roulette_rooms[ch] = rr;
            }
            dpp::message rl_msg = make_roulette_room_msg(roulette_rooms[ch]);
            rl_msg.channel_id = ch;
            bot.message_create(rl_msg, [ch](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (roulette_rooms.count(ch))
                        roulette_rooms[ch].msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
#endif // ── end roulette message block ─────────────────────────────────────────
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
                if (!fkey.empty()) {
                    auto it2 = inventory_data[t.from_uid].find(fkey);
                    if (it2 == inventory_data[t.from_uid].end() || it2->second <= 0)
                        fail_reason = "提案方已沒有該道具！交易取消。";
                }
                int64_t from_fee = (t.from_chips > 0) ? (t.from_chips + 99) / 100 : 0;
                int64_t to_fee   = (t.to_chips   > 0) ? (t.to_chips   + 99) / 100 : 0;
                if (fail_reason.empty() && t.from_chips > 0 && chip_data[t.from_uid].chips < t.from_chips + from_fee)
                    fail_reason = "提案方籌碼不足（含手續費）！交易取消。";
                if (fail_reason.empty() && !tkey.empty()) {
                    auto it2 = inventory_data[t.to_uid].find(tkey);
                    if (it2 == inventory_data[t.to_uid].end() || it2->second <= 0)
                        fail_reason = "你沒有對方要求的道具！交易取消。";
                }
                if (fail_reason.empty() && t.to_chips > 0 && chip_data[t.to_uid].chips < t.to_chips + to_fee)
                    fail_reason = "你的籌碼不足（含手續費）！交易取消。";

                if (!fail_reason.empty()) {
                    trade_offers.erase(tid);
                } else {
                    // Execute
                    if (!fkey.empty()) {
                        inventory_data[t.from_uid][fkey]--;
                        inventory_data[t.to_uid][fkey]++;
                    }
                    if (!tkey.empty()) {
                        inventory_data[t.to_uid][tkey]--;
                        inventory_data[t.from_uid][tkey]++;
                    }
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
            ev.reply(dpp::ir_update_message, make_trade_msg(t, from_name, to_name, "ok"));
        }
        // ── 商店按鈕（楓之谷 + 虛擬商店）────────────────────────────────────
        else if (cid.rfind("shop_", 0) == 0) {
            if (!page_is_mine(ev.command.message_id, uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的頁面！").set_flags(dpp::m_ephemeral)); return;
            }
            if (cid == "shop_main") {
                ev.reply(dpp::ir_update_message, make_shop_main_msg(std::to_string((uint64_t)uid)));
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
        // ── 轉蛋按鈕 ──────────────────────────────────────────────────────────
        else if (cid.rfind("gacha_", 0) == 0) {
            // Extract uid from end of button id (format: gacha_xxx_UID)
            auto get_btn_uid = [&](const std::string& s) -> dpp::snowflake {
                size_t p = s.rfind('_');
                if (p == std::string::npos) return 0;
                return dpp::snowflake(std::stoull(s.substr(p+1)));
            };
            dpp::snowflake btn_uid = get_btn_uid(cid);
            if (uid != btn_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral));
                return;
            }
            std::string dn = user.global_name.empty() ? user.username : user.global_name;
            std::string av = user.get_avatar_url();

            if (cid.rfind("gacha_main_", 0) == 0) {
                ev.reply(dpp::ir_update_message, make_gacha_main_msg(uid, dn, av));
            } else if (cid.rfind("gacha_banner_normal_", 0) == 0) {
                ev.reply(dpp::ir_update_message, make_gacha_banner_msg(uid, false, dn, av));
            } else if (cid.rfind("gacha_banner_star_", 0) == 0) {
                ev.reply(dpp::ir_update_message, make_gacha_banner_msg(uid, true, dn, av));
            } else if (cid.rfind("gacha_norm_", 0) == 0) {
                // gacha_norm_N_UID
                std::string mid = cid.substr(11); // remove "gacha_norm_"
                size_t sep = mid.find('_');
                int count = std::stoi(mid.substr(0, sep));
                int cost = count * 50;
                if (get_chips(uid) < cost) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 籌碼不足！需要 " + std::to_string(cost) + " 碼").set_flags(dpp::m_ephemeral));
                    return;
                }
                add_chips(uid, -(int64_t)cost);
                std::vector<const GachaItem*> pulls;
                for (int i = 0; i < count; i++) pulls.push_back(&gacha_pull_one(false));

                // 保底：每 200 抽強制 UR
                int pity_after = 0;
                bool pity_fired = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    gacha_pity_data[(uint64_t)uid] += count;
                    if (gacha_pity_data[(uint64_t)uid] >= 200) {
                        gacha_pity_data[(uint64_t)uid] -= 200;
                        pity_fired = true;
                    }
                    pity_after = gacha_pity_data[(uint64_t)uid];
                }
                if (pity_fired) pulls.push_back(&gacha_pull_ur_pity());

                save_chips(); save_inventory(); save_gacha_pity();
                ev.reply(dpp::ir_update_message, make_gacha_result_msg(uid, pulls, false, dn, av, pity_after, pity_fired));
            } else if (cid.rfind("gacha_star_", 0) == 0) {
                std::string mid = cid.substr(11);
                size_t sep = mid.find('_');
                int count = std::stoi(mid.substr(0, sep));
                int stars_needed = count;
                int cur_stars = 0;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = inventory_data.find(uid);
                    if (it != inventory_data.end()) {
                        auto sit = it->second.find("star_unknown");
                        if (sit != it->second.end()) cur_stars = sit->second;
                    }
                }
                if (cur_stars < stars_needed) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 星星不足！需要 " + std::to_string(stars_needed) + " 顆").set_flags(dpp::m_ephemeral));
                    return;
                }
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    inventory_data[uid]["star_unknown"] -= stars_needed;
                }
                std::vector<const GachaItem*> pulls;
                for (int i = 0; i < count; i++) pulls.push_back(&gacha_pull_one(true));
                save_inventory();
                ev.reply(dpp::ir_update_message, make_gacha_result_msg(uid, pulls, true, dn, av));
            }
        }
        // ── 裝備按鈕 ──────────────────────────────────────────────────────────
        else if (cid.rfind("equip_", 0) == 0) {
            auto get_btn_uid2 = [&](size_t pfx_len) -> dpp::snowflake {
                // format after prefix: UID_... or just UID
                std::string rest = cid.substr(pfx_len);
                size_t sep = rest.find('_');
                return dpp::snowflake(std::stoull(sep == std::string::npos ? rest : rest.substr(0, sep)));
            };

            std::string dn = user.global_name.empty() ? user.username : user.global_name;
            std::string av = user.get_avatar_url();
            Pet pet; { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) pet = it->second; }

            if (cid.rfind("equip_main_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(11)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                ev.reply(dpp::ir_update_message, make_equip_msg(uid, pet, dn, av));
            } else if (cid.rfind("equip_slot_", 0) == 0) {
                // equip_slot_UID_SLOT or equip_slot_UID_SLOT_PAGE
                std::string rest = cid.substr(11);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                std::string rest2 = rest.substr(s1+1);
                size_t s2 = rest2.find('_');
                std::string slot = (s2 == std::string::npos) ? rest2 : rest2.substr(0, s2);
                int page = (s2 == std::string::npos) ? 0 : std::stoi(rest2.substr(s2+1));
                ev.reply(dpp::ir_update_message, make_equip_slot_msg(uid, slot, dn, av, page));
            } else if (cid.rfind("equip_set_", 0) == 0) {
                // equip_set_UID_EQKEY
                std::string rest = cid.substr(10);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                std::string eq_key = rest.substr(s1+1);
                auto* gi = find_gacha_item(eq_key);
                if (!gi) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 裝備不存在！").set_flags(dpp::m_ephemeral)); return; }
                // Check inventory
                bool has_item = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = inventory_data.find(uid);
                    has_item = it != inventory_data.end() && it->second.count(eq_key) && it->second.at(eq_key) > 0;
                    if (has_item) {
                        auto& eq = equipped_data[uid];
                        if      (gi->slot == "W") eq.weapon  = eq_key;
                        else if (gi->slot == "G") eq.glove   = eq_key;
                        else if (gi->slot == "C") eq.clothes = eq_key;
                        else if (gi->slot == "S") eq.shoes   = eq_key;
                        else if (gi->slot == "K") eq.orb     = eq_key;
                    }
                }
                if (!has_item) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 背包中沒有此裝備！").set_flags(dpp::m_ephemeral)); return; }
                save_equipped();
                ev.reply(dpp::ir_update_message, make_equip_slot_msg(uid, gi->slot, dn, av));
            } else if (cid.rfind("equip_unequip_", 0) == 0) {
                // equip_unequip_UID_SLOT
                std::string rest = cid.substr(14);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                std::string slot = rest.substr(s1+1);
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto& eq = equipped_data[uid];
                    if      (slot == "W") eq.weapon  = "";
                    else if (slot == "G") eq.glove   = "";
                    else if (slot == "C") eq.clothes = "";
                    else if (slot == "S") eq.shoes   = "";
                    else if (slot == "K") eq.orb     = "";
                }
                save_equipped();
                ev.reply(dpp::ir_update_message, make_equip_slot_msg(uid, slot, dn, av));
            }
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
        // ── 單人怪物狩獵 / 村落按鈕 → handlers_hunt.cpp ──────────────────────
        else if (cid.rfind("hunt_", 0) == 0 || cid.rfind("village_", 0) == 0) {
            handle_hunt_button(ev); return;
        }
        // ── 21點按鈕 → handlers_bj.cpp ──────────────────────────────────────
        else if (cid.rfind("bj_", 0) == 0) {
            handle_bj_button(ev); return;
        }
#if 0 // ── raid/hunt/village button blocks moved to handlers_raid.cpp ──────────
        else if (cid.rfind("hunt_", 0) == 0 || cid.rfind("rroom_", 0) == 0 || cid.rfind("raid_", 0) == 0 || cid.rfind("village_", 0) == 0) {
            std::string dn = user.global_name.empty() ? user.username : user.global_name;
            std::string av = user.get_avatar_url();

            auto get_hunt_uid = [&](const std::string& prefix) -> dpp::snowflake {
                std::string rest = cid.substr(prefix.size());
                size_t sep = rest.find('_');
                return dpp::snowflake(std::stoull(sep == std::string::npos ? rest : rest.substr(0, sep)));
            };

            // ── Raid room buttons ──────────────────────────────────────────────────
            if (cid.rfind("rroom_join_", 0) == 0) {
                dpp::snowflake ch(std::stoull(cid.substr(11)));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = raid_rooms.find(ch);
                if (it == raid_rooms.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 找不到組隊房間！").set_flags(dpp::m_ephemeral));
                    return;
                }
                auto& room = it->second;
                if (room.msg_id == 0) room.msg_id = ev.command.message_id;
                if (room.member_uids.size() >= 4) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 房間已滿（4 人）！").set_flags(dpp::m_ephemeral));
                    return;
                }
                // Check not already in
                for (auto& m : room.member_uids) if (m == uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你已在房間內！").set_flags(dpp::m_ephemeral));
                    return;
                }
                room.member_uids.push_back(uid);
                room.member_names[uid]   = dn;
                room.member_avatars[uid] = av;
                ev.reply(dpp::ir_update_message, make_raid_room_msg(room));
                return;
            }

            if (cid.rfind("rroom_dissolve_", 0) == 0) {
                dpp::snowflake ch(std::stoull(cid.substr(15)));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = raid_rooms.find(ch);
                if (it == raid_rooms.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 找不到組隊房間！").set_flags(dpp::m_ephemeral));
                    return;
                }
                auto& room = it->second;
                if (uid != room.host_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 只有開房者可以解散！").set_flags(dpp::m_ephemeral));
                    return;
                }
                raid_rooms.erase(it);
                ev.reply(dpp::ir_update_message,
                    dpp::message("🗑️ **" + dn + "** 解散了組隊房間。"));
                return;
            }

            if (cid.rfind("rroom_start_", 0) == 0) {
                dpp::snowflake ch(std::stoull(cid.substr(12)));
                RaidRoom room;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = raid_rooms.find(ch);
                    if (it == raid_rooms.end()) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 找不到組隊房間！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    room = it->second;
                    if (uid != room.host_uid) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 只有開房者可以開始戰鬥！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    if ((int)room.member_uids.size() < 2) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 需要至少 2 名成員才可開始！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    // Check all members have weekly_hunt_scroll (skip in practice mode)
                    if (!room.practice_mode) {
                        for (auto& muid : room.member_uids) {
                            int cnt = 0;
                            auto iit = inventory_data.find(muid);
                            if (iit != inventory_data.end() && iit->second.count("weekly_hunt_scroll"))
                                cnt = iit->second.at("weekly_hunt_scroll");
                            if (cnt <= 0) {
                                std::string nm = room.member_names.count(muid) ? room.member_names.at(muid) : "某成員";
                                ev.reply(dpp::ir_channel_message_with_source,
                                    dpp::message("❌ **" + nm + "** 沒有每週怪物狩獵卷，無法開始！").set_flags(dpp::m_ephemeral));
                                return;
                            }
                        }
                    }
                    // Remove room (scrolls consumed only on victory)
                    raid_rooms.erase(ch);
                }

                // ── 暗黑龍王：特殊三頭戰鬥系統 ────────────────────────────────────
                if (room.boss_key == "dark_dragon") {
                    DDGame dg;
                    dg.channel_id    = room.channel_id;
                    dg.started_at    = time(nullptr);
                    dg.bomb_cooldown = 3;
                    // 三頭初始化
                    dg.heads[0] = DDHead{"左頭", 600, 600, 27, 5};
                    dg.heads[1] = DDHead{"中頭", 650, 650, 30, 3};
                    dg.heads[2] = DDHead{"右頭", 450, 450, 24, 10};
                    // 建立玩家（複製 RaidGame 的玩家建立邏輯）
                    for (auto& muid : room.member_uids) {
                        Pet pet2; PetStats ps;
                        std::string orb_key;
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            auto pit = pet_data.find(muid);
                            if (pit != pet_data.end()) pet2 = pit->second;
                            auto eit = equipped_data.find(muid);
                            if (eit != equipped_data.end()) orb_key = eit->second.orb;
                        }
                        ps = calc_pet_stats(muid, pet2);
                        DDPlayer p;
                        p.uid          = muid;
                        p.display_name = room.member_names.count(muid) ? room.member_names.at(muid) : "?";
                        p.avatar_url   = room.member_avatars.count(muid) ? room.member_avatars.at(muid) : "";
                        p.hp           = ps.hp;
                        p.max_hp       = ps.hp;
                        p.atk          = ps.atk;
                        p.def          = ps.def;
                        p.orb_key      = orb_key;
                        p.alive        = true;
                        // 組隊模式：UR 寶珠的個人加成移除
                        if (orb_key == "EQ_K_UR" && room.member_uids.size() > 1) {
                            auto* ur_gi = find_gacha_item("EQ_K_UR");
                            if (ur_gi) p.def = std::max(0, p.def - ur_gi->stat_val);
                        }
                        dg.players.push_back(p);
                    }
                    // 找到第一個存活玩家
                    dg.current_player = -1;
                    for (int i = 0; i < (int)dg.players.size(); i++)
                        if (dg.players[i].alive) { dg.current_player = i; break; }
                    dg.practice_mode = room.practice_mode;
                    // 練習模式不消耗每週卷
                    if (!room.practice_mode) {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        for (auto& muid : room.member_uids) {
                            if (inventory_data.count(muid) && inventory_data[muid].count("weekly_hunt_scroll"))
                                inventory_data[muid]["weekly_hunt_scroll"]--;
                        }
                        save_inventory();
                    }
                    // 儲存遊戲
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        dd_games[ch] = dg;
                    }
                    // 發送初始戰鬥訊息
                    auto ddmsg = make_dd_combat_msg(dg);
                    ddmsg.channel_id = ch;
                    bot.message_create(ddmsg, [ch](const dpp::confirmation_callback_t& cb) {
                        if (cb.is_error()) return;
                        auto mid = std::get<dpp::message>(cb.value).id;
                        std::lock_guard<std::mutex> lk(data_mutex);
                        if (dd_games.count(ch)) dd_games[ch].msg_id = mid;
                    });
                    // 更新房間訊息
                    ev.reply(dpp::ir_update_message, dpp::message("⚔️ **暗黑龍王**挑戰已開始！"));
                    // 設定 30 分鐘 timer
                    bot.start_timer([&bot, ch](dpp::timer t) {
                        std::vector<dpp::snowflake> player_uids;
                        bool is_practice = false;
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            auto it = dd_games.find(ch);
                            if (it == dd_games.end()) return;
                            auto& dg2 = it->second;
                            if (dg2.game_over) return;
                            dg2.game_over = true;
                            is_practice = dg2.practice_mode;
                            for (auto& p : dg2.players) player_uids.push_back(p.uid);
                            dd_games.erase(it);
                        }
                        if (!is_practice) {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            for (auto puid : player_uids) {
                                auto& pet2 = pet_data[puid];
                                bool already = false;
                                for (auto& s : pet2.statuses) if (s == "受傷") { already = true; break; }
                                if (!already) pet2.statuses.push_back("受傷");
                            }
                            save_pet_data();
                        }
                        bot.stop_timer(t);
                        dpp::message tm; tm.channel_id = ch;
                        tm.set_content("⏱️ **暗黑龍王挑戰** 已超時，遠征失敗！");
                        bot.message_create(tm);
                    }, 1800);
                    return;
                }

                // Build RaidGame
                const RaidBoss* boss = find_raid_boss(room.boss_key);
                RaidGame g;
                g.channel_id  = ch;
                g.boss_key    = boss->key;
                g.boss_name   = boss->name;
                g.boss_image  = boss->image;
                g.boss_hp     = boss->hp;
                g.boss_max_hp = boss->hp;
                g.boss_atk    = boss->atk;
                g.boss_def    = boss->def;
                g.started_at  = time(nullptr);
                g.round       = 1;
                g.boss_turn   = false;
                g.current_player = 0;

                // Build players
                for (auto& muid : room.member_uids) {
                    Pet pet2; PetStats ps;
                    std::string orb_key;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto pit = pet_data.find(muid);
                        if (pit != pet_data.end()) pet2 = pit->second;
                        auto eit = equipped_data.find(muid);
                        if (eit != equipped_data.end()) orb_key = eit->second.orb;
                    }
                    ps = calc_pet_stats(muid, pet2);
                    RaidPlayer p;
                    p.uid          = muid;
                    p.display_name = room.member_names.count(muid) ? room.member_names.at(muid) : "?";
                    p.avatar_url   = room.member_avatars.count(muid) ? room.member_avatars.at(muid) : "";
                    p.hp           = ps.hp;
                    p.max_hp       = ps.hp;
                    p.atk          = ps.atk;
                    p.def          = ps.def;
                    p.orb_key      = orb_key;
                    p.alive        = true;
                    // 組隊模式：UR 寶珠的個人 +5 DEF 改為全體 +2 DEF，移除個人加成
                    if (orb_key == "EQ_K_UR" && room.member_uids.size() > 1) {
                        auto* ur_gi = find_gacha_item("EQ_K_UR");
                        if (ur_gi) p.def = std::max(0, p.def - ur_gi->stat_val);
                    }
                    g.players.push_back(p);
                }

                // Check and skip stunned players at start (shouldn't happen turn 0 but safety)
                // Find first alive player
                g.current_player = -1;
                for (int i = 0; i < (int)g.players.size(); i++)
                    if (g.players[i].alive) { g.current_player = i; break; }
                g.practice_mode = room.practice_mode;

                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    raid_games[ch] = g;
                }

                dpp::message gmsg = make_raid_combat_msg(g);
                ev.reply(dpp::ir_update_message, gmsg,
                    [&bot, ch](const dpp::confirmation_callback_t& cb){
                        if (cb.is_error()) return;
                        // set msg_id via followup
                    });
                // Store msg_id from the updated room message
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (raid_games.count(ch))
                        raid_games[ch].msg_id = ev.command.message_id;
                }

                // 20-minute battle timeout
                bot.start_timer([&bot, ch](dpp::timer t){
                    dpp::snowflake mid = 0;
                    std::string boss_name;
                    std::vector<dpp::snowflake> player_uids;
                    bool is_practice_latus = false;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = raid_games.find(ch);
                        if (it == raid_games.end()) return;
                        if (it->second.game_over) return;
                        it->second.game_over = true;
                        it->second.victory   = false;
                        mid = it->second.msg_id;
                        boss_name = it->second.boss_name;
                        is_practice_latus = it->second.practice_mode;
                        for (auto& p : it->second.players) player_uids.push_back(p.uid);
                        raid_games.erase(it);
                    }
                    // 逾時失敗：非練習模式才受傷
                    if (!is_practice_latus) {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        for (auto puid : player_uids) {
                            auto& pet = pet_data[puid];
                            bool already = false;
                            for (auto& s : pet.statuses) if (s == "受傷") { already = true; break; }
                            if (!already) pet.statuses.push_back("受傷");
                        }
                        save_pet_data();
                    }
                    if (mid) {
                        dpp::embed e;
                        e.set_title("⌛ 討伐失敗").set_color(0x808080)
                         .set_description("戰鬥逾時（20 分鐘），討伐失敗，無獎勵。");
                        dpp::message edit_m; edit_m.id = mid; edit_m.channel_id = ch; edit_m.add_embed(e);
                        bot.message_edit(edit_m);
                        bot.message_create(dpp::message(ch,
                            "⌛ **討伐逾時！** 20 分鐘內未能擊敗 **" + boss_name + "**，討伐失敗，本次無獎勵。"));
                    }
                }, 1200);
                return;
            }

            // ── Raid combat buttons ────────────────────────────────────────────────
            // Helper: parse raid button id prefix_{ch}_{acting_uid}
            auto parse_raid_btn = [&](const std::string& prefix, size_t plen,
                                      dpp::snowflake& ch_out, dpp::snowflake& actor_out) -> bool {
                if (cid.rfind(prefix, 0) != 0) return false;
                std::string rest = cid.substr(plen);
                size_t s = rest.find('_');
                if (s == std::string::npos) return false;
                ch_out    = dpp::snowflake(std::stoull(rest.substr(0, s)));
                actor_out = dpp::snowflake(std::stoull(rest.substr(s+1)));
                return true;
            };

            // attack_type: 0=普通, 1=耗費氣力, 2=強攻
            auto do_raid_attack = [&](int attack_type, bool is_block, bool is_cry) {
                dpp::snowflake ch, actor;
                std::string pfx;
                size_t plen;
                if      (is_block)       { pfx = "raid_block_";  plen = 11; }
                else if (is_cry)         { pfx = "raid_cry_";    plen = 9;  }
                else if (attack_type==2) { pfx = "raid_pow_";    plen = 9;  }
                else if (attack_type==1) { pfx = "raid_gamble_"; plen = 12; }
                else                     { pfx = "raid_atk_";    plen = 9;  }
                if (!parse_raid_btn(pfx, plen, ch, actor)) return;
                if (uid != actor) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 還沒輪到你！").set_flags(dpp::m_ephemeral));
                    return;
                }
                std::string log;
                bool game_over = false;
                RaidGame g_snap;
                dpp::message combat_msg_out;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = raid_games.find(ch);
                    if (it == raid_games.end()) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 找不到戰鬥！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    auto& g = it->second;
                    if (g.game_over) { ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 戰鬥已結束！").set_flags(dpp::m_ephemeral)); return; }
                    if (g.boss_turn) { ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ Boss 正在行動！").set_flags(dpp::m_ephemeral)); return; }
                    if (g.current_player < 0 || g.current_player >= (int)g.players.size()) return;
                    auto& cp = g.players[g.current_player];
                    if (cp.uid != uid) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 還沒輪到你！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    if (is_cry) {
                        g.cry_pending_uid = uid;
                        g.log_line = "📣 **" + cp.display_name + "** 使用戰吼！請選擇增益對象：";
                        ev.reply(dpp::ir_update_message, make_raid_combat_msg(g));
                        return;
                    }
                    if (is_block) {
                        g.block_active = true;
                        log = "🛡️ **" + cp.display_name + "** 舉起盾牌！所有隊員受到的傷害減半（本輪）";
                    } else {
                        std::string heal_log = raid_athena_heal(g);
                        if (!heal_log.empty()) log = heal_log + "\n";
                        log += raid_do_player_attack(g, attack_type);
                    }
                    g.log_line = log;
                    if (!g.game_over) {
                        raid_finish_turn(g);
                    }
                    game_over = g.game_over;
                    if (game_over) {
                        g_snap = g;          // snapshot before erase
                        raid_games.erase(it);
                    } else {
                        combat_msg_out = make_raid_combat_msg(g);
                    }
                }
                // ── 鎖已釋放，在鎖外處理獎勵（避免 deadlock）──────────────────
                if (game_over) {
                    std::vector<std::pair<std::string,std::string>> reward_lines;
                    if (g_snap.victory) {
                        if (!g_snap.practice_mode) {
                            for (auto& p : g_snap.players) {
                                std::string drops = raid_give_rewards(p.uid, p.display_name);
                                reward_lines.push_back({p.display_name, drops});
                            }
                            // 勝利才消耗每週卷
                            {
                                std::lock_guard<std::mutex> lk2(data_mutex);
                                for (auto& p : g_snap.players)
                                    if (inventory_data[p.uid]["weekly_hunt_scroll"] > 0)
                                        inventory_data[p.uid]["weekly_hunt_scroll"]--;
                            }
                            save_chips(); save_inventory();
                        }
                    } else {
                        // 失敗：非練習模式才受傷
                        if (!g_snap.practice_mode) {
                            std::lock_guard<std::mutex> lk2(data_mutex);
                            for (auto& p : g_snap.players) {
                                auto& pet = pet_data[p.uid];
                                bool already = false;
                                for (auto& s : pet.statuses) if (s == "受傷") { already = true; break; }
                                if (!already) pet.statuses.push_back("受傷");
                            }
                            save_pet_data();
                        }
                    }
                    auto end_msg = make_raid_end_msg(g_snap, reward_lines);
                    end_msg.id         = g_snap.msg_id;
                    end_msg.channel_id = ch;
                    ev.reply(dpp::ir_update_message, end_msg);
                    return;
                }
                ev.reply(dpp::ir_update_message, combat_msg_out);
            };

            if (cid.rfind("raid_atk_", 0) == 0)    { do_raid_attack(0,false,false); return; }
            if (cid.rfind("raid_gamble_", 0) == 0) { do_raid_attack(1,false,false); return; }
            if (cid.rfind("raid_pow_", 0) == 0)    { do_raid_attack(2,false,false); return; }
            if (cid.rfind("raid_block_", 0) == 0)  { do_raid_attack(0,true, false); return; }
            if (cid.rfind("raid_cry_", 0) == 0)    { do_raid_attack(0,false,true);  return; }

            // ── Raid: 刷新狀態（重新發送新訊息）─────────────────────────────────
            if (cid.rfind("raid_refresh_", 0) == 0) {
                dpp::snowflake game_ch(std::stoull(cid.substr(13)));
                dpp::message new_msg;
                dpp::snowflake old_mid = 0;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = raid_games.find(game_ch);
                    if (it == raid_games.end()) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 戰鬥已結束！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    new_msg = make_raid_combat_msg(it->second);
                    old_mid = it->second.msg_id;
                }
                new_msg.channel_id = game_ch;
                ev.reply(dpp::ir_channel_message_with_source, new_msg,
                    [&bot, game_ch, old_mid](const dpp::confirmation_callback_t& cb) {
                        if (!cb.is_error()) {
                            dpp::snowflake new_mid = std::get<dpp::message>(cb.value).id;
                            std::lock_guard<std::mutex> lk(data_mutex);
                            auto it = raid_games.find(game_ch);
                            if (it != raid_games.end()) it->second.msg_id = new_mid;
                        }
                        // 把舊訊息的按鈕清掉
                        if (old_mid) {
                            dpp::message old_edit; old_edit.id = old_mid; old_edit.channel_id = game_ch;
                            old_edit.set_content("↩️ 已刷新，請看新訊息。");
                            bot.message_edit(old_edit);
                        }
                    });
                return;
            }

            // Battlecry target: raid_cryt_{ch}_{src_uid}_{target_idx}
            if (cid.rfind("raid_cryt_", 0) == 0) {
                std::string rest = cid.substr(10);
                size_t s1 = rest.find('_');
                if (s1 == std::string::npos) return;
                dpp::snowflake ch(std::stoull(rest.substr(0, s1)));
                std::string rest2 = rest.substr(s1+1);
                size_t s2 = rest2.find('_');
                if (s2 == std::string::npos) return;
                dpp::snowflake src_uid(std::stoull(rest2.substr(0, s2)));
                int tidx = std::stoi(rest2.substr(s2+1));
                if (uid != src_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的戰吼！").set_flags(dpp::m_ephemeral));
                    return;
                }
                bool cryt_over = false;
                RaidGame cryt_snap;
                dpp::message cryt_combat;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = raid_games.find(ch);
                    if (it == raid_games.end()) return;
                    auto& g = it->second;
                    if (tidx < 0 || tidx >= (int)g.players.size()) return;
                    g.players[tidx].battlecry_next = true;
                    g.cry_pending_uid = 0;
                    g.log_line = "📣 **" + g.players[g.current_player].display_name +
                                 "** 為 **" + g.players[tidx].display_name + "** 施加戰吼！下次攻擊 +25%";
                    raid_finish_turn(g);
                    cryt_over = g.game_over;
                    if (cryt_over) {
                        cryt_snap = g;
                        raid_games.erase(it);
                    } else {
                        cryt_combat = make_raid_combat_msg(g);
                    }
                }
                if (cryt_over) {
                    std::vector<std::pair<std::string,std::string>> reward_lines;
                    if (cryt_snap.victory && !cryt_snap.practice_mode) {
                        for (auto& p : cryt_snap.players) {
                            std::string drops = raid_give_rewards(p.uid, p.display_name);
                            reward_lines.push_back({p.display_name, drops});
                        }
                        {
                            std::lock_guard<std::mutex> lk2(data_mutex);
                            for (auto& p : cryt_snap.players)
                                if (inventory_data[p.uid]["weekly_hunt_scroll"] > 0)
                                    inventory_data[p.uid]["weekly_hunt_scroll"]--;
                        }
                        save_chips(); save_inventory();
                    }
                    ev.reply(dpp::ir_update_message, make_raid_end_msg(cryt_snap, reward_lines));
                    return;
                }
                ev.reply(dpp::ir_update_message, cryt_combat);
                return;
            }

            // hunt_team_{uid}: show boss selection screen
            if (cid.rfind("hunt_team_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(10)));
                if (uid != bu) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral));
                    return;
                }
                ev.reply(dpp::ir_update_message, make_raid_boss_select_msg(uid, dn, av));
                return;
            }

            // hunt_boss_latus_p_{uid}: open PRACTICE raid room for Rathalos
            if (cid.rfind("hunt_boss_latus_p_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(18)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                dpp::snowflake ch = ev.command.channel_id;
                RaidRoom new_room;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  if (raid_games.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有進行中的組隊戰鬥！").set_flags(dpp::m_ephemeral)); return; }
                  if (raid_rooms.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有等待中的組隊房間！").set_flags(dpp::m_ephemeral)); return; }
                  new_room.channel_id    = ch;
                  new_room.host_uid      = uid;
                  new_room.boss_key      = "latus";
                  new_room.practice_mode = true;
                  new_room.created_at    = time(nullptr);
                  new_room.member_uids.push_back(uid);
                  new_room.member_names[uid]   = dn;
                  new_room.member_avatars[uid] = av;
                  raid_rooms[ch] = new_room;
                }
                dpp::message rmsg = make_raid_room_msg(new_room);
                rmsg.channel_id = ch;
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ **拉圖斯**練習房間已開啟！（無消耗 · 無獎勵 · 失敗不受傷）").set_flags(dpp::m_ephemeral));
                bot.message_create(rmsg, [ch](const dpp::confirmation_callback_t& cb){
                    if (cb.is_error()) return;
                    auto mid = std::get<dpp::message>(cb.value).id;
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (raid_rooms.count(ch)) raid_rooms[ch].msg_id = mid;
                });
                bot.start_timer([&bot, ch](dpp::timer t){ std::lock_guard<std::mutex> lk(data_mutex); auto it = raid_rooms.find(ch); if (it == raid_rooms.end()) return; auto mid = it->second.msg_id; raid_rooms.erase(it); if (mid) { dpp::message m; m.id = mid; m.channel_id = ch; m.set_content("⌛ 組隊房間因逾時（10 分鐘）自動解散。"); bot.message_edit(m); } }, 600);
                return;
            }

            // hunt_boss_dark_p_{uid}: open PRACTICE raid room for 暗黑龍王
            if (cid.rfind("hunt_boss_dark_p_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(17)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                dpp::snowflake ch = ev.command.channel_id;
                RaidRoom new_room;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  if (raid_games.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有進行中的組隊戰鬥！").set_flags(dpp::m_ephemeral)); return; }
                  if (raid_rooms.count(ch)) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 此頻道已有等待中的組隊房間！").set_flags(dpp::m_ephemeral)); return; }
                  new_room.channel_id    = ch;
                  new_room.host_uid      = uid;
                  new_room.boss_key      = "dark_dragon";
                  new_room.practice_mode = true;
                  new_room.created_at    = time(nullptr);
                  new_room.member_uids.push_back(uid);
                  new_room.member_names[uid]   = dn;
                  new_room.member_avatars[uid] = av;
                  raid_rooms[ch] = new_room;
                }
                dpp::message rmsg = make_raid_room_msg(new_room);
                rmsg.channel_id = ch;
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ **暗黑龍王**練習房間已開啟！（無消耗 · 無獎勵 · 失敗不受傷）").set_flags(dpp::m_ephemeral));
                bot.message_create(rmsg, [ch](const dpp::confirmation_callback_t& cb){
                    if (cb.is_error()) return;
                    auto mid = std::get<dpp::message>(cb.value).id;
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (raid_rooms.count(ch)) raid_rooms[ch].msg_id = mid;
                });
                bot.start_timer([&bot, ch](dpp::timer t){ std::lock_guard<std::mutex> lk(data_mutex); auto it = raid_rooms.find(ch); if (it == raid_rooms.end()) return; auto mid = it->second.msg_id; raid_rooms.erase(it); if (mid) { dpp::message m; m.id = mid; m.channel_id = ch; m.set_content("⌛ 組隊房間因逾時（10 分鐘）自動解散。"); bot.message_edit(m); } }, 600);
                return;
            }

            // hunt_boss_latus_{uid}: open raid room for Rathalos
            if (cid.rfind("hunt_boss_latus_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(16)));
                if (uid != bu) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral));
                    return;
                }
                dpp::snowflake ch = ev.command.channel_id;
                RaidRoom new_room;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (raid_games.count(ch)) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 此頻道已有進行中的組隊戰鬥！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    if (raid_rooms.count(ch)) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 此頻道已有等待中的組隊房間！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    new_room.channel_id = ch;
                    new_room.host_uid   = uid;
                    new_room.boss_key   = "latus";
                    new_room.created_at = time(nullptr);
                    new_room.member_uids.push_back(uid);
                    new_room.member_names[uid]   = dn;
                    new_room.member_avatars[uid] = av;
                    raid_rooms[ch] = new_room;
                }
                dpp::message rmsg = make_raid_room_msg(new_room);
                rmsg.channel_id = ch;
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("✅ **拉圖斯**組隊房間已開啟！").set_flags(dpp::m_ephemeral));
                bot.message_create(rmsg, [ch](const dpp::confirmation_callback_t& cb){
                    if (cb.is_error()) return;
                    auto mid = std::get<dpp::message>(cb.value).id;
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (raid_rooms.count(ch)) raid_rooms[ch].msg_id = mid;
                });
                // 10-minute auto-dissolve
                bot.start_timer([&bot, ch](dpp::timer t){
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = raid_rooms.find(ch);
                    if (it == raid_rooms.end()) return;
                    auto mid = it->second.msg_id;
                    raid_rooms.erase(it);
                    if (mid) {
                        dpp::message m; m.id = mid; m.channel_id = ch;
                        m.set_content("⌛ 組隊房間因逾時（10 分鐘）自動解散。");
                        bot.message_edit(m);
                    }
                }, 600);
                return;
            }

            // hunt_boss_dark_{uid}: open raid room for 暗黑龍王
            if (cid.rfind("hunt_boss_dark_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(15)));
                if (uid != bu) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral));
                    return;
                }
                dpp::snowflake ch = ev.command.channel_id;
                RaidRoom new_room;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (raid_games.count(ch)) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 此頻道已有進行中的組隊戰鬥！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    if (raid_rooms.count(ch)) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message("❌ 此頻道已有等待中的組隊房間！").set_flags(dpp::m_ephemeral));
                        return;
                    }
                    new_room.channel_id = ch;
                    new_room.host_uid   = uid;
                    new_room.boss_key   = "dark_dragon";
                    new_room.created_at = time(nullptr);
                    new_room.member_uids.push_back(uid);
                    new_room.member_names[uid]   = dn;
                    new_room.member_avatars[uid] = av;
                    raid_rooms[ch] = new_room;
                }
                dpp::message rmsg = make_raid_room_msg(new_room);
                rmsg.channel_id = ch;
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("✅ **暗黑龍王**組隊房間已開啟！").set_flags(dpp::m_ephemeral));
                bot.message_create(rmsg, [ch](const dpp::confirmation_callback_t& cb){
                    if (cb.is_error()) return;
                    auto mid = std::get<dpp::message>(cb.value).id;
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (raid_rooms.count(ch)) raid_rooms[ch].msg_id = mid;
                });
                bot.start_timer([&bot, ch](dpp::timer t){
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = raid_rooms.find(ch);
                    if (it == raid_rooms.end()) return;
                    auto mid = it->second.msg_id;
                    raid_rooms.erase(it);
                    if (mid) {
                        dpp::message m; m.id = mid; m.channel_id = ch;
                        m.set_content("⌛ 組隊房間因逾時（10 分鐘）自動解散。");
                        bot.message_edit(m);
                    }
                }, 600);
                return;
            }

            // ── hunt_village_{uid}: 選擇村落 ─────────────────────────────────────
            if (cid.rfind("hunt_village_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(13)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                { std::lock_guard<std::mutex> lk(data_mutex);
                  if (village_games.count(uid)) {
                    ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你已有進行中的村落挑戰！").set_flags(dpp::m_ephemeral));
                    return;
                  }
                  bool has_scroll = inventory_data.count(uid) && inventory_data[uid]["hunt_scroll"] > 0;
                  if (!has_scroll) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有怪物狩獵卷！").set_flags(dpp::m_ephemeral)); return; }
                }
                ev.reply(dpp::ir_update_message, make_village_select_msg(uid, dn, av));
                return;
            }

            // ── village_start_{uid}_{group_key}: 確認選擇並開始 ──────────────────
            if (cid.rfind("village_start_", 0) == 0) {
                std::string rest = cid.substr(14);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                std::string group_key = rest.substr(s1 + 1);
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                const VillageGroupDef* gdp = find_village_group(group_key);
                if (!gdp) return;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  if (village_games.count(uid)) {
                    ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你已有進行中的村落挑戰！").set_flags(dpp::m_ephemeral));
                    return;
                  }
                }
                bool has_scroll = false;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = inventory_data.find(uid);
                  if (it != inventory_data.end() && it->second["hunt_scroll"] > 0) { it->second["hunt_scroll"]--; has_scroll = true; }
                }
                if (!has_scroll) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有怪物狩獵卷！").set_flags(dpp::m_ephemeral)); return; }
                save_inventory();
                Pet pet2; { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) pet2 = it->second; }
                PetStats ps = calc_pet_stats(uid, pet2);
                VillageGame vg;
                vg.uid = uid; vg.channel_id = ev.command.channel_id;
                vg.group_key = gdp->key;
                vg.spirits   = build_village_spirits(*gdp);
                vg.pet_hp = ps.hp; vg.pet_max_hp = ps.hp;
                vg.pet_atk = ps.atk; vg.pet_def = ps.def;
                vg.started_at = time(nullptr);
                { std::lock_guard<std::mutex> lk(data_mutex);
                  vg.orb_key = equipped_data.count(uid) ? equipped_data[uid].orb : "";
                  if (col_set_mushroom_basic(uid)) vg.pet_atk = (int)std::ceil(vg.pet_atk * 1.01);
                  if (col_set_water_basic(uid))  { vg.pet_hp = (int)std::ceil(vg.pet_hp * 1.01); vg.pet_max_hp = (int)std::ceil(vg.pet_max_hp * 1.01); }
                  if (col_set_ghost_basic(uid))    vg.pet_def = (int)std::ceil(vg.pet_def * 1.02);
                  vg.pet_atk += col_pet_atk_bonus(uid);
                  vg.pet_def += col_pet_def_bonus(uid);
                  int hp_bonus = col_pet_hp_bonus(uid);
                  vg.pet_hp += hp_bonus; vg.pet_max_hp += hp_bonus;
                }
                vg.msg_id = ev.command.message_id;
                dpp::timer vtid = g_bot->start_timer([uid](dpp::timer) {
                    VillageGame tg;
                    { std::lock_guard<std::mutex> lk(data_mutex);
                      auto it = village_games.find(uid);
                      if (it == village_games.end()) return;
                      tg = it->second; village_games.erase(it);
                    }
                    { std::lock_guard<std::mutex> lk(data_mutex);
                      auto& p = pet_data[uid]; bool already = false;
                      for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                      if (!already) p.statuses.push_back("受傷");
                    }
                    save_pet_data();
                    if ((uint64_t)tg.msg_id != 0) {
                        dpp::message tm = make_village_timeout_msg(tg, "", "");
                        tm.id = tg.msg_id; tm.channel_id = tg.channel_id;
                        g_bot->message_edit(tm);
                    }
                }, 600);
                vg.timer_id = vtid;
                { std::lock_guard<std::mutex> lk(data_mutex); village_games[uid] = vg; }
                ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
                return;
            }

            // ── village_atk_{uid}_{idx}: 選擇目標 ────────────────────────────────
            if (cid.rfind("village_atk_", 0) == 0) {
                std::string rest = cid.substr(12);
                size_t s1 = rest.rfind('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                int tidx = std::stoi(rest.substr(s1+1));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = village_games.find(uid);
                  if (it == village_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
                  if (tidx < 0 || tidx >= (int)it->second.spirits.size() || it->second.spirits[tidx].hp <= 0) {
                      ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 無效目標！").set_flags(dpp::m_ephemeral)); return;
                  }
                  it->second.selected_target = tidx;
                }
                VillageGame vg;
                { std::lock_guard<std::mutex> lk(data_mutex); vg = village_games[uid]; }
                ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
                return;
            }

            // ── village_back_{uid}: 取消目標選擇，返回目標列表 ───────────────────
            if (cid.rfind("village_back_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(13)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = village_games.find(uid);
                  if (it == village_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
                  it->second.selected_target = -1;
                }
                VillageGame vg;
                { std::lock_guard<std::mutex> lk(data_mutex); vg = village_games[uid]; }
                ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
                return;
            }

            // ── village_exec_{uid}_{n|p}: 執行攻擊 ───────────────────────────────
            if (cid.rfind("village_exec_", 0) == 0) {
                std::string rest = cid.substr(13);
                size_t s1 = rest.rfind('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                std::string atype = rest.substr(s1+1);
                int attack_type = (atype == "p") ? 1 : 0;
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                VillageGame vg;
                bool found = false;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = village_games.find(uid);
                  if (it != village_games.end()) { vg = it->second; found = true; }
                }
                if (!found) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
                int tidx = vg.selected_target;
                if (tidx < 0 || tidx >= (int)vg.spirits.size() || vg.spirits[tidx].hp <= 0) {
                    ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 請先選擇目標！").set_flags(dpp::m_ephemeral)); return;
                }
                vg.selected_target = -1;
                bool vwin = false; int64_t vreward = 0; int vkilled_unused = 0;
                HuntDropList vdrops;
                bool vended = process_village_combat(vg, tidx, attack_type, vwin, vreward, vkilled_unused, vdrops);
                if (vended) {
                    // Count total dead spirits for end message display
                    int vkilled = 0;
                    for (auto& s : vg.spirits) if (s.hp <= 0) vkilled++;
                    dpp::timer vt = 0;
                    { std::lock_guard<std::mutex> lk(data_mutex); vt = vg.timer_id; village_games.erase(uid); }
                    if (vt) g_bot->stop_timer(vt);
                    bool vfirst = false;
                    if (vwin) {
                        { std::lock_guard<std::mutex> lk(data_mutex);
                          vfirst = hunt_clear_data[uid].count(vg.group_key) == 0;
                          hunt_clear_data[uid].insert(vg.group_key);
                        }
                        if (vfirst) { auto* gd2 = find_village_group(vg.group_key); if (gd2) vreward += gd2->first_clear_reward; }
                        add_chips(uid, vreward);
                        { std::lock_guard<std::mutex> lk(data_mutex);
                          for (auto& [k, c] : vdrops) inventory_data[uid][k] += c;
                        }
                        save_chips(); save_hunt_clear();
                        if (!vdrops.empty()) save_inventory();
                    } else {
                        { std::lock_guard<std::mutex> lk(data_mutex);
                          auto& p = pet_data[uid]; bool already = false;
                          for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                          if (!already) p.statuses.push_back("受傷");
                        }
                        save_pet_data();
                    }
                    ev.reply(dpp::ir_update_message, make_village_end_msg(vwin, vg, vreward, vfirst, vdrops, dn, av, vkilled));
                } else {
                    { std::lock_guard<std::mutex> lk(data_mutex); village_games[uid] = vg; }
                    ev.reply(dpp::ir_update_message, make_village_combat_msg(vg, dn, av));
                }
                return;
            }

            // ── village_refresh_{uid}: 刷新村落戰鬥訊息 ─────────────────────────
            if (cid.rfind("village_refresh_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(16)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                VillageGame vg;
                bool found = false;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = village_games.find(uid);
                  if (it != village_games.end()) { vg = it->second; found = true; }
                }
                if (!found) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的村落挑戰！").set_flags(dpp::m_ephemeral)); return; }
                dpp::embed se; se.set_title("🔄 訊息已刷新").set_color(0x95A5A6).set_description("請往下滑查看最新戰況！");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(se));
                auto fresh = make_village_combat_msg(vg, dn, av);
                fresh.channel_id = vg.channel_id;
                g_bot->message_create(fresh, [uid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        auto nid = std::get<dpp::message>(cb.value).id;
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = village_games.find(uid);
                        if (it != village_games.end()) it->second.msg_id = nid;
                    }
                });
                return;
            }

            if (cid.rfind("hunt_main_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(10)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                Pet pet2; { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) pet2 = it->second; }
                ev.reply(dpp::ir_update_message, make_hunt_main_msg(uid, pet2, dn, av));
            } else if (cid.rfind("hunt_diff_", 0) == 0) {
                // hunt_diff_UID_DIFF
                std::string rest = cid.substr(10);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                std::string diff = rest.substr(s1+1);
                ev.reply(dpp::ir_update_message, make_hunt_diff_msg(uid, diff, dn, av));
            } else if (cid.rfind("hunt_monster_", 0) == 0) {
                // hunt_monster_UID_MKEY
                std::string rest = cid.substr(13);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }
                std::string mkey = rest.substr(s1+1);
                auto* md = find_monster(mkey);
                if (!md) return;

                // Check: not already in a hunt
                { std::lock_guard<std::mutex> lk(data_mutex);
                  if (monster_hunt_games.count(uid)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你已有進行中的狩獵！請先完成或等待逾時。").set_flags(dpp::m_ephemeral));
                    return;
                  }
                }

                // Consume a scroll
                bool has_scroll = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = inventory_data.find(uid);
                    if (it != inventory_data.end() && it->second["hunt_scroll"] > 0) {
                        it->second["hunt_scroll"]--;
                        has_scroll = true;
                    }
                }
                if (!has_scroll) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 沒有怪物狩獵卷！").set_flags(dpp::m_ephemeral));
                    return;
                }
                save_inventory();

                // Build game state
                Pet pet2; { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) pet2 = it->second; }
                PetStats ps = calc_pet_stats(uid, pet2);

                MonsterHuntGame g;
                g.uid           = uid;
                g.channel_id    = ev.command.channel_id;
                g.difficulty    = md->difficulty;
                g.monster_key   = mkey;
                g.monster_name  = md->name;
                g.monster_hp    = md->hp;
                g.monster_max_hp = md->hp;
                g.monster_atk   = md->atk;
                g.monster_def   = md->def;
                g.pet_hp        = ps.hp;
                g.pet_max_hp    = ps.hp;
                g.pet_atk       = ps.atk;
                g.pet_def       = ps.def;
                g.started_at    = time(nullptr);
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    g.orb_key = equipped_data.count(uid) ? equipped_data[uid].orb : "";
                    if (col_set_mushroom_basic(uid)) g.pet_atk = (int)std::ceil(g.pet_atk * 1.01);
                    if (col_set_water_basic(uid))  { g.pet_hp = (int)std::ceil(g.pet_hp * 1.01); g.pet_max_hp = (int)std::ceil(g.pet_max_hp * 1.01); }
                    if (col_set_ghost_basic(uid))    g.pet_def = (int)std::ceil(g.pet_def * 1.02);
                    g.pet_atk += col_pet_atk_bonus(uid);
                    g.pet_def += col_pet_def_bonus(uid);
                    int hp_bonus = col_pet_hp_bonus(uid);
                    g.pet_hp += hp_bonus; g.pet_max_hp += hp_bonus;
                }
                g.player_first = (g.orb_key == "EQ_K_SPEED") ||
                                 std::uniform_int_distribution<int>(0,3)(hunt_rng()) < 3; // 75%, orb=100%

                // Check if first clear
                bool is_first_clear = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    is_first_clear = hunt_clear_data[uid].count(mkey) == 0;
                }

                // Build initial combat message
                if (!g.player_first) {
                    // Monster attacks first
                    int mon_dmg = std::max(0, g.monster_atk - g.pet_def);
                    g.pet_hp -= mon_dmg;
                    g.log_line = "👹 怪物先手！**" + g.monster_name + "** 造成 **" + std::to_string(mon_dmg) + "** 傷害！";
                    if (g.pet_hp <= 0) {
                        // instant kill (very rare)
                        g.pet_hp = 0;
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            auto& p = pet_data[uid];
                            bool already = false;
                            for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                            if (!already) p.statuses.push_back("受傷");
                        }
                        save_pet_data();
                        ev.reply(dpp::ir_update_message,
                            make_combat_end_msg(false, g, 0, false, {}, dn, av));
                        return;
                    }
                }

                // ir_update_message edits the triggering message in place — its ID is already known
                g.msg_id = ev.command.message_id;
                auto combat_msg = make_combat_msg(g, dn, av);
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    monster_hunt_games[uid] = g;
                }

                ev.reply(dpp::ir_update_message, combat_msg);

                // Start 10-minute timeout timer
                dpp::timer tid = g_bot->start_timer([uid, dn, av](dpp::timer t) {
                    g_bot->stop_timer(t);
                    MonsterHuntGame tg;
                    bool found2 = false;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = monster_hunt_games.find(uid);
                        if (it != monster_hunt_games.end()) { tg = it->second; found2 = true; monster_hunt_games.erase(it); }
                    }
                    if (!found2) return;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto& p = pet_data[uid];
                        bool already2 = false;
                        for (auto& s : p.statuses) if (s == "受傷") { already2 = true; break; }
                        if (!already2) p.statuses.push_back("受傷");
                    }
                    save_pet_data();
                    if (tg.msg_id && tg.channel_id) {
                        auto tmsg = make_combat_timeout_msg(tg, dn, av);
                        tmsg.id = tg.msg_id;
                        tmsg.channel_id = tg.channel_id;
                        g_bot->message_edit(tmsg);
                    }
                }, 600);
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (monster_hunt_games.count(uid)) monster_hunt_games[uid].timer_id = tid;
                }
            } else if (cid.rfind("hunt_atk_", 0) == 0 || cid.rfind("hunt_pow_", 0) == 0) {
                bool power = cid.rfind("hunt_pow_", 0) == 0;
                dpp::snowflake bu(std::stoull(cid.substr(power ? 9 : 9)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }

                MonsterHuntGame g;
                bool found = false;
                std::string pet_muscle_tense = "";
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = monster_hunt_games.find(uid);
                    if (it != monster_hunt_games.end()) { g = it->second; found = true; }
                    auto pit = pet_data.find(uid);
                    if (pit != pet_data.end())
                        for (auto& s : pit->second.statuses) if (s == "肌肉緊繃") { pet_muscle_tense = s; break; }
                }
                if (!found) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 沒有進行中的狩獵！").set_flags(dpp::m_ephemeral));
                    return;
                }

                bool win = false; int64_t reward = 0; HuntDropList hunt_drops;
                bool ended = process_combat(g, power, !pet_muscle_tense.empty(), win, reward, hunt_drops);

                if (ended) {
                    // Stop timer
                    dpp::timer tid = 0;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        tid = g.timer_id;
                        monster_hunt_games.erase(uid);
                    }
                    if (tid) g_bot->stop_timer(tid);

                    if (win) {
                        // First clear check
                        bool first_clear = false;
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            first_clear = hunt_clear_data[uid].count(g.monster_key) == 0;
                            hunt_clear_data[uid].insert(g.monster_key);
                        }
                        if (first_clear) {
                            auto* md = find_monster(g.monster_key);
                            if (md) reward += md->first_clear_reward;
                        }
                        add_chips(uid, reward);
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            for (auto& [k, c] : hunt_drops) inventory_data[uid][k] += c;
                        }
                        save_chips(); save_hunt_clear();
                        if (!hunt_drops.empty()) save_inventory();
                        ev.reply(dpp::ir_update_message,
                            make_combat_end_msg(true, g, reward, first_clear, hunt_drops, dn, av));
                    } else {
                        // Loss: give 受傷 status
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            auto& p = pet_data[uid];
                            bool already = false;
                            for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                            if (!already) p.statuses.push_back("受傷");
                        }
                        save_pet_data();
                        ev.reply(dpp::ir_update_message,
                            make_combat_end_msg(false, g, 0, false, {}, dn, av));
                    }
                } else {
                    // Update game state
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        monster_hunt_games[uid] = g;
                    }
                    ev.reply(dpp::ir_update_message, make_combat_msg(g, dn, av));
                }
            } else if (cid.rfind("hunt_block_", 0) == 0 || cid.rfind("hunt_cry_", 0) == 0) {
                bool is_block    = cid.rfind("hunt_block_", 0) == 0;
                bool is_battlecry = !is_block;
                dpp::snowflake bu(std::stoull(cid.substr(is_block ? 11 : 9)));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return; }

                MonsterHuntGame g;
                bool found = false;
                std::string pet_muscle_tense;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = monster_hunt_games.find(uid);
                    if (it != monster_hunt_games.end()) { g = it->second; found = true; }
                    auto pit = pet_data.find(uid);
                    if (pit != pet_data.end())
                        for (auto& s : pit->second.statuses) if (s == "肌肉緊繃") { pet_muscle_tense = s; break; }
                }
                if (!found) {
                    ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 沒有進行中的狩獵！").set_flags(dpp::m_ephemeral));
                    return;
                }

                bool win = false; int64_t reward = 0; HuntDropList hunt_drops2;
                bool ended = process_combat(g, false, !pet_muscle_tense.empty(), win, reward, hunt_drops2, is_block, is_battlecry);

                if (ended) {
                    dpp::timer tid = 0;
                    { std::lock_guard<std::mutex> lk(data_mutex); tid = g.timer_id; monster_hunt_games.erase(uid); }
                    if (tid) g_bot->stop_timer(tid);
                    if (win) {
                        bool first_clear = false;
                        { std::lock_guard<std::mutex> lk(data_mutex); first_clear = hunt_clear_data[uid].count(g.monster_key) == 0; hunt_clear_data[uid].insert(g.monster_key); }
                        if (first_clear) { auto* md = find_monster(g.monster_key); if (md) reward += md->first_clear_reward; }
                        add_chips(uid, reward);
                        { std::lock_guard<std::mutex> lk(data_mutex);
                          for (auto& [k, c] : hunt_drops2) inventory_data[uid][k] += c; }
                        save_chips(); save_hunt_clear();
                        if (!hunt_drops2.empty()) save_inventory();
                        ev.reply(dpp::ir_update_message, make_combat_end_msg(true, g, reward, first_clear, hunt_drops2, dn, av));
                    } else {
                        { std::lock_guard<std::mutex> lk(data_mutex);
                          auto& p = pet_data[uid];
                          bool already = false;
                          for (auto& s : p.statuses) if (s == "受傷") { already = true; break; }
                          if (!already) p.statuses.push_back("受傷"); }
                        save_pet_data();
                        ev.reply(dpp::ir_update_message, make_combat_end_msg(false, g, 0, false, {}, dn, av));
                    }
                } else {
                    { std::lock_guard<std::mutex> lk(data_mutex); monster_hunt_games[uid] = g; }
                    ev.reply(dpp::ir_update_message, make_combat_msg(g, dn, av));
                }
            } else if (cid.rfind("hunt_refresh_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(13)));
                if (uid != bu) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
                }
                MonsterHuntGame hg;
                bool hfound = false;
                { std::lock_guard<std::mutex> lk(data_mutex);
                  auto it = monster_hunt_games.find(uid);
                  if (it != monster_hunt_games.end()) { hg = it->second; hfound = true; } }
                if (!hfound) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 沒有進行中的狩獵！").set_flags(dpp::m_ephemeral)); return;
                }
                // Mark old message as stale (remove buttons)
                dpp::embed se; se.set_title("🔄 訊息已刷新").set_color(0x95A5A6)
                                  .set_description("請往下滑查看最新戰況！");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(se));
                // Post fresh combat message and update msg_id
                auto fresh = make_combat_msg(hg, dn, av);
                fresh.channel_id = hg.channel_id;
                g_bot->message_create(fresh, [uid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        auto nid = std::get<dpp::message>(cb.value).id;
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = monster_hunt_games.find(uid);
                        if (it != monster_hunt_games.end()) it->second.msg_id = nid;
                    }
                });
            }
        }
        // ── 暗黑龍王 按鈕 ──────────────────────────────────────────────────────
        else if (cid.rfind("dd_", 0) == 0) {
            dpp::snowflake ch = ev.command.channel_id;

            if (cid.rfind("dd_refresh_", 0) == 0) {
                dpp::snowflake dch(std::stoull(cid.substr(11)));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = dd_games.find(dch);
                if (it == dd_games.end()) return;
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(it->second));
            }
            else if (cid.rfind("dd_target_", 0) == 0) {
                // dd_target_{uid}_{head_idx}
                std::string rest = cid.substr(10);
                size_t us = rest.rfind('_');
                if (us == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, us)));
                int head_idx = std::stoi(rest.substr(us+1));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 不是你的回合！").set_flags(dpp::m_ephemeral)); return; }
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch);
                if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn) return;
                if (dg.players[dg.current_player].uid != uid) return;
                if (head_idx < 0 || head_idx >= 3 || !dg.heads[head_idx].alive) return;
                dg.selected_head = head_idx;
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            else if (cid.rfind("dd_back_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(8)));
                if (uid != bu) return;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch);
                if (git == dd_games.end()) return;
                git->second.selected_head = -1;
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(git->second));
            }
            else if (cid.rfind("dd_atk_", 0) == 0 || cid.rfind("dd_gamble_", 0) == 0 || cid.rfind("dd_pow_", 0) == 0) {
                // parse uid from suffix
                std::string pfx = (cid.rfind("dd_atk_",0)==0) ? "dd_atk_" : (cid.rfind("dd_gamble_",0)==0) ? "dd_gamble_" : "dd_pow_";
                dpp::snowflake bu(std::stoull(cid.substr(pfx.size())));
                if (uid != bu) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 不是你的回合！").set_flags(dpp::m_ephemeral)); return; }
                int atype = (pfx == "dd_atk_") ? 0 : (pfx == "dd_gamble_") ? 1 : 2;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch);
                if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                if (dg.selected_head < 0) return;
                std::string plog = dd_do_attack(dg, atype);
                if (dg.game_over && dg.victory) {
                    bot.stop_timer(dg.timer_id);
                    std::vector<std::pair<std::string,std::string>> rewards;
                    if (!dg.practice_mode) {
                        for (auto& p : dg.players)
                            rewards.push_back({p.display_name, dd_give_rewards_one(p.uid)});
                        save_chips(); save_inventory();
                    }
                    auto emsg = make_dd_end_msg(dg, rewards);
                    emsg.channel_id = ch;
                    dd_games.erase(ch);
                    ev.reply(dpp::ir_update_message, emsg);
                    return;
                }
                dg.log_line = plog;
                dd_finish_turn(dg);
                if (dg.game_over) {
                    bot.stop_timer(dg.timer_id);
                    if (!dg.practice_mode) {
                        for (auto& p : dg.players) {
                            auto& pet2 = pet_data[p.uid];
                            bool al = false;
                            for (auto& s : pet2.statuses) if (s == "受傷") { al = true; break; }
                            if (!al) pet2.statuses.push_back("受傷");
                        }
                        save_pet_data();
                    }
                    auto emsg = make_dd_end_msg(dg, {}); emsg.channel_id = ch;
                    dd_games.erase(ch); ev.reply(dpp::ir_update_message, emsg); return;
                }
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            else if (cid.rfind("dd_block_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(9)));
                if (uid != bu) return;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch);
                if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                dg.block_active = true;
                dg.selected_head = -1;
                dg.log_line = "🛡️ **" + dg.players[dg.current_player].display_name + "** 進入防禦姿態！";
                dd_finish_turn(dg);
                if (dg.game_over) {
                    bot.stop_timer(dg.timer_id);
                    if (!dg.practice_mode) {
                        for (auto& p : dg.players) {
                            auto& pet2 = pet_data[p.uid];
                            bool al = false;
                            for (auto& s : pet2.statuses) if (s == "受傷") { al = true; break; }
                            if (!al) pet2.statuses.push_back("受傷");
                        }
                        save_pet_data();
                    }
                    auto emsg = make_dd_end_msg(dg, {}); emsg.channel_id = ch;
                    dd_games.erase(ch); ev.reply(dpp::ir_update_message, emsg); return;
                }
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            else if (cid.rfind("dd_altar_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(9)));
                if (uid != bu) return;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch);
                if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                if (dg.atk_triple) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 祭壇已毀滅！").set_flags(dpp::m_ephemeral)); return; }
                auto& cp = dg.players[dg.current_player];
                cp.at_altar = true;
                dg.selected_head = -1;
                dg.log_line = "🏛️ **" + cp.display_name + "** 移動至祭壇。";
                dd_finish_turn(dg);
                if (dg.game_over) {
                    bot.stop_timer(dg.timer_id);
                    if (!dg.practice_mode) {
                        for (auto& p : dg.players) {
                            auto& pet2 = pet_data[p.uid];
                            bool al = false;
                            for (auto& s : pet2.statuses) if (s == "受傷") { al = true; break; }
                            if (!al) pet2.statuses.push_back("受傷");
                        }
                        save_pet_data();
                    }
                    auto emsg = make_dd_end_msg(dg, {}); emsg.channel_id = ch;
                    dd_games.erase(ch); ev.reply(dpp::ir_update_message, emsg); return;
                }
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            else if (cid.rfind("dd_pool_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(8)));
                if (uid != bu) return;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch);
                if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                auto& cp = dg.players[dg.current_player];
                cp.at_altar = false;
                dg.log_line = "🏊 **" + cp.display_name + "** 回到龍池。";
                dd_finish_turn(dg);
                if (dg.game_over) {
                    bot.stop_timer(dg.timer_id);
                    if (!dg.practice_mode) {
                        for (auto& p : dg.players) {
                            auto& pet2 = pet_data[p.uid];
                            bool al = false;
                            for (auto& s : pet2.statuses) if (s == "受傷") { al = true; break; }
                            if (!al) pet2.statuses.push_back("受傷");
                        }
                        save_pet_data();
                    }
                    auto emsg = make_dd_end_msg(dg, {}); emsg.channel_id = ch;
                    dd_games.erase(ch); ev.reply(dpp::ir_update_message, emsg); return;
                }
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            else if (cid.rfind("dd_pray_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(8)));
                if (uid != bu) return;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch);
                if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                auto& cp = dg.players[dg.current_player];
                if (cp.has_bomb) {
                    cp.has_bomb = false; cp.bomb_turns = 0;
                    dg.log_line = "🙏 **" + cp.display_name + "** 向女神祈禱，炸彈解除！";
                } else {
                    dg.log_line = "🙏 **" + cp.display_name + "** 你誠心誠意的祈禱...";
                }
                dd_finish_turn(dg);
                if (dg.game_over) {
                    bot.stop_timer(dg.timer_id);
                    if (!dg.practice_mode) {
                        for (auto& p : dg.players) {
                            auto& pet2 = pet_data[p.uid];
                            bool al = false;
                            for (auto& s : pet2.statuses) if (s == "受傷") { al = true; break; }
                            if (!al) pet2.statuses.push_back("受傷");
                        }
                        save_pet_data();
                    }
                    auto emsg = make_dd_end_msg(dg, {}); emsg.channel_id = ch;
                    dd_games.erase(ch); ev.reply(dpp::ir_update_message, emsg); return;
                }
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
            else if (cid.rfind("dd_demolish_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(12)));
                if (uid != bu) return;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto git = dd_games.find(ch);
                if (git == dd_games.end()) return;
                auto& dg = git->second;
                if (dg.game_over || dg.boss_turn || dg.players[dg.current_player].uid != uid) return;
                auto& cp = dg.players[dg.current_player];
                if (!cp.at_altar || dg.atk_triple) return;
                dg.altar_hp--;
                std::string plog = "⛏️ **" + cp.display_name + "** 拆除祭壇！（剩餘 " + std::to_string(dg.altar_hp) + " 格血）";
                if (dg.altar_hp <= 0) {
                    dg.atk_triple = true;
                    plog += "\n💥 **祭壇毀滅！全體 ATK×3！**";
                    for (auto& p : dg.players) {
                        if (!p.alive) continue;
                        if (p.at_altar) { p.at_altar = false; }
                        int old = p.hp;
                        p.hp = std::min(p.hp + 25, p.max_hp);
                        if (p.hp > old)
                            plog += "\n  → " + p.display_name + " 回復 " + std::to_string(p.hp - old) + " HP";
                    }
                }
                dg.log_line = plog;
                dd_finish_turn(dg);
                if (dg.game_over) {
                    bot.stop_timer(dg.timer_id);
                    if (!dg.practice_mode) {
                        for (auto& p : dg.players) {
                            auto& pet2 = pet_data[p.uid];
                            bool al = false;
                            for (auto& s : pet2.statuses) if (s == "受傷") { al = true; break; }
                            if (!al) pet2.statuses.push_back("受傷");
                        }
                        save_pet_data();
                    }
                    auto emsg = make_dd_end_msg(dg, {}); emsg.channel_id = ch;
                    dd_games.erase(ch); ev.reply(dpp::ir_update_message, emsg); return;
                }
                ev.reply(dpp::ir_update_message, make_dd_combat_msg(dg));
            }
        }
#endif // ── end raid/hunt/village/dd button blocks ──────────────────────────────
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
        else if (cid.rfind("craft_main_", 0) == 0) {
            dpp::snowflake bu(std::stoull(cid.substr(11)));
            if (uid != bu) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
            }
            ev.reply(dpp::ir_update_message, make_craft_msg(uid));
        }
        // ── 背包按鈕 ──────────────────────────────────────────────────────────
        else if (cid.rfind("bag_", 0) == 0) {
            // bag_tab_equip_{uid}, bag_tab_items_{uid}
            // bag_sell_eq_{uid}_{key}, bag_sell_bulk_{uid}_{rarity}
            auto bag_uid = [&](size_t pfx) -> dpp::snowflake {
                std::string rest = cid.substr(pfx);
                size_t sep = rest.find('_');
                return dpp::snowflake(std::stoull(sep == std::string::npos ? rest : rest.substr(0, sep)));
            };
            auto chk = [&](dpp::snowflake bu) -> bool {
                if (uid != bu) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的背包！").set_flags(dpp::m_ephemeral));
                    return false;
                }
                return true;
            };

            if (cid.rfind("bag_tab_equip_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(14)));
                if (!chk(bu)) return;
                ev.reply(dpp::ir_update_message, make_bag_equip_msg(uid));

            } else if (cid.rfind("bag_tab_items_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(14)));
                if (!chk(bu)) return;
                ev.reply(dpp::ir_update_message, make_pet_use_msg(uid));

            } else if (cid.rfind("bag_tab_other_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(14)));
                if (!chk(bu)) return;
                ev.reply(dpp::ir_update_message, make_pet_other_msg(uid));

            } else if (cid.rfind("bag_sell_page_equip_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(20)));
                if (!chk(bu)) return;
                ev.reply(dpp::ir_update_message, make_bag_sell_equip_msg(uid));

            } else if (cid.rfind("bag_sell_page_items_", 0) == 0) {
                dpp::snowflake bu(std::stoull(cid.substr(20)));
                if (!chk(bu)) return;
                ev.reply(dpp::ir_update_message, make_bag_sell_items_msg(uid));

            } else if (cid.rfind("bag_sell_eq_", 0) == 0) {
                // bag_sell_eq_{uid}_{key}
                std::string rest = cid.substr(12);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                if (!chk(bu)) return;
                std::string eq_key = rest.substr(s1 + 1);
                auto* gi = find_gacha_item(eq_key);
                if (!gi) return;

                int64_t price = eq_sell_price(gi->rarity);
                bool sold = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto& inv = inventory_data[uid];
                    auto& eq  = equipped_data[uid];
                    int cnt = inv.count(eq_key) ? inv[eq_key] : 0;
                    bool is_eq = (eq_key == eq.weapon || eq_key == eq.glove ||
                                  eq_key == eq.clothes || eq_key == eq.shoes || eq_key == eq.orb);
                    int sellable = cnt - (is_eq ? 1 : 0);
                    if (sellable > 0) {
                        inv[eq_key]--;
                        chip_data[uid].chips += price;
                        sold = true;
                    }
                }
                if (sold) { save_inventory(); save_chips(); }
                ev.reply(dpp::ir_update_message, make_bag_equip_msg(uid));

            } else if (cid.rfind("bag_sell_item_", 0) == 0) {
                // bag_sell_item_{uid}_{key}
                std::string rest = cid.substr(14);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                if (!chk(bu)) return;
                std::string vi_key = rest.substr(s1 + 1);
                auto* vi = find_virtual_item(vi_key);
                if (!vi || vi->price <= 0 || vi->category == "hunt") return;
                int64_t sell_p = std::max((int64_t)1, (int64_t)(vi->price * 0.4));
                bool sold = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto& inv = inventory_data[uid];
                    if (inv.count(vi_key) && inv[vi_key] > 0) {
                        inv[vi_key]--;
                        chip_data[uid].chips += sell_p;
                        sold = true;
                    }
                }
                if (sold) { save_inventory(); save_chips(); }
                ev.reply(dpp::ir_update_message, make_pet_use_msg(uid));

            } else if (cid.rfind("bag_sell_bulk_", 0) == 0) {
                // bag_sell_bulk_{uid}_{rarity}
                std::string rest = cid.substr(14);
                size_t s1 = rest.find('_'); if (s1 == std::string::npos) return;
                dpp::snowflake bu(std::stoull(rest.substr(0, s1)));
                if (!chk(bu)) return;
                std::string rarity = rest.substr(s1 + 1);
                int64_t price = eq_sell_price(rarity);
                int64_t total = 0;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto& inv = inventory_data[uid];
                    auto& eq  = equipped_data[uid];
                    for (auto& [k, cnt] : inv) {
                        if (k.size() < 3 || k.substr(0, 3) != "EQ_" || cnt <= 0) continue;
                        auto* gi = find_gacha_item(k);
                        if (!gi || gi->rarity != rarity) continue;
                        bool is_eq = (k == eq.weapon || k == eq.glove ||
                                      k == eq.clothes || k == eq.shoes || k == eq.orb);
                        int sellable = cnt - (is_eq ? 1 : 0);
                        if (sellable > 0) {
                            total += (int64_t)sellable * price;
                            inv[k] -= sellable;
                        }
                    }
                    chip_data[uid].chips += total;
                }
                if (total > 0) { save_inventory(); save_chips(); }
                ev.reply(dpp::ir_update_message, make_bag_equip_msg(uid));
            }
        }
        // ── 寵物按鈕 ──────────────────────────────────────────────────────────
        else if (cid.rfind("lobby_", 0) == 0) {
            if (cid.rfind("lobby_main_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(11)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_lobby_msg(uid,
                    ev.command.usr.get_avatar_url(),
                    ev.command.member.get_nickname()));
            } else if (cid.rfind("lobby_shop_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(11)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的視窗！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_shop_main_msg(std::to_string((uint64_t)uid)));
            }
        }
        else if (cid.rfind("pet_", 0) == 0) {
            // All pet buttons embed the owner uid in the button ID
            if (cid.rfind("pet_work_select_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(16)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_pet_work_select_msg(uid));
            } else if (cid.rfind("pet_work_", 0) == 0) {
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
            } else if (cid.rfind("pet_cancel_work_confirm_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(24)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_cancel_work(uid));
            } else if (cid.rfind("pet_cancel_work_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(16)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                {
                    std::string uid_s = std::to_string((uint64_t)uid);
                    dpp::embed ce; ce.set_title("❓  確定要取消打工？").set_color(0xE74C3C);
                    ce.set_description("本次打工將**立即中止**，不會獲得任何報酬。");
                    dpp::component cr; cr.set_type(dpp::cot_action_row);
                    cr.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label("✅ 是，取消打工").set_id("pet_cancel_work_confirm_" + uid_s)
                        .set_style(dpp::cos_danger));
                    cr.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label("❌ 不，繼續打工").set_id("pet_refresh_" + uid_s)
                        .set_style(dpp::cos_success));
                    dpp::message cm; cm.add_embed(ce); cm.add_component(cr);
                    ev.reply(dpp::ir_update_message, cm);
                }
            } else if (cid.rfind("pet_start_onsen_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(16)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_start_onsen(uid));
            } else if (cid.rfind("pet_cancel_onsen_confirm_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(25)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_cancel_onsen(uid));
            } else if (cid.rfind("pet_cancel_onsen_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(17)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                {
                    std::string uid_s = std::to_string((uint64_t)uid);
                    dpp::embed ce;
                    ce.set_title("⚠️  確定要取消泡溫泉？").set_color(0xE67E22);
                    ce.set_description("取消後**負面狀態不會清除**，已花費的溫泉時間也不會退回。");
                    dpp::message cm; cm.add_embed(ce);
                    dpp::component row; row.set_type(dpp::cot_action_row);
                    dpp::component yes, no;
                    yes.set_type(dpp::cot_button).set_label("✅ 確認取消")
                       .set_id("pet_cancel_onsen_confirm_" + uid_s).set_style(dpp::cos_danger);
                    no.set_type(dpp::cot_button).set_label("↩️ 返回")
                       .set_id("pet_view_" + uid_s).set_style(dpp::cos_secondary);
                    row.add_component(yes); row.add_component(no);
                    cm.add_component(row);
                    ev.reply(dpp::ir_update_message, cm);
                }
            } else if (cid.rfind("pet_notify_toggle_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(18)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = pet_data.find(uid);
                    if (it != pet_data.end())
                        it->second.notify_after_work = !it->second.notify_after_work;
                }
                save_pet_data();
                ev.reply(dpp::ir_update_message, make_pet_view_msg(uid,
                    ev.command.usr.get_avatar_url(),
                    ev.command.member.get_nickname()));
            } else if (cid.rfind("pet_open_use_", 0) == 0) {
                dpp::snowflake btn_uid(std::stoull(cid.substr(13)));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_pet_use_msg(uid, 0));
            } else if (cid.rfind("pet_bag_page_", 0) == 0) {
                // pet_bag_page_{uid}_{page}
                std::string rest = cid.substr(13);
                size_t sep = rest.rfind('_');
                if (sep == std::string::npos) return;
                dpp::snowflake btn_uid(std::stoull(rest.substr(0, sep)));
                int page = std::stoi(rest.substr(sep + 1));
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的背包！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, make_pet_use_msg(uid, page));
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
                int cnt = 0;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto ii = inventory_data.find(uid);
                    if (ii != inventory_data.end()) { auto ci = ii->second.find(item_key); if (ci != ii->second.end()) cnt = ci->second; }
                }
                if (cnt > 1) { ev.reply(dpp::ir_update_message, make_pet_use_qty_msg(uid, item_key)); return; }
                ev.reply(dpp::ir_update_message, handle_pet_use_item(uid, item_key, 1));
            } else if (cid.rfind("pet_useqty_", 0) == 0) {
                // pet_useqty_{uid}_{qty}_{item_key}
                std::string rest = cid.substr(11);
                size_t s1 = rest.find('_');
                if (s1 == std::string::npos) return;
                dpp::snowflake btn_uid(std::stoull(rest.substr(0, s1)));
                rest = rest.substr(s1 + 1);
                size_t s2 = rest.find('_');
                if (s2 == std::string::npos) return;
                int qty = std::stoi(rest.substr(0, s2));
                std::string item_key = rest.substr(s2 + 1);
                if (uid != btn_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 這不是你的寵物！").set_flags(dpp::m_ephemeral)); return;
                }
                ev.reply(dpp::ir_update_message, handle_pet_use_item(uid, item_key, qty));
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
        // ── 天賦選擇按鈕（talent_pick_ 不以 pet_ 開頭，必須獨立在外層）──────
        else if (cid.rfind("talent_pick_", 0) == 0) {
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
            std::string old_talent;
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
                old_talent = pet.talent;
                inv["talent_scroll"]--;
                pet.talent = talent;
            }
            save_inventory();
            save_pet_data();
            auto talent_desc_fn = [](const std::string& t) -> std::string {
                if (t == "迅捷")      return "打工時間縮短 10%！";
                if (t == "招人喜歡")  return "打工報酬提升 10%！";
                if (t == "幸運")      return "打工有 5% 機率獲得雙倍報酬！";
                if (t == "天然呆")    return "使用道具時有 5% 機率不消耗道具！";
                if (t == "喜歡作夢")  return "每次打工完有 0.1% 機率將現有籌碼翻倍！";
                return "";
            };
            dpp::embed re;
            re.set_title("🌟  天賦覺醒！").set_color(0xF39C12);
            if (!old_talent.empty())
                re.set_description("🔄 天賦已替換！\n**" + old_talent + "** → **" + talent + "**\n" + talent_desc_fn(talent));
            else
                re.set_description("✨ 天賦賦予成功！\n**" + talent + "** — " + talent_desc_fn(talent));
            ev.reply(dpp::ir_update_message, dpp::message().add_embed(re));
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
#if 0 // ── UC button blocks moved to handlers_uc.cpp ────────────────────────────
        else if (cid.rfind("uc_again_", 0) == 0 || cid.rfind("uc_adult_again_", 0) == 0) {
            bool adult_game = (cid.rfind("uc_adult_again_", 0) == 0);
            size_t prefix_len = adult_game ? 15 : 9;
            std::string rest = cid.substr(prefix_len);
            auto sep = rest.rfind('_');
            dpp::snowflake target_ch(std::stoull(rest.substr(0, sep)));
            bool already = false; uint64_t new_gid = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (channel_uc_game.count(target_ch)) { already = true; }
                else {
                    new_gid = uc_counter++;
                    UCGame g;
                    g.id = new_gid; g.channel_id = target_ch;
                    g.guild_id = ev.command.guild_id; g.host_id = uid;
                    g.adult_allowed = adult_game;
                    if (adult_game) g.word_pool = "adult";
                    UCPlayer host;
                    host.uid = uid; host.seat = 0;
                    host.display_name = ev.command.member.get_nickname().empty()
                        ? ev.command.get_issuing_user().username
                        : ev.command.member.get_nickname();
                    g.players.push_back(host);
                    uc_games[new_gid] = g;
                    channel_uc_game[target_ch] = new_gid;
                }
            }
            if (already) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此頻道已有進行中的誰是臥底遊戲！").set_flags(dpp::m_ephemeral));
                return;
            }
            dpp::message lobby_m;
            { std::lock_guard<std::mutex> lk(data_mutex); lobby_m = uc_lobby_msg(uc_games[new_gid]); }
            lobby_m.channel_id = target_ch;
            ev.reply(dpp::ir_channel_message_with_source, lobby_m, [new_gid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = uc_games.find(new_gid);
                    if (it != uc_games.end())
                        it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
        else if (cid.rfind("uc_join_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(8));
            std::string err; bool ok = false; dpp::message new_msg;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end()) err = "❌ 遊戲不存在！";
                else {
                    auto& g = it->second;
                    if (g.phase != UCPhase::WAITING)          err = "⚠️ 遊戲已在進行中！";
                    else if ((int)g.players.size() >= 12)      err = "❌ 遊戲已滿（最多12人）！";
                    else {
                        bool found = false;
                        for (auto& p : g.players) if (p.uid == uid) { found = true; break; }
                        if (found) err = "⚠️ 你已在遊戲中！";
                        else {
                            UCPlayer np; np.uid = uid; np.seat = (int)g.players.size();
                            np.display_name = ev.command.member.get_nickname().empty()
                                ? ev.command.get_issuing_user().username
                                : ev.command.member.get_nickname();
                            g.players.push_back(np);
                            new_msg = uc_lobby_msg(g); ok = true;
                        }
                    }
                }
            }
            if (ok) ev.reply(dpp::ir_update_message, new_msg);
            else    ev.reply(dpp::ir_channel_message_with_source,
                             dpp::message(err).set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("uc_leave_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(9));
            std::string err; bool ok = false; dpp::message new_msg;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end()) err = "❌ 遊戲不存在！";
                else {
                    auto& g = it->second;
                    if (g.phase != UCPhase::WAITING) err = "⚠️ 遊戲已在進行中！";
                    else if (uid == g.host_id)       err = "⚠️ 主持人無法離開，請解散遊戲！";
                    else {
                        auto& ps = g.players;
                        auto rem = std::remove_if(ps.begin(), ps.end(),
                            [uid](const UCPlayer& p){ return p.uid == uid; });
                        if (rem == ps.end()) err = "⚠️ 你不在遊戲中！";
                        else { ps.erase(rem, ps.end()); new_msg = uc_lobby_msg(g); ok = true; }
                    }
                }
            }
            if (ok) ev.reply(dpp::ir_update_message, new_msg);
            else    ev.reply(dpp::ir_channel_message_with_source,
                             dpp::message(err).set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("uc_dissolve_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(12));
            std::string err; bool ok = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end()) err = "❌ 遊戲不存在！";
                else if (uid != it->second.host_id) err = "❌ 只有主持人可以解散！";
                else {
                    channel_uc_game.erase(it->second.channel_id);
                    uc_games.erase(it); ok = true;
                }
            }
            if (ok) {
                dpp::embed e; e.set_title("🗑️ 誰是臥底 — 遊戲已解散").set_color(0x95A5A6);
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
            } else ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message(err).set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("uc_mode_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(8));
            std::string err; dpp::message new_msg; bool ok = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end()) err = "❌ 遊戲不存在！";
                else if (it->second.phase != UCPhase::WAITING) err = "⚠️ 遊戲已開始，無法切換模式！";
                else if (uid != it->second.host_id) err = "❌ 只有主持人可以切換模式！";
                else {
                    it->second.blank_mode = !it->second.blank_mode;
                    new_msg = uc_lobby_msg(it->second); ok = true;
                }
            }
            if (ok) ev.reply(dpp::ir_update_message, new_msg);
            else    ev.reply(dpp::ir_channel_message_with_source,
                             dpp::message(err).set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("uc_start_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(9));
            std::string err; bool ok = false; dpp::message started_msg;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end()) err = "❌ 遊戲不存在！";
                else {
                    auto& g = it->second;
                    if (uid != g.host_id)             err = "❌ 只有主持人可以開始！";
                    else if (g.phase != UCPhase::WAITING) err = "⚠️ 遊戲已開始！";
                    else if ((int)g.players.size() < 4)   err = "❌ 至少需要 4 名玩家！";
                    else {
                        dpp::embed e; e.set_title("🕵️ 誰是臥底 — 遊戲開始！").set_color(0x9B59B6);
                        std::string spy_info = g.blank_mode
                            ? "1 位白板"
                            : std::to_string(uc_spy_num((int)g.players.size())) + " 位臥底";
                        std::string desc = "✅ 遊戲已開始！私訊已發送給所有玩家。\n共 "
                            + std::to_string(g.players.size()) + " 位玩家，"
                            + spy_info + "。\n\n";
                        for (auto& p : g.players) desc += "• " + p.display_name + "\n";
                        e.set_description(desc);
                        started_msg = dpp::message().add_embed(e);
                        ok = true;
                    }
                }
            }
            if (ok) {
                ev.reply(dpp::ir_update_message, started_msg);
                uc_begin_game(gid);
            } else ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message(err).set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("uc_spoke_", 0) == 0) {
            // Host-only skip for current speaker
            uint64_t gid = std::stoull(cid.substr(9));
            bool all_done = false;
            UCGame snap;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end() || it->second.phase != UCPhase::DESCRIBING) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 不是發言時間！").set_flags(dpp::m_ephemeral)); return;
                }
                auto& g = it->second;
                if (uid != g.host_id) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 只有主持人可以跳過！").set_flags(dpp::m_ephemeral)); return;
                }
                g.speak_pos++;
                all_done = (g.speak_pos >= (int)g.speak_order.size());
                if (all_done) g.phase = UCPhase::VOTING;
                snap = g;
            }
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("⏭️ 已跳過！").set_flags(dpp::m_ephemeral));
            if (all_done) {
                g_bot->message_delete(snap.describe_msg_id, snap.channel_id);
                auto ans_msg = uc_all_answers_msg(snap);
                ans_msg.channel_id = snap.channel_id;
                g_bot->message_create(ans_msg);
                auto vmsg = uc_vote_msg(snap);
                vmsg.channel_id = snap.channel_id;
                g_bot->message_create(vmsg, [gid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        if (uc_games.count(gid))
                            uc_games[gid].vote_msg_id = std::get<dpp::message>(cb.value).id;
                    }
                });
            } else {
                auto edit_msg = uc_describe_msg(snap);
                edit_msg.id = snap.describe_msg_id;
                edit_msg.channel_id = snap.channel_id;
                g_bot->message_edit(edit_msg);
            }
        }
        else if (cid.rfind("uc_answer_", 0) == 0) {
            // Open answer modal for the current speaker
            uint64_t gid = std::stoull(cid.substr(10));
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end() || it->second.phase != UCPhase::DESCRIBING) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 不是發言時間！").set_flags(dpp::m_ephemeral)); return;
                }
                auto& g = it->second;
                bool is_speaker = (g.speak_pos < (int)g.speak_order.size()
                                   && g.speak_order[g.speak_pos] == uid);
                if (!is_speaker) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 還沒輪到你！").set_flags(dpp::m_ephemeral)); return;
                }
            }
            dpp::interaction_modal_response modal(
                "uc_answer_modal_" + std::to_string(gid), "💬 輸入你的描述");
            modal.add_component(dpp::component()
                .set_type(dpp::cot_text)
                .set_label("你的描述（不能直接說出詞本身！）")
                .set_id("answer")
                .set_required(true)
                .set_min_length(1)
                .set_max_length(200)
                .set_text_style(dpp::text_short));
            ev.dialog(modal);
        }
        else if (cid.rfind("uc_vote_", 0) == 0) {
            std::string rest = cid.substr(8);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            uint64_t gid = std::stoull(rest.substr(0, sep));
            dpp::snowflake target(std::stoull(rest.substr(sep+1)));
            bool can_vote = false, auto_resolve = false;
            dpp::message new_vote;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end() || it->second.phase != UCPhase::VOTING) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 不是投票時間！").set_flags(dpp::m_ephemeral)); return;
                }
                auto& g = it->second;
                auto* vp = uc_find(g, uid);
                if (!vp || !vp->alive) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你不在遊戲中或已淘汰！").set_flags(dpp::m_ephemeral)); return;
                }
                if (g.votes.count(uid)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 你已投過票了！").set_flags(dpp::m_ephemeral)); return;
                }
                if (uid == target) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 不能投自己！").set_flags(dpp::m_ephemeral)); return;
                }
                g.votes[uid] = target;
                int alive_cnt = 0, voted_cnt = 0;
                for (auto& p : g.players) if (p.alive) { alive_cnt++; if (g.votes.count(p.uid)) voted_cnt++; }
                auto_resolve = (alive_cnt > 0 && voted_cnt == alive_cnt);
                new_vote = uc_vote_msg(g);
            }
            ev.reply(dpp::ir_update_message, new_vote);
            if (auto_resolve) uc_resolve_vote(gid);
        }
        else if (cid.rfind("uc_vskip_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(9));
            bool auto_resolve = false; dpp::message new_vote;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end() || it->second.phase != UCPhase::VOTING) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 不是投票時間！").set_flags(dpp::m_ephemeral)); return;
                }
                auto& g = it->second;
                auto* vp = uc_find(g, uid);
                if (!vp || !vp->alive) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你不在遊戲中或已淘汰！").set_flags(dpp::m_ephemeral)); return;
                }
                if (g.votes.count(uid)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 你已投過票了！").set_flags(dpp::m_ephemeral)); return;
                }
                g.votes[uid] = dpp::snowflake(0);
                int alive_cnt = 0, voted_cnt = 0;
                for (auto& p : g.players) if (p.alive) { alive_cnt++; if (g.votes.count(p.uid)) voted_cnt++; }
                auto_resolve = (alive_cnt > 0 && voted_cnt == alive_cnt);
                new_vote = uc_vote_msg(g);
            }
            ev.reply(dpp::ir_update_message, new_vote);
            if (auto_resolve) uc_resolve_vote(gid);
        }
        else if (cid.rfind("uc_vforce_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(10));
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 遊戲不存在！").set_flags(dpp::m_ephemeral)); return; }
                if (uid != it->second.host_id) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以強制結算！").set_flags(dpp::m_ephemeral)); return; }
                if (it->second.phase != UCPhase::VOTING) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是投票階段！").set_flags(dpp::m_ephemeral)); return; }
            }
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("⚡ 強制結算中...").set_flags(dpp::m_ephemeral));
            uc_resolve_vote(gid);
        }
        // uc_guess_ 猜詞按鈕（必須在 uc_pkforce_/uc_pk_ 之前）
        else if (cid.rfind("uc_guess_", 0) == 0) {
            std::string rest = cid.substr(9);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            uint64_t gid = std::stoull(rest.substr(0, sep));
            dpp::snowflake elim_uid(std::stoull(rest.substr(sep+1)));
            if (uid != elim_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 只有被淘汰的玩家可以猜詞！").set_flags(dpp::m_ephemeral));
                return;
            }
            bool still_pending = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it != uc_games.end())
                    still_pending = (it->second.pending_elim == elim_uid);
            }
            if (!still_pending) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("⏰ 猜詞時間已過！").set_flags(dpp::m_ephemeral));
                return;
            }
            dpp::interaction_modal_response modal(
                "uc_guess_modal_" + std::to_string(gid), "💭 猜詞翻盤");
            modal.add_component(dpp::component()
                .set_type(dpp::cot_text)
                .set_label("猜猜看：平民的詞是什麼？")
                .set_id("guess_word")
                .set_required(true)
                .set_min_length(1)
                .set_max_length(30)
                .set_text_style(dpp::text_short));
            ev.dialog(modal);
        }
        // uc_pkforce_ 必須在 uc_pk_ 之前
        else if (cid.rfind("uc_pkforce_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(11));
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 遊戲不存在！").set_flags(dpp::m_ephemeral)); return; }
                if (uid != it->second.host_id) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以強制結算！").set_flags(dpp::m_ephemeral)); return; }
                if (it->second.phase != UCPhase::VOTE_PK) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 不是 PK 階段！").set_flags(dpp::m_ephemeral)); return; }
            }
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("⚡ PK 結算中...").set_flags(dpp::m_ephemeral));
            uc_resolve_pk(gid);
        }
        else if (cid.rfind("uc_pk_", 0) == 0) {
            std::string rest = cid.substr(6);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            uint64_t gid = std::stoull(rest.substr(0, sep));
            dpp::snowflake cand(std::stoull(rest.substr(sep+1)));
            bool auto_resolve = false; dpp::message new_pk;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end() || it->second.phase != UCPhase::VOTE_PK) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 不是 PK 時間！").set_flags(dpp::m_ephemeral)); return;
                }
                auto& g = it->second;
                // PK candidates cannot vote
                bool is_cand = std::find(g.pk_candidates.begin(), g.pk_candidates.end(), uid)
                               != g.pk_candidates.end();
                if (is_cand) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ PK 玩家不能投票！").set_flags(dpp::m_ephemeral)); return;
                }
                auto* vp = uc_find(g, uid);
                if (!vp || !vp->alive) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你不在遊戲或已淘汰！").set_flags(dpp::m_ephemeral)); return;
                }
                if (g.pk_votes.count(uid)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 你已投過了！").set_flags(dpp::m_ephemeral)); return;
                }
                g.pk_votes[uid] = cand;
                // eligible voters = alive non-candidates
                int eligible = 0, voted2 = 0;
                for (auto& p : g.players) {
                    if (!p.alive) continue;
                    bool ic = std::find(g.pk_candidates.begin(), g.pk_candidates.end(), p.uid)
                              != g.pk_candidates.end();
                    if (!ic) { eligible++; if (g.pk_votes.count(p.uid)) voted2++; }
                }
                auto_resolve = (eligible > 0 && voted2 == eligible);
                new_pk = uc_pk_msg(g);
            }
            ev.reply(dpp::ir_update_message, new_pk);
            if (auto_resolve) uc_resolve_pk(gid);
        }
#endif // ── end UC button blocks ────────────────────────────────────────────────
        // ── onw / wolf buttons → handlers_wolf.cpp ────────────────────────────
        else if (cid.rfind("onw_again_", 0) == 0 || cid.rfind("onw_", 0) == 0 ||
                 cid.rfind("wolf_", 0) == 0) {
            handle_wolf_button(ev); return;
        }
#if 0 // ── onw/wolf button blocks moved to handlers_wolf.cpp ──────────────────
        else if (cid.rfind("onw_again_", 0) == 0) {
            dpp::snowflake game_ch(std::stoull(cid.substr(10)));
            bool already = false;
            uint64_t new_gid = 0;
            std::string dn = ev.command.member.get_nickname().empty()
                ? ev.command.usr.username : ev.command.member.get_nickname();
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (channel_onw_game.count(game_ch)) { already = true; }
                else {
                    new_gid = onw_counter++;
                    ONWGame g;
                    g.id = new_gid; g.channel_id = game_ch;
                    g.guild_id = ev.command.guild_id; g.host_id = uid;
                    g.role_counts = {{"狼人",2},{"預言家",1},{"強盜",1},{"搗蛋鬼",1},{"酒鬼",1},{"村民",1}};
                    ONWPlayer host; host.uid = uid; host.display_name = dn;
                    g.players.push_back(host);
                    onw_games[new_gid] = g;
                    channel_onw_game[game_ch] = new_gid;
                }
            }
            if (already) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此頻道已有進行中的一夜狼人遊戲！").set_flags(dpp::m_ephemeral));
            } else {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("✅ 已開新局！").set_flags(dpp::m_ephemeral));
                dpp::message m;
                { std::lock_guard<std::mutex> lk(data_mutex); m = make_onw_lobby_msg(onw_games[new_gid]); }
                m.channel_id = game_ch;
                bot.message_create(m, [new_gid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(new_gid);
                        if (it != onw_games.end())
                            it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
                    }
                });
            }
        }
        // ── 一夜狼人按鈕 ──────────────────────────────────────────────────────
        else if (cid.rfind("onw_", 0) == 0) {
            // Find game: channel-side interaction or DM interaction
            dpp::snowflake ev_ch = ev.command.channel_id;
            uint64_t gid = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = channel_onw_game.find(ev_ch);
                if (it != channel_onw_game.end()) {
                    gid = it->second;
                } else {
                    // DM interaction — find game by player uid
                    for (auto& [id, g] : onw_games)
                        for (auto& p : g.players)
                            if (p.uid == uid) { gid = id; break; }
                }
            }
            if (!gid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 找不到遊戲！").set_flags(dpp::m_ephemeral));
            } else {
                std::string gs = std::to_string(gid);
                dpp::snowflake game_ch = 0;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = onw_games.find(gid);
                    if (it != onw_games.end()) game_ch = it->second.channel_id;
                }

                // ── Lobby: join ──────────────────────────────────────────────
                if (cid == "onw_join_"+gs) {
                    std::string notice;
                    dpp::message m;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::WAITING) { notice = "❌ 遊戲已開始！"; }
                        else if ((int)g.players.size() >= 8) { notice = "❌ 已達人數上限（8人）！"; }
                        else if (onw_find(g, uid)) { notice = "❌ 你已在遊戲中！"; }
                        else {
                            ONWPlayer p;
                            p.uid = uid;
                            p.display_name = ev.command.member.get_nickname().empty()
                                ? ev.command.usr.username : ev.command.member.get_nickname();
                            g.players.push_back(p);
                            m = make_onw_lobby_msg(g);
                        }
                    }
                    if (!notice.empty()) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message(notice).set_flags(dpp::m_ephemeral)); return;
                    }
                    ev.reply(dpp::ir_update_message, m);
                }
                // ── Lobby: leave ─────────────────────────────────────────────
                else if (cid == "onw_leave_"+gs) {
                    dpp::message m;
                    std::string notice;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::WAITING) { notice = "❌ 遊戲已開始，無法離開！"; }
                        else if (uid == g.host_id) { notice = "❌ 主持人無法離開（請直接結束遊戲）！"; }
                        else {
                            auto& pv = g.players;
                            pv.erase(std::remove_if(pv.begin(), pv.end(),
                                [uid](auto& p){ return p.uid == uid; }), pv.end());
                            m = make_onw_lobby_msg(g);
                        }
                    }
                    if (!notice.empty()) {
                        ev.reply(dpp::ir_channel_message_with_source,
                            dpp::message(notice).set_flags(dpp::m_ephemeral)); return;
                    }
                    ev.reply(dpp::ir_update_message, m);
                }
                // ── Lobby: role inc/dec ───────────────────────────────────────
                else if (cid.rfind("onw_inc_"+gs+"_", 0) == 0 || cid.rfind("onw_dec_"+gs+"_", 0) == 0) {
                    bool inc = (cid.rfind("onw_inc_", 0) == 0);
                    std::string role = cid.substr(cid.rfind('_') + 1);
                    // Replace _ between multi-word roles (none in our set, but safe)
                    dpp::message m;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (uid != g.host_id) {
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("❌ 只有主持人可以調整角色！").set_flags(dpp::m_ephemeral)); return;
                        }
                        int& cnt = g.role_counts[role];
                        if (inc) cnt++;
                        else if (cnt > 0) cnt--;
                        m = make_onw_lobby_msg(g);
                    }
                    ev.reply(dpp::ir_update_message, m);
                }
                // ── Lobby: toggle single-copy role (頭狼/女巫/村子白痴) ─────
                else if (cid.rfind("onw_tog_"+gs+"_", 0) == 0) {
                    std::string role = cid.substr(("onw_tog_"+gs+"_").size());
                    dpp::message m;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (uid != g.host_id) {
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("❌ 只有主持人可以調整角色！").set_flags(dpp::m_ephemeral)); return;
                        }
                        int& cnt = g.role_counts[role];
                        cnt = (cnt == 0) ? 1 : 0;
                        m = make_onw_lobby_msg(g);
                    }
                    ev.reply(dpp::ir_update_message, m);
                }
                // ── Lobby: start ─────────────────────────────────────────────
                else if (cid == "onw_start_"+gs) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        if (uid != it->second.host_id) {
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("❌ 只有主持人可以開始遊戲！").set_flags(dpp::m_ephemeral)); return;
                        }
                    }
                    ev.reply(dpp::ir_update_message,
                        dpp::message().add_embed(dpp::embed().set_title("🌙 遊戲開始！").set_description("角色發放中...").set_color(0x2C3E50)));
                    onw_begin_game(bot, gid);
                }
                // ── Lobby: dissolve ───────────────────────────────────────────
                else if (cid == "onw_dissolve_"+gs) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        if (uid != it->second.host_id) {
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("❌ 只有主持人可以解散遊戲！").set_flags(dpp::m_ephemeral)); return;
                        }
                        channel_onw_game.erase(it->second.channel_id);
                        onw_games.erase(it);
                    }
                    ev.reply(dpp::ir_update_message,
                        dpp::message().add_embed(
                            dpp::embed().set_title("💥 遊戲已解散").set_color(0x95A5A6)
                                .set_description("主持人解散了本場遊戲。")));
                }
                // ── Night: wolf peek center ───────────────────────────────────
                else if (cid.rfind("onw_wolf_peek_"+gs+"_", 0) == 0) {
                    int idx = std::stoi(cid.substr(cid.rfind('_')+1));
                    std::string peeked;
                    bool alpha_ready = false;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_WOLVES) return;
                        peeked = g.center[idx];
                        std::string wname;
                        for (auto& p : g.players) if (p.uid == uid) { wname = p.display_name; break; }
                        g.night_log.push_back("🐺 " + wname + "（孤狼）偷看中央牌 " + std::to_string(idx+1) + "：" + peeked);
                        g.wolf_done = true;
                        alpha_ready = g.alpha_done;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("🃏 中央牌 " + std::to_string(idx+1) + "：" + onw_emoji(peeked) + " **" + peeked + "**").set_flags(dpp::m_ephemeral));
                    if (alpha_ready) onw_start_seer(bot, gid);
                }
                // ── Night: wolf skip / multi-wolf confirm ─────────────────────
                else if (cid == "onw_wolf_skip_"+gs) {
                    bool proceed = false;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_WOLVES) {
                            // 遊戲已推進，還是給 ACK 讓 Discord 不顯示 failed
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("✅ 確認！").set_flags(dpp::m_ephemeral));
                            return;
                        }
                        if (!g.wolf_done) {
                            // 只統計一般狼人（非頭狼）需要確認
                            int reg_cnt = 0;
                            std::string wname;
                            for (auto& p : g.players) {
                                if (p.original_role == "狼人") reg_cnt++;
                                if (p.uid == uid) wname = p.display_name;
                            }
                            g.wolves_confirmed.insert(uid);
                            bool all_confirmed = (int)g.wolves_confirmed.size() >= reg_cnt;
                            if (all_confirmed) {
                                if (reg_cnt > 1) g.night_log.push_back("🐺 多狼確認彼此身份，未執行其他動作");
                                else g.night_log.push_back("🐺 " + wname + "（孤狼）選擇略過");
                                g.wolf_done = true;
                                proceed = g.alpha_done;
                            }
                        }
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 確認！").set_flags(dpp::m_ephemeral));
                    if (proceed) {
                        onw_start_seer(bot, gid);
                    }
                }
                // ── Night: alpha wolf infect player ───────────────────────────
                else if (cid.rfind("onw_alpha_inf_"+gs+"_", 0) == 0) {
                    dpp::snowflake target(std::stoull(cid.substr(cid.rfind('_')+1)));
                    bool proceed = false;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_WOLVES || g.alpha_done) return;
                        // Find center wolf card index
                        int cw_idx = -1;
                        for (int i = 0; i < 3; i++) if (g.center[i] == "狼人") { cw_idx = i; break; }
                        if (cw_idx < 0) return;
                        auto* tp = onw_find(g, target);
                        if (!tp) return;
                        std::string alpha_name, tname = tp->display_name;
                        for (auto& p : g.players) if (p.uid == uid) { alpha_name = p.display_name; break; }
                        std::swap(tp->current_role, g.center[cw_idx]);
                        g.night_log.push_back("🐾 " + alpha_name + "（頭狼）把中央狼人牌換給了 " + tname);
                        g.alpha_done = true;
                        proceed = g.wolf_done;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("🐾 已把中央狼人牌換給目標玩家！").set_flags(dpp::m_ephemeral));
                    if (proceed) onw_start_seer(bot, gid);
                }
                // ── Night: alpha wolf skip ────────────────────────────────────
                else if (cid == "onw_alpha_skip_"+gs) {
                    bool proceed = false;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_WOLVES || g.alpha_done) return;
                        std::string alpha_name;
                        for (auto& p : g.players) if (p.uid == uid) { alpha_name = p.display_name; break; }
                        g.night_log.push_back("🐾 " + alpha_name + "（頭狼）選擇不感染玩家");
                        g.alpha_done = true;
                        proceed = g.wolf_done;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 略過。").set_flags(dpp::m_ephemeral));
                    if (proceed) onw_start_seer(bot, gid);
                }
                // ── Night: seer look at player ────────────────────────────────
                else if (cid.rfind("onw_seer_p_"+gs+"_", 0) == 0) {
                    dpp::snowflake target(std::stoull(cid.substr(cid.rfind('_')+1)));
                    std::string role, tname, seer_name;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_SEER || g.seer_done) return;
                        auto* tp = onw_find(g, target);
                        if (!tp) return;
                        role = tp->current_role;
                        tname = tp->display_name;
                        for (auto& p : g.players) if (p.uid == uid) { seer_name = p.display_name; break; }
                        g.night_log.push_back("🔮 " + seer_name + "（預言家）查驗了 " + tname + " → " + role);
                        g.seer_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("🔮 **" + tname + "** 的身份是：" + onw_emoji(role) + " **" + role + "**").set_flags(dpp::m_ephemeral));
                    onw_start_robber(bot, gid);
                }
                // ── Night: seer look at two center cards ──────────────────────
                else if (cid == "onw_seer_center_"+gs) {
                    std::array<std::string,3> center;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        if (it->second.phase != ONWPhase::NIGHT_SEER) return;
                        center = it->second.center;
                    }
                    // Ask which two center cards
                    dpp::embed e;
                    e.set_title("🔮 選擇兩張中央牌").set_color(0x5865F2);
                    e.set_description("選擇要查看的兩張中央牌。");
                    dpp::message dm; dm.add_embed(e);
                    dpp::component row; row.set_type(dpp::cot_action_row);
                    row.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label("中央1+2").set_id("onw_seer_c2_"+gs+"_0_1").set_style(dpp::cos_primary));
                    row.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label("中央1+3").set_id("onw_seer_c2_"+gs+"_0_2").set_style(dpp::cos_primary));
                    row.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label("中央2+3").set_id("onw_seer_c2_"+gs+"_1_2").set_style(dpp::cos_primary));
                    dm.add_component(row);
                    ev.reply(dpp::ir_update_message, dm);
                }
                else if (cid.rfind("onw_seer_c2_"+gs+"_", 0) == 0) {
                    // Parse two indices
                    std::string tail = cid.substr(("onw_seer_c2_"+gs+"_").size());
                    int i1 = tail[0]-'0', i2 = tail[2]-'0';
                    std::string r1, r2;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_SEER || g.seer_done) return;
                        r1 = g.center[i1]; r2 = g.center[i2];
                        std::string seer_name;
                        for (auto& p : g.players) if (p.uid == uid) { seer_name = p.display_name; break; }
                        g.night_log.push_back("🔮 " + seer_name + "（預言家）查看中央牌 " + std::to_string(i1+1) + " 和 " + std::to_string(i2+1) + " → " + r1 + "、" + r2);
                        g.seer_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("🔮 中央牌 " + std::to_string(i1+1) + "：" + onw_emoji(r1) + " **" + r1 + "**　"
                            + "中央牌 " + std::to_string(i2+1) + "：" + onw_emoji(r2) + " **" + r2 + "**")
                        .set_flags(dpp::m_ephemeral));
                    onw_start_robber(bot, gid);
                }
                // ── Night: seer skip ─────────────────────────────────────────
                else if (cid == "onw_seer_skip_"+gs) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        if (it->second.phase != ONWPhase::NIGHT_SEER || it->second.seer_done) return;
                        std::string seer_name;
                        for (auto& p : it->second.players) if (p.uid == uid) { seer_name = p.display_name; break; }
                        it->second.night_log.push_back("🔮 " + seer_name + "（預言家）選擇略過");
                        it->second.seer_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 略過。").set_flags(dpp::m_ephemeral));
                    onw_start_robber(bot, gid);
                }
                // ── Night: robber steal ───────────────────────────────────────
                else if (cid.rfind("onw_robber_"+gs+"_", 0) == 0 && cid != "onw_robber_skip_"+gs) {
                    dpp::snowflake target(std::stoull(cid.substr(cid.rfind('_')+1)));
                    std::string new_role, tname;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_ROBBER || g.robber_done) return;
                        auto* rob = onw_find(g, uid);
                        auto* tgt = onw_find(g, target);
                        if (!rob || !tgt) return;
                        std::swap(rob->current_role, tgt->current_role);
                        new_role = rob->current_role;
                        tname = tgt->display_name;
                        g.night_log.push_back("🗡️ " + rob->display_name + "（強盜）與 " + tname + " 交換 → 得到 " + new_role);
                        g.robber_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("🗡️ 你與 **" + tname + "** 交換！你現在的身份是：" + onw_emoji(new_role) + " **" + new_role + "**")
                        .set_flags(dpp::m_ephemeral));
                    onw_start_troublemaker(bot, gid);
                }
                // ── Night: robber skip ────────────────────────────────────────
                else if (cid == "onw_robber_skip_"+gs) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        if (it->second.phase != ONWPhase::NIGHT_ROBBER || it->second.robber_done) return;
                        std::string rob_name;
                        for (auto& p : it->second.players) if (p.uid == uid) { rob_name = p.display_name; break; }
                        it->second.night_log.push_back("🗡️ " + rob_name + "（強盜）選擇略過");
                        it->second.robber_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 略過。").set_flags(dpp::m_ephemeral));
                    onw_start_troublemaker(bot, gid);
                }
                // ── Night: troublemaker first pick ────────────────────────────
                else if (cid.rfind("onw_tm1_"+gs+"_", 0) == 0) {
                    dpp::snowflake target(std::stoull(cid.substr(cid.rfind('_')+1)));
                    ONWGame g_copy;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_TROUBLEMAKER || g.troublemaker_done) return;
                        g.tm_first = target;
                        g_copy = g;
                    }
                    // Send second pick
                    std::string tname;
                    for (auto& p : g_copy.players)
                        if (p.uid == target) { tname = p.display_name; break; }
                    dpp::message dm = onw_pick_player_msg(
                        g_copy,
                        "😈 搗蛋鬼 — 第二個玩家",
                        "已選 **" + tname + "**，現在選第二名玩家與其互換。",
                        "onw_tm2_"+gs+"_",
                        uid, false
                    );
                    // Disable the first pick target button (can't swap with same person)
                    // Already excluded self, but not target. Remove target from options by sending new msg
                    ev.reply(dpp::ir_update_message, dm);
                }
                // ── Night: troublemaker second pick ───────────────────────────
                else if (cid.rfind("onw_tm2_"+gs+"_", 0) == 0) {
                    dpp::snowflake target2(std::stoull(cid.substr(cid.rfind('_')+1)));
                    std::string n1, n2;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_TROUBLEMAKER || g.troublemaker_done || !g.tm_first) return;
                        auto* p1 = onw_find(g, g.tm_first);
                        auto* p2 = onw_find(g, target2);
                        if (!p1 || !p2 || p1->uid == p2->uid) return;
                        n1 = p1->display_name; n2 = p2->display_name;
                        std::swap(p1->current_role, p2->current_role);
                        std::string tm_name;
                        for (auto& p : g.players) if (p.uid == uid) { tm_name = p.display_name; break; }
                        g.night_log.push_back("😈 " + tm_name + "（搗蛋鬼）交換了 " + n1 + " 和 " + n2 + " 的身份牌");
                        g.troublemaker_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("😈 已交換 **" + n1 + "** 與 **" + n2 + "** 的身份牌！（你沒看到）")
                        .set_flags(dpp::m_ephemeral));
                    onw_start_witch(bot, gid);
                }
                // ── Night: troublemaker skip ──────────────────────────────────
                else if (cid == "onw_tm_skip_"+gs) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        if (it->second.phase != ONWPhase::NIGHT_TROUBLEMAKER || it->second.troublemaker_done) return;
                        std::string tm_name;
                        for (auto& p : it->second.players) if (p.uid == uid) { tm_name = p.display_name; break; }
                        it->second.night_log.push_back("😈 " + tm_name + "（搗蛋鬼）選擇略過");
                        it->second.troublemaker_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 略過。").set_flags(dpp::m_ephemeral));
                    onw_start_witch(bot, gid);
                }
                // ── Night: village idiot left shift ──────────────────────────
                else if (cid == "onw_vi_left_"+gs) {
                    std::string vi_name;
                    std::string new_role;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_VILLAGE_IDIOT || g.vi_done) return;
                        int n = (int)g.players.size();
                        std::vector<std::string> old_roles(n);
                        for (int i = 0; i < n; i++) old_roles[i] = g.players[i].current_role;
                        for (int i = 0; i < n; i++)
                            g.players[i].current_role = old_roles[(i+1) % n];
                        for (auto& p : g.players) {
                            if (p.original_role == "村子白痴") { vi_name = p.display_name; new_role = p.current_role; break; }
                        }
                        g.night_log.push_back("🃏 " + vi_name + "（村子白痴）選擇左移，所有玩家牌向左循環一格");
                        g.vi_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⬅ 左移完成！你的新身份是：" + onw_emoji(new_role) + " **" + new_role + "**").set_flags(dpp::m_ephemeral));
                    bot.message_create(dpp::message(game_ch,
                        "🃏 **" + vi_name + "** 是**村子白痴**！原始身份牌公開翻面：🃏 **村子白痴**\n所有玩家的牌已向左循環一格。"));
                    onw_start_drunk(bot, gid);
                }
                // ── Night: village idiot right shift ─────────────────────────
                else if (cid == "onw_vi_right_"+gs) {
                    std::string vi_name;
                    std::string new_role;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_VILLAGE_IDIOT || g.vi_done) return;
                        int n = (int)g.players.size();
                        std::vector<std::string> old_roles(n);
                        for (int i = 0; i < n; i++) old_roles[i] = g.players[i].current_role;
                        for (int i = 0; i < n; i++)
                            g.players[i].current_role = old_roles[(i-1+n) % n];
                        for (auto& p : g.players) {
                            if (p.original_role == "村子白痴") { vi_name = p.display_name; new_role = p.current_role; break; }
                        }
                        g.night_log.push_back("🃏 " + vi_name + "（村子白痴）選擇右移，所有玩家牌向右循環一格");
                        g.vi_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("➡ 右移完成！你的新身份是：" + onw_emoji(new_role) + " **" + new_role + "**").set_flags(dpp::m_ephemeral));
                    bot.message_create(dpp::message(game_ch,
                        "🃏 **" + vi_name + "** 是**村子白痴**！原始身份牌公開翻面：🃏 **村子白痴**\n所有玩家的牌已向右循環一格。"));
                    onw_start_drunk(bot, gid);
                }
                // ── Night: witch peek center ──────────────────────────────────
                else if (cid.rfind("onw_witch_c_"+gs+"_", 0) == 0) {
                    int idx = std::stoi(cid.substr(cid.rfind('_')+1));
                    std::string peeked;
                    std::vector<ONWPlayer> all_players;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_WITCH || g.witch_done || g.witch_peeked) return;
                        peeked = g.center[idx];
                        g.witch_center = idx;
                        g.witch_peeked = true;
                        all_players = g.players;
                    }
                    // Show the peeked card and offer swap/skip buttons
                    dpp::embed e;
                    e.set_title("🧙 女巫 — 你看到了").set_color(0x9B59B6);
                    e.set_description("中央牌 " + std::to_string(idx+1) + "：" + onw_emoji(peeked) + " **" + peeked + "**\n\n要把這張牌換給誰？（可以換給自己）");
                    dpp::message dm; dm.add_embed(e);
                    std::string gs2 = std::to_string(gid);
                    for (int i = 0; i < (int)all_players.size(); i += 5) {
                        dpp::component row; row.set_type(dpp::cot_action_row);
                        for (int j = i; j < std::min((int)all_players.size(), i+5); j++) {
                            row.add_component(dpp::component().set_type(dpp::cot_button)
                                .set_label(std::to_string(j+1)+". "+all_players[j].display_name)
                                .set_id("onw_witch_swap_"+gs2+"_"+std::to_string((uint64_t)all_players[j].uid))
                                .set_style(dpp::cos_primary));
                        }
                        dm.add_component(row);
                    }
                    dpp::component skip_row; skip_row.set_type(dpp::cot_action_row);
                    skip_row.add_component(dpp::component().set_type(dpp::cot_button)
                        .set_label("略過（不換）").set_id("onw_witch_skip_"+gs2).set_style(dpp::cos_secondary));
                    dm.add_component(skip_row);
                    ev.reply(dpp::ir_update_message, dm);
                }
                // ── Night: witch swap center card with player ─────────────────
                else if (cid.rfind("onw_witch_swap_"+gs+"_", 0) == 0) {
                    dpp::snowflake target(std::stoull(cid.substr(cid.rfind('_')+1)));
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_WITCH || g.witch_done || !g.witch_peeked) return;
                        auto* tp = onw_find(g, target);
                        if (!tp) return;
                        std::string witch_name, tname = tp->display_name;
                        for (auto& p : g.players) if (p.uid == uid) { witch_name = p.display_name; break; }
                        std::swap(tp->current_role, g.center[g.witch_center]);
                        g.night_log.push_back("🧙 " + witch_name + "（女巫）把中央牌 " + std::to_string(g.witch_center+1) + " 換給了 " + tname);
                        g.witch_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 已換牌！").set_flags(dpp::m_ephemeral));
                    onw_start_village_idiot(bot, gid);
                }
                // ── Night: witch skip swap ────────────────────────────────────
                else if (cid == "onw_witch_skip_"+gs) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_WITCH || g.witch_done) return;
                        std::string witch_name;
                        for (auto& p : g.players) if (p.uid == uid) { witch_name = p.display_name; break; }
                        g.night_log.push_back("🧙 " + witch_name + "（女巫）選擇不換牌");
                        g.witch_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 略過。").set_flags(dpp::m_ephemeral));
                    onw_start_village_idiot(bot, gid);
                }
                // ── Night: drunk pick center ──────────────────────────────────
                else if (cid.rfind("onw_drunk_"+gs+"_", 0) == 0) {
                    int idx = std::stoi(cid.substr(cid.rfind('_')+1));
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::NIGHT_DRUNK || g.drunk_done) return;
                        auto* dp = onw_find(g, uid);
                        if (!dp) return;
                        std::swap(dp->current_role, g.center[idx]);
                        g.night_log.push_back("🍺 " + dp->display_name + "（酒鬼）拿走了中央牌 " + std::to_string(idx+1));
                        g.drunk_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("🍺 你拿走了中央牌 " + std::to_string(idx+1) + "，但你不知道是什麼。")
                        .set_flags(dpp::m_ephemeral));
                    onw_start_insomniac(bot, gid);
                }
                // ── Night: insomniac confirm ──────────────────────────────────
                else if (cid == "onw_insomniac_ok_"+gs) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        if (it->second.phase != ONWPhase::NIGHT_INSOMNIAC || it->second.insomniac_done) return;
                        std::string ins_name, ins_role;
                        for (auto& p : it->second.players) if (p.uid == uid) { ins_name = p.display_name; ins_role = p.current_role; break; }
                        it->second.night_log.push_back("😴 " + ins_name + "（失眠者）最終身份是 " + ins_role);
                        it->second.insomniac_done = true;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 確認！").set_flags(dpp::m_ephemeral));
                    onw_start_day(bot, gid);
                }
                // ── Day: begin vote ───────────────────────────────────────────
                else if (cid == "onw_begin_vote_"+gs) {
                    bool ok = false;
                    dpp::message vm;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (uid != g.host_id) {
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("❌ 只有主持人可以開始投票！").set_flags(dpp::m_ephemeral)); return;
                        }
                        if (g.phase != ONWPhase::DAY_DISCUSS) return;
                        g.phase = ONWPhase::DAY_VOTE;
                        vm = make_onw_vote_msg(g);
                        ok = true;
                    }
                    if (ok) {
                        ev.reply(dpp::ir_update_message,
                            dpp::message().add_embed(dpp::embed().set_title("🗳️ 投票開始！").set_color(0xF39C12)));
                        vm.channel_id = game_ch;
                        bot.message_create(vm, [gid](const dpp::confirmation_callback_t& cb) {
                            if (!cb.is_error()) {
                                std::lock_guard<std::mutex> lk(data_mutex);
                                auto it = onw_games.find(gid);
                                if (it != onw_games.end())
                                    it->second.vote_msg_id = std::get<dpp::message>(cb.value).id;
                            }
                        });
                    }
                }
                // ── Day: vote ─────────────────────────────────────────────────
                else if (cid.rfind("onw_vote_"+gs+"_", 0) == 0 && cid != "onw_vote_resolve_"+gs) {
                    dpp::snowflake target(std::stoull(cid.substr(cid.rfind('_')+1)));
                    bool all_voted = false; dpp::snowflake vote_msg_id = 0;
                    dpp::message vm;
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        auto& g = it->second;
                        if (g.phase != ONWPhase::DAY_VOTE) {
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("❌ 目前不是投票階段！").set_flags(dpp::m_ephemeral)); return;
                        }
                        auto* vp = onw_find(g, uid);
                        if (!vp) {
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("❌ 你不是本場玩家！").set_flags(dpp::m_ephemeral)); return;
                        }
                        vp->vote_target = target;
                        vote_msg_id = g.vote_msg_id;
                        int voted = 0;
                        for (auto& p : g.players) if (p.vote_target != 0) voted++;
                        all_voted = (voted == (int)g.players.size());
                        vm = make_onw_vote_msg(g);
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 已投票！").set_flags(dpp::m_ephemeral));
                    if (vote_msg_id)
                        bot.message_edit(dpp::message(vote_msg_id, "").add_embed(vm.embeds[0])
                            .set_channel_id(game_ch));
                    if (all_voted) onw_resolve_vote(bot, gid);
                }
                // ── Day: force resolve ────────────────────────────────────────
                else if (cid == "onw_vote_resolve_"+gs) {
                    {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it == onw_games.end()) return;
                        if (uid != it->second.host_id) {
                            ev.reply(dpp::ir_channel_message_with_source,
                                dpp::message("❌ 只有主持人可以強制結算！").set_flags(dpp::m_ephemeral)); return;
                        }
                        if (it->second.phase != ONWPhase::DAY_VOTE) return;
                    }
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 強制結算！").set_flags(dpp::m_ephemeral));
                    onw_resolve_vote(bot, gid);
                }
            }
        }
#endif // ── end onw/wolf button blocks (warn/admin/ledger handlers follow) ────
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
        // ── 猜數字按鈕 ────────────────────────────────────────────────────────
        else if (cid.rfind("guess_kbd_", 0) == 0) {
            dpp::snowflake owner(std::stoull(cid.substr(10)));
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (!guess_games.count(uid)) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                }
            }
            dpp::interaction_modal_response modal(
                "guess_modal_" + std::to_string((uint64_t)uid), "💬 輸入猜測");
            modal.add_component(dpp::component()
                .set_type(dpp::cot_text)
                .set_label("輸入 4 位數字（不重複，可含 0）")
                .set_id("guess_input")
                .set_required(true)
                .set_min_length(4).set_max_length(4)
                .set_text_style(dpp::text_short));
            ev.dialog(modal);
        }
        else if (cid.rfind("guess_digit_", 0) == 0) {
            // "guess_digit_<uid>_<d>"
            size_t last = cid.rfind('_');
            char digit = cid[last + 1];
            dpp::snowflake owner(std::stoull(cid.substr(12, last - 12)));
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            GuessGame snap; bool found = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = guess_games.find(owner);
                if (it != guess_games.end()) {
                    auto& g = it->second;
                    if (g.input_buf.size() < 4 && g.input_buf.find(digit) == std::string::npos)
                        g.input_buf += digit;
                    snap = g; found = true;
                }
            }
            if (!found) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return; }
            ev.reply(dpp::ir_update_message, make_guess_msg(snap));
        }
        else if (cid.rfind("guess_back_", 0) == 0) {
            dpp::snowflake owner(std::stoull(cid.substr(11)));
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            GuessGame snap; bool found = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = guess_games.find(owner);
                if (it != guess_games.end()) {
                    if (!it->second.input_buf.empty()) it->second.input_buf.pop_back();
                    snap = it->second; found = true;
                }
            }
            if (!found) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return; }
            ev.reply(dpp::ir_update_message, make_guess_msg(snap));
        }
        else if (cid.rfind("guess_confirm_", 0) == 0) {
            dpp::snowflake owner(std::stoull(cid.substr(14)));
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            bool won = false, game_over = false;
            GuessGame snap;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = guess_games.find(owner);
                if (it == guess_games.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                }
                auto& g = it->second;
                if (!guess_valid(g.input_buf)) {
                    g.input_buf = "";
                    ev.reply(dpp::ir_update_message, make_guess_msg(g)); return;
                }
                std::string ab = guess_calc_ab(g.secret, g.input_buf);
                g.history.emplace_back(g.input_buf, ab);
                g.attempts++;
                g.input_buf = "";
                won = (ab == "4A0B");
                game_over = won || (g.attempts >= GuessGame::MAX_ATTEMPTS);
                if (game_over) {
                    auto& st = guess_stats_data[(uint64_t)owner];
                    st.games++;
                    if (won) { st.wins++; st.total_win_attempts += g.attempts; }
                }
                snap = g;
                if (game_over) guess_games.erase(it);
            }
            if (game_over) {
                save_guess_stats();
                ev.reply(dpp::ir_update_message, make_guess_result_msg(snap, won));
            } else {
                ev.reply(dpp::ir_update_message, make_guess_msg(snap));
            }
        }
        else if (cid.rfind("guess_quit_", 0) == 0) {
            dpp::snowflake owner(std::stoull(cid.substr(11)));
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            GuessGame snap; bool found = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = guess_games.find(uid);
                if (it != guess_games.end()) { snap = it->second; guess_games.erase(it); found = true; }
            }
            if (!found) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                guess_stats_data[(uint64_t)uid].games++;
            }
            save_guess_stats();
            ev.reply(dpp::ir_update_message, make_guess_result_msg(snap, false));
        }
        else if (cid.rfind("guess_again_", 0) == 0) {
            dpp::snowflake owner(std::stoull(cid.substr(12)));
            if (uid != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            if (guess_games.count(uid)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 你已有進行中的猜數字遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                GuessGame g;
                g.uid = uid; g.channel_id = ev.command.channel_id;
                g.secret = guess_gen_secret();
                g.avatar_url   = user.get_avatar_url();
                g.display_name = ev.command.member.get_nickname().empty()
                                 ? user.username : ev.command.member.get_nickname();
                guess_games[uid] = g;
            }
            GuessGame snap; { std::lock_guard<std::mutex> lk(data_mutex); snap = guess_games[uid]; }
            auto gmsg = make_guess_msg(snap);
            ev.reply(dpp::ir_channel_message_with_source, gmsg);
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    if (guess_games.count(uid))
                        guess_games[uid].msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
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
#if 0 // old bj handlers — already handled by handlers_bj.cpp
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
                BJGame old_g; bool had_old = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = user_bj.find(uid);
                    if (it != user_bj.end()) {
                        auto git = bj_games.find(it->second);
                        if (git != bj_games.end()) { old_g = git->second; had_old = true; }
                        bj_games.erase(it->second); user_bj.erase(it);
                    }
                }
                if (had_old) bj_disable_old_msg(bot, old_g);
            }
            const dpp::user& u = ev.command.get_issuing_user();
            add_chips(uid, -bet);
            BJGame g = start_bj(uid, ev.command.channel_id, bet, u.get_avatar_url(), u.username);
            std::string status;
            if (is_blackjack(g.main_hand.cards)) {
                bool dbj = is_blackjack(g.dealer_cards);
                g.game_over = true;
                if (dbj) {
                    add_chips(uid, bet);
                    status = "雙 BJ — 平局！";
                    { std::lock_guard<std::mutex> lk(data_mutex); bj_stats_data[uid].pushes++; }
                } else {
                    int64_t win = (int64_t)(bet * 1.5);
                    add_chips(uid, bet + win);
                    status = "🌟 Blackjack！贏得 **" + std::to_string(win) + "** 碼！";
                    { std::lock_guard<std::mutex> lk(data_mutex); bj_stats_data[uid].wins++; bj_stats_data[uid].profit += win; }
                }
                save_bjstats();
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
#endif // end old bj handlers — boss/signup/team buttons active from here
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
#if 0 // old wolf button handlers — already handled by handlers_wolf.cpp
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
            // ── 解散房間 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_dissolve_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(14));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 房間不存在！").set_flags(dpp::m_ephemeral)); return; }
                    if (uid != it->second.host_id) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有主持人可以解散房間！").set_flags(dpp::m_ephemeral)); return; }
                    if (it->second.phase != WolfPhase::WAITING) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 遊戲已開始，無法解散！").set_flags(dpp::m_ephemeral)); return; }
                    channel_wolf_game.erase(it->second.channel_id);
                    wolf_games.erase(it);
                }
                dpp::embed e; e.set_title("🗑️  房間已解散").set_color(0x808080).set_description("主持人已解散此狼人殺房間。");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
            }
            // ── 警長競選 ───────────────────────────────────────────────────────
            else if (cid.rfind("wolf_nominate_", 0) == 0) {
                uint64_t gid = std::stoull(cid.substr(14));
                enum class NomAction { REFRESH, TEAR_BADGE, AUTO_SPEECH } nom_action = NomAction::REFRESH;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::SHERIFF_NOMINATE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 報名已結束！").set_flags(dpp::m_ephemeral)); return; }
                    auto* p = wfind(g, uid);
                    if (!p || !p->alive) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中！").set_flags(dpp::m_ephemeral)); return; }
                    auto cit = std::find(g.candidates.begin(), g.candidates.end(), uid);
                    if (cit != g.candidates.end()) {
                        g.candidates.erase(cit);
                        ev.reply(dpp::ir_update_message, make_sheriff_nominate_msg(g)); return;
                    }
                    g.candidates.push_back(uid);
                    int alive_cnt = 0;
                    for (auto& pl : g.players) if (pl.alive) alive_cnt++;
                    bool all_nominated = ((int)g.candidates.size() == alive_cnt);
                    int decided = (int)g.candidates.size() + (int)g.not_running.size() + (int)g.withdrawn_candidates.size();
                    if (all_nominated) {
                        g.phase = WolfPhase::DAY_ANNOUNCE;
                        nom_action = NomAction::TEAR_BADGE;
                        dpp::embed te; te.set_title("🗑️  全員參選！撕毀警徽").set_color(0x808080)
                            .set_description("所有玩家均參選警長，依規則直接撕毀警徽，本局無警長。");
                        ev.reply(dpp::ir_update_message, dpp::message().add_embed(te));
                    } else if (decided == alive_cnt) {
                        g.speak_seats.clear(); g.speak_idx = 0;
                        for (auto cuid : g.candidates) { auto* cp = wfind(g, cuid); if (cp) g.speak_seats.push_back(cp->seat); }
                        nom_action = NomAction::AUTO_SPEECH;
                        dpp::embed ae; ae.set_title("🎤  全員決定！開始競選發言").set_color(0xF39C12)
                            .set_description("所有玩家已表達意願，候選人依序發言。");
                        ev.reply(dpp::ir_update_message, dpp::message().add_embed(ae));
                    } else {
                        ev.reply(dpp::ir_update_message, make_sheriff_nominate_msg(g));
                    }
                }
                if (nom_action == NomAction::TEAR_BADGE)  announce_night_and_start_day(bot, gid);
                else if (nom_action == NomAction::AUTO_SPEECH) start_sheriff_speech(bot, gid);
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
                enum class WdAction { REFRESH, AUTO_SPEECH, AUTO_SKIP } wd_action = WdAction::REFRESH;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it == wolf_games.end()) return;
                    auto& g = it->second;
                    if (g.phase != WolfPhase::SHERIFF_NOMINATE) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 報名階段已結束！").set_flags(dpp::m_ephemeral)); return; }
                    auto* pfound = wfind(g, uid);
                    if (!pfound) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不在遊戲中！").set_flags(dpp::m_ephemeral)); return; }
                    bool is_cand = std::find(g.candidates.begin(), g.candidates.end(), uid) != g.candidates.end();
                    bool not_run = std::find(g.not_running.begin(), g.not_running.end(), uid) != g.not_running.end();
                    if (is_cand) {
                        g.candidates.erase(std::remove(g.candidates.begin(), g.candidates.end(), uid), g.candidates.end());
                        g.withdrawn_candidates.push_back(uid);
                    } else if (!not_run) {
                        g.not_running.push_back(uid);
                    }
                    int alive_cnt = 0;
                    for (auto& pl : g.players) if (pl.alive) alive_cnt++;
                    int decided = (int)g.candidates.size() + (int)g.not_running.size() + (int)g.withdrawn_candidates.size();
                    if (decided == alive_cnt) {
                        if (g.candidates.empty()) {
                            g.phase = WolfPhase::DAY_ANNOUNCE;
                            wd_action = WdAction::AUTO_SKIP;
                            dpp::embed e; e.set_title("⏭  沒有人競選，本局無警長").set_color(0x808080)
                                .set_description("所有玩家均不競選，跳過警長選舉。");
                            ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
                        } else {
                            g.speak_seats.clear(); g.speak_idx = 0;
                            for (auto cuid : g.candidates) { auto* cp = wfind(g, cuid); if (cp) g.speak_seats.push_back(cp->seat); }
                            wd_action = WdAction::AUTO_SPEECH;
                            dpp::embed ae; ae.set_title("🎤  全員決定！開始競選發言").set_color(0xF39C12)
                                .set_description("所有玩家已表達意願，候選人依序發言。");
                            ev.reply(dpp::ir_update_message, dpp::message().add_embed(ae));
                        }
                    } else {
                        ev.reply(dpp::ir_update_message, make_sheriff_nominate_msg(g));
                    }
                }
                if (wd_action == WdAction::AUTO_SKIP)        announce_night_and_start_day(bot, gid);
                else if (wd_action == WdAction::AUTO_SPEECH) start_sheriff_speech(bot, gid);
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
            // ── PK 投票（必須在 wolf_dvote_ 之前，否則被通用分支攔截）─────────
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
                bool need_advance = false; // only if withdrawer is current speaker
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
                    int pseat = p ? p->seat : -1;
                    g.candidates.erase(std::remove(g.candidates.begin(), g.candidates.end(), uid), g.candidates.end());
                    g.withdrawn_candidates.push_back(uid);
                    if (in_speech) {
                        // Find position in speak_seats before removing
                        auto sit = std::find(g.speak_seats.begin(), g.speak_seats.end(), pseat);
                        if (sit != g.speak_seats.end()) {
                            int widx = (int)(sit - g.speak_seats.begin());
                            if (widx == g.speak_idx) {
                                g.speak_idx--; // pre-decrement so advance_speaker's ++ lands on the right slot
                                need_advance = true;
                            } else if (widx < g.speak_idx) {
                                g.speak_idx--; // seat before current, shift index back
                            }
                            // widx > speak_idx: nothing to adjust
                            g.speak_seats.erase(sit);
                        }
                    } else {
                        g.speak_seats.erase(std::remove(g.speak_seats.begin(), g.speak_seats.end(), pseat), g.speak_seats.end());
                    }
                    ok = true;
                }
                if (ok) {
                    bot.message_create(dpp::message(ev.command.channel_id, "📢 **" + pname + "** 退出了警長競選！"));
                    ev.reply(dpp::ir_channel_message_with_source, dpp::message("🚪 已退出候選！").set_flags(dpp::m_ephemeral));
                    if (need_advance) advance_speaker(bot, gid);
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
        }
#endif // ── end onw/wolf button blocks ─────────────────────────────────────────
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
                    save_scratch_games();
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
                    save_scratch_games();
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
                    save_scratch_games();
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
                        save_scratch_games();
                        ev.reply(dpp::ir_update_message, make_scratch_bomb_msg(g, idx));
                    } else {
                        g.safe_scratches++;
                        {
                            std::lock_guard<std::mutex> lk(data_mutex);
                            scratch_games[uid] = g;
                        }
                        save_scratch_games();
                        ev.reply(dpp::ir_update_message, make_scratch_play_msg(g));
                    }
                }
            }
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
#if 0 // ── roulette button block moved to handlers_roulette.cpp ───────────────
        else if (cid.rfind("rl_", 0) == 0) {
            auto rl_err = [&](const std::string& msg) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message(msg).set_flags(dpp::m_ephemeral));
            };
            // ── 加入房間 ──────────────────────────────────────────────────────
            if (cid.rfind("rl_join_", 0) == 0) {
                dpp::snowflake ch(std::stoull(cid.substr(8)));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = roulette_rooms.find(ch);
                if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
                auto& r = it->second;
                if (r.started)          { rl_err("❌ 遊戲已開始！"); return; }
                if (r.p2_uid != 0)      { rl_err("❌ 房間已有兩名玩家！"); return; }
                if (uid == r.p1_uid)    { rl_err("❌ 你已是玩家一！"); return; }
                if (r.invited_uid != 0 && uid != r.invited_uid) { rl_err("❌ 此房間為邀請制！"); return; }
                std::string dn = ev.command.member.get_nickname().empty()
                               ? user.username : ev.command.member.get_nickname();
                r.p2_uid = uid; r.p2_name = dn; r.p2_avatar = user.get_avatar_url();
                ev.reply(dpp::ir_update_message, make_roulette_room_msg(r));
            }
            // ── 調整籌碼量（Modal）────────────────────────────────────────────
            else if (cid.rfind("rl_stake_", 0) == 0) {
                std::string ch_s = cid.substr(9);
                dpp::snowflake ch(std::stoull(ch_s));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = roulette_rooms.find(ch);
                    if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
                    if (uid != it->second.p1_uid)   { rl_err("❌ 只有玩家一（先手）可調整籌碼量！"); return; }
                    if (it->second.started)          { rl_err("❌ 遊戲已開始！"); return; }
                }
                std::string uid_s = std::to_string((uint64_t)uid);
                dpp::interaction_modal_response modal("rl_stake_m_" + ch_s + "_" + uid_s, "調整對賭籌碼");
                modal.add_component(dpp::component()
                    .set_type(dpp::cot_text).set_id("amount")
                    .set_label("籌碼量（正整數）")
                    .set_min_length(1).set_max_length(15)
                    .set_required(true).set_text_style(dpp::text_short));
                ev.dialog(modal);
            }
            // ── 開始遊戲 ──────────────────────────────────────────────────────
            else if (cid.rfind("rl_start_", 0) == 0) {
                dpp::snowflake ch(std::stoull(cid.substr(9)));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = roulette_rooms.find(ch);
                if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
                auto& r = it->second;
                if (uid != r.p1_uid)  { rl_err("❌ 只有玩家一（先手）可開始遊戲！"); return; }
                if (r.p2_uid == 0)    { rl_err("❌ 尚未有第二名玩家！"); return; }
                if (r.started)        { rl_err("❌ 遊戲已開始！"); return; }
                if (get_chips(r.p1_uid) < r.stake) { rl_err("❌ 玩家一籌碼不足！"); return; }
                if (get_chips(r.p2_uid) < r.stake) { rl_err("❌ 玩家二籌碼不足！"); return; }
                r.started = true;
                r.bullet_chamber = rl_rand(1, 6);
                r.current_chamber = 1;
                r.active_player = 1;
                ev.reply(dpp::ir_update_message, make_roulette_game_msg(r));
            }
            // ── 解散房間 ──────────────────────────────────────────────────────
            else if (cid.rfind("rl_dissolve_", 0) == 0) {
                dpp::snowflake ch(std::stoull(cid.substr(12)));
                std::vector<std::pair<dpp::snowflake, int64_t>> refunds;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = roulette_rooms.find(ch);
                    if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
                    if (uid != it->second.p1_uid)   { rl_err("❌ 只有玩家一可解散房間！"); return; }
                    if (it->second.started)          { rl_err("❌ 遊戲進行中無法解散！"); return; }
                    for (auto& b : it->second.side_bets) refunds.emplace_back(b.uid, b.amount);
                    roulette_rooms.erase(it);
                }
                if (!refunds.empty()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    for (auto& [bid, bamt] : refunds) chip_data[bid].chips += bamt;
                    save_chips();
                }
                dpp::embed e;
                e.set_title("🗑  房間已解散").set_color(0x808080)
                 .set_description("俄羅斯輪盤房間已解散，邊注已退還。");
                ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
            }
            // ── 旁觀者下注按鈕 ────────────────────────────────────────────────
            else if (cid.rfind("rl_bet_p1_",   0) == 0 || cid.rfind("rl_bet_p2_",   0) == 0 ||
                     cid.rfind("rl_bet_odd_",  0) == 0 || cid.rfind("rl_bet_even_", 0) == 0) {
                std::string bet_type, ch_s;
                if      (cid.rfind("rl_bet_p1_",   0) == 0) { bet_type = "p1";   ch_s = cid.substr(10); }
                else if (cid.rfind("rl_bet_p2_",   0) == 0) { bet_type = "p2";   ch_s = cid.substr(10); }
                else if (cid.rfind("rl_bet_odd_",  0) == 0) { bet_type = "odd";  ch_s = cid.substr(11); }
                else                                          { bet_type = "even"; ch_s = cid.substr(12); }
                dpp::snowflake ch(std::stoull(ch_s));
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = roulette_rooms.find(ch);
                    if (it == roulette_rooms.end()) { rl_err("❌ 房間已不存在！"); return; }
                    if (it->second.started)          { rl_err("❌ 遊戲已開始，不可下注！"); return; }
                    if (uid == it->second.p1_uid || uid == it->second.p2_uid) {
                        rl_err("❌ 參賽玩家不可下注！"); return; }
                }
                rl_open_bet_modal(ev, bet_type, ch_s);
            }
            // ── 開自己一槍 ────────────────────────────────────────────────────
            else if (cid.rfind("rl_shoot_", 0) == 0) {
                dpp::snowflake ch(std::stoull(cid.substr(9)));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = roulette_rooms.find(ch);
                if (it == roulette_rooms.end()) { rl_err("❌ 遊戲不存在！"); return; }
                auto& r = it->second;
                if (!r.started || r.game_over) { rl_err("❌ 遊戲未進行中！"); return; }
                dpp::snowflake active_uid = (r.active_player == 1) ? r.p1_uid : r.p2_uid;
                if (uid != active_uid)    { rl_err("❌ 現在不是你的回合！"); return; }
                if (rl_shoot_disabled(r)) { rl_err("❌ 已射過第 5 發，這發只能 PASS！"); return; }
                // Record shot
                r.shots_this_turn++;
                if (r.current_chamber == 5) r.shot5_shooter = r.active_player;
                // Check bullet
                if (r.current_chamber == r.bullet_chamber) {
                    r.loser = r.active_player;
                    r.game_over = true;
                    dpp::snowflake winner_uid = (r.loser == 1) ? r.p2_uid : r.p1_uid;
                    dpp::snowflake loser_uid  = (r.loser == 1) ? r.p1_uid : r.p2_uid;
                    chip_data[winner_uid].chips += r.stake;
                    chip_data[loser_uid].chips  -= r.stake;
                    for (auto& b : r.side_bets)
                        if (rl_bet_wins(b.bet_type, r.bullet_chamber, r.loser))
                            chip_data[b.uid].chips += (int64_t)(b.amount * rl_multiplier(b.bet_type));
                    save_chips();
                    dpp::message result = make_roulette_result_msg(r);
                    roulette_rooms.erase(it);
                    ev.reply(dpp::ir_update_message, result);
                    return;
                }
                // Miss — active player stays active, advance chamber
                r.current_chamber++;
                ev.reply(dpp::ir_update_message, make_roulette_game_msg(r));
            }
            // ── PASS ──────────────────────────────────────────────────────────
            else if (cid.rfind("rl_pass_", 0) == 0) {
                dpp::snowflake ch(std::stoull(cid.substr(8)));
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = roulette_rooms.find(ch);
                if (it == roulette_rooms.end()) { rl_err("❌ 遊戲不存在！"); return; }
                auto& r = it->second;
                if (!r.started || r.game_over) { rl_err("❌ 遊戲未進行中！"); return; }
                dpp::snowflake active_uid = (r.active_player == 1) ? r.p1_uid : r.p2_uid;
                if (uid != active_uid) { rl_err("❌ 現在不是你的回合！"); return; }
                if (!rl_shoot_disabled(r) && !rl_can_pass(r)) {
                    rl_err("❌ 需要先開自己一槍才能 PASS！"); return; }
                // PASS: end current player's turn, opponent starts at current chamber
                r.shots_this_turn = 0;
                r.active_player   = (r.active_player == 1) ? 2 : 1;
                ev.reply(dpp::ir_update_message, make_roulette_game_msg(r));
            }
            else {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 未知的輪盤賭按鈕！").set_flags(dpp::m_ephemeral));
            }
        }
#endif // ── end roulette button block ──────────────────────────────────────────
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
            ev.reply(dpp::ir_channel_message_with_source,
                make_bank_msg(modal_uid, notice).set_flags(dpp::m_ephemeral));
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
            ev.reply(dpp::ir_channel_message_with_source,
                make_bank_msg(modal_uid, notice).set_flags(dpp::m_ephemeral));
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
            ev.reply(dpp::ir_channel_message_with_source,
                make_bank_msg(modal_uid, notice).set_flags(dpp::m_ephemeral));
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
            ev.reply(dpp::ir_channel_message_with_source,
                make_bank_msg(modal_uid, notice).set_flags(dpp::m_ephemeral));
            return;
        }

        // 猜數字 modal
        if (cid.rfind("guess_modal_", 0) == 0) {
            dpp::snowflake owner(std::stoull(cid.substr(12)));
            if (issuer != owner) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 不是你的遊戲！").set_flags(dpp::m_ephemeral)); return;
            }
            std::string input;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    input = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        input = std::get<std::string>(sub.value);
            }
            // trim
            while (!input.empty() && input.front() == ' ') input.erase(input.begin());
            while (!input.empty() && input.back()  == ' ') input.pop_back();

            if (!guess_valid(input)) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 請輸入 4 位不重複的數字（0-9），例如 `1023`！").set_flags(dpp::m_ephemeral)); return;
            }

            bool game_over = false, won = false;
            GuessGame snap;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = guess_games.find(owner);
                if (it == guess_games.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 找不到進行中的遊戲！").set_flags(dpp::m_ephemeral)); return;
                }
                auto& g = it->second;
                g.input_buf = "";
                std::string ab = guess_calc_ab(g.secret, input);
                g.history.emplace_back(input, ab);
                g.attempts++;
                won      = (ab == "4A0B");
                game_over = won || (g.attempts >= GuessGame::MAX_ATTEMPTS);
                if (game_over) {
                    auto& st = guess_stats_data[(uint64_t)owner];
                    st.games++;
                    if (won) { st.wins++; st.total_win_attempts += g.attempts; }
                }
                snap = g;
                if (game_over) guess_games.erase(it);
            }

            if (game_over) {
                save_guess_stats();
                // edit original game message to result
                auto result = make_guess_result_msg(snap, won);
                result.id = snap.msg_id; result.channel_id = snap.channel_id;
                g_bot->message_edit(result);
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message(won ? "🎉 猜中了！" : "💀 失敗！").set_flags(dpp::m_ephemeral));
            } else {
                // edit game message with updated history
                auto upd = make_guess_msg(snap);
                upd.id = snap.msg_id; upd.channel_id = snap.channel_id;
                g_bot->message_edit(upd);
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("✅ 已記錄：`" + input + "` → **" + snap.history.back().second + "**").set_flags(dpp::m_ephemeral));
            }
            return;
        }

        // Undercover modals → handlers_uc.cpp
        if (cid.rfind("uc_answer_modal_", 0) == 0 || cid.rfind("uc_guess_modal_", 0) == 0) {
            handle_uc_modal(ev); return;
        }
#if 0 // ── UC modal blocks moved to handlers_uc.cpp ──────────────────────────────
        // Undercover answer modal (describing phase)
        if (cid.rfind("uc_answer_modal_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(16));
            std::string answer;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    answer = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        answer = std::get<std::string>(sub.value);
            }
            while (!answer.empty() && (answer.front()==' '||answer.front()=='\t')) answer.erase(answer.begin());
            while (!answer.empty() && (answer.back()==' '||answer.back()=='\r'||answer.back()=='\n')) answer.pop_back();

            bool all_done = false; bool ok = false;
            std::string speaker_name;
            UCGame snap;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end() || it->second.phase != UCPhase::DESCRIBING) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("⚠️ 不是發言時間！").set_flags(dpp::m_ephemeral)); return;
                }
                auto& g = it->second;
                if (g.speak_pos >= (int)g.speak_order.size() || g.speak_order[g.speak_pos] != issuer) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 不是你的發言時間！").set_flags(dpp::m_ephemeral)); return;
                }
                for (auto& p : g.players)
                    if (p.uid == issuer) { speaker_name = p.display_name; break; }
                g.answers[issuer] = answer;
                g.speak_pos++;
                all_done = (g.speak_pos >= (int)g.speak_order.size());
                if (all_done) g.phase = UCPhase::VOTING;
                snap = g;
                ok = true;
            }
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 回答已送出！").set_flags(dpp::m_ephemeral));
            g_bot->message_delete(snap.describe_msg_id, snap.channel_id);
            if (all_done) {
                auto ans_msg = uc_all_answers_msg(snap);
                ans_msg.channel_id = snap.channel_id;
                g_bot->message_create(ans_msg);
                auto vmsg = uc_vote_msg(snap);
                vmsg.channel_id = snap.channel_id;
                g_bot->message_create(vmsg, [gid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        if (uc_games.count(gid))
                            uc_games[gid].vote_msg_id = std::get<dpp::message>(cb.value).id;
                    }
                });
            } else {
                auto new_desc = uc_describe_msg(snap);
                new_desc.channel_id = snap.channel_id;
                g_bot->message_create(new_desc, [gid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        if (uc_games.count(gid))
                            uc_games[gid].describe_msg_id = std::get<dpp::message>(cb.value).id;
                    }
                });
            }
            return;
        }

        // Undercover guess modal
        if (cid.rfind("uc_guess_modal_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(15));
            std::string guess;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value))
                    guess = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value))
                        guess = std::get<std::string>(sub.value);
            }
            // Trim
            while (!guess.empty() && (guess.front()==' '||guess.front()=='\t')) guess.erase(guess.begin());
            while (!guess.empty() && (guess.back()==' '||guess.back()=='\r'||guess.back()=='\n')) guess.pop_back();

            dpp::snowflake elim_uid = 0;
            dpp::snowflake ch_id;
            std::string civ_word, elim_name;
            bool found = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it != uc_games.end()) {
                    auto& g = it->second;
                    elim_uid = g.pending_elim;
                    ch_id    = g.channel_id;
                    civ_word = g.civilian_word;
                    if (elim_uid && elim_uid == issuer) {
                        found = true;
                        for (auto& p : g.players)
                            if (p.uid == elim_uid) { elim_name = p.display_name; break; }
                        if (g.guess_timer) { g_bot->stop_timer(g.guess_timer); g.guess_timer = 0; }
                        g.pending_elim = 0;
                    }
                }
            }
            if (!found) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("⏰ 猜詞時間已過！").set_flags(dpp::m_ephemeral));
                return;
            }
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 猜詞已送出！").set_flags(dpp::m_ephemeral));
            bool correct = (guess == civ_word);
            dpp::message result_m;
            result_m.channel_id = ch_id;
            if (correct) {
                result_m.set_content("💥 **" + elim_name + "** 猜對了！詞是「**" + civ_word + "**」！翻盤勝利！");
                g_bot->message_create(result_m);
                uc_end_game(gid, false, true);
            } else {
                result_m.set_content("❌ **" + elim_name + "** 猜錯了！「" + guess + "」不對，正式淘汰！");
                g_bot->message_create(result_m);
                uc_do_eliminate_confirmed(gid, elim_uid);
            }
            return;
        }
#endif // ── end UC modal blocks ────────────────────────────────────────────────

        // ── 輪盤賭 Modal → handlers_roulette.cpp ──────────────────────────────
        if (cid.rfind("rl_stake_m_", 0) == 0 || cid.rfind("rl_bet_m_", 0) == 0) {
            handle_roulette_modal(ev); return;
        }
#if 0 // ── roulette modal blocks moved to handlers_roulette.cpp ───────────────
        if (cid.rfind("rl_stake_m_", 0) == 0) {
            // ID: rl_stake_m_{ch}_{uid}
            std::string rest_s = cid.substr(11);
            size_t up = rest_s.rfind('_');
            if (up == std::string::npos) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 格式錯誤！").set_flags(dpp::m_ephemeral)); return; }
            dpp::snowflake ch_s(std::stoull(rest_s.substr(0, up)));
            dpp::snowflake modal_uid(std::stoull(rest_s.substr(up + 1)));
            if (issuer != modal_uid) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 非你操作！").set_flags(dpp::m_ephemeral)); return; }
            std::string val;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value)) val = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value)) val = std::get<std::string>(sub.value);
            }
            int64_t new_stake = 0;
            try { new_stake = std::stoll(val); } catch (...) {}
            if (new_stake <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 籌碼必須是正整數！").set_flags(dpp::m_ephemeral)); return; }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = roulette_rooms.find(ch_s);
                if (it == roulette_rooms.end() || it->second.started) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 房間已不存在或遊戲已開始！").set_flags(dpp::m_ephemeral)); return; }
                if (get_chips(issuer) < new_stake) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你的籌碼不足！").set_flags(dpp::m_ephemeral)); return; }
                it->second.stake = new_stake;
                if (it->second.msg_id) {
                    auto upd = make_roulette_room_msg(it->second);
                    upd.id = it->second.msg_id; upd.channel_id = ch_s;
                    g_bot->message_edit(upd);
                }
            }
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 籌碼已更新為 **" + std::to_string(new_stake) + "** 碼！").set_flags(dpp::m_ephemeral));
            return;
        }
        // ── 輪盤賭：旁觀者下注 Modal ──────────────────────────────────────────
        if (cid.rfind("rl_bet_m_", 0) == 0) {
            // ID: rl_bet_m_{type}_{ch}_{uid}
            std::string rest_s = cid.substr(9);
            size_t p1 = rest_s.find('_');
            if (p1 == std::string::npos) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 格式錯誤！").set_flags(dpp::m_ephemeral)); return; }
            std::string bet_type = rest_s.substr(0, p1);
            std::string rest2 = rest_s.substr(p1 + 1);
            size_t p2 = rest2.rfind('_');
            if (p2 == std::string::npos) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 格式錯誤！").set_flags(dpp::m_ephemeral)); return; }
            dpp::snowflake ch_s(std::stoull(rest2.substr(0, p2)));
            dpp::snowflake modal_uid(std::stoull(rest2.substr(p2 + 1)));
            if (issuer != modal_uid) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 非你操作！").set_flags(dpp::m_ephemeral)); return; }
            std::string val;
            for (auto& row : ev.components) {
                if (std::holds_alternative<std::string>(row.value)) val = std::get<std::string>(row.value);
                for (auto& sub : row.components)
                    if (std::holds_alternative<std::string>(sub.value)) val = std::get<std::string>(sub.value);
            }
            int64_t bet_amt = 0;
            try { bet_amt = std::stoll(val); } catch (...) {}
            if (bet_amt <= 0) { ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 下注金額必須是正整數！").set_flags(dpp::m_ephemeral)); return; }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = roulette_rooms.find(ch_s);
                if (it == roulette_rooms.end() || it->second.started) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 房間已不存在或遊戲已開始！").set_flags(dpp::m_ephemeral)); return; }
                if (issuer == it->second.p1_uid || issuer == it->second.p2_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 參賽玩家不可下注！").set_flags(dpp::m_ephemeral)); return; }
                if (get_chips(issuer) < bet_amt) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 籌碼不足！").set_flags(dpp::m_ephemeral)); return; }
                chip_data[issuer].chips -= bet_amt;
                std::string dn_bet = ev.command.member.get_nickname().empty()
                                   ? ev.command.get_issuing_user().username
                                   : ev.command.member.get_nickname();
                RouletteSideBet sb; sb.uid = issuer; sb.display_name = dn_bet;
                sb.bet_type = bet_type; sb.amount = bet_amt;
                it->second.side_bets.push_back(sb);
                save_chips();
                if (it->second.msg_id) {
                    auto upd = make_roulette_room_msg(it->second);
                    upd.id = it->second.msg_id; upd.channel_id = ch_s;
                    g_bot->message_edit(upd);
                }
            }
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 已下注 **" + std::to_string(bet_amt) + "** 碼（" + rl_bet_label(bet_type) + "）！").set_flags(dpp::m_ephemeral));
            return;
        }
#endif // ── end roulette modal blocks ──────────────────────────────────────────

        // Adventure funds modal
        if (cid.rfind("adv_funds_modal_", 0) == 0) {
            dpp::snowflake modal_uid(std::stoull(cid.substr(16)));
            if (issuer != modal_uid) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 這不是你的操作！").set_flags(dpp::m_ephemeral)); return;
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
            int64_t requested = amount;
            amount = std::max((int64_t)0, std::min((int64_t)10000, amount));
            int64_t chips = get_chips(modal_uid);
            std::string notice;
            if (amount > chips) {
                amount = chips;
                notice = "⚠️ 你輸入的 " + std::to_string(requested) + " 碼超過錢包餘額，已自動調整為 **" + std::to_string(amount) + "** 碼。";
            }
            { std::lock_guard<std::mutex> lk(data_mutex); adv_setups[modal_uid].funds = amount; }
            std::string mdn = ev.command.member.get_nickname();
            if (mdn.empty()) mdn = ev.command.get_issuing_user().global_name.empty()
                                 ? ev.command.get_issuing_user().username
                                 : ev.command.get_issuing_user().global_name;
            std::string mav = ev.command.get_issuing_user().get_avatar_url();
            ev.reply(dpp::ir_update_message, make_adv_setup_msg(modal_uid, mdn, mav, notice));
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
            // fields: [target_uid, item_key_or_id, qty]
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
            if (qty <= 0 || qty > 999) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 數量必須在 1~999 之間！").set_flags(dpp::m_ephemeral)); return;
            }
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[target_uid][key] += qty;
            }
            save_inventory();
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
        // ── 誰是臥底 select → handlers_uc.cpp ───────────────────────────────
        else if (cid.rfind("uc_pool_", 0) == 0) {
            handle_uc_select(ev, uid); return;
        }
#if 0 // ── UC select blocks moved to handlers_uc.cpp ────────────────────────────
        else if (cid.rfind("uc_pool_", 0) == 0) {
            if (ev.values.empty()) return;
            uint64_t gid = std::stoull(cid.substr(8));
            std::string new_pool = ev.values[0];
            dpp::snowflake host_id = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = uc_games.find(gid);
                if (it == uc_games.end() || it->second.phase != UCPhase::WAITING) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 遊戲不存在或已開始！").set_flags(dpp::m_ephemeral));
                    return;
                }
                host_id = it->second.host_id;
                if (uid != host_id) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 只有主持人可以切換題庫！").set_flags(dpp::m_ephemeral));
                    return;
                }
                if (new_pool == "adult" && !it->second.adult_allowed) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("🔞 你還想玩成人內容阿 小色鬼").set_flags(dpp::m_ephemeral));
                    return;
                }
                it->second.word_pool = new_pool;
            }
            UCGame snap;
            { std::lock_guard<std::mutex> lk(data_mutex); auto it = uc_games.find(gid); if (it != uc_games.end()) snap = it->second; }
            ev.reply(dpp::ir_update_message, uc_lobby_msg(snap));
        }
#endif // ── end UC select blocks ─────────────────────────────────────────────
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
#if 0 // ── roulette select block moved to handlers_roulette.cpp ────────────────
        else if (cid.rfind("rl_ch_sel_", 0) == 0) {
            if (ev.values.empty()) return;
            std::string ch_s = cid.substr(10);
            dpp::snowflake ch(std::stoull(ch_s));
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = roulette_rooms.find(ch);
                if (it == roulette_rooms.end()) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 房間已不存在！").set_flags(dpp::m_ephemeral)); return; }
                if (it->second.started) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 遊戲已開始，不可下注！").set_flags(dpp::m_ephemeral)); return; }
                if (uid == it->second.p1_uid || uid == it->second.p2_uid) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 參賽玩家不可下注！").set_flags(dpp::m_ephemeral)); return; }
            }
            std::string chamber_num = ev.values[0];  // "1"-"6"
            std::string bet_type = "ch" + chamber_num;
            std::string uid_s = std::to_string((uint64_t)uid);
            dpp::interaction_modal_response modal(
                "rl_bet_m_" + bet_type + "_" + ch_s + "_" + uid_s,
                "下注：第" + chamber_num + "發 ×5.5");
            modal.add_component(dpp::component()
                .set_type(dpp::cot_text).set_id("amount")
                .set_label("下注金額（籌碼）")
                .set_min_length(1).set_max_length(15)
                .set_required(true).set_text_style(dpp::text_short));
            ev.dialog(modal);
        }
#endif // ── end roulette select block ──────────────────────────────────────────
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
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = inventory_data.find(uid);
              if (it != inventory_data.end()) inv = it->second;
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
        else if (cmd_name == "幫助" || cmd_name == "help") {
            ev.reply(dpp::ir_channel_message_with_source, make_help_msg(0));
        }
        else if (cmd_name == "領取" || cmd_name == "claim") {
            bool claimed = false;
            dpp::message m = handle_claim(uid, &claimed);
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
        else if (cmd_name == "大廳" || cmd_name == "lobby") {
            ev.reply(dpp::ir_channel_message_with_source, make_lobby_msg(uid,
                user.get_avatar_url(),
                ev.command.member.get_nickname()));
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
        else if (cmd_name == "收藏" || cmd_name == "collect") {
            ev.reply(dpp::ir_channel_message_with_source, make_collection_msg(uid, ev.command.member.get_nickname(), user.get_avatar_url()));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[std::get<dpp::message>(cb.value).id] = uid; }
            });
        }
        else if (cmd_name == "探險" || cmd_name == "adventure") {
            ev.reply(dpp::ir_channel_message_with_source, make_adv_main_msg(uid, ev.command.member.get_nickname(), user.get_avatar_url()));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[std::get<dpp::message>(cb.value).id] = uid; }
            });
        }
        else if (cmd_name == "強化" || cmd_name == "enhance") {
            ev.reply(dpp::ir_channel_message_with_source, make_enhance_main_msg(uid, ev.command.member.get_nickname(), user.get_avatar_url()));
            ev.get_original_response([uid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) { std::lock_guard<std::mutex> lk(data_mutex); msg_owner[std::get<dpp::message>(cb.value).id] = uid; }
            });
        }
        else if (cmd_name == "猜拳" || cmd_name == "janken") {
            handle_rps_slash(ev, uid, ch);
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
        else if (cmd_name == "骰子" || cmd_name == "dice") {
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
        // ── 21點 slash → handlers_bj.cpp ─────────────────────────────────────
        else if (cmd_name == "21" || cmd_name == "blackjack") {
            handle_bj_slash(ev, cmd_name, uid, ch); return;
        }
#if 0 // ── BJ slash handler moved to handlers_bj.cpp ──────────────────────────
        else if (cmd_name == "21" || cmd_name == "blackjack") {
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
                BJGame old_g; bool had_old = false;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = user_bj.find(uid);
                    if (it != user_bj.end()) {
                        auto git = bj_games.find(it->second);
                        if (git != bj_games.end()) { old_g = git->second; had_old = true; }
                        bj_games.erase(it->second); user_bj.erase(it);
                    }
                }
                if (had_old) bj_disable_old_msg(bot, old_g);
            }
            add_chips(uid, -bet);
            {
                const dpp::user& u = ev.command.get_issuing_user();
                BJGame g = start_bj(uid, ch, bet, u.get_avatar_url(), u.username);
                std::string status;
                if (is_blackjack(g.main_hand.cards)) {
                    bool dbj = is_blackjack(g.dealer_cards);
                    g.game_over = true;
                    if (dbj) {
                        add_chips(uid, bet);
                        status = "雙 BJ — 平局！";
                        { std::lock_guard<std::mutex> lk(data_mutex); bj_stats_data[uid].pushes++; }
                    } else {
                        int64_t win = (int64_t)(bet * 1.5);
                        add_chips(uid, bet + win);
                        status = "🌟 Blackjack！贏得 **" + std::to_string(win) + "** 碼！";
                        { std::lock_guard<std::mutex> lk(data_mutex); bj_stats_data[uid].wins++; bj_stats_data[uid].profit += win; }
                    }
                    save_bjstats();
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
#endif // ── end BJ slash handler ─────────────────────────────────────────────
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
            int64_t from_chips = get_int("我的籌碼");
            int to_item_id   = (int)get_int("對方道具");
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
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = inventory_data[uid].find(from_key2);
                if (it == inventory_data[uid].end() || it->second <= 0) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("❌ 你沒有 **" + from_iname2 + "**！").set_flags(dpp::m_ephemeral)); return;
                }
            }
            if (from_chips < 0) from_chips = 0;
            if (from_chips > 0) {
                int64_t from_fee_chk2 = (from_chips + 99) / 100;
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
            TradeOffer t;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                t.id = trade_counter++;
                t.from_uid = uid; t.to_uid = target; t.channel_id = ch;
                t.from_item_id = from_item_id; t.from_chips = from_chips;
                t.to_item_id   = to_item_id;   t.to_chips   = to_chips;
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
#if 0 // ── wolf/onenight slash blocks moved to handlers_wolf.cpp ──────────────
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
        else if (cmd_name == "一夜狼人" || cmd_name == "onenight") {
            bool already = false;
            uint64_t gid = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (channel_onw_game.count(ch)) { already = true; }
                else {
                    gid = onw_counter++;
                    ONWGame g;
                    g.id = gid; g.channel_id = ch;
                    g.guild_id = ev.command.guild_id; g.host_id = uid;
                    // Default role pool for 4 players (7 cards)
                    g.role_counts = {{"狼人",2},{"預言家",1},{"強盜",1},{"搗蛋鬼",1},{"酒鬼",1},{"村民",1}};
                    // Add host as first player
                    ONWPlayer hp;
                    hp.uid = uid; hp.display_name = ev.command.member.get_nickname().empty()
                        ? ev.command.usr.username : ev.command.member.get_nickname();
                    g.players.push_back(hp);
                    onw_games[gid] = g;
                    channel_onw_game[ch] = gid;
                }
            }
            if (already) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此頻道已有進行中的一夜狼人遊戲！").set_flags(dpp::m_ephemeral));
                return;
            }
            dpp::message m;
            { std::lock_guard<std::mutex> lk(data_mutex); m = make_onw_lobby_msg(onw_games[gid]); }
            ev.reply(dpp::ir_channel_message_with_source, m);
            ev.get_original_response([gid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = onw_games.find(gid);
                    if (it != onw_games.end())
                        it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
#endif // ── end wolf/onenight slash blocks ─────────────────────────────────────
        // ── 誰是臥底 slash → handlers_uc.cpp ─────────────────────────────────
        else if (cmd_name == "臥底" || cmd_name == "誰是臥底" || cmd_name == "undercover") {
            handle_uc_slash(ev, cmd_name, uid, ch); return;
        }
#if 0 // ── UC slash blocks moved to handlers_uc.cpp ─────────────────────────────
        else if (cmd_name == "臥底" || cmd_name == "誰是臥底" || cmd_name == "undercover") {
            bool already = false;
            uint64_t gid = 0;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (channel_uc_game.count(ch)) { already = true; }
                else {
                    gid = uc_counter++;
                    UCGame g;
                    g.id = gid; g.channel_id = ch;
                    g.guild_id = ev.command.guild_id; g.host_id = uid;
                    UCPlayer hp;
                    hp.uid = uid; hp.seat = 0;
                    hp.display_name = ev.command.member.get_nickname().empty()
                        ? ev.command.usr.username : ev.command.member.get_nickname();
                    g.players.push_back(hp);
                    uc_games[gid] = g;
                    channel_uc_game[ch] = gid;
                }
            }
            if (already) {
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message("❌ 此頻道已有進行中的誰是臥底遊戲！").set_flags(dpp::m_ephemeral));
                return;
            }
            dpp::message m;
            { std::lock_guard<std::mutex> lk(data_mutex); m = uc_lobby_msg(uc_games[gid]); }
            ev.reply(dpp::ir_channel_message_with_source, m);
            ev.get_original_response([gid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = uc_games.find(gid);
                    if (it != uc_games.end())
                        it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
#endif // ── end UC slash blocks ──────────────────────────────────────────────
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
            trade_cmd.add_option(dpp::command_option(dpp::co_integer, "我的籌碼", "我出的籌碼（0=無）",  false))
                     .add_option(dpp::command_option(dpp::co_integer, "對方道具", "要對方出的道具ID（0=無）", false))
                     .add_option(dpp::command_option(dpp::co_integer, "對方籌碼", "要對方出的籌碼（0=無）",  false));

            dpp::slashcommand trade_en("trade", "Propose an item/chip trade with another player", bot.me.id);
            trade_en.add_option(dpp::command_option(dpp::co_user, "對象", "Trade target", true));
            { auto p = dpp::command_option(dpp::co_string, "我的道具", "Your item (pick from list or type ID)", false);
              p.set_auto_complete(true); trade_en.add_option(p); }
            trade_en.add_option(dpp::command_option(dpp::co_integer, "我的籌碼", "Your chips (0=none)",   false))
                    .add_option(dpp::command_option(dpp::co_integer, "對方道具", "Their item ID (0=none)",false))
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
        apply_daily_interest(); // 啟動時補算可能錯過的利息
        bot.start_timer([](dpp::timer)     { cleanup_expired(); },  3600);
        bot.start_timer([&bot](dpp::timer) { check_giveaways(bot); save_giveaways(); }, 30);
        bot.start_timer([](dpp::timer)     { apply_daily_interest(); }, 300); // 每 5 分鐘檢查是否跨日

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
                    if (pet.talent == "招人喜歡") reward = (int64_t)(reward * 1.1);
                    if (pet.is_supervisor_work) reward = (int64_t)(reward * 0.6);
                    chip_data[uid].chips += reward;
                    changed_chips = true;
                    if (pet.stage < 3)
                        pet.exp = std::min(pet.exp + exp_gain, exp_needed(pet.stage));
                    else
                        pet.exp += exp_gain;
                    // 再派（監工出勤，領取時收益 ×0.6）
                    int dur_sec = task * 3600;
                    if (pet.talent == "迅捷") dur_sec = (int)(dur_sec * 0.9);
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
                for (auto nuid : to_notify_work) {
                    g_bot->create_dm_channel(nuid, [nuid](const dpp::confirmation_callback_t& cb) {
                        if (cb.is_error()) return;
                        dpp::channel ch = std::get<dpp::channel>(cb.value);
                        g_bot->message_create(dpp::message(ch.id,
                            "🐾 你的寵物打工完成了！快輸入 `!寵物` 去領取獎勵吧！"));
                    });
                }
                for (auto nuid : to_notify_onsen) {
                    g_bot->create_dm_channel(nuid, [nuid](const dpp::confirmation_callback_t& cb) {
                        if (cb.is_error()) return;
                        dpp::channel ch = std::get<dpp::channel>(cb.value);
                        g_bot->message_create(dpp::message(ch.id,
                            "🛀 你的寵物溫泉回來了！負面狀態已全部清除，快輸入 `!寵物` 去看看牠吧！"));
                    });
                }
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
                for (auto nuid : to_notify_adv) {
                    g_bot->create_dm_channel(nuid, [nuid](const dpp::confirmation_callback_t& cb) {
                        if (cb.is_error()) return;
                        dpp::channel ch = std::get<dpp::channel>(cb.value);
                        g_bot->message_create(dpp::message(ch.id,
                            "🗺️ 你的探險完成了！快輸入 `!探險` 去收取結果吧！"));
                    });
                }
            }
        }, 300);

        printf("Bot 已上線：%s\n", bot.me.username.c_str());
    });

    bot.start(dpp::st_wait);
    return 0;
}
