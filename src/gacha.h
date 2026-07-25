#pragma once
#include "types.h"
#include "chips.h"
#include <random>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

// ─── Equipment item definitions ───────────────────────────────────────────────

struct GachaItem {
    std::string key;        // "EQ_W_A_SR"
    std::string name;
    std::string slot;       // W/G/C/S/K
    std::string rarity;     // C/R/SR/UR
    std::string set_tag;    // A/B/C, "" for K
    std::string set_name;   // 堅韌/生命/衝鋒
    int         stat_val;
    std::string stat_type;  // "atk"/"hp"/"def"
    std::string image_url;
    int         item_id = 0;
};

// Image URLs for 衝鋒套裝 SR/UR (provided by owner, may expire)
#define IMG_SR_W_C "https://media.discordapp.net/attachments/1514918524164898966/1521775721838546974/1402017.png?ex=6a460f95&is=6a44be15&hm=ba19cda12d44b39d2ef6d7b483c7cdddfc9efcf4751f887d85c3fe23e8654905&=&format=webp&quality=lossless&width=62&height=60"
#define IMG_SR_G_C "https://media.discordapp.net/attachments/1514918524164898966/1521775722388131840/1082168.png?ex=6a460f95&is=6a44be15&hm=ec9fafea788b680045598c0400fceaf62ca38a17b063c7392c09e174e055a9fd&=&format=webp&quality=lossless&width=56&height=60"
#define IMG_SR_C_C "https://media.discordapp.net/attachments/1514918524164898966/1521775722123755570/1052075.png?ex=6a460f95&is=6a44be15&hm=a708874d2b8b124f981444d3c87aa52b125d15288672a286dcc7c98b7278ba06&=&format=webp&quality=lossless&width=52&height=62"
#define IMG_SR_S_C "https://media.discordapp.net/attachments/1514918524164898966/1521775721498935456/1072355.png?ex=6a460f95&is=6a44be15&hm=ddee6379144cc03da7b7719fc3084922b35449283fa5ca6abc560f65f34a39ec&=&format=webp&quality=lossless&width=54&height=62"
#define IMG_UR_W_C "https://media.discordapp.net/attachments/1514918524164898966/1521767086144229386/1402153.png?ex=6a46078a&is=6a44b60a&hm=1fbcd084a411d9dc2bf00cb9a1d01588eba5a1bd0bc3d33d4b12bf37b72eff40&=&format=webp&quality=lossless&width=76&height=76"
#define IMG_UR_G_C "https://media.discordapp.net/attachments/1514918524164898966/1521767086848999524/1082613.png?ex=6a46078a&is=6a44b60a&hm=049361533a728f530b44aae4a0ceae8f8299d62f0fee1f1cdd6af590f04d8e59&=&format=webp&quality=lossless&width=62&height=60"
#define IMG_UR_C_C "https://media.discordapp.net/attachments/1514918524164898966/1521767086454472715/1052807.png?ex=6a46078a&is=6a44b60a&hm=07f430ed174f5d2d609c44614897c48cf86885cf7a6342c62048e3531c4e9aca&=&format=webp&quality=lossless&width=62&height=68"
#define IMG_UR_S_C "https://media.discordapp.net/attachments/1514918524164898966/1521767087222034482/1072972.png?ex=6a46078a&is=6a44b60a&hm=be927c1c2feea959244f79ae429afc7637a5026f6ae32456daaeab8d6b805cfa&=&format=webp&quality=lossless&width=52&height=64"
// 生命套裝 (B) SR/UR images
#define IMG_SR_W_B "https://media.discordapp.net/attachments/1514918524164898966/1522055829925134486/1452021.png?ex=6a471474&is=6a45c2f4&hm=decc1b68362fa8239d5b78563628ac9c3f3b961e2199def3e0b15d955ee08c78&=&format=webp&quality=lossless&width=66&height=64"
#define IMG_SR_G_B "https://media.discordapp.net/attachments/1514918524164898966/1522055829661024286/1082108.png?ex=6a471474&is=6a45c2f4&hm=e9663e9214437f5509132d8e8ef6ede9862cafe8e29a9ae249633141a85f57de&=&format=webp&quality=lossless&width=52&height=58"
#define IMG_SR_C_B "https://media.discordapp.net/attachments/1514918524164898966/1522055829384073256/1051065.png?ex=6a471474&is=6a45c2f4&hm=45f7c8a51e6a67e2d5aba0818613cbc41c16dfb5e5b847e4acf1f1c46dd84311&=&format=webp&quality=lossless&width=52&height=64"
#define IMG_SR_S_B "https://media.discordapp.net/attachments/1514918524164898966/1522055829115633734/1072167.png?ex=6a471474&is=6a45c2f4&hm=4ee6e6a6f8c013a38ad3a5db1df88b8de6043d33e447c4c680b33d55c7fd6567&=&format=webp&quality=lossless&width=52&height=58"
#define IMG_UR_W_B "https://media.discordapp.net/attachments/1514918524164898966/1522055343453114408/1452172.png?ex=6a471400&is=6a45c280&hm=460633908a3db0bf29840ddc6ac501517287d371412d4b5cf3dbfde28c41cf3d&=&format=webp&quality=lossless&width=72&height=72"
#define IMG_UR_G_B "https://media.discordapp.net/attachments/1514918524164898966/1522055344174399538/1082236.png?ex=6a471400&is=6a45c280&hm=cef73bf5745e9ddfbd45d07caf494f068a8ef3ec9d9aae6fb54b890138fe8eee&=&format=webp&quality=lossless&width=58&height=60"
#define IMG_UR_C_B "https://media.discordapp.net/attachments/1514918524164898966/1522055343922745384/1052157.png?ex=6a471400&is=6a45c280&hm=1fb513402a806a1811d6fed78c62f1365e254c85495ba90a64fadfa5d4f29e16&=&format=webp&quality=lossless&width=58&height=60"
#define IMG_UR_S_B "https://media.discordapp.net/attachments/1514918524164898966/1522055344438775928/1072357.png?ex=6a471400&is=6a45c280&hm=3b1922aa595c1d62bbe9f13a0c025ad6f33f469d81c2fbad0caafb15b453c936&=&format=webp&quality=lossless&width=52&height=60"
// 堅韌套裝 (A) SR/UR images
#define IMG_SR_W_A "https://media.discordapp.net/attachments/1514918524164898966/1522116104816951458/1312009.png?ex=6a474c96&is=6a45fb16&hm=d456bce396a15a586537a416f074c3fa8afea0d2c9579b679aed184ecebe3d19&=&format=webp&quality=lossless"
#define IMG_SR_G_A "https://media.discordapp.net/attachments/1514918524164898966/1522116105475326122/1082024.png?ex=6a474c97&is=6a45fb17&hm=d230245ad64d3c0e85c9cf6ddf8e7e71ba64e909785726cffdd0904fd16da1b9&=&format=webp&quality=lossless"
#define IMG_SR_C_A "https://media.discordapp.net/attachments/1514918524164898966/1522116105861074974/1051063.png?ex=6a474c97&is=6a45fb17&hm=30ff0e3b50355e1a39df0554564965fcf02d5c6954738c889619821604195e5b&=&format=webp&quality=lossless"
#define IMG_SR_S_A "https://media.discordapp.net/attachments/1514918524164898966/1522116105177403512/1072151.png?ex=6a474c97&is=6a45fb17&hm=e637a0745e8250b00173bd25270418bdc80d392effda55c9ba8851ca645ac896&=&format=webp&quality=lossless"
#define IMG_UR_W_A "https://media.discordapp.net/attachments/1514918524164898966/1522114180025548850/1412026.png?ex=6a474acc&is=6a45f94c&hm=dbc89978fb161ec08b1e3142aa5c3e08fe5ea69562ed13a0cc723230b4c6d7b4&=&format=webp&quality=lossless"
#define IMG_UR_G_A "https://media.discordapp.net/attachments/1514918524164898966/1522114180625469480/1082540.png?ex=6a474acc&is=6a45f94c&hm=fc27e5a0909d6d48572b8add93d9c5eb299b73139f04bd4d98109e53354fa0e4&=&format=webp&quality=lossless"
#define IMG_UR_C_A "https://media.discordapp.net/attachments/1514918524164898966/1522114180331602000/1052071.png?ex=6a474acc&is=6a45f94c&hm=4a4fffde8338c8e5c407a3f4d29da72e83ff611d1f228223335d621a3d88a87e&=&format=webp&quality=lossless"
#define IMG_UR_S_A "https://media.discordapp.net/attachments/1514918524164898966/1522114181040570479/1072215.png?ex=6a474acc&is=6a45f94c&hm=75886683242859bf0bcba5494d80e37c342d73d84acabfe4c86727a0585b1f15&=&format=webp&quality=lossless"

