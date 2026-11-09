#pragma once
#include "types.h"
#include "persistence.h"
#include <fstream>

// ─── 拍賣行：存檔 ───────────────────────────────────────────────────────────

static const std::string AUCTION_FILE = "auction.json";

static void save_auction() {
    nlohmann::json j;
    j["next_id"] = auction_counter.load();
    nlohmann::json arr = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [id, a] : auction_listings) {
            nlohmann::json o = {
                {"id",         a.id},
                {"uid",        std::to_string((uint64_t)a.uid)},
                {"is_buy",     a.is_buy},
                {"item_id",    a.item_id},
                {"item_key",   a.item_key},
                {"item_name",  a.item_name},
                {"qty",        a.qty},
                {"currency",   a.currency},
                {"price",      a.price},
                {"created_at", (int64_t)a.created_at},
                {"expires_at", (int64_t)a.expires_at},
            };
            if (a.has_enh) o["enh"] = mv_enh_to_json(a.enh);
            arr.push_back(o);
        }
    }
    j["listings"] = arr;
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(AUCTION_FILE, j.dump(2));
}

static void load_auction() {
    std::ifstream f(AUCTION_FILE);
    if (!f.is_open()) return;
    bool migrated = false; // 有舊訂單補上到期時間 → 載入完馬上存一次，避免每次重開都重新起算 7 天
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        auction_counter = j.value("next_id", (uint64_t)1);
        for (auto& v : j.value("listings", nlohmann::json::array())) {
            AuctionListing a;
            a.id         = v.value("id", (uint64_t)0);
            a.uid        = dpp::snowflake(std::stoull(v.value("uid", std::string("0"))));
            a.is_buy     = v.value("is_buy", false);
            a.item_id    = v.value("item_id", 0);
            a.item_key   = v.value("item_key", std::string());
            a.item_name  = v.value("item_name", std::string());
            a.qty        = v.value("qty", (int64_t)1);
            a.currency   = v.value("currency", std::string("chips"));
            a.price      = v.value("price", (int64_t)0);
            a.created_at = (time_t)v.value("created_at", (int64_t)0);
            // 舊訂單沒有到期時間：從這次上線起算 7 天，不要一開機就把舊訂單全部下架
            a.expires_at = (time_t)v.value("expires_at", (int64_t)0);
            if (a.expires_at <= 0) { a.expires_at = time(nullptr) + AUCTION_EXPIRE_SECS; migrated = true; }
            if (v.contains("enh") && v["enh"].is_object()) { a.has_enh = true; a.enh = mv_enh_from_json(v["enh"]); }
            // 道具改過 ID（例如每週狩獵卷 96001→80002）：以 key 為準校正顯示用 ID
            if (!a.has_enh)
                if (const VirtualShopItem* vi = find_virtual_item(a.item_key))
                    if (vi->item_id > 0 && vi->item_id != a.item_id) { a.item_id = vi->item_id; migrated = true; }
            if (a.id > 0) auction_listings[a.id] = a;
        }
    } catch (...) {}
    f.close();
    if (migrated) save_auction();
}
