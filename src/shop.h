#pragma once
#include "pet.h"
#include <fstream>
#include <nlohmann/json.hpp>

struct ShopItem { std::string name; int64_t price; int total; int sold; };
static std::vector<ShopItem> maple_items = {
    {"中華拉麵500份",  20000,  -1, 0},
    {"棒冰棒500份",    30000,  -1, 0},
    {"紅豆刨冰500份",  50000,  -1, 0},
    {"瞬移石10個",    120000,  -1, 0},
    {"突襲卷7張",     250000,  -1, 0},
};
static const std::string SHOP_FILE      = "shop.json";
static const std::string PURCHASES_FILE = "purchases.json";

static void load_purchases() {
    std::ifstream f(PURCHASES_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        purchase_records.clear();
        uint64_t max_id = 0;
        for (auto& item : j) {
            PurchaseRecord r;
            r.id        = item.value("id",        (uint64_t)0);
            r.uid       = item.value("uid",        (uint64_t)0);
            r.username  = item.value("username",   std::string{});
            r.item_name = item.value("item_name",  std::string{});
            r.price     = item.value("price",      (int64_t)0);
            r.timestamp = item.value("timestamp",  (int64_t)0);
            r.source    = item.value("source",     std::string{"maple"});
            purchase_records.push_back(r);
            if (r.id > max_id) max_id = r.id;
        }
        if (max_id >= purchase_counter.load()) purchase_counter.store(max_id + 1);
    } catch (...) {}
}

static void save_purchases() {
    nlohmann::json j = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& r : purchase_records)
            j.push_back({{"id", r.id}, {"uid", (uint64_t)r.uid},
                         {"username", r.username}, {"item_name", r.item_name},
                         {"price", r.price}, {"timestamp", (int64_t)r.timestamp},
                         {"source", r.source}});
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(PURCHASES_FILE, j.dump(2));
}

static bool has_maple_bought_this_month(dpp::snowflake uid, const std::string& item_name) {
    time_t now = time(nullptr);
    struct tm t{}; localtime_s(&t, &now);
    int yr = t.tm_year, mo = t.tm_mon;
    for (auto& r : purchase_records) {
        if (r.source == "maple" && r.uid == uid && r.item_name == item_name) {
            struct tm rt{}; localtime_s(&rt, &r.timestamp);
            if (rt.tm_year == yr && rt.tm_mon == mo) return true;
        }
    }
    return false;
}

// ─── Ledger UI (admin only) ────────────────────────────────────────────────────
// filter: "all" | "maple" | "virtual"

