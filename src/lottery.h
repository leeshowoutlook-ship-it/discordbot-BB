#pragma once
#include "chips.h"
#include "maple.h"
#include <random>

// ─── 摸彩（!摸彩／/摸彩／！摸彩）────────────────────────────────────────────────
// 消耗 1 張「彩券」(lottery_ticket) 抽一次獎。機率跟獎品內容不公開顯示在結果訊息裡
// （只顯示獎品名稱），獎品池日後可能調整，所以權重都寫在這裡集中管理。
//
// 「一組X」／「X張突襲」這幾個獎目前只顯示文字，不實際發放任何東西（不給瘋幣、不給突襲
// 次數、不給道具）——只有籌碼類獎品是真的會發放。日後要真的發放可以把 coins/raid_extra/
// item_key 填回去。
//
// stock/fallback_chips：部分「一組X」／「X張突襲」只有限量供應（全服共用一份計數，見
// lottery_stock），每被抽中一次就扣一組；扣到 0 之後，同一個機率格之後抽到都改發
// fallback_chips 籌碼（顯示名稱也會跟著變成「N萬籌碼」，見 lot_resolve_prize）。

struct LotteryPrize {
    std::string key;         // 空字串＝空包（無任何效果）
    std::string name;        // 中獎時顯示的文字（不會顯示機率或金額）
    std::string emoji;       // 轉動動畫用
    int         weight;      // 相對權重，全部加總為 1000（千分比）
    int64_t     chips   = 0; // 直接發放籌碼（0＝無）
    int64_t     coins   = 0; // 悄悄加碼瘋幣，不會顯示在結果訊息（0＝無）
    int         raid_extra = 0; // 本週突襲首領額外次數（0＝無）
    std::string item_key;    // 發放的背包道具 key（空字串＝無）
    int         stock = -1;         // -1＝不限量；>=0＝全服共用的初始庫存組數（之後實際剩餘量存在 lottery_stock）
    int64_t     fallback_chips = 0; // stock 用完之後，這個機率格改發的籌碼數
};

// 權重合計 1000：65% / 17% / 5% / 3% / 3% / 1% / 1% / 1% / 1% / 1% / 1% / 0.5% / 0.3% / 0.1% / 0.1%
static const std::vector<LotteryPrize> LOTTERY_PRIZES = {
    {"",              "空包",         "🍞", 650,        0, 0, 0, ""},
    {"chip_50k",      "5萬籌碼",      "💸", 170,    50000, 0, 0, ""},
    {"chip_250k",     "25萬籌碼",     "🤑",  50,   250000, 0, 0, ""},
    {"prize_satay",   "20萬籌碼",     "⚽",  30,   200000, 0, 0, ""},
    {"prize_cherry",  "一組櫻桃派",   "🏀",  30,        0, 0, 0, "", 3,  200000},
    {"prize_redbean", "一組紅豆刨冰", "🏈",  10,        0, 0, 0, "", 1,  200000},
    {"prize_cheese",  "20萬籌碼",     "🎾",  10,   200000, 0, 0, ""},
    {"prize_dew",     "一組清晨之露", "🏐",  10,        0, 0, 0, "", 1,  200000},
    {"prize_raid7",   "七張突襲",     "🥎",  10,        0, 0, 0, "", 2, 1000000},
    {"prize_raid4",   "四張突襲",     "🧊",  10,        0, 0, 0, "", 3,  800000},
    {"chip_2m",       "兩百萬籌碼",   "🐥",  10,  2000000, 0, 0, ""},
    {"chip_3m",       "三百萬籌碼",   "☃️",   5,  3000000, 0, 0, ""},
    {"chip_5m",       "五百萬籌碼",   "🌈",   3,  5000000, 0, 0, ""},
    {"prize_raid14",  "14張突襲",     "🗿",   1,        0, 0, 0, "", 3, 7770000},
    {"chip_10m",      "兩千萬籌碼",   "🚀",   1, 20000000, 0, 0, ""},
};

static std::mt19937& lot_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

