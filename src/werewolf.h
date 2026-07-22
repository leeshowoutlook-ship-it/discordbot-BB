#pragma once
#include "types.h"
#include "chips.h"
#include "wolfplayerstats.h"
#include <random>
#include <algorithm>
#include <sstream>

// ─── Enums ────────────────────────────────────────────────────────────────────

enum class WolfPhase {
    WAITING,
    SHERIFF_NOMINATE,
    SHERIFF_SPEECH,    // candidates speaking before sheriff vote
    SHERIFF_VOTE,
    NIGHT_WOLVES,
    NIGHT_SEER,
    NIGHT_WITCH,
    DAY_ANNOUNCE,
    LAST_WORDS,
    SHERIFF_SPEAK_DIR,
    DAY_SPEAK,         // all-player speaking before day vote
    BADGE_TRANSFER,
    DAY_VOTE,
    DAY_VOTE_PK,
    HUNTER_SHOOT,
    GAME_OVER,
    MVP_VOTE
};

// ─── Structs ──────────────────────────────────────────────────────────────────

struct WolfPlayer {
    dpp::snowflake uid;
    std::string    display_name;
    int            seat = 0;
    std::string    role;
    bool           alive     = true;
    bool           is_sheriff= false;
};

struct WolfGame {
    uint64_t       id;
    dpp::snowflake channel_id;
    dpp::snowflake guild_id;
    dpp::snowflake host_id;
    dpp::snowflake wolf_thread_id;
    WolfPhase      phase = WolfPhase::WAITING;

    std::vector<WolfPlayer> players;   // 9 total, seat 1-9
    int day = 1;
    bool first_morning = true; // sheriff election happens before death announce on day 1

    // Sheriff election
    dpp::snowflake sheriff_uid = 0;
    std::vector<dpp::snowflake>              candidates;
    std::map<dpp::snowflake, dpp::snowflake> sheriff_votes; // voter->candidate

    // Night state
    std::map<dpp::snowflake, dpp::snowflake> wolf_vote_map; // wolf->target
    dpp::snowflake wolf_victim = 0;
    bool witch_has_antidote  = true;
    bool witch_has_poison    = true;
    bool witch_used_tonight  = false;
    dpp::snowflake witch_save_target   = 0;
    dpp::snowflake witch_poison_target = 0;
    std::vector<dpp::snowflake> night_deaths;

    // Day vote
    std::map<dpp::snowflake, dpp::snowflake> day_votes; // voter->target
    std::vector<dpp::snowflake> pk_candidates;
    std::map<dpp::snowflake, dpp::snowflake> pk_votes;
    dpp::snowflake day_vote_msg_pk_id = 0;

    // Hunter
    bool           hunter_pending    = false;
    dpp::snowflake hunter_uid        = 0;
    WolfPhase      after_hunter      = WolfPhase::WAITING;

    // Badge transfer
    WolfPhase      after_badge       = WolfPhase::WAITING;
    dpp::snowflake badge_from        = 0; // who is transferring

    // Speaking order (when sheriff picks direction)
    std::vector<int> speak_seats;
    int speak_start_seat  = 0;
    bool speak_dir_needed = false; // true = waiting for sheriff direction choice

    // Speaking window state
    int speak_idx = 0;             // current speaker index into speak_seats

    // Last words continuation
    WolfPhase after_last_words = WolfPhase::WAITING;

    // DM tracking
    dpp::snowflake seer_dm_channel     = 0;
    dpp::snowflake witch_dm_channel    = 0;
    dpp::snowflake wolf_vote_msg_id    = 0;
    dpp::snowflake day_vote_msg_id     = 0;
    dpp::snowflake sheriff_vote_msg_id = 0;

    // Last words — one victim at a time; victim OR host clicks done
    dpp::snowflake lw_current_victim   = 0;

    // Sheriff candidacy tracking
    std::vector<dpp::snowflake> withdrawn_candidates; // withdrew after nominating
    std::vector<dpp::snowflake> not_running;          // explicitly opted out

    // MVP vote after game
    std::map<dpp::snowflake, dpp::snowflake> mvp_votes; // voter -> target
    dpp::snowflake mvp_vote_msg_id = 0;
};

// ─── Global state ─────────────────────────────────────────────────────────────

inline std::map<uint64_t, WolfGame>       wolf_games;
inline std::map<dpp::snowflake, uint64_t> channel_wolf_game; // channel_id -> game_id
inline std::atomic<uint64_t>              wolf_counter{1};

// ─── Forward declarations ─────────────────────────────────────────────────────

static void proceed_to_seer(dpp::cluster&, uint64_t);
static void proceed_to_witch(dpp::cluster&, uint64_t);
static void resolve_night(dpp::cluster&, uint64_t);
static void announce_night_and_start_day(dpp::cluster&, uint64_t);
static void start_last_words(dpp::cluster&, uint64_t, const std::vector<dpp::snowflake>&, WolfPhase);
static void continue_last_words(dpp::cluster&, uint64_t);
static void proceed_to_speak_order(dpp::cluster&, uint64_t);
static void start_day_vote(dpp::cluster&, uint64_t);
static void resolve_pk_vote(dpp::cluster&, uint64_t);
static void resolve_sheriff_vote(dpp::cluster&, uint64_t);
static void resolve_mvp_vote(dpp::cluster&, uint64_t);
static void end_game(dpp::cluster&, uint64_t, const std::string&);
static void start_night(dpp::cluster&, uint64_t);
static void trigger_hunter(dpp::cluster&, uint64_t, WolfPhase after);
static void trigger_badge(dpp::cluster&, uint64_t, WolfPhase after);
static void continue_after_badge(dpp::cluster&, uint64_t);
static void continue_after_hunter(dpp::cluster&, uint64_t);
static void start_sheriff_speech(dpp::cluster&, uint64_t);
static void start_day_speak(dpp::cluster&, uint64_t);
static void advance_speaker(dpp::cluster&, uint64_t);

// ─── Helpers ──────────────────────────────────────────────────────────────────

static WolfPlayer* wfind(WolfGame& g, dpp::snowflake uid) {
    for (auto& p : g.players) if (p.uid == uid) return &p;
    return nullptr;
}
static int wolf_alive_cnt(const WolfGame& g) {
    int n = 0;
    for (auto& p : g.players) if (p.alive && p.role == "狼人") n++;
    return n;
}
static int good_alive_cnt(const WolfGame& g) {
    int n = 0;
    for (auto& p : g.players) if (p.alive && p.role != "狼人") n++;
    return n;
}
static bool check_win(const WolfGame& g, std::string& winner) {
    int wolves = wolf_alive_cnt(g);
    if (wolves == 0) { winner = "好人"; return true; }
    // 屠邊局（數量壓制不算贏，必須屠邊）
    int civilians = 0, specials = 0;
    for (auto& p : g.players) {
        if (!p.alive) continue;
        if (p.role == "村民") civilians++;
        else if (p.role == "預言家" || p.role == "女巫" || p.role == "獵人") specials++;
    }
    if (civilians == 0) { winner = "狼人"; return true; }  // 屠平民邊
    if (specials == 0)  { winner = "狼人"; return true; }  // 屠神職邊
    return false;
}
static WolfPlayer* find_alive_role(WolfGame& g, const std::string& role) {
    for (auto& p : g.players) if (p.role == role && p.alive) return &p;
    return nullptr;
}
static std::mt19937& wrng() {
    static std::mt19937 r(std::random_device{}());
    return r;
}
static bool rand_bool() { return std::uniform_int_distribution<int>(0,1)(wrng()) == 0; }

// Alive seats in ascending order
static std::vector<int> alive_seats(const WolfGame& g) {
    std::vector<int> s;
    for (auto& p : g.players) if (p.alive) s.push_back(p.seat);
    std::sort(s.begin(), s.end());
    return s;
}

// Build a rotated seat order starting at ref_seat, CW (ascending) or CCW (descending)
static std::vector<int> rotate_from(const std::vector<int>& seats, int ref_seat, bool cw) {
    if (seats.empty()) return {};
    // Find closest seat to ref
    int best_idx = 0, best_d = 999;
    for (int i = 0; i < (int)seats.size(); i++) {
        int d = std::abs(seats[i] - ref_seat);
        if (d < best_d) { best_d = d; best_idx = i; }
    }
    std::vector<int> order;
    int n = (int)seats.size();
    if (cw) {
        for (int i = 0; i < n; i++) order.push_back(seats[(best_idx + i) % n]);
    } else {
        for (int i = 0; i < n; i++) order.push_back(seats[((best_idx - i) % n + n) % n]);
    }
    return order;
}

