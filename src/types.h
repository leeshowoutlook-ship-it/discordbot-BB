#pragma once
#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <atomic>
#include <tuple>
#include <cmath>

// ─── Structs ──────────────────────────────────────────────────────────────────

struct Config {
    std::string token;
    std::string notify_user_id;
    std::string img_normal;
    std::string img_hard;
    std::string img_flame;
    std::string min_bet_thread_id;  // thread that enforces 1000 minimum bet
    std::string allin_thread_id;    // thread that enforces ALLIN (full balance, min 5000)
};

struct RegState {
    std::string boss;
    int         view_day = 0;  // 0=週四 … 6=週三
    std::set<std::pair<std::string,std::string>> slots; // (date_label, time_val)
};

struct Registration {
    uint64_t       id;
    dpp::snowflake user_id;
    dpp::snowflake channel_id;
    std::string    username;
    std::string    boss;
    std::vector<std::pair<std::string,std::string>> slots; // (date_label, time_val)
    std::string    position;
};

struct ProposedTeam {
    uint64_t       id;
    std::string    boss;
    std::string    day;       // date_label like "6/12(四)"
    std::string    time_slot;
    std::vector<Registration> members;
};

struct ActiveMsg {
    dpp::snowflake msg_id;
    dpp::snowflake channel_id;
};

struct Giveaway {
    uint64_t       id;
    dpp::snowflake channel_id;
    dpp::snowflake msg_id;
    dpp::snowflake host_id;
    std::string    prize;
    int            winner_count = 1;
    std::string    provider;          // optional mention string, e.g. "<@123>"
    std::string    mention;           // optional extra ping
    std::string    note;              // optional
    dpp::snowflake role_restriction;  // 0 = no restriction
    std::string    role_name;         // display name for role_restriction
    time_t         end_time = 0;
    int64_t        entry_cost = 0;  // chips required to join (0 = free)
    std::set<dpp::snowflake> participants;
    bool           ended = false;
};

using SlotKey = std::tuple<std::string, std::string, std::string>; // boss,day,time

// ─── Chip system ──────────────────────────────────────────────────────────────

struct ChipData {
    int64_t chips              = 0;
    time_t  last_claim         = 0;
    time_t  last_weekly        = 0;
    time_t  last_hunt_daily    = 0;
    time_t  last_weekly_scroll = 0;
    time_t  vip_until          = 0; // 尊爵VIP 到期時間
    time_t  vip_last_claim     = 0; // VIP 上次自動領取時間（獨立於手動領取）
    time_t  supervisor_until   = 0; // 寵物監工 到期時間
    time_t  insurance_until    = 0; // 醫療保險 到期時間
    int     free_xfer          = 0; // 免手續費轉帳次數
    time_t  risk_dice_day      = 0; // 园园的風險骰子：上次使用的時間（用來判斷是否跨天）
    int     risk_dice_uses     = 0; // 當天已使用次數（上限 2，跨天重置）
    int     claim_fail_streak  = 0; // 領取驗證連續答錯/逾時次數，每次領取成功歸零，用來延長鎖定時間
    int     claim_fail_total   = 0; // 累積總答錯/逾時次數，不會歸零
};

// ─── !領取 防腳本按鈕驗證 ───────────────────────────────────────────────────────
// 每次手動領取都有 CLAIM_VERIFY_CHANCE% 機率要求按鈕驗證才會真正發放
// （不限連續整點，避免靠跳過整點規避），提高固定排程腳本的模擬成本。用 data_mutex 保護。
struct ClaimChallenge {
    int                      correct_idx = 0;
    std::vector<std::string> options;    // 按鈕上顯示的文字（表情符號或數字答案）
    std::string              prompt;     // 驗證題目描述
    time_t                   expires_at  = 0;
    uint64_t                 token       = 0;  // 讓逾時計時器只處理自己那一次挑戰，不會誤傷後來新出的題目
    dpp::snowflake           channel_id;       // 逾時後要編輯提示訊息用（!領取，一般頻道訊息）
    dpp::snowflake           message_id;
    std::string              interaction_token; // 逾時後要編輯提示訊息用（/領取，ephemeral 只能靠這個編輯）
};
inline std::map<dpp::snowflake, ClaimChallenge> claim_challenges;
inline std::atomic<uint64_t> claim_challenge_token_seq{1};
static const int CLAIM_VERIFY_CHANCE   = 20; // 每次手動領取觸發驗證的機率（%）——不看是否連續整點，避免被規避
static const int CLAIM_VERIFY_SECS     = 60; // 驗證時限（秒）：逾時未按也會被鎖
static const int CLAIM_PENALTY_HOURS   = 2;  // 答錯／逾時：基礎鎖定時數，會依連續失敗次數累加
static const int CLAIM_PENALTY_MAX_HRS = 24; // 鎖定時數上限
inline bool g_claim_verify_enabled = true;   // 全局開關：false = 所有人領取跳過驗證

struct SignInSession {
    bool active = false;
    dpp::snowflake guild_id    = 0;
    dpp::snowflake channel_id  = 0;
    dpp::snowflake message_id  = 0;
    time_t deadline  = 0;       // Unix timestamp；0 = 無截止時間
    dpp::timer timer_id = 0;    // 截止時間自動結束 timer handle
    std::map<dpp::snowflake, std::string> signed_in;   // uid -> display_name
    std::map<dpp::snowflake, std::string> not_signed;  // uid -> display_name
};
inline SignInSession g_signin;

struct BankData {
    int64_t deposited         = 0;
    int64_t deposit_time      = 0;  // legacy, kept for JSON compat
    int64_t loan              = 0;
    int64_t loan_time         = 0;
    int64_t daily_min         = 0;  // 當天最低存款（計息基礎）
    int64_t last_interest_day = 0;  // 上次計息的 UTC+8 day number
};

struct PendingTransfer {
    dpp::snowflake from_uid, to_uid;
    int64_t        amount;
    std::string    from_name, to_name;
    time_t         created_at;
};

// ─── Blackjack stats ──────────────────────────────────────────────────────────

struct BJStats {
    int     wins   = 0;
    int     losses = 0;
    int     pushes = 0;
    int64_t profit = 0;
};