// 依權重抽出一個獎品索引（0 = 空包）
static int lot_draw() {
    int total = 0;
    for (auto& p : LOTTERY_PRIZES) total += p.weight;
    int roll = std::uniform_int_distribution<int>(0, total - 1)(lot_rng());
    int acc = 0;
    for (int i = 0; i < (int)LOTTERY_PRIZES.size(); i++) {
        acc += LOTTERY_PRIZES[i].weight;
        if (roll < acc) return i;
    }
    return 0;
}

// 動畫用：隨機挑一個格子顯示（跟上一格不同，避免看起來卡住不動）
static int lot_random_display(int exclude_idx) {
    int n = (int)LOTTERY_PRIZES.size();
    if (n <= 1) return 0;
    int idx;
    do { idx = std::uniform_int_distribution<int>(0, n - 1)(lot_rng()); } while (idx == exclude_idx);
    return idx;
}

// ─── Persistence ──────────────────────────────────────────────────────────────
// 進行中的動畫狀態也要存檔：中獎結果（prize_idx）在扣彩券的當下就已經決定了，動畫只是
// 揭曉過程。如果動畫跑到一半 bot 重啟，timer 會直接消失、訊息永遠卡在「轉動中」——
// 所以開機時要把啟動前還沒結算完的摸彩，直接用已經決定好的 prize_idx 結算掉
// （見 lot_settle_orphaned_games_impl，在 on_ready_once 裡呼叫）。

static const std::string LOTTERY_STATS_FILE = "lotterystats.json";
static const std::string LOTTERY_GAMES_FILE = "lottery_games.json";

static void save_lottery_games() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, g] : lottery_games) {
            j[std::to_string((uint64_t)uid)] = {
                {"channel_id",   (uint64_t)g.channel_id},
                {"msg_id",       (uint64_t)g.msg_id},
                {"prize_idx",    g.prize_idx},
                {"avatar_url",   g.avatar_url},
                {"display_name", g.display_name},
            };
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(LOTTERY_GAMES_FILE, j.dump(2));
}

// 實際載入邏輯（static，每個 TU 各自一份沒關係）；對外可呼叫的 load_lotterystats()
// 包裝在 handlers_games.cpp，順便一起載入進行中的摸彩狀態。
static void lot_load_games_impl() {
    std::ifstream f(LOTTERY_GAMES_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            LotteryGame g;
            g.uid          = uid;
            g.channel_id   = dpp::snowflake(v.value("channel_id", (uint64_t)0));
            g.msg_id       = dpp::snowflake(v.value("msg_id",     (uint64_t)0));
            g.prize_idx    = v.value("prize_idx", 0);
            g.avatar_url   = v.value("avatar_url",   std::string{});
            g.display_name = v.value("display_name", std::string{});
            lottery_games[uid] = g;
        }
    } catch (...) {}
}

// 實際載入邏輯（static，每個 TU 各自一份沒關係）；對外可呼叫的 load_lotterystats()
// 包裝在 handlers_games.cpp（一般函式，main.cpp 啟動時透過 handler_decls.h 的宣告呼叫）。
static void lot_load_stats_impl() {
    std::ifstream f(LOTTERY_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = lottery_stats_data[uid];
            s.draws     = v.value("draws",     0);
            s.wins      = v.value("wins",      0);
            s.chips_won = v.value("chips_won", (int64_t)0);
        }
    } catch (...) {}
}

static void save_lotterystats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : lottery_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"draws",     s.draws},
                {"wins",      s.wins},
                {"chips_won", s.chips_won}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(LOTTERY_STATS_FILE, j.dump(2));
}

// ─── 摸彩單次紀錄（!摸彩紀錄／/摸彩紀錄）────────────────────────────────────────
// 每人最多留最近 LOTTERY_HISTORY_MAX 筆，避免存檔無限長大。

static const std::string LOTTERY_HISTORY_FILE = "lottery_history.json";
static const size_t LOTTERY_HISTORY_MAX = 50;