static dpp::message make_ledger_msg(int page = 0, const std::string& filter = "all") {
    const int PAGE_SIZE = 8;
    std::vector<PurchaseRecord> records;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& r : purchase_records)
            if (filter == "all" || r.source == filter)
                records.push_back(r);
    }
    // newest first
    std::sort(records.begin(), records.end(),
        [](const PurchaseRecord& a, const PurchaseRecord& b){ return a.id > b.id; });

    int total = (int)records.size();
    int total_pages = std::max(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    page = std::max(0, std::min(page, total_pages - 1));
    int start = page * PAGE_SIZE;
    int end   = std::min(start + PAGE_SIZE, total);

    std::string title = (filter == "maple") ? "📒  楓之谷商店帳本"
                      : (filter == "virtual") ? "📒  虛擬商店帳本"
                      : "📒  購買記帳本";
    dpp::embed e;
    e.set_title(title).set_color(0x9B59B6);
    if (total == 0) {
        e.set_description("目前沒有購買紀錄。");
    } else {
        std::string desc;
        for (int i = start; i < end; i++) {
            auto& r = records[i];
            char buf[32]; struct tm t{}; localtime_s(&t, &r.timestamp);
            std::strftime(buf, sizeof(buf), "%m/%d %H:%M", &t);
            std::string src_tag = (r.source == "virtual") ? "🎮" : "🍁";
            desc += src_tag + " `#" + std::to_string(r.id) + "` **" + r.username + "** 購買了 **"
                  + r.item_name + "**（" + std::to_string(r.price) + " 碼）"
                  + "｜" + buf + "\n";
        }
        e.set_description(desc);
    }
    e.set_footer(dpp::embed_footer().set_text(
        "第 " + std::to_string(page+1) + "/" + std::to_string(total_pages) + " 頁  |  共 " + std::to_string(total) + " 筆"));

    dpp::message msg; msg.add_embed(e);

    // Tab row
    dpp::component tab_row; tab_row.set_type(dpp::cot_action_row);
    for (auto& [lbl, f] : std::vector<std::pair<std::string,std::string>>{
            {"全部", "all"}, {"🍁 楓之谷", "maple"}, {"💻 虛擬商店", "virtual"}}) {
        dpp::component btn;
        btn.set_type(dpp::cot_button).set_label(lbl)
           .set_id("ledger_tab_" + f)
           .set_style(filter == f ? dpp::cos_primary : dpp::cos_secondary)
           .set_disabled(filter == f);
        tab_row.add_component(btn);
    }
    msg.add_component(tab_row);

    // Delete buttons
    if (total > 0) {
        dpp::component del_row; del_row.set_type(dpp::cot_action_row);
        int added = 0;
        for (int i = start; i < end && added < 5; i++, added++) {
            auto& r = records[i];
            dpp::component btn;
            btn.set_type(dpp::cot_button).set_label("刪 #" + std::to_string(r.id))
               .set_id("ledger_del_" + filter + "_" + std::to_string(page) + "_" + std::to_string(r.id))
               .set_style(dpp::cos_danger);
            del_row.add_component(btn);
        }
        msg.add_component(del_row);
        if (end - start > 5) {
            dpp::component del_row2; del_row2.set_type(dpp::cot_action_row);
            for (int i = start + 5; i < end; i++) {
                auto& r = records[i];
                dpp::component btn;
                btn.set_type(dpp::cot_button).set_label("刪 #" + std::to_string(r.id))
                   .set_id("ledger_del_" + filter + "_" + std::to_string(page) + "_" + std::to_string(r.id))
                   .set_style(dpp::cos_danger);
                del_row2.add_component(btn);
            }
            msg.add_component(del_row2);
        }
    }

    // Nav row
    if (total_pages > 1) {
        dpp::component nav; nav.set_type(dpp::cot_action_row);
        dpp::component prev_btn, next_btn;
        prev_btn.set_type(dpp::cot_button).set_label("◀ 上一頁")
                .set_id("ledger_page_" + filter + "_" + std::to_string(page-1))
                .set_style(dpp::cos_secondary).set_disabled(page == 0);
        next_btn.set_type(dpp::cot_button).set_label("下一頁 ▶")
                .set_id("ledger_page_" + filter + "_" + std::to_string(page+1))
                .set_style(dpp::cos_secondary).set_disabled(page == total_pages-1);
        nav.add_component(prev_btn); nav.add_component(next_btn);
        msg.add_component(nav);
    }
    return msg;
}

// ─── Personal ledger (any user, own records, no delete) ───────────────────────

