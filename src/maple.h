#pragma once
#include "monster.h"
#include <string>
#include <vector>
#include <cmath>

// ─── 楓之谷世界：養成系統 ───────────────────────────────────────────────────
// 每位玩家都有一個獨立的楓之谷角色（等級／經驗值／瘋幣／能力值／職業）。
// 瘋幣與經驗值都獨立於現有的籌碼／寵物系統之外，是這個養成系統自己的一套經濟。
// 經驗值取得方式另外規劃中，這裡先只提供角色資料、升級曲線、能力值分配與轉職。

// ─── 升級曲線 ───────────────────────────────────────────────────────────────
// 第1~30級（升到第2~31級）所需經驗值為固定表；第31級起每級是前一級的1.05倍。
// 等級上限暫定70。

static const std::vector<int64_t> MAPLE_EXP_TABLE = {
    15, 34, 57, 92, 135, 372, 560, 840, 1242, 1716,
    2360, 3216, 4200, 5460, 7050, 8840, 11040, 13716, 16680, 20216,
    24402, 28980, 34320, 40512, 47216, 54900, 63666, 73080, 83720, 95700,
};
static const int MAPLE_LEVEL_CAP = 30; // 暫定：剛好能點滿初心者＋一轉技能

static int64_t maple_exp_to_next(int level) {
    if (level < 1) level = 1;
    if (level <= (int)MAPLE_EXP_TABLE.size()) return MAPLE_EXP_TABLE[level - 1];
    double v = (double)MAPLE_EXP_TABLE.back();
    for (int lv = (int)MAPLE_EXP_TABLE.size() + 1; lv <= level; lv++) v *= 1.05;
    return (int64_t)llround(v);
}

// 套用經驗值並處理連續升級（升級不直接加能力值，改用可分配點數），回傳升了幾級
static int maple_apply_exp(MapleCharacter& c, int64_t exp_gain) {
    c.exp += exp_gain;
    int levels = 0;
    while (c.level < MAPLE_LEVEL_CAP && c.exp >= maple_exp_to_next(c.level)) {
        c.exp -= maple_exp_to_next(c.level);
        c.level++;
        levels++;
    }
    return levels;
}

// ─── 能力值分配 ─────────────────────────────────────────────────────────────
// 基礎每項能力值 4 點，每升一級可自由分配 5 點。

static int maple_total_ap(const MapleCharacter& c) { return (c.level - 1) * 5; }
static int maple_spent_ap(const MapleCharacter& c) {
    return (c.str_stat - 4) + (c.dex_stat - 4) + (c.int_stat - 4) + (c.luk_stat - 4);
}
static int maple_unspent_ap(const MapleCharacter& c) { return maple_total_ap(c) - maple_spent_ap(c); }

// 自己分配的能力值（基礎4 + 已投入點數）
static int maple_stat_alloc(const MapleCharacter& c, const std::string& stat) {
    if (stat == "str") return c.str_stat;
    if (stat == "dex") return c.dex_stat;
    if (stat == "int") return c.int_stat;
    return c.luk_stat;
}
static std::string maple_stat_name(const std::string& stat) {
    if (stat == "str") return "力量";
    if (stat == "dex") return "敏捷";
    if (stat == "int") return "智力";
    return "幸運";
}

// ─── 職業定義 ───────────────────────────────────────────────────────────────
// tier 0=初心者／1=一轉／2=二轉。攻擊力算法：(主屬性係數*主屬性 + 副屬性係數*副屬性) × 基礎攻擊力

struct MapleJobDef {
    std::string key, name, parent;
    int tier;
    std::string primary_stat, secondary_stat;
    double primary_coef, secondary_coef;
};

static const std::vector<MapleJobDef> MAPLE_JOBS = {
    {"beginner",     "初心者",   "",       0, "str","dex", 3.2, 1.0},
    {"warrior",      "劍士",     "",       1, "str","dex", 4.8, 1.0},
    {"mage",         "法師",     "",       1, "int","luk", 4.4, 1.0},
    {"thief",        "盜賊",     "",       1, "luk","dex", 3.6, 1.0},
    {"archer",       "弓箭手",   "",       1, "dex","str", 4.2, 1.0},
    {"pirate",       "海盜",     "",       1, "str","dex", 2.0, 2.0},
    {"berserker",    "狂戰士",   "warrior",2, "str","dex", 4.8, 1.0},
    {"page",         "見習騎士", "warrior",2, "str","dex", 4.8, 1.0},
    {"icelightning", "冰雷",     "mage",   2, "int","luk", 4.4, 1.0},
    {"priest",       "僧侶",     "mage",   2, "int","luk", 4.4, 1.0},
    {"assassin",     "刺客",     "thief",  2, "luk","dex", 3.6, 1.0},
    {"bandit",       "俠盜",     "thief",  2, "luk","dex", 3.6, 1.0},
    {"hunter",       "獵人",     "archer", 2, "dex","str", 4.2, 1.0},
    {"crossbowman",  "弩弓手",   "archer", 2, "dex","str", 4.2, 1.0},
    {"brawler",      "打手",     "pirate", 2, "str","dex", 4.0, 1.0},
    {"gunslinger",   "槍手",     "pirate", 2, "dex","str", 4.0, 1.0},
};

static const MapleJobDef* maple_find_job(const std::string& key) {
    for (auto& j : MAPLE_JOBS) if (j.key == key) return &j;
    return nullptr;
}

static const MapleJobDef& maple_job_of(const MapleCharacter& c) {
    const MapleJobDef* j = maple_find_job(c.job);
    return j ? *j : *maple_find_job("beginner");
}

static std::vector<const MapleJobDef*> maple_first_jobs() {
    std::vector<const MapleJobDef*> out;
    for (auto& j : MAPLE_JOBS) if (j.tier == 1) out.push_back(&j);
    return out;
}
static std::vector<const MapleJobDef*> maple_second_jobs(const std::string& first_job_key) {
    std::vector<const MapleJobDef*> out;
    for (auto& j : MAPLE_JOBS) if (j.tier == 2 && j.parent == first_job_key) out.push_back(&j);
    return out;
}

static bool maple_can_first_job(const MapleCharacter& c) {
    return c.level >= 10 && c.job == "beginner";
}
static bool maple_can_second_job(const MapleCharacter& c) {
    return c.level >= 30 && maple_job_of(c).tier == 1;
}

// ─── 技能 ───────────────────────────────────────────────────────────────────
// 每升一級給1點技能點。初心者3個技能各上限3點（剛好9點點滿）；
// 一轉起每個職業2個技能各上限10點。

struct MapleSkillDef {
    std::string key, name, desc, job;
    int max_level;
    std::string type; // "damage_fixed"（固定傷害）/"damage_coef"（技能係數%）/"buff_pct"（效果%）/"unlock"（滿級解鎖功能）
    std::vector<double> values; // 每級數值，index 0 = 1級
    std::string unlock_feature; // type=="unlock" 專用："equip" 或 "ap"
    std::vector<std::string> weapon_req; // 攻擊技能：需裝備的武器類型（任一即可）；空＝無限制
    int hits = 1;               // 攻擊技能一次動作打幾下（傷害 ×hits）
};

static const std::vector<MapleSkillDef> MAPLE_SKILLS = {
    // 初心者
    {"baby_throw", "嫩寶投擲術", "將傷害固定為所示數值", "beginner", 3, "damage_fixed", {10,20,30}, "", {}},
    {"free_equip", "換裝自由",   "點滿三點後開放裝備系統", "beginner", 3, "unlock", {}, "equip", {}},
    {"free_ap",    "能力值自由", "點滿三點後開放能力值系統", "beginner", 3, "unlock", {}, "ap", {}},
    // 盜賊
    {"double_throw", "雙飛閃", "快速投擲三枚飛鏢攻擊敵人（每下套用技能係數，共 3 下）", "thief", 10, "damage_coef",
        {100,108,116,124,130,135,140,144,148,150}, "", {"拳套"}, 3},
    {"haste", "速度激發", "自己與周圍玩家的攻擊間隔縮短（每級 -0.1 秒，滿級 -1 秒）", "thief", 10, "buff_pct",
        {1,2,3,4,5,6,7,8,9,10}, "", {}},
    // 法師
    {"magic_claw", "魔力爪", "運用自身魔力化成尖銳的爪擊攻擊敵人，技能係數如上", "mage", 10, "damage_coef",
        {120,140,160,175,180,190,200,210,220,230}, "", {"法杖"}},
    {"teleport", "瞬間移動", "自己的攻擊間隔縮短（每級 -0.2 秒，滿級 -2 秒）", "mage", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    {"rage_slash", "魔天一擊", "粗暴地揮舞大劍攻擊敵人，技能係數如上", "warrior", 10, "damage_coef",
        {170,190,210,230,250,270,290,310,330,350}, "", {"大劍"}},
    {"endurance", "自身強化", "冒險時擊殺後的休息時間縮短（每級 -2%，滿級 -20%）", "warrior", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    // 海盜
    {"impact_punch", "衝擊拳", "粗暴地揮舞拳頭攻擊敵人，技能係數如上", "pirate", 10, "damage_coef",
        {162,174,186,198,210,222,234,246,258,270}, "", {"指虎"}},
    {"charge", "衝鋒", "自己的攻擊間隔 -0.5~-5 秒、休息時間 -0.5%~-5%（滿級）", "pirate", 10, "buff_pct",
        {0.5,1.0,1.5,2.0,2.5,3.0,3.5,4.0,4.5,5.0}, "", {}},
    // 弓箭手
    {"double_shot", "二連箭", "發射特製的箭矢射擊敵人，技能係數如上", "archer", 10, "damage_coef",
        {196,202,208,214,220,226,234,240,250,260}, "", {"弓", "弩"}},
    {"eagle_eye", "霸王箭", "提升爆擊率，爆擊造成 2 倍傷害（每級 +4%，滿級 +40%）", "archer", 10, "buff_pct",
        {4,8,12,16,20,24,28,32,36,40}, "", {}},
};

static const MapleSkillDef* maple_find_skill(const std::string& key) {
    for (auto& s : MAPLE_SKILLS) if (s.key == key) return &s;
    return nullptr;
}
static std::vector<const MapleSkillDef*> maple_skills_for_job(const std::string& job_key) {
    std::vector<const MapleSkillDef*> out;
    for (auto& s : MAPLE_SKILLS) if (s.job == job_key) out.push_back(&s);
    return out;
}
static int maple_skill_level(const MapleCharacter& c, const std::string& key) {
    auto it = c.skill_levels.find(key);
    return it != c.skill_levels.end() ? it->second : 0;
}
static bool maple_skill_maxed(const MapleCharacter& c, const std::string& key) {
    const MapleSkillDef* sd = maple_find_skill(key);
    return sd && maple_skill_level(c, key) >= sd->max_level;
}
static bool maple_equip_unlocked(const MapleCharacter& c) { return maple_skill_maxed(c, "free_equip"); }
static bool maple_ap_unlocked(const MapleCharacter& c)    { return maple_skill_maxed(c, "free_ap"); }

static int maple_sp_total(const MapleCharacter& c) { return c.level - 1; }
static int maple_sp_spent(const MapleCharacter& c) {
    int total = 0;
    for (auto& [k, v] : c.skill_levels) total += v;
    return total;
}
static int maple_sp_unspent(const MapleCharacter& c) { return maple_sp_total(c) - maple_sp_spent(c); }

// 轉職後仍要看得到之前職業的技能：一律顯示初心者技能，再加上目前一轉職業的技能
// （二轉職業目前尚無技能資料，沿用其一轉父職業的技能）
static std::vector<std::string> maple_visible_skill_jobs(const MapleCharacter& c) {
    std::vector<std::string> out = {"beginner"};
    const MapleJobDef& j = maple_job_of(c);
    if (j.tier == 1) out.push_back(j.key);
    else if (j.tier == 2) out.push_back(j.parent);
    return out;
}
static bool maple_skill_visible(const MapleCharacter& c, const std::string& skill_job) {
    for (auto& vj : maple_visible_skill_jobs(c)) if (vj == skill_job) return true;
    return false;
}

// ─── 裝備欄位 ───────────────────────────────────────────────────────────────
// 顯示／更換順序固定：武器、耳環、頭盔、手套、套服、鞋子

struct MapleSlotDef { std::string key, name, icon; };
static const std::vector<MapleSlotDef> MAPLE_SLOTS = {
    {"weapon",  "武器", "⚔️"},
    {"earring", "耳環", "💍"},
    {"helmet",  "頭盔", "🪖"},
    {"glove",   "手套", "🧤"},
    {"clothes", "套服", "👘"},
    {"shoes",   "鞋子", "👟"},
};

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
    {"指虎", "knuckle", "pirate",  "str", 70, {20,35,49,63,85,103,115}},
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
        return items;
    }();
    return v;
}
#define MAPLE_ITEMS maple_items()

