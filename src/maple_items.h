#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

// ─── 楓之谷世界：裝備／卷軸定義 ─────────────────────────────────────────────
// 這個檔案只放「純資料」：裝備（武器/防具/耳環）與卷軸的定義表。
// 找裝備/卷軸、套用效果等邏輯留在 maple.h，這裡只負責越長越多的資料本體。

struct MapleItemDef {
    std::string key, name, slot;
    int  level_req     = 0; // 限制等級
    int  primary_req   = 0; // 限制主屬性（依角色目前職業的主屬性判斷）
    int  secondary_req = 0; // 限制副屬性
    bool sellable       = true;
    int  atk_bonus      = 0;
    int  atk_speed_sec  = 60; // 武器攻速（秒／下）：30最快 45較快 60普通 70較慢 90最慢
    int  str_bonus = 0, dex_bonus = 0, int_bonus = 0, luk_bonus = 0; // 裝備提供的能力值
    std::string weapon_type; // 武器類型（法杖/拳套/匕首/弓/弩/大劍/火槍/指虎/棒子），非武器留空
    std::string job_req;     // 職業限制（一轉職業key，"" = 無限制）
    int64_t price = 0;       // 裝備商店售價（瘋幣），0 = 非商店販售
    int  item_id = 0;        // 交易用數字ID
    int  primary_generic   = 0; // 給予「穿戴者當前職業主屬性」+N（防具用，不分職業）
    int  secondary_generic = 0; // 給予「穿戴者當前職業副屬性」+N
};

// 武器：8 種類型 × 7 個等級階（等級 10/30/50/70/100/120/150）
struct MapleWeaponTypeDef {
    std::string type_cn;        // 中文武器類型（給技能武器需求比對）
    std::string type_key;       // 內部 key 片段
    std::string job_req;        // 一轉職業 key
    std::string primary_stat;   // 給予的主屬性種類（str/dex/int/luk）
    int         speed_sec;
    int         atk[7];
};
static const std::vector<MapleWeaponTypeDef> MAPLE_WEAPON_TYPES = {
    {"大劍", "gsword",  "warrior", "str", 70, {22,40,55,70,92,108,120}},
    {"法杖", "staff",   "mage",    "int", 90, {40,70,85,102,135,170,210}},
    {"拳套", "claw",    "thief",   "luk", 45, {12,24,35,46,57,64,74}},
    {"匕首", "dagger",  "thief",   "luk", 45, {25,40,54,68,90,108,118}},
    {"弩",   "xbow",    "archer",  "dex", 60, {19,35,52,67,88,103,115}},
    {"弓",   "bow",     "archer",  "dex", 60, {15,33,49,64,85,100,112}},
    {"火槍", "gun",     "pirate",  "dex", 60, {20,35,49,63,85,103,115}},
    {"指虎", "knuckle", "pirate",  "str", 60, {20,35,49,63,85,103,115}},
};
static const int   MAPLE_WPN_TIER_LV[7]    = {10, 30, 50, 70, 100, 120, 150};
static const int64_t MAPLE_WPN_TIER_PRICE[7] = {3000, 5000, 10000, 25000, 60000, 200000, 1000000};
static const char* MAPLE_WPN_TIER_NAME[7]  = {"鐵製", "精鋼", "秘銀", "山銅", "黯金", "龍鱗", "傳說"};

// ─── 防具：頭盔／套服／手套／鞋子，各 7 階（等級 10/30/50/70/100/120/150）────
// 副屬性限制 = 等級 − 10。售價／屬性由武器階價衍生（見下方註解）。
static const int MAPLE_ARMOR_TIER_LV[7] = {10, 30, 50, 70, 100, 120, 150};

