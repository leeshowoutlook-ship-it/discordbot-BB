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
            arr.push_back({
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
            });
        }
    }
    j["listings"] = arr;
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(AUCTION_FILE, j.dump(2));
}

static void load_auction() {
    std::ifstream f(AUCTION_FILE);
    if (!f.is_open()) return;
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
            if (a.id > 0) auction_listings[a.id] = a;
        }
    } catch (...) {}
}