// ─── Dice stats ───────────────────────────────────────────────────────────────

struct DiceStats {
    int     wins   = 0;
    int     losses = 0;
    int64_t profit = 0;
};

// ─── Shop purchase record ─────────────────────────────────────────────────────

struct PurchaseRecord {
    uint64_t       id;
    dpp::snowflake uid;
    std::string    username;
    std::string    item_name;
    int64_t        price;
    time_t         timestamp;
    std::string    source;   // "maple" | "virtual"
};

// ─── Virtual pet ─────────────────────────────────────────────────────────────

struct Pet {
    std::string chain;        // "嫩寶" | "菇菇仔" | "肥肥" | "小企鵝"
    int         stage  = 0;   // 0=egg, 1, 2, 3
    int         exp    = 0;
    int         work_task = 0; // 0=none, 1=1hr, 4=4hr, 8=8hr
    time_t      work_end  = 0;
    std::string variant   = ""; // "" | "苔蘚" | "殭屍" | "藍菇" | "沙漠" | "企鵝王"
    std::string custom_name = "";
    std::string talent    = ""; // 天賦一，見 ALL_TALENTS（pet.h）
    std::string talent2   = ""; // 天賦二，需先用「第二天賦解鎖石」解鎖才能賦予；不可跟天賦一相同
    bool        talent2_unlocked = false; // 是否已使用第二天賦解鎖石
    std::vector<std::string> statuses; // "受傷"|"憂鬱"|"肌肉緊繃"|"疲勞"
    time_t      onsen_end  = 0;      // 泡溫泉到期時間 (0 = 未在泡溫泉)
    bool        is_supervisor_work = false; // 由監工自動派出的打工，收益×0.6
    bool        notify_after_work  = false; // 打工/溫泉完成後私訊通知
    bool        work_notified      = false; // 打工：已發送通知（防重複）
    bool        onsen_notified     = false; // 溫泉：已發送通知（防重複）
    int         enh_atk = 0; // 強化等級 0~10：攻擊力每層 +1%
    int         enh_def = 0; // 強化等級 0~10：防禦力每兩層 +1
    int         enh_hp  = 0; // 強化等級 0~10：生命值每層 +1%
};

// ─── Equipment ────────────────────────────────────────────────────────────────

struct PlayerEquipment {
    std::string weapon;   // GachaItem key or ""
    std::string glove;
    std::string clothes;
    std::string shoes;
    std::string orb;
};

// ─── Village game ─────────────────────────────────────────────────────────────

struct VillageSpirit {
    std::string name;
    int hp;
    int max_hp;
    int atk;
    int def;
};

struct VillageGame {
    dpp::snowflake uid;
    dpp::snowflake channel_id;
    dpp::snowflake msg_id  = 0;
    std::string    group_key;
    std::vector<VillageSpirit> spirits;
    int  pet_hp     = 0;
    int  pet_max_hp = 0;
    int  pet_atk    = 0;
    int  pet_def    = 0;
    int  pet_crit   = 0; // 江湖套裝：爆擊率%
    int  pet_hermes_atk_pct = 100; int pet_hermes_double_pct = 0; int pet_hermes_crit_dmg_pct = 0; // 赫耳墨斯套裝
    int  turn           = 1;
    int  selected_target = -1;
    time_t started_at = 0;
    dpp::timer timer_id = 0;
    std::string log_line;
    std::string orb_key;
    bool        latus_orb_triggered = false;
    int         bear_block_turns = 0;
    bool        underwear_first_atk_used = false; // 觀觀遺失的胖次：本場首次攻擊 +5 ATK 是否已用掉
    int         lifegoddess_uses = 0; // 生命女神的寶珠：單人回血已使用次數（上限3）
};

// ─── Adventure ────────────────────────────────────────────────────────────────

struct AdventureSetup {
    std::string    region_key;
    int            duration_hours  = 0;
    int64_t        funds           = -1;  // -1 = not set
    int            partner         = -1;  // -1 = unset, 0 = no pet, 1 = with pet
    bool           notify_on_finish = false;
    bool           star_boost       = false; // 投入星星：收取時防空包彈 + 探索度+10
    dpp::snowflake setup_msg_id    = 0;   // for modal response to edit original setup page
    dpp::snowflake setup_ch_id     = 0;
};

struct AdventureGame {
    dpp::snowflake uid;
    std::string region_key;
    int     duration_hours = 0;
    int64_t funds          = 0;
    bool    pet_along      = false;
    int     pet_stage      = 0; // 出發時同行寵物的階段（0=未帶寵物，鎖定出發當下的階段避免中途進化影響已算好的探索度）
    std::string pet_talent1, pet_talent2; // 出發時同行寵物的天賦（同樣鎖定出發當下，避免中途改天賦影響結果）
    time_t  start_time     = 0;
    time_t  end_time       = 0;
    bool    notify_on_finish = false; // 探險完成時私訊通知
    bool    finish_notified  = false; // 是否已發送過完成通知（防重複）
    bool    star_boost       = false; // 投入星星：防空包彈 + 探索度+10
};

// ─── Monster hunt active game ─────────────────────────────────────────────────

struct MonsterHuntGame {
    dpp::snowflake uid;
    dpp::snowflake channel_id;
    dpp::snowflake msg_id      = 0;
    std::string    difficulty;     // "easy"/"normal"/"hard"/"king"
    std::string    monster_key;
    std::string    monster_name;
    int            monster_hp     = 0;
    int            monster_max_hp = 0;
    int            monster_atk    = 0;
    int            monster_def    = 0;
    int            pet_hp         = 0;
    int            pet_max_hp     = 0;
    int            pet_atk        = 0;
    int            pet_def        = 0;
    int            pet_crit       = 0; // 江湖套裝：爆擊率%
    int            pet_hermes_atk_pct = 100; int pet_hermes_double_pct = 0; int pet_hermes_crit_dmg_pct = 0; // 赫耳墨斯套裝
    bool           player_first   = true;
    int            turn            = 1;
    time_t         started_at     = 0;
    dpp::timer     timer_id       = 0;
    std::string    log_line;
    std::string    orb_key;
    bool           battlecry_pending = false;
    int            atk_down_turns    = 0;  // wargod orb: turns of 60% monster ATK reduction remaining
    bool           latus_orb_triggered = false;
    bool           underwear_first_atk_used = false; // 觀觀遺失的胖次：本場首次攻擊 +5 ATK 是否已用掉
    int            lifegoddess_uses = 0; // 生命女神的寶珠：單人回血已使用次數（上限3）
};

