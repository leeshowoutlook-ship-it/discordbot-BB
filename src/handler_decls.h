#pragma once
#include <dpp/dpp.h>

// ─── Blackjack (21) ──────────────────────────────────────────────────────────
void handle_bj_message(const dpp::message_create_t& ev, const std::string& content, dpp::snowflake uid, dpp::snowflake ch);
void handle_bj_button (const dpp::button_click_t& ev);
void handle_bj_slash  (const dpp::slashcommand_t& ev, const std::string& cmd_name, dpp::snowflake uid, dpp::snowflake ch);

// ─── Roulette ─────────────────────────────────────────────────────────────────
void handle_roulette_message(const dpp::message_create_t& ev, const std::string& content, dpp::snowflake uid, dpp::snowflake ch);
void handle_roulette_slash  (const dpp::slashcommand_t& ev, dpp::snowflake uid, dpp::snowflake ch);
void handle_roulette_button (const dpp::button_click_t& ev);
void handle_roulette_modal  (const dpp::form_submit_t& ev);
void handle_roulette_select (const dpp::select_click_t& ev);

// ─── Wolf + Onenight ──────────────────────────────────────────────────────────
void handle_wolf_message(const dpp::message_create_t& ev, const std::string& content, dpp::snowflake uid, dpp::snowflake ch);
void handle_wolf_button (const dpp::button_click_t& ev);
void handle_wolf_slash  (const dpp::slashcommand_t& ev, const std::string& cmd_name, dpp::snowflake uid, dpp::snowflake ch);

// ─── Undercover ───────────────────────────────────────────────────────────────
void handle_uc_message(const dpp::message_create_t& ev, const std::string& content, dpp::snowflake uid, dpp::snowflake ch);
void handle_uc_button (const dpp::button_click_t& ev);
void handle_uc_modal  (const dpp::form_submit_t& ev);
void handle_uc_select (const dpp::select_click_t& ev, dpp::snowflake uid);
void handle_uc_slash  (const dpp::slashcommand_t& ev, const std::string& cmd_name, dpp::snowflake uid, dpp::snowflake ch);

// ─── Hunt (solo monster) ──────────────────────────────────────────────────────
void handle_hunt_message(const dpp::message_create_t& ev, const std::string& content, dpp::snowflake uid, dpp::snowflake ch);
void handle_hunt_button (const dpp::button_click_t& ev);
void handle_hunt_slash  (const dpp::slashcommand_t& ev, const std::string& cmd_name, dpp::snowflake uid, dpp::snowflake ch);

// ─── Raid (multiplayer Rathalos) ─────────────────────────────────────────────
void handle_raid_button (const dpp::button_click_t& ev);

// ─── DarkDragon (multiplayer) ─────────────────────────────────────────────────
void handle_dd_button   (const dpp::button_click_t& ev);

// ─── Shop / Gacha / Equip / Bag ──────────────────────────────────────────────
void handle_shop_button(const dpp::button_click_t& ev);

// ─── Pet / Lobby / Talent ─────────────────────────────────────────────────────
void handle_pet_button(const dpp::button_click_t& ev);
void handle_pet_modal (const dpp::form_submit_t&  ev);
void handle_pet_slash (const dpp::slashcommand_t& ev, const std::string& cmd_name);

// ─── Games (骰子 / 射 / 火箭 / 刮刮樂 / 猜數字) ────────────────────────────
void handle_games_message(const dpp::message_create_t& ev, const std::string& content, dpp::snowflake uid, dpp::snowflake ch);
void handle_games_button (const dpp::button_click_t& ev);
void handle_games_modal  (const dpp::form_submit_t&  ev);
void handle_games_slash  (const dpp::slashcommand_t& ev, const std::string& cmd_name, dpp::snowflake uid, dpp::snowflake ch);

// ─── Adventure / Collection / Enhance ────────────────────────────────────────
void handle_adv_slash(const dpp::slashcommand_t& ev, const std::string& cmd_name, dpp::snowflake uid, dpp::snowflake ch);
void handle_adv_modal(const dpp::form_submit_t&  ev);

// ─── Stock market ─────────────────────────────────────────────────────────────
void handle_stock_button(const dpp::button_click_t& ev);
void handle_stock_modal (const dpp::form_submit_t&  ev);

// ─── Chest reward helpers（實作在各自的 cpp）──────────────────────────────────
// 呼叫前不可持有 data_mutex（內部自行加鎖）
std::string give_latus_chest_reward(dpp::snowflake uid);
// 呼叫前必須已持有 data_mutex
std::string give_darkdragon_chest_reward(dpp::snowflake uid);