static dpp::message make_my_ledger_msg(dpp::snowflake uid, int page = 0, const std::string& filter = "all") {
    const int PAGE_SIZE = 8;
    std::string uid_s = std::to_string((uint64_t)uid);
    std::vector<PurchaseRecord> records;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& r : purchase_records)
            if (r.uid == uid && (filter == "all" || r.source == filter))
                records.push_back(r);
    }
    std::sort(records.begin(), records.end(),
        [](const PurchaseRecord& a, const PurchaseRecord& b){ return a.id > b.id; });

    int total = (int)records.size();
    int total_pages = std::max(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    page = std::max(0, std::min(page, total_pages - 1));
    int start = page * PAGE_SIZE;
    int end   = std::min(start + PAGE_SIZE, total);

    std::string title = (filter == "maple") ? "📒  我的楓之谷購買紀錄"
                      : (filter == "virtual") ? "📒  我的虛擬商店紀錄"
                      : "📒  我的購買紀錄";
    dpp::embed e;
    e.set_title(title).set_color(0x3498DB);
    if (total == 0) {
        e.set_description("你還沒有購買紀錄！");
    } else {
        std::string desc;
        for (int i = start; i < end; i++) {
            auto& r = records[i];
            char buf[32]; struct tm t{}; localtime_s(&t, &r.timestamp);
            std::strftime(buf, sizeof(buf), "%m/%d %H:%M", &t);
            std::string src_tag = (r.source == "virtual") ? "🎮" : "🍁";
            desc += src_tag + " **" + r.item_name + "**（" + std::to_string(r.price) + " 碼）｜" + buf + "\n";
        }
        e.set_description(desc);
    }
    e.set_footer(dpp::embed_footer().set_text(
        "第 " + std::to_string(page+1) + "/" + std::to_string(total_pages) + " 頁  |  共 " + std::to_string(total) + " 筆"));

    dpp::message msg; msg.add_embed(e);

    // Tab row
    dpp::component tab_row; tab_row.set_type(dpp::cot_action_row);
    for (auto& [lbl, f] : std::vector<std::pair<std::string,std::string>>{
            {"全部", "all"}, {"🍁 楓之谷", "maple"}, {"💻 虛擬商店", "virtual"}}) {
        dpp::component btn;
        btn.set_type(dpp::cot_button).set_label(lbl)
           .set_id("my_ledger_tab_" + uid_s + "_" + f)
           .set_style(filter == f ? dpp::cos_primary : dpp::cos_secondary)
           .set_disabled(filter == f);
        tab_row.add_component(btn);
    }
    msg.add_component(tab_row);

    // Nav row
    if (total_pages > 1) {
        dpp::component nav; nav.set_type(dpp::cot_action_row);
        dpp::component prev_btn, next_btn;
        prev_btn.set_type(dpp::cot_button).set_label("◀ 上一頁")
                .set_id("my_ledger_page_" + uid_s + "_" + filter + "_" + std::to_string(page-1))
                .set_style(dpp::cos_secondary).set_disabled(page == 0);
        next_btn.set_type(dpp::cot_button).set_label("下一頁 ▶")
                .set_id("my_ledger_page_" + uid_s + "_" + filter + "_" + std::to_string(page+1))
                .set_style(dpp::cos_secondary).set_disabled(page == total_pages-1);
        nav.add_component(prev_btn); nav.add_component(next_btn);
        msg.add_component(nav);
    }
    return msg;
}

static void load_shop() {
    std::ifstream f(SHOP_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        if (!j.contains("maple")) return;
        for (auto& item : maple_items) {
            if (j["maple"].contains(item.name))
                item.sold = j["maple"][item.name].value("sold", 0);
        }
    } catch (...) {}
}

static void save_shop() {
    nlohmann::json j;
    for (auto& item : maple_items)
        j["maple"][item.name] = {{"total", item.total}, {"sold", item.sold}};
    atomic_write(SHOP_FILE, j.dump(2));
}

// ─── Shop UI ──────────────────────────────────────────────────────────────────

static dpp::message make_shop_main_msg() {
    dpp::embed e;
    e.set_title("🏪  商店").set_color(0x1ABC9C);
    e.set_description("請選擇要前往的商店：");
    e.add_field("🍁  楓之谷商店", "購買楓之谷道具", true);
    e.add_field("💻  虛擬商店",   "購買寵物蛋、孵蛋工具、成長道具等",   true);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component maple_btn, virtual_btn;
    maple_btn.set_type(dpp::cot_button).set_label("🍁 楓之谷商店")
             .set_id("shop_maple_0").set_style(dpp::cos_success);
    virtual_btn.set_type(dpp::cot_button).set_label("💻 虛擬商店")
               .set_id("shop_virtual").set_style(dpp::cos_primary);
    row.add_component(maple_btn); row.add_component(virtual_btn);
    msg.add_component(row);
    return msg;
}

