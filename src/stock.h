#pragma once
#include "types.h"
#include "chips.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

// ─── 股票系統：0050／台積電／國巨／比特幣走 Yahoo Finance，LeeShoW的心情純手動 ──────

static const std::string STOCK_HOLDINGS_FILE = "stock_holdings.json";
static const std::string STOCK_MARKET_FILE   = "stock_market.json";
static const double      STOCK_FEE_RATE      = 0.02; // 買賣手續費 2%

// UTC+8 day number，用來判斷手動股票是否跨天（跨天要重置每日漲跌幅限制的開盤價基準）
static int64_t stock_utc8_day_number() {
    return ((int64_t)time(nullptr) + 8 * 3600) / 86400;
}

// ─── Persistence ──────────────────────────────────────────────────────────────

static void save_stock_holdings() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, holdings] : player_stocks) {
            nlohmann::json hj;
            for (auto& [key, h] : holdings) {
                if (h.shares <= 0) continue;
                hj[key] = {{"shares", h.shares}, {"avg_cost", h.avg_cost}};
            }
            if (!hj.empty()) j[std::to_string((uint64_t)uid)] = hj;
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(STOCK_HOLDINGS_FILE, j.dump(2));
}

static void load_stock_holdings() {
    std::ifstream f(STOCK_HOLDINGS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            for (auto& [key, hv] : v.items()) {
                StockHolding h;
                h.shares   = hv.value("shares",   (int64_t)0);
                h.avg_cost = hv.value("avg_cost", (int64_t)0);
                if (h.shares > 0) player_stocks[uid][key] = h;
            }
        }
    } catch (...) {}
}

static void save_stock_market() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(stock_mutex);
        for (auto& [key, s] : stock_market)
            j[key] = {
                {"price",          s.price},
                {"prev_close",     s.prev_close},
                {"last_update",    (int64_t)s.last_update},
                {"day_open_price", s.day_open_price},
                {"day_number",     s.day_number},
            };
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(STOCK_MARKET_FILE, j.dump(2));
}

static void load_stock_market() {
    // 先用預設值把 5 支股票初始化好（心情股從 1000 碼開始，其他等第一次抓價）
    {
        std::lock_guard<std::mutex> lk(stock_mutex);
        for (auto& d : STOCK_DEFS) {
            StockInfo s;
            s.key = d.key; s.name = d.name; s.ticker = d.ticker;
            if (d.key == "stock_mood" || d.key == "stock_catbro" || d.key == "stock_purse") s.price = 1000;
            stock_market[d.key] = s;
        }
    }
    std::ifstream f(STOCK_MARKET_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(stock_mutex);
        for (auto& [key, v] : j.items()) {
            auto it = stock_market.find(key);
            if (it == stock_market.end()) continue;
            it->second.price          = v.value("price",       it->second.price);
            it->second.prev_close     = v.value("prev_close",  (int64_t)0);
            it->second.last_update    = (time_t)v.value("last_update", (int64_t)0);
            it->second.day_open_price = v.value("day_open_price", (int64_t)0);
            it->second.day_number     = v.value("day_number",     (int64_t)0);
        }
    } catch (...) {}
}

// ─── 抓即時股價（Yahoo Finance，非官方端點，失敗時沿用上次已知價格）───────────────

static void fetch_stock_price(const std::string& key, const std::string& ticker) {
    std::string url = "https://query1.finance.yahoo.com/v8/finance/chart/" + ticker + "?interval=1d&range=1d";
    g_bot->request(url, dpp::m_get,
        [key](const dpp::http_request_completion_t& cc) {
            bool ok = false;
            int64_t price = 0;
            if (cc.status == 200) {
                try {
                    auto j = nlohmann::json::parse(cc.body);
                    double p = j.at("chart").at("result").at(0).at("meta").at("regularMarketPrice").get<double>();
                    if (p > 0) { price = (int64_t)(p + 0.5); ok = true; }
                } catch (...) {}
            }
            bool changed = false;
            {
                std::lock_guard<std::mutex> lk(stock_mutex);
                auto it = stock_market.find(key);
                if (it != stock_market.end()) {
                    it->second.fetch_ok = ok;
                    if (ok) {
                        it->second.prev_close = (it->second.price > 0) ? it->second.price : price;
                        it->second.price      = price;
                        it->second.last_update = time(nullptr);
                        changed = true;
                    }
                }
            }
            if (changed) save_stock_market();
        },
        "", "application/json", {{"User-Agent", "Mozilla/5.0"}}
    );
}

