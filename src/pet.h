#pragma once
#include "chips.h"
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
    {"star_unknown", "未知的星星", 0, "special", "不知道有什麼用的星星，可能在未來某一天會用到", 70001},
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
static const std::string INVENTORY_FILE = "inventory.json";

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
            p.variant     = v.value("variant",      std::string{});
            p.custom_name = v.value("custom_name",  std::string{});
            p.talent      = v.value("talent",       std::string{});
        }
    } catch (...) {}
}

static void save_pet_data() {
    nlohmann::json j;
    std::lock_guard<std::mutex> lk(data_mutex);
    for (auto& [uid, p] : pet_data)
        j[std::to_string((uint64_t)uid)] = {
            {"chain", p.chain}, {"stage", p.stage}, {"exp", p.exp},
            {"work_task", p.work_task}, {"work_end", (int64_t)p.work_end},
            {"variant", p.variant}, {"custom_name", p.custom_name},
            {"talent", p.talent}
        };
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

static void save_inventory() {
    nlohmann::json j;
    std::lock_guard<std::mutex> lk(data_mutex);
    for (auto& [uid, inv] : inventory_data) {
        nlohmann::json inv_j;
        for (auto& [key, cnt] : inv) if (cnt > 0) inv_j[key] = cnt;
        if (!inv_j.empty()) j[std::to_string((uint64_t)uid)] = inv_j;
    }
    atomic_write(INVENTORY_FILE, j.dump(2));
}

// ─── Pet view ─────────────────────────────────────────────────────────────────

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
        dpp::component btn, rel0;
        btn.set_type(dpp::cot_button).set_label("🎒 背包")
           .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_primary);
        rel0.set_type(dpp::cot_button).set_label("🕊️ 放生")
            .set_id("pet_release_" + uid_s).set_style(dpp::cos_danger);
        row.add_component(btn); row.add_component(rel0);
        msg.add_component(row);
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
    e.add_field("✦  天賦", pet.talent.empty() ? "無" : pet.talent, true);

    time_t now = time(nullptr);
    bool working   = (pet.work_task > 0 && pet.work_end > now);
    bool work_done = (pet.work_task > 0 && pet.work_end <= now);

    if (working) {
        int remain = (int)(pet.work_end - now);
        int h = remain/3600, m = (remain%3600)/60, s = remain%60;
        char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
        e.add_field("💼  打工狀態", "⏳ 打工中，剩餘 " + std::string(buf), false);
    } else if (work_done) {
        e.add_field("💼  打工狀態", "✅ 打工完成！按下按鈕領取獎勵", false);
    } else {
        auto opts = work_options(pet.stage);
        e.add_field("💼  打工狀態", "😴 閒置", false);
        msg.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        auto mk = [&](const std::string& lbl, const std::string& id, dpp::component_style sty) {
            dpp::component b;
            b.set_type(dpp::cot_button).set_label(lbl).set_id(id).set_style(sty);
            row.add_component(b);
        };
        mk("🏃 1小時 (+" + std::to_string(opts[0].pay) + "碼/+" + std::to_string(opts[0].exp_gain) + "exp)",
           "pet_work_" + uid_s + "_1", dpp::cos_primary);
        mk("🏋 4小時 (+" + std::to_string(opts[1].pay) + "碼/+" + std::to_string(opts[1].exp_gain) + "exp)",
           "pet_work_" + uid_s + "_4", dpp::cos_primary);
        mk("🌙 8小時 (+" + std::to_string(opts[2].pay) + "碼/+" + std::to_string(opts[2].exp_gain) + "exp)",
           "pet_work_" + uid_s + "_8", dpp::cos_primary);
        msg.add_component(row);
        dpp::component row2; row2.set_type(dpp::cot_action_row);
        dpp::component ub, rb, rel2;
        ub.set_type(dpp::cot_button).set_label("🎒 背包")
          .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary);
        rb.set_type(dpp::cot_button).set_label("✏️ 改名")
          .set_id("pet_rename_" + uid_s).set_style(dpp::cos_secondary);
        rel2.set_type(dpp::cot_button).set_label("🕊️ 放生")
            .set_id("pet_release_" + uid_s).set_style(dpp::cos_danger);
        row2.add_component(ub); row2.add_component(rb); row2.add_component(rel2);
        msg.add_component(row2);
        if (pet.stage == 3) {
            dpp::component refrow; refrow.set_type(dpp::cot_action_row);
            dpp::component rfb;
            rfb.set_type(dpp::cot_button).set_label("✨ 提煉星星（-50 exp）")
               .set_id("pet_refine_star_" + uid_s).set_style(dpp::cos_primary)
               .set_disabled(pet.exp < 50);
            refrow.add_component(rfb); msg.add_component(refrow);
        }
        return msg;
    }

    msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    if (work_done) {
        dpp::component cb;
        cb.set_type(dpp::cot_button).set_label("💰 領取打工獎勵")
          .set_id("pet_claim_" + uid_s).set_style(dpp::cos_success);
        row.add_component(cb);
    } else {
        dpp::component rb;
        rb.set_type(dpp::cot_button).set_label("🔄 重新整理")
          .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary);
        row.add_component(rb);
    }
    dpp::component ub, rn, rel;
    ub.set_type(dpp::cot_button).set_label("🎒 背包")
      .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary);
    rn.set_type(dpp::cot_button).set_label("✏️ 改名")
      .set_id("pet_rename_" + uid_s).set_style(dpp::cos_secondary);
    rel.set_type(dpp::cot_button).set_label("🕊️ 放生")
       .set_id("pet_release_" + uid_s).set_style(dpp::cos_danger);
    row.add_component(ub); row.add_component(rn); row.add_component(rel);
    msg.add_component(row);
    if (pet.stage == 3) {
        dpp::component refrow; refrow.set_type(dpp::cot_action_row);
        dpp::component rfb;
        rfb.set_type(dpp::cot_button).set_label("✨ 提煉星星（-50 exp）")
           .set_id("pet_refine_star_" + uid_s).set_style(dpp::cos_primary)
           .set_disabled(pet.exp < 50);
        refrow.add_component(rfb); msg.add_component(refrow);
    }
    return msg;
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
    back.set_type(dpp::cot_button).set_label("↩ 返回寵物狀態")
        .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary);
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