static dpp::message make_maple_shop_msg(int page = 0) {
    const int PAGE_SIZE = 5;
    int total_pages = ((int)maple_items.size() + PAGE_SIZE - 1) / PAGE_SIZE;
    page = std::max(0, std::min(page, total_pages - 1));
    int start = page * PAGE_SIZE;
    int end   = std::min(start + PAGE_SIZE, (int)maple_items.size());

    dpp::embed e;
    e.set_title("🍁  楓之谷商店").set_color(0xE74C3C);

    for (int i = start; i < end; i++) {
        auto& item = maple_items[i];
        std::string remain_str = (item.total == -1) ? "∞" : std::to_string(item.total - item.sold);
        e.add_field(item.name,
            "💰 **" + std::to_string(item.price) + "** 碼  |  剩餘：**" + remain_str + "**",
            false);
    }
    e.set_footer(dpp::embed_footer().set_text(
        "第 " + std::to_string(page+1) + "/" + std::to_string(total_pages) + " 頁"));

    dpp::message msg; msg.add_embed(e);

    // Buy buttons
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    for (int i = start; i < end; i++) {
        auto& item = maple_items[i];
        bool out = (item.total != -1 && item.sold >= item.total);
        dpp::component b;
        b.set_type(dpp::cot_button)
         .set_label((out ? "❌ " : "購買 ") + item.name)
         .set_id("shop_buy_" + std::to_string(i))
         .set_style(out ? dpp::cos_secondary : dpp::cos_primary)
         .set_disabled(out);
        row1.add_component(b);
    }
    msg.add_component(row1);

    // Nav buttons
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    dpp::component back_btn, prev_btn, next_btn;
    back_btn.set_type(dpp::cot_button).set_label("↩ 返回商店")
            .set_id("shop_main").set_style(dpp::cos_secondary);
    prev_btn.set_type(dpp::cot_button).set_label("◀ 上一頁")
            .set_id("shop_maple_" + std::to_string(page-1))
            .set_style(dpp::cos_secondary).set_disabled(page == 0);
    next_btn.set_type(dpp::cot_button).set_label("下一頁 ▶")
            .set_id("shop_maple_" + std::to_string(page+1))
            .set_style(dpp::cos_secondary).set_disabled(page == total_pages-1);
    row2.add_component(back_btn);
    if (total_pages > 1) { row2.add_component(prev_btn); row2.add_component(next_btn); }
    msg.add_component(row2);
    return msg;
}

static dpp::message make_buy_confirm_msg(dpp::snowflake uid, int item_idx) {
    if (item_idx < 0 || item_idx >= (int)maple_items.size()) return {};
    auto& item = maple_items[item_idx];
    bool unlimited = (item.total == -1);
    int remain = unlimited ? 1 : (item.total - item.sold);
    int64_t bal = get_chips(uid);

    dpp::embed e;
    e.set_title("🛒  購買確認").set_color(0xF39C12);
    e.add_field("📦  商品",   item.name,                             true);
    e.add_field("💰  售價",   std::to_string(item.price) + " 碼",   true);
    e.add_field("📊  剩餘",   unlimited ? "∞" : (std::to_string(remain) + " 份"), true);
    e.add_field("💼  你的餘額", std::to_string(bal) + " 碼",        false);

    dpp::message msg; msg.add_embed(e);
    if (remain <= 0 || bal < item.price) {
        dpp::embed err; err.set_title("❌  無法購買").set_color(0xE74C3C);
        err.set_description(remain <= 0 ? "商品已售完" : "籌碼不足");
        dpp::message m; m.add_embed(err); return m;
    }

    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component ok, cancel;
    ok.set_type(dpp::cot_button).set_label("✅ 確認購買")
      .set_id("shop_confirm_" + std::to_string(item_idx))
      .set_style(dpp::cos_success);
    cancel.set_type(dpp::cot_button).set_label("❌ 取消")
          .set_id("shop_maple_0").set_style(dpp::cos_danger);
    row.add_component(ok); row.add_component(cancel);
    msg.add_component(row);
    return msg;
}

