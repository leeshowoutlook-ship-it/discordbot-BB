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
    std::set<dpp::snowflake> participants;
    bool           ended = false;
};

using SlotKey = std::tuple<std::string, std::string, std::string>; // boss,day,time

// ─── Global state ─────────────────────────────────────────────────────────────

inline Config cfg;
inline std::atomic<uint64_t> reg_counter{1};
inline std::atomic<uint64_t> team_counter{1};
inline std::atomic<uint64_t> giveaway_counter{1};

inline std::map<dpp::snowflake, RegState>       user_states;
inline std::map<dpp::snowflake, std::string>    view_filters;
inline std::map<dpp::snowflake, dpp::snowflake> msg_owner;       // msg_id -> owner_uid
inline std::map<dpp::snowflake, ActiveMsg>      user_active_msg; // uid -> active msg
inline std::map<uint64_t, ProposedTeam>         proposed_teams;
inline std::set<SlotKey>                        proposed_slots;
inline std::vector<Registration>                registrations;
inline std::map<uint64_t, Giveaway>             giveaways;
inline std::mutex                               data_mutex;