// ─── Raid system ─────────────────────────────────────────────────────────────

struct RaidPlayer {
    dpp::snowflake uid;
    std::string    display_name;
    std::string    avatar_url;
    int            hp         = 0;
    int            max_hp     = 0;
    int            atk        = 0;
    int            def        = 0;
    int            crit_pct   = 0; // 江湖套裝：爆擊率%
    int            hermes_atk_pct = 100; int hermes_double_pct = 0; int hermes_crit_dmg_pct = 0; // 赫耳墨斯套裝
    std::string    orb_key;          // equipped orb
    bool           alive          = true;
    int            stunned_turns  = 0;     // turns remaining stunned (boss skill)
    bool           power_skip     = false; // skip next turn after using 強攻
    bool           speed_extra_used = false; // 迅捷：already got extra turn this round
    bool           battlecry_next = false; // next attack +25% (from 戰吼 targeting)
    bool           latus_orb_triggered = false;
    bool           underwear_first_atk_used = false; // 觀觀遺失的胖次：本場首次攻擊 +5 ATK 是否已用掉
};

struct RaidRoom {
    dpp::snowflake         channel_id;
    dpp::snowflake         host_uid;
    dpp::snowflake         msg_id   = 0;
    std::string            boss_key = "latus";
    std::vector<dpp::snowflake>     member_uids;
    std::map<dpp::snowflake, std::string> member_names;
    std::map<dpp::snowflake, std::string> member_avatars;
    time_t                 created_at = 0;
    bool                   practice_mode = false;
    dpp::timer             timer_id   = 0; // 10 分鐘逾時計時器；房間提早解散/開戰時要記得停掉，
                                            // 不然頻道換了新房間後，舊計時器到期還是會誤殺新房間
};

struct RaidGame {
    dpp::snowflake              channel_id;
    dpp::snowflake              msg_id         = 0;
    std::string                 boss_key;
    std::string                 boss_name;
    std::string                 boss_image;
    int                         boss_hp        = 0;
    int                         boss_max_hp    = 0;
    int                         boss_atk       = 0;
    int                         boss_def       = 0;
    std::vector<RaidPlayer>     players;
    int                         current_player = 0;  // index into players
    int                         round          = 1;
    bool                        boss_turn      = false; // true = boss acts next
    bool                        block_active        = false;  // 防禦 used this round
    bool                        round_first_action  = true;   // 先鋒：每輪第一個出手
    bool                        speed_extra_pending = false;  // 迅捷：extra turn queued
    bool                        last_boss_aoe       = false;  // 防連續 AOE 保護
    bool                        last_boss_single    = false;  // 單體後禁 AOE+單體
    // battlecry: src player uid -> target player idx
    std::map<dpp::snowflake, int> battlecry_pending;
    // cry target picking: src uid is waiting for target selection
    dpp::snowflake              cry_pending_uid = 0;
    std::string                 log_line;
    time_t                      started_at     = 0;
    dpp::timer                  timer_id       = 0;
    bool                        game_over      = false;
    bool                        victory        = false;
    bool                        practice_mode  = false;
    std::set<dpp::snowflake>    lifegoddess_used_by;      // 生命女神的寶珠：各玩家是否已使用（每人每場限1次）
};

// ─── 暗黑龍王 ─────────────────────────────────────────────────────────────────

struct DDHead {
    std::string name;
    int hp       = 0;
    int max_hp   = 0;
    int atk      = 0;
    int def      = 0;
    bool alive   = true;
    int skill_idx      = 0;    // 左/右頭技能輪轉 index（0-3）
    int chain_cd       = 0;    // 中頭：黑暗鎖鍊冷卻（1回合間隔）
    int rage_turns     = 0;    // 右頭：狂暴剩餘回合
    bool rage_triggered = false; // 右頭：已進入狂暴（只觸發一次）
};

struct DDPlayer {
    dpp::snowflake uid;
    std::string    display_name;
    std::string    avatar_url;
    int hp       = 0;
    int max_hp   = 0;
    int atk      = 0;
    int def      = 0;
    int crit_pct = 0; // 江湖套裝：爆擊率%
    int hermes_atk_pct = 100; int hermes_double_pct = 0; int hermes_crit_dmg_pct = 0; // 赫耳墨斯套裝
    std::string  orb_key;
    bool         alive          = true;
    bool         at_altar       = false;
    int          stunned_turns  = 0;
    bool         power_skip     = false;
    bool         speed_extra_used = false;
    bool         has_bomb       = false;
    int          bomb_turns     = 0;        // 倒數回合（0=死亡）
    int          atk_down_turns = 0;        // 力量竊取剩餘回合
    int          def_down_turns = 0;        // 防禦瓦解剩餘回合
    bool         burning        = false;    // 燃燒狀態（下回合+10傷）
    bool         latus_orb_triggered = false;
    bool         underwear_first_atk_used = false; // 觀觀遺失的胖次：本場首次攻擊 +5 ATK 是否已用掉
};

struct DDGame {
    dpp::snowflake            channel_id;
    dpp::snowflake            msg_id         = 0;
    std::vector<DDPlayer>     players;
    std::array<DDHead, 3>     heads;         // [0]=左頭 [1]=中頭 [2]=右頭
    int                       altar_hp       = 3;    // 祭壇血量（0=毀滅）
    int                       altar_counter  = 0;    // 連續有人在祭壇的 boss 回合數
    bool                      atk_triple     = false; // 祭壇毀滅後 ATK×3
    int                       current_player = 0;
    int                       round          = 1;
    bool                      boss_turn      = false;
    bool                      game_over      = false;
    bool                      victory        = false;
    bool                      practice_mode  = false;
    bool                      block_active   = false;
    bool                      round_first_action = true;
    bool                      speed_extra_pending = false;
    int                       bomb_cooldown  = 4;    // 中頭投彈冷卻（4~5回合）
    int                       selected_head  = -1;   // 玩家選擇的目標頭部
    std::string               log_line;
    time_t                    started_at     = 0;
    dpp::timer                timer_id       = 0;
    std::set<dpp::snowflake>  lifegoddess_used_by;      // 生命女神的寶珠：各玩家是否已使用（每人每場限1次）
};

