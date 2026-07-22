#include "types.h"
#include "chips.h"
#include "werewolf.h"
#include "onenight.h"
#include "wolfplayerstats.h"
#include "onwstats.h"
#include "handler_decls.h"

// ─── Message handler ──────────────────────────────────────────────────────────

void handle_wolf_message(const dpp::message_create_t& ev, const std::string& content,
                         dpp::snowflake uid, dpp::snowflake ch)
{
    if (content == "!狼人殺" || content == "！狼人殺") {
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
            m.channel_id = ch; g_bot->message_create(m); return;
        }
        dpp::message m;
        { std::lock_guard<std::mutex> lk(data_mutex); m = make_wolf_lobby_msg(wolf_games[gid]); }
        m.channel_id = ch; g_bot->message_create(m);
    }
    else if (content == "!偷看" || content == "！偷看") {
        if (cfg.notify_user_id.empty() || std::to_string(uid) != cfg.notify_user_id) return;
        g_bot->message_delete(ev.msg.id, ch);
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
        g_bot->direct_message_create(uid, dpp::message(dm));
    }
    else if (content == "!狼人殺榜單" || content == "！狼人殺榜單") {
        ev.reply(make_wolf_leaderboard_msg());
    }
    else if (content == "!一夜狼人" || content == "！一夜狼人") {
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
            m.channel_id = ch; g_bot->message_create(m); return;
        }
        dpp::message m;
        { std::lock_guard<std::mutex> lk(data_mutex); m = make_onw_lobby_msg(onw_games[gid]); }
        m.channel_id = ch;
        g_bot->message_create(m, [gid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = onw_games.find(gid);
                if (it != onw_games.end())
                    it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
            }
        });
    }
    else if (content == "!一夜狼人規則" || content == "！一夜狼人規則") {
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
        g_bot->message_create(m);
    }
    else if (content == "!狼人殺規則" || content == "！狼人殺規則") {
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
        g_bot->message_create(m);
    }
}

// ─── Slash handler ────────────────────────────────────────────────────────────