static void save_lottery_history() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, hist] : lottery_history) {
            if (hist.empty()) continue;
            nlohmann::json arr = nlohmann::json::array();
            for (auto& r : hist)
                arr.push_back({{"ts", (int64_t)r.ts}, {"name", r.name}, {"emoji", r.emoji}});
            j[std::to_string((uint64_t)uid)] = arr;
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(LOTTERY_HISTORY_FILE, j.dump(2));
}

static void lot_load_history_impl() {
    std::ifstream f(LOTTERY_HISTORY_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& hist = lottery_history[uid];
            for (auto& rv : v) {
                LotteryDrawRecord r;
                r.ts    = (time_t)rv.value("ts", (int64_t)0);
                r.name  = rv.value("name",  std::string{});
                r.emoji = rv.value("emoji", std::string{});
                hist.push_back(r);
            }
        }
    } catch (...) {}
}

// ─── 限量獎品庫存（全服共用）───────────────────────────────────────────────────

static const std::string LOTTERY_STOCK_FILE = "lottery_stock.json";

static void save_lottery_stock() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : lottery_stock) j[k] = v;
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(LOTTERY_STOCK_FILE, j.dump(2));
}

// 先讀存檔（如果有的話），再用 LOTTERY_PRIZES 的初始庫存補上「存檔裡還沒出現過」的獎項
// （第一次上線這個機制、或日後新增限量獎都會吃到這段，不會動到已經在追蹤中的剩餘量）。
static void lot_load_stock_impl() {
    {
        std::ifstream f(LOTTERY_STOCK_FILE);
        if (f.is_open()) {
            try {
                nlohmann::json j; f >> j;
                std::lock_guard<std::mutex> lk(data_mutex);
                for (auto& [k, v] : j.items()) lottery_stock[k] = v.get<int>();
            } catch (...) {}
        }
    }
    bool need_save = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& p : LOTTERY_PRIZES) {
            if (p.stock < 0) continue;
            if (!lottery_stock.count(p.key)) { lottery_stock[p.key] = p.stock; need_save = true; }
        }
    }
    if (need_save) save_lottery_stock();
}

// 把中獎格換成「實際要發放的獎品」：不限量就直接回傳原樣；限量的話扣一組庫存，
// 扣到 0 之後，之後都回傳一個「改發籌碼」的虛擬獎品（名稱直接顯示金額，比照其他籌碼類獎品）。
// 呼叫前不可持有 data_mutex（自己上鎖＋存檔）。
static LotteryPrize lot_resolve_prize(const LotteryPrize& p) {
    if (p.stock < 0) return p;
    bool depleted;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        int& remain = lottery_stock[p.key];
        if (remain > 0) { remain--; depleted = false; }
        else depleted = true;
    }
    save_lottery_stock();
    if (!depleted) return p;
    LotteryPrize fallback = p;
    fallback.name = std::to_string(p.fallback_chips / 10000) + "萬籌碼";
    fallback.chips = p.fallback_chips;
    fallback.item_key.clear();
    return fallback;
}

// 記一筆摸彩紀錄；呼叫前不可持有 data_mutex（自己上鎖）。
static void lot_record_history(dpp::snowflake uid, const LotteryPrize& p) {
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& hist = lottery_history[uid];
        hist.push_back({time(nullptr), p.name, p.emoji});
        if (hist.size() > LOTTERY_HISTORY_MAX)
            hist.erase(hist.begin(), hist.begin() + (hist.size() - LOTTERY_HISTORY_MAX));
    }
    save_lottery_history();
}

// ─── Stats summary line ───────────────────────────────────────────────────────

static std::string lot_stats_line(dpp::snowflake uid) {
    LotteryStats st;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = lottery_stats_data.find(uid);
        if (it != lottery_stats_data.end()) st = it->second;
    }
    std::string wr = (st.draws > 0) ? std::to_string(st.wins * 100 / st.draws) + "%" : "—";
    return "摸彩 **" + std::to_string(st.draws) + "** 次　中獎 **" + std::to_string(st.wins)
         + "** 次（" + wr + "）　累積獲得 **" + std::to_string(st.chips_won) + "** 碼";
}