// ─── Dice game ────────────────────────────────────────────────────────────────

struct DiceGame {
    uint64_t       id;
    dpp::snowflake uid;
    dpp::snowflake ch;
    int64_t        bet;
    int            choice     = 0;
    std::string    avatar_url;
    std::string    display_name;
};

// ─── Blackjack ────────────────────────────────────────────────────────────────

struct BJCard {
    int rank; // 1=A 2-10 11=J 12=Q 13=K
    int suit; // 0=♠ 1=♥ 2=♦ 3=♣
};

struct BJHand {
    std::vector<BJCard> cards;
    bool doubled = false;
    bool done    = false;
};

struct BJGame {
    uint64_t       id;
    dpp::snowflake user_id, channel_id, msg_id;
    int64_t        bet;
    std::vector<BJCard> deck;
    BJHand         main_hand;
    BJHand         split_hand;
    bool           has_split    = false;
    bool           split_active = false;
    std::vector<BJCard> dealer_cards;
    bool           game_over    = false;
    std::string    avatar_url;
    std::string    display_name;
};

// ─── 射龍門 ────────────────────────────────────────────────────────────────────

struct ShootStats {
    int     wins   = 0;
    int     losses = 0;
    int     bumps  = 0;   // 撞柱次數
    int     passes = 0;   // PASS 次數
    int64_t profit = 0;
};

struct ShootGame {
    dpp::snowflake uid;
    dpp::snowflake channel_id;
    dpp::snowflake msg_id;
    int64_t bet = 0;
    int c1 = 0; // 0-51: rank=c/4+1 (1=A..13=K), suit=c%4 (0=♠1=♥2=♦3=♣)
    int c2 = 0;
    std::string avatar_url;
    std::string display_name;
};

// ─── 火箭升空 ──────────────────────────────────────────────────────────────────

struct RocketStats {
    int     wins   = 0;
    int     losses = 0;
    int64_t profit = 0;
};

struct RocketGame {
    dpp::snowflake uid;
    dpp::snowflake channel_id;
    int64_t        bet      = 0;
    int            presses  = 0;   // 0-10
    std::string    avatar_url;
    std::string    display_name;
};

// ─── 刮刮樂 ──────────────────────────────────────────────────────────────────

struct ScratchStats {
    int     wins   = 0;
    int     losses = 0;
    int64_t profit = 0;
};

struct WolfPlayerStats {
    int good_games = 0;
    int good_wins  = 0;
    int bad_games  = 0;
    int bad_wins   = 0;
};

struct ONWStats {
    int wolf_games    = 0;
    int wolf_wins     = 0;
    int village_games = 0;
    int village_wins  = 0;
    int tanner_games  = 0;
    int tanner_wins   = 0;
};

// sq[i]: -1=炸彈, 0=空格, 10=1x, 15=1.5x, 20=2x
struct ScratchGame {
    dpp::snowflake     uid;
    dpp::snowflake     channel_id;
    int64_t            bet           = 0;
    int64_t            total_paid    = 0;  // bet × (1 + extra_scratches)
    std::array<int, 9> sq            = {};
    uint16_t           revealed      = 0;  // bitmask
    int                safe_scratches = 0; // non-bomb squares revealed
    bool               extra_mode    = false; // paid for 1 extra scratch, waiting to use
    int                extra_count   = 0;    // how many extra scratches used (max 4)
    std::string        avatar_url;
    std::string        display_name;
};

// ─── Global state ─────────────────────────────────────────────────────────────

inline Config cfg;
inline std::atomic<uint64_t> reg_counter{1};
inline std::atomic<uint64_t> team_counter{1};
inline std::atomic<uint64_t> giveaway_counter{1};
inline std::atomic<uint64_t> bj_counter{1};

inline std::map<dpp::snowflake, RegState>       user_states;
inline std::map<dpp::snowflake, std::string>    view_filters;
inline std::map<dpp::snowflake, dpp::snowflake> msg_owner;
inline std::map<dpp::snowflake, ActiveMsg>      user_active_msg;
inline std::map<uint64_t, ProposedTeam>         proposed_teams;
inline std::set<SlotKey>                        proposed_slots;
inline std::vector<Registration>                registrations;
inline std::map<uint64_t, Giveaway>             giveaways;
inline std::map<dpp::snowflake, ChipData>       chip_data;
inline std::map<dpp::snowflake, BankData>       bank_data;
inline std::map<uint64_t, BJGame>               bj_games;
inline std::map<dpp::snowflake, uint64_t>       user_bj; // uid -> game id
inline std::map<uint64_t, std::string>          emoji_tag_map;       // emoji_id   -> "<:name:id>"
inline std::map<std::string, std::string>       emoji_name_map;      // emoji_name -> "<:name:id>"
struct WarnRecord {
    time_t      timestamp = 0;
    std::string reason;
};
inline std::map<dpp::snowflake, std::vector<WarnRecord>> warn_data;
inline std::map<dpp::snowflake, BJStats>        bj_stats_data;
inline std::map<dpp::snowflake, DiceStats>      dice_stats_data;
inline std::map<uint64_t, PendingTransfer>      pending_transfers;
inline std::atomic<uint64_t>                    transfer_counter{1};
inline std::map<uint64_t, DiceGame>             dice_games;
inline std::map<dpp::snowflake, uint64_t>       user_dice;
inline std::atomic<uint64_t>                    dice_counter{1};
inline std::vector<PurchaseRecord>              purchase_records;
inline std::atomic<uint64_t>                    purchase_counter{1};
inline std::map<dpp::snowflake, Pet>            pet_data;
inline std::map<dpp::snowflake, std::map<std::string,int>> inventory_data;
inline std::map<uint64_t, int>                              gacha_pity_data; // 一般池保底計數器
inline std::map<uint64_t, int>                              gacha_hero_pity_data; // 俠客之路池保底計數器（獨立計算）
inline std::map<uint64_t, int>                              gacha_mystery_pity_data; // 神秘轉蛋池保底計數器（獨立計算，尚未開放）
inline std::map<dpp::snowflake, ShootGame>      shoot_games;
inline std::map<dpp::snowflake, ShootStats>     shoot_stats_data;
inline std::map<dpp::snowflake, RocketGame>     rocket_games;
inline std::map<dpp::snowflake, RocketStats>    rocket_stats_data;
inline std::map<dpp::snowflake, ScratchGame>    scratch_games;
inline std::map<dpp::snowflake, ScratchStats>   scratch_stats_data;
inline std::map<dpp::snowflake, WolfPlayerStats> wolf_player_stats_data;
inline std::map<dpp::snowflake, ONWStats>           onw_stats_data;
inline std::map<dpp::snowflake, PlayerEquipment>    equipped_data;
inline std::map<dpp::snowflake, std::set<std::string>> hunt_clear_data;
inline std::map<dpp::snowflake, MonsterHuntGame>    monster_hunt_games;
inline std::map<dpp::snowflake, VillageGame>        village_games;
inline std::map<dpp::snowflake, AdventureSetup>     adv_setups;
inline std::map<dpp::snowflake, AdventureGame>      adv_games;
inline std::map<dpp::snowflake, RaidRoom>           raid_rooms;

