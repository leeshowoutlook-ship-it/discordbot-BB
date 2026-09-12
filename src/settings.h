#pragma once
#include "types.h"
#include "helpers.h"
#include <nlohmann/json.hpp>
#include <fstream>

// ─── 全域設定持久化：管理員可切換、需要跨重啟保留的開關 ───────────────────────

static const std::string SETTINGS_FILE = "settings.json";

static void save_settings() {
    nlohmann::json j = {
        {"claim_verify_enabled", g_claim_verify_enabled},
    };
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(SETTINGS_FILE, j.dump(2));
}

static void load_settings() {
    std::ifstream f(SETTINGS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        g_claim_verify_enabled = j.value("claim_verify_enabled", true);
    } catch (...) {}
}