static void lot_set_user(dpp::embed& e, const LotteryGame& g) {
    if (!g.avatar_url.empty()) e.set_thumbnail(g.avatar_url);
    e.set_footer(dpp::embed_footer().set_text("👤 " + (g.display_name.empty() ? std::to_string((uint64_t)g.uid) : g.display_name)));
}

static dpp::message make_lottery_history_msg(dpp::snowflake uid, const std::string& display_name) {
    std::vector<LotteryDrawRecord> hist;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = lottery_history.find(uid);
        if (it != lottery_history.end()) hist = it->second;
    }
    dpp::embed e;
    e.set_title("📜  摸彩紀錄").set_color(0xF1C40F);
    if (hist.empty()) {
        e.set_description("還沒有摸彩紀錄。");
    } else {
        std::ostringstream oss;
        int shown = 0;
        const int SHOW_MAX = 15;
        for (auto it = hist.rbegin(); it != hist.rend() && shown < SHOW_MAX; ++it, ++shown)
            oss << it->emoji << " **" << it->name << "**　<t:" << (int64_t)it->ts << ":R>\n";
        e.set_description(oss.str());
        if ((int)hist.size() > SHOW_MAX)
            e.set_footer(dpp::embed_footer().set_text("僅顯示最近 " + std::to_string(SHOW_MAX) + " 筆，共 " + std::to_string(hist.size()) + " 筆"));
    }
    e.add_field("📊  累計", lot_stats_line(uid), false);
    dpp::message msg; msg.add_embed(e); return msg;
}

// ─── Spin animation frame ──────────────────────────────────────────────────────

static dpp::message make_lottery_spin_frame_msg(const LotteryGame& g, int display_idx) {
    dpp::embed e;
    e.set_title("🎰  摸彩抽獎中...").set_color(0xF1C40F);
    e.set_description("目前轉到：" + LOTTERY_PRIZES[display_idx].emoji + "\n\n-# 已消耗彩券 ×1");
    lot_set_user(e, g);
    dpp::message msg; msg.add_embed(e);
    return msg;
}

// ─── Final result ──────────────────────────────────────────────────────────────
// Caller must NOT hold data_mutex（add_chips／save_* 內部會自己上鎖）

static dpp::message make_lottery_result_msg(const LotteryGame& g) {
    LotteryPrize p = lot_resolve_prize(LOTTERY_PRIZES[g.prize_idx]); // 限量獎可能在這裡被換成「改發籌碼」
    bool win = (g.prize_idx != 0); // index 0 固定是空包

    if (p.chips > 0) add_chips(g.uid, p.chips);
    if (p.coins > 0 || p.raid_extra > 0) {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& c = maple_data[g.uid];
        maple_raid_week_reset_if_needed(c);
        c.coins += p.coins;
        c.raid_week_extra += p.raid_extra;
    }
    if (!p.item_key.empty()) {
        std::lock_guard<std::mutex> lk(data_mutex);
        inventory_data[g.uid][p.item_key]++;
    }
    if (p.coins > 0 || p.raid_extra > 0) save_maple_data();
    if (!p.item_key.empty()) save_inventory();

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = lottery_stats_data[g.uid];
        st.draws++;
        if (win) st.wins++;
        st.chips_won += p.chips;
    }
    save_lotterystats();
    lot_record_history(g.uid, p);

    int tickets_left = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(g.uid);
        if (it != inventory_data.end()) {
            auto t2 = it->second.find("lottery_ticket");
            if (t2 != it->second.end()) tickets_left = t2->second;
        }
    }

    dpp::embed e;
    e.set_title(win ? "🎉  中獎了！" : "💀  銘謝惠顧").set_color(win ? 0x2ECC71 : 0x95A5A6);
    e.set_description(p.emoji + "　**" + p.name + "**");
    e.add_field("💼  持有籌碼", std::to_string(get_chips(g.uid)) + " 碼", false);
    e.add_field("📊  摸彩紀錄", lot_stats_line(g.uid), false);
    lot_set_user(e, g);

    dpp::message msg; msg.add_embed(e);
    std::string uid_s = std::to_string((uint64_t)g.uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎰 再抽一次（剩 " + std::to_string(tickets_left) + " 張彩券）")
        .set_id("lot_again_" + uid_s).set_style(dpp::cos_primary)
        .set_disabled(tickets_left <= 0));
    msg.add_component(row);
    return msg;
}

