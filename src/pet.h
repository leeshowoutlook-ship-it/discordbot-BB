#pragma once
#include "chips.h"
#include "gacha.h"
#include <random>
#include <fstream>
#include <nlohmann/json.hpp>

// ─── Virtual shop item definitions ───────────────────────────────────────────

struct VirtualShopItem {
    std::string key;
    std::string name;
    int64_t     price;
    std::string category; // "egg"|"incubator"|"growth"|"evolution"|"path"|"talent"
    std::string desc;
    int         item_id = 0; // 5-digit display ID
};

static const std::vector<VirtualShopItem> VIRTUAL_ITEMS = {
    // ── Eggs ────────────────────────────────────────────────────────────────
    {"egg_nunbao",  "嫩寶的蛋",   10000, "egg", "孵化成嫩寶",        10001},
    {"egg_gugu",    "菇菇仔的蛋", 10000, "egg", "孵化成菇菇仔",      10002},
    {"egg_feifei",  "肥肥的蛋",   10000, "egg", "孵化成肥肥",        10003},
    {"egg_penguin", "企鵝的蛋",   10000, "egg", "孵化成小企鵝",      10004},
    // ── Incubators ───────────────────────────────────────────────────────────
    {"inc_100", "豪華孵蛋器", 10000, "incubator", "100% 孵化成功",   20001},
    {"inc_60",  "優質孵蛋器",  5500, "incubator", "60% 孵化成功",    20002},
    {"inc_30",  "普通孵蛋器",  2000, "incubator", "30% 孵化成功",    20003},
    {"inc_10",  "一般孵蛋器",   500, "incubator", "10% 孵化成功",    20004},
    // ── Growth tools ─────────────────────────────────────────────────────────
    {"grow_1", "平平安安健健康康成長", 3000, "growth", "100% +10 經驗",              30001},
    {"grow_2", "野放自己亂長我無所謂", 3000, "growth", "60% +15 經驗",               30002},
    {"grow_3", "我加的是金珂拉",       3000, "growth", "30% +30 經驗",               30003},
    {"grow_4", "你必須要拿一百分",     3000, "growth", "10% +50 經驗",               30004},
    {"grow_5", "塞滿成長激素快快長大", 3000, "growth", "1% +250 經驗",               30005},
    {"grow_6", "壓力給滿 給老子長",   3000, "growth", "50%+30 / 25%+10 / 25%-20 經驗", 30006},
    // ── Evolution tools ───────────────────────────────────────────────────────
    {"evo_1",       "一階段進化祕笈",     10000, "evolution", "100% 進化（一階段）",                           40001},
    {"evo_2",       "二階段進化祕笈",     20000, "evolution", "100% 進化（二階段）",                           40002},
    {"evo_3",       "一階段進化祕笈碎片",  6000, "evolution", "70% 進化，失敗無懲罰（一階段）",                 40003},
    {"evo_4",       "二階段進化祕笈碎片", 11000, "evolution", "70% 進化，失敗無懲罰（二階段）",                 40004},
    {"evo_5",       "萬用進化機緣",        5000, "evolution", "50% 進化，50% -50 經驗",                        40005},
    {"evo_degrade", "退化卡",              5000, "evolution", "將寵物退化至前一階（保留最大經驗值，一階段無法退化）", 40006},
    // ── Talent items ─────────────────────────────────────────────────────────
    {"talent_scroll",  "天賦賦予卷軸",       15000, "talent", "100% 為寵物賦予一個天賦（可自選）",  50001},
    {"talent_class",   "送去上才藝班",        2000, "talent", "25% 為寵物發現一個隨機天賦",                            50002},
    {"talent_reroll",  "你不可以學畫畫!",    2000, "talent", "重新抽一個不同的天賦（需已有天賦）",                    50003},
    // ── Branch evolution path items ───────────────────────────────────────────
    {"path_moss_shell",    "苔癬蝸牛殼",       500, "path", "嫩寶一階段進化時觸發苔蘚分支",          60001},
    {"path_penguin_crown", "企鵝王冠",         500, "path", "小企鵝一階段進化時觸發王冠分支",        60002},
    {"path_curse",         "飽含詛咒的符咒",   500, "path", "菇菇仔一階段進化時觸發殭屍分支",        60003},
    {"path_blue_dye",      "藍色染料",         500, "path", "菇菇仔一階段進化時觸發藍菇分支（符咒優先）", 60004},
    {"path_desert",        "移居沙漠的機緣",   500, "path", "肥肥一階段進化時觸發沙漠分支",          60005},
    // ── Special collectibles ─────────────────────────────────────────────────
    {"star_unknown",    "未知的星星",    0, "special",  "不知道有什麼用的星星，可能在未來某一天會用到", 70001},
    // ── 怪物狩獵卷 ────────────────────────────────────────────────────────────
    {"hunt_scroll",        "怪物狩獵卷",      3000, "hunt",    "可用於開始怪物狩獵",                                   80001},
    {"weekly_hunt_scroll", "每週怪物狩獵卷",     0, "special", "用於挑戰組隊王，不可售出。每週四隨每週領取一同發放",   96001},
    {"orb_ticket",         "寶珠池專屬兌換卷",  0, "special", "使用後從所有寶珠（含稀有合成限定）中隨機獲得一顆，補償專用，不可交易", 97001},
    {"game_cancel",        "這局不算",          0, "special", "輸掉任何小遊戲後可按下「這局不算！！」取消該局並退還下注籌碼。已停止發放，持有者仍可使用。", 97002},
    {"half_refund",        "對不起我錯了",      0, "special", "輸掉任何小遊戲後可按下「對不起我錯了！！」返還一半輸掉的籌碼。組隊遠征（拉圖斯/暗黑龍王）極低機率掉落。", 97003},
    // ── Recovery items ────────────────────────────────────────────────────────
    {"recover_depress", "抗憂鬱藥物",  2000, "recovery", "解除負面狀態「憂鬱」",                      81001},
    {"recover_injury",  "高級傷藥",    2000, "recovery", "解除負面狀態「受傷」",                      81002},
    {"recover_muscle",  "肌肉舒緩劑",  2000, "recovery", "解除負面狀態「肌肉緊繃」",                  81003},
    {"recover_fatigue", "高級強效咖啡",2000, "recovery", "解除負面狀態「疲勞」",                      81004},
    // ── Orb shards（怪物掉落，10 個可合成寶珠）──────────────────────────────
    {"orb_shard_speed",  "迅捷狼王的寶珠碎片", 0, "shard", "10 個可合成「迅捷狼王的寶珠」，單人必定先手；組隊：20%機率多行動一回合",  95001},
    {"orb_shard_athena", "雅典娜的寶珠碎片",   0, "shard", "10 個可合成「雅典娜的寶珠」，單人30%恢復8滴血；組隊20%全體恢復5滴血", 95002},
    {"orb_shard_bear",   "巨山狂熊的寶珠碎片", 0, "shard", "10 個可合成「巨山狂熊的寶珠」，單人：防禦降低怪物下兩次攻擊60%；組隊：防禦降低王的傷害20%", 95003},
    {"orb_shard_viking", "維京的寶珠碎片",     0, "shard", "10 個可合成「維京的寶珠」，HP≤50% 傷害×1.4，HP≤25% 傷害×1.7（被動狂暴）", 95004},
    {"orb_shard_wargod",      "狂怒戰神的寶珠碎片", 0, "shard", "10 個可合成「狂怒戰神的寶珠」，裝備後攻擊力+10",                              95005},
    {"orb_shard_latus",      "拉圖斯的寶珠碎片",   0, "shard", "10 個可合成「拉圖斯的寶珠」，HP≤20%時回復至50%（每場一次）",               95006},
    {"orb_shard_darkdragon", "暗黑龍王的寶珠碎片", 0, "shard", "10 個可合成「暗黑龍王的寶珠」，攻擊後回復造成傷害的 1/10（最多回10HP）", 95007},
    // ── 特權道具 ─────────────────────────────────────────────────────────────
    {"vip_daily",            "尊爵VIP（每日）",  8000, "privilege", "使用後 24 小時內，每小時自動為你領取籌碼", 99001},
    {"pet_supervisor_daily", "寵物監工（每日）", 1000, "privilege", "使用後 24 小時內，寵物打工結束 10 分鐘後若未領取，自動以 0.6 倍收益再次出勤", 99002},
    {"pet_insurance",        "醫療保險",         1000, "privilege", "使用後三天內，寵物打工回來若生病（受傷除外），立即給付 4000 保險金並結束效果", 99003},
};

static const VirtualShopItem* find_virtual_item(const std::string& key) {
    for (auto& vi : VIRTUAL_ITEMS)
        if (vi.key == key) return &vi;
    return nullptr;
}

static const VirtualShopItem* find_virtual_item_by_id(int id) {
    if (!id) return nullptr;
    for (auto& vi : VIRTUAL_ITEMS)
        if (vi.item_id == id) return &vi;
    return nullptr;
}

// ─── Pet helpers ──────────────────────────────────────────────────────────────

static std::string pet_image_url(const std::string& chain, int stage,
                                  const std::string& variant = "") {
    if (stage < 0 || stage > 3) return "";

    if (chain == "嫩寶") {
        if (variant == "苔蘚") {
            static const char* u[] = {
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517006784626167918/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517007352631529563/image.png",
                "https://media.discordapp.net/attachments/1514918524164898966/1517223219638173836/content.png",
                "https://media.discordapp.net/attachments/1514918524164898966/1517223220158402681/content.png"
            };
            return u[stage];
        }
        static const char* u[] = {
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517006784626167918/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007352631529563/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007352878989423/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007353172725971/image.png"
        };
        return u[stage];
    }
    if (chain == "菇菇仔") {
        if (variant == "殭屍") {
            static const char* u[] = {
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517006737348104344/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517007192031494184/image.png",
                "https://media.discordapp.net/attachments/1514918524164898966/1517223216496508939/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517223217348087990/image.png"
            };
            return u[stage];
        }
        if (variant == "藍菇") {
            static const char* u[] = {
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517006737348104344/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517007192031494184/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517223218061246524/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517223218799312926/image.png"
            };
            return u[stage];
        }
        static const char* u[] = {
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517006737348104344/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007192031494184/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007192396660987/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007192908103791/image.png"
        };
        return u[stage];
    }
    if (chain == "肥肥") {
        if (variant == "沙漠") {
            static const char* u[] = {
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517006814305321041/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517007193461887137/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517223220728561844/image.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517223221252984892/image.png"
            };
            return u[stage];
        }
        static const char* u[] = {
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517006814305321041/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007193461887137/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007193839501562/image.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517007194195890226/image.png"
        };
        return u[stage];
    }
    if (chain == "小企鵝") {
        if (variant == "企鵝王") {
            static const char* u[] = {
                "https://media.discordapp.net/attachments/1514918524164898966/1517215013033738270/content.png",
                "https://cdn.discordapp.com/attachments/1514918524164898966/1517215049335439411/image.png",
                "https://media.discordapp.net/attachments/1514918524164898966/1517215487556321370/image.png",
                "https://media.discordapp.net/attachments/1514918524164898966/1517217516446027796/image.png"
            };
            return u[stage];
        }
        static const char* u[] = {
            "https://media.discordapp.net/attachments/1514918524164898966/1517215013033738270/content.png",
            "https://cdn.discordapp.com/attachments/1514918524164898966/1517215049335439411/image.png",
            "https://media.discordapp.net/attachments/1514918524164898966/1517217418253176994/image.png",
            "https://media.discordapp.net/attachments/1514918524164898966/1517215512260513812/image.png"
        };
        return u[stage];
    }
    return "";
}