static const std::vector<GachaItem> GACHA_ITEMS = {
    // ── 堅韌套裝 (A) ─────────────────────────────────────────────────────────
    {"EQ_W_A_C",  "破爛的堅韌武器","W","C", "A","堅韌", 1,"atk","",  91001},
    {"EQ_W_A_R",  "一般的堅韌武器","W","R", "A","堅韌", 2,"atk","",  91002},
    {"EQ_W_A_SR", "高山巨木之斧",  "W","SR","A","堅韌", 3,"atk",IMG_SR_W_A, 91003},
    {"EQ_W_A_UR", "闢天裂地王斧",  "W","UR","A","堅韌", 6,"atk",IMG_UR_W_A, 91004},
    {"EQ_G_A_R",  "一般的堅韌手套","G","R", "A","堅韌", 1,"atk","",  91011},
    {"EQ_G_A_SR", "高山巨木手套",  "G","SR","A","堅韌", 2,"atk",IMG_SR_G_A, 91012},
    {"EQ_G_A_UR", "闢天裂地手套",  "G","UR","A","堅韌", 4,"atk",IMG_UR_G_A, 91013},
    {"EQ_C_A_C",  "破爛的堅韌套服","C","C", "A","堅韌", 5,"hp", "",  91021},
    {"EQ_C_A_R",  "一般的堅韌套服","C","R", "A","堅韌",10,"hp", "",  91022},
    {"EQ_C_A_SR", "高山巨木套服",  "C","SR","A","堅韌",15,"hp", IMG_SR_C_A, 91023},
    {"EQ_C_A_UR", "闢天裂地套服",  "C","UR","A","堅韌",30,"hp", IMG_UR_C_A, 91024},
    {"EQ_S_A_R",  "一般的堅韌之靴","S","R", "A","堅韌", 5,"hp", "",  91031},
    {"EQ_S_A_SR", "高山巨木之靴",  "S","SR","A","堅韌",10,"hp", IMG_SR_S_A, 91032},
    {"EQ_S_A_UR", "闢天裂地之靴",  "S","UR","A","堅韌",20,"hp", IMG_UR_S_A, 91033},
    // ── 生命套裝 (B) ─────────────────────────────────────────────────────────
    {"EQ_W_B_C",  "破爛的生命武器","W","C", "B","生命", 1,"atk","",  92001},
    {"EQ_W_B_R",  "一般的生命武器","W","R", "B","生命", 2,"atk","",  92002},
    {"EQ_W_B_SR", "生機湧現之弓",  "W","SR","B","生命", 3,"atk",IMG_SR_W_B, 92003},
    {"EQ_W_B_UR", "生生不息魔弓",  "W","UR","B","生命", 6,"atk",IMG_UR_W_B, 92004},
    {"EQ_G_B_R",  "一般的生命手套","G","R", "B","生命", 1,"atk","",  92011},
    {"EQ_G_B_SR", "生機湧現手套",  "G","SR","B","生命", 2,"atk",IMG_SR_G_B, 92012},
    {"EQ_G_B_UR", "生生不息手套",  "G","UR","B","生命", 4,"atk",IMG_UR_G_B, 92013},
    {"EQ_C_B_C",  "破爛的生命套服","C","C", "B","生命", 5,"hp", "",  92021},
    {"EQ_C_B_R",  "一般的生命套服","C","R", "B","生命",10,"hp", "",  92022},
    {"EQ_C_B_SR", "生機湧現套服",  "C","SR","B","生命",15,"hp", IMG_SR_C_B, 92023},
    {"EQ_C_B_UR", "生生不息套服",  "C","UR","B","生命",30,"hp", IMG_UR_C_B, 92024},
    {"EQ_S_B_R",  "一般的生命之靴","S","R", "B","生命", 5,"hp", "",  92031},
    {"EQ_S_B_SR", "生機湧現之靴",  "S","SR","B","生命",10,"hp", IMG_SR_S_B, 92032},
    {"EQ_S_B_UR", "生生不息之靴",  "S","UR","B","生命",20,"hp", IMG_UR_S_B, 92033},
    // ── 衝鋒套裝 (C) ─────────────────────────────────────────────────────────
    {"EQ_W_C_C",  "破爛的衝鋒武器","W","C", "C","衝鋒", 1,"atk","",  93001},
    {"EQ_W_C_R",  "一般的衝鋒武器","W","R", "C","衝鋒", 2,"atk","",  93002},
    {"EQ_W_C_SR", "血斬萬人之刃",  "W","SR","C","衝鋒", 3,"atk",IMG_SR_W_C, 93003},
    {"EQ_W_C_UR", "撼天震地聖劍",  "W","UR","C","衝鋒", 6,"atk",IMG_UR_W_C, 93004},
    {"EQ_G_C_R",  "一般的衝鋒手套","G","R", "C","衝鋒", 1,"atk","",  93011},
    {"EQ_G_C_SR", "血斬萬人手套",  "G","SR","C","衝鋒", 2,"atk",IMG_SR_G_C, 93012},
    {"EQ_G_C_UR", "撼天震地手套",  "G","UR","C","衝鋒", 4,"atk",IMG_UR_G_C, 93013},
    {"EQ_C_C_C",  "破爛的衝鋒套服","C","C", "C","衝鋒", 5,"hp", "",  93021},
    {"EQ_C_C_R",  "一般的衝鋒套服","C","R", "C","衝鋒",10,"hp", "",  93022},
    {"EQ_C_C_SR", "血斬萬人套服",  "C","SR","C","衝鋒",15,"hp", IMG_SR_C_C, 93023},
    {"EQ_C_C_UR", "撼天震地套服",  "C","UR","C","衝鋒",30,"hp", IMG_UR_C_C, 93024},
    {"EQ_S_C_R",  "一般的衝鋒之靴","S","R", "C","衝鋒", 5,"hp", "",  93031},
    {"EQ_S_C_SR", "血斬萬人之靴",  "S","SR","C","衝鋒",10,"hp", IMG_SR_S_C, 93032},
    {"EQ_S_C_UR", "撼天震地之靴",  "S","UR","C","衝鋒",20,"hp", IMG_UR_S_C, 93033},
    // ── 靈魂寶珠 (K) ─────────────────────────────────────────────────────────
    // 可抽取
    {"EQ_K_UR",    "無名女神的寶珠",  "K","UR","","", 5,"def",  "", 94001},
    {"EQ_K_SPEED", "迅捷狼王的寶珠", "K","UR","","", 0,"spd",  "", 94002},
    {"EQ_K_ATHENA","雅典娜的寶珠",   "K","UR","","", 8,"regen","", 94003},
    // 合成限定（不可抽取）
    {"EQ_K_BEAR",  "巨山狂熊的寶珠", "K","UR","","", 0,"block","", 94004},
    {"EQ_K_VIKING","維京的寶珠",     "K","UR","","", 0,"cry",  "", 94005},
    {"EQ_K_WARGOD",      "狂怒戰神的寶珠", "K","UR","","",10,"atk",       "", 94006},
    {"EQ_K_LATUS",       "拉圖斯的寶珠",   "K","UR","","", 0,"latus_orb", "", 94007},
    {"EQ_K_DARKDRAGON",  "暗黑龍王的寶珠", "K","UR","","", 0,"dd_orb",    "", 94008},
};