static const int MAPLE_ATK_SPEED_DEFAULT_SEC = 60; // 未裝備武器時的預設攻速（普通）

static std::string maple_atk_speed_name(int sec) {
    if (sec <= 30) return "最快";
    if (sec <= 45) return "較快";
    if (sec <= 60) return "普通";
    if (sec <= 70) return "較慢";
    return "最慢";
}

static const MapleItemDef* maple_find_item(const std::string& key) {
    if (key.empty()) return nullptr;
    for (auto& it : MAPLE_ITEMS) if (it.key == key) return &it;
    return nullptr;
}
static const MapleItemDef* maple_find_item_by_id(int id) {
    if (!id) return nullptr;
    for (auto& it : MAPLE_ITEMS) if (it.item_id == id) return &it;
    return nullptr;
}

// ─── 強化裝備實例 ─────────────────────────────────────────────────────────────
// eq_* 欄位存 "#<id>" 代表指向 enh_items 中的強化實例，否則是純裝備 key。
static bool maple_eq_is_enh(const std::string& s) { return s.size() > 1 && s[0] == '#'; }
static int  maple_eq_enh_id(const std::string& s) { return maple_eq_is_enh(s) ? atoi(s.c_str() + 1) : 0; }

static const MapleEnhItem* maple_find_enh(const MapleCharacter& c, int id) {
    if (id <= 0) return nullptr;
    for (auto& e : c.enh_items) if (e.id == id) return &e;
    return nullptr;
}
static MapleEnhItem* maple_find_enh(MapleCharacter& c, int id) {
    if (id <= 0) return nullptr;
    for (auto& e : c.enh_items) if (e.id == id) return &e;
    return nullptr;
}

// 某部位存的原始字串（可能是 "#id" 或裝備 key 或空）
static std::string maple_equipped_raw(const MapleCharacter& c, const std::string& slot) {
    if (slot == "weapon")  return c.eq_weapon;
    if (slot == "earring") return c.eq_earring;
    if (slot == "helmet")  return c.eq_helmet;
    if (slot == "glove")   return c.eq_glove;
    if (slot == "clothes") return c.eq_clothes;
    if (slot == "shoes")   return c.eq_shoes;
    return "";
}
// 某部位目前裝備的「基礎裝備 key」（強化實例會解析回 base_key）
static std::string maple_equipped_key(const MapleCharacter& c, const std::string& slot) {
    std::string s = maple_equipped_raw(c, slot);
    if (maple_eq_is_enh(s)) {
        const MapleEnhItem* e = maple_find_enh(c, maple_eq_enh_id(s));
        return e ? e->base_key : std::string();
    }
    return s;
}
// 某部位目前裝備的強化實例（沒有則 nullptr）
static const MapleEnhItem* maple_equipped_enh(const MapleCharacter& c, const std::string& slot) {
    std::string s = maple_equipped_raw(c, slot);
    return maple_eq_is_enh(s) ? maple_find_enh(c, maple_eq_enh_id(s)) : nullptr;
}
// 此強化實例 id 是否正穿在身上（任一部位）
static bool maple_enh_is_equipped(const MapleCharacter& c, int id) {
    static const char* slots[] = {"weapon","earring","helmet","glove","clothes","shoes"};
    for (auto* s : slots) if (maple_eq_enh_id(maple_equipped_raw(c, s)) == id) return true;
    return false;
}

// 目前裝備武器的攻速（秒／下）；未裝備武器則用預設
static int maple_atk_speed_sec(const MapleCharacter& c) {
    const MapleItemDef* w = maple_find_item(maple_equipped_key(c, "weapon"));
    return (w && w->slot == "weapon") ? w->atk_speed_sec : MAPLE_ATK_SPEED_DEFAULT_SEC;
}

// 目前裝備武器的類型（法杖/大劍/弓...），未裝備武器回傳空字串
static std::string maple_weapon_type(const MapleCharacter& c) {
    const MapleItemDef* w = maple_find_item(maple_equipped_key(c, "weapon"));
    return (w && w->slot == "weapon") ? w->weapon_type : std::string();
}

// 已裝備道具在某能力值上的加總；excl_slot 指定的部位不計入（""＝全部計入）
static int maple_equip_stat_bonus_excl(const MapleCharacter& c, const std::string& stat,
                                       const std::string& excl_slot) {
    int total = 0;
    const MapleJobDef& j = maple_job_of(c);
    static const char* slots[] = {"weapon","earring","helmet","glove","clothes","shoes"};
    for (auto* s : slots) {
        if (excl_slot == s) continue;
        const MapleItemDef* it = maple_find_item(maple_equipped_key(c, s));
        if (!it) continue;
        if      (stat == "str") total += it->str_bonus;
        else if (stat == "dex") total += it->dex_bonus;
        else if (stat == "int") total += it->int_bonus;
        else                    total += it->luk_bonus;
        // 防具的「主／副屬性」加成依穿戴者職業對應
        if (stat == j.primary_stat)   total += it->primary_generic;
        if (stat == j.secondary_stat) total += it->secondary_generic;
        // 強化卷軸累積的主／副屬性
        if (const MapleEnhItem* e = maple_equipped_enh(c, s)) {
            if (stat == j.primary_stat)   total += e->add_primary;
            if (stat == j.secondary_stat) total += e->add_secondary;
        }
    }
    return total;
}
// 所有已裝備道具在某個能力值上的加總
static int maple_equip_stat_bonus(const MapleCharacter& c, const std::string& stat) {
    return maple_equip_stat_bonus_excl(c, stat, "");
}

// 能力值總和（自己分配 + 裝備加成）
static int maple_stat_value(const MapleCharacter& c, const std::string& stat) {
    return maple_stat_alloc(c, stat) + maple_equip_stat_bonus(c, stat);
}

// 顯示用：「總和 (自己+裝備)」；沒有裝備加成時只顯示總和
static std::string maple_stat_breakdown(const MapleCharacter& c, const std::string& stat) {
    int a = maple_stat_alloc(c, stat);
    int e = maple_equip_stat_bonus(c, stat);
    if (e == 0) return std::to_string(a);
    return std::to_string(a + e) + " (" + std::to_string(a) + "+" + std::to_string(e) + ")";
}

static void maple_set_equipped(MapleCharacter& c, const std::string& slot, const std::string& key) {
    if      (slot == "weapon")  c.eq_weapon  = key;
    else if (slot == "earring") c.eq_earring = key;
    else if (slot == "helmet")  c.eq_helmet  = key;
    else if (slot == "glove")   c.eq_glove   = key;
    else if (slot == "clothes") c.eq_clothes = key;
    else if (slot == "shoes")   c.eq_shoes   = key;
}

// 角色的一轉基準職業（二轉回傳其父職業；初心者回傳 "beginner"）
static std::string maple_base_job(const MapleCharacter& c) {
    const MapleJobDef& j = maple_job_of(c);
    if (j.tier == 2) return j.parent;
    return j.key;
}

// 玩家目前是否符合裝備該道具的需求（等級／主屬性／副屬性／職業，依目前職業判斷）
// 屬性檢查會「扣掉該部位當前那件裝備的加成」再比：例如穿 150 武器需副屬性 150，
// 是看「脫掉現在的武器後、其他部位＋配點」夠不夠 150。
static bool maple_meets_requirement(const MapleCharacter& c, const MapleItemDef& item) {
    const MapleJobDef& j = maple_job_of(c);
    if (c.level < item.level_req) return false;
    int prim = maple_stat_alloc(c, j.primary_stat)
             + maple_equip_stat_bonus_excl(c, j.primary_stat,   item.slot);
    int sec  = maple_stat_alloc(c, j.secondary_stat)
             + maple_equip_stat_bonus_excl(c, j.secondary_stat, item.slot);
    if (prim < item.primary_req)   return false;
    if (sec  < item.secondary_req) return false;
    if (!item.job_req.empty() && maple_base_job(c) != item.job_req) return false;
    return true;
}

// 是否擁有此裝備（新手木劍人人皆有；已裝備中的視為擁有；強化實例或 equipment 庫存皆算）
static bool maple_owns_item(const MapleCharacter& c, const std::string& key) {
    if (key.empty()) return false;
    if (key == "wooden_sword") return true;
    const MapleItemDef* it = maple_find_item(key);
    if (it && maple_equipped_key(c, it->slot) == key) return true;
    auto eit = c.equipment.find(key);
    if (eit != c.equipment.end() && eit->second > 0) return true;
    for (auto& e : c.enh_items) if (e.base_key == key) return true;
    return false;
}
// 該 base_key 目前有沒有「未穿在身上」的強化實例（給裝備欄／交易用）
static const MapleEnhItem* maple_spare_enh(const MapleCharacter& c, const std::string& key) {
    for (auto& e : c.enh_items)
        if (e.base_key == key && !maple_enh_is_equipped(c, e.id)) return &e;
    return nullptr;
}

