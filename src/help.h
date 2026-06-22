#pragma once
#include "helpers.h"

// ─── Help pages ───────────────────────────────────────────────────────────────
// Each page is a vector of {name, value} field pairs.

struct HelpPage {
    std::string title;
    std::vector<std::pair<std::string,std::string>> fields;
};

static const std::vector<HelpPage> HELP_PAGES = {
    {
        "⚔️  王團系統",
        {
            { "!王團報名 / /王團報名", "報名本週王團，選擇職業與可用時段" },
            { "!王團紀錄 / /王團紀錄", "查看所有報名紀錄，可按王篩選" },
        }
    },
    {
        "🪙  籌碼系統",
        {
            { "!領取 / /領取",             "每整點可領取 500 碼（只對自己顯示）" },
            { "!每週領取 / /每週領取",     "每週四12:00(UTC+8)可領取 2000 碼" },
            { "!錢包 / /錢包",             "查看自己籌碼量與統計" },
            { "!富豪榜 / /富豪榜",         "查看全伺服器籌碼排行榜（10人一頁）" },
            { "!轉帳 @對象 <碼> / /轉帳",  "轉移籌碼給其他人（需確認）" },
        }
    },
    {
        "🎮  遊戲系統",
        {
            { "!21 <碼> / /21 <碼>",           "用籌碼玩21點（支援軟21要牌、過五關必勝）" },
            { "!骰子 <碼> / /骰子 <碼>",       "擲三顆骰子，押大/小（1:1）或豹子3/18（1:220）" },
            { "!射 <碼> / /射 <碼>",            "射龍門：猜中間牌，間距越小賠率越高（撞柱輸雙倍）" },
            { "!火箭 <碼> / /火箭 <碼>",        "火箭升空：按鈕推進，隨時收手或等爆炸" },
            { "!刮 <碼> / /刮 <碼>",            "刮刮樂：翻開格子，3格安全後可加刮或收手" },
            { "!虧損榜 / /虧損榜",              "查看全伺服器各遊戲虧損排行榜" },
            { "!商店 / /商店",                  "瀏覽並購買楓之谷道具或虛擬商店道具" },
        }
    },
    {
        "🐾  寵物系統",
        {
            { "!寵物 / /寵物",             "查看寵物狀態、派出打工" },
            { "!背包 / /背包",              "查看背包道具，點選使用" },
            { "!寵物圖鑑 / /寵物圖鑑",     "查看所有寵物進化路線圖" },
            { "（商店 → 虛擬商店）",       "購買寵物蛋、孵蛋器、成長道具、進化道具、天賦道具" },
        }
    },
    {
        "🎊  活動與管理",
        {
            { "/抽獎",                         "（管理員）開抽獎，可設定報名費、身分組限制等" },
            { "!幸運頻道 <最大> / /幸運頻道",  "隨機抽出幸運頻道號碼" },
            { "!警告 @對象 [原因] / /警告",     "警告成員並累計次數" },
            { "!警告榜單 / /警告榜單",          "顯示警告排行榜" },
            { "!記帳 / /記帳",                  "（管理員）查看商店購買記帳本" },
            { "!管理員權限",                    "（擁有者）調整碼數、給蛋、給道具" },
            { "!幫助 / /幫助",                  "顯示此說明" },
        }
    },
};

// ─── Build help message ───────────────────────────────────────────────────────

static dpp::message make_help_msg(int page = 0) {
    int total = (int)HELP_PAGES.size();
    page = std::max(0, std::min(page, total - 1));
    const auto& hp = HELP_PAGES[page];

    dpp::embed e;
    e.set_title("📖  指令說明 — " + hp.title)
     .set_color(0x5865F2)
     .set_footer(dpp::embed_footer().set_text(
         "第 " + std::to_string(page + 1) + " 頁 / 共 " + std::to_string(total) + " 頁"));

    for (auto& [name, val] : hp.fields)
        e.add_field(name, val, false);

    dpp::message msg; msg.add_embed(e);

    if (total > 1) {
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component prev, next;
        prev.set_type(dpp::cot_button).set_label("◀ 上一頁")
            .set_id("help_prev_" + std::to_string(page))
            .set_style(dpp::cos_secondary).set_disabled(page == 0);
        next.set_type(dpp::cot_button).set_label("下一頁 ▶")
            .set_id("help_next_" + std::to_string(page))
            .set_style(dpp::cos_secondary).set_disabled(page == total - 1);
        row.add_component(prev);
        row.add_component(next);
        msg.add_component(row);
    }
    return msg;
}