// Compute speak order after deaths. Returns true if sheriff needs to pick direction.
// Deaths = who died this morning.
static bool compute_speak_order(WolfGame& g, const std::vector<dpp::snowflake>& deaths,
                                  bool have_dir = false, bool cw_dir = true,
                                  bool is_sheriff_election = false) {
    auto as = alive_seats(g);
    if (as.empty()) { g.speak_seats = {}; return false; }

    bool cw = have_dir ? cw_dir : rand_bool();

    if (is_sheriff_election) {
        int start = as[std::uniform_int_distribution<int>(0,(int)as.size()-1)(wrng())];
        g.speak_seats = rotate_from(as, start, cw);
        return false;
    }

    bool sheriff_alive = g.sheriff_uid && wfind(g, g.sheriff_uid) && wfind(g, g.sheriff_uid)->alive;
    int num_dead = (int)deaths.size();
    int ref_seat = 0;

    if (num_dead == 0) {
        // 平安夜：警長選擇從哪側開始，警長最後發言
        if (sheriff_alive && !have_dir) {
            for (auto& p : g.players)
                if (p.uid == g.sheriff_uid) { ref_seat = p.seat; break; }
            g.speak_start_seat = ref_seat;
            return true;
        }
        if (sheriff_alive && have_dir) {
            for (auto& p : g.players)
                if (p.uid == g.sheriff_uid) { ref_seat = p.seat; break; }
            int n = (int)as.size();
            int sheriff_idx = 0;
            for (int i = 0; i < n; i++) if (as[i] == ref_seat) { sheriff_idx = i; break; }
            // Adjacent seat in chosen direction; sheriff wraps to last
            int adj = cw_dir ? (sheriff_idx + 1) % n : (sheriff_idx - 1 + n) % n;
            g.speak_seats = rotate_from(as, as[adj], cw_dir);
            return false;
        }
        int start = as[std::uniform_int_distribution<int>(0,(int)as.size()-1)(wrng())];
        g.speak_seats = rotate_from(as, start, cw);
        return false;
    }

    // num_dead >= 1: sheriff always speaks last, picks CW or CCW from adjacent seat
    ref_seat = 0;
    if (sheriff_alive) {
        for (auto& p : g.players)
            if (p.uid == g.sheriff_uid) { ref_seat = p.seat; break; }
        if (!have_dir) {
            g.speak_start_seat = ref_seat;
            return true; // need sheriff direction
        }
        int n = (int)as.size();
        int sheriff_idx = 0;
        for (int i = 0; i < n; i++) if (as[i] == ref_seat) { sheriff_idx = i; break; }
        int adj = cw_dir ? (sheriff_idx + 1) % n : (sheriff_idx - 1 + n) % n;
        g.speak_seats = rotate_from(as, as[adj], cw_dir);
        return false;
    } else {
        int start = as[std::uniform_int_distribution<int>(0,(int)as.size()-1)(wrng())];
        g.speak_seats = rotate_from(as, start, cw);
        return false;
    }
}

// ─── Message builders ─────────────────────────────────────────────────────────

static std::string player_list_str(const WolfGame& g, bool show_roles = false) {
    std::ostringstream oss;
    for (auto& p : g.players) {
        oss << (p.alive ? "🟢" : "☠️") << " **" << p.seat << ".** "
            << "<@" << (uint64_t)p.uid << ">";
        if (p.is_sheriff) oss << " 🏅";
        if (show_roles) oss << " — " << p.role;
        oss << "\n";
    }
    return oss.str();
}

static dpp::message make_wolf_lobby_msg(const WolfGame& g) {
    int n = (int)g.players.size();
    dpp::embed e;
    e.set_title("🐺  狼人殺 — 等待玩家加入").set_color(0x8B0000);
    std::string joined;
    for (auto& p : g.players)
        joined += "<@" + std::to_string((uint64_t)p.uid) + "> ";
    e.set_description("需要 **9** 名玩家（目前 **" + std::to_string(n) + "** 人）\n"
                      "主持人：<@" + std::to_string((uint64_t)g.host_id) + ">\n\n"
                      "已加入：\n" + (joined.empty() ? "（尚無玩家）" : joined));
    e.set_footer(dpp::embed_footer().set_text("加入遊戲後，主持人按「開始遊戲」即可"));

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐺 加入遊戲").set_id("wolf_join_" + std::to_string(g.id)).set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 退出").set_id("wolf_leave_" + std::to_string(g.id)).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶ 開始遊戲").set_id("wolf_start_" + std::to_string(g.id))
        .set_style(dpp::cos_primary).set_disabled(n != 9));
    msg.add_component(row);
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗑️ 解散房間").set_id("wolf_dissolve_" + std::to_string(g.id)).set_style(dpp::cos_danger));
    msg.add_component(row2);
    return msg;
}

static dpp::message make_sheriff_nominate_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🏅  第一天 — 警長競選報名").set_color(0xF39C12);
    std::string status_str;
    int decided = 0, total = 0;
    for (auto& p : g.players) {
        if (p.seat == 0 || !p.alive) continue; total++;
        bool is_cand  = std::find(g.candidates.begin(), g.candidates.end(), p.uid) != g.candidates.end();
        bool not_run  = std::find(g.not_running.begin(), g.not_running.end(), p.uid) != g.not_running.end();
        bool withdrew = std::find(g.withdrawn_candidates.begin(), g.withdrawn_candidates.end(), p.uid) != g.withdrawn_candidates.end();
        if (is_cand || not_run || withdrew) decided++;
    }
    bool all_decided = (decided == total);
    for (auto& p : g.players) {
        if (p.seat == 0 || !p.alive) continue;
        bool is_cand  = std::find(g.candidates.begin(), g.candidates.end(), p.uid) != g.candidates.end();
        bool not_run  = std::find(g.not_running.begin(), g.not_running.end(), p.uid) != g.not_running.end();
        bool withdrew = std::find(g.withdrawn_candidates.begin(), g.withdrawn_candidates.end(), p.uid) != g.withdrawn_candidates.end();
        if (all_decided) {
            if (is_cand)              status_str += "🏃 " + std::to_string(p.seat) + ". " + p.display_name + " — **競選中**\n";
            else if (not_run || withdrew) status_str += "❌ " + std::to_string(p.seat) + ". " + p.display_name + "\n";
        } else {
            bool has_decided = is_cand || not_run || withdrew;
            status_str += (has_decided ? "✅ " : "⏳ ") + std::to_string(p.seat) + ". " + p.display_name + "\n";
        }
    }
    std::string speak;
    if (all_decided && !g.speak_seats.empty()) {
        for (int i = 0; i < (int)g.speak_seats.size(); i++) {
            if (i) speak += " → "; speak += "**" + std::to_string(g.speak_seats[i]) + "**";
        }
        speak = "\n\n🎤 **競選發言順序：**\n" + speak;
    }
    e.set_description("任何玩家可自薦擔任警長！候選人依順序發言後全體投票。" + speak);
    e.add_field("決定狀況 (" + std::to_string(decided) + "/" + std::to_string(total) + ")", status_str, false);
    e.set_footer(dpp::embed_footer().set_text("🙋 競選 | ❌ 不競選 | 主持人按「開始發言」或「跳過警長」"));

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🙋 我要競選").set_id("wolf_nominate_" + std::to_string(g.id)).set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 不競選").set_id("wolf_withdraw_nominate_" + std::to_string(g.id)).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎤 開始發言").set_id("wolf_sheriff_vote_start_" + std::to_string(g.id))
        .set_style(dpp::cos_primary).set_disabled(g.candidates.empty()));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⏭ 跳過警長").set_id("wolf_skip_sheriff_" + std::to_string(g.id)).set_style(dpp::cos_secondary));
    msg.add_component(row);
    return msg;
}