// 武器等裝備提供的攻擊力加總（未裝備武器時為0；含強化卷軸累積）
static int maple_total_atk(const MapleCharacter& c) {
    int total = 0;
    for (auto& slot : MAPLE_SLOTS) {
        auto* it = maple_find_item(maple_equipped_key(c, slot.key));
        if (it) total += it->atk_bonus;
        if (const MapleEnhItem* e = maple_equipped_enh(c, slot.key)) total += e->add_atk;
    }
    return total;
}

// 目前裝備的武器是否滿足此技能的武器要求（空要求＝任何武器甚至沒武器都行）
static bool maple_skill_weapon_ok(const MapleCharacter& c, const MapleSkillDef& sd) {
    if (sd.weapon_req.empty()) return true;
    std::string wt = maple_weapon_type(c);
    if (wt.empty()) return false;
    for (auto& req : sd.weapon_req) if (req == wt) return true;
    return false;
}

// 取得玩家目前選擇、且實際可用（已學會、轉職後仍看得到、武器符合）的攻擊技能；回傳 nullptr＝普通攻擊
static const MapleSkillDef* maple_current_atk_skill(const MapleCharacter& c) {
    if (c.adv_atk_skill.empty()) return nullptr;
    const MapleSkillDef* sd = maple_find_skill(c.adv_atk_skill);
    if (!sd) return nullptr;
    if (sd->type != "damage_fixed" && sd->type != "damage_coef") return nullptr;
    if (!maple_skill_visible(c, sd->job)) return nullptr;
    int lvl = maple_skill_level(c, sd->key);
    if (lvl <= 0) return nullptr;
    if (!maple_skill_weapon_ok(c, *sd)) return nullptr;
    return sd;
}

// 攻擊力是一個範圍：最大＝主屬性係數全開；最小＝主屬性係數只算 0.9*熟練度（預設10%）
// 兩者最後都要 /100。若玩家選擇了攻擊技能：damage_coef 套用技能係數取代基礎100%；
// damage_fixed 直接固定傷害（不吃屬性，最大最小相同）
static int64_t maple_atk_power_max(const MapleCharacter& c) {
    const MapleJobDef& j = maple_job_of(c);
    double primary   = maple_stat_value(c, j.primary_stat);
    double secondary = maple_stat_value(c, j.secondary_stat);
    int64_t base = (int64_t)llround((j.primary_coef * primary + j.secondary_coef * secondary) * maple_total_atk(c) / 100.0);
    const MapleSkillDef* sd = maple_current_atk_skill(c);
    if (!sd) return base;
    int lvl = maple_skill_level(c, sd->key);
    if (sd->type == "damage_fixed") return (int64_t)sd->values[lvl-1] * sd->hits;
    return (int64_t)llround(base * sd->values[lvl-1] / 100.0 * sd->hits);
}
static int64_t maple_atk_power_min(const MapleCharacter& c) {
    const MapleJobDef& j = maple_job_of(c);
    double primary   = maple_stat_value(c, j.primary_stat);
    double secondary = maple_stat_value(c, j.secondary_stat);
    int64_t base = (int64_t)llround((j.primary_coef * 0.9 * c.weapon_mastery * primary + j.secondary_coef * secondary) * maple_total_atk(c) / 100.0);
    const MapleSkillDef* sd = maple_current_atk_skill(c);
    if (!sd) return base;
    int lvl = maple_skill_level(c, sd->key);
    if (sd->type == "damage_fixed") return (int64_t)sd->values[lvl-1] * sd->hits;
    return (int64_t)llround(base * sd->values[lvl-1] / 100.0 * sd->hits);
}
static double maple_atk_power_avg(const MapleCharacter& c) {
    return (maple_atk_power_min(c) + maple_atk_power_max(c)) / 2.0;
}

// ─── 卷軸商店 ───────────────────────────────────────────────────────────────
// 目前只做「販售」，用瘋幣購買。卷軸的實際使用（強化裝備）之後再做。

struct MapleScrollDef {
    std::string key, name;
    int         rate;        // 成功率 %
    int64_t     price;       // 瘋幣
    std::string applies_to;  // 顯示用：適用的部位／武器類型
    int         primary_bonus   = 0; // 主屬性
    int         secondary_bonus = 0; // 副屬性
    int         atk_bonus       = 0; // 攻擊力
    int         item_id         = 0; // 交易用數字ID
};

static const std::vector<MapleScrollDef> MAPLE_SCROLLS = {
    // 防具
    {"sc_armor_main100", "頭盔／套服／鞋子 主屬性卷軸 100%", 100,  50000, "頭盔／套服／鞋子", 2, 0, 0, 96501},
    {"sc_glove_sec100",  "手套 副屬性卷軸 100%",             100,  30000, "手套",             0, 3, 0, 96502},
    {"sc_glove_atk100",  "手套 攻擊力卷軸 100%",             100, 200000, "手套",             0, 1, 1, 96503},
    {"sc_earring_ms100", "耳環 主副屬性卷軸 100%",           100, 100000, "耳環",             1, 2, 0, 96504},
    {"sc_earring_ms20",  "耳環 主副屬性卷軸 20%",            20,  500000, "耳環",             3, 5, 0, 96505},
    // 武器（各武器類型專屬）
    {"sc_wpn_staff",   "法杖 攻擊力卷軸 100%", 100, 70000, "法杖", 1, 0, 1, 96506},
    {"sc_wpn_claw",    "拳套 攻擊力卷軸 100%", 100, 70000, "拳套", 1, 0, 1, 96507},
    {"sc_wpn_dagger",  "匕首 攻擊力卷軸 100%", 100, 70000, "匕首", 1, 0, 1, 96508},
    {"sc_wpn_bow",     "弓 攻擊力卷軸 100%",   100, 70000, "弓",   1, 0, 1, 96509},
    {"sc_wpn_xbow",    "弩 攻擊力卷軸 100%",   100, 70000, "弩",   1, 0, 1, 96510},
    {"sc_wpn_gsword",  "大劍 攻擊力卷軸 100%", 100, 70000, "大劍", 1, 0, 1, 96511},
    {"sc_wpn_gun",     "火槍 攻擊力卷軸 100%", 100, 70000, "火槍", 1, 0, 1, 96512},
    {"sc_wpn_knuckle", "指虎 攻擊力卷軸 100%", 100, 70000, "指虎", 1, 0, 1, 96513},
    {"sc_wpn_rod",     "棒子 攻擊力卷軸 100%", 100, 50000, "棒子", 5, 0, 3, 96514},
};

static const MapleScrollDef* maple_find_scroll(const std::string& key) {
    for (auto& s : MAPLE_SCROLLS) if (s.key == key) return &s;
    return nullptr;
}
static const MapleScrollDef* maple_find_scroll_by_id(int id) {
    if (!id) return nullptr;
    for (auto& s : MAPLE_SCROLLS) if (s.item_id == id) return &s;
    return nullptr;
}

static std::string maple_scroll_effect_text(const MapleScrollDef& s) {
    std::string t;
    if (s.atk_bonus       > 0) t += (t.empty() ? "" : "、") + std::string("攻擊力+") + std::to_string(s.atk_bonus);
    if (s.primary_bonus   > 0) t += (t.empty() ? "" : "、") + std::string("主屬性+") + std::to_string(s.primary_bonus);
    if (s.secondary_bonus > 0) t += (t.empty() ? "" : "、") + std::string("副屬性+") + std::to_string(s.secondary_bonus);
    return t;
}

// 某卷軸能否用在某部位（依 applies_to 文字比對；武器部位用當前武器類型）
static bool maple_scroll_applies(const MapleScrollDef& s, const MapleCharacter& c, const std::string& slot) {
    if (slot == "weapon") {
        std::string wt = maple_weapon_type(c);
        return !wt.empty() && s.applies_to.find(wt) != std::string::npos;
    }
    std::string cn;
    if      (slot == "earring") cn = "耳環";
    else if (slot == "helmet")  cn = "頭盔";
    else if (slot == "glove")   cn = "手套";
    else if (slot == "clothes") cn = "套服";
    else if (slot == "shoes")   cn = "鞋子";
    return !cn.empty() && s.applies_to.find(cn) != std::string::npos;
}

// 某部位的卷軸使用次數上限（武器 7、其餘 5）
static int maple_enh_max_slots(const std::string& slot) { return slot == "weapon" ? 7 : 5; }

// 對某部位「目前裝備的那件」使用一張卷軸。回傳結果訊息；ok 表示是否有實際消耗卷軸。
// 呼叫前必須持有 data_mutex。
static std::string maple_enh_apply(MapleCharacter& c, const std::string& slot,
                                   const std::string& scroll_key, bool& ok) {
    ok = false;
    const MapleScrollDef* s = maple_find_scroll(scroll_key);
    if (!s) return "找不到這張卷軸。";
    std::string base = maple_equipped_key(c, slot);
    if (base.empty() || base == "wooden_sword") return "這個部位沒有可強化的裝備。";
    if (!maple_scroll_applies(*s, c, slot))     return "這張卷軸不能用在這個部位。";
    auto sit = c.scrolls.find(scroll_key);
    if (sit == c.scrolls.end() || sit->second <= 0) return "你沒有這張卷軸。";

    int max_slots = maple_enh_max_slots(slot);

    // 取得（必要時建立）此部位的強化實例
    std::string raw = maple_equipped_raw(c, slot);
    MapleEnhItem* e = nullptr;
    if (maple_eq_is_enh(raw)) {
        e = maple_find_enh(c, maple_eq_enh_id(raw));
        if (e && e->slots_used >= max_slots)
            return "🚫 這件裝備的強化次數已用完（上限 " + std::to_string(max_slots) + " 次）。";
    } else {
        // 把目前這件純裝備「升級」成強化實例
        if (auto eqi = c.equipment.find(base); eqi != c.equipment.end() && eqi->second > 0) {
            eqi->second--;
            if (eqi->second <= 0) c.equipment.erase(eqi);
        }
        MapleEnhItem ne;
        ne.id = c.enh_next_id++;
        ne.base_key = base;
        c.enh_items.push_back(ne);
        e = &c.enh_items.back();
        maple_set_equipped(c, slot, "#" + std::to_string(ne.id));
    }
    if (!e) return "強化實例遺失，請重試。";

    // 消耗卷軸並擲骰
    sit->second--;
    if (sit->second <= 0) c.scrolls.erase(sit);
    e->slots_used++;
    ok = true;

    int left = max_slots - e->slots_used;
    bool success = (rand() % 100) < s->rate;
    if (!success)
        return "💥 強化失敗！卷軸已消耗（裝備沒有損壞）。剩餘次數：" + std::to_string(left);
    e->add_primary   += s->primary_bonus;
    e->add_secondary += s->secondary_bonus;
    e->add_atk       += s->atk_bonus;
    e->enh_count++;
    return "✨ 強化成功！" + maple_scroll_effect_text(*s) + "　剩餘次數：" + std::to_string(left);
}