struct MapleArmorDef {
    std::string slot;     // helmet / clothes / glove / shoes
    std::string slot_cn;  // 頭盔 / 套服 / 手套 / 鞋子
    int64_t price[7];     // 售價（瘋幣）
    int     primary[7];   // 給予「穿戴者主屬性」+N
    int     secondary[7]; // 給予「穿戴者副屬性」+N
};
static const std::vector<MapleArmorDef> MAPLE_ARMORS = {
    //  價格：套服 = 武器階價×0.75 進位到最高位；頭/手/鞋 = 武器階價×0.4 進位到最高位
    //  頭盔  主 = 等級/10−1   副 = 等級/10+1
    //  套服  主 = 等級/10+2   副 = 等級/10
    //  手套  主 = (等級−10)/20 副 = (等級−30)/20（皆不低於 0，整數除法）
    //  鞋子  主 = 0            副 = 等級/5
    //  slot        cn     price ── 等級      10    30    50     70     100     120      150
    { "helmet",  "頭盔", { 2000, 2000, 4000, 10000, 30000,  80000, 400000 }, { 0,  2,  4,  6,  9, 11, 14 }, { 2,  4,  6,  8, 11, 13, 16 } },
    { "clothes", "套服", { 3000, 4000, 8000, 20000, 50000, 200000, 800000 }, { 3,  5,  7,  9, 12, 14, 17 }, { 1,  3,  5,  7, 10, 12, 15 } },
    { "glove",   "手套", { 2000, 2000, 4000, 10000, 30000,  80000, 400000 }, { 0,  1,  2,  3,  4,  5,  7 }, { 0,  0,  1,  2,  3,  4,  6 } },
    { "shoes",   "鞋子", { 2000, 2000, 4000, 10000, 30000,  80000, 400000 }, { 0,  0,  0,  0,  0,  0,  0 }, { 2,  6, 10, 14, 20, 24, 30 } },
};

static const std::vector<MapleItemDef>& maple_items() {
    static const std::vector<MapleItemDef> v = []{
        std::vector<MapleItemDef> items;
        items.push_back({"wooden_sword", "新手木劍", "weapon", 0, 0, 0, false, 10, 60, 0, 0, 0, 0, "大劍", "", 0, 96600});
        int idx = 0;
        for (auto& wt : MAPLE_WEAPON_TYPES) {
            for (int t = 0; t < 7; t++) {
                MapleItemDef it;
                it.key   = "wpn_" + wt.type_key + "_" + std::to_string(MAPLE_WPN_TIER_LV[t]);
                it.name  = std::string(MAPLE_WPN_TIER_NAME[t]) + wt.type_cn;
                it.slot  = "weapon";
                it.level_req = MAPLE_WPN_TIER_LV[t];
                it.primary_req   = 0;
                it.secondary_req = MAPLE_WPN_TIER_LV[t];      // 副屬性限制 = 等級
                it.sellable      = true;
                it.atk_bonus     = wt.atk[t];
                it.atk_speed_sec = wt.speed_sec;
                int pb = MAPLE_WPN_TIER_LV[t] / 10;           // 主屬性加成 = 等級/10
                if      (wt.primary_stat == "str") it.str_bonus = pb;
                else if (wt.primary_stat == "dex") it.dex_bonus = pb;
                else if (wt.primary_stat == "int") it.int_bonus = pb;
                else                               it.luk_bonus = pb;
                it.weapon_type = wt.type_cn;
                it.job_req     = wt.job_req;
                it.price       = MAPLE_WPN_TIER_PRICE[t];
                it.item_id     = 96601 + idx * 7 + t;         // 96601..96656
                items.push_back(it);
            }
            idx++;
        }
        // 初階耳環：所有人可買，無等級／屬性／職業限制、無加成
        {
            MapleItemDef e;
            e.key = "earring_basic";
            e.name = "初階耳環";
            e.slot = "earring";
            e.sellable = true;
            e.atk_speed_sec = 60;
            e.price = 3000;
            e.item_id = 96660;
            items.push_back(e);
        }
        // 防具：頭盔／套服／手套／鞋子 × 7 階（item_id 96661..96688）
        int aidx = 0;
        for (auto& ar : MAPLE_ARMORS) {
            for (int t = 0; t < 7; t++) {
                MapleItemDef it;
                it.key   = "arm_" + ar.slot + "_" + std::to_string(MAPLE_ARMOR_TIER_LV[t]);
                it.name  = std::string(MAPLE_WPN_TIER_NAME[t]) + ar.slot_cn;
                it.slot  = ar.slot;
                it.level_req     = MAPLE_ARMOR_TIER_LV[t];
                it.secondary_req = std::max(0, MAPLE_ARMOR_TIER_LV[t] - 10); // 副屬性限制 = 等級−10
                it.sellable  = true;
                it.price     = ar.price[t];
                it.primary_generic   = ar.primary[t];
                it.secondary_generic = ar.secondary[t];
                it.item_id   = 96661 + aidx * 7 + t;
                items.push_back(it);
            }
            aidx++;
        }
        // 特殊裝備：蝸牛殼耳環，無職業／等級／屬性要求，全屬性+3（不在商店販售，price=0）
        {
            MapleItemDef e;
            e.key = "earring_snail";
            e.name = "蝸牛殼耳環";
            e.slot = "earring";
            e.sellable = true;
            e.atk_speed_sec = 60;
            e.str_bonus = 3; e.dex_bonus = 3; e.int_bonus = 3; e.luk_bonus = 3;
            e.price = 0;
            e.item_id = 96689;
            items.push_back(e);
        }
        // 特殊武器：原始人棒槌（棒子），無職業限制，等級限制10，無副屬性限制，
        // 攻擊力50、主屬性+50、副屬性+30（用 primary_generic/secondary_generic 依穿戴者當下職業判斷，不分職業都吃得到）
        // 樹妖王 1% 掉落，不在商店販售
        {
            MapleItemDef e;
            e.key = "wpn_rod_club";
            e.name = "原始人棒槌";
            e.slot = "weapon";
            e.level_req = 10;
            e.sellable = true;
            e.atk_bonus = 50;
            e.atk_speed_sec = 60;
            e.weapon_type = "棒子";
            e.primary_generic = 50;
            e.secondary_generic = 30;
            e.price = 0;
            e.item_id = 96690;
            items.push_back(e);
        }
        return items;
    }();
    return v;
}
#define MAPLE_ITEMS maple_items()