static std::string pet_name(const std::string& chain, int stage,
                             const std::string& variant = "") {
    if (stage < 0 || stage > 3) return "???";
    if (chain == "嫩寶") {
        if (variant == "苔蘚") {
            static const char* n[] = {"嫩寶的蛋","嫩寶","苔蘚藍寶","苔蘚蝸牛"};
            return n[stage];
        }
        static const char* n[] = {"嫩寶的蛋","嫩寶","紅寶","紅寶王"};
        return n[stage];
    }
    if (chain == "菇菇仔") {
        if (variant == "殭屍") {
            static const char* n[] = {"菇菇仔的蛋","菇菇仔","殭屍菇菇","殭屍菇菇王"};
            return n[stage];
        }
        if (variant == "藍菇") {
            static const char* n[] = {"菇菇仔的蛋","菇菇仔","藍菇菇","藍菇菇王"};
            return n[stage];
        }
        static const char* n[] = {"菇菇仔的蛋","菇菇仔","菇菇寶貝","菇菇王"};
        return n[stage];
    }
    if (chain == "肥肥") {
        if (variant == "沙漠") {
            static const char* n[] = {"肥肥的蛋","肥肥","黑肥肥","鋼之黑肥肥"};
            return n[stage];
        }
        static const char* n[] = {"肥肥的蛋","肥肥","緞帶肥肥","鋼鐵肥肥"};
        return n[stage];
    }
    if (chain == "小企鵝") {
        if (variant == "企鵝王") {
            static const char* n[] = {"企鵝的蛋","小企鵝","企鵝王","黑企鵝王"};
            return n[stage];
        }
        static const char* n[] = {"企鵝的蛋","小企鵝","槍企鵝","頭盔企鵝"};
        return n[stage];
    }
    return "???";
}

static std::string chain_from_egg_key(const std::string& key) {
    if (key == "egg_nunbao")  return "嫩寶";
    if (key == "egg_gugu")    return "菇菇仔";
    if (key == "egg_feifei")  return "肥肥";
    if (key == "egg_penguin") return "小企鵝";
    return "";
}

static int exp_needed(int stage) {
    if (stage == 1) return 100;
    if (stage == 2) return 250;
    return 0;
}

struct WorkOption { int64_t pay; int exp_gain; };

// Returns (1hr, 4hr, 8hr) pay/exp for given stage.
static std::array<WorkOption, 3> work_options(int stage) {
    if (stage == 1) return {{ {70,1},  {250,3},  {450,5}  }};
    if (stage == 2) return {{ {190,1}, {670,3},  {1206,5} }};
    if (stage == 3) return {{ {570,1}, {2000,3}, {3600,5} }};
    return {{ {0,0}, {0,0}, {0,0} }};
}

// ─── Persistence ─────────────────────────────────────────────────────────────

static const std::string PETS_FILE      = "pets.json";
// INVENTORY_FILE defined in chips.h

static void load_pet_data() {
    std::ifstream f(PETS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            Pet& p = pet_data[uid];
            p.chain       = v.value("chain",       std::string{});
            p.stage       = v.value("stage",        0);
            p.exp         = v.value("exp",           0);
            p.work_task   = v.value("work_task",     0);
            p.work_end    = (time_t)v.value("work_end", (int64_t)0);
            p.onsen_end   = (time_t)v.value("onsen_end", (int64_t)0);
            p.is_supervisor_work = v.value("is_supervisor_work", false);
            p.notify_after_work  = v.value("notify_after_work",  false);
            p.work_notified      = v.value("work_notified",       false);
            p.onsen_notified     = v.value("onsen_notified",      false);
            p.variant     = v.value("variant",      std::string{});
            p.custom_name = v.value("custom_name",  std::string{});
            p.talent      = v.value("talent",       std::string{});
            p.statuses.clear();
            if (v.contains("statuses"))
                for (auto& s : v["statuses"]) p.statuses.push_back(s.get<std::string>());
        }
    } catch (...) {}
}

static void save_pet_data() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, p] : pet_data)
            j[std::to_string((uint64_t)uid)] = {
                {"chain", p.chain}, {"stage", p.stage}, {"exp", p.exp},
                {"work_task", p.work_task}, {"work_end", (int64_t)p.work_end},
                {"onsen_end", (int64_t)p.onsen_end}, {"is_supervisor_work", p.is_supervisor_work},
                {"notify_after_work", p.notify_after_work}, {"work_notified", p.work_notified},
                {"onsen_notified", p.onsen_notified},
                {"variant", p.variant}, {"custom_name", p.custom_name},
                {"talent", p.talent},
                {"statuses", [&]{ nlohmann::json a=nlohmann::json::array();
                    for (auto& s:p.statuses) a.push_back(s); return a; }()}
            };
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(PETS_FILE, j.dump(2));
}

static void load_inventory() {
    std::ifstream f(INVENTORY_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            for (auto& [ik, iv] : v.items())
                inventory_data[uid][ik] = iv.get<int>();
        }
    } catch (...) {}
}

// save_inventory() defined in chips.h (inline)

// ─── Pet view ─────────────────────────────────────────────────────────────────

static dpp::message make_lobby_msg(dpp::snowflake uid,
                                    const std::string& avatar_url = "",
                                    const std::string& display_name = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::embed e;
    e.set_title("🏠  大廳").set_color(0x5865F2);
    e.set_description("請選擇要前往的頁面：");
    {
        dpp::embed_footer footer;
        footer.text = "👤 " + (display_name.empty() ? uid_s : display_name);
        if (!avatar_url.empty()) footer.icon_url = avatar_url;
        e.set_footer(footer);
    }
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐾 寵物").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎒 背包").set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⚔️ 裝備").set_id("equip_main_" + uid_s).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏪 商店").set_id("lobby_shop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row);
    return msg;
}

static dpp::message make_pet_work_select_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    Pet pet;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end()) pet = it->second;
    }
    auto opts = work_options(pet.stage);
    dpp::embed e;
    e.set_title("💼  選擇打工時長").set_color(0x9B59B6);
    e.set_description("請選擇打工時長：");
    dpp::message msg; msg.add_embed(e);
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏃 1小時 (+" + std::to_string(opts[0].pay) + "碼/+" + std::to_string(opts[0].exp_gain) + "exp)")
        .set_id("pet_work_" + uid_s + "_1").set_style(dpp::cos_primary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏋 4小時 (+" + std::to_string(opts[1].pay) + "碼/+" + std::to_string(opts[1].exp_gain) + "exp)")
        .set_id("pet_work_" + uid_s + "_4").set_style(dpp::cos_primary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🌙 8小時 (+" + std::to_string(opts[2].pay) + "碼/+" + std::to_string(opts[2].exp_gain) + "exp)")
        .set_id("pet_work_" + uid_s + "_8").set_style(dpp::cos_primary));
    msg.add_component(row1);
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row2);
    return msg;
}

static dpp::message make_pet_view_msg(dpp::snowflake uid,
                                       const std::string& avatar_url = "",
                                       const std::string& display_name = "") {
    Pet pet; bool has_pet = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end()) { pet = it->second; has_pet = true; }
    }

    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::embed e; e.set_color(0x9B59B6);
    {
        dpp::embed_footer footer;
        footer.text = "👤 " + (display_name.empty() ? std::to_string((uint64_t)uid) : display_name);
        if (!avatar_url.empty()) footer.icon_url = avatar_url;
        e.set_footer(footer);
    }
    dpp::message msg;

    if (!has_pet) {
        e.set_title("🐾  我的寵物");
        e.set_description("你還沒有寵物！\n前往 **商店 → 虛擬商店 → 寵物蛋** 購買蛋。");
        msg.add_embed(e); return msg;
    }

    std::string name = pet_name(pet.chain, pet.stage, pet.variant);
    std::string display = pet.custom_name.empty() ? name : (pet.custom_name + "（" + name + "）");
    e.set_title("🐾  " + display);

    if (pet.stage == 0) {
        e.set_description("這是一顆 **" + name + "**\n使用孵蛋工具來孵化它！");
        e.add_field("狀態", "🥚 未孵化", false);
        std::string img0 = pet_image_url(pet.chain, 0, pet.variant);
        if (!img0.empty()) e.set_thumbnail(img0);
        msg.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component btn;
        btn.set_type(dpp::cot_button).set_label("🎒 背包")
           .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_primary);
        row.add_component(btn);
        msg.add_component(row);
        dpp::component rowlob; rowlob.set_type(dpp::cot_action_row);
        rowlob.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component(rowlob);
        dpp::component rowrel; rowrel.set_type(dpp::cot_action_row);
        dpp::component rel0;
        rel0.set_type(dpp::cot_button).set_label("🕊️ 放生")
            .set_id("pet_release_" + uid_s).set_style(dpp::cos_danger);
        rowrel.add_component(rel0);
        msg.add_component(rowrel);
        return msg;
    }

    // Stage 1-3
    std::string img = pet_image_url(pet.chain, pet.stage, pet.variant);
    if (!img.empty()) e.set_thumbnail(img);
    int need = exp_needed(pet.stage);
    std::string exp_str = std::to_string(pet.exp);
    if (pet.stage < 3) exp_str += " / " + std::to_string(need);
    e.add_field("📊  階段", "第 " + std::to_string(pet.stage) + " 階", true);
    e.add_field("✨  經驗值", exp_str, true);

    // Talent with brief description
    static const std::map<std::string,std::string> TALENT_DESC = {
        {"迅捷",     "迅捷：打工時長縮短 10%"},
        {"招人喜歡", "招人喜歡：打工報酬 +10%"},
        {"幸運",     "幸運：5% 機率雙倍打工報酬"},
        {"天然呆",   "天然呆：使用道具時不消耗（5%）"},
        {"喜歡作夢", "喜歡作夢：0.1% 機率籌碼翻倍"},
    };
    std::string talent_display = pet.talent.empty() ? "無" : pet.talent;
    if (!pet.talent.empty() && TALENT_DESC.count(pet.talent))
        talent_display = TALENT_DESC.at(pet.talent);
    e.add_field("✦  天賦", talent_display, true);

    // ATK/HP/DEF
    {
        PetStats stats = calc_pet_stats(uid, pet);
        e.add_field("⚔️  攻擊力", std::to_string(stats.atk), true);
        e.add_field("❤️  生命值",  std::to_string(stats.hp),  true);
        e.add_field("🛡️  防禦力", std::to_string(stats.def), true);
    }

    // Status effects
    if (!pet.statuses.empty()) {
        static const std::map<std::string,std::string> STATUS_DESC = {
            {"受傷",    "受傷：無法狩獵，打工報酬 -10%"},
            {"憂鬱",    "憂鬱：打工報酬 -20%，有機率隨機花錢"},
            {"肌肉緊繃","肌肉緊繃：狩獵時 30% 機率攻擊失敗"},
            {"疲勞",    "疲勞：打工時長 +30%"},
        };
        std::string status_str;
        for (auto& s : pet.statuses) status_str += "⚠️ **" + s + "**  ";
        e.add_field("🩹  狀態", status_str, false);
        std::string detail;
        for (auto& s : pet.statuses)
            if (STATUS_DESC.count(s)) detail += "• " + STATUS_DESC.at(s) + "\n";
        if (!detail.empty()) e.add_field("📋  狀態詳情", detail, false);
    }

    time_t now = time(nullptr);

    // Handle onsen completion: auto-clear debuffs
    bool onsen_done = (pet.onsen_end > 0 && pet.onsen_end <= now);
    if (onsen_done) {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& p = pet_data[uid];
            p.statuses.clear();
            p.onsen_end = 0;
            pet = p;
        }
        save_pet_data();
    }

    bool in_onsen  = (pet.onsen_end > 0 && pet.onsen_end > now);
    bool working   = (pet.work_task > 0 && pet.work_end > now);
    bool work_done = (pet.work_task > 0 && pet.work_end <= now);

    // Common row helpers
    auto add_lobby_row = [&]() {
        dpp::component r; r.set_type(dpp::cot_action_row);
        r.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component(r);
    };
    auto add_utility_row = [&]() {
        bool notify = pet.notify_after_work;
        dpp::component r; r.set_type(dpp::cot_action_row);
        r.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("✏️ 改名").set_id("pet_rename_" + uid_s).set_style(dpp::cos_secondary));
        r.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🕊️ 放生").set_id("pet_release_" + uid_s).set_style(dpp::cos_danger));
        r.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(notify ? "🔔 通知" : "🔕 通知")
            .set_id("pet_notify_toggle_" + uid_s)
            .set_style(notify ? dpp::cos_success : dpp::cos_secondary));
        msg.add_component(r);
    };

    // Onsen state
    if (in_onsen) {
        int remain = (int)(pet.onsen_end - now);
        int h = remain/3600, m = (remain%3600)/60, s = remain%60;
        char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
        e.add_field("🛀  溫泉狀態", "🌊 泡溫泉中，剩餘 " + std::string(buf) + "\n結束後自動清除所有負面狀態", false);
        msg.add_embed(e);
        dpp::component row1; row1.set_type(dpp::cot_action_row);
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🔄 刷新").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary));
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("❌ 取消泡溫泉").set_id("pet_cancel_onsen_" + uid_s).set_style(dpp::cos_danger));
        msg.add_component(row1);
        add_lobby_row();
        add_utility_row();
        return msg;
    }

    if (onsen_done)
        e.add_field("🛀  泡溫泉結果", "✨ 溫泉療癒完成！所有負面狀態已清除！", false);

    if (working) {
        std::string status_label = pet.is_supervisor_work
            ? "⏳ 打工中（監工派出，收益×0.6）" : "⏳ 打工中";
        int remain = (int)(pet.work_end - now);
        int h = remain/3600, m = (remain%3600)/60, s = remain%60;
        char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
        e.add_field("💼  打工狀態", status_label + "，剩餘 " + std::string(buf), false);
        msg.add_embed(e);
        dpp::component row1; row1.set_type(dpp::cot_action_row);
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🔄 刷新").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary));
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("❌ 取消打工").set_id("pet_cancel_work_" + uid_s).set_style(dpp::cos_danger));
        msg.add_component(row1);
        add_lobby_row();
        add_utility_row();
        return msg;
    } else if (work_done) {
        e.add_field("💼  打工狀態", "✅ 打工完成！按下按鈕領取獎勵", false);
        msg.add_embed(e);
        dpp::component row1; row1.set_type(dpp::cot_action_row);
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("💰 領取打工獎勵").set_id("pet_claim_" + uid_s).set_style(dpp::cos_success));
        msg.add_component(row1);
        add_lobby_row();
        add_utility_row();
        return msg;
    } else {
        e.add_field("💼  打工狀態", "😴 閒置", false);
        msg.add_embed(e);
        dpp::component row1; row1.set_type(dpp::cot_action_row);
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏃 打工").set_id("pet_work_select_" + uid_s).set_style(dpp::cos_primary));
        if (!pet.statuses.empty()) {
            row1.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("🛀 泡溫泉（2h）").set_id("pet_start_onsen_" + uid_s).set_style(dpp::cos_primary));
        }
        if (pet.stage == 3) {
            row1.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("✨ 提煉星星（-50 exp）").set_id("pet_refine_star_" + uid_s)
                .set_style(dpp::cos_primary).set_disabled(pet.exp < 50));
        }
        msg.add_component(row1);
        add_lobby_row();
        add_utility_row();
        return msg;
    }
}