static const GachaItem* find_gacha_item(const std::string& key) {
    for (auto& gi : GACHA_ITEMS) if (gi.key == key) return &gi;
    return nullptr;
}

static const GachaItem* find_gacha_item_by_id(int id) {
    if (!id) return nullptr;
    for (auto& gi : GACHA_ITEMS) if (gi.item_id == id) return &gi;
    return nullptr;
}

static bool is_eq_key(const std::string& k) {
    return k.size() > 3 && k[0]=='E' && k[1]=='Q' && k[2]=='_';
}

// ─── Rarity helpers ───────────────────────────────────────────────────────────

static uint32_t rarity_color(const std::string& r) {
    if (r == "UR") return 0xFFD700;
    if (r == "SR") return 0x9B59B6;
    if (r == "R")  return 0x3498DB;
    return 0x95A5A6; // C
}

static std::string rarity_label(const std::string& r) {
    if (r == "UR") return "✨UR";
    if (r == "SR") return "💜SR";
    if (r == "R")  return "🔵R";
    return "⬜C";
}

static int rarity_sell_price(const std::string& r) {
    if (r == "UR") return 100;
    if (r == "SR") return 10;
    if (r == "R")  return 5;
    return 1; // C
}

static std::string slot_label(const std::string& s) {
    if (s == "W") return "⚔️ 武器";
    if (s == "G") return "🧤 手套";
    if (s == "C") return "👘 套服";
    if (s == "S") return "👟 鞋子";
    if (s == "K") return "💎 靈魂寶珠";
    return s;
}

static std::string stat_label(const GachaItem& gi) {
    if (gi.stat_type == "atk")   return "⚔️ +" + std::to_string(gi.stat_val) + " 攻擊力";
    if (gi.stat_type == "hp")    return "❤️ +" + std::to_string(gi.stat_val) + " 生命值";
    if (gi.stat_type == "def") {
        if (gi.key == "EQ_K_UR") return "🛡️ 單人自身+5防禦；組隊全體+2防禦";
        return "🛡️ +" + std::to_string(gi.stat_val) + " 防禦力";
    }
    if (gi.stat_type == "spd")   return "⚡ 單人必定先手；組隊：40%機率多行動一回合";
    if (gi.stat_type == "regen") return "💚 單人：30%機率自身恢復 +8 HP；組隊：20%機率全體恢復 +5 HP";
    if (gi.stat_type == "block") return "🛡️ 單人：防禦降低怪物下兩次攻擊60%；組隊：防禦降低傷害20%";
    if (gi.stat_type == "cry")      return "🔥 被動狂暴：HP≤50% 傷害×1.4，HP≤25% 傷害×1.7";
    if (gi.stat_type == "latus_orb") return "🔶 HP≤20% 時回復至 50%（每場一次）";
    if (gi.stat_type == "dd_orb")    return "🌑 攻擊後回復造成傷害的 1/10（最多 10 HP）";
    return "";
}

