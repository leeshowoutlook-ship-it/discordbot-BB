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

// ─── Maple Valley（楓之谷世界養成系統）────────────────────────────────────────
void handle_maple_message(const dpp::message_create_t& ev, const std::string& content, dpp::snowflake uid, dpp::snowflake ch);
void handle_maple_button (const dpp::button_click_t& ev);
void handle_maple_select (const dpp::select_click_t& ev, dpp::snowflake uid);
void handle_maple_slash  (const dpp::slashcommand_t& ev, const std::string& cmd_name, dpp::snowflake uid, dpp::snowflake ch);
void load_maple_all_data(); // 啟動時呼叫，載入角色資料與進行中的戰鬥
void load_lotterystats();   // 啟動時呼叫，載入摸彩累積統計＋進行中的摸彩狀態（定義在 handlers_games.cpp；main.cpp 不直接 include lottery.h，避免跟 maple.h 在同一個 TU 編譯出錯）
void lot_settle_orphaned_games(); // bot 連線後（on_ready_once）呼叫：把啟動前卡在動畫中的摸彩直接結算掉
void maple_save_exp_event(); // !經驗活動 設定/結束後呼叫，存檔（maple_exp_event 這個全域struct在types.h，main.cpp可以直接讀寫欄位）
void maple_exp_event_archive_current(); // !經驗活動 換下一場／手動結束前呼叫，把目前這場存進歷史（呼叫前需持有 data_mutex）
// 管理員面板「給養成經驗」用：補發楓之谷世界養成經驗值（amount 必須 > 0，沒有扣除/降級功能）。
// 回傳結果描述文字（目前等級／剩餘經驗，升級的話會註明），已經處理好存檔。
std::string maple_admin_give_exp(dpp::snowflake uid, int64_t amount);

// ─── 楓之谷世界養成系統：交易輔助（實作在 handlers_maple.cpp）────────────────
// mv_ = Maple Valley。用於讓 !交易／/交易 支援楓之谷卷軸、裝備與瘋幣。
bool    mv_item_info(int id, std::string& key_out, std::string& name_out); // true=此ID是楓之谷卷軸或裝備
bool    mv_is_item_key(const std::string& key);
bool    mv_has_item(dpp::snowflake uid, const std::string& key, int64_t qty); // 內部自行加鎖
int64_t mv_get_coins(dpp::snowflake uid);                                     // 內部自行加鎖
// 以下需在呼叫前持有 data_mutex：
bool    mv_locked_has_item(dpp::snowflake uid, const std::string& key, int64_t qty);
void    mv_locked_transfer_item(dpp::snowflake from, dpp::snowflake to, const std::string& key, int64_t qty);
int64_t mv_locked_get_coins(dpp::snowflake uid);
void    mv_locked_add_coins(dpp::snowflake uid, int64_t delta);
void    mv_save_data();
// 管理員給道具／道具ID：接受道具ID或key字串找楓之谷卷軸／裝備；give 回傳實際變動量（沒收會夾在0，不扣成負的）
bool    mv_resolve_item(const std::string& raw, std::string& key_out, std::string& name_out);
int64_t mv_locked_give_item(dpp::snowflake uid, const std::string& key, int64_t qty); // 呼叫前需持有 data_mutex
struct MvOwnedItem { std::string key, name; int item_id; int64_t qty; };
std::vector<MvOwnedItem> mv_list_owned_items(dpp::snowflake uid); // 供 /交易 autocomplete 列出玩家持有的楓之谷卷軸／裝備（未強化才會出現在背包計數裡）

// ─── 強化裝備交易（實作在 handlers_maple.cpp）──────────────────────────────────
// MV_ENH_TRADE_ID_BASE／mv_is_enh_trade_id／mv_enh_id_from_trade_id／mv_enh_trade_id
// 定義在 types.h（不是這裡）——因為 maple.h 裡也會用到這幾個，而 maple.h 在大部分 .cpp 的
// include 順序裡排在 handler_decls.h 前面，放這裡會在那些檔案裡變成「用到還沒宣告的東西」。
// 查詢某個強化實例是否存在、可交易（未裝備中），並回傳顯示用的 key/name（內部自行加鎖）
bool    mv_resolve_enh_item(dpp::snowflake owner_uid, int enh_id, std::string& key_out, std::string& name_out);
// 以下需在呼叫前持有 data_mutex：
bool    mv_locked_has_enh_item(dpp::snowflake uid, int enh_id);
bool    mv_locked_resolve_enh_item(dpp::snowflake owner_uid, int enh_id, std::string& key_out, std::string& name_out);
void    mv_locked_transfer_enh_item(dpp::snowflake from, dpp::snowflake to, int enh_id); // 轉移後會替對方重新編號

// ─── Chest reward helpers（實作在各自的 cpp）──────────────────────────────────
// 呼叫前不可持有 data_mutex（內部自行加鎖）
std::string give_latus_chest_reward(dpp::snowflake uid);
// 呼叫前必須已持有 data_mutex
std::string give_darkdragon_chest_reward(dpp::snowflake uid);