static dpp::message handle_buy(dpp::snowflake uid, const std::string& username, int item_idx) {
    if (item_idx < 0 || item_idx >= (int)maple_items.size()) return {};
    auto& item = maple_items[item_idx];
    dpp::embed e;
    if (item.total != -1 && item.sold >= item.total) {
        e.set_title("❌  商品已售完").set_color(0xE74C3C);
        dpp::message m; m.add_embed(e); return m;
    }
    // Monthly limit: one purchase per item per person per calendar month
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (has_maple_bought_this_month(uid, item.name)) {
            e.set_title("❌  本月已購買過").set_color(0xE74C3C);
            e.set_description("**" + item.name + "** 每人每月限購一次。");
            dpp::message m; m.add_embed(e); return m;
        }
    }
    int64_t bal;
    bool ok = false;
    PurchaseRecord rec;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        bal = chip_data[uid].chips;
        if (bal >= item.price) {
            chip_data[uid].chips -= item.price;
            item.sold++;
            ok = true;
            bal = chip_data[uid].chips;
            rec.id        = purchase_counter.fetch_add(1);
            rec.uid       = uid;
            rec.username  = username;
            rec.item_name = item.name;
            rec.price     = item.price;
            rec.timestamp = std::time(nullptr);
            rec.source    = "maple";
            purchase_records.push_back(rec);
        }
    }
    if (!ok) {
        e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        dpp::message m; m.add_embed(e); return m;
    }
    save_chips(); save_shop(); save_purchases();
    e.set_title("✅  購買成功").set_color(0x2ECC71);
    e.add_field("📦  商品", item.name,                           true);
    e.add_field("💰  花費", std::to_string(item.price) + " 碼", true);
    e.add_field("💼  餘額", std::to_string(bal)        + " 碼", false);
    e.set_footer(dpp::embed_footer().set_text("請聯繫管理員領取商品"));
    dpp::message ret; ret.add_embed(e); return ret;
}

// ─── Virtual shop ─────────────────────────────────────────────────────────────

static dpp::message make_virtual_shop_msg() {
    dpp::embed e;
    e.set_title("💻  虛擬商店").set_color(0x9B59B6);
    e.set_description("請選擇商品類別：");
    e.add_field("🥚  寵物蛋",  "購買未孵化的寵物蛋",           true);
    e.add_field("🔥  孵蛋工具","使用後有機率孵化寵物蛋",        true);
    e.add_field("🌱  成長工具","使用後讓寵物獲得經驗值",         true);
    e.add_field("⚡  進化工具","使用後讓寵物進化到下一個階段",   true);
    e.add_field("🌿  成長路徑","購買道具開啟分支進化路線",       true);
    e.add_field("✨  天賦道具","賦予或重抽寵物天賦技能",         true);
    e.add_field("📜  特殊道具","怪物狩獵卷等特殊消耗品",         true);
    e.add_field("💊  恢復道具","解除寵物負面狀態的藥品",         true);
    e.add_field("⭐  特權道具","VIP 自動領籌碼、寵物監工等特殊效果", true);
    dpp::message msg; msg.add_embed(e);
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    auto mk = [&](const std::string& lbl, const std::string& id) {
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(lbl).set_id(id).set_style(dpp::cos_primary);
        row1.add_component(b);
    };
    mk("🥚 寵物蛋",  "shop_vcat_egg");
    mk("🔥 孵蛋工具","shop_vcat_incubator");
    mk("🌱 成長工具","shop_vcat_growth");
    mk("⚡ 進化工具","shop_vcat_evolution");
    mk("🌿 成長路徑","shop_vcat_path");
    msg.add_component(row1);
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    auto mk2 = [&](const std::string& lbl, const std::string& id) {
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(lbl).set_id(id).set_style(dpp::cos_primary);
        row2.add_component(b);
    };
    mk2("✨ 天賦道具","shop_vcat_talent");
    mk2("📜 特殊道具","shop_vcat_hunt");
    mk2("💊 恢復道具","shop_vcat_recovery");
    mk2("⭐ 特權道具","shop_vcat_privilege");
    dpp::component back_btn;
    back_btn.set_type(dpp::cot_button).set_label("↩ 返回商店主頁")
            .set_id("shop_main").set_style(dpp::cos_secondary);
    row2.add_component(back_btn); msg.add_component(row2);
    return msg;
}

