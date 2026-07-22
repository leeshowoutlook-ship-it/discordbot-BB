#include "types.h"
#include "chips.h"
#include "undercover.h"
#include "handler_decls.h"

// ─── Message handler ──────────────────────────────────────────────────────────

void handle_uc_message(const dpp::message_create_t& ev, const std::string& content,
                       dpp::snowflake uid, dpp::snowflake ch)
{
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
        m.channel_id = ch; g_bot->message_create(m); return;
    }
    dpp::message m;
    { std::lock_guard<std::mutex> lk(data_mutex); m = uc_lobby_msg(uc_games[gid]); }
    m.channel_id = ch;
    g_bot->message_create(m, [gid](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = uc_games.find(gid);
            if (it != uc_games.end())
                it->second.lobby_msg_id = std::get<dpp::message>(cb.value).id;
        }
    });
}

// ─── Button handler ───────────────────────────────────────────────────────────

void handle_uc_button(const dpp::button_click_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;

    if (cid.rfind("uc_again_", 0) == 0 || cid.rfind("uc_adult_again_", 0) == 0) {
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
        (void)can_vote;
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
}

// ─── Modal handler ────────────────────────────────────────────────────────────

void handle_uc_modal(const dpp::form_submit_t& ev)
{
    const std::string& cid = ev.custom_id;
    dpp::snowflake issuer = ev.command.get_issuing_user().id;

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
        (void)ok;
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
}

// ─── Select handler ───────────────────────────────────────────────────────────

void handle_uc_select(const dpp::select_click_t& ev, dpp::snowflake uid)
{
    const std::string& cid = ev.custom_id;

    if (cid.rfind("uc_pool_", 0) == 0) {
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
}

// ─── Slash handler ────────────────────────────────────────────────────────────

void handle_uc_slash(const dpp::slashcommand_t& ev, const std::string& cmd_name,
                     dpp::snowflake uid, dpp::snowflake ch)
{
    (void)cmd_name;
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