// ─── Refine star (stage 3 only) ───────────────────────────────────────────────

static dpp::message handle_pet_refine_star(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::embed e; dpp::message m;
    bool success = false;
    int new_exp = 0, star_count = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto pi = pet_data.find(uid);
        if (pi == pet_data.end() || pi->second.stage != 3) {
            e.set_title("❌  無法提煉").set_color(0xE74C3C);
            e.set_description("需要三階段寵物才能提煉！");
            m.add_embed(e); return m;
        }
        auto& pet = pi->second;
        if (pet.exp < 50) {
            e.set_title("❌  經驗值不足").set_color(0xE74C3C);
            e.set_description("需要 **50** 經驗值才能提煉，目前只有 **" + std::to_string(pet.exp) + "**！");
            m.add_embed(e); return m;
        }
        pet.exp -= 50;
        new_exp = pet.exp;
        static std::mt19937 rng(std::random_device{}());
        success = std::uniform_int_distribution<int>(1, 100)(rng) <= 90;
        if (success) {
            inventory_data[uid]["star_unknown"]++;
            star_count = inventory_data[uid]["star_unknown"];
        }
    }
    save_pet_data();
    save_inventory();
    if (success) {
        e.set_title("✨  提煉成功！").set_color(0xF1C40F);
        e.set_description("消耗 **50** 經驗值，成功提煉出一顆 ⭐ **未知的星星**！\n"
                          "不知道有什麼用的星星，可能在未來某一天會用到。\n\n"
                          "目前持有：**" + std::to_string(star_count) + "** 顆星星\n"
                          "剩餘經驗值：**" + std::to_string(new_exp) + "**");
    } else {
        e.set_title("💨  提煉失敗").set_color(0x95A5A6);
        e.set_description("消耗 **50** 經驗值，但這次提煉失敗了...\n"
                          "剩餘經驗值：**" + std::to_string(new_exp) + "**");
    }
    m.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("🏠 大廳")
        .set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(back); m.add_component(row);
    return m;
}

// ─── Release pet ─────────────────────────────────────────────────────────────

static dpp::message handle_pet_release(dpp::snowflake uid) {
    std::string name;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it == pet_data.end()) {
            dpp::embed e; e.set_title("❌  錯誤").set_color(0xE74C3C);
            e.set_description("你沒有寵物！");
            dpp::message m; m.add_embed(e); return m;
        }
        name = pet_name(it->second.chain, it->second.stage, it->second.variant);
        pet_data.erase(it);
    }
    save_pet_data();
    dpp::embed e; e.set_title("🕊️  放生成功").set_color(0x95A5A6);
    e.set_description("**" + name + "** 已經被放生了，希望牠一切都好。");
    dpp::message m; m.add_embed(e); return m;
}

// ─── Backpack sell price by rarity ───────────────────────────────────────────

static int64_t eq_sell_price(const std::string& rarity) {
    if (rarity == "C")  return 1;
    if (rarity == "R")  return 5;
    if (rarity == "SR") return 10;
    if (rarity == "UR") return 100;
    return 0;
}

// ─── Backpack — Equipment tab ─────────────────────────────────────────────────

static dpp::message make_bag_equip_msg(dpp::snowflake uid) {
    std::map<std::string,int> inv;
    PlayerEquipment cur_eq;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) inv = ii->second;
        auto ei = equipped_data.find(uid);
        if (ei != equipped_data.end()) cur_eq = ei->second;
    }
    std::string uid_s = std::to_string((uint64_t)uid);

    auto is_equipped = [&](const std::string& key) {
        return key == cur_eq.weapon || key == cur_eq.glove || key == cur_eq.clothes
            || key == cur_eq.shoes  || key == cur_eq.orb;
    };

    static const std::map<std::string,std::string> SLOT_LABEL = {
        {"W","⚔️ 武器"}, {"G","🧤 手套"}, {"C","👘 套服"}, {"S","👟 鞋子"}, {"K","💎 靈魂寶珠"}
    };

    // Collect EQ_ entries, sorted by rarity desc then slot
    struct EqEntry { std::string key; int count; std::string rarity; std::string slot; };
    std::vector<EqEntry> eq_entries;
    for (auto& [k, cnt] : inv) {
        if (k.size() > 3 && k.substr(0, 3) == "EQ_" && cnt > 0) {
            auto* gi = find_gacha_item(k);
            if (gi) eq_entries.push_back({k, cnt, gi->rarity, gi->slot});
        }
    }
    std::sort(eq_entries.begin(), eq_entries.end(), [](const EqEntry& a, const EqEntry& b) {
        static const std::map<std::string,int> SLOT_ORD = {{"W",0},{"G",1},{"C",2},{"S",3},{"K",4}};
        int sa = SLOT_ORD.count(a.slot) ? SLOT_ORD.at(a.slot) : 9;
        int sb = SLOT_ORD.count(b.slot) ? SLOT_ORD.at(b.slot) : 9;
        if (sa != sb) return sa < sb;
        static const std::map<std::string,int> RAR_ORD = {{"UR",0},{"SR",1},{"R",2},{"C",3}};
        int ra = RAR_ORD.count(a.rarity) ? RAR_ORD.at(a.rarity) : 9;
        int rb = RAR_ORD.count(b.rarity) ? RAR_ORD.at(b.rarity) : 9;
        return ra < rb;
    });

    dpp::embed e; e.set_title("⚔️  背包 — 裝備").set_color(0xE67E22);
    dpp::message msg;

    if (eq_entries.empty()) {
        e.set_description("還沒有任何裝備！\n使用 `!轉蛋` 來抽取裝備。");
        msg.add_embed(e);
        dpp::component nav; nav.set_type(dpp::cot_action_row);
        dpp::component to_items, back;
        to_items.set_type(dpp::cot_button).set_label("🎒 道具")
                .set_id("bag_tab_items_" + uid_s).set_style(dpp::cos_secondary);
        back.set_type(dpp::cot_button).set_label("🏠 大廳")
            .set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary);
        nav.add_component(to_items); nav.add_component(back);
        msg.add_component(nav);
        return msg;
    }

    // Build description
    std::string desc;
    bool has_sellable_C = false, has_sellable_R = false,
         has_sellable_SR = false, has_sellable_UR = false;
    std::string last_slot;
    for (auto& en : eq_entries) {
        auto* gi = find_gacha_item(en.key);
        if (!gi) continue;
        if (gi->slot != last_slot) {
            if (!last_slot.empty()) desc += "\n";
            last_slot = gi->slot;
            desc += "**" + (SLOT_LABEL.count(gi->slot) ? SLOT_LABEL.at(gi->slot) : gi->slot) + "**\n";
        }
        bool eq = is_equipped(en.key);
        desc += std::string(eq ? "✅ " : "　") + "**" + gi->name + "**"
              + "（" + gi->rarity + "）"
              + " ×" + std::to_string(en.count)
              + "  `#" + std::to_string(gi->item_id) + "`"
              + "  💰" + std::to_string(eq_sell_price(gi->rarity)) + "碼\n";
        int unequipped = en.count - (eq ? 1 : 0);
        if (unequipped > 0) {
            if (gi->rarity == "C")  has_sellable_C  = true;
            if (gi->rarity == "R")  has_sellable_R  = true;
            if (gi->rarity == "SR") has_sellable_SR = true;
            if (gi->rarity == "UR") has_sellable_UR = true;
        }
    }
    e.set_description(desc);
    e.set_footer(dpp::embed_footer().set_text("✅ = 已裝備（批量售出會跳過已裝備的）"));
    msg.add_embed(e);

    // Nav row only — sell page is separate
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    dpp::component to_items, sell_btn, back;
    to_items.set_type(dpp::cot_button).set_label("🎒 道具")
            .set_id("bag_tab_items_" + uid_s).set_style(dpp::cos_secondary);
    sell_btn.set_type(dpp::cot_button).set_label("💰 售出")
            .set_id("bag_sell_page_equip_" + uid_s).set_style(dpp::cos_danger);
    back.set_type(dpp::cot_button).set_label("🏠 大廳")
        .set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary);
    nav_row.add_component(to_items); nav_row.add_component(sell_btn); nav_row.add_component(back);
    msg.add_component(nav_row);
    return msg;
}