// ─── Spin loop（跟 /轉 的單人動畫走同一套：每隔幾秒編輯一次訊息，最後一格揭曉真正結果）──

static const int LOTTERY_SPIN_FRAMES = 6;

static void lot_start_spin(dpp::snowflake uid) {
    dpp::timer tid = g_bot->start_timer([uid](dpp::timer t) {
        LotteryGame g; bool found = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = lottery_games.find(uid);
            if (it == lottery_games.end()) { g_bot->stop_timer(t); return; }
            g = it->second; found = true;
        }
        if (!found) { g_bot->stop_timer(t); return; }

        g.spin_frame++;
        if (g.spin_frame < LOTTERY_SPIN_FRAMES) {
            int display_idx = (g.spin_frame == LOTTERY_SPIN_FRAMES - 1)
                ? g.prize_idx
                : lot_random_display(g.last_display);
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = lottery_games.find(uid);
                if (it != lottery_games.end()) {
                    it->second.spin_frame   = g.spin_frame;
                    it->second.last_display = display_idx;
                }
            }
            auto fmsg = make_lottery_spin_frame_msg(g, display_idx);
            fmsg.id = g.msg_id; fmsg.channel_id = g.channel_id;
            g_bot->message_edit(fmsg, [uid](const dpp::confirmation_callback_t& cb) {
                if (cb.is_error()) {
                    auto err = cb.get_error();
                    fprintf(stderr, "[摸彩] 動畫中途 message_edit 失敗 uid=%llu http=%d code=%d msg=%s\n",
                        (unsigned long long)(uint64_t)uid, cb.http_info.status, err.code, err.message.c_str());
                    fflush(stderr);
                }
            });
            return;
        }

        // 最後一拍：結算並揭曉
        g_bot->stop_timer(t);
        auto fmsg = make_lottery_result_msg(g);
        fmsg.id = g.msg_id; fmsg.channel_id = g.channel_id;
        { std::lock_guard<std::mutex> lk(data_mutex); lottery_games.erase(uid); }
        save_lottery_games();
        dpp::snowflake fallback_ch = g.channel_id;
        g_bot->message_edit(fmsg, [uid, fallback_ch, fmsg](const dpp::confirmation_callback_t& cb) {
            if (cb.is_error()) {
                auto err = cb.get_error();
                fprintf(stderr, "[摸彩] 結算 message_edit 失敗，改發新訊息 uid=%llu http=%d code=%d msg=%s\n",
                    (unsigned long long)(uint64_t)uid, cb.http_info.status, err.code, err.message.c_str());
                fflush(stderr);
                // 編輯失敗（常見原因是連續編輯太快被 Discord 限流429），改發一則新訊息，確保玩家一定看得到結果
                dpp::message newmsg = fmsg;
                newmsg.id = 0; newmsg.channel_id = fallback_ch;
                g_bot->message_create(newmsg, [uid](const dpp::confirmation_callback_t& cb2) {
                    if (cb2.is_error()) {
                        auto err2 = cb2.get_error();
                        fprintf(stderr, "[摸彩] 補發新訊息也失敗 uid=%llu http=%d code=%d msg=%s\n",
                            (unsigned long long)(uint64_t)uid, cb2.http_info.status, err2.code, err2.message.c_str());
                        fflush(stderr);
                    }
                });
            }
        });
    }, 2); // 原本是每秒編輯一次，連續6-7次很容易被 Discord 限流(429)；拉長到每2秒一次降低頻率
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = lottery_games.find(uid);
    if (it != lottery_games.end()) it->second.timer_id = tid;
}

