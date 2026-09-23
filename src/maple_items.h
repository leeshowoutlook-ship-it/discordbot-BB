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
    std::string weapon_type; // 武器類型（法杖/拳套/匕首/弓/弩/大劍/棒子），非武器留空
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
    {"弓",   "bow",     "archer",  "dex", 60, {13,29,43,56,75,88,99}},
};
static const int   MAPLE_WPN_TIER_LV[7]    = {10, 30, 50, 70, 100, 120, 150};
static const int64_t MAPLE_WPN_TIER_PRICE[7] = {3000, 5000, 10000, 25000, 60000, 200000, 1000000};
static const char* MAPLE_WPN_TIER_NAME[7]  = {"鐵製", "精鋼", "秘銀", "山銅", "黯金", "龍鱗", "傳說"};

// ─── 防具：頭盔／套服／手套／鞋子，各 7 階（等級 10/30/50/70/100/120/150）────
// 副屬性限制 = 等級 − 10。售價／屬性由武器階價衍生（見下方註解）。
static const int MAPLE_ARMOR_TIER_LV[7] = {10, 30, 50, 70, 100, 120, 150};

struct MapleArmorDef {
    std::string slot;     // helmet / clothes / glove / shoes / ring / necklace
    std::string slot_cn;  // 頭盔 / 套服 / 手套 / 鞋子 / 戒指 / 項鍊
    int64_t price[7];     // 售價（瘋幣）
    int     primary[7];   // 給予「穿戴者主屬性」+N
    int     secondary[7]; // 給予「穿戴者副屬性」+N
    int     id_base;      // 這個部位7階裝備的起始 item_id（id_base ~ id_base+6），固定寫死避免加新部位時跟其他道具ID撞號
};
static const std::vector<MapleArmorDef> MAPLE_ARMORS = {
    //  價格：套服 = 武器階價×0.75 進位到最高位；戒指=×0.5、項鍊=×0.6；頭/手/鞋 = 武器階價×0.4 進位到最高位
    //  頭盔  主 = 等級/10−1   副 = 等級/10+1
    //  套服  主 = 等級/10+2   副 = 等級/10
    //  手套  主 = (等級−10)/20 副 = (等級−30)/20（皆不低於 0，整數除法）
    //  鞋子  主 = 0            副 = 等級/5
    //  戒指  主 = 等級/10      副 = 等級/10
    //  項鍊  主 = 等級/10+3    副 = 等級/10+2
    //  slot        cn     price ── 等級      10    30    50     70     100     120      150
    { "helmet",   "頭盔", { 2000, 2000, 4000, 10000, 30000,  80000, 400000 }, { 0,  2,  4,  6,  9, 11, 14 }, { 2,  4,  6,  8, 11, 13, 16 }, 96661 },
    { "clothes",  "套服", { 3000, 4000, 8000, 20000, 50000, 200000, 800000 }, { 3,  5,  7,  9, 12, 14, 17 }, { 1,  3,  5,  7, 10, 12, 15 }, 96668 },
    { "glove",    "手套", { 2000, 2000, 4000, 10000, 30000,  80000, 400000 }, { 0,  1,  2,  3,  4,  5,  7 }, { 0,  0,  1,  2,  3,  4,  6 }, 96675 },
    { "shoes",    "鞋子", { 2000, 2000, 4000, 10000, 30000,  80000, 400000 }, { 0,  0,  0,  0,  0,  0,  0 }, { 2,  6, 10, 14, 20, 24, 30 }, 96682 },
    { "ring",     "戒指", { 2000, 3000, 5000, 20000, 30000, 100000, 500000 }, { 1,  3,  5,  7, 10, 12, 15 }, { 1,  3,  5,  7, 10, 12, 15 }, 96710 },
    { "necklace", "項鍊", { 2000, 3000, 6000, 20000, 40000, 200000, 600000 }, { 4,  6,  8, 10, 13, 15, 18 }, { 3,  5,  7,  9, 12, 14, 17 }, 96717 },
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
        // 防具：頭盔／套服／手套／鞋子／戒指／項鍊 × 7 階（item_id 各自對應 ar.id_base ~ +6）
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
                it.item_id   = ar.id_base + t;
                items.push_back(it);
            }
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
        // 冥界武器系列：8種一般武器類型都各一把（不含棒子），等級限制20、副屬性限制20，
        // 攻擊力＝10等裝與30等裝的平均、再依武器各自加一點；主屬性+5（吃該武器類型原本對應的那個屬性）。
        // 冥界幽靈（地鐵三號站）掉落，不在商店販售。
        {
            struct UwWpn { std::string type_cn, type_key, job_req, stat; int speed; int atk; int item_id; };
            static const std::vector<UwWpn> UW_WPNS = {
                {"大劍", "gsword",  "warrior", "str", 70, 34, 96691},
                {"法杖", "staff",   "mage",    "int", 90, 58, 96692},
                {"拳套", "claw",    "thief",   "luk", 45, 21, 96693},
                {"匕首", "dagger",  "thief",   "luk", 45, 36, 96694},
                {"弩",   "xbow",    "archer",  "dex", 60, 30, 96695},
                {"弓",   "bow",     "archer",  "dex", 60, 27, 96696},
            };
            for (auto& w : UW_WPNS) {
                MapleItemDef e;
                e.key = "wpn_underworld_" + w.type_key;
                e.name = "冥界" + w.type_cn;
                e.slot = "weapon";
                e.level_req = 20;
                e.secondary_req = 20;
                e.sellable = true;
                e.atk_bonus = w.atk;
                e.atk_speed_sec = w.speed;
                e.weapon_type = w.type_cn;
                e.job_req = w.job_req;
                if      (w.stat == "str") e.str_bonus = 5;
                else if (w.stat == "dex") e.dex_bonus = 5;
                else if (w.stat == "int") e.int_bonus = 5;
                else                      e.luk_bonus = 5;
                e.price = 0;
                e.item_id = w.item_id;
                items.push_back(e);
            }
        }
        // 冥界套服：等級限制20、無屬性限制，主屬性+0、副屬性+10。冥界幽靈掉落，不在商店販售。
        {
            MapleItemDef e;
            e.key = "arm_underworld_clothes";
            e.name = "冥界套服";
            e.slot = "clothes";
            e.level_req = 20;
            e.sellable = true;
            e.atk_speed_sec = 60;
            e.secondary_generic = 10;
            e.price = 0;
            e.item_id = 96699;
            items.push_back(e);
        }
        // 黃金杖：法杖，限制等級40、限制副屬性20，攻擊力80、主屬性(智力)+5。殭屍猴王5%掉落，不在商店販售。
        {
            MapleItemDef e;
            e.key = "wpn_golden_staff";
            e.name = "黃金杖";
            e.slot = "weapon";
            e.level_req = 40;
            e.secondary_req = 20;
            e.sellable = true;
            e.atk_bonus = 80;
            e.atk_speed_sec = 90;
            e.weapon_type = "法杖";
            e.job_req = "mage";
            e.int_bonus = 5;
            e.price = 0;
            e.item_id = 96701;
            items.push_back(e);
        }
        // 狼牙：拳套，限制等級40、限制副屬性30，攻擊力30、主屬性(幸運)+7。裝備這把武器時「雙飛閃」攻速額外快一階（見 maple_atk_speed_sec）。
        // 雪山巨狼5%掉落，不在商店販售。
        {
            MapleItemDef e;
            e.key = "wpn_wolf_fang";
            e.name = "狼牙";
            e.slot = "weapon";
            e.level_req = 40;
            e.secondary_req = 30;
            e.sellable = true;
            e.atk_bonus = 30;
            e.atk_speed_sec = 45;
            e.weapon_type = "拳套";
            e.job_req = "thief";
            e.luk_bonus = 7;
            e.price = 0;
            e.item_id = 96724;
            items.push_back(e);
        }
        // 螃蟹鉗：手套，無職業限制、無副屬性限制，攻擊力+1。寄居蟹 0.5% 掉落，不在商店販售。
        {
            MapleItemDef e;
            e.key = "arm_crab_claw";
            e.name = "螃蟹鉗";
            e.slot = "glove";
            e.sellable = true;
            e.atk_bonus = 3;
            e.atk_speed_sec = 60;
            e.price = 0;
            e.item_id = 96725;
            items.push_back(e);
        }
        // 狼牙項鏈：項鍊，無職業限制、限制等級30、無副屬性限制，全屬性(力/敏/智/幸)各+12。
        // 冰雪狼王掉落，不在商店販售。
        {
            MapleItemDef e;
            e.key = "necklace_wolf_fang";
            e.name = "狼牙項鏈";
            e.slot = "necklace";
            e.level_req = 30;
            e.sellable = true;
            e.atk_speed_sec = 60;
            e.str_bonus = 12; e.dex_bonus = 12; e.int_bonus = 12; e.luk_bonus = 12;
            e.price = 0;
            e.item_id = 96726;
            items.push_back(e);
        }
        // 狼王象徵：材料道具，放背包「其他」分頁，目前無作用。冰雪狼王掉落，不在商店販售。
        {
            MapleItemDef e;
            e.key = "mat_wolf_king_token";
            e.name = "狼王象徵";
            e.slot = "material";
            e.sellable = true;
            e.atk_speed_sec = 60;
            e.price = 0;
            e.item_id = 96727;
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
    bool        restore_slot    = false; // 純白卷軸專用：只能用在「已有強化失敗紀錄」的裝備，不佔用強化次數上限，成功時歸還1次已使用次數
    int         str_bonus = 0, dex_bonus = 0, int_bonus = 0, luk_bonus = 0; // 「全屬性」卷軸專用：不分職業，四維各自加
};