// ─── 冒險 ───────────────────────────────────────────────────────────────────
// 每隻怪的瘋幣掉落是一個範圍，收益估算與結算都用範圍平均值。

struct MapleAdvMonsterDef { std::string name; int hp; int64_t exp; int coin_min; int coin_max; };
struct MapleAdvRegionDef {
    std::string key, name;
    int suggested_level;
    bool open; // 是否已經開放
    MapleAdvMonsterDef monster;
};

static const std::vector<MapleAdvRegionDef> MAPLE_ADV_REGIONS = {
    {"archer_range",         "弓箭手訓練場", 1,  true, {"紅寶",     40,  4,  6,  9}},
    {"trapdoor",             "小心掉落",     10, true, {"三眼章魚", 200, 12, 12, 18}},
    {"blue_mushroom_forest", "藍菇菇樹林",   15, true, {"藍菇菇",   350, 16, 18, 27}},
};

static const MapleAdvRegionDef* maple_find_adv_region(const std::string& key) {
    for (auto& r : MAPLE_ADV_REGIONS) if (r.key == key) return &r;
    return nullptr;
}

// 擊殺一隻怪物後的休息秒數（休息完才能開始打下一隻）
static const int64_t MAPLE_ADV_REST_SEC = 50;

// ─── buff 技能效果 ─────────────────────────────────────────────────────────
// 取某 buff 技能目前等級的數值（未學會回 0）
static double maple_buff_value(const MapleCharacter& c, const std::string& key) {
    const MapleSkillDef* sd = maple_find_skill(key);
    if (!sd) return 0.0;
    int lvl = maple_skill_level(c, key);
    if (lvl <= 0 || lvl > (int)sd->values.size()) return 0.0;
    return sd->values[lvl - 1];
}
// 有效攻擊間隔（秒）：底攻速 − 瞬間移動/速度激發/衝鋒 縮減；下限 = max(5, 底值×30%)
static int maple_eff_atk_interval(const MapleCharacter& c) {
    double base = maple_atk_speed_sec(c);
    double cut = maple_buff_value(c, "teleport") * 0.1   // 瞬間移動：每級 0.1 秒 → 滿級 -2 秒
               + maple_buff_value(c, "haste")    * 0.1   // 速度激發：每級 0.1 秒 → 滿級 -1 秒（全體，solo 吃自己）
               + maple_buff_value(c, "charge");          // 衝鋒：每級約 0.5 秒 → 滿級 -5 秒
    double floor_v = std::max(5.0, base * 0.3);
    double v = base - cut;
    if (v < floor_v) v = floor_v;
    return (int)llround(v);
}
// 有效休息秒數：50 秒 − 自身強化/衝鋒 的百分比縮減（合計上限 70%）
static int maple_eff_rest_sec(const MapleCharacter& c) {
    double pct = maple_buff_value(c, "endurance")  // 自身強化：每級 2% → 滿級 20%
               + maple_buff_value(c, "charge");    // 衝鋒：每級約 0.5% → 滿級 5%
    if (pct > 70.0) pct = 70.0;
    return (int)llround(MAPLE_ADV_REST_SEC * (1.0 - pct / 100.0));
}
// 爆擊平均加成倍率（霸王箭）：爆擊率 p、爆擊 2 倍傷害 → 平均 = 1 + p
static double maple_crit_avg_mult(const MapleCharacter& c) {
    return 1.0 + maple_buff_value(c, "eagle_eye") / 100.0; // 每級 4% → 滿級 40%
}

// 擊殺一隻怪物需要的攻擊次數（用平均傷害＋爆擊期望算，無條件進位，最少1下）
static int maple_adv_hits_to_kill(const MapleCharacter& c, const MapleAdvRegionDef& region) {
    double avg_dmg = std::max(1.0, maple_atk_power_avg(c) * maple_crit_avg_mult(c));
    int hits = (int)std::ceil(region.monster.hp / avg_dmg);
    return hits < 1 ? 1 : hits;
}
// 純攻擊時間（不含休息）
static int64_t maple_adv_attack_secs(const MapleCharacter& c, const MapleAdvRegionDef& region) {
    return (int64_t)maple_adv_hits_to_kill(c, region) * maple_eff_atk_interval(c);
}
// 穩態每隻循環時間 = 攻擊時間 + 休息，估算每小時收益用
static int64_t maple_adv_seconds_per_kill(const MapleCharacter& c, const MapleAdvRegionDef& region) {
    return maple_adv_attack_secs(c, region) + maple_eff_rest_sec(c);
}
// elapsed_sec 內殺滿幾隻：第一隻在「攻擊時間」殺滿，之後每隻多花「攻擊時間 + 休息」
static int64_t maple_adv_kills_done(const MapleCharacter& c, const MapleAdvRegionDef& region, int64_t elapsed_sec) {
    int64_t atk = maple_adv_attack_secs(c, region);
    if (atk <= 0 || elapsed_sec < atk) return 0;
    return (elapsed_sec - atk) / (atk + maple_eff_rest_sec(c)) + 1;
}
static int64_t maple_adv_coins_per_kill(const MapleAdvRegionDef& region) {
    return (int64_t)llround((region.monster.coin_min + region.monster.coin_max) / 2.0);
}

// 估算每小時擊殺數／經驗／瘋幣（依「殺滿一隻才有收益」的離散模型）
static void maple_adv_estimate(const MapleCharacter& c, const MapleAdvRegionDef& region,
                               double& kills_per_hour, double& exp_per_hour, double& coins_per_hour) {
    int64_t spk = maple_adv_seconds_per_kill(c, region);
    kills_per_hour = spk > 0 ? 3600.0 / spk : 0.0;
    exp_per_hour   = kills_per_hour * region.monster.exp;
    coins_per_hour = kills_per_hour * maple_adv_coins_per_kill(region);
}

static bool maple_is_adventuring(const MapleCharacter& c) { return !c.adv_region.empty(); }

// 目前這場冒險已累積多少經驗／瘋幣：只計「已經殺滿的怪物數」，還在打的那隻不算
static void maple_adv_progress(const MapleCharacter& c, int64_t& exp_out, int64_t& coins_out, int64_t& seconds_out) {
    exp_out = 0; coins_out = 0; seconds_out = 0;
    if (!maple_is_adventuring(c)) return;
    const MapleAdvRegionDef* region = maple_find_adv_region(c.adv_region);
    if (!region) return;
    seconds_out = std::max((time_t)0, time(nullptr) - c.adv_started_at);
    int64_t kills = maple_adv_kills_done(c, *region, seconds_out); // 還在打的那隻、休息中都不算
    exp_out   = kills * region->monster.exp;
    coins_out = kills * maple_adv_coins_per_kill(*region);
}

// ─── Persistence ─────────────────────────────────────────────────────────────

static const std::string MAPLE_DATA_FILE = "maple_data.json";

static void save_maple_data() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, c] : maple_data) {
            j[std::to_string((uint64_t)uid)] = {
                {"level",             c.level},
                {"exp",               c.exp},
                {"coins",             c.coins},
                {"def",               c.def},
                {"max_hp",            c.max_hp},
                {"str_stat",          c.str_stat},
                {"dex_stat",          c.dex_stat},
                {"int_stat",          c.int_stat},
                {"luk_stat",          c.luk_stat},
                {"ap_reset_used",     c.ap_reset_used},
                {"job",               c.job},
                {"eq_weapon",         c.eq_weapon},
                {"eq_earring",        c.eq_earring},
                {"eq_helmet",         c.eq_helmet},
                {"eq_glove",          c.eq_glove},
                {"eq_clothes",        c.eq_clothes},
                {"eq_shoes",          c.eq_shoes},
                {"weapon_mastery",    c.weapon_mastery},
                {"skill_levels",      c.skill_levels},
                {"scrolls",           c.scrolls},
                {"equipment",         c.equipment},
                {"enh_next_id",       c.enh_next_id},
                {"enh_items",         [&]{
                    nlohmann::json arr = nlohmann::json::array();
                    for (auto& e : c.enh_items)
                        arr.push_back({{"id", e.id}, {"base_key", e.base_key},
                                       {"add_primary", e.add_primary}, {"add_secondary", e.add_secondary},
                                       {"add_atk", e.add_atk}, {"enh_count", e.enh_count},
                                       {"slots_used", e.slots_used}});
                    return arr;
                }()},
                {"adv_atk_skill",     c.adv_atk_skill},
                {"adv_region",        c.adv_region},
                {"adv_started_at",    (int64_t)c.adv_started_at},
                {"monsters_defeated", c.monsters_defeated},
                {"token_week_id",     c.token_week_id},
                {"token_week_spent",  c.token_week_spent},
                {"created_at",        (int64_t)c.created_at},
            };
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(MAPLE_DATA_FILE, j.dump(2));
}

static void load_maple_data() {
    std::ifstream f(MAPLE_DATA_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            MapleCharacter c;
            c.uid               = uid;
            c.level             = v.value("level",             1);
            c.exp               = v.value("exp",               (int64_t)0);
            c.coins             = v.value("coins",              (int64_t)0);
            c.def               = v.value("def",               5);
            c.max_hp            = v.value("max_hp",            50);
            c.str_stat          = v.value("str_stat",          4);
            c.dex_stat          = v.value("dex_stat",          4);
            c.int_stat          = v.value("int_stat",          4);
            c.luk_stat          = v.value("luk_stat",          4);
            c.ap_reset_used     = v.value("ap_reset_used",     false);
            c.job               = v.value("job",               std::string("beginner"));
            c.eq_weapon         = v.value("eq_weapon",         std::string("wooden_sword"));
            c.eq_earring        = v.value("eq_earring",        std::string());
            c.eq_helmet         = v.value("eq_helmet",         std::string());
            c.eq_glove          = v.value("eq_glove",          std::string());
            c.eq_clothes        = v.value("eq_clothes",        std::string());
            c.eq_shoes          = v.value("eq_shoes",          std::string());
            c.weapon_mastery    = v.value("weapon_mastery",    0.10);
            if (v.contains("skill_levels") && v["skill_levels"].is_object())
                c.skill_levels  = v["skill_levels"].get<std::map<std::string,int>>();
            if (v.contains("scrolls") && v["scrolls"].is_object())
                c.scrolls       = v["scrolls"].get<std::map<std::string,int>>();
            if (v.contains("equipment") && v["equipment"].is_object())
                c.equipment     = v["equipment"].get<std::map<std::string,int>>();
            c.enh_next_id       = v.value("enh_next_id", 1);
            if (v.contains("enh_items") && v["enh_items"].is_array()) {
                for (auto& ej : v["enh_items"]) {
                    MapleEnhItem e;
                    e.id            = ej.value("id", 0);
                    e.base_key      = ej.value("base_key", std::string());
                    e.add_primary   = ej.value("add_primary", 0);
                    e.add_secondary = ej.value("add_secondary", 0);
                    e.add_atk       = ej.value("add_atk", 0);
                    e.enh_count     = ej.value("enh_count", 0);
                    e.slots_used    = ej.value("slots_used", 0);
                    if (e.id > 0 && !e.base_key.empty()) c.enh_items.push_back(e);
                }
            }
            c.adv_atk_skill     = v.value("adv_atk_skill",     std::string());
            c.adv_region        = v.value("adv_region",        std::string());
            c.adv_started_at    = (time_t)v.value("adv_started_at", (int64_t)0);
            c.monsters_defeated = v.value("monsters_defeated", (int64_t)0);
            c.token_week_id     = v.value("token_week_id",    (int64_t)0);
            c.token_week_spent  = v.value("token_week_spent", (int64_t)0);
            c.created_at        = (time_t)v.value("created_at", (int64_t)0);
            maple_data[uid] = c;
        }
    } catch (...) {}
}