static const int MAPLE_ATK_SPEED_DEFAULT_SEC = 60; // 未裝備武器時的預設攻速（普通）

// ─── 卷軸商店 ───────────────────────────────────────────────────────────────
// 目前只做「販售」，用瘋幣購買。卷軸的實際使用（強化裝備）邏輯在 maple.h。

struct MapleScrollDef {
    std::string key, name;
    int         rate;        // 成功率 %
    int64_t     price;       // 瘋幣
    std::string applies_to;  // 顯示用：適用的部位／武器類型
    int         primary_bonus   = 0; // 主屬性
    int         secondary_bonus = 0; // 副屬性
    int         atk_bonus       = 0; // 攻擊力
    int         item_id         = 0; // 交易用數字ID
    int         explode_pct     = 0; // 詛咒卷軸專用：失敗中有這麼多%機率裝備直接爆炸消失（0＝一般卷軸，失敗只是沒效果）
    bool        shop            = true; // 是否顯示在卷軸商店（false＝不上架，只能用其他方式取得，仍可交易/使用）
};

static const std::vector<MapleScrollDef> MAPLE_SCROLLS = {
    // 防具
    {"sc_armor_main100", "頭盔／套服／鞋子 主屬性卷軸 100%", 100,  5000, "頭盔／套服／鞋子", 2, 0, 0, 96501},
    {"sc_glove_sec100",  "手套 副屬性卷軸 100%",             100,  3000, "手套",             0, 3, 0, 96502},
    {"sc_glove_atk100",  "手套 攻擊力卷軸 100%",             100, 20000, "手套",             0, 1, 1, 96503},
    {"sc_earring_ms100", "耳環 主副屬性卷軸 100%",           100, 10000, "耳環",             1, 2, 0, 96504},
    {"sc_earring_ms20",  "耳環 主副屬性卷軸 20%",            20,  50000, "耳環",             3, 5, 0, 96505},
    // 頭盔／鞋子／套服 主屬性卷軸60%：主屬性+5（樹妖王掉落，不上架商店）
    {"sc_helmet60",  "頭盔主屬性卷軸 60%", 60, 20000, "頭盔", 5, 0, 0, 96534, 0, false},
    {"sc_shoes60",   "鞋子主屬性卷軸 60%", 60, 20000, "鞋子", 5, 0, 0, 96535, 0, false},
    {"sc_clothes60", "套服主屬性卷軸 60%", 60, 20000, "套服", 5, 0, 0, 96536, 0, false},
    // 武器（各武器類型專屬）
    {"sc_wpn_staff",   "法杖 攻擊力卷軸 100%", 100, 7000, "法杖", 1, 0, 1, 96506},
    {"sc_wpn_claw",    "拳套 攻擊力卷軸 100%", 100, 7000, "拳套", 1, 0, 1, 96507},
    {"sc_wpn_dagger",  "匕首 攻擊力卷軸 100%", 100, 7000, "匕首", 1, 0, 1, 96508},
    {"sc_wpn_bow",     "弓 攻擊力卷軸 100%",   100, 7000, "弓",   1, 0, 1, 96509},
    {"sc_wpn_xbow",    "弩 攻擊力卷軸 100%",   100, 7000, "弩",   1, 0, 1, 96510},
    {"sc_wpn_gsword",  "大劍 攻擊力卷軸 100%", 100, 7000, "大劍", 1, 0, 1, 96511},
    {"sc_wpn_gun",     "火槍 攻擊力卷軸 100%", 100, 7000, "火槍", 1, 0, 1, 96512},
    {"sc_wpn_knuckle", "指虎 攻擊力卷軸 100%", 100, 7000, "指虎", 1, 0, 1, 96513},
    {"sc_wpn_rod",     "棒子 攻擊力卷軸 100%", 100, 5000, "棒子", 5, 0, 3, 96514},
    // 武器（除了棒子）60% 攻擊力卷軸：攻擊+2、主屬性+3（不上架商店，另外取得）
    {"sc_wpn_staff60",   "法杖 攻擊力卷軸 60%", 60, 12000, "法杖", 3, 0, 2, 96515, 0, false},
    {"sc_wpn_claw60",    "拳套 攻擊力卷軸 60%", 60, 12000, "拳套", 3, 0, 2, 96516, 0, false},
    {"sc_wpn_dagger60",  "匕首 攻擊力卷軸 60%", 60, 12000, "匕首", 3, 0, 2, 96517, 0, false},
    {"sc_wpn_bow60",     "弓 攻擊力卷軸 60%",   60, 12000, "弓",   3, 0, 2, 96518, 0, false},
    {"sc_wpn_xbow60",    "弩 攻擊力卷軸 60%",   60, 12000, "弩",   3, 0, 2, 96519, 0, false},
    {"sc_wpn_gsword60",  "大劍 攻擊力卷軸 60%", 60, 12000, "大劍", 3, 0, 2, 96520, 0, false},
    {"sc_wpn_gun60",     "火槍 攻擊力卷軸 60%", 60, 12000, "火槍", 3, 0, 2, 96521, 0, false},
    {"sc_wpn_knuckle60", "指虎 攻擊力卷軸 60%", 60, 12000, "指虎", 3, 0, 2, 96522, 0, false},
    // 武器（除了棒子）20% 攻擊力卷軸：攻擊+5、主屬性+5、副屬性+1（不上架商店，另外取得）
    {"sc_wpn_staff20",   "法杖 攻擊力卷軸 20%", 20, 45000, "法杖", 5, 1, 5, 96523, 0, false},
    {"sc_wpn_claw20",    "拳套 攻擊力卷軸 20%", 20, 45000, "拳套", 5, 1, 5, 96524, 0, false},
    {"sc_wpn_dagger20",  "匕首 攻擊力卷軸 20%", 20, 45000, "匕首", 5, 1, 5, 96525, 0, false},
    {"sc_wpn_bow20",     "弓 攻擊力卷軸 20%",   20, 45000, "弓",   5, 1, 5, 96526, 0, false},
    {"sc_wpn_xbow20",    "弩 攻擊力卷軸 20%",   20, 45000, "弩",   5, 1, 5, 96527, 0, false},
    {"sc_wpn_gsword20",  "大劍 攻擊力卷軸 20%", 20, 45000, "大劍", 5, 1, 5, 96528, 0, false},
    {"sc_wpn_gun20",     "火槍 攻擊力卷軸 20%", 20, 45000, "火槍", 5, 1, 5, 96529, 0, false},
    {"sc_wpn_knuckle20", "指虎 攻擊力卷軸 20%", 20, 45000, "指虎", 5, 1, 5, 96530, 0, false},
    // 耳環詛咒卷軸：效果等同耳環主副屬性卷軸20%，但機率是 50%成功／25%失敗／25%爆炸（裝備直接消失）（不上架商店，另外取得）
    {"sc_earring_curse50", "耳環詛咒卷軸 50%", 50, 40000, "耳環", 3, 5, 0, 96531, 25, false},
    // 棒子攻擊力詛咒卷軸：50%成功／25%失敗／25%爆炸（裝備直接消失），攻擊+10、主屬性+12、副屬性+5（樹妖王掉落，不上架商店）
    {"sc_wpn_rod_curse50", "棒子攻擊力詛咒卷軸 50%", 50, 60000, "棒子", 12, 5, 10, 96532, 25, false},
    // 棒子攻擊力卷軸60%：攻擊+5、主屬性+8（樹妖王掉落，不上架商店）
    {"sc_wpn_rod60", "棒子攻擊力卷軸 60%", 60, 25000, "棒子", 8, 0, 5, 96533, 0, false},
};