// ─── Backpack — Equipment sell page ──────────────────────────────────────────

static dpp::message make_bag_sell_equip_msg(dpp::snowflake uid) {
    std::map<std::string,int> inv;
    PlayerEquipment cur_eq;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) inv = ii->second;
        auto ei = equipped_data.find(uid);
        if (ei != equipped_data.end()) cur_eq = ei->second;
    }
    std::string uid_s = std::to_string((uint64_t)uid);

    auto is_equipped = [&](const std::string& key) {
        return key == cur_eq.weapon || key == cur_eq.glove || key == cur_eq.clothes
            || key == cur_eq.shoes  || key == cur_eq.orb;
    };

    struct EqEntry { std::string key; int count; std::string rarity; std::string slot; };
    std::vector<EqEntry> eq_entries;
    for (auto& [k, cnt] : inv) {
        if (k.size() > 3 && k.substr(0, 3) == "EQ_" && cnt > 0) {
            auto* gi = find_gacha_item(k);
            if (gi) eq_entries.push_back({k, cnt, gi->rarity, gi->slot});
        }
    }
    static const std::map<std::string,int> SLOT_ORDER  = {{"W",0},{"G",1},{"C",2},{"S",3},{"K",4}};
    static const std::map<std::string,int> RARITY_ORDER = {{"UR",0},{"SR",1},{"R",2},{"C",3}};
    std::sort(eq_entries.begin(), eq_entries.end(), [](const EqEntry& a, const EqEntry& b) {
        int sa = SLOT_ORDER.count(a.slot) ? SLOT_ORDER.at(a.slot) : 9;
        int sb = SLOT_ORDER.count(b.slot) ? SLOT_ORDER.at(b.slot) : 9;
        if (sa != sb) return sa < sb;
        int ra = RARITY_ORDER.count(a.rarity) ? RARITY_ORDER.at(a.rarity) : 9;
        int rb = RARITY_ORDER.count(b.rarity) ? RARITY_ORDER.at(b.rarity) : 9;
        return ra < rb;
    });

    dpp::embed e; e.set_title("💰  售出裝備").set_color(0xE74C3C);
    dpp::message msg;

    if (eq_entries.empty()) {
        e.set_description("沒有可售出的裝備。");
        msg.add_embed(e);
    } else {
        bool has_C = false, has_R = false, has_SR = false, has_UR = false;
        for (auto& en : eq_entries) {
            auto* gi = find_gacha_item(en.key);
            if (!gi) continue;
            int unequipped = en.count - (is_equipped(en.key) ? 1 : 0);
            if (unequipped > 0) {
                if (gi->rarity == "C")  has_C  = true;
                if (gi->rarity == "R")  has_R  = true;
                if (gi->rarity == "SR") has_SR = true;
                if (gi->rarity == "UR") has_UR = true;
            }
        }
        e.set_description("點擊按鈕售出裝備（已裝備中的會跳過）。\n**回收價：** C=1碼 R=5碼 SR=10碼 UR=100碼");
        e.set_footer(dpp::embed_footer().set_text("✅ = 已裝備，無法售出"));
        msg.add_embed(e);

        // Individual sell buttons (rows 1-3, max 15)
        dpp::component cur_row; cur_row.set_type(dpp::cot_action_row);
        int n = 0;
        for (auto& en : eq_entries) {
            if (n >= 15) break;
            auto* gi = find_gacha_item(en.key);
            if (!gi) continue;
            if (n > 0 && n % 5 == 0) {
                msg.add_component(cur_row);
                cur_row = dpp::component(); cur_row.set_type(dpp::cot_action_row);
            }
            bool eq = is_equipped(en.key);
            int unequipped = en.count - (eq ? 1 : 0);
            dpp::component btn;
            btn.set_type(dpp::cot_button)
               .set_label((eq ? "✅ " : "") + gi->name + "（" + gi->rarity + "）+" + std::to_string(eq_sell_price(gi->rarity)) + "碼")
               .set_id("bag_sell_eq_" + uid_s + "_" + en.key)
               .set_style(dpp::cos_danger)
               .set_disabled(unequipped <= 0);
            cur_row.add_component(btn); n++;
        }
        if (n > 0) msg.add_component(cur_row);

        // Bulk sell row (row 4)
        dpp::component bulk_row; bulk_row.set_type(dpp::cot_action_row);
        auto mk_bulk = [&](const std::string& rarity, bool has_any) {
            dpp::component b;
            b.set_type(dpp::cot_button)
             .set_label("批量售出 " + rarity)
             .set_id("bag_sell_bulk_" + uid_s + "_" + rarity)
             .set_style(dpp::cos_danger).set_disabled(!has_any);
            bulk_row.add_component(b);
        };
        mk_bulk("C", has_C); mk_bulk("R", has_R); mk_bulk("SR", has_SR); mk_bulk("UR", has_UR);
        msg.add_component(bulk_row);
    }

    // Nav row (row 5)
    dpp::component nav; nav.set_type(dpp::cot_action_row);
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回裝備背包")
        .set_id("bag_tab_equip_" + uid_s).set_style(dpp::cos_secondary);
    nav.add_component(back); msg.add_component(nav);
    return msg;
}

// ─── Backpack — Items tab ─────────────────────────────────────────────────────

static dpp::message make_pet_use_msg(dpp::snowflake uid, int page = 0) {
    Pet pet; bool has_pet = false;
    std::map<std::string,int> inv;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto pi = pet_data.find(uid);
        if (pi != pet_data.end()) { pet = pi->second; has_pet = true; }
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) inv = ii->second;
    }

    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::embed e; e.set_title("🎒  背包 — 道具").set_color(0x3498DB);
    dpp::message msg;

    // Gather virtual items (excluding eggs)
    struct ItemEntry { std::string key; int count; };
    std::vector<ItemEntry> entries;
    for (auto& vi : VIRTUAL_ITEMS) {
        if (vi.category == "egg") continue;
        auto it = inv.find(vi.key);
        if (it != inv.end() && it->second > 0)
            entries.push_back({vi.key, it->second});
    }

    if (entries.empty()) {
        e.set_description("道具欄是空的！\n前往 **商店 → 虛擬商店** 購買道具。");
        msg.add_embed(e);
        dpp::component nav; nav.set_type(dpp::cot_action_row);
        dpp::component to_eq, back;
        to_eq.set_type(dpp::cot_button).set_label("⚔️ 裝備")
             .set_id("bag_tab_equip_" + uid_s).set_style(dpp::cos_secondary);
        back.set_type(dpp::cot_button).set_label("🏠 大廳")
            .set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary);
        nav.add_component(to_eq); nav.add_component(back);
        msg.add_component(nav);
        return msg;
    }

    std::string desc;
    for (auto& en : entries) {
        auto* vi = find_virtual_item(en.key);
        if (!vi) continue;
        std::string id_str = vi->item_id ? ("`" + std::to_string(vi->item_id) + "`  ") : "";
        desc += id_str + "**" + vi->name + "** ×" + std::to_string(en.count) + "　" + vi->desc + "\n";
    }
    e.set_description(desc);
    msg.add_embed(e);

    auto is_disabled = [&](const std::string& key) -> bool {
        if (key == "orb_ticket") return false;
        // 特權道具不需要寵物
        if (key == "vip_daily" || key == "pet_supervisor_daily" || key == "pet_insurance") return false;
        auto* vi = find_virtual_item(key);
        if (!vi) return true;
        // 蛋：沒有寵物時才能使用
        if (vi->category == "egg") return has_pet;
        if (!has_pet) return true;
        if (vi->category == "incubator") return pet.stage != 0;
        if (vi->category == "growth")    return pet.stage == 0; // stage 3 allowed: grind exp for star refine
        if (vi->category == "talent") {
            if (key == "talent_reroll") return (pet.stage == 0 || pet.talent.empty());
            if (key == "talent_scroll") return (pet.stage == 0);
            return (pet.stage == 0 || !pet.talent.empty());
        }
        if (vi->category == "path")      return false;
        if (vi->category == "special")   return key != "orb_ticket";
        if (vi->category == "recovery")  return !has_pet || pet.stage == 0;
        if (vi->category == "hunt")      return true;
        if (vi->category == "evolution") {
            if (key == "evo_degrade") return (pet.stage <= 1);
            if (pet.stage == 0 || pet.stage == 3) return true;
            if ((key == "evo_1" || key == "evo_3") && pet.stage != 1) return true;
            if ((key == "evo_2" || key == "evo_4") && pet.stage != 2) return true;
            if (key == "evo_5") return (pet.stage == 0 || pet.stage == 3);
            int need = exp_needed(pet.stage);
            if (pet.exp < need) return true;
        }
        return false;
    };

    // Use buttons — paginated, 10 items per page (2 rows × 5)
    const int PER_PAGE = 10;
    int total_pages = std::max(1, ((int)entries.size() + PER_PAGE - 1) / PER_PAGE);
    if (page < 0) page = 0;
    if (page >= total_pages) page = total_pages - 1;
    int start = page * PER_PAGE;
    int end = std::min(start + PER_PAGE, (int)entries.size());

    dpp::component cur_row; cur_row.set_type(dpp::cot_action_row);
    int n = 0;
    for (int i = start; i < end; i++) {
        auto& en = entries[i];
        auto* vi = find_virtual_item(en.key);
        if (!vi) continue;
        if (n > 0 && n % 5 == 0) {
            msg.add_component(cur_row);
            cur_row = dpp::component(); cur_row.set_type(dpp::cot_action_row);
        }
        bool dis = is_disabled(en.key);
        dpp::component btn;
        btn.set_type(dpp::cot_button)
           .set_label(vi->name + " ×" + std::to_string(en.count))
           .set_id("pet_use_" + uid_s + "_" + en.key)
           .set_style(dis ? dpp::cos_secondary : dpp::cos_primary)
           .set_disabled(dis);
        cur_row.add_component(btn); n++;
    }
    if (n > 0) msg.add_component(cur_row);

    // Pagination row (only shown when more than one page)
    if (total_pages > 1) {
        dpp::component pg_row; pg_row.set_type(dpp::cot_action_row);
        dpp::component prev_btn, page_lbl, next_btn;
        prev_btn.set_type(dpp::cot_button).set_label("◀ 上頁")
                .set_id("pet_bag_page_" + uid_s + "_" + std::to_string(page - 1))
                .set_style(dpp::cos_secondary).set_disabled(page == 0);
        page_lbl.set_type(dpp::cot_button)
                .set_label("第 " + std::to_string(page + 1) + " / " + std::to_string(total_pages) + " 頁")
                .set_id("pet_bag_noop_" + uid_s)
                .set_style(dpp::cos_secondary).set_disabled(true);
        next_btn.set_type(dpp::cot_button).set_label("▶ 下頁")
                .set_id("pet_bag_page_" + uid_s + "_" + std::to_string(page + 1))
                .set_style(dpp::cos_secondary).set_disabled(page == total_pages - 1);
        pg_row.add_component(prev_btn); pg_row.add_component(page_lbl); pg_row.add_component(next_btn);
        msg.add_component(pg_row);
    }

    // Nav row (rows 3) — sell page is separate
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    dpp::component to_eq, sell_btn2, back_btn, discard_btn;
    to_eq.set_type(dpp::cot_button).set_label("⚔️ 裝備")
         .set_id("bag_tab_equip_" + uid_s).set_style(dpp::cos_secondary);
    sell_btn2.set_type(dpp::cot_button).set_label("💰 售出")
             .set_id("bag_sell_page_items_" + uid_s).set_style(dpp::cos_danger);
    back_btn.set_type(dpp::cot_button).set_label("🏠 大廳")
            .set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary);
    discard_btn.set_type(dpp::cot_button).set_label("🗑️ 丟棄道具")
               .set_id("pet_discard_mode_" + uid_s).set_style(dpp::cos_danger);
    nav_row.add_component(to_eq); nav_row.add_component(sell_btn2);
    nav_row.add_component(back_btn); nav_row.add_component(discard_btn);
    msg.add_component(nav_row);
    return msg;
}