static bool can_buy_virtual_cat(dpp::snowflake, const std::string&) {
    return true;
}

static dpp::message make_vcat_shop_msg(dpp::snowflake uid, const std::string& cat) {
    static const std::map<std::string,std::string> titles = {
        {"egg","🥚  寵物蛋"}, {"incubator","🔥  孵蛋工具"},
        {"growth","🌱  成長工具"}, {"evolution","⚡  進化工具"},
        {"path","🌿  成長路徑"}, {"talent","✨  天賦道具"},
        {"hunt","📜  特殊道具"}, {"recovery","💊  恢復道具"},
        {"privilege","⭐  特權道具"}
    };
    // hunt, recovery and privilege are always purchasable
    bool can_buy = (cat == "hunt" || cat == "recovery" || cat == "privilege") ? true : can_buy_virtual_cat(uid, cat);
    dpp::embed e;
    e.set_title(titles.count(cat) ? titles.at(cat) : "虛擬商店");
    e.set_color(0x1ABC9C);
    if (!can_buy) {
        std::string reason;
        if (cat == "egg")       reason = "你已擁有寵物，無法再購買蛋！";
        else if (cat == "incubator") reason = "你的寵物已孵化，不需要孵蛋工具！";
        else reason = "你的寵物已達最高階！";
        e.set_description("❌ " + reason + "\n\n以下商品供參考：");
    }

    std::vector<const VirtualShopItem*> items;
    for (auto& vi : VIRTUAL_ITEMS) if (vi.category == cat) items.push_back(&vi);
    for (auto* vi : items)
        e.add_field(vi->name, "💰 **" + std::to_string(vi->price) + "** 碼　" + vi->desc, false);
    int64_t bal = get_chips(uid);
    e.set_footer(dpp::embed_footer().set_text("你的餘額：" + std::to_string(bal) + " 碼"));

    dpp::message msg; msg.add_embed(e);
    if (can_buy) {
        dpp::component cur_row; cur_row.set_type(dpp::cot_action_row);
        int n = 0;
        for (auto* vi : items) {
            if (n > 0 && n % 5 == 0) {
                msg.add_component(cur_row);
                cur_row = dpp::component(); cur_row.set_type(dpp::cot_action_row);
            }
            dpp::component btn;
            btn.set_type(dpp::cot_button)
               .set_label("購買 " + vi->name)
               .set_id("shop_vbuy_" + vi->key)
               .set_style(dpp::cos_primary);
            cur_row.add_component(btn); n++;
        }
        if (n > 0) msg.add_component(cur_row);
    }
    dpp::component nav; nav.set_type(dpp::cot_action_row);
    dpp::component back_btn;
    back_btn.set_type(dpp::cot_button).set_label("↩ 返回虛擬商店")
            .set_id("shop_vback").set_style(dpp::cos_secondary);
    nav.add_component(back_btn); msg.add_component(nav);
    return msg;
}