// 取得（必要時建立）玩家的楓之谷角色
static MapleCharacter maple_get_or_create(dpp::snowflake uid) {
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = maple_data.find(uid);
    if (it != maple_data.end()) return it->second;
    MapleCharacter c;
    c.uid = uid;
    c.created_at = time(nullptr);
    maple_data[uid] = c;
    return c;
}

// ─── 訊息畫面 ───────────────────────────────────────────────────────────────

static dpp::message make_maple_home_msg(dpp::snowflake uid, const std::string& display_name,
                                        const std::string& avatar_url) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleJobDef& job = maple_job_of(c);

    std::string content = "## 🍁 楓之谷世界\n";
    content += "職業：**" + job.name + "**\n";
    if (c.level >= MAPLE_LEVEL_CAP) {
        content += "**Lv. " + std::to_string(c.level) + "**　（已達等級上限）\n";
    } else {
        int64_t need = maple_exp_to_next(c.level);
        content += "**Lv. " + std::to_string(c.level) + "**　" + hp_bar((int)c.exp, (int)need, 10)
                 + "　" + std::to_string(c.exp) + "/" + std::to_string(need) + " EXP\n";
    }
    content += "🪙 瘋幣：**" + std::to_string(c.coins) + "**\n\n";
    content += "**⚔️ 屬性**\n";
    content += "主屬性：" + maple_stat_name(job.primary_stat) + " **" + maple_stat_breakdown(c, job.primary_stat) + "**　"
             + "副屬性：" + maple_stat_name(job.secondary_stat) + " **" + maple_stat_breakdown(c, job.secondary_stat) + "**\n";
    content += "攻擊力 **" + std::to_string(maple_atk_power_min(c)) + " ~ " + std::to_string(maple_atk_power_max(c)) + "**\n";
    {
        int spd = maple_atk_speed_sec(c);
        int eff = maple_eff_atk_interval(c);
        content += "⚡ 攻速：**" + maple_atk_speed_name(spd) + "**（" + std::to_string(spd) + " 秒／下";
        if (eff != spd) content += " → 技能後 " + std::to_string(eff) + " 秒";
        content += "）\n";
    }
    // 防禦力／生命值暫時不顯示（欄位保留，之後可能用到）
    {
        const MapleSkillDef* atk_sd = maple_current_atk_skill(c);
        content += "⚔️ 目前攻擊方式：**" + (atk_sd ? atk_sd->name : std::string("普通攻擊")) + "**\n";
        double crit = maple_buff_value(c, "eagle_eye");
        if (crit > 0)
            content += "🎯 爆擊率：**" + std::to_string((int)crit) + "%**（爆擊 2 倍傷害）\n";
        int rest = maple_eff_rest_sec(c);
        if (rest != (int)MAPLE_ADV_REST_SEC)
            content += "😮‍💨 冒險擊殺後休息：**" + std::to_string(rest) + " 秒**\n";
    }
    content += "👑 累計擊敗首領：**" + std::to_string(c.monsters_defeated) + "**\n";
    content += "🌟 剩餘技能點：**" + std::to_string(maple_sp_unspent(c)) + "**\n";
    if (!maple_equip_unlocked(c)) content += "🔒 「換裝自由」點滿後開放裝備系統\n";
    if (!maple_ap_unlocked(c))    content += "🔒 「能力值自由」點滿後開放能力值系統\n";
    if (maple_is_adventuring(c))  content += "🗺️ 冒險中，無法調整裝備、能力值與攻擊方式\n";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(v2_section(content, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔢 能力值").set_id("maple_ap_" + uid_s).set_style(dpp::cos_primary)
        .set_disabled(!maple_ap_unlocked(c) || maple_is_adventuring(c)));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎽 裝備").set_id("maple_eq_" + uid_s).set_style(dpp::cos_primary)
        .set_disabled(!maple_equip_unlocked(c) || maple_is_adventuring(c)));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🌟 技能").set_id("maple_skill_" + uid_s).set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗺️ 冒險").set_id("maple_adv_" + uid_s).set_style(dpp::cos_success));
    if (maple_can_first_job(c)) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("⚡ 轉職").set_id("maple_j1open_" + uid_s).set_style(dpp::cos_success));
    } else if (maple_can_second_job(c)) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("⚡ 二轉").set_id("maple_j2open_" + uid_s).set_style(dpp::cos_success));
    }
    msg.add_component_v2(row);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⚔️ 攻擊方式").set_id("maple_atktype_" + uid_s).set_style(dpp::cos_secondary)
        .set_disabled(maple_is_adventuring(c)));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎒 背包").set_id("maple_bag_" + uid_s + "_scroll").set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏪 商店").set_id("maple_shop_" + uid_s).set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);

    return msg;
}

static std::string maple_skill_value_text(const MapleSkillDef& sd, int level) {
    if (level <= 0) return "尚未學習";
    if (sd.type == "damage_fixed") return "固定造成 " + std::to_string((int64_t)sd.values[level-1]) + " 傷害"
                                        + (sd.hits > 1 ? "（×" + std::to_string(sd.hits) + " 下）" : "");
    if (sd.type == "damage_coef")  return "技能係數 " + std::to_string((int64_t)sd.values[level-1]) + "%"
                                        + (sd.hits > 1 ? "（×" + std::to_string(sd.hits) + " 下）" : "");
    if (sd.type == "buff_pct") {
        double v = sd.values[level-1];
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f", v);
        std::string s(buf);
        if (s.size() > 2 && s.substr(s.size()-2) == ".0") s = s.substr(0, s.size()-2);
        if (sd.key == "eagle_eye")  return "爆擊率 +" + s + "%（爆擊 2 倍傷害）";
        if (sd.key == "endurance")  return "冒險休息時間 -" + s + "%";
        if (sd.key == "haste")      return "全體攻擊間隔 -" + std::to_string(v * 0.1) .substr(0,3) + " 秒";
        if (sd.key == "teleport")   return "自身攻擊間隔 -" + std::to_string(v * 0.1).substr(0,3) + " 秒";
        if (sd.key == "charge")     return "攻擊間隔 -" + s + " 秒、休息時間 -" + s + "%";
        return "效果 +" + s + "%";
    }
    if (sd.type == "unlock") return level >= sd.max_level ? "✅ 已解鎖" : "滿級後解鎖";
    return "";
}

// ─── 攻擊方式選擇（普通攻擊 或 已學會的攻擊技能）─────────────────────────────

static dpp::message make_maple_atktype_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleSkillDef* cur_sd = maple_current_atk_skill(c);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚔️ 攻擊方式\n選擇冒險與戰鬥計算時使用的攻擊方式，只能選擇已經學會的攻擊技能。"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    auto row_for = [&](const std::string& name, const std::string& desc, bool current,
                       const std::string& skill_key, bool weapon_ok) {
        std::string text = "**" + name + "**\n" + desc;
        if (current) text += "\n✅ 目前使用中";
        else if (!weapon_ok) text += "\n🚫 武器不符，無法選用";
        return dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label(current ? "使用中" : "選擇")
                .set_id("maple_atkpick_" + uid_s + "_" + skill_key)
                .set_style(current ? dpp::cos_secondary : dpp::cos_success)
                .set_disabled(current || !weapon_ok));
    };

    container.add_component_v2(row_for("🔹 普通攻擊", "基礎攻擊力，不套用任何技能係數。",
        cur_sd == nullptr, "normal", true));

    for (auto& skill_job : maple_visible_skill_jobs(c)) {
        for (auto* sd : maple_skills_for_job(skill_job)) {
            if (sd->type != "damage_fixed" && sd->type != "damage_coef") continue;
            int lvl = maple_skill_level(c, sd->key);
            if (lvl <= 0) continue; // 尚未學習，不能選
            std::string desc = "Lv." + std::to_string(lvl) + "／" + maple_skill_value_text(*sd, lvl);
            if (!sd->weapon_req.empty()) {
                std::string wr;
                for (auto& r : sd->weapon_req) wr += (wr.empty() ? "" : "／") + r;
                desc += "\n需裝備武器：**" + wr + "**";
            }
            bool wok = maple_skill_weapon_ok(c, *sd);
            container.add_component_v2(row_for(sd->name, desc, cur_sd == sd, sd->key, wok));
        }
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_skill_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🌟 技能\n剩餘技能點：**" + std::to_string(maple_sp_unspent(c)) + "**"));

    // 轉職後之前職業的技能仍然顯示（初心者技能一律顯示，再加上目前一轉職業的技能）
    for (auto& skill_job : maple_visible_skill_jobs(c)) {
        const MapleJobDef* jd = maple_find_job(skill_job);
        container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
            .set_spacing(dpp::sep_small).set_divider(true));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("**── " + (jd ? jd->name : skill_job) + " ──**"));
        for (auto* sd : maple_skills_for_job(skill_job)) {
            int lvl = maple_skill_level(c, sd->key);
            bool maxed = lvl >= sd->max_level;
            std::string text = "**" + sd->name + "** Lv." + std::to_string(lvl) + "/" + std::to_string(sd->max_level) + "\n";
            text += sd->desc + "\n";
            text += "目前：" + maple_skill_value_text(*sd, lvl);
            container.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label("+1").set_id("maple_skadd_" + uid_s + "_" + sd->key)
                    .set_style(dpp::cos_success)
                    .set_disabled(maxed || maple_sp_unspent(c) <= 0)));
        }
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_ap_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    int unspent = maple_unspent_ap(c);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🔢 能力值分配\n剩餘可分配點數：**" + std::to_string(unspent) + "**"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    auto stat_text = [&](const std::string& stat) {
        return dpp::component().set_type(dpp::cot_text_display)
            .set_content("**" + maple_stat_name(stat) + "**：" + maple_stat_breakdown(c, stat));
    };
    container.add_component_v2(stat_text("str"));
    container.add_component_v2(stat_text("dex"));
    container.add_component_v2(stat_text("int"));
    container.add_component_v2(stat_text("luk"));
    msg.add_component_v2(container);

    // 每項能力值一列，各給 +1／+5／+10 三顆按鈕（超過剩餘點數時實際只會加到剩餘量，不會卡住不能按）
    auto stat_btn_row = [&](const std::string& stat) {
        dpp::component r; r.set_type(dpp::cot_action_row);
        std::string nm = maple_stat_name(stat);
        for (int amt : {1, 5, 10}) {
            r.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(nm + " +" + std::to_string(amt))
                .set_id("maple_apadd_" + uid_s + "_" + stat + "_" + std::to_string(amt))
                .set_style(dpp::cos_success).set_disabled(unspent <= 0));
        }
        return r;
    };
    msg.add_component_v2(stat_btn_row("str"));
    msg.add_component_v2(stat_btn_row("dex"));
    msg.add_component_v2(stat_btn_row("int"));
    msg.add_component_v2(stat_btn_row("luk"));

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 重新配點").set_id("maple_apreset_" + uid_s).set_style(dpp::cos_danger)
        .set_disabled(c.ap_reset_used));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_ap_reset_confirm_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚠️ 重新配點確認\n所有能力值會歸零重新分配，**這是你唯一一次重新配點的機會**，確定要繼續嗎？"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確定重新配點").set_id("maple_apresetok_" + uid_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("maple_apresetno_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_job1_select_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚡ 轉職\n選擇你的第一個職業（選擇後無法更改）："));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto* j : maple_first_jobs())
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(j->name).set_id("maple_j1pick_" + uid_s + "_" + j->key).set_style(dpp::cos_primary));
    msg.add_component_v2(row);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);

    return msg;
}