// 左邊畫個龍的柴犬百科全書：全球唯一道具，次數全域計算（不因交易換人重置），用 data_mutex 保護
inline int g_dogbook_week      = 0;
inline int g_dogbook_uses_left = 0;

// 寵物聖物加成（需持有 data_mutex 呼叫）
inline int col_pet_atk_bonus(dpp::snowflake uid) {
    auto it = inventory_data.find(uid); if (it == inventory_data.end()) return 0;
    auto jt = it->second.find("col_shark_relic");
    return (jt != it->second.end() && jt->second > 0) ? 1 : 0;
}
inline int col_pet_def_bonus(dpp::snowflake uid) {
    auto it = inventory_data.find(uid); if (it == inventory_data.end()) return 0;
    auto jt = it->second.find("col_penguin_relic");
    return (jt != it->second.end() && jt->second > 0) ? 1 : 0;
}
inline int col_pet_hp_bonus(dpp::snowflake uid) {
    auto it = inventory_data.find(uid); if (it == inventory_data.end()) return 0;
    auto jt = it->second.find("col_koala_relic");
    return (jt != it->second.end() && jt->second > 0) ? 10 : 0;
}

// 收藏套組完成判定（需持有 data_mutex 呼叫）
inline bool col_all_owned(dpp::snowflake uid, std::initializer_list<const char*> keys) {
    auto it = inventory_data.find(uid); if (it == inventory_data.end()) return false;
    for (auto k : keys) { auto jt = it->second.find(k); if (jt == it->second.end() || jt->second <= 0) return false; }
    return true;
}
// 初級：寵物攻擊力 ×1.01
inline bool col_set_mushroom_basic(dpp::snowflake uid) { return col_all_owned(uid, {"col_ms_handkerchief","col_gm_beret","col_sm_spine"}); }
// 初級：寵物生命值 ×1.01
inline bool col_set_water_basic(dpp::snowflake uid)    { return col_all_owned(uid, {"col_gwl_popsicle","col_bwl_cake"}); }
// 初級：寵物防禦力 ×1.02
inline bool col_set_ghost_basic(dpp::snowflake uid)    { return col_all_owned(uid, {"col_ghost_heels","col_kappa_cucumber","col_zombie_eyepatch","col_ghost_cloak"}); }
// 中級：探險時長 -1%（三區各一）
inline bool col_set_mushroom_mid(dpp::snowflake uid)   { return col_all_owned(uid, {"col_bm_tear","col_zm_cheese"}); }
inline bool col_set_water_mid(dpp::snowflake uid)      { return col_all_owned(uid, {"col_dwl_tiramisu","col_rwl_velvet"}); }
inline bool col_set_ghost_mid(dpp::snowflake uid)      { return col_all_owned(uid, {"col_witch_broom"}); }
inline int col_adv_reduction_count(dpp::snowflake uid) {
    return (col_set_mushroom_mid(uid)?1:0)+(col_set_water_mid(uid)?1:0)+(col_set_ghost_mid(uid)?1:0);
}
// BB自然博物館：Xu的探險放大鏡，持有時探索時長額外 -5%（跟上面的 -1% 疊乘各自獨立相乘）
inline bool col_has_bb_magnifier(dpp::snowflake uid) {
    auto it = inventory_data.find(uid);
    return it != inventory_data.end() && it->second.count("col_bb_magnifier") && it->second.at("col_bb_magnifier") > 0;
}
// 赤龍山脈：貓哥的戀愛教典，持有時轉帳／交易免手續費、虛擬商店95折（需持有 data_mutex 呼叫）
inline bool col_has_lovebook(dpp::snowflake uid) {
    auto it = inventory_data.find(uid);
    return it != inventory_data.end() && it->second.count("col_rd_lovebook") && it->second.at("col_rd_lovebook") > 0;
}
// 高級
inline bool col_set_mushroom_adv(dpp::snowflake uid)   { return col_all_owned(uid, {"col_mushroom_head","col_mb_crown","col_mb_staff"}); }
inline bool col_set_water_adv(dpp::snowflake uid)      { return col_all_owned(uid, {"col_awl_avocado","col_sqwl_brownie","col_ywl_caramel"}); }
inline bool col_set_ghost_adv(dpp::snowflake uid)      { return col_all_owned(uid, {"col_demon_tear","col_demon_heart","col_demon_horn","col_demon_costume"}); }