// ─── Gacha pity persistence ──────────────────────────────────────────────────

static void load_gacha_pity() {
    std::ifstream f("gacha_pity.json");
    if (!f) return;
    nlohmann::json j; f >> j;
    std::lock_guard<std::mutex> lk(data_mutex);
    for (auto& [k, v] : j.items())
        gacha_pity_data[std::stoull(k)] = v.get<int>();
}

static void save_gacha_pity() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, cnt] : gacha_pity_data)
            j[std::to_string(uid)] = cnt;
    }
    atomic_write("gacha_pity.json", j.dump(2));
}

// ─── Persistence ─────────────────────────────────────────────────────────────

static const std::string EQUIPPED_FILE   = "equipped.json";
static const std::string HUNT_CLEAR_FILE = "hunt_clear.json";

static void save_equipped() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, eq] : equipped_data) {
            j[std::to_string((uint64_t)uid)] = {
                {"weapon",  eq.weapon},  {"glove",   eq.glove},
                {"clothes", eq.clothes}, {"shoes",   eq.shoes},
                {"orb",     eq.orb}
            };
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(EQUIPPED_FILE, j.dump(2));
}

static void load_equipped() {
    std::ifstream f(EQUIPPED_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& eq = equipped_data[uid];
            eq.weapon  = v.value("weapon",  "");
            eq.glove   = v.value("glove",   "");
            eq.clothes = v.value("clothes", "");
            eq.shoes   = v.value("shoes",   "");
            eq.orb     = v.value("orb",     "");
        }
    } catch (...) {}
}

static void save_hunt_clear() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, clears] : hunt_clear_data) {
            nlohmann::json arr = nlohmann::json::array();
            for (auto& mk : clears) arr.push_back(mk);
            j[std::to_string((uint64_t)uid)] = arr;
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(HUNT_CLEAR_FILE, j.dump(2));
}

static void load_hunt_clear() {
    std::ifstream f(HUNT_CLEAR_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            for (auto& mk : v) hunt_clear_data[uid].insert(mk.get<std::string>());
        }
    } catch (...) {}
}

// ─── Pet stats (base + equipment) ────────────────────────────────────────────

struct PetStats { int hp = 0; int atk = 0; int def = 0; };

// Count set pieces from equipped items (W/G/C/S slots only; K has no set)
static std::map<std::string,int> calc_set_count(const PlayerEquipment& eq) {
    std::map<std::string,int> cnt;
    for (auto& key : {eq.weapon, eq.glove, eq.clothes, eq.shoes}) {
        if (key.empty()) continue;
        auto* gi = find_gacha_item(key);
        if (gi && !gi->set_tag.empty()) cnt[gi->set_tag]++;
    }
    return cnt;
}

static PetStats calc_pet_stats(dpp::snowflake uid, const Pet& pet) {
    if (pet.stage == 0) return {};
    PetStats s;
    if      (pet.stage == 1) { s.hp = 50;  s.atk = 10; }
    else if (pet.stage == 2) { s.hp = 75;  s.atk = 15; }
    else                     { s.hp = 100; s.atk = 20; }

    PlayerEquipment eq;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = equipped_data.find(uid);
        if (it != equipped_data.end()) eq = it->second;
    }
    // Individual item stats
    auto apply = [&](const std::string& key) {
        if (key.empty()) return;
        auto* gi = find_gacha_item(key);
        if (!gi) return;
        if (gi->stat_type == "atk") s.atk += gi->stat_val;
        if (gi->stat_type == "hp")  s.hp  += gi->stat_val;
        if (gi->stat_type == "def") s.def += gi->stat_val;
    };
    apply(eq.weapon); apply(eq.glove); apply(eq.clothes);
    apply(eq.shoes);  apply(eq.orb);

    // Set bonuses
    auto sc = calc_set_count(eq);
    int a = sc.count("A") ? sc["A"] : 0;
    int b = sc.count("B") ? sc["B"] : 0;
    int c = sc.count("C") ? sc["C"] : 0;
    // 堅韌 (A): 2件 → +2 DEF; 4件 → +5 DEF（不累積）
    if (a >= 4) s.def += 5;
    else if (a >= 2) s.def += 2;
    // 生命 (B): 2件 → +10 HP; 4件 → +25 HP（不累積）
    if (b >= 4) s.hp += 25;
    else if (b >= 2) s.hp += 10;
    // 衝鋒 (C): 2件 → +2 ATK; 4件 → +5 ATK（不累積）
    if (c >= 4) s.atk += 5;
    else if (c >= 2) s.atk += 2;

    return s;
}

// ─── Gacha pull RNG ───────────────────────────────────────────────────────────

static std::mt19937& gacha_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