// ─── Backpack — Items sell page ───────────────────────────────────────────────

static dpp::message make_bag_sell_items_msg(dpp::snowflake uid) {
    std::map<std::string,int> inv;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) inv = ii->second;
    }
    std::string uid_s = std::to_string((uint64_t)uid);

    struct ItemEntry { std::string key; int count; };
    std::vector<ItemEntry> entries;
    for (auto& vi : VIRTUAL_ITEMS) {
        if (vi.category == "egg") continue;
        auto it = inv.find(vi.key);
        if (it != inv.end() && it->second > 0)
            entries.push_back({vi.key, it->second});
    }

    dpp::embed e; e.set_title("💰  售出道具").set_color(0xE74C3C);
    dpp::message msg;

    if (entries.empty()) {
        e.set_description("沒有可售出的道具。");
        msg.add_embed(e);
    } else {
        e.set_description("售出價格為購買價的 40%。狩獵卷不可售出。");
        msg.add_embed(e);

        // Sell buttons (rows 1-2, max 10)
        dpp::component sell_row; sell_row.set_type(dpp::cot_action_row);
        int sn = 0;
        for (auto& en : entries) {
            if (sn >= 10) break;
            auto* vi = find_virtual_item(en.key);
            if (!vi) continue;
            if (sn > 0 && sn % 5 == 0) {
                msg.add_component(sell_row);
                sell_row = dpp::component(); sell_row.set_type(dpp::cot_action_row);
            }
            int64_t sell_p = (vi->price > 0 && vi->category != "hunt") ? std::max((int64_t)1, (int64_t)(vi->price * 0.4)) : 0;
            bool can_sell = sell_p > 0;
            dpp::component sbtn;
            sbtn.set_type(dpp::cot_button)
                .set_label(vi->name + (can_sell ? (" +" + std::to_string(sell_p) + "碼") : "（不可售）"))
                .set_id("bag_sell_item_" + uid_s + "_" + en.key)
                .set_style(dpp::cos_danger).set_disabled(!can_sell);
            sell_row.add_component(sbtn); sn++;
        }
        if (sn > 0) msg.add_component(sell_row);
    }

    // Nav row
    dpp::component nav; nav.set_type(dpp::cot_action_row);
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回道具背包")
        .set_id("bag_tab_items_" + uid_s).set_style(dpp::cos_secondary);
    nav.add_component(back); msg.add_component(nav);
    return msg;
}

// ─── Use item handler ─────────────────────────────────────────────────────────