static dpp::message make_sheriff_vote_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🏅  警長競選 — 投票").set_color(0xF39C12);
    int alive_cnt = 0;
    for (auto& p : g.players) if (p.alive) alive_cnt++;
    // Count non-candidates who can vote
    int voter_cnt = 0;
    for (auto& p : g.players) {
        if (!p.alive) continue;
        bool is_cand = std::find(g.candidates.begin(), g.candidates.end(), p.uid) != g.candidates.end();
        if (!is_cand) voter_cnt++;
    }
    std::string cand_str;
    for (auto cuid : g.candidates) {
        auto* p = wfind(const_cast<WolfGame&>(g), cuid);
        if (!p) continue;
        cand_str += "  • **" + std::to_string(p->seat) + ". " + p->display_name + "**\n";
    }
    // Show only voted/not-voted status — hide targets until resolve
    std::string voter_status;
    for (auto& p : g.players) {
        if (!p.alive) continue;
        bool is_cand = std::find(g.candidates.begin(), g.candidates.end(), p.uid) != g.candidates.end();
        if (is_cand) continue;
        bool voted = g.sheriff_votes.count(p.uid) > 0;
        voter_status += (voted ? "✅ " : "⏳ ") + std::to_string(p.seat) + ". " + p.display_name + "\n";
    }
    std::string desc = "候選人不可投票。請非候選人點擊按鈕投票：\n\n**候選人：**\n" + cand_str;
    if (!voter_status.empty()) desc += "\n**投票狀況（結算後揭曉明細）：**\n" + voter_status;
    e.set_description(desc);
    e.set_footer(dpp::embed_footer().set_text("已投 " + std::to_string(g.sheriff_votes.size()) + " / " + std::to_string(voter_cnt) + " 票（候選人不投票）"));

    dpp::message msg; msg.add_embed(e);
    // Candidate buttons (up to 5 per row)
    for (int i = 0; i < (int)g.candidates.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)g.candidates.size(), i+5); j++) {
            auto* p = wfind(const_cast<WolfGame&>(g), g.candidates[j]);
            if (!p) continue;
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(p->display_name)
                .set_id("wolf_svote_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)g.candidates[j]))
                .set_style(dpp::cos_primary));
        }
        msg.add_component(row);
    }
    dpp::component ctrl; ctrl.set_type(dpp::cot_action_row);
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚫 棄票").set_id("wolf_svote_abstain_" + std::to_string(g.id)).set_style(dpp::cos_secondary));
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏃 退出候選").set_id("wolf_candidate_withdraw_" + std::to_string(g.id)).set_style(dpp::cos_danger));
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 結算投票").set_id("wolf_sheriff_resolve_" + std::to_string(g.id)).set_style(dpp::cos_danger));
    msg.add_component(ctrl);
    return msg;
}

static dpp::message make_wolf_vote_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🐺  狼人討論 — 第 " + std::to_string(g.day) + " 天夜晚").set_color(0x800000);
    std::string status;
    for (auto& p : g.players) {
        if (p.role != "狼人" || !p.alive) continue;
        bool voted = g.wolf_vote_map.count(p.uid) > 0;
        status += (voted ? "✅ " : "⏳ ") + p.display_name;
        if (voted) {
            auto it = g.wolf_vote_map.find(p.uid);
            auto* t = wfind(const_cast<WolfGame&>(g), it->second);
            if (t) status += " → " + t->display_name;
        }
        status += "\n";
    }
    e.set_description("**狼人投票狀況：**\n" + status + "\n請選擇今晚的目標（多人投同一目標則選出，否則票最多者出線）：");

    dpp::message msg; msg.add_embed(e);
    std::vector<const WolfPlayer*> targets;
    for (auto& p : g.players) if (p.alive) targets.push_back(&p);
    for (int i = 0; i < (int)targets.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)targets.size(), i+5); j++) {
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(targets[j]->seat) + ". " + targets[j]->display_name)
                .set_id("wolf_wvote_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)targets[j]->uid))
                .set_style(dpp::cos_danger));
        }
        msg.add_component(row);
    }
    return msg;
}

static dpp::message make_seer_dm_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🔮  預言家 — 第 " + std::to_string(g.day) + " 天夜晚").set_color(0x9B59B6);
    e.set_description("請選擇一名玩家查驗身份：");
    dpp::message msg; msg.add_embed(e);
    std::vector<const WolfPlayer*> targets;
    for (auto& p : g.players) if (p.alive && p.role != "預言家") targets.push_back(&p);
    for (int i = 0; i < (int)targets.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)targets.size(), i+5); j++) {
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(targets[j]->seat) + ". " + targets[j]->display_name)
                .set_id("wolf_seer_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)targets[j]->uid))
                .set_style(dpp::cos_primary));
        }
        msg.add_component(row);
    }
    return msg;
}

static dpp::message make_witch_dm_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🧪  女巫 — 第 " + std::to_string(g.day) + " 天夜晚").set_color(0x1ABC9C);
    std::string victim_info = "今晚無人被攻擊";
    if (g.wolf_victim) {
        if (g.witch_has_antidote) {
            auto* v = wfind(const_cast<WolfGame&>(g), g.wolf_victim);
            if (v) victim_info = "狼人目標：**" + std::to_string(v->seat) + ". " + v->display_name + "**";
        } else {
            victim_info = "（解藥已用完，今晚無法得知攻擊目標）";
        }
    }
    std::string potions = (g.witch_has_antidote ? "💊 解藥 ✅" : "💊 解藥 ❌") + std::string("  ") +
                          (g.witch_has_poison   ? "☠️ 毒藥 ✅" : "☠️ 毒藥 ❌");
    e.set_description(victim_info + "\n" + potions + "\n\n今晚只能使用一種藥水（或跳過）：");

    dpp::message msg; msg.add_embed(e);
    // Save button
    {
        dpp::component row; row.set_type(dpp::cot_action_row);
        if (g.wolf_victim && g.witch_has_antidote && !g.witch_used_tonight) {
            auto* v = wfind(const_cast<WolfGame&>(g), g.wolf_victim);
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("💊 使用解藥")
                .set_id("wolf_witch_save_" + std::to_string(g.id)).set_style(dpp::cos_success));
        }
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("⏭ 跳過").set_id("wolf_witch_skip_" + std::to_string(g.id)).set_style(dpp::cos_secondary));
        msg.add_component(row);
    }
    // Poison buttons
    if (g.witch_has_poison && !g.witch_used_tonight) {
        std::vector<const WolfPlayer*> targets;
        for (auto& p : g.players) if (p.alive) targets.push_back(&p);
        for (int i = 0; i < (int)targets.size(); i += 5) {
            dpp::component row; row.set_type(dpp::cot_action_row);
            for (int j = i; j < std::min((int)targets.size(), i+5); j++) {
                row.add_component(dpp::component().set_type(dpp::cot_button)
                    .set_label("☠️ " + std::to_string(targets[j]->seat) + ". " + targets[j]->display_name)
                    .set_id("wolf_witch_poison_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)targets[j]->uid))
                    .set_style(dpp::cos_danger));
            }
            msg.add_component(row);
        }
    }
    return msg;
}

static dpp::message make_day_vote_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🗳️  白天投票 — 第 " + std::to_string(g.day) + " 天").set_color(0xE67E22);
    // Speaking order
    if (!g.speak_seats.empty()) {
        std::string order;
        for (int i = 0; i < (int)g.speak_seats.size(); i++) {
            if (i) order += " → "; order += "**" + std::to_string(g.speak_seats[i]) + "**";
        }
        e.add_field("🎤 今日發言順序", order, false);
    }
    // Show only voted/not-voted — hide targets until resolve
    std::string voted_str;
    int alive_cnt = 0;
    for (auto& p : g.players) {
        if (!p.alive) continue; alive_cnt++;
        bool voted = g.day_votes.count(p.uid) > 0;
        voted_str += (voted ? "✅ " : "⏳ ") + std::to_string(p.seat) + ". **" + p.display_name + "**";
        if (p.is_sheriff) voted_str += " 🏅";
        voted_str += "\n";
    }
    e.add_field("投票狀況（結算後揭曉明細）(" + std::to_string(g.day_votes.size()) + "/" + std::to_string(alive_cnt) + ")", voted_str, false);
    e.set_footer(dpp::embed_footer().set_text("警長票數 × 1.5 | 主持人按「結算投票」"));

    dpp::message msg; msg.add_embed(e);
    std::vector<const WolfPlayer*> targets;
    for (auto& p : g.players) if (p.alive) targets.push_back(&p);
    for (int i = 0; i < (int)targets.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)targets.size(), i+5); j++) {
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(targets[j]->seat) + ". " + targets[j]->display_name)
                .set_id("wolf_dvote_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)targets[j]->uid))
                .set_style(dpp::cos_primary));
        }
        msg.add_component(row);
    }
    dpp::component ctrl; ctrl.set_type(dpp::cot_action_row);
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚫 棄票").set_id("wolf_dvote_abstain_" + std::to_string(g.id)).set_style(dpp::cos_secondary));
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 結算投票").set_id("wolf_dvote_resolve_" + std::to_string(g.id)).set_style(dpp::cos_danger));
    msg.add_component(ctrl);
    return msg;
}

static dpp::message make_sheriff_dir_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🏅  警長選擇發言方向").set_color(0xF39C12);
    bool peaceful = g.night_deaths.empty();
    if (peaceful) {
        e.set_description("<@" + std::to_string((uint64_t)g.sheriff_uid) + "> 今晚平安夜，請選擇發言方向\n"
                          "**（警長最後發言）**\n選擇從警長哪側開始：");
    } else {
        e.set_description("<@" + std::to_string((uint64_t)g.sheriff_uid) + "> 請選擇今日發言順序的方向\n"
                          "（從 **座位 " + std::to_string(g.speak_start_seat) + "** 開始）：");
    }
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label(peaceful ? "▶ 從下家開始（警長最後）" : "▶ 順時針（座位遞增）")
        .set_id("wolf_dir_cw_" + std::to_string(g.id)).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label(peaceful ? "◀ 從上家開始（警長最後）" : "◀ 逆時針（座位遞減）")
        .set_id("wolf_dir_ccw_" + std::to_string(g.id)).set_style(dpp::cos_secondary));
    msg.add_component(row);
    return msg;
}