static const GachaItem& gacha_pull_one(bool star_pool) {
    // 合成限定寶珠：不進入抽取池
    static const std::set<std::string> NOT_GACHABLE = {"EQ_K_BEAR","EQ_K_VIKING","EQ_K_WARGOD","EQ_K_LATUS","EQ_K_DARKDRAGON"};
    static std::vector<const GachaItem*> pool_C, pool_R, pool_SR, pool_UR_eq, pool_UR_orb;
    static bool pools_built = false;
    if (!pools_built) {
        for (auto& gi : GACHA_ITEMS) {
            if (NOT_GACHABLE.count(gi.key)) continue;
            if      (gi.rarity == "C")  pool_C.push_back(&gi);
            else if (gi.rarity == "R")  pool_R.push_back(&gi);
            else if (gi.rarity == "SR") pool_SR.push_back(&gi);
            else if (gi.rarity == "UR") {
                if (gi.slot == "K") pool_UR_orb.push_back(&gi); // 寶珠
                else                pool_UR_eq.push_back(&gi);  // 一般裝備
            }
        }
        pools_built = true;
    }

    // 從 UR 子池抽取：10% 機率抽寶珠，90% 機率抽裝備
    auto pick_ur = [&]() -> const std::vector<const GachaItem*>* {
        int orb_roll = std::uniform_int_distribution<int>(1,10)(gacha_rng());
        return (orb_roll == 1 && !pool_UR_orb.empty()) ? &pool_UR_orb : &pool_UR_eq;
    };

    int roll = std::uniform_int_distribution<int>(1, 100)(gacha_rng());
    const std::vector<const GachaItem*>* pool = nullptr;

    if (star_pool) {
        // SR 80%，UR 20%（其中寶珠 10% = 整體 2%）
        pool = (roll <= 80) ? &pool_SR : pick_ur();
    } else {
        // C 69%，R 20%，SR 10%，UR 1%（其中寶珠 10% = 整體 0.1%）
        if      (roll <= 69) pool = &pool_C;
        else if (roll <= 89) pool = &pool_R;
        else if (roll <= 99) pool = &pool_SR;
        else                 pool = pick_ur();
    }

    int idx = std::uniform_int_distribution<int>(0, (int)pool->size()-1)(gacha_rng());
    return *(*pool)[idx];
}

// 保底 UR：從所有可抽 UR 裝備＋寶珠中隨機一個
static const GachaItem& gacha_pull_ur_pity() {
    static const std::set<std::string> NOT_GACHABLE = {"EQ_K_BEAR","EQ_K_VIKING","EQ_K_WARGOD","EQ_K_LATUS","EQ_K_DARKDRAGON"};
    static std::vector<const GachaItem*> pool_UR_all;
    static bool built = false;
    if (!built) {
        for (auto& gi : GACHA_ITEMS)
            if (gi.rarity == "UR" && !NOT_GACHABLE.count(gi.key))
                pool_UR_all.push_back(&gi);
        built = true;
    }
    int idx2 = std::uniform_int_distribution<int>(0, (int)pool_UR_all.size()-1)(gacha_rng());
    return *pool_UR_all[idx2];
}

// ─── Main gacha lobby message ─────────────────────────────────────────────────

static dpp::message make_gacha_main_msg(dpp::snowflake uid,
                                        const std::string& display_name,
                                        const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)uid);
    int stars = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid);
        if (it != inventory_data.end()) {
            auto sit = it->second.find("star_unknown");
            if (sit != it->second.end()) stars = sit->second;
        }
    }

    dpp::embed e;
    e.set_title("🎰  轉蛋機").set_color(0xF39C12);
    e.set_description("選擇轉蛋池開始抽轉蛋！");
    e.add_field("🎲 一般池",
        "每抽 **50 籌碼**\n機率：⬜C 69% ｜ 🔵R 20% ｜ 💜SR 10% ｜ ✨UR 1%\n"
        "內含武器、手套、套服、鞋子、靈魂寶珠（UR限定）\n🔮 **每 200 抽保底 UR**", false);
    e.add_field("⭐ 群星閃耀之時",
        "每抽 **1 顆星星**（目前持有：" + std::to_string(stars) + " 顆）\n"
        "機率：💜SR 80% ｜ ✨UR 20%", false);
    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    auto mk = [&](const std::string& lbl, const std::string& id, dpp::component_style sty) {
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(lbl).set_id(id).set_style(sty);
        row.add_component(b);
    };
    mk("🎲 一般池",       "gacha_banner_normal_" + uid_s, dpp::cos_primary);
    mk("⭐ 群星閃耀之時", "gacha_banner_star_"   + uid_s, dpp::cos_success);
    msg.add_component(row);
    return msg;
}

// ─── Banner detail + pull message ────────────────────────────────────────────

static dpp::message make_gacha_banner_msg(dpp::snowflake uid, bool star_pool,
                                          const std::string& display_name,
                                          const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)uid);
    int64_t chips = get_chips(uid);
    int stars = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = inventory_data.find(uid);
        if (it != inventory_data.end()) {
            auto sit = it->second.find("star_unknown");
            if (sit != it->second.end()) stars = sit->second;
        }
    }

    dpp::embed e;
    if (star_pool) {
        e.set_title("⭐  群星閃耀之時").set_color(0xF1C40F);
        e.set_description("高機率獲得稀有裝備！\n機率：💜SR **80%** ｜ ✨UR **20%**");
        e.add_field("⭐ 持有星星", std::to_string(stars) + " 顆", true);
        e.add_field("💰 餘額",     std::to_string(chips) + " 碼", true);
    } else {
        int pity = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = gacha_pity_data.find((uint64_t)uid);
            if (it != gacha_pity_data.end()) pity = it->second;
        }
        e.set_title("🎲  一般池").set_color(0x3498DB);
        e.set_description("機率：⬜C **69%** ｜ 🔵R **20%** ｜ 💜SR **10%** ｜ ✨UR **1%**\n"
                          "UR 靈魂寶珠僅在 UR 池中出現。\n"
                          "每 **200 抽**保底抽一次 UR！");
        e.add_field("💰 餘額",      std::to_string(chips) + " 碼", true);
        e.add_field("🎟️ 1連費用",   "50 碼", true);
        e.add_field("🎟️ 10連費用", "500 碼", true);
        e.add_field("🔮 保底進度",  std::to_string(pity) + " / 200", true);
    }
    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    std::string pfx = star_pool ? "gacha_star_" : "gacha_norm_";
    auto mk = [&](const std::string& lbl, const std::string& id, bool dis = false) {
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(lbl).set_id(id)
         .set_style(star_pool ? dpp::cos_success : dpp::cos_primary)
         .set_disabled(dis);
        row.add_component(b);
    };
    if (star_pool) {
        mk("⭐ 1連（1顆）",  pfx + "1_"  + uid_s, stars < 1);
        mk("⭐ 10連（10顆）",pfx + "10_" + uid_s, stars < 10);
    } else {
        mk("🎲 1連（50碼）",  pfx + "1_"  + uid_s, chips < 50);
        mk("🎲 10連（500碼）",pfx + "10_" + uid_s, chips < 500);
    }
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回")
        .set_id("gacha_main_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(back);
    msg.add_component(row);
    return msg;
}

// ─── Pull result message ──────────────────────────────────────────────────────