static dpp::message make_maple_equip_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🎽 裝備\n武器攻擊力目前 **" + std::to_string(maple_total_atk(c)) + "**"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    for (auto& slot : MAPLE_SLOTS) {
        std::string key = maple_equipped_key(c, slot.key);
        auto* item = maple_find_item(key);
        const MapleEnhItem* e = maple_equipped_enh(c, slot.key);
        std::string text = "**" + slot.icon + " " + slot.name + "**：";
        if (item) {
            text += item->name;
            if (e && e->enh_count > 0) text += " ✨+" + std::to_string(e->enh_count);
            if (item->atk_bonus > 0) text += "（+" + std::to_string(item->atk_bonus) + " ATK）";
            if (slot.key == "weapon")
                text += "　攻速 " + maple_atk_speed_name(item->atk_speed_sec) + "（" + std::to_string(item->atk_speed_sec) + "秒）";
            if (e && (e->add_primary || e->add_secondary || e->add_atk)) {
                text += "\n　強化：";
                if (e->add_atk)       text += "攻擊力+" + std::to_string(e->add_atk) + " ";
                if (e->add_primary)   text += "主屬性+" + std::to_string(e->add_primary) + " ";
                if (e->add_secondary) text += "副屬性+" + std::to_string(e->add_secondary);
            }
        } else {
            text += "（未裝備）";
        }
        container.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label("更換").set_id("maple_eqopen_" + uid_s + "_" + slot.key).set_style(dpp::cos_secondary)));
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔨 強化").set_id("maple_enhpick_" + uid_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

// 強化：部位選擇
static dpp::message make_maple_enh_pick_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🔨 強化裝備\n選擇要強化的部位（強化的是「目前穿在該部位」的裝備）"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    for (auto& slot : MAPLE_SLOTS) {
        std::string key = maple_equipped_key(c, slot.key);
        auto* item = maple_find_item(key);
        const MapleEnhItem* e = maple_equipped_enh(c, slot.key);
        bool canonly = item && key != "wooden_sword";
        std::string text = "**" + slot.icon + " " + slot.name + "**：";
        text += item ? item->name : std::string("（未裝備）");
        if (e && e->enh_count > 0) text += " ✨+" + std::to_string(e->enh_count);
        if (canonly) {
            int used = e ? e->slots_used : 0;
            text += "　強化 " + std::to_string(used) + "/" + std::to_string(maple_enh_max_slots(slot.key));
        } else {
            text += "\n-# 沒有可強化的裝備";
        }
        container.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label("強化").set_id("maple_enhopen_" + uid_s + "_" + slot.key)
                .set_style(dpp::cos_success).set_disabled(!canonly)));
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_eq_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

// 強化：某部位可用的卷軸清單
static dpp::message make_maple_enh_msg(dpp::snowflake uid, const std::string& slot,
                                      const std::string& result = "") {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleSlotDef* sd = nullptr;
    for (auto& s : MAPLE_SLOTS) if (s.key == slot) { sd = &s; break; }
    std::string slot_name = sd ? sd->name : slot;
    std::string key = maple_equipped_key(c, slot);
    auto* item = maple_find_item(key);
    const MapleEnhItem* e = maple_equipped_enh(c, slot);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
    int max_slots = maple_enh_max_slots(slot);
    int used = e ? e->slots_used : 0;
    bool slots_full = used >= max_slots;
    std::string head = "## 🔨 強化 — " + slot_name + "\n";
    if (item) {
        head += "**" + item->name + "**";
        if (e && e->enh_count > 0) head += "　✨ 成功 " + std::to_string(e->enh_count) + " 次";
        head += "\n強化次數：**" + std::to_string(used) + " / " + std::to_string(max_slots) + "**";
        if (slots_full) head += "（已用完）";
        if (e && (e->add_primary || e->add_secondary || e->add_atk)) {
            head += "\n目前累積：";
            if (e->add_atk)       head += "攻擊力+" + std::to_string(e->add_atk) + " ";
            if (e->add_primary)   head += "主屬性+" + std::to_string(e->add_primary) + " ";
            if (e->add_secondary) head += "副屬性+" + std::to_string(e->add_secondary);
        }
    } else {
        head += "（這個部位沒有裝備）";
    }
    if (!result.empty()) head += "\n\n" + result;
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(head));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    bool any = false;
    if (item && key != "wooden_sword") {
        for (auto& s : MAPLE_SCROLLS) {
            if (!maple_scroll_applies(s, c, slot)) continue;
            int n = c.scrolls.count(s.key) ? c.scrolls.at(s.key) : 0;
            if (n <= 0) continue;
            any = true;
            std::string text = "**" + s.name + "**　×" + std::to_string(n) + "\n"
                             + "成功率 " + std::to_string(s.rate) + "%　效果："
                             + maple_scroll_effect_text(s);
            container.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label("使用").set_id("maple_enhuse_" + uid_s + "_" + slot + "_" + s.key)
                    .set_style(dpp::cos_success).set_disabled(slots_full)));
        }
    }
    if (slots_full) {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("🚫 這件裝備的強化次數已用完。"));
    } else if (!any) {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("沒有可用在這個部位的卷軸。到卷軸商店購買。"));
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_enhpick_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_equip_slot_msg(dpp::snowflake uid, const std::string& slot) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleSlotDef* sd = nullptr;
    for (auto& s : MAPLE_SLOTS) if (s.key == slot) { sd = &s; break; }
    std::string slot_name = sd ? sd->name : slot;
    std::string cur_key = maple_equipped_key(c, slot);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🔄 更換" + slot_name));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    bool any = false;
    for (auto& item : MAPLE_ITEMS) {
        if (item.slot != slot) continue;
        if (!maple_owns_item(c, item.key)) continue; // 只顯示擁有的裝備
        any = true;
        bool equipped = (item.key == cur_key);
        bool eligible = maple_meets_requirement(c, item);
        const MapleEnhItem* enh = equipped ? maple_equipped_enh(c, slot) : maple_spare_enh(c, item.key);
        std::string text = "**" + item.name + "**";
        if (enh && enh->enh_count > 0) text += " ✨+" + std::to_string(enh->enh_count);
        if (item.atk_bonus > 0) text += "　⚔️+" + std::to_string(item.atk_bonus);
        if (item.slot == "weapon")
            text += "　⚡" + maple_atk_speed_name(item.atk_speed_sec) + "(" + std::to_string(item.atk_speed_sec) + "秒)";
        text += "\n限制等級 " + std::to_string(item.level_req)
              + "　限制副屬性 " + std::to_string(item.secondary_req);
        {
            const MapleJobDef* jr = item.job_req.empty() ? nullptr : maple_find_job(item.job_req);
            if (jr) text += "　限制職業 " + jr->name;
        }
        if (item.str_bonus) text += "　力量+" + std::to_string(item.str_bonus);
        if (item.dex_bonus) text += "　敏捷+" + std::to_string(item.dex_bonus);
        if (item.int_bonus) text += "　智力+" + std::to_string(item.int_bonus);
        if (item.luk_bonus) text += "　幸運+" + std::to_string(item.luk_bonus);
        if (item.primary_generic)   text += "　主屬性+" + std::to_string(item.primary_generic);
        if (item.secondary_generic) text += "　副屬性+" + std::to_string(item.secondary_generic);
        if (enh && (enh->add_primary || enh->add_secondary || enh->add_atk)) {
            text += "\n強化：";
            if (enh->add_atk)       text += "攻擊力+" + std::to_string(enh->add_atk) + " ";
            if (enh->add_primary)   text += "主屬性+" + std::to_string(enh->add_primary) + " ";
            if (enh->add_secondary) text += "副屬性+" + std::to_string(enh->add_secondary);
        }
        if (!item.sellable) text += "　🚫無法售出";
        container.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label(equipped ? "已裝備" : "裝備")
                .set_id("maple_eqpick_" + uid_s + "_" + slot + "_" + item.key)
                .set_style(equipped ? dpp::cos_secondary : dpp::cos_success)
                .set_disabled(equipped || !eligible)));
    }
    if (!any) {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("目前沒有擁有的" + slot_name + "，去裝備商店購買。"));
    }
    // 卸下（非武器；武器的「卸下」＝換回新手木劍）
    bool has_something = !cur_key.empty() && cur_key != "wooden_sword";
    if (has_something) {
        container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
            .set_spacing(dpp::sep_small).set_divider(true));
        container.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display)
                .set_content(slot == "weapon" ? "換回 **新手木劍**" : "卸下目前的" + slot_name + "（放回背包）"))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label("卸下").set_id("maple_equnequip_" + uid_s + "_" + slot).set_style(dpp::cos_danger)));
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_eq_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_job2_select_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚡ 二轉\n選擇你的第二個職業（選擇後無法更改）："));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto* j : maple_second_jobs(c.job))
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(j->name).set_id("maple_j2pick_" + uid_s + "_" + j->key).set_style(dpp::cos_primary));
    msg.add_component_v2(row);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);

    return msg;
}

static std::string maple_fmt_duration(int64_t seconds) {
    int64_t m = seconds / 60;
    int64_t h = m / 60;
    m %= 60;
    if (h > 0) return std::to_string(h) + " 小時 " + std::to_string(m) + " 分鐘";
    return std::to_string(m) + " 分鐘";
}