// BB自然博物館：初級強化成功率+5%／中級狩獵與王團獎勵+3%／高級銀行借款利率-0.5%
inline bool col_set_bb_basic(dpp::snowflake uid) { return col_all_owned(uid, {"col_bb_pink_cup","col_bb_desk_terror","col_bb_signus_chalice","col_bb_mercury_staff"}); }
inline bool col_set_bb_mid(dpp::snowflake uid)   { return col_all_owned(uid, {"col_bb_horn","col_bb_death_ring","col_bb_ski"}); }
inline bool col_set_bb_adv(dpp::snowflake uid)   { return col_all_owned(uid, {"col_bb_blood_gem","col_bb_bracelet","col_bb_mirror"}); }

// 赤龍山脈：初級強化所需金額-5%／中級-10%／高級-15%（同乘區，加總後一次套用）
inline bool col_set_rd_basic(dpp::snowflake uid) { return col_all_owned(uid, {"col_rd_amber","col_rd_claw","col_rd_azurescale","col_rd_azuremarrow","col_rd_earthbone","col_rd_earthblood"}); }
inline bool col_set_rd_mid(dpp::snowflake uid)   { return col_all_owned(uid, {"col_rd_iceeye","col_rd_icescale","col_rd_blackwing","col_rd_blackeye","col_rd_demonclaw","col_rd_demoneye","col_rd_rainbow"}); }
inline bool col_set_rd_adv(dpp::snowflake uid)   { return col_all_owned(uid, {"col_rd_azureorb","col_rd_redorb","col_rd_iceorb","col_rd_blackorb","col_rd_demonorb","col_rd_earthorb"}); }

// 初級套組 + 強化等級的攻/血/防加成，共用同一個「乘區」相加，最後一次套用乘法
// （而非逐一連乘取整）。以後同類加成變多時直接在這裡加一行 += 即可，不會因多次
// 連乘造成誤差或超出預期倍率。防禦力的強化是固定值（非百分比），獨立疊加。
inline void apply_pet_basic_set_bonus(dpp::snowflake uid, const Pet& pet, int& atk, int& hp, int& max_hp, int& def) {
    double atk_mult = 0.0, hp_mult = 0.0, def_mult = 0.0;
    if (col_set_mushroom_basic(uid)) atk_mult += 0.01;
    if (col_set_water_basic(uid))    hp_mult  += 0.01;
    if (col_set_ghost_basic(uid))    def_mult += 0.02;
    atk_mult += pet.enh_atk * 0.01;
    hp_mult  += pet.enh_hp  * 0.01;
    if (atk_mult > 0) atk = (int)std::ceil(atk * (1.0 + atk_mult));
    if (hp_mult  > 0) { hp = (int)std::ceil(hp * (1.0 + hp_mult)); max_hp = (int)std::ceil(max_hp * (1.0 + hp_mult)); }
    if (def_mult > 0) def = (int)std::ceil(def * (1.0 + def_mult));
    def += pet.enh_def / 2;
}

// 背包分頁的返回按鈕（V2 訊息用）
inline void add_bag_home_button(dpp::message& msg, dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📋 背包首頁").set_id("bag_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
}

// channel_id -> room
inline std::map<dpp::snowflake, RaidGame>           raid_games;       // channel_id -> game
inline std::map<dpp::snowflake, DDGame>             dd_games;         // channel_id -> game
inline std::atomic<uint64_t>                        raid_counter{1};
inline std::mutex                               data_mutex;
inline std::mutex                               io_mutex;   // serializes disk writes, held only during atomic_write

// ─── 交易系統 ─────────────────────────────────────────────────────────────────

struct TradeOffer {
    uint64_t       id           = 0;
    dpp::snowflake from_uid;
    dpp::snowflake to_uid;
    dpp::snowflake channel_id;
    time_t         created_at   = 0;
    int            from_item_id = 0;  // 0 = no item
    int64_t        from_qty     = 1;  // from_item_id 的數量
    int64_t        from_chips   = 0;
    int            to_item_id   = 0;  // 0 = no item
    int64_t        to_qty       = 1;  // to_item_id 的數量
    int64_t        to_chips     = 0;
};
inline std::map<uint64_t, TradeOffer> trade_offers;
inline std::atomic<uint64_t>          trade_counter{1};

// ─── 一夜狼人 ─────────────────────────────────────────────────────────────────

enum class ONWPhase {
    WAITING,
    NIGHT_WOLVES,
    NIGHT_SEER,
    NIGHT_ROBBER,
    NIGHT_TROUBLEMAKER,
    NIGHT_VILLAGE_IDIOT,
    NIGHT_WITCH,
    NIGHT_DRUNK,
    NIGHT_INSOMNIAC,
    DAY_DISCUSS,
    DAY_VOTE,
    GAME_OVER
};

struct ONWPlayer {
    dpp::snowflake uid;
    std::string    display_name;
    std::string    original_role;
    std::string    current_role;
    dpp::snowflake vote_target = 0;
};

struct ONWGame {
    uint64_t       id;
    dpp::snowflake channel_id;
    dpp::snowflake guild_id;
    dpp::snowflake host_id;
    dpp::snowflake lobby_msg_id = 0;
    dpp::snowflake vote_msg_id  = 0;
    ONWPhase       phase = ONWPhase::WAITING;

    std::vector<ONWPlayer>    players;
    std::array<std::string,3> center = {};

    // Role pool (host configures in lobby)
    std::map<std::string,int> role_counts;

    // Night state
    std::set<dpp::snowflake> wolves_confirmed; // 已確認的一般狼人
    bool           wolf_done         = false;
    bool           seer_done         = false;
    bool           robber_done       = false;
    dpp::snowflake tm_first          = 0;   // troublemaker first pick
    bool           troublemaker_done = false;
    bool           drunk_done        = false;
    bool           insomniac_done    = false;
    bool           alpha_done        = true;  // 頭狼行動完成（預設true；有頭狼+中央狼人牌時設false）
    bool           vi_done           = false;
    bool           witch_done        = false;
    bool           witch_peeked      = false; // 女巫已偷看，等待交換/略過
    int            witch_center      = -1;    // 女巫偷看的中央牌 index

    std::vector<std::string> night_log; // post-game recap
};

inline std::map<uint64_t, ONWGame>        onw_games;
inline std::map<dpp::snowflake, uint64_t> channel_onw_game;
inline std::atomic<uint64_t>              onw_counter{1};