static dpp::message make_speak_window_msg(const WolfGame& g) {
    bool is_sheriff = (g.phase == WolfPhase::SHERIFF_SPEECH);
    if (g.speak_idx < 0 || g.speak_idx >= (int)g.speak_seats.size())
        return dpp::message();
    int cur_seat = g.speak_seats[g.speak_idx];
    const WolfPlayer* cur_p = nullptr;
    for (auto& p : g.players) if (p.seat == cur_seat) { cur_p = &p; break; }

    // Build progress line
    std::ostringstream prog;
    for (int i = 0; i < (int)g.speak_seats.size(); i++) {
        if (i) prog << " → ";
        int s = g.speak_seats[i];
        const WolfPlayer* sp = nullptr;
        for (auto& p : g.players) if (p.seat == s) { sp = &p; break; }
        std::string label = std::to_string(s) + (sp ? ("." + sp->display_name) : "");
        if (i < g.speak_idx)       prog << "~~" << label << "~~";
        else if (i == g.speak_idx) prog << "**▶ " << label << "**";
        else                        prog << label;
    }

    dpp::embed e;
    e.set_title(is_sheriff ? "🎤  警長候選人競選發言" : "🎤  今日玩家發言")
     .set_color(0x3498DB);
    std::string desc = "現在輪到：**" + std::to_string(cur_seat) + ". "
                     + (cur_p ? cur_p->display_name : "?") + "**\n\n"
                     + prog.str();
    e.set_description(desc);
    e.set_footer(dpp::embed_footer().set_text(
        "發言者或主持人按「結束發言」繼續"));

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 結束發言")
        .set_id("wolf_speak_done_" + std::to_string(g.id))
        .set_style(dpp::cos_success));
    if (is_sheriff) {
        // Any candidate can withdraw at any time during speech phase
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🚪 退出候選")
            .set_id("wolf_candidate_withdraw_" + std::to_string(g.id))
            .set_style(dpp::cos_danger));
    }
    msg.add_component(row);
    return msg;
}

static dpp::message make_hunter_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🏹  獵人技能觸發").set_color(0xE67E22);
    e.set_description("<@" + std::to_string((uint64_t)g.hunter_uid) + "> 你已死亡！\n"
                      "你可以帶走一名玩家，或選擇**不開槍**：");
    dpp::message msg; msg.add_embed(e);
    std::vector<const WolfPlayer*> targets;
    for (auto& p : g.players) if (p.alive) targets.push_back(&p);
    for (int i = 0; i < (int)targets.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)targets.size(), i+5); j++) {
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(targets[j]->seat) + ". " + targets[j]->display_name)
                .set_id("wolf_hunter_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)targets[j]->uid))
                .set_style(dpp::cos_danger));
        }
        msg.add_component(row);
    }
    dpp::component skip_row; skip_row.set_type(dpp::cot_action_row);
    skip_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⏭ 不開槍").set_id("wolf_hunter_skip_" + std::to_string(g.id)).set_style(dpp::cos_secondary));
    msg.add_component(skip_row);
    return msg;
}

static dpp::message make_badge_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🏅  警長傳徽").set_color(0xF39C12);
    e.set_description("<@" + std::to_string((uint64_t)g.badge_from) + "> 警長已死亡！\n"
                      "請選擇傳遞警徽給誰，或直接撕毀：");
    dpp::message msg; msg.add_embed(e);
    std::vector<const WolfPlayer*> alive;
    for (auto& p : g.players) if (p.alive) alive.push_back(&p);
    for (int i = 0; i < (int)alive.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)alive.size(), i+5); j++) {
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(alive[j]->seat) + ". " + alive[j]->display_name)
                .set_id("wolf_badge_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)alive[j]->uid))
                .set_style(dpp::cos_primary));
        }
        msg.add_component(row);
    }
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗑️ 撕毀警徽").set_id("wolf_badge_destroy_" + std::to_string(g.id)).set_style(dpp::cos_danger));
    msg.add_component(row2);
    return msg;
}

static dpp::message make_day_pk_vote_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("⚔️  PK 加時投票").set_color(0xE74C3C);
    std::string cand_str;
    for (auto uid : g.pk_candidates) {
        auto* p = wfind(const_cast<WolfGame&>(g), uid);
        if (!p) continue;
        int votes = 0;
        for (auto& [v, t] : g.pk_votes) if (t == uid) votes++;
        cand_str += "  • **" + std::to_string(p->seat) + ". " + p->display_name + "** — " + std::to_string(votes) + " 票\n";
    }
    int alive_cnt = 0;
    for (auto& p : g.players) if (p.alive) alive_cnt++;
    e.set_description("同票！PK 加時投票，僅可投以下候選人：\n" + cand_str);
    e.set_footer(dpp::embed_footer().set_text("已投 " + std::to_string(g.pk_votes.size()) + " / " + std::to_string(alive_cnt) + " 票 | 警長票 ×1.5 | 主持人按「結算 PK」"));
    dpp::message msg; msg.add_embed(e);
    for (int i = 0; i < (int)g.pk_candidates.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)g.pk_candidates.size(), i+5); j++) {
            auto* p = wfind(const_cast<WolfGame&>(g), g.pk_candidates[j]);
            if (!p) continue;
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(p->seat) + ". " + p->display_name)
                .set_id("wolf_dvote_pk_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)g.pk_candidates[j]))
                .set_style(dpp::cos_danger));
        }
        msg.add_component(row);
    }
    dpp::component ctrl; ctrl.set_type(dpp::cot_action_row);
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 結算 PK").set_id("wolf_dvote_pk_resolve_" + std::to_string(g.id)).set_style(dpp::cos_danger));
    msg.add_component(ctrl);
    return msg;
}

static dpp::message make_wolf_gameover_msg(const WolfGame& g, const std::string& winner) {
    dpp::embed e;
    e.set_title(winner == "好人" ? "🎉  好人陣營獲勝！" : "🐺  狼人陣營獲勝！")
     .set_color(winner == "好人" ? 0x2ECC71 : 0x8B0000);
    e.set_description(player_list_str(g, true));
    e.add_field("獎勵", "獲勝方 **+300** 碼，落敗方 **+150** 碼", false);
    return dpp::message().add_embed(e);
}

// ─── Role assignment & DMs ────────────────────────────────────────────────────

static void assign_roles(WolfGame& g) {
    std::vector<std::string> roles = {
        "狼人","狼人","狼人","村民","村民","村民","預言家","女巫","獵人"
    };
    std::shuffle(roles.begin(), roles.end(), wrng());
    // Shuffle seat order
    std::vector<int> order(9); std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), wrng());
    for (int i = 0; i < 9; i++) {
        g.players[order[i]].seat = i + 1;
        g.players[order[i]].role = roles[i];
    }
    std::sort(g.players.begin(), g.players.end(), [](auto& a, auto& b){ return a.seat < b.seat; });
}

static void send_role_dms(dpp::cluster& bot, const WolfGame& g) {
    for (auto& p : g.players) {
        std::string emoji, desc;
        if      (p.role == "狼人")  { emoji = "🐺"; desc = "你是**狼人**！夜晚在私人討論串與同伴商議，選擇目標。"; }
        else if (p.role == "村民")  { emoji = "🏘️"; desc = "你是**村民**！用白天的討論與投票找出狼人。"; }
        else if (p.role == "預言家"){ emoji = "🔮"; desc = "你是**預言家**！每晚可查驗一人的陣營（好人或狼人）。"; }
        else if (p.role == "女巫")  { emoji = "🧪"; desc = "你是**女巫**！有一瓶解藥和一瓶毒藥，各用一次。"; }
        else if (p.role == "獵人")  { emoji = "🏹"; desc = "你是**獵人**！死亡時可帶走一名玩家。"; }
        dpp::embed e;
        e.set_title(emoji + "  你的角色：" + p.role).set_color(0x5865F2);
        e.set_description(desc + "\n\n**座位：" + std::to_string(p.seat) + "**");
        bot.direct_message_create(p.uid, dpp::message().add_embed(e));
    }
}

// ─── Phase transitions ────────────────────────────────────────────────────────