// 走勢圖資料：另外抓近一個月的日收盤價（跟目前價格/漲跌%用的 1d 端點分開，
// 因為 1mo 範圍的 meta.previousClose 是「一個月前」而不是「昨天」，不能拿來算漲跌%）
static void fetch_stock_history(const std::string& key, const std::string& ticker) {
    std::string url = "https://query1.finance.yahoo.com/v8/finance/chart/" + ticker + "?interval=1d&range=1mo";
    g_bot->request(url, dpp::m_get,
        [key](const dpp::http_request_completion_t& cc) {
            if (cc.status != 200) return;
            std::vector<int64_t> hist;
            try {
                auto j = nlohmann::json::parse(cc.body);
                auto& closes = j.at("chart").at("result").at(0).at("indicators").at("quote").at(0).at("close");
                for (auto& v : closes) {
                    if (v.is_null()) continue;
                    hist.push_back((int64_t)(v.get<double>() + 0.5));
                }
            } catch (...) { return; }
            if (hist.size() > 30) hist.erase(hist.begin(), hist.end() - 30);
            if (hist.empty()) return;
            std::lock_guard<std::mutex> lk(stock_mutex);
            auto it = stock_market.find(key);
            if (it != stock_market.end()) it->second.history = hist;
        },
        "", "application/json", {{"User-Agent", "Mozilla/5.0"}}
    );
}

static void start_stock_price_timer() {
    auto fetch_all = []() {
        for (auto& d : STOCK_DEFS) {
            if (d.ticker.empty()) continue; // 心情股不接 API
            fetch_stock_price(d.key, d.ticker);
            fetch_stock_history(d.key, d.ticker);
        }
    };
    fetch_all(); // 開機立即抓一次，不用等第一次計時器觸發
    g_bot->start_timer([fetch_all](dpp::timer) { fetch_all(); }, 300); // 每 5 分鐘更新一次
}

// ─── 走勢圖：組 QuickChart.io 網址，不用本地畫圖函式庫 ─────────────────────────

static std::string url_encode(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else { char buf[4]; snprintf(buf, sizeof(buf), "%%%02X", c); out += buf; }
    }
    return out;
}

static std::string build_stock_chart_url(const std::vector<int64_t>& history) {
    if (history.size() < 2) return "";
    bool up = history.back() >= history.front();
    std::string color = up ? "#2ECC71" : "#E74C3C";
    nlohmann::json data = nlohmann::json::array();
    for (auto v : history) data.push_back(v);
    nlohmann::json labels = nlohmann::json::array();
    for (size_t i = 0; i < history.size(); i++) labels.push_back("");
    nlohmann::json cfg = {
        {"type", "line"},
        {"data", {
            {"labels", labels},
            {"datasets", {{
                {"data", data},
                {"borderColor", color},
                {"backgroundColor", color},
                {"fill", false},
                {"pointRadius", 0},
                {"borderWidth", 2},
                {"tension", 0.15},
            }}}
        }},
        {"options", {
            {"legend", {{"display", false}}},
            {"scales", {
                {"xAxes", {{{"display", false}}}},
                {"yAxes", {{{"display", false}}}},
            }},
        }},
    };
    return "https://quickchart.io/chart?width=520&height=200&backgroundColor=white&c=" + url_encode(cfg.dump());
}

// ─── UI：股票市場首頁（Components V2 清單）────────────────────────────────────

static std::string stock_change_line(const StockInfo& s) {
    if (s.price <= 0) return "價格讀取中...";
    std::string line = "現價 **" + std::to_string(s.price) + "** 碼";
    if (s.prev_close > 0) {
        int64_t diff = s.price - s.prev_close;
        double pct = (double)diff / s.prev_close * 100.0;
        char buf[32]; snprintf(buf, sizeof(buf), "%+.2f%%", pct);
        line += "　" + std::string(diff >= 0 ? "📈 +" : "📉 ") + std::to_string(diff) + " (" + buf + ")";
    }
    return line;
}