static dpp::message make_maple_adv_region_list_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🗺️ 冒險\n攻擊間隔取決於武器攻速，建議等級僅供參考、未達也能進入。"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    for (auto& r : MAPLE_ADV_REGIONS) {
        std::string text = "**" + r.name + "**　建議 Lv. " + std::to_string(r.suggested_level) + "~";
        if (!r.open) text += "　🚧尚未開放";
        else text += "\n" + r.monster.name + "：" + std::to_string(r.monster.hp) + " HP　"
                   + std::to_string(r.monster.exp) + " EXP　"
                   + std::to_string(r.monster.coin_min) + "~" + std::to_string(r.monster.coin_max) + " 幣";
        container.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label(r.open ? "進入" : "尚未開放")
                .set_id("maple_advopen_" + uid_s + "_" + r.key)
                .set_style(r.open ? dpp::cos_success : dpp::cos_secondary)
                .set_disabled(!r.open)));
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_adv_preview_msg(dpp::snowflake uid, const std::string& region_key) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleAdvRegionDef* region = maple_find_adv_region(region_key);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    std::string content;
    if (!region || !region->open) {
        content = "## 🚧 尚未開放\n這個區域還沒有開放。";
    } else {
        double kph, eph, cph;
        maple_adv_estimate(c, *region, kph, eph, cph);
        content = "## 🗺️ " + region->name + "\n";
        content += "怪物：**" + region->monster.name + "**　" + std::to_string(region->monster.hp) + " HP　"
                 + std::to_string(region->monster.exp) + " EXP　"
                 + std::to_string(region->monster.coin_min) + "~" + std::to_string(region->monster.coin_max) + " 幣\n\n";
        content += "**預估收益（每小時）**\n";
        content += "✨ 經驗值：約 **" + std::to_string((int64_t)llround(eph)) + "**\n";
        content += "🪙 瘋幣：約 **" + std::to_string((int64_t)llround(cph)) + "**\n";
        content += "-# 依目前攻擊力平均值估算（每擊殺 1 隻後休息 "
                 + std::to_string(maple_eff_rest_sec(c)) + " 秒），實際會因隨機浮動而略有差異";
    }

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    if (region && region->open) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("▶️ 開始冒險").set_id("maple_advstart_" + uid_s + "_" + region_key).set_style(dpp::cos_success));
    }
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_adv_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_adv_status_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleAdvRegionDef* region = maple_find_adv_region(c.adv_region);

    int64_t exp_now, coins_now, secs;
    maple_adv_progress(c, exp_now, coins_now, secs);
    int64_t kills = 0, spk = 0;
    if (region) {
        spk = maple_adv_seconds_per_kill(c, *region);
        kills = maple_adv_kills_done(c, *region, secs);
    }

    std::string content = "## 🗺️ 冒險中 — " + (region ? region->name : c.adv_region) + "\n";
    content += "已經過 **" + maple_fmt_duration(secs) + "**\n";
    if (region)
        content += "每隻約 **" + std::to_string(spk) + " 秒**（含擊殺後休息 "
                 + std::to_string(maple_eff_rest_sec(c)) + " 秒）— " + region->monster.name + "\n";
    content += "\n**目前累積**\n";
    content += "🗡️ 已擊殺 **" + std::to_string(kills) + "** 隻\n";
    content += "✨ 經驗值 +**" + std::to_string(exp_now) + "**\n";
    content += "🪙 瘋幣 +**" + std::to_string(coins_now) + "**\n";
    content += "-# 還在打的那隻不算，殺滿才有收益。冒險期間無法調整裝備與能力值";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 刷新").set_id("maple_advstatus_" + uid_s).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 結算").set_id("maple_advsettle_" + uid_s).set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消探險").set_id("maple_advcancel_" + uid_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_adv_cancel_confirm_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚠️ 確認取消冒險\n目前累積的經驗值與瘋幣都不會保留，確定要取消嗎？"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確定取消").set_id("maple_advcancelok_" + uid_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 返回").set_id("maple_advstatus_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_adv_settle_msg(dpp::snowflake uid, const std::string& region_name,
                                              int64_t exp_gain, int64_t coin_gain, int64_t secs, int level_ups) {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string content = "## ✅ 冒險結算\n在 **" + region_name + "** 冒險了 **" + maple_fmt_duration(secs) + "**\n\n";
    content += "✨ 獲得經驗值 +**" + std::to_string(exp_gain) + "**\n";
    content += "🪙 獲得瘋幣 +**" + std::to_string(coin_gain) + "**\n";
    if (level_ups > 0) content += "\n🆙 **升級！** 連升 **" + std::to_string(level_ups) + "** 級！";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗺️ 返回冒險").set_id("maple_adv_" + uid_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

// 冒險入口：有進行中的冒險就顯示狀態，否則顯示區域列表
static dpp::message make_maple_adventure_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    if (maple_is_adventuring(c)) return make_maple_adv_status_msg(uid);
    return make_maple_adv_region_list_msg(uid);
}

// ─── 商店 ───────────────────────────────────────────────────────────────────

static std::vector<std::string> maple_eqshop_categories(const MapleCharacter& c); // 定義在下方裝備商店區

static dpp::message make_maple_shop_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🏪 商店\n🪙 瘋幣：**" + std::to_string(c.coins) + "**\n選擇要進入的商店："));
    msg.add_component_v2(container);

    auto eqcats = maple_eqshop_categories(c);
    std::string eqcat0 = eqcats.empty() ? "earring" : eqcats.front();

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📜 卷軸商店").set_id("maple_scshop_" + uid_s + "_0").set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🪙 代幣商店").set_id("maple_tokenshop_" + uid_s).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎽 裝備商店").set_id("maple_eqshop_" + uid_s + "_" + eqcat0 + "_0").set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);

    return msg;
}

// ─── 代幣商店（籌碼 → 瘋幣）────────────────────────────────────────────────────
static const int64_t MAPLE_TOKEN_WEEKLY_CAP = 20000; // 每週最多可用多少籌碼兌換
static const int     MAPLE_TOKEN_RATE_NUM   = 1;     // 瘋幣 = 籌碼 × num / den（之後可調比值）
static const int     MAPLE_TOKEN_RATE_DEN   = 1;

static int64_t maple_token_week_now()             { return (int64_t)(time(nullptr) / 604800); }
static int64_t maple_token_coins_for(int64_t chips) { return chips * MAPLE_TOKEN_RATE_NUM / MAPLE_TOKEN_RATE_DEN; }
static int64_t maple_token_week_spent(const MapleCharacter& c) {
    return c.token_week_id == maple_token_week_now() ? c.token_week_spent : 0;
}
static int64_t maple_token_remaining(const MapleCharacter& c) {
    int64_t r = MAPLE_TOKEN_WEEKLY_CAP - maple_token_week_spent(c);
    return r < 0 ? 0 : r;
}

static dpp::message make_maple_tokenshop_msg(dpp::snowflake uid) {
    int64_t chips = get_chips(uid);
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);

    int64_t spent = maple_token_week_spent(c);
    int64_t remain = maple_token_remaining(c);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF1, 0xC4, 0x0F));
    std::string body = "## 🪙 代幣商店\n"
        "用**籌碼**兌換**瘋幣**，比值 " + std::to_string(MAPLE_TOKEN_RATE_NUM) + " : " + std::to_string(MAPLE_TOKEN_RATE_DEN) + "（籌碼 : 瘋幣）\n\n"
        "💰 你的籌碼：**" + std::to_string(chips) + "**\n"
        "🪙 你的瘋幣：**" + std::to_string(c.coins) + "**\n"
        "📅 本週已兌換：**" + std::to_string(spent) + " / " + std::to_string(MAPLE_TOKEN_WEEKLY_CAP) + "** 籌碼（剩 **" + std::to_string(remain) + "**）";
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(body));
    msg.add_component_v2(container);

    static const int64_t opts[] = {1000, 5000, 10000, 20000};
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (int64_t a : opts) {
        bool ok = a <= remain && a <= chips;
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("兌換 " + std::to_string(a))
            .set_id("maple_tokenex_" + uid_s + "_" + std::to_string(a))
            .set_style(dpp::cos_success).set_disabled(!ok));
    }
    msg.add_component_v2(row);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    {
        int64_t mx = std::min(remain, chips);
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(mx > 0 ? "兌換上限 " + std::to_string(mx) : "額度已用完")
            .set_id("maple_tokenex_" + uid_s + "_" + std::to_string(mx))
            .set_style(dpp::cos_primary).set_disabled(mx <= 0));
    }
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回商店").set_id("maple_shop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);

    return msg;
}

static dpp::message make_maple_shop_soon_msg(dpp::snowflake uid, const std::string& name) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x95, 0x95, 0x95));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🚧 " + name + "\n尚未開放，敬請期待。"));
    msg.add_component_v2(container);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回商店").set_id("maple_shop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

static const int MAPLE_SCSHOP_PAGE_SIZE = 6;

static dpp::message make_maple_scshop_msg(dpp::snowflake uid, int page) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    int total = (int)MAPLE_SCROLLS.size();
    int pages = (total + MAPLE_SCSHOP_PAGE_SIZE - 1) / MAPLE_SCSHOP_PAGE_SIZE;
    if (page < 0) page = 0;
    if (page >= pages) page = pages - 1;
    int start = page * MAPLE_SCSHOP_PAGE_SIZE;
    int end   = std::min(start + MAPLE_SCSHOP_PAGE_SIZE, total);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component header;
    header.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    header.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 📜 卷軸商店（" + std::to_string(page + 1) + "/" + std::to_string(pages) + "）\n"
                     "🪙 瘋幣：**" + std::to_string(c.coins) + "**"));
    msg.add_component_v2(header);

    for (int i = start; i < end; i++) {
        const MapleScrollDef& s = MAPLE_SCROLLS[i];
        int owned = c.scrolls.count(s.key) ? c.scrolls.at(s.key) : 0;
        std::string text = "**" + s.name + "**\n"
                         + "適用：" + s.applies_to + "　成功率 " + std::to_string(s.rate) + "%\n"
                         + "效果：" + maple_scroll_effect_text(s) + "\n"
                         + "💰 " + std::to_string(s.price) + " 瘋幣" + (owned > 0 ? "　（持有 " + std::to_string(owned) + "）" : "");
        msg.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label("購買").set_id("maple_scbuy_" + uid_s + "_" + s.key)
                .set_style(dpp::cos_success).set_disabled(c.coins < s.price)));
    }

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("◀ 上一頁").set_id("maple_scshop_" + uid_s + "_" + std::to_string(page - 1))
        .set_style(dpp::cos_secondary).set_disabled(page <= 0));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶ 下一頁").set_id("maple_scshop_" + uid_s + "_" + std::to_string(page + 1))
        .set_style(dpp::cos_secondary).set_disabled(page >= pages - 1));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回商店").set_id("maple_shop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);

    return msg;
}