static dpp::message handle_pet_use_item(dpp::snowflake uid, const std::string& key) {
    Pet pet; bool has_pet = false; int item_count = 0;
    std::map<std::string,int> inv_snapshot;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto pi = pet_data.find(uid);
        if (pi != pet_data.end()) { pet = pi->second; has_pet = true; }
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) {
            inv_snapshot = ii->second;
            auto ci = ii->second.find(key);
            if (ci != ii->second.end()) item_count = ci->second;
        }
    }

    const VirtualShopItem* vi = find_virtual_item(key);
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::embed e; dpp::message m;

    auto err = [&](const std::string& msg_text) {
        e.set_title("❌  無法使用").set_color(0xE74C3C);
        e.set_description(msg_text);
        m.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component bb;
        bb.set_type(dpp::cot_button).set_label("↩ 返回").set_id("pet_open_use_" + uid_s)
          .set_style(dpp::cos_secondary);
        row.add_component(bb); m.add_component(row); return m;
    };

    // 寶珠兌換卷：不需要寵物，提前處理
    if (vi && key == "orb_ticket") {
        if (item_count <= 0) return err("道具數量不足！");
        static const std::vector<std::pair<std::string,std::string>> ALL_ORBS2 = {
            {"EQ_K_UR",         "無名女神的寶珠"},
            {"EQ_K_SPEED",      "迅捷狼王的寶珠"},
            {"EQ_K_ATHENA",     "雅典娜的寶珠"},
            {"EQ_K_BEAR",       "巨山狂熊的寶珠"},
            {"EQ_K_VIKING",     "維京的寶珠"},
            {"EQ_K_WARGOD",     "狂怒戰神的寶珠"},
            {"EQ_K_LATUS",      "拉圖斯的寶珠"},
            {"EQ_K_DARKDRAGON", "暗黑龍王的寶珠"},
        };
        static std::mt19937 orb_rng2(std::random_device{}());
        int idx = std::uniform_int_distribution<int>(0, (int)ALL_ORBS2.size()-1)(orb_rng2);
        auto& [orb_key, orb_name] = ALL_ORBS2[idx];
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            inventory_data[uid]["orb_ticket"]--;
            inventory_data[uid][orb_key]++;
        }
        save_inventory();
        e.set_title("💎  寶珠兌換成功").set_color(0xFFD700);
        e.set_description("💎 獲得了 **✨UR " + orb_name + "**！");
        m.add_embed(e); return m;
    }

    // 特權道具：不需要寵物，提前處理
    if (vi && vi->category == "privilege") {
        if (item_count <= 0) return err("道具數量不足！");
        time_t now_p = time(nullptr);
        std::string result;
        if (key == "vip_daily") {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key]--;
                auto& cd = chip_data[uid];
                cd.vip_until = std::max(cd.vip_until, now_p) + 86400; // stack if used again
            }
            save_inventory(); save_chips();
            e.set_title("👑  尊爵VIP 啟動！").set_color(0xF1C40F);
            e.set_description("✅ 尊爵VIP 已啟動！\n接下來 **24 小時**內，每小時自動為你領取籌碼。");
        } else if (key == "pet_supervisor_daily") {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key]--;
                auto& cd = chip_data[uid];
                cd.supervisor_until = std::max(cd.supervisor_until, now_p) + 86400;
            }
            save_inventory(); save_chips();
            e.set_title("🤖  寵物監工 啟動！").set_color(0x3498DB);
            e.set_description("✅ 寵物監工 已啟動！\n接下來 **24 小時**內，寵物打工結束 **10 分鐘**後若未領取，自動以 **0.6 倍**收益再次出勤。");
        } else if (key == "pet_insurance") {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key]--;
                chip_data[uid].insurance_until = now_p + 3 * 86400;
            }
            save_inventory(); save_chips();
            e.set_title("🏥  醫療保險 啟動！").set_color(0x2ECC71);
            e.set_description("✅ 醫療保險已生效！\n接下來 **三天內**，寵物打工回來若生病（受傷除外），立即給付 **4000** 保險金並結束效果。");
        } else {
            return err("未知的特權道具！");
        }
        m.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component bb;
        bb.set_type(dpp::cot_button).set_label("↩ 返回").set_id("pet_open_use_" + uid_s)
          .set_style(dpp::cos_secondary);
        row.add_component(bb); m.add_component(row);
        return m;
    }

    if (!vi || !has_pet || item_count <= 0) return err("道具不存在或數量不足！");

    static thread_local std::mt19937 rng(std::random_device{}());
    auto roll = [&](int pct) { return std::uniform_int_distribution<int>(1,100)(rng) <= pct; };

    // 天賦：天然呆 — 5% 機率不消耗道具
    bool consume_item = !(pet.talent == "天然呆" && roll(5));

    bool success = false;
    std::string result_desc;

    // ── 蛋（從背包使用）──────────────────────────────────────────────────────
    if (vi->category == "egg") {
        if (has_pet) return err("你已經有寵物了！放生後才能孵新蛋。");
        std::string chain = chain_from_egg_key(key);
        if (chain.empty()) return err("無效的蛋！");
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            inventory_data[uid][key]--;
            Pet p; p.chain = chain; p.stage = 0; p.exp = 0;
            pet_data[uid] = p;
        }
        save_inventory(); save_pet_data();
        e.set_title("🥚  已放入寵物欄").set_color(0x2ECC71);
        e.set_description("**" + (vi ? vi->name : key) + "** 已設為當前寵物蛋！\n使用孵蛋工具來孵化它。");
        m.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component bb;
        bb.set_type(dpp::cot_button).set_label("↩ 返回").set_id("pet_open_use_" + uid_s)
          .set_style(dpp::cos_secondary);
        row.add_component(bb); m.add_component(row); return m;
    }

    // ── 孵蛋工具 ──────────────────────────────────────────────────────────────
    if (vi->category == "incubator") {
        if (pet.stage != 0) return err("寵物已經孵化了！");
        int pct = 0;
        if (key == "inc_100") pct = 100;
        else if (key == "inc_60") pct = 60;
        else if (key == "inc_30") pct = 30;
        else if (key == "inc_10") pct = 10;
        success = roll(pct);
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (consume_item) inventory_data[uid][key]--;
            if (success) { pet_data[uid].stage = 1; pet_data[uid].exp = 0; }
        }
        if (success) {
            Pet updated;
            { std::lock_guard<std::mutex> lk(data_mutex); updated = pet_data[uid]; }
            result_desc = "🎉 孵化成功！**" + pet_name(updated.chain, 1) + "** 誕生了！";
            e.set_title("🎉  孵化成功！").set_color(0x2ECC71);
        } else {
            result_desc = "😢 孵化失敗...蛋還在，可以再試！";
            e.set_title("😢  孵化失敗").set_color(0xE74C3C);
        }
    }
    // ── 成長工具 ──────────────────────────────────────────────────────────────
    else if (vi->category == "growth") {
        if (pet.stage == 0)
            return err("蛋還沒孵化！");
        int exp_gain = 0; bool punished = false;
        if (key == "grow_1") { exp_gain = 10; success = true; }
        else if (key == "grow_2") { success = roll(60); if (success) exp_gain = 15; }
        else if (key == "grow_3") { success = roll(30); if (success) exp_gain = 30; }
        else if (key == "grow_4") { success = roll(10); if (success) exp_gain = 50; }
        else if (key == "grow_5") { success = roll(1);  if (success) exp_gain = 250; }
        else if (key == "grow_6") {
            int r = std::uniform_int_distribution<int>(1,100)(rng);
            if (r <= 50)      { exp_gain = 30; success = true; }
            else if (r <= 75) { exp_gain = 10; success = true; }
            else              { exp_gain = -20; punished = true; }
        }
        int new_exp = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (consume_item) inventory_data[uid][key]--;
            auto& p = pet_data[uid];
            int raw = p.exp + exp_gain;
            if (p.stage > 0 && p.stage < 3)
                p.exp = std::min(std::max(0, raw), exp_needed(p.stage));
            else
                p.exp = std::max(0, raw);
            new_exp = p.exp;
        }
        if (punished)
            result_desc = "😰 壓力太大！-20 經驗值";
        else if (!success)
            result_desc = "😢 成長失敗...沒有獲得經驗值";
        else
            result_desc = "✨ 成長！+" + std::to_string(exp_gain) + " 經驗值";
        result_desc += "\n目前經驗值：**" + std::to_string(new_exp) + "**";
        if (punished) e.set_title("😰  成長懲罰").set_color(0xE74C3C);
        else if (success) e.set_title("✨  成長成功！").set_color(0x2ECC71);
        else e.set_title("😢  成長失敗").set_color(0xE74C3C);
    }
    // ── 天賦道具 ─────────────────────────────────────────────────────────────
    else if (vi->category == "talent") {
        if (pet.stage == 0) return err("蛋還沒孵化，無法賦予天賦！");
        static const std::vector<std::string> TALENTS = {"迅捷", "招人喜歡", "幸運", "天然呆", "喜歡作夢"};
        auto talent_desc = [](const std::string& t) -> std::string {
            if (t == "迅捷")        return "打工時間縮短 10%！";
            if (t == "招人喜歡")    return "打工報酬提升 10%！";
            if (t == "幸運")        return "打工有 5% 機率獲得雙倍報酬！";
            if (t == "天然呆")      return "使用道具時有 5% 機率不消耗道具！";
            if (t == "喜歡作夢")    return "每次打工完有 0.1% 機率將現有籌碼翻倍！";
            return "";
        };
        if (key == "talent_reroll") {
            // Reroll: needs existing talent, picks a different one
            if (pet.talent.empty()) return err("寵物還沒有天賦，請先使用天賦賦予卷軸！");
            std::string old_talent = pet.talent;
            std::vector<std::string> others;
            for (auto& t : TALENTS) if (t != old_talent) others.push_back(t);
            int idx = std::uniform_int_distribution<int>(0, (int)others.size()-1)(rng);
            std::string new_talent = others[idx];
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key]--; // 天然呆不影響天賦道具
                pet_data[uid].talent = new_talent;
            }
            result_desc = "🎨 天賦改變！**" + old_talent + "** → **" + new_talent + "**\n" + talent_desc(new_talent);
            e.set_title("🎨  天賦重置！").set_color(0x3498DB);
            success = true;
        } else {
            if (key == "talent_scroll") {
                // 自選天賦：顯示選單，不立即消耗卷軸（有天賦也可覆蓋）
                dpp::embed se;
                se.set_title("✨  選擇天賦").set_color(0xF39C12);
                std::string sdesc;
                if (!pet.talent.empty())
                    sdesc = "目前天賦：**" + pet.talent + "**\n選擇後將覆蓋現有天賦：\n\n";
                else
                    sdesc = "請選擇要賦予 **" + pet_name(pet.chain, pet.stage, pet.variant) + "** 的天賦：\n\n";
                for (auto& t : TALENTS) sdesc += "・**" + t + "** — " + talent_desc(t) + "\n";
                se.set_description(sdesc);
                dpp::message sm; sm.add_embed(se);
                dpp::component row; row.set_type(dpp::cot_action_row);
                for (auto& t : TALENTS) {
                    dpp::component btn;
                    btn.set_type(dpp::cot_button).set_label(t)
                       .set_id("talent_pick_" + t + "_" + std::to_string((uint64_t)uid))
                       .set_style(dpp::cos_primary);
                    row.add_component(btn);
                }
                sm.add_component(row);
                return sm;
            }
            // talent_class: 25% random（已有天賦則不可使用）
            if (!pet.talent.empty()) return err("寵物已擁有天賦：**" + pet.talent + "**\n如需更換請使用「你不可以學畫畫!」或「天賦賦予卷軸」！");
            success = roll(25);
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key]--; // 天然呆不影響天賦道具
                if (success) {
                    int idx = std::uniform_int_distribution<int>(0, (int)TALENTS.size()-1)(rng);
                    pet_data[uid].talent = TALENTS[idx];
                }
            }
            if (success) {
                Pet updated;
                { std::lock_guard<std::mutex> lk(data_mutex); updated = pet_data[uid]; }
                result_desc = "🌟 發現天賦！**" + updated.talent + "**\n" + talent_desc(updated.talent);
                e.set_title("🌟  天賦覺醒！").set_color(0xF39C12);
            } else {
                result_desc = "😢 這次沒有發現天賦...可以再試！";
                e.set_title("😢  未發現天賦").set_color(0xE74C3C);
            }
        }
    }
    // ── 成長路徑道具（path）── 不可直接使用，提示 ────────────────────────────
    else if (vi->category == "recovery") {
        // Recovery items remove a status from the pet
        static const std::map<std::string,std::string> ITEM_STATUS = {
            {"recover_depress","憂鬱"}, {"recover_injury","受傷"},
            {"recover_muscle","肌肉緊繃"}, {"recover_fatigue","疲勞"},
        };
        if (!ITEM_STATUS.count(key)) return err("無效的恢復道具！");
        std::string target_status = ITEM_STATUS.at(key);
        bool found = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& p = pet_data[uid];
            auto& ss = p.statuses;
            auto it2 = std::find(ss.begin(), ss.end(), target_status);
            if (it2 != ss.end()) {
                ss.erase(it2); found = true;
                if (consume_item) inventory_data[uid][key]--;
            }
        }
        if (!found) return err("你的寵物目前沒有「" + target_status + "」狀態！");
        save_pet_data(); save_inventory();
        e.set_title("💊  恢復成功！").set_color(0x2ECC71);
        e.set_description("解除了「**" + target_status + "**」狀態！");
        m.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component vb;
        vb.set_type(dpp::cot_button).set_label("🐾 查看寵物")
          .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_primary);
        row.add_component(vb); m.add_component(row);
        return m;
    }
    else if (vi->category == "hunt") {
        return err("狩獵卷請透過「!怪物狩獵」指令使用！");
    }
    else if (vi->category == "path") {
        return err("這是成長路徑道具！\n進化一階段時若背包中有對應道具，會自動觸發分支進化並消耗。");
    }
    // ── 進化工具 ──────────────────────────────────────────────────────────────
    else if (vi->category == "evolution") {
        // 退化卡
        if (key == "evo_degrade") {
            if (pet.stage <= 1) return err(pet.stage == 0 ? "蛋無法退化！" : "一階段無法再退化！");
            int new_stage = pet.stage - 1;
            int new_exp   = exp_needed(new_stage); // max exp for new stage
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (consume_item) inventory_data[uid][key]--;
                auto& p = pet_data[uid];
                p.stage   = new_stage;
                p.exp     = new_exp;
                p.variant = ""; // reset branch on demotion
            }
            Pet updated;
            { std::lock_guard<std::mutex> lk(data_mutex); updated = pet_data[uid]; }
            result_desc = "📉 退化成功！**" + pet_name(updated.chain, updated.stage, updated.variant) + "**\n"
                        + "✨ 經驗值：**" + std::to_string(updated.exp) + "**（最大值）";
            e.set_title("📉  退化完成").set_color(0x95A5A6);
            success = true;
        } else {
            // Normal evolution tools
            if (pet.stage == 0 || pet.stage == 3)
                return err(pet.stage == 0 ? "蛋還沒孵化！" : "寵物已是最高階！");
            if ((key == "evo_1" || key == "evo_3") && pet.stage != 1)
                return err("這個道具只能在一階段使用！");
            if ((key == "evo_2" || key == "evo_4") && pet.stage != 2)
                return err("這個道具只能在二階段使用！");
            int need = exp_needed(pet.stage);
            if (pet.exp < need)
                return err("經驗值不足！需要 " + std::to_string(need) + " 點，目前 " + std::to_string(pet.exp) + " 點。");

            int pct = 100;
            bool no_penalty = false;
            if (key == "evo_3" || key == "evo_4") { pct = 70; no_penalty = true; }
            else if (key == "evo_5") pct = 50;
            success = roll(pct);

            // For stage 1 evolution (evo_1/evo_3/evo_5), check for branch items
            std::string branch_consumed;
            std::string new_variant;
            if (success && pet.stage == 1) {
                // Check which branch item is in inventory (priority order)
                auto has_item = [&](const std::string& k) {
                    auto it = inv_snapshot.find(k);
                    return it != inv_snapshot.end() && it->second > 0;
                };
                if (pet.chain == "嫩寶" && has_item("path_moss_shell")) {
                    branch_consumed = "path_moss_shell"; new_variant = "苔蘚";
                } else if (pet.chain == "小企鵝" && has_item("path_penguin_crown")) {
                    branch_consumed = "path_penguin_crown"; new_variant = "企鵝王";
                } else if (pet.chain == "菇菇仔" && has_item("path_curse")) {
                    branch_consumed = "path_curse"; new_variant = "殭屍";
                } else if (pet.chain == "菇菇仔" && has_item("path_blue_dye")) {
                    branch_consumed = "path_blue_dye"; new_variant = "藍菇";
                } else if (pet.chain == "肥肥" && has_item("path_desert")) {
                    branch_consumed = "path_desert"; new_variant = "沙漠";
                }
            }

            std::string new_name;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (consume_item) inventory_data[uid][key]--;
                auto& p = pet_data[uid];
                if (success) {
                    if (!branch_consumed.empty()) {
                        inventory_data[uid][branch_consumed]--;
                        p.variant = new_variant;
                    }
                    p.stage++; p.exp = 0;
                    new_name = pet_name(p.chain, p.stage, p.variant);
                } else if (key == "evo_5") {
                    p.exp = std::max(0, p.exp - 50);
                }
            }
            if (success) {
                result_desc = "🌟 進化成功！**" + new_name + "** 誕生了！";
                if (!branch_consumed.empty()) {
                    auto* bi = find_virtual_item(branch_consumed);
                    result_desc += "\n（消耗了 **" + (bi ? bi->name : branch_consumed) + "** 觸發分支進化）";
                }
                e.set_title("🌟  進化成功！").set_color(0xF39C12);
            } else {
                result_desc = "😢 進化失敗...";
                if (key == "evo_5") result_desc += " -50 經驗值";
                else if (no_penalty) result_desc += "（碎片失敗，沒有懲罰）";
                e.set_title("😢  進化失敗").set_color(0xE74C3C);
            }
        }
    } else {
        return err("無法使用此道具！");
    }

    save_pet_data(); save_inventory();

    Pet updated;
    { std::lock_guard<std::mutex> lk(data_mutex); auto it = pet_data.find(uid); if (it != pet_data.end()) updated = it->second; }
    result_desc += "\n🐾 **" + pet_name(updated.chain, updated.stage, updated.variant) + "**";
    if (updated.stage > 0)
        result_desc += "　✨ 經驗值 " + std::to_string(updated.exp);
    e.set_description(result_desc);
    m.add_embed(e);

    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component more_btn, view_btn;
    more_btn.set_type(dpp::cot_button).set_label("🎒 返回背包")
            .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_primary);
    view_btn.set_type(dpp::cot_button).set_label("🐾 查看寵物")
            .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(more_btn); row.add_component(view_btn);
    m.add_component(row);
    return m;
}

// ─── Discard: mode view ───────────────────────────────────────────────────────

static dpp::message make_pet_discard_mode_msg(dpp::snowflake uid) {
    std::map<std::string,int> inv;
    bool has_pet = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        has_pet = pet_data.count(uid) > 0;
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) inv = ii->second;
    }
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::embed e; e.set_title("🗑️  選擇要丟棄的道具").set_color(0xE67E22);
    dpp::message msg;

    if (!has_pet) {
        e.set_description("你還沒有寵物！");
        msg.add_embed(e); return msg;
    }

    struct ItemEntry { std::string key; int count; };
    std::vector<ItemEntry> entries;
    for (auto& vi : VIRTUAL_ITEMS) {
        if (vi.category == "egg") continue;
        auto it = inv.find(vi.key);
        if (it != inv.end() && it->second > 0)
            entries.push_back({vi.key, it->second});
    }

    if (entries.empty()) {
        e.set_description("背包是空的，沒有可以丟棄的道具。");
        msg.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component back;
        back.set_type(dpp::cot_button).set_label("↩ 返回背包")
            .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary);
        row.add_component(back); msg.add_component(row);
        return msg;
    }

    e.set_description("點選道具丟棄 **1 個**，操作前會再次確認。");
    msg.add_embed(e);

    dpp::component cur_row; cur_row.set_type(dpp::cot_action_row);
    int n = 0;
    for (auto& en : entries) {
        if (n >= 20) break;
        auto* vi = find_virtual_item(en.key);
        if (!vi) continue;
        if (n > 0 && n % 5 == 0) {
            msg.add_component(cur_row);
            cur_row = dpp::component(); cur_row.set_type(dpp::cot_action_row);
        }
        dpp::component btn;
        btn.set_type(dpp::cot_button)
           .set_label("🗑️ " + vi->name + " ×" + std::to_string(en.count))
           .set_id("pet_discard_confirm_" + uid_s + "_" + en.key)
           .set_style(dpp::cos_danger);
        cur_row.add_component(btn); n++;
    }
    if (n > 0) msg.add_component(cur_row);

    dpp::component back_row; back_row.set_type(dpp::cot_action_row);
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回背包")
        .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary);
    back_row.add_component(back); msg.add_component(back_row);
    return msg;
}