static const std::vector<MapleScrollDef> MAPLE_SCROLLS = {
    // 防具
    {"sc_armor_main100", "頭盔／套服／鞋子 主屬性卷軸 100%", 100,  5000, "頭盔／套服／鞋子", 2, 0, 0, 96501},
    {"sc_glove_sec100",  "手套 副屬性卷軸 100%",             100,  3000, "手套",             0, 3, 0, 96502},
    {"sc_glove_atk100",  "手套 攻擊力卷軸 100%",             100, 20000, "手套",             0, 1, 1, 96503},
    {"sc_earring_ms100", "耳環 主副屬性卷軸 100%",           100, 10000, "耳環",             1, 2, 0, 96504},
    {"sc_earring_ms20",  "耳環 主副屬性卷軸 20%",            20,  50000, "耳環",             3, 5, 0, 96505},
    // 耳環主副屬性卷軸60%：介於100%(+1,+2)與20%(+3,+5)之間（不上架商店）
    {"sc_earring60", "耳環主副屬性卷軸 60%", 60, 25000, "耳環", 2, 3, 0, 96550, 0, false},
    // 頭盔／鞋子／套服 主屬性卷軸60%：主屬性+5（樹妖王掉落，不上架商店）
    {"sc_helmet60",  "頭盔主屬性卷軸 60%", 60, 20000, "頭盔", 5, 0, 0, 96534, 0, false},
    {"sc_shoes60",   "鞋子主屬性卷軸 60%", 60, 20000, "鞋子", 5, 0, 0, 96535, 0, false},
    {"sc_clothes60", "套服主屬性卷軸 60%", 60, 20000, "套服", 5, 0, 0, 96536, 0, false},
    // 頭盔/套服/鞋子 主屬性卷軸20%：主屬性+8（不上架商店）
    {"sc_helmet20",  "頭盔主屬性卷軸 20%", 20, 55000, "頭盔", 8, 0, 0, 96551, 0, false},
    {"sc_shoes20",   "鞋子主屬性卷軸 20%", 20, 55000, "鞋子", 8, 0, 0, 96552, 0, false},
    {"sc_clothes20", "套服主屬性卷軸 20%", 20, 55000, "套服", 8, 0, 0, 96553, 0, false},
    // 頭盔/套服/鞋子 主屬性詛咒卷軸50%：效果等同20%，50%成功／25%失敗／25%爆炸（不上架商店）
    {"sc_helmet_curse50",  "頭盔主屬性詛咒卷軸 50%", 50, 47000, "頭盔", 8, 0, 0, 96554, 25, false},
    {"sc_shoes_curse50",   "鞋子主屬性詛咒卷軸 50%", 50, 47000, "鞋子", 8, 0, 0, 96555, 25, false},
    {"sc_clothes_curse50", "套服主屬性詛咒卷軸 50%", 50, 47000, "套服", 8, 0, 0, 96556, 25, false},
    // 武器（各武器類型專屬）
    {"sc_wpn_staff",   "法杖 攻擊力卷軸 100%", 100, 7000, "法杖", 1, 0, 1, 96506},
    {"sc_wpn_claw",    "拳套 攻擊力卷軸 100%", 100, 7000, "拳套", 1, 0, 1, 96507},
    {"sc_wpn_dagger",  "匕首 攻擊力卷軸 100%", 100, 7000, "匕首", 1, 0, 1, 96508},
    {"sc_wpn_bow",     "弓 攻擊力卷軸 100%",   100, 7000, "弓",   1, 0, 1, 96509},
    {"sc_wpn_xbow",    "弩 攻擊力卷軸 100%",   100, 7000, "弩",   1, 0, 1, 96510},
    {"sc_wpn_gsword",  "大劍 攻擊力卷軸 100%", 100, 7000, "大劍", 1, 0, 1, 96511},
    {"sc_wpn_rod",     "棒子 攻擊力卷軸 100%", 100, 5000, "棒子", 5, 0, 3, 96514},
    // 武器（除了棒子）60% 攻擊力卷軸：攻擊+2、主屬性+3（不上架商店，另外取得）
    {"sc_wpn_staff60",   "法杖 攻擊力卷軸 60%", 60, 12000, "法杖", 3, 0, 2, 96515, 0, false},
    {"sc_wpn_claw60",    "拳套 攻擊力卷軸 60%", 60, 12000, "拳套", 3, 0, 2, 96516, 0, false},
    {"sc_wpn_dagger60",  "匕首 攻擊力卷軸 60%", 60, 12000, "匕首", 3, 0, 2, 96517, 0, false},
    {"sc_wpn_bow60",     "弓 攻擊力卷軸 60%",   60, 12000, "弓",   3, 0, 2, 96518, 0, false},
    {"sc_wpn_xbow60",    "弩 攻擊力卷軸 60%",   60, 12000, "弩",   3, 0, 2, 96519, 0, false},
    {"sc_wpn_gsword60",  "大劍 攻擊力卷軸 60%", 60, 12000, "大劍", 3, 0, 2, 96520, 0, false},
    // 武器（除了棒子）20% 攻擊力卷軸：攻擊+5、主屬性+5、副屬性+1（不上架商店，另外取得）
    {"sc_wpn_staff20",   "法杖 攻擊力卷軸 20%", 20, 45000, "法杖", 5, 1, 5, 96523, 0, false},
    {"sc_wpn_claw20",    "拳套 攻擊力卷軸 20%", 20, 45000, "拳套", 5, 1, 5, 96524, 0, false},
    {"sc_wpn_dagger20",  "匕首 攻擊力卷軸 20%", 20, 45000, "匕首", 5, 1, 5, 96525, 0, false},
    {"sc_wpn_bow20",     "弓 攻擊力卷軸 20%",   20, 45000, "弓",   5, 1, 5, 96526, 0, false},
    {"sc_wpn_xbow20",    "弩 攻擊力卷軸 20%",   20, 45000, "弩",   5, 1, 5, 96527, 0, false},
    {"sc_wpn_gsword20",  "大劍 攻擊力卷軸 20%", 20, 45000, "大劍", 5, 1, 5, 96528, 0, false},
    // 耳環詛咒卷軸：效果等同耳環主副屬性卷軸20%，但機率是 50%成功／25%失敗／25%爆炸（裝備直接消失）（不上架商店，另外取得）
    {"sc_earring_curse50", "耳環詛咒卷軸 50%", 50, 40000, "耳環", 3, 5, 0, 96531, 25, false},
    // 棒子攻擊力詛咒卷軸：50%成功／25%失敗／25%爆炸（裝備直接消失），攻擊+10、主屬性+12、副屬性+5（樹妖王掉落，不上架商店）
    {"sc_wpn_rod_curse50", "棒子攻擊力詛咒卷軸 50%", 50, 60000, "棒子", 12, 5, 10, 96532, 25, false},
    // 棒子攻擊力卷軸60%：攻擊+5、主屬性+8（樹妖王掉落，不上架商店）
    {"sc_wpn_rod60", "棒子攻擊力卷軸 60%", 60, 25000, "棒子", 8, 0, 5, 96533, 0, false},
    // 武器詛咒卷軸50%（除了棒子，每種一張）：效果等同該武器20%卷軸，50%成功／25%失敗／25%爆炸（裝備直接消失）（不上架商店）
    {"sc_wpn_staff_curse50",  "法杖攻擊力詛咒卷軸 50%", 50, 40000, "法杖", 5, 1, 5, 96537, 25, false},
    {"sc_wpn_claw_curse50",   "拳套攻擊力詛咒卷軸 50%", 50, 40000, "拳套", 5, 1, 5, 96538, 25, false},
    {"sc_wpn_dagger_curse50", "匕首攻擊力詛咒卷軸 50%", 50, 40000, "匕首", 5, 1, 5, 96539, 25, false},
    {"sc_wpn_bow_curse50",    "弓攻擊力詛咒卷軸 50%",   50, 40000, "弓",   5, 1, 5, 96540, 25, false},
    {"sc_wpn_xbow_curse50",   "弩攻擊力詛咒卷軸 50%",   50, 40000, "弩",   5, 1, 5, 96541, 25, false},
    {"sc_wpn_gsword_curse50", "大劍攻擊力詛咒卷軸 50%", 50, 40000, "大劍", 5, 1, 5, 96542, 25, false},
    // 純白武器/裝備卷軸50%：任何「有強化失敗紀錄」的武器/其他裝備都能用，不佔強化次數上限，
    // 50%成功歸還1次已使用次數／50%爆炸裝備消失
    {"sc_wpn_purewhite50",   "純白武器卷軸 50%", 50, 700000, "大劍／法杖／拳套／匕首／弩／弓", 0, 0, 0, 96548, 50, true, true},
    {"sc_equip_purewhite50", "純白裝備卷軸 50%", 50, 500000, "耳環／戒指／項鍊／頭盔／手套／套服／鞋子", 0, 0, 0, 96549, 50, true, true},
    // 手套攻擊卷軸60%：攻擊+3、主屬性+5、副屬性+1（不上架商店）
    {"sc_glove_atk60", "手套攻擊卷軸 60%", 60, 18000, "手套", 5, 1, 3, 96543, 0, false},
    // 手套攻擊卷軸20%：攻擊+5、主屬性+8、副屬性+5（不上架商店）
    {"sc_glove_atk20", "手套攻擊卷軸 20%", 20, 50000, "手套", 8, 5, 5, 96544, 0, false},
    // 手套攻擊詛咒卷軸50%：效果等同手套攻擊卷軸20%，50%成功／25%失敗／25%爆炸（裝備直接消失）（不上架商店）
    {"sc_glove_atk_curse50", "手套攻擊詛咒卷軸 50%", 50, 42000, "手套", 8, 5, 5, 96545, 25, false},
    // 手套副屬性卷軸60%：副屬性+8（不上架商店）
    {"sc_glove_sec60", "手套副屬性卷軸 60%", 60, 15000, "手套", 0, 8, 0, 96546, 0, false},
    // 手套副屬性卷軸20%：副屬性+15（不上架商店）
    {"sc_glove_sec20", "手套副屬性卷軸 20%", 20, 40000, "手套", 0, 15, 0, 96547, 0, false},
    // 手套副屬性詛咒卷軸50%：效果等同副屬性20%，50%成功／25%失敗／25%爆炸（裝備直接消失）（不上架商店）
    {"sc_glove_sec_curse50", "手套副屬性詛咒卷軸 50%", 50, 34000, "手套", 0, 15, 0, 96557, 25, false},
    // 項鍊全屬性卷軸：力/敏/智/幸 四維各自加，不分職業（冰雪狼王掉落，不上架商店）
    // 20%版額外多給主屬性+2（用 primary_bonus 欄位，會依穿戴者職業對應到對的那個屬性）
    {"sc_necklace100", "項鍊全屬性卷軸 100%", 100, 15000, "項鍊", 0, 0, 0, 96558, 0, false, false, 1, 1, 1, 1},
    {"sc_necklace60",  "項鍊全屬性卷軸 60%",  60, 40000, "項鍊", 0, 0, 0, 96559, 0, false, false, 3, 3, 3, 3},
    {"sc_necklace20",  "項鍊全屬性卷軸 20%",  20, 90000, "項鍊", 2, 0, 0, 96560, 0, false, false, 5, 5, 5, 5},
};

// ─── 陣營系統 ─────────────────────────────────────────────────────────────────
// 三大陣營，玩家可任選一個加入；退出後保留該陣營的等級/經驗，之後回鍋接續。
// 陣營經驗值目前還沒有接來源（先建架構），等級/經驗公式已經定案。

struct MapleFactionDef { std::string key, name; };
static const std::vector<MapleFactionDef> MAPLE_FACTIONS = {
    {"mushroom_baby",     "菇菇寶貝"},
    {"water_spirit",      "綠水靈"},
    {"three_eye_octopus", "三眼章魚"},
};
static const int64_t MAPLE_FACTION_JOIN_FEE  = 50000; // 入陣營費
static const int64_t MAPLE_FACTION_LEAVE_FEE = 50000; // 退陣營費