// ─── 開機補結算（實作在這裡，對外包裝在 handlers_games.cpp，main.cpp 在 on_ready_once
//     裡呼叫，需要 g_bot 已經連線）──────────────────────────────────────────────
// 把啟動前還卡在「轉動中」畫面、animation timer 已經隨著上次程序結束而消失的摸彩，
// 直接用當初扣彩券時就決定好的 prize_idx 結算掉，編輯那則卡住的訊息揭曉結果。
static void lot_settle_orphaned_games_impl() {
    std::vector<LotteryGame> orphans;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, g] : lottery_games) orphans.push_back(g);
        lottery_games.clear();
    }
    if (orphans.empty()) return;
    save_lottery_games(); // 先清空存檔，避免下次啟動又重複結算一次
    for (auto& g : orphans) {
        if (!g.msg_id || !g.channel_id) continue; // 訊息都還沒送出成功就重啟了，沒東西可補
        auto fmsg = make_lottery_result_msg(g);
        fmsg.id = g.msg_id; fmsg.channel_id = g.channel_id;
        dpp::snowflake uid = g.uid, fallback_ch = g.channel_id;
        g_bot->message_edit(fmsg, [uid, fallback_ch, fmsg](const dpp::confirmation_callback_t& cb) {
            if (cb.is_error()) {
                auto err = cb.get_error();
                fprintf(stderr, "[摸彩] 補結算 message_edit 失敗，改發新訊息 uid=%llu http=%d code=%d msg=%s\n",
                    (unsigned long long)(uint64_t)uid, cb.http_info.status, err.code, err.message.c_str());
                fflush(stderr);
                dpp::message newmsg = fmsg;
                newmsg.id = 0; newmsg.channel_id = fallback_ch;
                g_bot->message_create(newmsg);
            }
        });
    }
}

// ─── Start helper（由 !摸彩 / /摸彩 / lot_again_ 按鈕呼叫）───────────────────────
// ok_out：是否成功開始（成功才需要呼叫端在送出訊息後填 msg_id 並呼叫 lot_start_spin）
static dpp::message handle_lottery_start(dpp::snowflake uid, dpp::snowflake ch,
                                          const std::string& avatar_url, const std::string& display_name,
                                          bool* ok_out = nullptr) {
    if (ok_out) *ok_out = false;
    std::string ch_s = std::to_string((uint64_t)ch);
    bool ch_allowed = (!cfg.allin_500k_thread_id.empty() && ch_s == cfg.allin_500k_thread_id)
                    || (!cfg.lottery_thread2_id.empty()  && ch_s == cfg.lottery_thread2_id);
    if (!ch_allowed) {
        dpp::embed e; e.set_title("❌  這裡不能摸彩").set_color(0xE74C3C);
        e.set_description("摸彩只能在發放彩券的那個討論串使用！");
        return dpp::message().add_embed(e);
    }
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (lottery_games.count(uid)) {
            dpp::embed e; e.set_title("⏳  摸彩進行中").set_color(0x808080);
            e.set_description("上一次摸彩動畫還在跑，請等幾秒再試一次！");
            return dpp::message().add_embed(e);
        }
    }
    int tickets = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid);
        if (it != inventory_data.end()) {
            auto t2 = it->second.find("lottery_ticket");
            if (t2 != it->second.end()) tickets = t2->second;
        }
    }
    if (tickets <= 0) {
        dpp::embed e; e.set_title("❌  沒有彩券").set_color(0xE74C3C);
        e.set_description("摸彩需要消耗 **1 張彩券**，你目前沒有彩券。\n高額 ALLIN 房間下注可以獲得彩券。");
        return dpp::message().add_embed(e);
    }
    { std::lock_guard<std::mutex> lk(data_mutex); inventory_data[uid]["lottery_ticket"]--; }
    save_inventory();

    LotteryGame g;
    g.uid = uid; g.channel_id = ch;
    g.prize_idx = lot_draw();
    g.avatar_url = avatar_url; g.display_name = display_name;
    { std::lock_guard<std::mutex> lk(data_mutex); lottery_games[uid] = g; }
    if (ok_out) *ok_out = true;
    return make_lottery_spin_frame_msg(g, lot_random_display(-1));
}