// ─── Discard: confirmation view ───────────────────────────────────────────────

static dpp::message make_pet_discard_confirm_msg(dpp::snowflake uid, const std::string& key) {
    std::string uid_s = std::to_string((uint64_t)uid);
    const VirtualShopItem* vi = find_virtual_item(key);
    dpp::embed e; dpp::message msg;
    e.set_title("⚠️  確認丟棄").set_color(0xE74C3C);
    if (!vi) {
        e.set_description("道具不存在！");
        msg.add_embed(e); return msg;
    }
    e.set_description("確定要丟棄 **1 個** **" + vi->name + "** 嗎？\n此操作無法復原！");
    msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component yes, no;
    yes.set_type(dpp::cot_button).set_label("✅ 確認丟棄")
       .set_id("pet_discard_do_" + uid_s + "_" + key).set_style(dpp::cos_danger);
    no.set_type(dpp::cot_button).set_label("❌ 取消")
       .set_id("pet_discard_mode_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(yes); row.add_component(no);
    msg.add_component(row);
    return msg;
}

// ─── Discard: execute ─────────────────────────────────────────────────────────

static dpp::message handle_pet_discard_item(dpp::snowflake uid, const std::string& key) {
    std::string uid_s = std::to_string((uint64_t)uid);
    const VirtualShopItem* vi = find_virtual_item(key);
    dpp::embed e; dpp::message msg;
    if (!vi) {
        e.set_title("❌  錯誤").set_color(0xE74C3C);
        e.set_description("道具不存在！");
        msg.add_embed(e); return msg;
    }
    bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& inv = inventory_data[uid];
        if (inv.count(key) && inv[key] > 0) { inv[key]--; ok = true; }
    }
    if (!ok) {
        e.set_title("❌  數量不足").set_color(0xE74C3C);
        e.set_description("你沒有 **" + vi->name + "**！");
        msg.add_embed(e); return msg;
    }
    save_inventory();
    e.set_title("🗑️  已丟棄").set_color(0x95A5A6);
    e.set_description("丟棄了 **1 個** **" + vi->name + "**。");
    msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component back_bag, back_pet;
    back_bag.set_type(dpp::cot_button).set_label("🎒 返回背包")
            .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary);
    back_pet.set_type(dpp::cot_button).set_label("🐾 查看寵物")
            .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(back_bag); row.add_component(back_pet);
    msg.add_component(row);
    return msg;
}

// ─── Work handlers ────────────────────────────────────────────────────────────

static dpp::message handle_pet_work_start(dpp::snowflake uid, int task) {
    Pet pet; bool has_pet = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end()) { pet = it->second; has_pet = true; }
    }
    dpp::embed e; dpp::message m;
    if (!has_pet || pet.stage == 0) {
        e.set_title("❌  無法打工").set_color(0xE74C3C);
        e.set_description("你沒有可以打工的寵物！");
        m.add_embed(e); return m;
    }
    if (pet.work_task > 0) {
        e.set_title("❌  已在打工").set_color(0xE74C3C);
        e.set_description("你的寵物已經在打工中！");
        m.add_embed(e); return m;
    }
    int dur = (task == 1) ? 3600 : (task == 4) ? 14400 : 28800;
    // 天賦：迅捷 — 打工時間縮短 10%
    if (pet.talent == "迅捷") dur = (int)(dur * 0.9);
    // 負面狀態：疲勞 — 打工時長 +30%
    for (auto& s : pet.statuses) if (s == "疲勞") { dur = (int)(dur * 1.3); break; }
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        pet_data[uid].work_task          = task;
        pet_data[uid].work_end           = time(nullptr) + dur;
        pet_data[uid].work_notified      = false;
        pet_data[uid].is_supervisor_work = false;
    }
    save_pet_data();
    return make_pet_view_msg(uid);
}

static dpp::message handle_pet_work_claim(dpp::snowflake uid) {
    Pet pet; bool has_pet = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end()) { pet = it->second; has_pet = true; }
    }
    dpp::embed e; dpp::message m;
    if (!has_pet || pet.stage == 0 || pet.work_task == 0) {
        e.set_title("❌  沒有可領取的獎勵").set_color(0xE74C3C);
        m.add_embed(e); return m;
    }
    if (pet.work_end > time(nullptr)) {
        e.set_title("❌  打工尚未完成").set_color(0xE74C3C);
        e.set_description("請稍後再試！");
        m.add_embed(e); return m;
    }

    auto opts = work_options(pet.stage);
    int idx = (pet.work_task == 1) ? 0 : (pet.work_task == 4) ? 1 : 2;
    int64_t reward   = opts[idx].pay;
    int     exp_gain = opts[idx].exp_gain;

    // 監工派出的打工收益 ×0.6
    bool is_supervisor = pet.is_supervisor_work;
    if (is_supervisor) reward = (int64_t)(reward * 0.6);

    static thread_local std::mt19937 claim_rng(std::random_device{}());
    auto roll_pct = [&](int pct) { return std::uniform_int_distribution<int>(1,1000)(claim_rng) <= pct*10; };

    // 負面狀態：受傷 — 報酬 -10%
    bool status_injured = false, status_depress = false;
    for (auto& s : pet.statuses) {
        if (s == "受傷")  status_injured  = true;
        if (s == "憂鬱")  status_depress  = true;
    }
    if (status_injured) reward = (int64_t)(reward * 0.9);
    // 負面狀態：憂鬱 — 報酬 -20%
    if (status_depress) reward = (int64_t)(reward * 0.8);

    // 天賦：招人喜歡 — 報酬 +10%
    bool doubled_lucky = false;
    if (pet.talent == "招人喜歡") reward = (int64_t)(reward * 1.1);
    // 天賦：幸運 — 5% 雙倍報酬
    if (pet.talent == "幸運" && roll_pct(5)) { reward *= 2; doubled_lucky = true; }

    // 負面狀態：憂鬱 — 有機率隨機花錢 (扣 30~60% 報酬)
    bool depress_spend = false;
    if (status_depress && roll_pct(30)) {
        int64_t lost = (int64_t)(reward * (0.3 + std::uniform_real_distribution<double>(0,0.3)(claim_rng)));
        reward = std::max(0LL, reward - lost);
        depress_spend = true;
    }

    add_chips(uid, reward);

    // 天賦：喜歡作夢 — 0.1% 翻倍現有籌碼
    bool dream_triggered = false;
    if (pet.talent == "喜歡作夢" && std::uniform_int_distribution<int>(1,1000)(claim_rng) == 1) {
        int64_t cur = get_chips(uid);
        add_chips(uid, cur); // double = add same amount again
        dream_triggered = true;
    }

    // Chance of random negative status from work
    static const std::vector<std::string> NEG_STATUS = {"憂鬱","肌肉緊繃","疲勞"};
    int neg_chance = (pet.work_task == 1) ? 1 : (pet.work_task == 4) ? 2 : 5;
    std::string new_neg_status;
    if (roll_pct(neg_chance)) {
        int si = std::uniform_int_distribution<int>(0,2)(claim_rng);
        new_neg_status = NEG_STATUS[si];
    }

    // 醫療保險：打工回來生病時觸發（受傷不算，但工作負面狀態都不含受傷）
    int64_t insurance_payout = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& p = pet_data[uid];
        if (p.stage > 0) {
            if (p.stage < 3)
                p.exp = std::min(p.exp + exp_gain, exp_needed(p.stage));
            else
                p.exp += exp_gain; // stage 3: no cap
        }
        p.work_task          = 0;
        p.work_end           = 0;
        p.is_supervisor_work = false;
        p.work_notified      = false;
        if (!new_neg_status.empty()) {
            bool already = false;
            for (auto& s : p.statuses) if (s == new_neg_status) { already = true; break; }
            if (!already) p.statuses.push_back(new_neg_status);
        }
        auto& cd = chip_data[uid];
        if (!new_neg_status.empty() && cd.insurance_until > time(nullptr)) {
            insurance_payout = 4000;
            cd.chips += insurance_payout;
            cd.insurance_until = 0;
        }
    }
    save_pet_data();
    if (insurance_payout > 0) save_chips();

    e.set_title("💰  打工完成！").set_color(0x2ECC71);
    std::string pet_disp = pet_name(pet.chain, pet.stage, pet.variant);
    if (!pet.talent.empty()) pet_disp += " ✦" + pet.talent;
    e.set_description("**" + pet_disp + "** 打工回來了！");
    std::string reward_str = "+" + std::to_string(reward) + " 碼";
    if (is_supervisor)   reward_str += " 🤖（監工派出 ×0.6）";
    if (doubled_lucky)   reward_str += " 🍀（幸運雙倍！）";
    if (status_injured)  reward_str += " ⚠️（受傷 -10%）";
    if (status_depress)  reward_str += " ⚠️（憂鬱 -20%）";
    if (depress_spend)   reward_str += " 💸（憂鬱花錢了）";
    e.add_field("💰  獎勵",   reward_str, true);
    e.add_field("✨  經驗",   "+" + std::to_string(exp_gain) + " exp", true);
    e.add_field("💼  餘額",   std::to_string(get_chips(uid)) + " 碼", true);
    if (dream_triggered)      e.add_field("🌙  喜歡作夢", "🎆 籌碼翻倍！！", false);
    if (!new_neg_status.empty()) e.add_field("⚠️  新增狀態", "「**" + new_neg_status + "**」", false);
    if (insurance_payout > 0) e.add_field("🏥  醫療保險", "+4000 碼 保險金理賠！效果已結束。", false);
    m.add_embed(e);

    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component vb;
    vb.set_type(dpp::cot_button).set_label("🐾 查看寵物")
      .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_primary);
    row.add_component(vb); m.add_component(row);
    return m;
}

// ─── Cancel work ─────────────────────────────────────────────────────────────

static dpp::message handle_pet_cancel_work(dpp::snowflake uid) {
    dpp::embed e; dpp::message m;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it == pet_data.end() || it->second.work_task == 0) {
            e.set_title("❌  沒有進行中的打工").set_color(0xE74C3C);
            m.add_embed(e); return m;
        }
        auto& p = it->second;
        p.work_task = 0; p.work_end = 0; p.is_supervisor_work = false; p.work_notified = false;
    }
    save_pet_data();
    e.set_title("✅  已取消打工").set_color(0x95A5A6);
    e.set_description("寵物已立即返回，本次打工無獎勵。");
    m.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component vb;
    vb.set_type(dpp::cot_button).set_label("🐾 查看寵物")
      .set_id("pet_refresh_" + std::to_string((uint64_t)uid)).set_style(dpp::cos_primary);
    row.add_component(vb); m.add_component(row);
    return m;
}

// ─── Start onsen ──────────────────────────────────────────────────────────────