static dpp::message make_gacha_result_msg(dpp::snowflake uid,
                                          const std::vector<const GachaItem*>& pulls,
                                          bool star_pool,
                                          const std::string& display_name,
                                          const std::string& avatar_url,
                                          int pity_after = -1,
                                          bool pity_fired = false) {
    std::string uid_s = std::to_string((uint64_t)uid);

    // Find highest rarity for embed color
    uint32_t color = 0x95A5A6;
    std::string last_img;
    for (auto* gi : pulls) {
        if (gi->rarity == "UR") { color = 0xFFD700; if (!gi->image_url.empty()) last_img = gi->image_url; }
        else if (gi->rarity == "SR" && color != 0xFFD700) { color = 0x9B59B6; if (!gi->image_url.empty()) last_img = gi->image_url; }
        else if (gi->rarity == "R"  && color == 0x95A5A6) color = 0x3498DB;
    }

    dpp::embed e;
    e.set_title("🎰  抽卡結果").set_color(color);
    if (!last_img.empty()) e.set_thumbnail(last_img);

    std::string desc;
    for (auto* gi : pulls)
        desc += rarity_label(gi->rarity) + " **" + gi->name + "** "
             + slot_label(gi->slot) + " " + stat_label(*gi) + "\n";
    e.set_description(desc);

    // Inventory add (locked)
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto* gi : pulls) inventory_data[uid][gi->key]++;
    }

    if (pity_fired)
        e.add_field("🔮 保底觸發！", "恭喜獲得額外 **UR** 裝備！", false);
    if (pity_after >= 0)
        e.add_field("🔮 保底進度", std::to_string(pity_after) + " / 200", true);

    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);

    // Row 1: 再抽十次 (only after 10-pull) + 返回轉蛋機
    if (pulls.size() == 10) {
        int64_t chips2 = 0; int stars2 = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            chips2 = chip_data.count(uid) ? chip_data.at(uid).chips : 0;
            if (inventory_data.count(uid) && inventory_data.at(uid).count("star_unknown"))
                stars2 = inventory_data.at(uid).at("star_unknown");
        }
        dpp::component row10; row10.set_type(dpp::cot_action_row);
        dpp::component ten;
        ten.set_type(dpp::cot_button)
           .set_label(star_pool ? "⭐ 再抽十次（10顆）" : "🎲 再抽十次（500碼）")
           .set_id(star_pool ? ("gacha_star_10_" + uid_s) : ("gacha_norm_10_" + uid_s))
           .set_style(star_pool ? dpp::cos_success : dpp::cos_primary)
           .set_disabled(star_pool ? stars2 < 10 : chips2 < 500);
        dpp::component back10;
        back10.set_type(dpp::cot_button).set_label("↩ 返回轉蛋機")
              .set_id("gacha_main_" + uid_s).set_style(dpp::cos_secondary);
        row10.add_component(ten); row10.add_component(back10);
        msg.add_component(row10);
    } else {
        dpp::component row; row.set_type(dpp::cot_action_row);
        dpp::component again, back_main;
        again.set_type(dpp::cot_button)
             .set_label(star_pool ? "⭐ 繼續抽（星星池）" : "🎲 繼續抽（一般池）")
             .set_id(star_pool ? ("gacha_banner_star_" + uid_s) : ("gacha_banner_normal_" + uid_s))
             .set_style(star_pool ? dpp::cos_success : dpp::cos_primary);
        back_main.set_type(dpp::cot_button).set_label("↩ 返回轉蛋機")
                 .set_id("gacha_main_" + uid_s).set_style(dpp::cos_secondary);
        row.add_component(again); row.add_component(back_main);
        msg.add_component(row);
    }
    return msg;
}

// ─── Equipment overview ───────────────────────────────────────────────────────

static dpp::message make_equip_msg(dpp::snowflake uid, const Pet& pet,
                                   const std::string& display_name,
                                   const std::string& avatar_url) {
    std::string uid_s = std::to_string((uint64_t)uid);
    PlayerEquipment eq;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = equipped_data.find(uid);
        if (it != equipped_data.end()) eq = it->second;
    }
    PetStats stats = calc_pet_stats(uid, pet);

    dpp::embed e;
    e.set_title("🗡️  裝備總覽").set_color(0xE67E22);

    auto eq_line = [](const std::string& key) -> std::string {
        if (key.empty()) return "（空）";
        auto* gi = find_gacha_item(key);
        return gi ? (rarity_label(gi->rarity) + " " + gi->name) : key;
    };
    e.add_field("⚔️ 武器",    eq_line(eq.weapon),  true);
    e.add_field("🧤 手套",    eq_line(eq.glove),   true);
    e.add_field("👘 套服",    eq_line(eq.clothes), true);
    e.add_field("👟 鞋子",    eq_line(eq.shoes),   true);
    e.add_field("💎 靈魂寶珠",eq_line(eq.orb),     true);
    e.add_field("　","　",true); // spacer
    e.add_field("⚔️ 總攻擊力", std::to_string(stats.atk), true);
    e.add_field("❤️ 總生命值", std::to_string(stats.hp),  true);
    e.add_field("🛡️ 總防禦力", std::to_string(stats.def), true);

    // Set bonus status
    auto sc = calc_set_count(eq);
    int a = sc.count("A") ? sc["A"] : 0;
    int b = sc.count("B") ? sc["B"] : 0;
    int c = sc.count("C") ? sc["C"] : 0;
    auto tick = [](bool on) { return std::string(on ? "✅" : "⬜"); };
    std::string set_desc;
    set_desc += "**堅韌** (" + std::to_string(a) + "/4)  "
              + tick(a>=2) + " 2件：防禦力 +2　"
              + tick(a>=4) + " 4件：防禦力 +5\n";
    set_desc += "**生命** (" + std::to_string(b) + "/4)  "
              + tick(b>=2) + " 2件：生命 +10　"
              + tick(b>=4) + " 4件：生命 +25\n";
    set_desc += "**衝鋒** (" + std::to_string(c) + "/4)  "
              + tick(c>=2) + " 2件：攻擊力 +2　"
              + tick(c>=4) + " 4件：攻擊力 +5\n";
    e.add_field("✨ 套裝效果", set_desc, false);

    dpp::embed_footer footer;
    footer.text = "👤 " + display_name;
    if (!avatar_url.empty()) footer.icon_url = avatar_url;
    e.set_footer(footer);

    dpp::message msg; msg.add_embed(e);
    // Slot buttons
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    for (auto& [lbl, slot] : std::vector<std::pair<std::string,std::string>>{
            {"⚔️ 武器","W"},{"🧤 手套","G"},{"👘 套服","C"},{"👟 鞋子","S"},{"💎 靈魂","K"}}) {
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(lbl)
         .set_id("equip_slot_" + uid_s + "_" + slot).set_style(dpp::cos_primary);
        row1.add_component(b);
    }
    msg.add_component(row1);
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    dpp::component pet_back;
    pet_back.set_type(dpp::cot_button).set_label("🏠 大廳")
            .set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary);
    nav_row.add_component(pet_back);
    msg.add_component(nav_row);
    return msg;
}