static dpp::message make_maple_scbuy_confirm_msg(dpp::snowflake uid, const std::string& scroll_key) {
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleScrollDef* s = maple_find_scroll(scroll_key);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF1, 0xC4, 0x0F));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content(s ? ("## 🛒 確認購買\n**" + s->name + "**\n效果：" + maple_scroll_effect_text(*s)
                          + "\n花費 **" + std::to_string(s->price) + "** 瘋幣，確定要購買嗎？")
                       : "## ❌ 找不到這個卷軸"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    if (s) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("✅ 確定購買").set_id("maple_scbuyok_" + uid_s + "_" + scroll_key).set_style(dpp::cos_success));
    }
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("maple_scshop_" + uid_s + "_0").set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

// ─── 裝備商店 ───────────────────────────────────────────────────────────────
// 分類切換：本職業的各武器類型 + 耳環（所有人）。之後防具再擴充。

// 玩家在裝備商店可看的分類：本職業武器類型 → 頭盔/套服/手套/鞋子 → 耳環
static std::vector<std::string> maple_eqshop_categories(const MapleCharacter& c) {
    std::vector<std::string> cats;
    std::string bj = maple_base_job(c);
    for (auto& wt : MAPLE_WEAPON_TYPES)
        if (wt.job_req == bj) cats.push_back(wt.type_key);
    for (auto& ar : MAPLE_ARMORS) cats.push_back(ar.slot);
    cats.push_back("earring");
    return cats;
}
static std::string maple_eqshop_cat_label(const std::string& cat) {
    if (cat == "earring") return "耳環";
    for (auto& wt : MAPLE_WEAPON_TYPES) if (wt.type_key == cat) return wt.type_cn;
    for (auto& ar : MAPLE_ARMORS)       if (ar.slot     == cat) return ar.slot_cn;
    return cat;
}
// 某件裝備屬於哪個分類 key
static std::string maple_cat_of_item(const MapleItemDef& it) {
    if (it.slot == "earring") return "earring";
    for (auto& ar : MAPLE_ARMORS) if (ar.slot == it.slot) return it.slot;
    for (auto& wt : MAPLE_WEAPON_TYPES) if (wt.type_cn == it.weapon_type) return wt.type_key;
    return "earring";
}

static std::vector<const MapleItemDef*> maple_eqshop_list(const std::string& cat) {
    std::vector<const MapleItemDef*> out;
    if (cat == "earring") {
        for (auto& it : MAPLE_ITEMS)
            if (it.slot == "earring" && it.price > 0) out.push_back(&it);
        return out;
    }
    for (auto& ar : MAPLE_ARMORS) {
        if (ar.slot != cat) continue;
        for (auto& it : MAPLE_ITEMS)
            if (it.slot == cat && it.price > 0) out.push_back(&it);
        return out;
    }
    std::string cn = maple_eqshop_cat_label(cat);
    for (auto& it : MAPLE_ITEMS) {
        if (it.slot != "weapon" || it.price <= 0) continue;
        if (it.weapon_type != cn) continue;
        out.push_back(&it);
    }
    return out;
}

static const int MAPLE_EQSHOP_PAGE_SIZE = 5;

static dpp::message make_maple_eqshop_msg(dpp::snowflake uid, const std::string& cat_in, int page) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);

    auto cats = maple_eqshop_categories(c);
    std::string cat = cat_in;
    if (std::find(cats.begin(), cats.end(), cat) == cats.end())
        cat = cats.empty() ? "earring" : cats.front();

    auto list = maple_eqshop_list(cat);
    int total = (int)list.size();
    int pages = total > 0 ? (total + MAPLE_EQSHOP_PAGE_SIZE - 1) / MAPLE_EQSHOP_PAGE_SIZE : 1;
    if (page < 0) page = 0;
    if (page >= pages) page = pages - 1;

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component header;
    header.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    header.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🎽 裝備商店 — " + maple_eqshop_cat_label(cat)
                     + "（" + std::to_string(page + 1) + "/" + std::to_string(pages) + "）\n"
                     "🪙 瘋幣：**" + std::to_string(c.coins) + "**"));
    msg.add_component_v2(header);

    // 分類切換列（每列最多 5 顆）
    for (size_t i = 0; i < cats.size(); i += 5) {
        dpp::component catrow; catrow.set_type(dpp::cot_action_row);
        for (size_t k = i; k < cats.size() && k < i + 5; k++) {
            catrow.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label(maple_eqshop_cat_label(cats[k]))
                .set_id("maple_eqshop_" + uid_s + "_" + cats[k] + "_0")
                .set_style(cats[k] == cat ? dpp::cos_primary : dpp::cos_secondary)
                .set_disabled(cats[k] == cat));
        }
        msg.add_component_v2(catrow);
    }

    if (total == 0) {
        dpp::component box;
        box.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x95, 0x95, 0x95));
        box.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("這個分類目前沒有可購買的裝備。"));
        msg.add_component_v2(box);
    } else {
        int start = page * MAPLE_EQSHOP_PAGE_SIZE;
        int end   = std::min(start + MAPLE_EQSHOP_PAGE_SIZE, total);
        for (int i = start; i < end; i++) {
            const MapleItemDef& it = *list[i];
            int owned = c.equipment.count(it.key) ? c.equipment.at(it.key) : 0;
            std::string pb;
            if (it.str_bonus) pb += " 力量+" + std::to_string(it.str_bonus);
            if (it.dex_bonus) pb += " 敏捷+" + std::to_string(it.dex_bonus);
            if (it.int_bonus) pb += " 智力+" + std::to_string(it.int_bonus);
            if (it.luk_bonus) pb += " 幸運+" + std::to_string(it.luk_bonus);
            std::string gb;
            if (it.primary_generic)   gb += " 主屬性+" + std::to_string(it.primary_generic);
            if (it.secondary_generic) gb += " 副屬性+" + std::to_string(it.secondary_generic);
            std::string text = "**" + it.name + "**　ID:`" + std::to_string(it.item_id) + "`\n";
            if (it.slot == "weapon")
                text += "⚔️ 攻擊力 " + std::to_string(it.atk_bonus)
                      + "　⚡ " + maple_atk_speed_name(it.atk_speed_sec) + pb + "\n"
                      + "限制：Lv." + std::to_string(it.level_req)
                      + "／副屬性 " + std::to_string(it.secondary_req) + "\n";
            else if (it.slot == "earring")
                text += "無限制　無加成\n";
            else  // 防具
                text += "限制：Lv." + std::to_string(it.level_req)
                      + "／副屬性 " + std::to_string(it.secondary_req)
                      + (gb.empty() ? "　無加成" : "　加成：" + gb) + "\n";
            text += "💰 " + std::to_string(it.price) + " 瘋幣"
                  + (owned > 0 ? "　（持有 " + std::to_string(owned) + "）" : "");
            msg.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label("購買").set_id("maple_eqbuy_" + uid_s + "_" + it.key)
                    .set_style(dpp::cos_success).set_disabled(c.coins < it.price)));
        }
    }

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("◀ 上一頁").set_id("maple_eqshop_" + uid_s + "_" + cat + "_" + std::to_string(page - 1))
        .set_style(dpp::cos_secondary).set_disabled(page <= 0));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶ 下一頁").set_id("maple_eqshop_" + uid_s + "_" + cat + "_" + std::to_string(page + 1))
        .set_style(dpp::cos_secondary).set_disabled(page >= pages - 1));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回商店").set_id("maple_shop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);

    return msg;
}

static dpp::message make_maple_eqbuy_confirm_msg(dpp::snowflake uid, const std::string& item_key) {
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleItemDef* it = maple_find_item(item_key);
    std::string cat = it ? maple_cat_of_item(*it) : "earring";
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    std::string body;
    if (!it) {
        body = "## ❌ 找不到這件裝備";
    } else {
        body = "## 🛒 確認購買\n**" + it->name + "**\n";
        if (it->slot == "weapon")
            body += "⚔️ 攻擊力 " + std::to_string(it->atk_bonus)
                  + "　⚡ " + maple_atk_speed_name(it->atk_speed_sec) + "\n";
        body += "花費 **" + std::to_string(it->price) + "** 瘋幣，確定要購買嗎？";
    }
    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF1, 0xC4, 0x0F));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(body));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    if (it) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("✅ 確定購買").set_id("maple_eqbuyok_" + uid_s + "_" + item_key).set_style(dpp::cos_success));
    }
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("maple_eqshop_" + uid_s + "_" + cat + "_0").set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

// ─── 背包 ───────────────────────────────────────────────────────────────────

static dpp::message make_maple_bag_msg(dpp::snowflake uid, const std::string& tab) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    bool scroll_tab = (tab != "other");

    std::string content = "## 🎒 背包\n🪙 瘋幣：**" + std::to_string(c.coins) + "**\n\n";
    if (scroll_tab) {
        content += "**📜 卷軸**\n";
        bool any = false;
        for (auto& s : MAPLE_SCROLLS) {
            int n = c.scrolls.count(s.key) ? c.scrolls.at(s.key) : 0;
            if (n <= 0) continue;
            any = true;
            content += "• **" + s.name + "**　ID:`" + std::to_string(s.item_id) + "`　×" + std::to_string(n) + "\n";
        }
        if (!any) content += "（沒有卷軸）\n";
        content += "\n-# 可用 `!交易` 指令交換（帶上 ID）";
    } else {
        content += "**📦 裝備（未穿在身上）**\n";
        bool any = false;
        for (auto& [k, n] : c.equipment) {
            if (n <= 0) continue;
            const MapleItemDef* it = maple_find_item(k);
            if (!it) continue;
            any = true;
            content += "• **" + it->name + "**　ID:`" + std::to_string(it->item_id) + "`　×" + std::to_string(n) + "\n";
        }
        for (auto& e : c.enh_items) {
            if (maple_enh_is_equipped(c, e.id)) continue;
            const MapleItemDef* it = maple_find_item(e.base_key);
            if (!it) continue;
            any = true;
            content += "• **" + it->name + "** ✨+" + std::to_string(e.enh_count) + "　🚫不可交易";
            if (e.add_atk || e.add_primary || e.add_secondary) {
                content += "（";
                if (e.add_atk)       content += "攻+" + std::to_string(e.add_atk) + " ";
                if (e.add_primary)   content += "主+" + std::to_string(e.add_primary) + " ";
                if (e.add_secondary) content += "副+" + std::to_string(e.add_secondary);
                content += "）";
            }
            content += "\n";
        }
        if (!any) content += "（沒有備用裝備）\n";
        content += "\n-# 純裝備可用 `!交易` 交換（帶上 ID）；**點過卷軸的裝備不可交易**";
    }

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component tabs; tabs.set_type(dpp::cot_action_row);
    tabs.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📜 卷軸").set_id("maple_bag_" + uid_s + "_scroll")
        .set_style(scroll_tab ? dpp::cos_primary : dpp::cos_secondary).set_disabled(scroll_tab));
    tabs.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📦 其他").set_id("maple_bag_" + uid_s + "_other")
        .set_style(!scroll_tab ? dpp::cos_primary : dpp::cos_secondary).set_disabled(!scroll_tab));
    msg.add_component_v2(tabs);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);

    return msg;
}