static dpp::message handle_pet_start_onsen(dpp::snowflake uid) {
    dpp::embed e; dpp::message m;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it == pet_data.end() || it->second.stage == 0) {
            e.set_title("❌  沒有寵物").set_color(0xE74C3C);
            m.add_embed(e); return m;
        }
        auto& p = it->second;
        if (p.statuses.empty()) {
            e.set_title("❌  寵物沒有負面狀態").set_color(0xE74C3C);
            e.set_description("泡溫泉的用途是清除負面狀態，但你的寵物目前狀態很好！");
            m.add_embed(e); return m;
        }
        if (p.work_task > 0 && p.work_end > time(nullptr)) {
            e.set_title("❌  打工中無法泡溫泉").set_color(0xE74C3C);
            e.set_description("寵物正在打工，請先取消或等打工結束。");
            m.add_embed(e); return m;
        }
        p.onsen_end      = time(nullptr) + 7200; // 2 hours
        p.onsen_notified = false;
        p.work_task = 0; p.work_end = 0; p.is_supervisor_work = false;
    }
    save_pet_data();
    e.set_title("🛀  已開始泡溫泉！").set_color(0x3498DB);
    e.set_description("寵物將在 **2 小時**後療癒完畢，自動清除所有負面狀態。");
    m.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component vb;
    vb.set_type(dpp::cot_button).set_label("🐾 查看寵物")
      .set_id("pet_refresh_" + std::to_string((uint64_t)uid)).set_style(dpp::cos_primary);
    row.add_component(vb); m.add_component(row);
    return m;
}

// ─── Cancel onsen ─────────────────────────────────────────────────────────────

static dpp::message handle_pet_cancel_onsen(dpp::snowflake uid) {
    dpp::embed e; dpp::message m;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it == pet_data.end() || it->second.onsen_end == 0) {
            e.set_title("❌  沒有進行中的溫泉").set_color(0xE74C3C);
            m.add_embed(e); return m;
        }
        it->second.onsen_end      = 0;
        it->second.onsen_notified = false;
    }
    save_pet_data();
    e.set_title("✅  已取消泡溫泉").set_color(0x95A5A6);
    e.set_description("寵物已立即離開溫泉，負面狀態未清除。");
    m.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component vb;
    vb.set_type(dpp::cot_button).set_label("🐾 查看寵物")
      .set_id("pet_refresh_" + std::to_string((uint64_t)uid)).set_style(dpp::cos_primary);
    row.add_component(vb); m.add_component(row);
    return m;
}

// ─── Pet dex ─────────────────────────────────────────────────────────────────

struct PetDexEntry {
    std::string chain;
    std::string variant; // "" = standard
    int stage;
    std::string path_item; // item needed for this branch ("" = standard)
};

static const std::vector<PetDexEntry> PET_DEX = {
    // 嫩寶 standard chain
    {"嫩寶", "",    0, ""},
    {"嫩寶", "",    1, ""},
    {"嫩寶", "",    2, ""},
    {"嫩寶", "",    3, ""},
    // 嫩寶 苔蘚 branch (diverges at stage 2)
    {"嫩寶", "苔蘚", 2, "path_moss_shell"},
    {"嫩寶", "苔蘚", 3, "path_moss_shell"},
    // 菇菇仔 standard chain
    {"菇菇仔", "",    0, ""},
    {"菇菇仔", "",    1, ""},
    {"菇菇仔", "",    2, ""},
    {"菇菇仔", "",    3, ""},
    // 菇菇仔 殭屍 branch
    {"菇菇仔", "殭屍", 2, "path_curse"},
    {"菇菇仔", "殭屍", 3, "path_curse"},
    // 菇菇仔 藍菇 branch
    {"菇菇仔", "藍菇", 2, "path_blue_dye"},
    {"菇菇仔", "藍菇", 3, "path_blue_dye"},
    // 肥肥 standard chain
    {"肥肥", "",    0, ""},
    {"肥肥", "",    1, ""},
    {"肥肥", "",    2, ""},
    {"肥肥", "",    3, ""},
    // 肥肥 沙漠 branch
    {"肥肥", "沙漠", 2, "path_desert"},
    {"肥肥", "沙漠", 3, "path_desert"},
    // 小企鵝 standard chain
    {"小企鵝", "",    0, ""},
    {"小企鵝", "",    1, ""},
    {"小企鵝", "",    2, ""},
    {"小企鵝", "",    3, ""},
    // 小企鵝 企鵝王 branch
    {"小企鵝", "企鵝王", 2, "path_penguin_crown"},
    {"小企鵝", "企鵝王", 3, "path_penguin_crown"},
};

static dpp::message make_petdex_msg(const std::string& chain = "嫩寶") {
    dpp::embed e;
    e.set_title("📖  寵物圖鑑 — " + chain).set_color(0x9B59B6);

    // Collect entries for this chain
    struct Branch {
        std::string variant;
        std::string path_item;
    };
    // Get unique variants for this chain
    std::vector<Branch> branches;
    {
        std::set<std::string> seen;
        for (auto& entry : PET_DEX) {
            if (entry.chain != chain) continue;
            if (seen.count(entry.variant)) continue;
            seen.insert(entry.variant);
            Branch b;
            b.variant   = entry.variant;
            b.path_item = entry.path_item;
            branches.push_back(b);
        }
    }

    std::string desc;
    for (auto& br : branches) {
        std::string branch_label = br.variant.empty() ? "★ 標準路線" : ("⬡ 分支：" + br.variant);
        if (!br.path_item.empty()) {
            auto* vi = find_virtual_item(br.path_item);
            branch_label += "（需持有 **" + (vi ? vi->name : br.path_item) + "**）";
        }
        desc += "**" + branch_label + "**\n";

        // Stage 0-3 for this branch
        for (int s = 0; s <= 3; s++) {
            // Find if this branch has this stage
            bool has = false;
            for (auto& entry : PET_DEX)
                if (entry.chain == chain && entry.variant == br.variant && entry.stage == s) { has = true; break; }
            if (!has) {
                // For branches that diverge at stage 2, show standard names for stages 0-1
                if (s < 2 && !br.variant.empty()) {
                    std::string sname = pet_name(chain, s, "");
                    desc += "  **第 " + std::to_string(s) + " 階**　" + sname + "\n";
                }
                continue;
            }
            std::string sname = pet_name(chain, s, br.variant);
            std::string arrow = (s < 3) ? " →" : "";
            std::string stage_label;
            if (s == 0) stage_label = "🥚 蛋";
            else if (s == 1) stage_label = "第 1 階";
            else if (s == 2) stage_label = "第 2 階";
            else stage_label = "第 3 階（最終）";
            desc += "  **" + stage_label + "**　**" + sname + "**" + arrow + "\n";
        }
        desc += "\n";
    }

    // Work rewards table
    desc += "**打工報酬（依階段）：**\n";
    static const char* labels[] = {"1小時", "4小時", "8小時"};
    for (int s = 1; s <= 3; s++) {
        auto opts = work_options(s);
        desc += "第 " + std::to_string(s) + " 階：";
        for (int i = 0; i < 3; i++)
            desc += std::string(labels[i]) + " +" + std::to_string(opts[i].pay) + "碼/+" + std::to_string(opts[i].exp_gain) + "exp　";
        desc += "\n";
    }
    desc += "\n**進化所需經驗：** 1→2階 **100 exp**、2→3階 **250 exp**";

    e.set_description(desc);

    // Show image for stage 1 standard of selected chain
    std::string img = pet_image_url(chain, 1, "");
    if (!img.empty()) e.set_thumbnail(img);

    dpp::message msg; msg.add_embed(e);

    // Dropdown to select chain
    dpp::component sel_row; sel_row.set_type(dpp::cot_action_row);
    dpp::component sel; sel.set_type(dpp::cot_selectmenu)
        .set_id("petdex_chain_sel")
        .set_placeholder("選擇寵物系列");
    sel.add_select_option(dpp::select_option("嫩寶",    "嫩寶",   "嫩寶 / 紅寶 / 苔蘚系列").set_default(chain == "嫩寶"));
    sel.add_select_option(dpp::select_option("菇菇仔",  "菇菇仔", "菇菇仔 / 殭屍 / 藍菇系列").set_default(chain == "菇菇仔"));
    sel.add_select_option(dpp::select_option("肥肥",    "肥肥",   "肥肥 / 沙漠肥肥系列").set_default(chain == "肥肥"));
    sel.add_select_option(dpp::select_option("小企鵝",  "小企鵝", "小企鵝 / 企鵝王系列").set_default(chain == "小企鵝"));
    sel_row.add_component(sel);
    msg.add_component(sel_row);
    return msg;
}

// ─── Item compendium ──────────────────────────────────────────────────────────

static dpp::component make_itemdex_menu(dpp::snowflake uid, const std::string& placeholder) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::component menu;
    menu.set_type(dpp::cot_selectmenu).set_id("itemdex_cat_" + uid_s)
        .set_placeholder(placeholder);
    menu.add_select_option(dpp::select_option("🥚 寵物蛋",    "egg",       "各種寵物蛋"));
    menu.add_select_option(dpp::select_option("🔥 孵蛋工具",  "incubator", "用於孵化寵物蛋"));
    menu.add_select_option(dpp::select_option("🌱 成長工具",  "growth",    "讓寵物獲得經驗值"));
    menu.add_select_option(dpp::select_option("⚡ 進化工具",  "evolution", "讓寵物進化"));
    menu.add_select_option(dpp::select_option("🌿 成長路徑",  "path",      "開啟分支進化路線"));
    menu.add_select_option(dpp::select_option("✨ 天賦道具",  "talent",    "賦予或重抽天賦"));
    menu.add_select_option(dpp::select_option("📜 狩獵道具",  "hunt",      "怪物狩獵卷"));
    menu.add_select_option(dpp::select_option("💊 恢復道具",  "recovery",  "解除負面狀態"));
    menu.add_select_option(dpp::select_option("💎 寶珠碎片",  "shard",     "合成寶珠所需的碎片"));
    menu.add_select_option(dpp::select_option("🌟 特殊道具",  "special",   "特殊效果道具"));
    return menu;
}

static dpp::message make_itemdex_main_msg(dpp::snowflake uid) {
    dpp::embed e;
    e.set_title("🎒  道具圖鑑").set_color(0x3498DB);
    e.set_description("選擇分類查看所有道具資訊。");
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(make_itemdex_menu(uid, "選擇分類"));
    msg.add_component(row);
    return msg;
}

static dpp::message make_itemdex_cat_msg(dpp::snowflake uid, const std::string& cat) {
    static const std::map<std::string,std::string> CAT_TITLES = {
        {"egg","🥚 寵物蛋"},{"incubator","🔥 孵蛋工具"},{"growth","🌱 成長工具"},
        {"evolution","⚡ 進化工具"},{"path","🌿 成長路徑"},{"talent","✨ 天賦道具"},
        {"hunt","📜 狩獵道具"},{"recovery","💊 恢復道具"},{"shard","💎 寶珠碎片"},{"special","🌟 特殊道具"}
    };
    std::string title = CAT_TITLES.count(cat) ? CAT_TITLES.at(cat) : cat;
    dpp::embed e;
    e.set_title("🎒  道具圖鑑 — " + title).set_color(0x3498DB);
    std::string desc;
    for (auto& vi : VIRTUAL_ITEMS) {
        if (vi.category != cat) continue;
        std::string id_s = vi.item_id ? ("`#" + std::to_string(vi.item_id) + "`  ") : "";
        std::string price_s = vi.price > 0 ? ("  💰" + std::to_string(vi.price) + "碼") : "";
        desc += id_s + "**" + vi.name + "**" + price_s + "\n" + vi.desc + "\n\n";
    }
    if (desc.empty()) desc = "（此分類暫無道具）";
    e.set_description(desc);
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(make_itemdex_menu(uid, "切換分類"));
    msg.add_component(row);
    return msg;
}