static dpp::message make_mvp_vote_msg(const WolfGame& g) {
    dpp::embed e;
    e.set_title("🏆  MVP 投票").set_color(0xF1C40F);
    e.set_description("遊戲結束！請為本場遊戲投出 MVP，不能投自己。\n所有人投完後自動結算，結果保留在場上。");
    std::string status;
    for (auto& p : g.players) {
        if (p.seat == 0) continue;
        bool voted = g.mvp_votes.count(p.uid) > 0;
        status += (voted ? "✅ " : "⏳ ") + std::to_string(p.seat) + ". " + p.display_name + "\n";
    }
    e.add_field("投票狀況（結算後揭曉）", status, false);
    dpp::message msg; msg.add_embed(e);
    std::vector<const WolfPlayer*> all;
    for (auto& p : g.players) if (p.seat > 0) all.push_back(&p);
    for (int i = 0; i < (int)all.size(); i += 5) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int j = i; j < std::min((int)all.size(), i+5); j++) {
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(std::to_string(all[j]->seat) + ". " + all[j]->display_name)
                .set_id("wolf_mvp_" + std::to_string(g.id) + "_" + std::to_string((uint64_t)all[j]->uid))
                .set_style(dpp::cos_primary));
        }
        msg.add_component(row);
    }
    return msg;
}

static void resolve_mvp_vote(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, wolf_thread, mvp_uid = 0, vote_msg_id = 0;
    std::string vote_detail;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id; wolf_thread = g.wolf_thread_id; vote_msg_id = g.mvp_vote_msg_id;
        for (auto& p : g.players) {
            if (p.seat == 0) continue;
            auto vit = g.mvp_votes.find(p.uid);
            vote_detail += (vit != g.mvp_votes.end() ? "✅ " : "⏳ ") + std::to_string(p.seat) + ". " + p.display_name;
            if (vit != g.mvp_votes.end()) {
                auto* tp = wfind(g, vit->second);
                vote_detail += " → **" + (tp ? std::to_string(tp->seat) + ". " + tp->display_name : "?") + "**";
            }
            vote_detail += "\n";
        }
        std::map<dpp::snowflake, int> tally;
        for (auto& [v, t] : g.mvp_votes) tally[t]++;
        int best = 0;
        for (auto& [t, cnt] : tally) if (cnt > best) { best = cnt; mvp_uid = t; }
        channel_wolf_game.erase(ch);
        wolf_games.erase(it);
    }
    if (vote_msg_id) {
        dpp::embed fe; fe.set_title("🏆  MVP 投票結束").set_color(0x808080);
        dpp::message upd; upd.id = vote_msg_id; upd.channel_id = ch; upd.add_embed(fe);
        bot.message_edit(upd);
    }
    dpp::embed e; e.set_title("🏆  MVP 揭曉！").set_color(0xF1C40F);
    if (!vote_detail.empty()) e.add_field("📋 投票明細", vote_detail, false);
    e.add_field(mvp_uid ? "🏆 本場 MVP" : "結果",
                mvp_uid ? "<@" + std::to_string((uint64_t)mvp_uid) + "> 恭喜！" : "平票，無法選出 MVP", false);
    bot.message_create(dpp::message(ch, "").add_embed(e));
    if (wolf_thread) bot.channel_delete(wolf_thread);
}

static void end_game(dpp::cluster& bot, uint64_t gid, const std::string& winner) {
    dpp::snowflake ch;
    std::vector<dpp::snowflake> winners_uid, losers_uid;
    dpp::message gm, mvp_msg;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        g.phase = WolfPhase::MVP_VOTE;
        g.mvp_votes.clear();
        for (auto& p : g.players) {
            bool wins = (winner == "狼人") ? (p.role == "狼人") : (p.role != "狼人");
            (wins ? winners_uid : losers_uid).push_back(p.uid);
        }
        gm = make_wolf_gameover_msg(g, winner);
        gm.channel_id = ch;
        mvp_msg = make_mvp_vote_msg(g);
        mvp_msg.channel_id = ch;
        // Keep channel_wolf_game and wolf_games alive until MVP resolves
    }
    for (auto u : winners_uid) add_chips(u, 300);
    for (auto u : losers_uid)  add_chips(u, 150);
    save_chips();
    // Record wolf player stats
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& g = wolf_games[gid];
        for (auto& p : g.players) {
            auto& s = wolf_player_stats_data[p.uid];
            bool is_wolf = (p.role == "狼人");
            bool won = (winner == "狼人") ? is_wolf : !is_wolf;
            if (is_wolf) { s.bad_games++; if (won) s.bad_wins++; }
            else         { s.good_games++; if (won) s.good_wins++; }
        }
    }
    save_wolf_player_stats();
    bot.message_create(gm);
    bot.message_create(mvp_msg, [gid](const dpp::confirmation_callback_t& cb) {
        if (!cb.is_error()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it != wolf_games.end())
                it->second.mvp_vote_msg_id = std::get<dpp::message>(cb.value).id;
        }
    });
    // Auto-resolve MVP vote after 90s if not all players vote
    bot.start_timer([&bot, gid](dpp::timer t) {
        resolve_mvp_vote(bot, gid);
        bot.stop_timer(t);
    }, 90);
}

static void trigger_hunter(dpp::cluster& bot, uint64_t gid, WolfPhase after) {
    dpp::snowflake ch;
    dpp::message hm;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        g.phase = WolfPhase::HUNTER_SHOOT;
        g.after_hunter = after;
        hm = make_hunter_msg(g);
        hm.channel_id = ch;
    }
    bot.message_create(hm);
}

static void continue_after_hunter(dpp::cluster& bot, uint64_t gid) {
    WolfPhase next;
    std::string winner;
    bool has_win = false;
    dpp::snowflake ch;
    bool badge_needed = false;
    dpp::snowflake dead_sheriff = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        next = g.after_hunter;
        has_win = check_win(g, winner);
        // Check if sheriff died
        if (g.sheriff_uid && wfind(g, g.sheriff_uid) && !wfind(g, g.sheriff_uid)->alive) {
            badge_needed = true;
            dead_sheriff = g.sheriff_uid;
            g.badge_from = dead_sheriff;
            g.after_badge = next;
        }
    }
    if (has_win) { end_game(bot, gid, winner); return; }
    if (badge_needed) { trigger_badge(bot, gid, next); return; }

    // Continue
    if (next == WolfPhase::DAY_ANNOUNCE) {
        proceed_to_speak_order(bot, gid); // 夜晚死亡無遺言
    } else {
        start_night(bot, gid);
    }
}

static void trigger_badge(dpp::cluster& bot, uint64_t gid, WolfPhase after) {
    dpp::snowflake ch;
    dpp::message bm;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        g.phase = WolfPhase::BADGE_TRANSFER;
        g.after_badge = after;
        bm = make_badge_msg(g);
        bm.channel_id = ch;
    }
    bot.message_create(bm);
}

static void continue_after_badge(dpp::cluster& bot, uint64_t gid) {
    WolfPhase next;
    dpp::snowflake ch;
    std::vector<dpp::snowflake> deaths;
    std::vector<int> speak;
    bool need_dir = false;
    std::string winner;
    bool has_win = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        next = g.after_badge;
        has_win = check_win(g, winner);
    }
    if (has_win) { end_game(bot, gid, winner); return; }

    if (next == WolfPhase::DAY_ANNOUNCE) {
        proceed_to_speak_order(bot, gid); // 夜晚死亡無遺言
    } else {
        start_night(bot, gid);
    }
}

static void start_night(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, wolf_thread;
    dpp::message wolf_msg;
    int day;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        g.phase = WolfPhase::NIGHT_WOLVES;
        g.wolf_vote_map.clear();
        g.wolf_victim = 0;
        g.witch_save_target = 0;
        g.witch_poison_target = 0;
        g.witch_used_tonight = false;
        g.night_deaths.clear();
        g.pk_candidates.clear();
        g.pk_votes.clear();
        g.day_vote_msg_pk_id = 0;
        ch = g.channel_id;
        wolf_thread = g.wolf_thread_id;
        day = g.day;
        wolf_msg = make_wolf_vote_msg(g);
        wolf_msg.channel_id = wolf_thread;
    }
    {
        dpp::embed e;
        e.set_title("🌙  第 " + std::to_string(day) + " 天 — 入夜").set_color(0x2C3E50);
        e.set_description("🌙 天黑請閉眼\n狼人請在狼人討論串投票...");
        bot.message_create(dpp::message(ch, "").add_embed(e));
    }
    bot.message_create(wolf_msg, [gid](const dpp::confirmation_callback_t& cb){
        if (!cb.is_error()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it != wolf_games.end())
                it->second.wolf_vote_msg_id = std::get<dpp::message>(cb.value).id;
        }
    });
}