void handle_wolf_slash(const dpp::slashcommand_t& ev, const std::string& cmd_name,
                       dpp::snowflake uid, dpp::snowflake ch)
{
    if (cmd_name == "狼人殺" || cmd_name == "werewolf") {
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
                g.role_counts = {{"狼人",2},{"預言家",1},{"強盜",1},{"搗蛋鬼",1},{"酒鬼",1},{"村民",1}};
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
}

// ─── Button handler ───────────────────────────────────────────────────────────

void handle_wolf_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;

    // ── 一夜狼人「再來一場」────────────────────────────────────────────────────
    if (cid.rfind("onw_again_", 0) == 0) {
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
            g_bot->message_create(m, [new_gid](const dpp::confirmation_callback_t& cb) {
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = onw_games.find(new_gid);
                    if (it != onw_games.end())
                        it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        }
    }
    // ── 一夜狼人按鈕 ─────────────────────────────────────────────────────────
    else if (cid.rfind("onw_", 0) == 0) {
        dpp::snowflake ev_ch = ev.command.channel_id;
        uint64_t gid = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = channel_onw_game.find(ev_ch);
            if (it != channel_onw_game.end()) {
                gid = it->second;
            } else {
                for (auto& [id, g] : onw_games)
                    for (auto& p : g.players)
                        if (p.uid == uid) { gid = id; break; }
            }
        }
        if (!gid) {
            ev.reply(dpp::ir_channel_message_with_source,
                dpp::message("❌ 找不到遊戲！").set_flags(dpp::m_ephemeral));
            return;
        }
        std::string gs = std::to_string(gid);
        dpp::snowflake game_ch = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = onw_games.find(gid);
            if (it != onw_games.end()) game_ch = it->second.channel_id;
        }

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
        else if (cid.rfind("onw_inc_"+gs+"_", 0) == 0 || cid.rfind("onw_dec_"+gs+"_", 0) == 0) {
            bool inc = (cid.rfind("onw_inc_", 0) == 0);
            std::string role = cid.substr(cid.rfind('_') + 1);
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
            onw_begin_game(*g_bot, gid);
        }
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
            if (alpha_ready) onw_start_seer(*g_bot, gid);
        }
        else if (cid == "onw_wolf_skip_"+gs) {
            bool proceed = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = onw_games.find(gid);
                if (it == onw_games.end()) return;
                auto& g = it->second;
                if (g.phase != ONWPhase::NIGHT_WOLVES) {
                    ev.reply(dpp::ir_channel_message_with_source,
                        dpp::message("✅ 確認！").set_flags(dpp::m_ephemeral));
                    return;
                }
                if (!g.wolf_done) {
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
            if (proceed) onw_start_seer(*g_bot, gid);
        }
        else if (cid.rfind("onw_alpha_inf_"+gs+"_", 0) == 0) {
            dpp::snowflake target(std::stoull(cid.substr(cid.rfind('_')+1)));
            bool proceed = false;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = onw_games.find(gid);
                if (it == onw_games.end()) return;
                auto& g = it->second;
                if (g.phase != ONWPhase::NIGHT_WOLVES || g.alpha_done) return;
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
            if (proceed) onw_start_seer(*g_bot, gid);
        }
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
            if (proceed) onw_start_seer(*g_bot, gid);
        }
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
            onw_start_robber(*g_bot, gid);
        }
        else if (cid == "onw_seer_center_"+gs) {
            std::array<std::string,3> center;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = onw_games.find(gid);
                if (it == onw_games.end()) return;
                if (it->second.phase != ONWPhase::NIGHT_SEER) return;
                center = it->second.center;
            }
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
            onw_start_robber(*g_bot, gid);
        }
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
            onw_start_robber(*g_bot, gid);
        }
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
            onw_start_troublemaker(*g_bot, gid);
        }
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
            onw_start_troublemaker(*g_bot, gid);
        }
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
            ev.reply(dpp::ir_update_message, dm);
        }
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
            onw_start_witch(*g_bot, gid);
        }
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
            onw_start_witch(*g_bot, gid);
        }
        else if (cid == "onw_vi_left_"+gs) {
            std::string vi_name, new_role;
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
            g_bot->message_create(dpp::message(game_ch,
                "🃏 **" + vi_name + "** 是**村子白痴**！原始身份牌公開翻面：🃏 **村子白痴**\n所有玩家的牌已向左循環一格。"));
            onw_start_drunk(*g_bot, gid);
        }
        else if (cid == "onw_vi_right_"+gs) {
            std::string vi_name, new_role;
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
            g_bot->message_create(dpp::message(game_ch,
                "🃏 **" + vi_name + "** 是**村子白痴**！原始身份牌公開翻面：🃏 **村子白痴**\n所有玩家的牌已向右循環一格。"));
            onw_start_drunk(*g_bot, gid);
        }
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
            onw_start_village_idiot(*g_bot, gid);
        }
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
            onw_start_village_idiot(*g_bot, gid);
        }
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
            onw_start_insomniac(*g_bot, gid);
        }
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
            onw_start_day(*g_bot, gid);
        }
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
                g_bot->message_create(vm, [gid](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        std::lock_guard<std::mutex> lk(data_mutex);
                        auto it = onw_games.find(gid);
                        if (it != onw_games.end())
                            it->second.vote_msg_id = std::get<dpp::message>(cb.value).id;
                    }
                });
            }
        }
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
                g_bot->message_edit(dpp::message(vote_msg_id, "").add_embed(vm.embeds[0])
                    .set_channel_id(game_ch));
            if (all_voted) onw_resolve_vote(*g_bot, gid);
        }
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
            onw_resolve_vote(*g_bot, gid);
        }
    }
    // ── 狼人殺按鈕 ───────────────────────────────────────────────────────────
    else if (cid.rfind("wolf_", 0) == 0) {
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
            { std::string dn = ev.command.member.get_nickname(); p.display_name = dn; }
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
                begin_wolf_game(*g_bot, gid);
            }
        }
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
            if (nom_action == NomAction::TEAR_BADGE)  announce_night_and_start_day(*g_bot, gid);
            else if (nom_action == NomAction::AUTO_SPEECH) start_sheriff_speech(*g_bot, gid);
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
                g.speak_seats.clear(); g.speak_idx = 0;
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
                start_sheriff_speech(*g_bot, gid);
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
            announce_night_and_start_day(*g_bot, gid);
        }
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
            if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 所有人已投票，自動結算！").set_flags(dpp::m_ephemeral)); resolve_sheriff_vote(*g_bot, gid); }
        }
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
            if (wd_action == WdAction::AUTO_SKIP)        announce_night_and_start_day(*g_bot, gid);
            else if (wd_action == WdAction::AUTO_SPEECH) start_sheriff_speech(*g_bot, gid);
        }
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
            if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 所有人已投票，自動結算！").set_flags(dpp::m_ephemeral)); resolve_sheriff_vote(*g_bot, gid); }
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
            resolve_sheriff_vote(*g_bot, gid);
        }
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
                    g_bot->message_edit(upd);
                }
            }
            ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票！").set_flags(dpp::m_ephemeral));
            if (all_voted) proceed_to_seer(*g_bot, gid);
        }
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
            proceed_to_witch(*g_bot, gid);
        }
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
            if (ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("💊 已使用解藥！").set_flags(dpp::m_ephemeral)); resolve_night(*g_bot, gid); }
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
            if (ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⏭ 跳過！").set_flags(dpp::m_ephemeral)); resolve_night(*g_bot, gid); }
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
            if (ok) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("☠️ 已使用毒藥！").set_flags(dpp::m_ephemeral)); resolve_night(*g_bot, gid); }
        }
        else if (cid.rfind("wolf_dir_cw_", 0) == 0 || cid.rfind("wolf_dir_ccw_", 0) == 0) {
            bool is_cw = cid.rfind("wolf_dir_cw_", 0) == 0;
            uint64_t gid = std::stoull(cid.substr(is_cw ? 12 : 13));
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                auto& g = it->second;
                if (uid != g.sheriff_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有警長可以選擇！").set_flags(dpp::m_ephemeral)); return; }
                if (g.phase != WolfPhase::SHERIFF_SPEAK_DIR) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                compute_speak_order(g, g.night_deaths, true, is_cw);
            }
            std::string dir_label = is_cw ? "順時針 ▶" : "逆時針 ◀";
            dpp::embed e; e.set_title("🎤  今日發言順序（" + dir_label + "）").set_color(0x3498DB)
                .set_description("開始依序發言...");
            ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
            start_day_speak(*g_bot, gid);
        }
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
                g.day_votes[uid] = dpp::snowflake(0);
                int alive_cnt = 0, voted_cnt = 0;
                for (auto& p2 : g.players) { if (p2.alive) { alive_cnt++; if (g.day_votes.count(p2.uid)) voted_cnt++; } }
                auto_resolve = (alive_cnt > 0 && voted_cnt == alive_cnt);
                if (!auto_resolve && g.day_vote_msg_id) {
                    dpp::message upd = make_day_vote_msg(g);
                    upd.id = g.day_vote_msg_id; upd.channel_id = g.channel_id;
                    g_bot->message_edit(upd);
                }
            }
            if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("🚫 已棄票，所有人已投完，自動結算！").set_flags(dpp::m_ephemeral)); resolve_day_vote(*g_bot, gid); }
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
            resolve_day_vote(*g_bot, gid);
        }
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
            resolve_pk_vote(*g_bot, gid);
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
                    g_bot->message_edit(upd);
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
                    g_bot->message_edit(upd);
                }
            }
            if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票，所有人已投完，自動結算！").set_flags(dpp::m_ephemeral)); resolve_day_vote(*g_bot, gid); }
            else ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票給 **" + tname + "**！").set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("wolf_hunter_skip_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(17));
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                auto& g = it->second;
                if (g.phase != WolfPhase::HUNTER_SHOOT) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                if (uid != g.hunter_uid) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 你不是獵人！").set_flags(dpp::m_ephemeral)); return; }
                g.hunter_pending = false;
            }
            dpp::embed e; e.set_title("🏹  獵人選擇不開槍").set_color(0x808080);
            e.set_description("獵人選擇不帶走任何人。");
            ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
            continue_after_hunter(*g_bot, gid);
        }
        else if (cid.rfind("wolf_candidate_withdraw_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(24));
            std::string pname;
            bool ok = false;
            bool need_advance = false;
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
                    auto sit = std::find(g.speak_seats.begin(), g.speak_seats.end(), pseat);
                    if (sit != g.speak_seats.end()) {
                        int widx = (int)(sit - g.speak_seats.begin());
                        if (widx == g.speak_idx) {
                            g.speak_idx--;
                            need_advance = true;
                        } else if (widx < g.speak_idx) {
                            g.speak_idx--;
                        }
                        g.speak_seats.erase(sit);
                    }
                } else {
                    g.speak_seats.erase(std::remove(g.speak_seats.begin(), g.speak_seats.end(), pseat), g.speak_seats.end());
                }
                ok = true;
            }
            if (ok) {
                g_bot->message_create(dpp::message(ev.command.channel_id, "📢 **" + pname + "** 退出了警長競選！"));
                ev.reply(dpp::ir_channel_message_with_source, dpp::message("🚪 已退出候選！").set_flags(dpp::m_ephemeral));
                if (need_advance) advance_speaker(*g_bot, gid);
            }
        }
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
                    g_bot->message_edit(upd);
                }
            }
            if (auto_resolve) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票，所有人投完，自動結算！").set_flags(dpp::m_ephemeral)); resolve_mvp_vote(*g_bot, gid); }
            else ev.reply(dpp::ir_channel_message_with_source, dpp::message("✅ 已投票！").set_flags(dpp::m_ephemeral));
        }
        else if (cid.rfind("wolf_hunter_", 0) == 0) {
            std::string rest = cid.substr(12);
            size_t sep = rest.find('_');
            if (sep == std::string::npos) return;
            uint64_t gid = std::stoull(rest.substr(0, sep));
            dpp::snowflake target(std::stoull(rest.substr(sep+1)));
            std::string tname;
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
            }
            dpp::embed e; e.set_title("🏹  獵人射擊！").set_color(0xE67E22);
            e.set_description("獵人帶走了 **" + tname + "**！");
            ev.reply(dpp::ir_update_message, dpp::message().add_embed(e));
            continue_after_hunter(*g_bot, gid);
        }
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
            continue_after_badge(*g_bot, gid);
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
            continue_after_badge(*g_bot, gid);
        }
        else if (cid.rfind("wolf_last_words_", 0) == 0) {
            uint64_t gid = std::stoull(cid.substr(16));
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                auto& g = it->second;
                if (g.phase != WolfPhase::LAST_WORDS) { ev.reply(dpp::ir_channel_message_with_source, dpp::message("⚠️ 已過時！").set_flags(dpp::m_ephemeral)); return; }
                if (uid != g.lw_current_victim && uid != g.host_id) {
                    ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 只有遺言者本人或主持人可以操作！").set_flags(dpp::m_ephemeral)); return;
                }
            }
            ev.reply(dpp::ir_update_message, dpp::message().add_embed(
                dpp::embed().set_title("💬  遺言結束").set_color(0x7F8C8D).set_description("遺言時間結束。")));
            continue_last_words(*g_bot, gid);
        }
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
            advance_speaker(*g_bot, gid);
        }
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
                advance_speaker(*g_bot, gid);
            }
        }
    }
}