// ─── 誰是臥底 ─────────────────────────────────────────────────────────────────

enum class UCPhase { WAITING, DESCRIBING, VOTING, VOTE_PK, GAME_OVER };

struct UCPlayer {
    dpp::snowflake uid;
    std::string    display_name;
    int            seat          = 0;
    bool           is_undercover = false;
    bool           is_blank      = false;  // white-board mode: no word
    bool           alive         = true;
};

struct UCGame {
    uint64_t       id           = 0;
    dpp::snowflake channel_id;
    dpp::snowflake guild_id;
    dpp::snowflake host_id;
    UCPhase        phase        = UCPhase::WAITING;

    std::vector<UCPlayer> players;
    std::string civilian_word;
    std::string undercover_word;  // empty in blank_mode

    bool blank_mode    = false;   // true = 白板模式, false = 臥底模式
    bool adult_allowed = false;   // 僅 !臥底 遊玩成人內容 才開啟
    std::string word_pool = "general";

    int round     = 1;
    std::vector<dpp::snowflake> seat_order;   // permanent order set in round 1
    std::vector<dpp::snowflake> speak_order;  // alive players for current round
    int speak_pos = 0;

    std::map<dpp::snowflake, std::string>     answers;   // uid → answer text for current round
    std::map<dpp::snowflake, dpp::snowflake> votes;
    std::vector<dpp::snowflake> pk_candidates;
    std::map<dpp::snowflake, dpp::snowflake> pk_votes;

    dpp::snowflake pending_elim = 0;  // player waiting to guess (0 = none)
    dpp::timer     guess_timer  = 0;  // 30-second guess countdown

    dpp::snowflake lobby_msg_id    = 0;
    dpp::snowflake describe_msg_id = 0;
    dpp::snowflake vote_msg_id     = 0;
    dpp::snowflake pk_msg_id       = 0;
};

struct UCStats {
    int civ_games = 0, civ_wins = 0;
    int spy_games = 0, spy_wins = 0;
};

inline std::map<uint64_t, UCGame>          uc_games;
inline std::map<dpp::snowflake, uint64_t>  channel_uc_game;
inline std::atomic<uint64_t>               uc_counter{1};
inline std::map<dpp::snowflake, UCStats>   uc_stats_data;

// ── 猜數字 ────────────────────────────────────────────────────────────────────
struct GuessGame {
    dpp::snowflake uid;
    dpp::snowflake channel_id;
    std::string    secret;
    int            attempts     = 0;
    static const int MAX_ATTEMPTS = 10;
    std::vector<std::pair<std::string, std::string>> history;
    dpp::snowflake msg_id       = 0;
    std::string    avatar_url;
    std::string    display_name;
    std::string    input_buf;
};

struct GuessStats {
    int games             = 0;
    int wins              = 0;
    int total_win_attempts = 0;  // sum of attempts for won games → avg = total_win_attempts/wins
};

inline std::map<dpp::snowflake, GuessGame> guess_games;
inline std::map<uint64_t, GuessStats>      guess_stats_data;

// ── 輪盤統計 ──────────────────────────────────────────────────────────────────
struct RouletteStats {
    int     wins   = 0;
    int     losses = 0;
    int64_t profit = 0;
};
inline std::map<dpp::snowflake, RouletteStats> roulette_stats_data;

// ── 輪盤賭 ────────────────────────────────────────────────────────────────────
struct RouletteSideBet {
    dpp::snowflake uid;
    std::string    display_name;
    std::string    bet_type;   // "p1"|"p2"|"odd"|"even"|"ch1"~"ch6"
    int64_t        amount = 0;
};

struct RouletteRoom {
    // ── 房間設定 ──
    dpp::snowflake channel_id;
    dpp::snowflake msg_id      = 0;
    dpp::snowflake p1_uid      = 0;
    std::string    p1_name, p1_avatar;
    dpp::snowflake p2_uid      = 0;
    std::string    p2_name, p2_avatar;
    dpp::snowflake invited_uid  = 0;  // 0 = 開放任何人加入
    std::string    invited_name;      // 邀請對象的顯示名稱
    int64_t        stake       = 0;
    std::vector<RouletteSideBet> side_bets;  // 旁觀者邊注（遊戲前下注）
    // ── 遊戲狀態 ──
    // 規則：每回合必須先射一槍，miss 後可繼續射或 PASS 給對方。
    //       射了第 5 發後不能繼續射第 6 發，只能 PASS。
    bool started         = false;
    int  bullet_chamber  = 0;  // 1-6，遊戲開始時決定（雙方不知）
    int  current_chamber = 1;  // 當前要射的發數
    int  active_player   = 1;  // 1=P1, 2=P2
    int  shots_this_turn = 0;  // 本回合已射幾槍（PASS 時歸零）
    int  shot5_shooter   = 0;  // 射了第 5 發的人（0=未到）
    bool game_over       = false;
    int  loser           = 0;  // 1=P1, 2=P2
    dpp::timer timer_id  = 0;  // 10分鐘自動解散
};

inline std::map<dpp::snowflake, RouletteRoom> roulette_rooms;

// ── 猜拳 ──────────────────────────────────────────────────────────────────────
struct RpsStats {
    int     wins   = 0;
    int     losses = 0;
    int64_t profit = 0;
};
inline std::map<dpp::snowflake, RpsStats> rps_stats_data;

struct RpsGame {
    dpp::snowflake host_uid   = 0;
    dpp::snowflake channel_id = 0;
    dpp::snowflake message_id = 0;
    int64_t        bet        = 0;
    bool           started    = false;
    bool           draw_state = false;
    std::map<dpp::snowflake, std::string> players;    // uid → display_name
    std::map<dpp::snowflake, std::string> avatars;    // uid → avatar_url
    std::map<dpp::snowflake, int>         choices;    // uid → 0=未選 1=石頭 2=剪刀 3=布
    std::map<dpp::snowflake, bool>        draw_votes; // uid → true=再來 false=離場
};
inline std::map<dpp::snowflake, RpsGame> rps_games; // key = channel_id