static void proceed_to_seer(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake seer_uid = 0;
    dpp::message seer_msg;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        // Tally wolf votes
        std::map<dpp::snowflake, int> tally;
        for (auto& [wolf, tgt] : g.wolf_vote_map) tally[tgt]++;
        dpp::snowflake best = 0; int best_cnt = 0;
        for (auto& [t, cnt] : tally) if (cnt > best_cnt) { best_cnt = cnt; best = t; }
        g.wolf_victim = best;
        g.phase = WolfPhase::NIGHT_SEER;
        auto* seer = find_alive_role(g, "預言家");
        seer_uid = seer ? seer->uid : dpp::snowflake(0);
        if (seer_uid) seer_msg = make_seer_dm_msg(g);
    }
    if (!seer_uid) { proceed_to_witch(bot, gid); return; }
    bot.direct_message_create(seer_uid, seer_msg, [gid](const dpp::confirmation_callback_t& cb){
        if (!cb.is_error()) {
            auto& m = std::get<dpp::message>(cb.value);
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it != wolf_games.end()) it->second.seer_dm_channel = m.channel_id;
        }
    });
}

static void proceed_to_witch(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake witch_uid = 0;
    dpp::message witch_msg;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        g.phase = WolfPhase::NIGHT_WITCH;
        auto* witch = find_alive_role(g, "女巫");
        witch_uid = witch ? witch->uid : dpp::snowflake(0);
        if (witch_uid) witch_msg = make_witch_dm_msg(g);
    }
    if (!witch_uid) { resolve_night(bot, gid); return; }
    bot.direct_message_create(witch_uid, witch_msg, [gid](const dpp::confirmation_callback_t& cb){
        if (!cb.is_error()) {
            auto& m = std::get<dpp::message>(cb.value);
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it != wolf_games.end()) it->second.witch_dm_channel = m.channel_id;
        }
    });
}

static void resolve_night(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    int day;
    bool is_first_morning = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        g.day++;
        day = g.day;
        g.phase = WolfPhase::DAY_ANNOUNCE;
        is_first_morning = g.first_morning;

        // Compute deaths
        std::set<dpp::snowflake> dead_set;
        if (g.wolf_victim && g.witch_save_target != g.wolf_victim)
            dead_set.insert(g.wolf_victim);
        if (g.witch_poison_target)
            dead_set.insert(g.witch_poison_target);
        if (is_first_morning) {
            // Defer marking alive=false until after sheriff election
            // so first-night victims can participate in the election
            for (auto uid : dead_set) {
                auto* p = wfind(g, uid);
                if (p) g.night_deaths.push_back(uid);
            }
        } else {
            for (auto uid : dead_set) {
                auto* p = wfind(g, uid);
                if (p && p->alive) { p->alive = false; g.night_deaths.push_back(uid); }
            }
        }
    }

    if (is_first_morning) {
        // Day 1: sheriff election first, death announcement comes after
        dpp::message nm;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it == wolf_games.end()) return;
            auto& g = it->second;
            g.first_morning = false;
            g.phase = WolfPhase::SHERIFF_NOMINATE;
            compute_speak_order(g, {}, false, false, true);
            nm = make_sheriff_nominate_msg(g);
        }
        dpp::embed wake;
        wake.set_title("☀️  天亮了 — 第 " + std::to_string(day) + " 天").set_color(0xF39C12);
        wake.set_description("天亮了！**先進行警長競選**，死亡名單稍後公布。");
        bot.message_create(dpp::message(ch, "").add_embed(wake));
        nm.channel_id = ch;
        bot.message_create(nm);
        return;
    }

    // Day 2+: announce deaths immediately
    announce_night_and_start_day(bot, gid);
}

// ─── Sheriff vote resolve (called from button handler or auto-resolve) ────────

static void resolve_sheriff_vote(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, svote_msg_id = 0, new_sheriff = 0;
    std::string vote_detail;
    bool is_tie = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id; svote_msg_id = g.sheriff_vote_msg_id;
        for (auto& p : g.players) {
            if (!p.alive) continue;
            bool is_cand = std::find(g.candidates.begin(), g.candidates.end(), p.uid) != g.candidates.end();
            bool withdrew = std::find(g.withdrawn_candidates.begin(), g.withdrawn_candidates.end(), p.uid) != g.withdrawn_candidates.end();
            if (is_cand || withdrew) continue;
            auto vit = g.sheriff_votes.find(p.uid);
            vote_detail += (vit != g.sheriff_votes.end() ? "✅ " : "⏳ ") + std::to_string(p.seat) + ". " + p.display_name;
            if (vit != g.sheriff_votes.end()) {
                if (!vit->second) vote_detail += " → **棄票**";
                else {
                    auto* tp = wfind(g, vit->second);
                    vote_detail += " → **" + (tp ? std::to_string(tp->seat) + ". " + tp->display_name : "?") + "**";
                }
            }
            vote_detail += "\n";
        }
        std::map<dpp::snowflake, int> tally;
        for (auto& [v, t] : g.sheriff_votes) if (t) tally[t]++;
        int best = 0;
        for (auto& [t, cnt] : tally) if (cnt > best) { best = cnt; new_sheriff = t; }
        if (best > 0) {
            int top_count = 0;
            for (auto& [t, cnt] : tally) if (cnt == best) top_count++;
            if (top_count > 1) { new_sheriff = 0; is_tie = true; } // 平票 → 撕毀警徽
        }
        if (new_sheriff) {
            g.sheriff_uid = new_sheriff;
            auto* p = wfind(g, new_sheriff); if (p) p->is_sheriff = true;
        }
    }
    if (svote_msg_id) {
        dpp::embed fe; fe.set_title("🏅  警長競選 — 結算完畢").set_color(0x808080);
        fe.set_description("投票已結算，請見下方明細。");
        dpp::message upd; upd.id = svote_msg_id; upd.channel_id = ch; upd.add_embed(fe);
        bot.message_edit(upd);
    }
    dpp::embed e;
    if (!vote_detail.empty()) e.add_field("📋 投票明細", vote_detail, false);
    if (new_sheriff) {
        e.set_title("🏅  警長選出！").set_color(0xF39C12);
        e.add_field("結果", "<@" + std::to_string((uint64_t)new_sheriff) + "> 當選警長！警長票計 **1.5 票**，死亡可傳徽。", false);
    } else if (is_tie) {
        e.set_title("🗳️  平票 — 警徽撕毀").set_color(0xE74C3C);
        e.add_field("結果", "競選**平票**，警徽撕毀，本局**無警長**。", false);
    } else {
        e.set_title("⏭  流票 — 無警長").set_color(0x808080);
        e.add_field("結果", "本局無警長。", false);
    }
    bot.message_create(dpp::message(ch, "").add_embed(e));
    announce_night_and_start_day(bot, gid);
}

// ─── Announce night deaths + start day ───────────────────────────────────────
// Called after sheriff election on day 1, or directly on day 2+.

static void announce_night_and_start_day(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    std::vector<dpp::snowflake> deaths;
    bool hunter_triggered = false;
    dpp::snowflake hunter_uid = 0;
    bool badge_needed = false;
    int day;
    std::string win_str;
    bool has_win = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        day = g.day;
        deaths = g.night_deaths;
        // Mark first-night deferred deaths as dead now
        for (auto uid : deaths) {
            auto* p = wfind(g, uid);
            if (p && p->alive) p->alive = false;
        }
        dpp::snowflake witch_poison = g.witch_poison_target;
        for (auto uid : deaths) {
            auto* p = wfind(g, uid);
            if (!p) continue;
            if (uid == g.sheriff_uid) badge_needed = true;
            // 獵人被女巫毒死不能開槍
            if (p->role == "獵人" && uid != witch_poison) { hunter_triggered = true; hunter_uid = uid; }
        }
        has_win = check_win(g, win_str);
    }

    // Post death announcement
    {
        dpp::embed e;
        e.set_title("☠️  昨晚死亡名單 — 第 " + std::to_string(day) + " 天").set_color(0xF39C12);
        if (deaths.empty()) {
            e.set_description("昨晚是 **平安夜**，無人死亡！");
        } else {
            std::string desc;
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& g = wolf_games[gid];
            for (auto uid : deaths) {
                auto* p = wfind(g, uid);
                if (p) desc += "☠️ **" + std::to_string(p->seat) + ". " + p->display_name + "**\n";
            }
            e.set_description("昨晚 **" + std::to_string(deaths.size()) + "** 人死亡：\n" + desc);
        }
        bot.message_create(dpp::message(ch, "").add_embed(e));
    }

    if (has_win) { end_game(bot, gid, win_str); return; }

    if (hunter_triggered) {
        WolfPhase after_h = badge_needed ? WolfPhase::BADGE_TRANSFER : WolfPhase::DAY_ANNOUNCE;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& g = wolf_games[gid];
            g.hunter_uid = hunter_uid;
            if (badge_needed) { g.badge_from = g.sheriff_uid; g.after_badge = WolfPhase::DAY_ANNOUNCE; }
        }
        trigger_hunter(bot, gid, after_h);
        return;
    }

    if (badge_needed) {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            wolf_games[gid].badge_from = wolf_games[gid].sheriff_uid;
        }
        trigger_badge(bot, gid, WolfPhase::DAY_ANNOUNCE);
        return;
    }

    // 夜晚死亡無遺言，直接進入發言順序
    proceed_to_speak_order(bot, gid);
}

