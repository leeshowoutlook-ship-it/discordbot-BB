#pragma once
#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <atomic>
#include <tuple>

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
    int64_t chips       = 0;
    time_t  last_claim  = 0;
    time_t  last_weekly = 0;
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
    std::string talent    = ""; // "" | "迅捷" | "招人喜歡" | "幸運" | "天然呆" | "喜歡作夢"
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
inline std::map<dpp::snowflake, ShootGame>      shoot_games;
inline std::map<dpp::snowflake, ShootStats>     shoot_stats_data;
inline std::map<dpp::snowflake, RocketGame>     rocket_games;
inline std::map<dpp::snowflake, RocketStats>    rocket_stats_data;
inline std::map<dpp::snowflake, ScratchGame>    scratch_games;
inline std::map<dpp::snowflake, ScratchStats>   scratch_stats_data;
inline std::map<dpp::snowflake, WolfPlayerStats> wolf_player_stats_data;
inline std::mutex                               data_mutex;

// ─── 交易系統 ─────────────────────────────────────────────────────────────────

struct TradeOffer {
    uint64_t       id           = 0;
    dpp::snowflake from_uid;
    dpp::snowflake to_uid;
    dpp::snowflake channel_id;
    int            from_item_id = 0;  // 0 = no item
    int64_t        from_chips   = 0;
    int            to_item_id   = 0;  // 0 = no item
    int64_t        to_chips     = 0;
};
inline std::map<uint64_t, TradeOffer> trade_offers;
inline std::atomic<uint64_t>          trade_counter{1};
