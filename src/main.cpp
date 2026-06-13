#include "team.h"
#include "giveaway.h"

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

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    cfg = load_config();
    if (cfg.token.empty()) { fprintf(stderr, "找不到 BOT_TOKEN\n"); return 1; }
    { FILE* f = fopen("C:\\bot_debug.txt","a"); if(f){fprintf(f,"bot starting, token ok\n");fclose(f);} }

    dpp::cluster bot(cfg.token, dpp::i_default_intents | dpp::i_message_content);
    bot.on_log(dpp::utility::cout_logger());

    // ── 訊息指令 ──────────────────────────────────────────────────────────────
    bot.on_message_create([&bot](const dpp::message_create_t& ev) {
        const std::string& content = ev.msg.content;
        dpp::snowflake     uid     = ev.msg.author.id;
        dpp::snowflake     ch      = ev.msg.channel_id;

        if (content == "!王團報名") {
            start_cmd(bot, uid, ch, make_boss_msg(ev.msg.author), ev.msg.id);
        }
        else if (content == "!王團紀錄") {
            start_cmd(bot, uid, ch, make_records_select_msg(ev.msg.author), ev.msg.id);
        }
        else if (content == "!ping") {
            ev.reply("Pong! 🏓");
        }
        // !抽獎 時間 人數 獎品  (e.g.  !抽獎 5h 4 3張突襲)
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
                dpp::message gw_msg = make_giveaway_msg(gw);
                gw_msg.set_reference(ev.msg.id);
                bot.message_create(gw_msg, [gid = gw.id](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        auto& m = std::get<dpp::message>(cb.value);
                        std::lock_guard<std::mutex> lk(data_mutex);
                        giveaways[gid].msg_id     = m.id;
                        giveaways[gid].channel_id = m.channel_id;
                    }
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

        // ── 選王 ──────────────────────────────────────────────────────────────
        if (cid.rfind("boss_", 0) == 0) {
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
            ev.reply(dpp::ir_update_message, make_success_msg(reg));
            check_team_formation(bot, reg.boss, reg.channel_id);
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
                bot.message_create(ann_msg);
            } else {
                dpp::embed cancel_e;
                cancel_e.set_title("❌  組隊已撤銷").set_color(0x808080);
                cancel_e.add_field("⚔️  王",   pt.boss,                       true);
                cancel_e.add_field("🕐  時間", pt.day + "  " + pt.time_slot,  true);
                dpp::message cancel_msg; cancel_msg.add_embed(cancel_e);
                ev.reply(dpp::ir_update_message, cancel_msg);
            }
        }
        // ── 抽獎：加入 ────────────────────────────────────────────────────────
        else if (cid.rfind("giveaway_join_", 0) == 0 || cid.rfind("giveaway_leave_", 0) == 0) {
            bool is_join = cid.rfind("giveaway_join_", 0) == 0;
            uint64_t gid = std::stoull(cid.substr(is_join ? 14 : 15));
            bool role_ok = true, over = false, already_in = false;
            Giveaway gw;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = giveaways.find(gid);
                if (it == giveaways.end()) {
                    dpp::embed e; e.set_title("⚠️  抽獎不存在").set_color(0x808080);
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message().add_embed(e).set_flags(dpp::m_ephemeral));
                    return;
                }
                if (it->second.ended) { over = true; }
                else {
                    if (it->second.role_restriction) {
                        role_ok = false;
                        for (auto& rid : ev.command.member.get_roles())
                            if (rid == it->second.role_restriction) { role_ok = true; break; }
                    }
                    if (role_ok) {
                        auto& p = it->second.participants;
                        already_in = p.count(uid) > 0;
                        if (is_join)  p.insert(uid);
                        else          p.erase(uid);
                        gw = it->second;
                    }
                }
            }
            if (over) {
                dpp::embed e; e.set_title("⚠️  抽獎已結束").set_color(0x808080);
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral));
                return;
            }
            if (!role_ok) {
                dpp::embed e; e.set_title("❌  你沒有參加此抽獎的資格").set_color(0xE74C3C);
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral));
                return;
            }
            if (is_join && already_in) {
                dpp::embed e; e.set_title("ℹ️  你已在報名名單中").set_color(0x3498DB);
                e.set_description("如要取消報名請按 **↩ 取消報名** 按鈕。");
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral));
                return;
            }
            if (!is_join && !already_in) {
                dpp::embed e; e.set_title("ℹ️  你尚未報名此抽獎").set_color(0x3498DB);
                ev.reply(dpp::ir_channel_message_with_source,
                    dpp::message().add_embed(e).set_flags(dpp::m_ephemeral));
                return;
            }
            // Update public giveaway message (participant count changed)
            dpp::message updated = make_giveaway_msg(gw);
            updated.id = gw.msg_id; updated.channel_id = gw.channel_id;
            bot.message_edit(updated);
            // Ephemeral embed response
            dpp::embed fb;
            if (is_join) fb.set_title("✅  報名成功！").set_color(0x2ECC71)
                           .set_description("你已加入抽獎 **" + gw.prize + "**！\n抽獎結束時會 @通知中獎者。");
            else         fb.set_title("↩  已取消報名").set_color(0x808080)
                           .set_description("你已從抽獎 **" + gw.prize + "** 中退出。");
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message().add_embed(fb).set_flags(dpp::m_ephemeral));
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
    });

    // ── 斜線指令 ──────────────────────────────────────────────────────────────
    bot.on_slashcommand([&bot](const dpp::slashcommand_t& ev) {
        const std::string  cmd_name = ev.command.get_command_name();
        { FILE* f = fopen("C:\\bot_debug.txt","a"); if(f){fprintf(f,"slash: [%s]\n",cmd_name.c_str());fclose(f);} }
        const dpp::user&   user     = ev.command.get_issuing_user();
        dpp::snowflake     uid      = user.id;
        dpp::snowflake     ch       = ev.command.channel_id;

        if (cmd_name == "王團報名" || cmd_name == "王團紀錄") {
            invalidate_old_msg(bot, uid);
            dpp::message m = (cmd_name == "王團報名")
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
                giveaways[gw.id]    = gw;
            }

            // Confirm to caller (ephemeral)
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("✅ 抽獎已建立！").set_flags(dpp::m_ephemeral));

            // Post the giveaway in the target channel
            bot.message_create(make_giveaway_msg(gw),
                [gid = gw.id](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        auto& m = std::get<dpp::message>(cb.value);
                        std::lock_guard<std::mutex> lk(data_mutex);
                        giveaways[gid].msg_id     = m.id;
                        giveaways[gid].channel_id = m.channel_id;
                    }
                });
        }
    });

    // ── on_ready ──────────────────────────────────────────────────────────────
    bot.on_ready([&bot](const dpp::ready_t&) {
        { FILE* f = fopen("C:\\bot_debug.txt","a"); if(f){fprintf(f,"on_ready fired\n");fclose(f);} }
        if (dpp::run_once<struct register_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping",    "測試機器人是否在線", bot.me.id));
            bot.global_command_create(dpp::slashcommand("王團報名", "王團報名",           bot.me.id));
            bot.global_command_create(dpp::slashcommand("王團紀錄", "查看王團報名紀錄",   bot.me.id));

            dpp::slashcommand draw("抽獎", "開始抽獎（需要管理員或副會長）", bot.me.id);
            draw.add_option(dpp::command_option(dpp::co_string,  "時間",      "抽獎時長，例: 5h 30m", true))
                .add_option(dpp::command_option(dpp::co_integer, "獲獎人數",  "中獎人數",             true))
                .add_option(dpp::command_option(dpp::co_string,  "獎品名稱",  "獎品名稱",             true))
                .add_option(dpp::command_option(dpp::co_channel, "抽獎頻道",  "在哪個頻道發起抽獎",   true))
                .add_option(dpp::command_option(dpp::co_role,    "限制身分組","限制特定身分組才能參加",false))
                .add_option(dpp::command_option(dpp::co_user,    "提供者",    "獎品提供者",           false))
                .add_option(dpp::command_option(dpp::co_user,    "提及",      "特別提及的對象",        false))
                .add_option(dpp::command_option(dpp::co_string,  "備註",      "備註說明",             false));
            bot.global_command_create(draw);
        }

        cleanup_expired();
        bot.start_timer([](dpp::timer)         { cleanup_expired(); },  3600); // hourly
        bot.start_timer([&bot](dpp::timer)     { check_giveaways(bot); }, 30); // every 30 s
        printf("Bot 已上線：%s\n", bot.me.username.c_str());
    });

    bot.start(dpp::st_wait);
    return 0;
}