static dpp::message make_stock_home_msg(dpp::snowflake uid,
                                         const std::string& dn = "", const std::string& av = "",
                                         int page = 0) {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::map<std::string, StockInfo> snap;
    { std::lock_guard<std::mutex> lk(stock_mutex); snap = stock_market; }
    std::map<std::string, StockHolding> holdings;
    { std::lock_guard<std::mutex> lk(data_mutex);
      auto hit = player_stocks.find(uid);
      if (hit != player_stocks.end()) holdings = hit->second;
    }

    const int PAGE_SIZE = 5;
    int total       = (int)STOCK_DEFS.size();
    int total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    page = std::max(0, std::min(page, total_pages - 1));
    int start = page * PAGE_SIZE;
    int end   = std::min(start + PAGE_SIZE, total);

    auto mk_section = [&](const StockDef& d) {
        auto it = snap.find(d.key);
        std::string square = "⬜";
        if (it != snap.end() && it->second.price > 0 && it->second.prev_close > 0)
            square = (it->second.price >= it->second.prev_close) ? "🟩" : "🟥";
        std::string text = square + " **" + d.emoji + " " + d.name + "**\n" + d.desc + "\n";
        if (it == snap.end()) text += "價格讀取中...";
        else {
            text += stock_change_line(it->second);
            if (!it->second.fetch_ok && !d.ticker.empty()) text += "\n⚠️ 上次更新失敗，顯示最後已知價格";
        }
        auto hit = holdings.find(d.key);
        if (hit != holdings.end() && hit->second.shares > 0)
            text += "\n持有：**" + std::to_string(hit->second.shares) + "** 股";
        return dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label("查看").set_id("stock_view_" + uid_s + "_" + d.key).set_style(dpp::cos_secondary));
    };

    std::string header = "## 📊 股票市場（第 " + std::to_string(page + 1)
                       + " / " + std::to_string(total_pages) + " 頁）\n"
                       + (dn.empty() ? uid_s : dn) + "，選擇要查看的股票：";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(header));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));
    for (int i = start; i < end; i++) container.add_component_v2(mk_section(STOCK_DEFS[i]));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("◀ 上一頁").set_id("stock_page_" + uid_s + "_" + std::to_string(page - 1))
        .set_style(dpp::cos_secondary).set_disabled(page == 0));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶ 下一頁").set_id("stock_page_" + uid_s + "_" + std::to_string(page + 1))
        .set_style(dpp::cos_secondary).set_disabled(page >= total_pages - 1));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

// ─── UI：個股詳細頁（傳統 embed，買賣按鈕）─────────────────────────────────────

static dpp::message make_stock_detail_msg(dpp::snowflake uid, const std::string& key,
                                           const std::string& dn = "", const std::string& av = "",
                                           const std::string& notice = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    const StockDef* def = find_stock_def(key);

    StockInfo info;
    { std::lock_guard<std::mutex> lk(stock_mutex); auto it = stock_market.find(key); if (it != stock_market.end()) info = it->second; }

    int64_t shares = 0, avg_cost = 0;
    int64_t chips = get_chips(uid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto pit = player_stocks.find(uid);
        if (pit != player_stocks.end()) {
            auto hit = pit->second.find(key);
            if (hit != pit->second.end()) { shares = hit->second.shares; avg_cost = hit->second.avg_cost; }
        }
    }

    std::string stock_title = def ? (def->emoji + " " + def->name) : key;
    std::string content = "## " + stock_title + "\n";
    if (!notice.empty()) content += notice + "\n\n";
    content += stock_change_line(info) + "\n\n";
    content += "持有：**" + std::to_string(shares) + "** 股";
    if (shares > 0) {
        int64_t pnl = (info.price - avg_cost) * shares;
        content += "　均價 " + std::to_string(avg_cost) + " 碼　損益 " + (pnl >= 0 ? "+" : "") + std::to_string(pnl) + " 碼";
    }
    content += "\n💼 錢包：**" + std::to_string(chips) + "** 碼";
    if (def && def->ticker.empty()) {
        content += "\n\n*「" + def->name + "」漲跌純看心情，僅供娛樂。*";
        if (def->daily_cap_pct > 0) content += "\n*單日最大漲跌幅限制 ±" + std::to_string(def->daily_cap_pct) + "%。*";
    }
    content += "\n\n-# 👤 " + (dn.empty() ? uid_s : dn);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 買進").set_id("stock_buy_" + uid_s + "_" + key)
        .set_style(dpp::cos_success).set_disabled(info.price <= 0 || chips < info.price));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📤 賣出").set_id("stock_sell_" + uid_s + "_" + key)
        .set_style(dpp::cos_danger).set_disabled(shares <= 0));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 重整").set_id("stock_view_" + uid_s + "_" + key).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回股市").set_id("stock_home_" + uid_s).set_style(dpp::cos_secondary));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📋 持股").set_id("stock_holders_" + uid_s + "_" + key).set_style(dpp::cos_secondary));
    if (def && def->ticker.empty()) {
        std::string required_uid = def->controller_uid.empty() ? cfg.notify_user_id : def->controller_uid;
        if (!required_uid.empty() && uid_s == required_uid) {
            nav.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🎛️ 調整心情").set_id("stock_manual_set_" + uid_s + "_" + key).set_style(dpp::cos_primary));
        }
    }
    msg.add_component_v2(nav);
    return msg;
}