// ─── Pet use items view ───────────────────────────────────────────────────────

static dpp::message make_pet_use_msg(dpp::snowflake uid) {
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
    dpp::embed e; e.set_title("🎒  背包").set_color(0x3498DB);
    dpp::message msg;

    struct ItemEntry { std::string key; int count; };
    std::vector<ItemEntry> entries;
    for (auto& vi : VIRTUAL_ITEMS) {
        if (vi.category == "egg") continue;
        auto it = inv.find(vi.key);
        if (it != inv.end() && it->second > 0)
            entries.push_back({vi.key, it->second});
    }

    if (entries.empty()) {
        e.set_description("背包是空的！\n前往 **商店 → 虛擬商店** 購買道具。");
        msg.add_embed(e);
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component back_btn;
        back_btn.set_type(dpp::cot_button).set_label("↩ 返回寵物狀態")
                .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary);
        row.add_component(back_btn); msg.add_component(row);
        return msg;
    }

    std::string desc;
    for (auto& en : entries) {
        auto* vi = find_virtual_item(en.key);
        if (!vi) continue;
        std::string id_str = vi->item_id ? ("`" + std::to_string(vi->item_id) + "`") : "";
        desc += id_str + " **" + vi->name + "** ×" + std::to_string(en.count) + "　" + vi->desc + "\n";
    }
    e.set_description(desc);
    msg.add_embed(e);

    auto is_disabled = [&](const std::string& key) -> bool {
        if (!has_pet) return true;
        auto* vi = find_virtual_item(key);
        if (!vi) return true;
        if (vi->category == "incubator") return pet.stage != 0;
        if (vi->category == "growth")    return (pet.stage == 0 || pet.stage == 3);
        if (vi->category == "talent") {
            if (key == "talent_reroll") return (pet.stage == 0 || pet.talent.empty()); // reroll: needs existing talent
            return (pet.stage == 0 || !pet.talent.empty()); // scroll/class: disabled if already has talent
        }
        if (vi->category == "path")      return false; // path items just sit in inventory
        if (vi->category == "special")   return true;  // collectibles, no use action
        if (vi->category == "evolution") {
            if (key == "evo_degrade") return (pet.stage <= 1); // can't degrade egg or stage 1
            if (pet.stage == 0 || pet.stage == 3) return true;
            if ((key == "evo_1" || key == "evo_3") && pet.stage != 1) return true;
            if ((key == "evo_2" || key == "evo_4") && pet.stage != 2) return true;
            if (key == "evo_5") return (pet.stage == 0 || pet.stage == 3);
            int need = exp_needed(pet.stage);
            if (pet.exp < need) return true;
        }
        return false;
    };

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

    dpp::component back_row; back_row.set_type(dpp::cot_action_row);
    dpp::component back_btn, discard_btn;
    back_btn.set_type(dpp::cot_button).set_label("↩ 返回寵物狀態")
            .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary);
    discard_btn.set_type(dpp::cot_button).set_label("🗑️ 丟棄道具")
               .set_id("pet_discard_mode_" + uid_s).set_style(dpp::cos_danger);
    back_row.add_component(back_btn); back_row.add_component(discard_btn);
    msg.add_component(back_row);
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

    if (!vi || !has_pet || item_count <= 0) return err("道具不存在或數量不足！");

    static thread_local std::mt19937 rng(std::random_device{}());
    auto roll = [&](int pct) { return std::uniform_int_distribution<int>(1,100)(rng) <= pct; };

    // 天賦：天然呆 — 5% 機率不消耗道具
    bool consume_item = !(pet.talent == "天然呆" && roll(5));

    bool success = false;
    std::string result_desc;

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
        if (pet.stage == 0 || pet.stage == 3)
            return err(pet.stage == 0 ? "蛋還沒孵化！" : "寵物已是最高階！");
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
            if (!pet.talent.empty()) return err("寵物已擁有天賦：**" + pet.talent + "**\n每隻寵物只能有一個天賦！");
            if (key == "talent_scroll") {
                // 自選天賦：顯示選單，不立即消耗卷軸
                dpp::embed se;
                se.set_title("✨  選擇天賦").set_color(0xF39C12);
                std::string sdesc = "請選擇要賦予 **" + pet_name(pet.chain, pet.stage, pet.variant) + "** 的天賦：\n\n";
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
            // talent_class: 25% random
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
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        pet_data[uid].work_task = task;
        pet_data[uid].work_end  = time(nullptr) + dur;
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

    static thread_local std::mt19937 claim_rng(std::random_device{}());
    auto roll_pct = [&](int pct) { return std::uniform_int_distribution<int>(1,1000)(claim_rng) <= pct*10; };

    // 天賦：招人喜歡 — 報酬 +10%
    bool doubled_lucky = false;
    if (pet.talent == "招人喜歡") reward = (int64_t)(reward * 1.1);
    // 天賦：幸運 — 5% 雙倍報酬
    if (pet.talent == "幸運" && roll_pct(5)) { reward *= 2; doubled_lucky = true; }

    add_chips(uid, reward);

    // 天賦：喜歡作夢 — 0.1% 翻倍現有籌碼
    bool dream_triggered = false;
    if (pet.talent == "喜歡作夢" && std::uniform_int_distribution<int>(1,1000)(claim_rng) == 1) {
        int64_t cur = get_chips(uid);
        add_chips(uid, cur); // double = add same amount again
        dream_triggered = true;
    }

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& p = pet_data[uid];
        if (p.stage > 0) {
            if (p.stage < 3)
                p.exp = std::min(p.exp + exp_gain, exp_needed(p.stage));
            else
                p.exp += exp_gain; // stage 3: no cap
        }
        p.work_task = 0;
        p.work_end  = 0;
    }
    save_pet_data();

    e.set_title("💰  打工完成！").set_color(0x2ECC71);
    std::string pet_disp = pet_name(pet.chain, pet.stage, pet.variant);
    if (!pet.talent.empty()) pet_disp += " ✦" + pet.talent;
    e.set_description("**" + pet_disp + "** 打工回來了！");
    std::string reward_str = "+" + std::to_string(reward) + " 碼";
    if (doubled_lucky) reward_str += " 🍀（幸運雙倍！）";
    e.add_field("💰  獎勵",   reward_str, true);
    e.add_field("✨  經驗",   "+" + std::to_string(exp_gain) + " exp", true);
    e.add_field("💼  餘額",   std::to_string(get_chips(uid)) + " 碼", true);
    if (dream_triggered) e.add_field("🌙  喜歡作夢", "🎆 籌碼翻倍！！", false);
    m.add_embed(e);

    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component vb;
    vb.set_type(dpp::cot_button).set_label("🐾 查看寵物")
      .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_primary);
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