// ─── Equipment slot detail (inventory list for that slot) ────────────────────

static dpp::message make_equip_slot_msg(dpp::snowflake uid, const std::string& slot,
                                        const std::string& display_name,
                                        const std::string& avatar_url,
                                        int page = 0) {
    std::string uid_s = std::to_string((uint64_t)uid);
    PlayerEquipment eq;
    std::map<std::string, int> inv;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto eit = equipped_data.find(uid);
        if (eit != equipped_data.end()) eq = eit->second;
        auto iit = inventory_data.find(uid);
        if (iit != inventory_data.end()) inv = iit->second;
    }

    std::string cur_equipped;
    if      (slot == "W") cur_equipped = eq.weapon;
    else if (slot == "G") cur_equipped = eq.glove;
    else if (slot == "C") cur_equipped = eq.clothes;
    else if (slot == "S") cur_equipped = eq.shoes;
    else if (slot == "K") cur_equipped = eq.orb;

    // Collect items of this slot in inventory
    std::vector<const GachaItem*> items;
    for (auto& gi : GACHA_ITEMS) {
        if (gi.slot != slot) continue;
        auto it = inv.find(gi.key);
        if (it != inv.end() && it->second > 0) items.push_back(&gi);
    }

    // Sort by rarity desc
    auto rrank = [](const std::string& r) {
        if (r=="UR") return 3; if (r=="SR") return 2; if (r=="R") return 1; return 0;
    };
    std::sort(items.begin(), items.end(), [&](auto a, auto b) {
        return rrank(a->rarity) > rrank(b->rarity);
    });

    const int PER_PAGE = 4;
    int total_pages = std::max(1, ((int)items.size() + PER_PAGE - 1) / PER_PAGE);
    page = std::max(0, std::min(page, total_pages - 1));
    int start = page * PER_PAGE;
    int end   = std::min(start + PER_PAGE, (int)items.size());

    dpp::embed e;
    e.set_title(slot_label(slot) + "  裝備選擇").set_color(0xE67E22);

    std::string cur_line = cur_equipped.empty() ? "（空）" : "";
    if (!cur_equipped.empty()) {
        auto* gi = find_gacha_item(cur_equipped);
        cur_line = gi ? (rarity_label(gi->rarity) + " **" + gi->name + "** " + stat_label(*gi)) : cur_equipped;
    }
    e.add_field("目前裝備", cur_line, false);

    if (items.empty()) {
        e.set_description("背包中沒有此部位的裝備。");
    } else {
        std::string desc;
        for (int i = start; i < end; i++) {
            auto* gi = items[i];
            int cnt = inv.count(gi->key) ? inv.at(gi->key) : 0;
            bool is_eq = (gi->key == cur_equipped);
            desc += (is_eq ? "✅ " : "") + rarity_label(gi->rarity)
                  + " **" + gi->name + "** " + stat_label(*gi)
                  + " ×" + std::to_string(cnt) + "\n";
        }
        e.set_description(desc);
        e.set_footer(dpp::embed_footer().set_text(
            "第 " + std::to_string(page+1) + "/" + std::to_string(total_pages) + " 頁  |  👤 " + display_name));
    }
    if (!e.footer.has_value() || e.footer->text.empty()) {
        dpp::embed_footer footer; footer.text = "👤 " + display_name;
        if (!avatar_url.empty()) footer.icon_url = avatar_url;
        e.set_footer(footer);
    }

    // Show currently equipped item image as thumbnail
    if (!cur_equipped.empty()) {
        auto* ceq = find_gacha_item(cur_equipped);
        if (ceq && !ceq->image_url.empty()) e.set_thumbnail(ceq->image_url);
    }

    dpp::message msg; msg.add_embed(e);

    // Item buttons (current page)
    if (!items.empty()) {
        dpp::component item_row; item_row.set_type(dpp::cot_action_row);
        for (int i = start; i < end; i++) {
            auto* gi = items[i];
            bool is_eq = (gi->key == cur_equipped);
            dpp::component b;
            b.set_type(dpp::cot_button)
             .set_label((is_eq ? "✅ " : "") + gi->name)
             .set_id("equip_set_" + uid_s + "_" + gi->key)
             .set_style(is_eq ? dpp::cos_success : dpp::cos_secondary);
            item_row.add_component(b);
        }
        msg.add_component(item_row);
    }

    // Pagination + back row
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    if (page > 0) {
        dpp::component prev;
        prev.set_type(dpp::cot_button).set_label("◀ 上頁")
            .set_id("equip_slot_" + uid_s + "_" + slot + "_" + std::to_string(page-1))
            .set_style(dpp::cos_secondary);
        nav_row.add_component(prev);
    }
    if (page < total_pages - 1) {
        dpp::component nxt;
        nxt.set_type(dpp::cot_button).set_label("下頁 ▶")
           .set_id("equip_slot_" + uid_s + "_" + slot + "_" + std::to_string(page+1))
           .set_style(dpp::cos_secondary);
        nav_row.add_component(nxt);
    }
    if (!cur_equipped.empty()) {
        dpp::component uneq;
        uneq.set_type(dpp::cot_button).set_label("脫下裝備")
            .set_id("equip_unequip_" + uid_s + "_" + slot).set_style(dpp::cos_danger);
        nav_row.add_component(uneq);
    }
    dpp::component back;
    back.set_type(dpp::cot_button).set_label("↩ 返回裝備總覽")
        .set_id("equip_main_" + uid_s).set_style(dpp::cos_secondary);
    nav_row.add_component(back);
    dpp::component pet_back;
    pet_back.set_type(dpp::cot_button).set_label("🐾 寵物頁面")
            .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary);
    nav_row.add_component(pet_back);
    if (!nav_row.components.empty()) msg.add_component(nav_row);
    return msg;
}