// ─── UI：股東頁（顯示某支股票所有持有人與持股數）─────────────────────────────────

static dpp::message make_stock_holders_msg(dpp::snowflake uid, const std::string& key) {
    std::string uid_s = std::to_string((uint64_t)uid);
    const StockDef* def = find_stock_def(key);
    std::string stock_title = def ? (def->emoji + " " + def->name) : key;

    std::vector<std::pair<dpp::snowflake, int64_t>> holders;
    int64_t total_shares = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [huid, holdings] : player_stocks) {
            auto hit = holdings.find(key);
            if (hit == holdings.end() || hit->second.shares <= 0) continue;
            holders.push_back({huid, hit->second.shares});
            total_shares += hit->second.shares;
        }
    }
    std::sort(holders.begin(), holders.end(), [](auto& a, auto& b) { return a.second > b.second; });

    std::string content = "## 📋  " + stock_title + " 股東名單\n";
    if (holders.empty()) {
        content += "目前沒有人持有這支股票。";
    } else {
        static const char* MEDALS[] = {"🥇", "🥈", "🥉"};
        int rank = 0;
        for (auto& [huid, shares] : holders) {
            std::string label = (rank < 3) ? MEDALS[rank] : (std::to_string(rank + 1) + ".");
            content += label + " <@" + std::to_string((uint64_t)huid) + ">　**" + std::to_string(shares) + "** 股\n";
            rank++;
            if (rank >= 25) { content += "-# ……僅顯示前 25 名"; break; }
        }
        content += "\n總計流通 **" + std::to_string(total_shares) + "** 股（" + std::to_string(holders.size()) + " 人持有）";
    }

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("stock_view_" + uid_s + "_" + key).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

// ─── 買賣執行 ─────────────────────────────────────────────────────────────────

static std::string do_stock_buy(dpp::snowflake uid, const std::string& key, int64_t qty) {
    if (qty <= 0) return "❌ 股數需為正整數！";
    int64_t price = 0;
    { std::lock_guard<std::mutex> lk(stock_mutex); auto it = stock_market.find(key); if (it != stock_market.end()) price = it->second.price; }
    if (price <= 0) return "❌ 目前查不到價格，請稍後再試！";
    int64_t cost = price * qty; // 買進不收手續費
    bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (chip_data[uid].chips >= cost) {
            chip_data[uid].chips -= cost;
            auto& h = player_stocks[uid][key];
            int64_t total_cost = h.avg_cost * h.shares + cost;
            h.shares += qty;
            h.avg_cost = h.shares > 0 ? (total_cost / h.shares) : 0;
            ok = true;
        }
    }
    if (!ok) return "❌ 籌碼不足！需要 " + std::to_string(cost) + " 碼。";
    save_chips(); save_stock_holdings();
    return "✅ 買進 **" + std::to_string(qty) + "** 股，花費 **" + std::to_string(cost) + "** 碼！";
}

static std::string do_stock_sell(dpp::snowflake uid, const std::string& key, int64_t qty) {
    if (qty <= 0) return "❌ 股數需為正整數！";
    int64_t price = 0;
    { std::lock_guard<std::mutex> lk(stock_mutex); auto it = stock_market.find(key); if (it != stock_market.end()) price = it->second.price; }
    if (price <= 0) return "❌ 目前查不到價格，請稍後再試！";
    int64_t revenue = 0; bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto pit = player_stocks.find(uid);
        if (pit != player_stocks.end()) {
            auto hit = pit->second.find(key);
            if (hit != pit->second.end() && hit->second.shares >= qty) {
                int64_t gross = price * qty;
                int64_t fee   = (int64_t)(gross * STOCK_FEE_RATE + 0.5); // 無條件進位
                revenue = gross - fee;
                hit->second.shares -= qty;
                if (hit->second.shares == 0) hit->second.avg_cost = 0;
                chip_data[uid].chips += revenue;
                ok = true;
            }
        }
    }
    if (!ok) return "❌ 持有股數不足！";
    save_chips(); save_stock_holdings();
    int64_t fee = price * qty - revenue;
    return "✅ 賣出 **" + std::to_string(qty) + "** 股，獲得 **" + std::to_string(revenue) + "** 碼（扣除手續費 " + std::to_string(fee) + " 碼）！";
}