static dpp::message make_vbuy_confirm_msg(dpp::snowflake uid, const std::string& key) {
    const VirtualShopItem* vi = find_virtual_item(key);
    dpp::embed e;
    if (!vi) {
        e.set_title("❌  商品不存在").set_color(0xE74C3C);
        dpp::message m; m.add_embed(e); return m;
    }
    int64_t bal = get_chips(uid);
    e.set_title("🛒  購買確認").set_color(0xF39C12);
    e.add_field("📦  商品",    vi->name,                           true);
    e.add_field("💰  售價",    std::to_string(vi->price) + " 碼", true);
    e.add_field("💼  你的餘額", std::to_string(bal) + " 碼",      false);
    dpp::message msg; msg.add_embed(e);
    if (bal < vi->price) {
        dpp::embed err; err.set_title("❌  籌碼不足").set_color(0xE74C3C);
        dpp::message m; m.add_embed(err); return m;
    }
    if (vi->category != "hunt" && vi->category != "recovery" && vi->category != "privilege"
        && !can_buy_virtual_cat(uid, vi->category)) {
        dpp::embed err; err.set_title("❌  條件不符").set_color(0xE74C3C);
        dpp::message m; m.add_embed(err); return m;
    }
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component ok_btn, cancel_btn;
    ok_btn.set_type(dpp::cot_button).set_label("✅ 確認購買")
          .set_id("shop_vconfirm_" + key).set_style(dpp::cos_success);
    cancel_btn.set_type(dpp::cot_button).set_label("❌ 取消")
              .set_id("shop_vcat_" + vi->category).set_style(dpp::cos_danger);
    row.add_component(ok_btn); row.add_component(cancel_btn);
    msg.add_component(row);
    return msg;
}

static dpp::message handle_vbuy(dpp::snowflake uid, const std::string& username,
                                 const std::string& key) {
    const VirtualShopItem* vi = find_virtual_item(key);
    dpp::embed e;
    if (!vi) {
        e.set_title("❌  商品不存在").set_color(0xE74C3C);
        dpp::message m; m.add_embed(e); return m;
    }
    // Check restriction again
    if (vi->category != "hunt" && vi->category != "recovery" && vi->category != "privilege"
        && !can_buy_virtual_cat(uid, vi->category)) {
        e.set_title("❌  條件不符").set_color(0xE74C3C);
        dpp::message m; m.add_embed(e); return m;
    }
    int64_t bal = 0; bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        bal = chip_data[uid].chips;
        if (bal >= vi->price) {
            chip_data[uid].chips -= vi->price;
            bal = chip_data[uid].chips;
            ok = true;
        }
    }
    if (!ok) {
        e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        dpp::message m; m.add_embed(e); return m;
    }

    // Eggs: create pet if no pet exists, otherwise add to inventory
    if (vi->category == "egg") {
        bool has_existing = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            has_existing = (pet_data.count(uid) > 0);
        }
        if (!has_existing) {
            std::string chain = chain_from_egg_key(key);
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                Pet p; p.chain = chain; p.stage = 0; p.exp = 0;
                pet_data[uid] = p;
            }
            save_pet_data();
        } else {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key]++;
            }
            save_inventory();
        }
    } else {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            inventory_data[uid][key]++;
        }
        save_inventory();
    }

    // Record purchase
    {
        PurchaseRecord rec;
        rec.id        = purchase_counter.fetch_add(1);
        rec.uid       = uid;
        rec.username  = username;
        rec.item_name = vi->name;
        rec.price     = vi->price;
        rec.timestamp = std::time(nullptr);
        rec.source    = "virtual";
        std::lock_guard<std::mutex> lk(data_mutex);
        purchase_records.push_back(rec);
    }
    save_chips(); save_purchases();

    e.set_title("✅  購買成功").set_color(0x2ECC71);
    e.add_field("📦  商品", vi->name,                           true);
    e.add_field("💰  花費", std::to_string(vi->price) + " 碼", true);
    e.add_field("💼  餘額", std::to_string(bal)        + " 碼", false);
    if (vi->category == "egg")
        e.set_footer(dpp::embed_footer().set_text("使用 !寵物 查看你的蛋！"));
    else
        e.set_footer(dpp::embed_footer().set_text("使用 !背包 來使用道具！"));
    dpp::message ret; ret.add_embed(e);

    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component cont_btn;
    cont_btn.set_type(dpp::cot_button).set_label("↩ 繼續購物")
            .set_id("shop_vcat_" + vi->category).set_style(dpp::cos_secondary);
    row.add_component(cont_btn); ret.add_component(row);
    return ret;
}