// ─── Last words → speak order → day vote ─────────────────────────────────────

static void start_last_words(dpp::cluster& bot, uint64_t gid,
                              const std::vector<dpp::snowflake>& victims,
                              WolfPhase after) {
    if (victims.empty()) {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it != wolf_games.end()) it->second.after_last_words = after;
        }
        continue_last_words(bot, gid);
        return;
    }

    dpp::snowflake ch, cur_uid;
    std::string cur_name;
    int cur_seat = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        g.after_last_words = after;
        g.phase = WolfPhase::LAST_WORDS;
        // Take the first victim; subsequent ones (rare with new rules) handled when button clicked
        cur_uid = victims[0];
        g.lw_current_victim = cur_uid;
        auto* p = wfind(g, cur_uid);
        if (p) { cur_name = p->display_name; cur_seat = p->seat; }
    }
    dpp::embed e;
    e.set_title("💬  遺言時間").set_color(0x7F8C8D);
    e.set_description("<@" + std::to_string((uint64_t)cur_uid) + "> **（座位 " + std::to_string(cur_seat) + "）** 請說遺言\n說完後按「結束遺言」繼續。");
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 結束遺言").set_id("wolf_last_words_" + std::to_string(gid)).set_style(dpp::cos_primary));
    msg.add_component(row);
    msg.channel_id = ch;
    bot.message_create(msg);
}

static void continue_last_words(dpp::cluster& bot, uint64_t gid) {
    WolfPhase after = WolfPhase::WAITING;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        after = it->second.after_last_words;
    }
    if (after == WolfPhase::NIGHT_WOLVES) {
        start_night(bot, gid);
    } else if (after == WolfPhase::BADGE_TRANSFER) {
        trigger_badge(bot, gid, WolfPhase::NIGHT_WOLVES);
    } else {
        proceed_to_speak_order(bot, gid);
    }
}

static void proceed_to_speak_order(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    std::vector<dpp::snowflake> deaths;
    std::vector<int> speak;
    bool need_dir = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        deaths = g.night_deaths;
        need_dir = compute_speak_order(g, deaths);
        speak = g.speak_seats;
        if (need_dir) {
            g.phase = WolfPhase::SHERIFF_SPEAK_DIR;
            dpp::message dm = make_sheriff_dir_msg(g); dm.channel_id = ch;
            bot.message_create(dm);
            return;
        }
    }
    start_day_speak(bot, gid);
}

// ─── Sheriff speech phase ─────────────────────────────────────────────────────

static void start_sheriff_speech(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    dpp::message msg;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        g.phase = WolfPhase::SHERIFF_SPEECH;
        g.speak_idx = 0;
        // speak_seats already contains candidates (set by caller)
        if (g.speak_seats.empty()) {
            // No candidates left after withdrawals → go to vote
            g.phase = WolfPhase::SHERIFF_VOTE;
            msg = make_sheriff_vote_msg(g); msg.channel_id = ch;
            bot.message_create(msg, [gid](const dpp::confirmation_callback_t& cb){
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it != wolf_games.end())
                        it->second.sheriff_vote_msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
            return;
        }
        msg = make_speak_window_msg(g); msg.channel_id = ch;
    }
    bot.message_create(msg);
}

// ─── Day speak phase ──────────────────────────────────────────────────────────

static void start_day_speak(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    dpp::message msg;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        g.phase = WolfPhase::DAY_SPEAK;
        g.speak_idx = 0;
        if (g.speak_seats.empty()) {
            // No speakers → go directly to vote
            start_day_vote(bot, gid); return;
        }
        msg = make_speak_window_msg(g); msg.channel_id = ch;
    }
    bot.message_create(msg);
}

// ─── Advance speaker ──────────────────────────────────────────────────────────

static void advance_speaker(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    dpp::message msg;
    bool done = false;
    WolfPhase phase = WolfPhase::WAITING;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        phase = g.phase;
        g.speak_idx++;
        if (g.speak_idx >= (int)g.speak_seats.size()) {
            done = true;
        } else {
            msg = make_speak_window_msg(g); msg.channel_id = ch;
        }
    }
    if (done) {
        if (phase == WolfPhase::SHERIFF_SPEECH) {
            // All candidates spoke → start sheriff vote
            dpp::message vm;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                auto& g = it->second;
                g.phase = WolfPhase::SHERIFF_VOTE;
                vm = make_sheriff_vote_msg(g); vm.channel_id = ch;
            }
            bot.message_create(vm, [gid](const dpp::confirmation_callback_t& cb){
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it != wolf_games.end())
                        it->second.sheriff_vote_msg_id = std::get<dpp::message>(cb.value).id;
                }
            });
        } else {
            // DAY_SPEAK done → start day vote
            start_day_vote(bot, gid);
        }
    } else {
        bot.message_create(msg);
    }
}

static void resolve_pk_vote(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    dpp::snowflake eliminated = 0;
    bool hunter_triggered = false, badge_needed = false;
    dpp::snowflake hunter_uid = 0;
    std::string win_str;
    bool has_win = false;
    bool pk_tie = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        std::map<dpp::snowflake, double> tally;
        for (auto& [voter, tgt] : g.pk_votes) {
            double w = (voter == g.sheriff_uid) ? 1.5 : 1.0;
            tally[tgt] += w;
        }
        double best = 0;
        std::vector<dpp::snowflake> top;
        for (auto& [t, s] : tally) if (s > best) best = s;
        for (auto& [t, s] : tally) if (s == best) top.push_back(t);
        if (top.size() == 1) {
            eliminated = top[0];
            auto* p = wfind(g, eliminated);
            if (p && p->alive) {
                p->alive = false;
                if (eliminated == g.sheriff_uid) badge_needed = true;
                if (p->role == "獵人") { hunter_triggered = true; hunter_uid = eliminated; }
            }
        } else { pk_tie = true; }
        has_win = check_win(g, win_str);
    }
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& g = wolf_games[gid];
        dpp::embed e;
        if (pk_tie || !eliminated) {
            e.set_title("🗳️  PK 同票 — 無人處決").set_color(0x808080);
            e.set_description("PK 仍然平票，今天無人出局。");
        } else {
            auto* p = wfind(g, eliminated);
            bool pk_is_hunter = (p && p->role == "獵人");
            e.set_title("☠️  PK 出局").set_color(0xE74C3C);
            e.set_description("**" + std::to_string(p ? p->seat : 0) + ". " + (p ? p->display_name : "?") +
                              "** PK 出局！" + (pk_is_hunter ? "\n身分：**獵人 🏹**" : ""));
        }
        if (g.day_vote_msg_pk_id) {
            dpp::message upd; upd.id = g.day_vote_msg_pk_id; upd.channel_id = ch; upd.add_embed(e);
            bot.message_edit(upd);
        } else {
            bot.message_create(dpp::message(ch, "").add_embed(e));
        }
    }
    if (pk_tie || !eliminated) { start_night(bot, gid); return; }
    if (has_win) { end_game(bot, gid, win_str); return; }
    if (hunter_triggered) {
        WolfPhase after_h = badge_needed ? WolfPhase::BADGE_TRANSFER : WolfPhase::NIGHT_WOLVES;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& g = wolf_games[gid];
            g.hunter_uid = hunter_uid;
            if (badge_needed) { g.badge_from = g.sheriff_uid; g.after_badge = WolfPhase::NIGHT_WOLVES; }
        }
        trigger_hunter(bot, gid, after_h);
        return;
    }
    if (badge_needed) {
        { std::lock_guard<std::mutex> lk(data_mutex); wolf_games[gid].badge_from = wolf_games[gid].sheriff_uid; }
        trigger_badge(bot, gid, WolfPhase::NIGHT_WOLVES);
        return;
    }
    start_last_words(bot, gid, {eliminated}, WolfPhase::NIGHT_WOLVES);
}

static void start_day_vote(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    dpp::message vm;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        g.phase = WolfPhase::DAY_VOTE;
        g.day_votes.clear();
        ch = g.channel_id;
        vm = make_day_vote_msg(g);
        vm.channel_id = ch;
    }
    bot.message_create(vm, [gid](const dpp::confirmation_callback_t& cb){
        if (!cb.is_error()) {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it != wolf_games.end())
                it->second.day_vote_msg_id = std::get<dpp::message>(cb.value).id;
        }
    });
}