// 組隊遠征名單顯示用：每種寶珠固定的代表圖示（不論有沒有觸發都顯示，讓隊友知道彼此帶什麼寶珠）。
// raid.h（王團）跟 darkdragon.h（暗黑龍）共用同一份，避免兩邊各自維護出現不一致。
static inline std::string orb_baseline_icon(const std::string& orb_key) {
    if (orb_key == "EQ_K_UR")     return "🌟";
    if (orb_key == "EQ_K_SPEED")  return "🐺";
    if (orb_key == "EQ_K_ATHENA") return "💚";
    if (orb_key == "EQ_K_BEAR")   return "🐻";
    if (orb_key == "EQ_K_VIKING") return "⚔️";
    if (orb_key == "EQ_K_WARGOD") return "💢";
    if (orb_key == "EQ_K_LATUS")  return "🔶";
    if (orb_key == "EQ_K_LIFEGODDESS") return "💗";
    return "";
}

// ─── 大廳公告（管理員可編輯，只保留最新一則）───────────────────────────────────
struct Announcement {
    std::string text;
    std::string updated_by; // 顯示名稱
    time_t      updated_at = 0;
};
inline Announcement g_announcement; // 用 data_mutex 保護，跟其他玩家資料一致

// ─── 股票系統 ─────────────────────────────────────────────────────────────────

struct StockInfo {
    std::string key;          // "0050" | "tsmc" | "yageo" | "btc" | "mood"
    std::string name;         // 顯示名稱
    std::string ticker;       // Yahoo Finance 代號，"" = 不接 API（心情股）
    int64_t     price      = 0; // 目前價格（碼／股）
    int64_t     prev_close = 0; // 前一次價格，用來算漲跌
    time_t      last_update = 0;
    bool        fetch_ok    = true; // 上次抓取是否成功（心情股恆為 true）
    std::vector<int64_t> history; // 走勢圖用的近期收盤價（僅存於記憶體，不落地存檔，重啟後重新累積即可）
    int64_t     day_open_price = 0; // 當日開盤價，僅供有每日漲跌幅限制的手動股票使用（例如貓哥的心情）
    int64_t     day_number     = 0; // 上次更新對應的 UTC+8 day number，跨天時重置 day_open_price
};
inline std::map<std::string, StockInfo> stock_market;   // key -> 目前市場資訊
inline std::mutex                       stock_mutex;    // 保護 stock_market（跟 data_mutex 分開，避免跟遊戲邏輯互相卡）

struct StockHolding {
    int64_t shares   = 0;
    int64_t avg_cost = 0; // 平均成本（碼／股），賣出時算損益用
};
inline std::map<dpp::snowflake, std::map<std::string, StockHolding>> player_stocks; // uid -> key -> holding（用 data_mutex 保護，跟其他玩家資料一致）

// 股票的靜態定義（純資料，跟抓價/UI邏輯分開放，讓 adventure.h 的背包特殊分頁也能直接引用）
struct StockDef {
    std::string key, name, ticker, emoji, desc;
    int         item_id = 0;
    std::string controller_uid = ""; // 手動股票專用：可調整價格的使用者ID字串。空="" 代表使用 cfg.notify_user_id（機器人擁有者）
    int         daily_cap_pct  = 0;  // 手動股票專用：單日最大漲跌幅百分比，0=無限制
};
static const std::vector<StockDef> STOCK_DEFS = {
    // ── 第一頁前三個：心情股（手動，無API）────────────────────────────────────
    {"stock_mood",  "LeeShoW的心情",      "",        "😶", "價格全憑 LeeShoW 心情決定，僅供娛樂。", 98005}, // ticker="" 代表不接 API，價格由管理員手動調整
    {"stock_catbro","貓哥的心情",      "",        "😶", "價格全憑貓哥心情決定，但每天最多只能浮動5%，僅供娛樂。", 98011,
        "604244623124070423", 5}, // ticker="" 不接API；只有這個使用者能調整；單日漲跌幅限制5%
    {"stock_purse", "皮包的心情",      "",        "😶", "價格全憑皮包心情決定，但每天最多只能浮動5%，僅供娛樂。", 98012,
        "353567910285017090", 5}, // ticker="" 不接API；只有這個使用者能調整；單日漲跌幅限制5%
    {"stock_0050",  "元大台灣50 (0050)", "0050.TW", "📈", "追蹤台灣市值前50大企業的ETF，波動較穩健。", 98001},
    {"stock_tsmc",  "台積電 (2330)",      "2330.TW", "🏭", "台灣護國神山，全球晶圓代工龍頭。", 98002},
    // ── 第二頁 ───────────────────────────────────────────────────────────────
    {"stock_yageo", "國巨 (2327)",        "2327.TW", "🔧", "被動元件大廠，波動較大。", 98003},
    {"stock_btc",   "比特幣",             "BTC-USD", "₿",  "去中心化加密貨幣，價格波動劇烈。", 98004},
    {"stock_nvda",  "NVIDIA",         "NVDA",    "🖥️", "AI晶片霸主，近年漲幅驚人，波動劇烈。", 98007},
    {"stock_mstr",  "MicroStrategy",  "MSTR",    "💾", "全倉押比特幣的公司，波動比 BTC 還激烈。", 98008},
    {"stock_ntdo",  "任天堂",          "7974.T",  "🎮", "日本遊戲巨頭，Switch 系列創造者（日圓計價）。", 98009},
};
static const StockDef* find_stock_def(const std::string& key) {
    for (auto& d : STOCK_DEFS) if (d.key == key) return &d;
    return nullptr;
}
static const StockDef* find_stock_def_by_id(int id) {
    if (!id) return nullptr;
    for (auto& d : STOCK_DEFS) if (d.item_id == id) return &d;
    return nullptr;
}

// V2 helper：把文字（與可選縮圖）包成一個 section，再加入 container
// 若 thumb_url 為空，退化成純 text_display（不使用 section）
static inline dpp::component v2_section(const std::string& text, const std::string& thumb_url = "") {
    if (thumb_url.empty())
        return dpp::component().set_type(dpp::cot_text_display).set_content(text);
    return dpp::component().set_type(dpp::cot_section)
        .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
        .set_accessory(dpp::component().set_type(dpp::cot_thumbnail).set_thumbnail(thumb_url));
}