// ─── Equipment compendium ─────────────────────────────────────────────────────

static dpp::message make_equipdex_main_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::embed e;
    e.set_title("⚔️  裝備圖鑑").set_color(0xE67E22);
    e.set_description("選擇套裝查看詳細資訊與圖片。");
    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component menu;
    menu.set_type(dpp::cot_selectmenu).set_id("equipdex_set_" + uid_s)
        .set_placeholder("選擇套裝");
    menu.add_select_option(dpp::select_option("⚔️ 堅韌套裝","A","2件+2防禦 / 4件+5防禦"));
    menu.add_select_option(dpp::select_option("🌿 生命套裝","B","2件+10生命 / 4件+25生命"));
    menu.add_select_option(dpp::select_option("💥 衝鋒套裝","C","2件+2攻擊力 / 4件+5攻擊力"));
    menu.add_select_option(dpp::select_option("💎 靈魂寶珠","K","獨立UR單品"));
    row.add_component(menu); msg.add_component(row);
    return msg;
}

static dpp::message make_equipdex_set_msg(dpp::snowflake uid, const std::string& set_tag) {
    std::string uid_s = std::to_string((uint64_t)uid);
    static const std::map<std::string,std::string> SET_NAMES = {
        {"A","堅韌"},{"B","生命"},{"C","衝鋒"},{"K","靈魂寶珠"}
    };
    static const std::map<std::string,uint32_t> SET_COLORS = {
        {"A",0xE74C3C},{"B",0x2ECC71},{"C",0x3498DB},{"K",0xFFD700}
    };
    static const std::map<std::string,std::string> SET_BONUS = {
        {"A","2件效果：🛡️ 防禦力 +2\n4件效果：🛡️ 防禦力 +5"},
        {"B","2件效果：❤️ 生命 +10\n4件效果：❤️ 生命 +25（累積 +35）"},
        {"C","2件效果：⚔️ 攻擊力 +2\n4件效果：⚔️ 攻擊力 +5（累積 +7）"},
        {"K","獨立 UR 單品，不計入套裝計數"}
    };
    std::string set_name = SET_NAMES.count(set_tag) ? SET_NAMES.at(set_tag) : set_tag;
    uint32_t color = SET_COLORS.count(set_tag) ? SET_COLORS.at(set_tag) : 0x95A5A6;

    // Collect items for this set
    std::vector<const GachaItem*> items;
    for (auto& gi : GACHA_ITEMS)
        if (set_tag == "K" ? gi.slot == "K" : gi.set_tag == set_tag)
            items.push_back(&gi);

    // Build description
    static const std::map<std::string,int> SLOT_ORDER = {{"W",0},{"G",1},{"C",2},{"S",3},{"K",4}};
    static const std::map<std::string,int> RARITY_ORDER = {{"C",0},{"R",1},{"SR",2},{"UR",3}};
    std::sort(items.begin(), items.end(), [](const GachaItem* a, const GachaItem* b) {
        int sa = SLOT_ORDER.count(a->slot) ? SLOT_ORDER.at(a->slot) : 9;
        int sb = SLOT_ORDER.count(b->slot) ? SLOT_ORDER.at(b->slot) : 9;
        if (sa != sb) return sa < sb;
        int ra = RARITY_ORDER.count(a->rarity) ? RARITY_ORDER.at(a->rarity) : 9;
        int rb = RARITY_ORDER.count(b->rarity) ? RARITY_ORDER.at(b->rarity) : 9;
        return ra < rb;
    });

    static const std::map<std::string,std::string> SLOT_LABEL = {
        {"W","⚔️武器"},{"G","🧤手套"},{"C","👘套服"},{"S","👟鞋子"},{"K","💎靈魂寶珠"}
    };
    std::string desc;
    if (SET_BONUS.count(set_tag)) desc += "**套裝效果**\n" + SET_BONUS.at(set_tag) + "\n\n**裝備列表**\n";
    for (auto* gi : items) {
        std::string slot_s = SLOT_LABEL.count(gi->slot) ? SLOT_LABEL.at(gi->slot) : gi->slot;
        std::string stat_s = stat_label(*gi);
        desc += rarity_label(gi->rarity) + " `#" + std::to_string(gi->item_id) + "` **" + gi->name + "**"
              + "  " + slot_s + "  " + stat_s + "\n";
    }

    // Main embed
    dpp::embed e;
    e.set_title("⚔️  裝備圖鑑 — " + set_name + "套裝").set_color(color);
    e.set_description(desc);
    dpp::message msg; msg.add_embed(e);

    // Image embeds: one per item that has an image
    for (auto* gi : items) {
        if (gi->image_url.empty()) continue;
        dpp::embed ie;
        ie.set_color(rarity_color(gi->rarity));
        {
            dpp::embed_author auth;
            auth.name = rarity_label(gi->rarity) + " " + gi->name + "  #" + std::to_string(gi->item_id);
            ie.set_author(auth);
        }
        ie.set_image(gi->image_url);
        msg.add_embed(ie);
    }

    // Re-add dropdown so user can switch sets
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component menu;
    menu.set_type(dpp::cot_selectmenu).set_id("equipdex_set_" + uid_s)
        .set_placeholder("切換套裝");
    menu.add_select_option(dpp::select_option("⚔️ 堅韌套裝","A","2件+2防禦 / 4件+5防禦"));
    menu.add_select_option(dpp::select_option("🌿 生命套裝","B","2件+10生命 / 4件+25生命"));
    menu.add_select_option(dpp::select_option("💥 衝鋒套裝","C","2件+2攻擊力 / 4件+5攻擊力"));
    menu.add_select_option(dpp::select_option("💎 靈魂寶珠","K","獨立UR單品"));
    row.add_component(menu); msg.add_component(row);
    return msg;
}