static void resolve_day_vote(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch;
    dpp::snowflake eliminated = 0;
    bool hunter_triggered = false, badge_needed = false;
    dpp::snowflake hunter_uid = 0;
    std::string win_str;
    bool has_win = false;
    std::string vote_detail;
    int vote_day = 0;
    dpp::snowflake vote_msg_id = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        ch = g.channel_id;
        vote_day = g.day;
        vote_msg_id = g.day_vote_msg_id;

        // Build full vote breakdown BEFORE modifying alive status
        for (auto& p : g.players) {
            if (!p.alive) continue;
            auto vit = g.day_votes.find(p.uid);
            if (vit == g.day_votes.end()) {
                vote_detail += "⏳ " + std::to_string(p.seat) + ". " + p.display_name + " → 未投票\n";
                continue;
            }
            vote_detail += "✅ " + std::to_string(p.seat) + ". " + p.display_name;
            if (p.uid == g.sheriff_uid) vote_detail += " 🏅×1.5";
            if (!vit->second) {
                vote_detail += " → **棄票**\n";
            } else {
                auto* tp = wfind(g, vit->second);
                vote_detail += " → **" + (tp ? std::to_string(tp->seat) + ". " + tp->display_name : "?") + "**\n";
            }
        }

        // Tally — skip abstain (target=0)
        std::map<dpp::snowflake, double> tally;
        for (auto& [voter, tgt] : g.day_votes) {
            if (!tgt) continue;
            double w = (voter == g.sheriff_uid) ? 1.5 : 1.0;
            tally[tgt] += w;
        }
        double best = 0;
        std::vector<dpp::snowflake> top;
        for (auto& [t, s] : tally) if (s > best) best = s;
        for (auto& [t, s] : tally) if (s == best) top.push_back(t);

        if (top.size() == 1) {
            eliminated = top[0];
            auto* p = wfind(g, eliminated);
            if (p && p->alive) {
                p->alive = false;
                if (eliminated == g.sheriff_uid) badge_needed = true;
                if (p->role == "獵人") { hunter_triggered = true; hunter_uid = eliminated; }
            }
        } else if (top.size() > 1) {
            g.pk_candidates = top;
            g.pk_votes.clear();
            g.phase = WolfPhase::DAY_VOTE_PK;
        }
        has_win = check_win(g, win_str);
    }

    // Freeze original vote message (remove buttons, keep embed)
    if (vote_msg_id) {
        dpp::embed fe;
        fe.set_title("🗳️  投票結束 — 第 " + std::to_string(vote_day) + " 天").set_color(0x808080);
        fe.set_description("投票已結算，請見下方明細。");
        dpp::message upd; upd.id = vote_msg_id; upd.channel_id = ch; upd.add_embed(fe);
        bot.message_edit(upd); // no components = buttons removed
    }

    // Tie → PK
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& g = wolf_games[gid];
        if (g.phase == WolfPhase::DAY_VOTE_PK) {
            dpp::message pm = make_day_pk_vote_msg(g);
            pm.channel_id = ch;
            bot.message_create(pm, [gid](const dpp::confirmation_callback_t& cb){
                if (!cb.is_error()) {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    auto it = wolf_games.find(gid);
                    if (it != wolf_games.end())
                        it->second.day_vote_msg_pk_id = std::get<dpp::message>(cb.value).id;
                }
            });
            return;
        }
    }

    // Post new result message with full breakdown
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& g = wolf_games[gid];
        dpp::embed e;
        e.set_title("🗳️  投票結果 — 第 " + std::to_string(vote_day) + " 天").set_color(0xE74C3C);
        if (!vote_detail.empty())
            e.add_field("📋 投票明細", vote_detail, false);
        if (!eliminated) {
            e.add_field("結果", "無人出局（平票或全棄票）", false);
            e.set_color(0x808080);
        } else {
            auto* p = wfind(g, eliminated);
            bool is_hunter = (p && p->role == "獵人");
            e.add_field("☠️ 出局", "**" + std::to_string(p ? p->seat : 0) + ". " +
                        (p ? p->display_name : "?") + "**" +
                        (is_hunter ? "　身分：**獵人 🏹**" : ""), false);
        }
        bot.message_create(dpp::message(ch, "").add_embed(e));
    }

    if (has_win) { end_game(bot, gid, win_str); return; }

    if (hunter_triggered) {
        WolfPhase after_h = badge_needed ? WolfPhase::BADGE_TRANSFER : WolfPhase::NIGHT_WOLVES;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& g = wolf_games[gid];
            g.hunter_uid = hunter_uid;
            if (badge_needed) { g.badge_from = g.sheriff_uid; g.after_badge = WolfPhase::NIGHT_WOLVES; }
        }
        trigger_hunter(bot, gid, after_h);
        return;
    }

    // Last words → then badge (if sheriff) → then night
    if (eliminated) {
        if (badge_needed) {
            std::lock_guard<std::mutex> lk(data_mutex);
            wolf_games[gid].badge_from = wolf_games[gid].sheriff_uid;
        }
        WolfPhase after_lw = badge_needed ? WolfPhase::BADGE_TRANSFER : WolfPhase::NIGHT_WOLVES;
        start_last_words(bot, gid, {eliminated}, after_lw);
        return;
    }
    start_night(bot, gid);
}

// ─── Game start ───────────────────────────────────────────────────────────────

// Called when host presses Start — assigns roles, sends DMs, creates wolf thread
static void begin_wolf_game(dpp::cluster& bot, uint64_t gid) {
    dpp::snowflake ch, guild;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_games.find(gid);
        if (it == wolf_games.end()) return;
        auto& g = it->second;
        assign_roles(g);
        ch = g.channel_id;
        guild = g.guild_id;
    }

    // Send role DMs
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        send_role_dms(bot, wolf_games[gid]);
    }

    // Announce player list (without roles)
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& g = wolf_games[gid];
        dpp::embed e;
        e.set_title("🐺  狼人殺 — 遊戲開始！").set_color(0x8B0000);
        e.set_description("角色已通過私訊發送，請查收！\n\n" + player_list_str(g));
        e.set_footer(dpp::embed_footer().set_text("第一晚入夜，天亮後進行警長競選"));
        bot.message_create(dpp::message(ch, "").add_embed(e));
    }

    // If game started inside a thread, we must create the wolf thread in the parent channel.
    // Fetch channel info first to detect parent_id.
    bot.channel_get(ch, [&bot, gid, ch](const dpp::confirmation_callback_t& gcb) {
        dpp::snowflake thread_ch = ch;
        if (!gcb.is_error()) {
            auto& chan = std::get<dpp::channel>(gcb.value);
            if (chan.parent_id) thread_ch = chan.parent_id;
        }

    // Create private wolf thread
    bot.thread_create("🐺 狼人密室", thread_ch, 1440, dpp::CHANNEL_PRIVATE_THREAD, false, 0,
    [&bot, gid](const dpp::confirmation_callback_t& cb){
        if (cb.is_error()) {
            // Fallback: skip thread, post wolves will DM
            dpp::snowflake ch2;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                ch2 = it->second.channel_id;
            }
            dpp::embed e; e.set_title("⚠️ 無法建立私人討論串").set_color(0xE74C3C)
                          .set_description("狼人討論串建立失敗，狼人請自行私訊協商！");
            bot.message_create(dpp::message(ch2, "").add_embed(e));

            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = wolf_games.find(gid);
                if (it == wolf_games.end()) return;
                it->second.wolf_thread_id = 0;
            }
            start_night(bot, gid);
            return;
        }
        auto& t = std::get<dpp::thread>(cb.value);
        dpp::snowflake thread_id = t.id;

        // Add wolf members
        std::vector<dpp::snowflake> wolf_uids;
        dpp::snowflake ch2;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it == wolf_games.end()) return;
            auto& g = it->second;
            g.wolf_thread_id = thread_id;
            ch2 = g.channel_id;
            for (auto& p : g.players)
                if (p.role == "狼人") wolf_uids.push_back(p.uid);
        }
        for (auto wu : wolf_uids)
            bot.thread_member_add(thread_id, wu);

        // Post welcome in wolf thread
        dpp::embed we;
        we.set_title("🐺  狼人密室").set_color(0x800000);
        we.set_description("只有狼人可以看到這裡！\n\n**狼人成員：**\n");
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = wolf_games.find(gid);
            if (it != wolf_games.end()) {
                std::string wm;
                for (auto& p : it->second.players)
                    if (p.role == "狼人") wm += "<@" + std::to_string((uint64_t)p.uid) + "> ";
                we.set_description(we.description + wm);
            }
        }
        bot.message_create(dpp::message(thread_id, "").add_embed(we));

        start_night(bot, gid);
    }); // thread_create callback
    }); // channel_get callback
}
