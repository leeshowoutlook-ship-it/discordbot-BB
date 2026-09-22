#pragma once
#include "monster.h"
#include "maple_items.h"
#include <string>
#include <vector>
#include <cmath>
#include <random>

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
static const int MAPLE_LEVEL_CAP = 70; // 配合「70等可以三轉」，剛好能把二轉的40點技能點滿

// 新手加成：角色等級未滿此值時，升級所需經驗打 1/MAPLE_ROOKIE_EXP_MULT 折
// （不是把囤積的經驗 ×4，避免玩家故意卡等級不領、囤一大包 4 倍經驗的漏洞——
//   折扣是逐級套用的，只有「真的把角色從目前等級推到 10 級」的那幾級才享折扣，超過 10 級的部分全額）
static const int    MAPLE_ROOKIE_EXP_LEVEL = 10;
static const double MAPLE_ROOKIE_EXP_MULT  = 4.0;

static int64_t maple_exp_to_next_raw(int level) {
    if (level < 1) level = 1;
    if (level <= (int)MAPLE_EXP_TABLE.size()) return MAPLE_EXP_TABLE[level - 1];
    double v = (double)MAPLE_EXP_TABLE.back();
    for (int lv = (int)MAPLE_EXP_TABLE.size() + 1; lv <= level; lv++) v *= 1.05;
    return (int64_t)llround(v);
}

// 從 level 升到下一級「實際」需要的經驗（含新手折扣）——顯示與結算都用這個
static int64_t maple_exp_to_next(int level) {
    int64_t need = maple_exp_to_next_raw(level);
    if (level < MAPLE_ROOKIE_EXP_LEVEL)
        need = std::max((int64_t)1, (int64_t)llround(need / MAPLE_ROOKIE_EXP_MULT));
    return need;
}

// 目前是否還在吃新手加成（給 UI 顯示提示用）
static double maple_exp_mult(const MapleCharacter& c) {
    return c.level < MAPLE_ROOKIE_EXP_LEVEL ? MAPLE_ROOKIE_EXP_MULT : 1.0;
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
    {"berserker",    "狂戰士",   "warrior",2, "str","dex", 4.8, 1.0},
    {"page",         "見習騎士", "warrior",2, "str","dex", 4.8, 1.0},
    {"icelightning", "冰雷",     "mage",   2, "int","luk", 4.4, 1.0},
    {"priest",       "僧侶",     "mage",   2, "int","luk", 4.4, 1.0},
    {"assassin",     "刺客",     "thief",  2, "luk","dex", 3.6, 1.0},
    {"bandit",       "俠盜",     "thief",  2, "luk","dex", 3.6, 1.0},
    {"hunter",       "獵人",     "archer", 2, "dex","str", 4.2, 1.0},
    {"crossbowman",  "弩弓手",   "archer", 2, "dex","str", 4.2, 1.0},
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
    {"double_throw", "雙飛閃", "快速投擲兩枚飛鏢攻擊敵人（每下套用技能係數，共 2 下）", "thief", 10, "damage_coef",
        {100,108,116,124,130,135,140,144,148,150}, "", {"拳套"}, 2},
    {"haste", "速度激發", "自己與周圍玩家的攻擊間隔縮短（每級 -0.1 秒，滿級 -1 秒）", "thief", 10, "buff_pct",
        {1,2,3,4,5,6,7,8,9,10}, "", {}},
    // 法師
    {"magic_claw", "魔力爪", "運用自身魔力化成尖銳的爪擊攻擊敵人，技能係數如上", "mage", 10, "damage_coef",
        {120,140,160,175,180,190,200,210,220,230}, "", {"法杖"}},
    {"teleport", "瞬間移動", "自己的攻擊間隔縮短（每級 -0.5 秒，滿級 -5 秒）", "mage", 10, "buff_pct",
        {5,10,15,20,25,30,35,40,45,50}, "", {}},
    {"rage_slash", "魔天一擊", "粗暴地揮舞大劍攻擊敵人，技能係數如上", "warrior", 10, "damage_coef",
        {170,190,210,230,250,270,290,310,330,350}, "", {"大劍"}},
    {"endurance", "自身強化", "冒險時擊殺後的休息時間縮短（每級 -2%，滿級 -20%）", "warrior", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    // 弓箭手
    {"double_shot", "斷魂箭", "發射特製的箭矢射擊敵人，技能係數如上", "archer", 10, "damage_coef",
        {196,202,208,214,220,226,234,240,250,260}, "", {"弓", "弩"}},
    {"eagle_eye", "霸王箭", "提升爆擊率，爆擊造成 2 倍傷害（每級 +4%，滿級 +40%）", "archer", 10, "buff_pct",
        {4,8,12,16,20,24,28,32,36,40}, "", {}},

    // ── 二轉：武器精通／領悟／快速武器（每種武器一套，8 種武器 × 3 技能）─────
    // 大劍（狂戰士／見習騎士共用，掛在父職業 warrior 上）
    {"wm_gsword", "大劍精通", "熟練掌握大劍的揮舞訣竅，攻擊力下限隨等級提升（每級 +5% 武器熟練度）", "warrior", 10, "buff_pct",
        {5,10,15,20,25,30,35,40,45,50}, "", {}},
    {"we_gsword", "大劍領悟", "領悟大劍蘊含的奧義，每級主屬性+2、副屬性+1", "warrior", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    {"wq_gsword", "快速大劍", "反覆鍛鍊揮劍節奏，點滿五級時永久提升大劍一階攻速", "warrior", 5, "buff_pct",
        {20,40,60,80,100}, "", {}},
    // 法杖（冰雷／僧侶共用，掛在父職業 mage 上）
    {"wm_staff", "法杖精通", "熟練掌握法杖的施法訣竅，攻擊力下限隨等級提升（每級 +5% 武器熟練度）", "mage", 10, "buff_pct",
        {5,10,15,20,25,30,35,40,45,50}, "", {}},
    {"we_staff", "法杖領悟", "領悟法杖蘊含的奧義，每級主屬性+2、副屬性+1", "mage", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    {"wq_staff", "快速法杖", "反覆鍛鍊施法節奏，點滿五級時永久提升法杖一階攻速", "mage", 5, "buff_pct",
        {20,40,60,80,100}, "", {}},
    // 拳套（刺客）
    {"wm_claw", "拳套精通", "熟練掌握拳套的出拳訣竅，攻擊力下限隨等級提升（每級 +5% 武器熟練度）", "assassin", 10, "buff_pct",
        {5,10,15,20,25,30,35,40,45,50}, "", {}},
    {"we_claw", "拳套領悟", "領悟拳套蘊含的奧義，每級主屬性+2、副屬性+1", "assassin", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    {"wq_claw", "快速拳套", "反覆鍛鍊出拳節奏，點滿五級時永久提升拳套一階攻速", "assassin", 5, "buff_pct",
        {20,40,60,80,100}, "", {}},
    // 匕首（俠盜）
    {"wm_dagger", "匕首精通", "熟練掌握匕首的揮舞訣竅，攻擊力下限隨等級提升（每級 +5% 武器熟練度）", "bandit", 10, "buff_pct",
        {5,10,15,20,25,30,35,40,45,50}, "", {}},
    {"we_dagger", "匕首領悟", "領悟匕首蘊含的奧義，每級主屬性+2、副屬性+1", "bandit", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    {"wq_dagger", "快速匕首", "反覆鍛鍊揮舞節奏，點滿五級時永久提升匕首一階攻速", "bandit", 5, "buff_pct",
        {20,40,60,80,100}, "", {}},
    // 弓（獵人）
    {"wm_bow", "弓精通", "熟練掌握弓的拉弦訣竅，攻擊力下限隨等級提升（每級 +5% 武器熟練度）", "hunter", 10, "buff_pct",
        {5,10,15,20,25,30,35,40,45,50}, "", {}},
    {"we_bow", "弓領悟", "領悟弓術蘊含的奧義，每級主屬性+2、副屬性+1", "hunter", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    {"wq_bow", "快速弓", "反覆鍛鍊拉弦節奏，點滿五級時永久提升弓兩階攻速", "hunter", 5, "buff_pct",
        {20,40,60,80,100}, "", {}},
    // 弩（弩弓手）
    {"wm_xbow", "弩精通", "熟練掌握弩的上弦訣竅，攻擊力下限隨等級提升（每級 +5% 武器熟練度）", "crossbowman", 10, "buff_pct",
        {5,10,15,20,25,30,35,40,45,50}, "", {}},
    {"we_xbow", "弩領悟", "領悟弩術蘊含的奧義，每級主屬性+2、副屬性+1", "crossbowman", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    {"wq_xbow", "快速弩", "反覆鍛鍊上弦節奏，點滿五級時永久提升弩一階攻速", "crossbowman", 5, "buff_pct",
        {20,40,60,80,100}, "", {}},

    // ── 二轉：職業專屬技能（每職業 15 點：1~5 一組、1~10 一組）───────────────
    {"frenzy_berserker", "狂躁", "陷入戰鬥的狂熱，攻擊會不定期附加一次額外攻擊（1級每9下觸發一次、5級每5下觸發一次，此處數值換算成平均加成%）",
        "berserker", 5, "buff_pct", {11.1,12.5,14.3,16.7,20.0}, "", {}},
    {"strong_berserker", "遇強則強", "在野外首領戰中，找到首領後能挑戰的時間跟著延長", "berserker", 10, "buff_pct",
        {1,2,3,4,5,6,7,8,9,10}, "", {}},

    {"element_page", "屬性賦予", "初步掌握屬性的使用方式，根據點數增強自己的輸出能力（每級 +4%）", "page", 5, "buff_pct",
        {4,8,12,16,20}, "", {}},
    {"strong_page", "遇強則強", "在野外首領戰中，找到首領後能挑戰的時間跟著延長", "page", 10, "buff_pct",
        {1,2,3,4,5,6,7,8,9,10}, "", {}},

    {"mana_boost_ice", "魔力強化", "熟練掌握魔力的運行法則，根據點數強化魔力流動（每級 +2 攻擊力、魔力爪技能係數 +10%），點滿十級時額外永久提升法杖一階攻速", "icelightning", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},
    {"mana_resist_ice", "魔法封印", "總能找到野外首領法力薄弱的弱點，對野外首領傷害 +3~30%；同時封印一般怪物的抵抗力，對普通怪物傷害 +1~10%",
        "icelightning", 10, "buff_pct", {3,6,9,12,15,18,21,24,27,30}, "", {}},

    {"angel_blessing", "天使祝福", "獲得來自上蒼的注視，獲得神明恩惠（每級 +1 攻擊力、+5% 爆擊率）", "priest", 5, "buff_pct",
        {1,2,3,4,5}, "", {}},
    {"group_heal", "群體恢復", "不需要使用生命藥水，因此瘋幣收益略為增加（每級 +1%）", "priest", 10, "buff_pct",
        {1,2,3,4,5,6,7,8,9,10}, "", {}},

    {"curse_assassin", "詛咒術", "增加對野外首領、突襲戰首領的傷害（每級 +1%）", "assassin", 5, "buff_pct",
        {1,2,3,4,5}, "", {}},
    {"power_throw", "強力投擲", "開始掌握暗器的投擲技巧，提升爆擊率（每級 +5%）", "assassin", 10, "buff_pct",
        {5,10,15,20,25,30,35,40,45,50}, "", {}},

    {"curse_bandit", "詛咒術", "增加對野外首領、突襲戰首領的傷害（每級 +1%）", "bandit", 5, "buff_pct",
        {1,2,3,4,5}, "", {}},
    {"spin_slash", "迴旋斬", "用匕首對敵人進行快速斬擊，技能係數如上，共可攻擊 8 下", "bandit", 10, "damage_coef",
        {20,22,24,26,28,30,32,34,36,38}, "", {"匕首"}, 8},

    {"dragon_arrow_hunter", "龍魂箭", "對發射的箭矢灌注龍之力，增加總傷害（每級 +1%）", "hunter", 5, "buff_pct",
        {1,2,3,4,5}, "", {}},
    {"invisible_bow", "無形之弓", "對弓的掌握越發熟練，減少擊殺後休息時間（每級 -2%）", "hunter", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},

    {"dragon_arrow_xbow", "龍魂箭", "對發射的箭矢灌注龍之力，增加總傷害（每級 +1%）", "crossbowman", 5, "buff_pct",
        {1,2,3,4,5}, "", {}},
    {"invisible_xbow", "無形之弩", "對弩的掌握越發熟練，減少擊殺後休息時間（每級 -2%）", "crossbowman", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, "", {}},

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
// 取某 buff 技能目前等級的數值（未學會回 0）
static double maple_buff_value(const MapleCharacter& c, const std::string& key) {
    const MapleSkillDef* sd = maple_find_skill(key);
    if (!sd) return 0.0;
    int lvl = maple_skill_level(c, key);
    if (lvl <= 0 || lvl > (int)sd->values.size()) return 0.0;
    return sd->values[lvl - 1];
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

// 轉職後仍要看得到之前職業的技能：一律顯示初心者技能，加上目前一轉父職業的技能（武器共用的技能掛在父職業上），
// 二轉後再加上該二轉職業自己專屬的技能
static std::vector<std::string> maple_visible_skill_jobs(const MapleCharacter& c) {
    std::vector<std::string> out = {"beginner"};
    const MapleJobDef& j = maple_job_of(c);
    if (j.tier == 1) out.push_back(j.key);
    else if (j.tier == 2) { out.push_back(j.parent); out.push_back(j.key); }
    return out;
}
static bool maple_skill_visible(const MapleCharacter& c, const std::string& skill_job) {
    for (auto& vj : maple_visible_skill_jobs(c)) if (vj == skill_job) return true;
    return false;
}

// ─── 裝備欄位 ───────────────────────────────────────────────────────────────
// 顯示／更換順序固定：武器、耳環、戒指、項鍊、頭盔、手套、套服、鞋子

struct MapleSlotDef { std::string key, name, icon; };
static const std::vector<MapleSlotDef> MAPLE_SLOTS = {
    {"weapon",   "武器", "⚔️"},
    {"earring",  "耳環", "👂"},
    {"ring",     "戒指", "💍"},
    {"necklace", "項鍊", "📿"},
    {"helmet",   "頭盔", "🪖"},
    {"glove",    "手套", "🧤"},
    {"clothes",  "套服", "👘"},
    {"shoes",    "鞋子", "👟"},
};

// MapleItemDef／MAPLE_ITEMS／MAPLE_WEAPON_TYPES／MAPLE_ARMORS 等裝備資料定義都搬到 maple_items.h 了。

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
    if (slot == "weapon")   return c.eq_weapon;
    if (slot == "earring")  return c.eq_earring;
    if (slot == "ring")     return c.eq_ring;
    if (slot == "necklace") return c.eq_necklace;
    if (slot == "helmet")   return c.eq_helmet;
    if (slot == "glove")    return c.eq_glove;
    if (slot == "clothes")  return c.eq_clothes;
    if (slot == "shoes")    return c.eq_shoes;
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
    static const char* slots[] = {"weapon","earring","ring","necklace","helmet","glove","clothes","shoes"};
    for (auto* s : slots) if (maple_eq_enh_id(maple_equipped_raw(c, s)) == id) return true;
    return false;
}

// ─── 售出（把汰換掉的裝備換回瘋幣）────────────────────────────────────────────
// 基礎售價：商店有賣的裝備＝售價的40%；商店沒賣（首領掉落的特殊裝備）給固定保底價。
static int64_t maple_item_sell_price(const MapleItemDef& it) {
    if (it.key == "earring_snail") return 8000;  // 蝸牛殼耳環：紅寶王3%掉落
    if (it.key == "wpn_rod_club")  return 15000; // 原始人棒槌：樹妖王1%掉落
    if (it.key.rfind("wpn_underworld_", 0) == 0) return 3000; // 冥界武器系列：冥界幽靈10%掉落
    if (it.key == "arm_underworld_clothes")      return 6000;  // 冥界套服：冥界幽靈3%掉落
    if (it.key == "wpn_golden_staff")            return 20000; // 黃金杖：殭屍猴王5%掉落
    if (it.key == "wpn_wolf_fang")               return 15000; // 狼牙：雪山巨狼5%掉落
    if (it.price > 0) return (int64_t)std::ceil(it.price * 0.4);
    return 1000; // 其他沒設售價、也沒特別列出的道具的保底售價
}
// 強化過的裝備，卷軸疊加的加成額外折算的售價（每點主/攻/副屬性 300 瘋幣，簡單估一個數字）
static int64_t maple_enh_extra_sell_value(const MapleEnhItem& e) {
    return (int64_t)(e.add_atk + e.add_primary + e.add_secondary) * 300;
}

// 目前裝備武器的類型（法杖/大劍/弓...），未裝備武器回傳空字串
static std::string maple_weapon_type(const MapleCharacter& c) {
    const MapleItemDef* w = maple_find_item(maple_equipped_key(c, "weapon"));
    return (w && w->slot == "weapon") ? w->weapon_type : std::string();
}
// 目前裝備武器對應的武器類型 key（gsword/staff/claw/dagger/xbow/bow/gun/knuckle），用來找對應的武器精通/領悟/快速武器技能
static std::string maple_current_weapon_type_key(const MapleCharacter& c) {
    std::string cn = maple_weapon_type(c);
    if (cn.empty()) return "";
    for (auto& wt : MAPLE_WEAPON_TYPES) if (wt.type_cn == cn) return wt.type_key;
    return "";
}
// 一階更快的攻速（秒／下）：90→70→60→45→30，已經是最快就不變
static int maple_atk_speed_one_tier_faster(int sec) {
    static const int TIERS[] = {90, 70, 60, 45, 30}; // 慢→快，索引越大越快
    for (int i = 0; i < 5; i++)
        if (TIERS[i] == sec) return TIERS[std::min(4, i + 1)]; // 往「快」的方向移一階
    return sec;
}

// 目前裝備武器的攻速（秒／下）；未裝備武器則用預設；點滿「快速武器」永久快一階；
// 冰雷點滿「魔力強化」再額外快一階（跟快速法杖疊加）
static int maple_atk_speed_sec(const MapleCharacter& c) {
    const MapleItemDef* w = maple_find_item(maple_equipped_key(c, "weapon"));
    int base = (w && w->slot == "weapon") ? w->atk_speed_sec : MAPLE_ATK_SPEED_DEFAULT_SEC;
    std::string tk = maple_current_weapon_type_key(c);
    if (!tk.empty() && maple_skill_maxed(c, "wq_" + tk)) {
        base = maple_atk_speed_one_tier_faster(base);
        if (tk == "bow") base = maple_atk_speed_one_tier_faster(base); // 快速弓額外多快一階（共兩階），補償弓天生攻擊力比弩低
    }
    if (maple_skill_maxed(c, "mana_boost_ice")) base = maple_atk_speed_one_tier_faster(base);
    // 狼牙：裝備這把拳套時，使用「雙飛閃」攻速額外快一階
    if (c.adv_atk_skill == "double_throw" && maple_equipped_key(c, "weapon") == "wpn_wolf_fang")
        base = maple_atk_speed_one_tier_faster(base);
    return base;
}

// 已裝備道具在某能力值上的加總；excl_slot 指定的部位不計入（""＝全部計入）
static int maple_equip_stat_bonus_excl(const MapleCharacter& c, const std::string& stat,
                                       const std::string& excl_slot) {
    int total = 0;
    const MapleJobDef& j = maple_job_of(c);
    static const char* slots[] = {"weapon","earring","ring","necklace","helmet","glove","clothes","shoes"};
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

// 武器領悟加成：每級主屬性+2、副屬性+1（依目前裝備的武器類型找對應技能）
static int maple_weapon_enlighten_bonus(const MapleCharacter& c, const std::string& stat) {
    std::string tk = maple_current_weapon_type_key(c);
    if (tk.empty()) return 0;
    int lvl = maple_skill_level(c, "we_" + tk);
    if (lvl <= 0) return 0;
    const MapleJobDef& j = maple_job_of(c);
    if (stat == j.primary_stat)   return lvl * 2;
    if (stat == j.secondary_stat) return lvl * 1;
    return 0;
}

// 能力值總和（自己分配 + 裝備加成 + 武器領悟）
static int maple_stat_value(const MapleCharacter& c, const std::string& stat) {
    return maple_stat_alloc(c, stat) + maple_equip_stat_bonus(c, stat) + maple_weapon_enlighten_bonus(c, stat);
}

// 顯示用：「總和 (自己+裝備)」；沒有額外加成時只顯示總和
static std::string maple_stat_breakdown(const MapleCharacter& c, const std::string& stat) {
    int a = maple_stat_alloc(c, stat);
    int e = maple_equip_stat_bonus(c, stat) + maple_weapon_enlighten_bonus(c, stat);
    if (e == 0) return std::to_string(a);
    return std::to_string(a + e) + " (" + std::to_string(a) + "+" + std::to_string(e) + ")";
}

static void maple_set_equipped(MapleCharacter& c, const std::string& slot, const std::string& key) {
    if      (slot == "weapon")   c.eq_weapon   = key;
    else if (slot == "earring")  c.eq_earring  = key;
    else if (slot == "ring")     c.eq_ring     = key;
    else if (slot == "necklace") c.eq_necklace = key;
    else if (slot == "helmet")   c.eq_helmet   = key;
    else if (slot == "glove")    c.eq_glove    = key;
    else if (slot == "clothes")  c.eq_clothes  = key;
    else if (slot == "shoes")    c.eq_shoes    = key;
}

// 角色的一轉基準職業（二轉回傳其父職業；初心者回傳 "beginner"）
static std::string maple_base_job(const MapleCharacter& c) {
    const MapleJobDef& j = maple_job_of(c);
    if (j.tier == 2) return j.parent;
    return j.key;
}

// ─── 技能點數投入的等級限制 ─────────────────────────────────────────────────
// 這技能是不是「二轉技能」：武器精通/領悟/快速武器（wm_/we_/wq_ 開頭，共用武器所以掛在一轉父職業上顯示）
// 或掛在二轉職業(tier==2)本身的職業專屬技能。跟顯示用的 job tag 無關，因為共用武器技能顯示時掛在一轉職業上。
static bool maple_skill_is_tier2(const MapleSkillDef& sd) {
    if (sd.key.rfind("wm_", 0) == 0 || sd.key.rfind("we_", 0) == 0 || sd.key.rfind("wq_", 0) == 0) return true;
    const MapleJobDef* j = maple_find_job(sd.job);
    return j && j->tier == 2;
}
// 初心者技能已投入點數總和
static int maple_beginner_sp_spent(const MapleCharacter& c) {
    int total = 0;
    for (auto& s : MAPLE_SKILLS) if (s.job == "beginner") total += maple_skill_level(c, s.key);
    return total;
}
// 一轉技能（不含掛在一轉職業上顯示的二轉共用武器技能）已投入點數總和，依角色目前的一轉基準職業計算
static int maple_tier1_sp_spent(const MapleCharacter& c) {
    std::string base = maple_base_job(c);
    int total = 0;
    for (auto& s : MAPLE_SKILLS) {
        if (s.job != base || maple_skill_is_tier2(s)) continue;
        total += maple_skill_level(c, s.key);
    }
    return total;
}
static const int MAPLE_TIER1_UNLOCK_BEGINNER_SP = 6;  // 一轉技能：初心者技能至少投入6點才能點
static const int MAPLE_TIER2_UNLOCK_TIER1_SP     = 20; // 二轉技能：一轉技能至少投入20點（滿級）才能點
// 這個技能目前是否可以投入點數（除了「還有剩餘點數、還沒滿級」以外的額外限制）
static bool maple_skill_unlockable(const MapleCharacter& c, const MapleSkillDef& sd) {
    if (sd.job == "beginner") return true;
    if (maple_skill_is_tier2(sd))
        return maple_job_of(c).tier == 2 && maple_tier1_sp_spent(c) >= MAPLE_TIER2_UNLOCK_TIER1_SP;
    return maple_beginner_sp_spent(c) >= MAPLE_TIER1_UNLOCK_BEGINNER_SP;
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

// 武器精通：影響下限公式的熟練度＝基礎 10% ＋ 每級 5%（依目前裝備的武器類型找對應技能）
static double maple_eff_weapon_mastery(const MapleCharacter& c) {
    std::string tk = maple_current_weapon_type_key(c);
    double bonus = tk.empty() ? 0.0 : maple_buff_value(c, "wm_" + tk) / 100.0;
    return 0.10 + bonus;
}
// 二轉職業技能對「總傷害」的加成百分比（狂躁／屬性賦予／龍魂箭），套用在最終傷害上，跟選用哪種攻擊方式無關
static double maple_job_dmg_pct_bonus(const MapleCharacter& c) {
    double pct = maple_buff_value(c, "frenzy_berserker")
               + maple_buff_value(c, "element_page")
               + maple_buff_value(c, "dragon_arrow_hunter")
               + maple_buff_value(c, "dragon_arrow_xbow");
    return pct / 100.0;
}
// 魔力強化：每級讓「魔力爪」的技能係數本身 +10 個百分點（滿級230%+100%=330%），只在目前選用魔力爪時生效
static double maple_skill_coef_bonus_pct(const MapleCharacter& c, const std::string& skill_key) {
    if (skill_key == "magic_claw") return maple_skill_level(c, "mana_boost_ice") * 10.0;
    return 0.0;
}
// 二轉職業技能對「攻擊力」的固定加成（魔力強化／天使祝福／續能激發）
static int64_t maple_job_flat_atk_bonus(const MapleCharacter& c) {
    return (int64_t)(maple_buff_value(c, "mana_boost_ice")
                    + maple_buff_value(c, "angel_blessing")
                    + maple_buff_value(c, "energy_boost"));
}
// 章魚砲台：額外增加「武器攻擊力」一定比例的攻擊力（吃武器本身的 ATK，不是乘算完的總攻擊力）
static int64_t maple_job_mult_atk_bonus(const MapleCharacter& c) {
    double pct = maple_buff_value(c, "octopus_turret");
    return pct > 0 ? (int64_t)std::ceil(maple_total_atk(c) * pct / 100.0) : 0;
}

// 攻擊力是一個範圍：最大＝主屬性係數全開；最小＝主屬性係數只算 0.9*熟練度（基礎10%，武器精通每級+5%）
// 兩者最後都要 /100。若玩家選擇了攻擊技能：damage_coef 套用技能係數取代基礎100%；
// damage_fixed 直接固定傷害（不吃屬性，最大最小相同）。算完後再疊加二轉職業技能的固定/百分比/倍率加成。
static int64_t maple_atk_power_max(const MapleCharacter& c) {
    const MapleJobDef& j = maple_job_of(c);
    double primary   = maple_stat_value(c, j.primary_stat);
    double secondary = maple_stat_value(c, j.secondary_stat);
    int64_t base = (int64_t)std::ceil((j.primary_coef * primary + j.secondary_coef * secondary) * maple_total_atk(c) / 100.0);
    int64_t result;
    const MapleSkillDef* sd = maple_current_atk_skill(c);
    if (!sd) result = base;
    else {
        int lvl = maple_skill_level(c, sd->key);
        double coef = sd->values[lvl-1] + maple_skill_coef_bonus_pct(c, sd->key);
        result = (sd->type == "damage_fixed") ? (int64_t)sd->values[lvl-1] * sd->hits
                                               : (int64_t)std::ceil(base * coef / 100.0 * sd->hits);
    }
    result += maple_job_flat_atk_bonus(c);
    result = (int64_t)std::ceil(result * (1.0 + maple_job_dmg_pct_bonus(c)));
    result += maple_job_mult_atk_bonus(c);
    return result;
}
static int64_t maple_atk_power_min(const MapleCharacter& c) {
    const MapleJobDef& j = maple_job_of(c);
    double primary   = maple_stat_value(c, j.primary_stat);
    double secondary = maple_stat_value(c, j.secondary_stat);
    double mastery = maple_eff_weapon_mastery(c);
    int64_t base = (int64_t)std::ceil((j.primary_coef * 0.9 * mastery * primary + j.secondary_coef * secondary) * maple_total_atk(c) / 100.0);
    int64_t result;
    const MapleSkillDef* sd = maple_current_atk_skill(c);
    if (!sd) result = base;
    else {
        int lvl = maple_skill_level(c, sd->key);
        double coef = sd->values[lvl-1] + maple_skill_coef_bonus_pct(c, sd->key);
        result = (sd->type == "damage_fixed") ? (int64_t)sd->values[lvl-1] * sd->hits
                                               : (int64_t)std::ceil(base * coef / 100.0 * sd->hits);
    }
    result += maple_job_flat_atk_bonus(c);
    result = (int64_t)std::ceil(result * (1.0 + maple_job_dmg_pct_bonus(c)));
    result += maple_job_mult_atk_bonus(c);
    return result;
}
static double maple_atk_power_avg(const MapleCharacter& c) {
    return (maple_atk_power_min(c) + maple_atk_power_max(c)) / 2.0;
}

// ─── 卷軸商店 ───────────────────────────────────────────────────────────────
// MapleScrollDef／MAPLE_SCROLLS 資料表搬到 maple_items.h 了，這裡只留邏輯。

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
    if (s.restore_slot) return "歸還1次已使用的卷軸次數（只能用在已有強化失敗紀錄的裝備）";
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
    if      (slot == "earring")  cn = "耳環";
    else if (slot == "ring")     cn = "戒指";
    else if (slot == "necklace") cn = "項鍊";
    else if (slot == "helmet")   cn = "頭盔";
    else if (slot == "glove")    cn = "手套";
    else if (slot == "clothes")  cn = "套服";
    else if (slot == "shoes")    cn = "鞋子";
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
    std::string raw = maple_equipped_raw(c, slot);

    // 純白卷軸：只能用在已經有強化失敗紀錄的裝備上，不佔用強化次數上限，
    // 50%成功歸還1次已使用的卷軸次數／50%爆炸裝備直接消失（跟其他詛咒卷軸一樣沒有「純失敗」的中間結果）。
    if (s->restore_slot) {
        if (!maple_eq_is_enh(raw)) return "這件裝備沒有強化失敗紀錄，不能使用純白卷軸。";
        MapleEnhItem* e = maple_find_enh(c, maple_eq_enh_id(raw));
        if (!e) return "強化實例遺失，請重試。";
        if (e->slots_used <= e->enh_count) return "這件裝備沒有強化失敗紀錄，不能使用純白卷軸。";
        sit->second--;
        if (sit->second <= 0) c.scrolls.erase(sit);
        ok = true;
        static std::mt19937 white_rng(std::random_device{}());
        int roll = std::uniform_int_distribution<int>(0, 99)(white_rng);
        if (roll >= s->rate) {
            int eid = e->id;
            maple_set_equipped(c, slot, "");
            c.enh_items.erase(std::remove_if(c.enh_items.begin(), c.enh_items.end(),
                                              [&](const MapleEnhItem& x){ return x.id == eid; }), c.enh_items.end());
            return "💥💥 純白卷軸失敗，裝備直接爆炸消失了！這個部位現在是空的。";
        }
        e->slots_used--;
        return "✨ 純白卷軸成功！已歸還 1 次可用卷軸次數，目前已使用 "
             + std::to_string(e->slots_used) + "/" + std::to_string(max_slots) + " 次。";
    }

    // 取得（必要時建立）此部位的強化實例
    MapleEnhItem* e = nullptr;
    if (maple_eq_is_enh(raw)) {
        e = maple_find_enh(c, maple_eq_enh_id(raw));
        if (e && e->slots_used >= max_slots)
            return "🚫 這件裝備的強化次數已用完（上限 " + std::to_string(max_slots) + " 次）。";
    } else {
        // 把目前「穿在身上」這件純裝備升級成強化實例。
        // 注意：這裡不能去扣 c.equipment[base]——穿在身上的這件早在裝備當下就已經從背包扣掉了，
        // c.equipment[base] 這時候如果有數量，代表的是「另外的備用品」，不是這件正在被強化的本體，
        // 扣它會誤刪玩家的備用裝備（同一件武器穿一把、包包裡還放一把沒強化的情況就會踩到）。
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
    static std::mt19937 enh_rng(std::random_device{}());
    int roll = std::uniform_int_distribution<int>(0, 99)(enh_rng);
    bool success = roll < s->rate;
    bool boom    = !success && s->explode_pct > 0 && roll < s->rate + s->explode_pct;
    if (boom) {
        // 詛咒卷軸爆炸：裝備直接消失（連同已疊加的強化效果一起沒了），這個部位變回空的
        int eid = e->id;
        maple_set_equipped(c, slot, "");
        c.enh_items.erase(std::remove_if(c.enh_items.begin(), c.enh_items.end(),
                                          [&](const MapleEnhItem& x){ return x.id == eid; }), c.enh_items.end());
        return "💥💥 詛咒發動，裝備直接爆炸消失了！這個部位現在是空的。";
    }
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
    std::string bonus_job;    // 特定職業在這張圖有攻擊力加成，"" = 無（key 對應 MAPLE_JOBS）
    double      bonus_mult = 1.0; // 該職業的攻擊力倍率
};

static const std::vector<MapleAdvRegionDef> MAPLE_ADV_REGIONS = {
    {"archer_range",         "弓箭手訓練場", 1,  true, {"紅寶",     40,   8,  12,  18}},
    {"trapdoor",             "小心掉落",     10, true, {"三眼章魚", 200, 24,  24,  36}},
    {"blue_mushroom_forest", "藍菇菇樹林",   15, true, {"藍菇菇",   350, 32,  36,  54}},
    {"ruins_dig_site",       "遺跡發掘地",   20, true, {"石面怪人", 600, 45,  44,  66}},
    {"aiosta_57f",           "愛奧斯塔57層", 25, true, {"兔子鼓手", 950, 60,  97, 139}},
    // 掉落幣先用 exp×1.5 抓的暫定值（±20%），等你給正式數字
    {"water_canyon",         "水中峽谷",     30, true, {"粉紅小海豹", 1550,  81, 100, 145}},
    {"monkey_swamp_3",       "猴子沼澤地III", 35, true, {"天使猴",     1800,  90, 110, 160}, "priest", 1.5},
    {"time_road_4",          "時間之路<4>",  40, true, {"妖魔隊長",   2600, 115, 140, 205}, "priest", 1.5},
    {"dragon_hunting_ground","龍族狩獵場",   45, true, {"青龍",       3200, 135, 165, 240}},
};

static const MapleAdvRegionDef* maple_find_adv_region(const std::string& key) {
    for (auto& r : MAPLE_ADV_REGIONS) if (r.key == key) return &r;
    return nullptr;
}

// 擊殺一隻怪物後的休息秒數（休息完才能開始打下一隻）
static const int64_t MAPLE_ADV_REST_SEC = 60;

// ─── buff 技能效果 ─────────────────────────────────────────────────────────
// 有效攻擊間隔（秒）：底攻速 − 瞬間移動/速度激發/衝鋒 縮減；下限 = max(5, 底值×30%)
static int maple_eff_atk_interval(const MapleCharacter& c) {
    double base = maple_atk_speed_sec(c);
    double cut = maple_buff_value(c, "teleport") * 0.1   // 瞬間移動：每級 0.1 秒 → 滿級 -2 秒
               + maple_buff_value(c, "haste")    * 0.1   // 速度激發：每級 0.1 秒 → 滿級 -1 秒（全體，solo 吃自己）
               + maple_buff_value(c, "charge")            // 衝鋒：每級約 0.5 秒 → 滿級 -5 秒
               + maple_buff_value(c, "energy_boost") * 0.5; // 續能激發：每級 0.5 秒 → 滿級 -5 秒
    double floor_v = std::max(5.0, base * 0.3);
    double v = base - cut;
    if (v < floor_v) v = floor_v;
    return (int)llround(v);
}
// 有效休息秒數：MAPLE_ADV_REST_SEC 秒 − 自身強化/衝鋒/無形之弓弩 的百分比縮減（合計上限 70%），
// 再扣掉續能激發的固定秒數縮減，下限 5 秒
static int maple_eff_rest_sec(const MapleCharacter& c) {
    double pct = maple_buff_value(c, "endurance")        // 自身強化：每級 2% → 滿級 20%
               + maple_buff_value(c, "charge")            // 衝鋒：每級約 0.5% → 滿級 5%
               + maple_buff_value(c, "invisible_bow")      // 無形之弓：每級 2% → 滿級 20%
               + maple_buff_value(c, "invisible_xbow");    // 無形之弩：每級 2% → 滿級 20%
    if (pct > 70.0) pct = 70.0;
    double v = MAPLE_ADV_REST_SEC * (1.0 - pct / 100.0) - maple_buff_value(c, "energy_boost"); // 續能激發：每級 -1 秒
    if (v < 5.0) v = 5.0;
    return (int)llround(v);
}
// 爆擊平均加成倍率（霸王箭／強力投擲／天使祝福）：爆擊率 p、爆擊 2 倍傷害 → 平均 = 1 + p
static double maple_crit_avg_mult(const MapleCharacter& c) {
    double crit_pct = maple_buff_value(c, "eagle_eye")     // 每級 4% → 滿級 40%
                     + maple_buff_value(c, "power_throw")  // 每級 5% → 滿級 50%
                     + maple_skill_level(c, "angel_blessing") * 5.0; // 每級 5% → 滿級(5級)25%
    return 1.0 + crit_pct / 100.0;
}

// 特定職業在某張圖有固定攻擊力倍率加成（例如僧侶在猴子沼澤地III／時間之路<4> ×1.5）
static double maple_adv_region_job_mult(const MapleCharacter& c, const MapleAdvRegionDef& region) {
    return (!region.bonus_job.empty() && c.job == region.bonus_job) ? region.bonus_mult : 1.0;
}

// 魔法封印對「一般怪物」的傷害加成（每級+1%，滿級10%）：只在冒險/練等場景套用，跟野外首領那組+3~30%分開算
static double maple_adv_dmg_pct_bonus(const MapleCharacter& c) {
    return 1.0 + maple_skill_level(c, "mana_resist_ice") * 1.0 / 100.0;
}

// 等差懲罰：玩家等級低於區域建議等級「5 級以上」，超過的部分每低 1 級傷害 -1%
// 例：Lv35 區域、玩家 Lv29 → 差 6 級，超過門檻 1 級 → ×0.99
static double maple_adv_underlevel_mult(const MapleCharacter& c, const MapleAdvRegionDef& region) {
    int gap = region.suggested_level - c.level;
    int excess = gap > 5 ? gap - 5 : 0;
    double mult = 1.0 - 0.01 * excess;
    return mult < 0.0 ? 0.0 : mult;
}

// 擊殺一隻怪物需要的攻擊次數（用平均傷害＋爆擊期望算，無條件進位，最少1下）
static int maple_adv_hits_to_kill(const MapleCharacter& c, const MapleAdvRegionDef& region) {
    double avg_dmg = std::max(1.0, maple_atk_power_avg(c) * maple_crit_avg_mult(c)
                                  * maple_adv_region_job_mult(c, region)
                                  * maple_adv_underlevel_mult(c, region)
                                  * maple_adv_dmg_pct_bonus(c));
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
// 每隻平均瘋幣，套用「群體恢復」加成（每級 +1%，滿級 +10%）
static int64_t maple_adv_coins_per_kill(const MapleCharacter& c, const MapleAdvRegionDef& region) {
    double base = (region.monster.coin_min + region.monster.coin_max) / 2.0;
    return (int64_t)llround(base * (1.0 + maple_buff_value(c, "group_heal") / 100.0));
}

// 估算每小時擊殺數／經驗／瘋幣（依「殺滿一隻才有收益」的離散模型）
static void maple_adv_estimate(const MapleCharacter& c, const MapleAdvRegionDef& region,
                               double& kills_per_hour, double& exp_per_hour, double& coins_per_hour) {
    int64_t spk = maple_adv_seconds_per_kill(c, region);
    kills_per_hour = spk > 0 ? 3600.0 / spk : 0.0;
    exp_per_hour   = kills_per_hour * region.monster.exp; // 新手加成改為降低升級所需經驗，不在這裡放大
    coins_per_hour = kills_per_hour * maple_adv_coins_per_kill(c, region);
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
    exp_out   = kills * region->monster.exp; // 原始經驗；新手加成在結算時以「降低升級所需經驗」的方式套用
    coins_out = kills * maple_adv_coins_per_kill(c, *region);
}

// ─── 野外首領：全服共用一隻，先搶先贏；用你的攻擊力決定要打多久，
//     過程中被別人先殺掉的話就無功而返（不顯示需要打多久，避免精算卡點）───────

// 掉落表一筆：pct=機率(%)，每筆各自獨立擲骰；set 大小1＝固定掉那個，>1＝從裡面隨機選一個（scroll key 或 item key 皆可，會自動判斷）
struct MapleWbDropEntry { int pct; std::vector<std::string> set; };
struct MapleWbMonsterDef {
    std::string name; int64_t hp; int64_t exp; int64_t coin_min, coin_max;
    std::vector<MapleWbDropEntry> drops;
};
struct MapleWbRegionDef {
    std::string key, name;
    int suggested_level;
    int respawn_min_lo, respawn_min_hi; // 重生間隔範圍（分鐘），本輪關閉時隨機決定實際值；lo==hi 就是固定值
    bool open;
    MapleWbMonsterDef boss;
};

static const std::vector<std::string> MAPLE_WB_WPN60_SET_NO_ROD = {
    "sc_wpn_staff60", "sc_wpn_claw60", "sc_wpn_dagger60", "sc_wpn_bow60",
    "sc_wpn_xbow60", "sc_wpn_gsword60",
};
static const std::vector<std::string> MAPLE_WB_WPN20_SET_NO_ROD = {
    "sc_wpn_staff20", "sc_wpn_claw20", "sc_wpn_dagger20", "sc_wpn_bow20",
    "sc_wpn_xbow20", "sc_wpn_gsword20",
};
// 60%／20%卷軸合併成一包，隨機開出其中一張（不含棒子）
static const std::vector<std::string> MAPLE_WB_WPN60_20_SET_NO_ROD = []{
    std::vector<std::string> v = MAPLE_WB_WPN60_SET_NO_ROD;
    v.insert(v.end(), MAPLE_WB_WPN20_SET_NO_ROD.begin(), MAPLE_WB_WPN20_SET_NO_ROD.end());
    return v;
}();
// 冥界武器系列（6種一般武器類型，不含棒子／火槍／指虎）
static const std::vector<std::string> MAPLE_WB_UNDERWORLD_WPN_SET = {
    "wpn_underworld_gsword", "wpn_underworld_staff", "wpn_underworld_claw", "wpn_underworld_dagger",
    "wpn_underworld_xbow", "wpn_underworld_bow",
};
static const std::vector<std::string> MAPLE_WB_WPN_CURSE50_SET = {
    "sc_wpn_staff_curse50", "sc_wpn_claw_curse50", "sc_wpn_dagger_curse50",
    "sc_wpn_bow_curse50", "sc_wpn_xbow_curse50", "sc_wpn_gsword_curse50",
};

// 依建議等級由低到高排列，清單顯示順序就是這個陣列的順序
static const std::vector<MapleWbRegionDef> MAPLE_WB_REGIONS = {
    {"coastal_grass", "海岸草叢", 5, 50, 50, true, {"紅寶王", 1000, 50, 100, 200, {
        {100, MAPLE_WB_WPN60_SET_NO_ROD},
        {10,  {"sc_earring_curse50"}},
        {3,   {"earring_snail"}},
    }}},
    {"subway_station3", "地鐵三號站", 15, 55, 60, true, {"冥界幽靈", 2300, 60, 150, 250, {
        {50, MAPLE_WB_WPN60_20_SET_NO_ROD},
        {10, MAPLE_WB_UNDERWORLD_WPN_SET},
        {3,  {"arm_underworld_clothes"}},
    }}},
    {"east_rock4", "東方岩石山4", 20, 65, 75, true, {"樹妖王", 3000, 85, 250, 350, {
        {100, MAPLE_WB_WPN20_SET_NO_ROD},
        {40,  {"sc_helmet60", "sc_shoes60", "sc_clothes60"}},
        {15,  {"sc_wpn_rod_curse50"}},
        {5,   {"sc_wpn_rod60"}},
        {1,   {"wpn_rod_club"}},
    }}},
    {"turtle_beach", "海龜沙灘", 30, 45, 95, true, {"寄居蟹", 5000, 100, 280, 400, {
        {50, {"sc_glove_atk100"}},
        {30, {"sc_glove_sec_curse50"}},
        {10, {"sc_glove_atk20"}},
    }}},
    {"ice_canyon2", "冰雪峽谷II", 40, 120, 180, true, {"雪山巨狼", 8000, 115, 320, 460, {
        {25, {"sc_shoes20"}},
        {15, {"sc_shoes_curse50"}},
        {5,  {"wpn_wolf_fang"}},
    }}},
    {"witch_forest", "女巫之森", 50, 90, 120, true, {"殭屍猴王", 15000, 450, 1200, 1800, {
        {80, {"sc_glove_sec60"}},
        {20, {"sc_glove_sec20"}},
        {5,  {"wpn_golden_staff"}},
    }}},
    {"dead_forest4", "亡者之林IV", 65, 50, 120, true, {"厄運死神", 40000, 500, 1350, 2000, {
        {30, {"sc_clothes_curse50"}},
        {20, {"sc_clothes_curse50"}},
        {10, MAPLE_WB_WPN_CURSE50_SET},
    }}},
    {"cursed_temple", "被詛咒的神殿", 80, 300, 480, true, {"巴洛古", 90000, 2500, 8000, 12000, {
        {100, MAPLE_WB_WPN60_SET_NO_ROD},
        {30,  {"sc_glove_atk_curse50"}},
        {15,  {"sc_glove_atk60"}},
    }}},
};

static const MapleWbRegionDef* maple_find_wb_region(const std::string& key) {
    for (auto& r : MAPLE_WB_REGIONS) if (r.key == key) return &r;
    return nullptr;
}

// 這隻首領目前記錄到的「上次被討伐時間」，0＝從未被討伐過。呼叫前需持有 data_mutex。
static time_t maple_wb_dead_since_locked(const std::string& region_key) {
    auto it = maple_wb_state.find(region_key);
    return it == maple_wb_state.end() ? 0 : it->second.dead_since;
}
// 首領目前是否生成中可挑戰（動態依重生間隔推算，不需要背景計時器）。呼叫前需持有 data_mutex。
static bool maple_wb_is_up_locked(const std::string& region_key, time_t now) {
    auto it = maple_wb_state.find(region_key);
    time_t dead_since   = it == maple_wb_state.end() ? 0 : it->second.dead_since;
    int    respawn_secs = it == maple_wb_state.end() ? 0 : it->second.respawn_secs;
    if (dead_since == 0) return true;
    if (respawn_secs <= 0) { // 舊存檔沒有這個欄位時的保底：用區域重生下限
        const MapleWbRegionDef* r = maple_find_wb_region(region_key);
        respawn_secs = (r ? r->respawn_min_lo : 60) * 60;
    }
    return now - dead_since >= respawn_secs;
}
static bool maple_wb_is_up(const std::string& region_key, time_t now) {
    std::lock_guard<std::mutex> lk(data_mutex);
    return maple_wb_is_up_locked(region_key, now);
}

static bool maple_is_wb_fighting(const MapleCharacter& c) { return !c.wb_region.empty(); }

// 目前有幾個人正在挑戰這個區域的首領（呼叫前不可持有 data_mutex）
static int maple_wb_hunters_count(const std::string& region_key) {
    std::lock_guard<std::mutex> lk(data_mutex);
    int n = 0;
    for (auto& [uid, c] : maple_data) if (c.wb_region == region_key) n++;
    return n;
}

// 每一輪野外首領最多幾個人可以擊殺成功並獲得獎勵；湊滿這個人數後才關閉本輪、開始算重生
static const int MAPLE_WB_MAX_WINNERS = 3;

// 打贏這隻首領需要多少秒：跟冒險同一套「平均傷害＋爆擊期望」模型算完一次（不含休息，一次性戰鬥）。
// 詛咒術／法力抗性：對首領傷害加成，直接縮短需要的攻擊次數；遇強則強／偽裝術：找到首領後可挑戰時間延長，直接縮短總耗時。
static int64_t maple_wb_kill_secs(const MapleCharacter& c, const MapleWbMonsterDef& boss) {
    double boss_dmg_bonus = maple_buff_value(c, "curse_assassin") + maple_buff_value(c, "curse_bandit")
                           + maple_buff_value(c, "mana_resist_ice");
    double avg_dmg = std::max(1.0, maple_atk_power_avg(c) * maple_crit_avg_mult(c) * (1.0 + boss_dmg_bonus / 100.0));
    int hits = (int)std::ceil((double)boss.hp / avg_dmg);
    if (hits < 1) hits = 1;
    int64_t secs = (int64_t)hits * maple_eff_atk_interval(c);

    double time_bonus = maple_buff_value(c, "strong_berserker") + maple_buff_value(c, "strong_page")
                       + maple_buff_value(c, "disguise_brawler");
    if (time_bonus > 0) secs = (int64_t)std::ceil(secs * (1.0 - std::min(70.0, time_bonus) / 100.0));
    return secs < 1 ? 1 : secs;
}

// 共用的野外首領亂數產生器（隨機重生秒數用；各處都要用這個，不能用裸的 rand()）
static std::mt19937& maple_wb_rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

// 全服共用的首領狀態，存在獨立檔案（key 不是玩家 uid，不能塞進 maple_data.json 的每人物件迴圈裡）
static const std::string MAPLE_WB_STATE_FILE = "maple_wb_state.json";
static void save_maple_wb_state() {
    nlohmann::json j;
    { std::lock_guard<std::mutex> lk(data_mutex);
      for (auto& [key, st] : maple_wb_state)
          j[key] = {{"dead_since", (int64_t)st.dead_since}, {"round_kills", st.round_kills},
                     {"respawn_secs", st.respawn_secs}, {"total_kills", st.total_kills}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(MAPLE_WB_STATE_FILE, j.dump(2));
}
static void load_maple_wb_state() {
    std::ifstream f(MAPLE_WB_STATE_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [key, v] : j.items()) {
            MapleWorldBossState st;
            st.dead_since   = (time_t)v.value("dead_since", (int64_t)0);
            st.round_kills  = v.value("round_kills", 0);
            st.respawn_secs = v.value("respawn_secs", 0);
            st.total_kills  = v.value("total_kills", (int64_t)0);
            maple_wb_state[key] = st;
        }
    } catch (...) {}
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
                {"eq_ring",           c.eq_ring},
                {"eq_necklace",       c.eq_necklace},
                {"eq_helmet",         c.eq_helmet},
                {"eq_glove",          c.eq_glove},
                {"eq_clothes",        c.eq_clothes},
                {"eq_shoes",          c.eq_shoes},
                {"weapon_mastery",    c.weapon_mastery},
                {"skill_levels",      c.skill_levels},
                {"skill_reset_used",  c.skill_reset_used},
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
                {"adv_last_bracket",  c.adv_last_bracket},
                {"wb_region",         c.wb_region},
                {"wb_started_at",     (int64_t)c.wb_started_at},
                {"wb_required_secs",  c.wb_required_secs},
                {"wb_epoch",          c.wb_epoch},
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
            c.eq_ring           = v.value("eq_ring",           std::string());
            c.eq_necklace       = v.value("eq_necklace",       std::string());
            c.eq_helmet         = v.value("eq_helmet",         std::string());
            c.eq_glove          = v.value("eq_glove",          std::string());
            c.eq_clothes        = v.value("eq_clothes",        std::string());
            c.eq_shoes          = v.value("eq_shoes",          std::string());
            c.weapon_mastery    = v.value("weapon_mastery",    0.10);
            if (v.contains("skill_levels") && v["skill_levels"].is_object())
                c.skill_levels  = v["skill_levels"].get<std::map<std::string,int>>();
            c.skill_reset_used  = v.value("skill_reset_used",  false);
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
            c.adv_last_bracket  = v.value("adv_last_bracket",  -1);
            c.wb_region         = v.value("wb_region",         std::string());
            c.wb_started_at     = (time_t)v.value("wb_started_at", (int64_t)0);
            c.wb_required_secs  = v.value("wb_required_secs",  (int64_t)0);
            c.wb_epoch          = v.value("wb_epoch",          (int64_t)0);
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

    std::string content = "## 🍁 瘋子谷世界\n";
    content += "職業：**" + job.name + "**\n";
    if (c.level >= MAPLE_LEVEL_CAP) {
        content += "**Lv. " + std::to_string(c.level) + "**　（已達等級上限）\n";
    } else {
        int64_t need = maple_exp_to_next(c.level);
        content += "**Lv. " + std::to_string(c.level) + "**　" + hp_bar((int)c.exp, (int)need, 10)
                 + "　" + std::to_string(c.exp) + "/" + std::to_string(need) + " EXP\n";
    }
    if (maple_exp_mult(c) > 1.0)
        content += "🔰 新手加成：**未滿 " + std::to_string(MAPLE_ROOKIE_EXP_LEVEL)
                 + " 級升級所需經驗只要 1/" + std::to_string((int)MAPLE_ROOKIE_EXP_MULT) + "**\n";
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
    msg.add_component_v2(row2);

    dpp::component row3; row3.set_type(dpp::cot_action_row);
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏆 排行榜").set_id("maple_rank_" + uid_s + "_all_0").set_style(dpp::cos_secondary));
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐉 野外首領").set_id("maple_wb_" + uid_s).set_style(dpp::cos_danger));
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💥 突襲首領").set_id("maple_ambush_" + uid_s).set_style(dpp::cos_secondary));
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏰 公會").set_id("maple_guild_" + uid_s).set_style(dpp::cos_secondary));
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row3);

    return msg;
}

// ─── 排行榜：依 等級 → 經驗值 排序 ──────────────────────────────────────────
static const int MAPLE_RANK_PAGE_SIZE = 10;

// 排行榜職業篩選分類：key -> 顯示名稱（含它的一二轉，用 maple_base_job 判斷屬於哪個一轉底）
static const std::vector<std::pair<std::string,std::string>> MAPLE_RANK_FILTERS = {
    {"all",    "全部職業"},
    {"warrior","劍士（含轉職）"},
    {"mage",   "法師（含轉職）"},
    {"thief",  "盜賊（含轉職）"},
    {"archer", "弓箭手（含轉職）"},
};
static std::string maple_rank_filter_label(const std::string& key) {
    for (auto& f : MAPLE_RANK_FILTERS) if (f.first == key) return f.second;
    return "全部職業";
}

static dpp::message make_maple_rank_msg(dpp::snowflake uid, int page, const std::string& filter = "all") {
    std::string uid_s = std::to_string((uint64_t)uid);

    std::vector<std::tuple<int, int64_t, dpp::snowflake>> board; // (level, exp, uid)
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [u, ch] : maple_data) {
            if (filter != "all" && maple_base_job(ch) != filter) continue;
            board.push_back({ch.level, ch.exp, u});
        }
    }
    std::sort(board.begin(), board.end(), [](auto& a, auto& b) {
        if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
        return std::get<1>(a) > std::get<1>(b);
    });

    int total = (int)board.size();
    int pages = std::max(1, (total + MAPLE_RANK_PAGE_SIZE - 1) / MAPLE_RANK_PAGE_SIZE);
    if (page < 0) page = 0;
    if (page >= pages) page = pages - 1;

    int my_rank = -1;
    for (int i = 0; i < total; i++)
        if (std::get<2>(board[i]) == uid) { my_rank = i + 1; break; }

    static const char* MEDALS[] = {"🥇", "🥈", "🥉"};
    std::string content = "## 🏆 養成排行榜\n依 **等級 → 經驗值** 排序，分類：**" + maple_rank_filter_label(filter)
                         + "**（共 " + std::to_string(total) + " 位角色）\n";
    if (my_rank > 0) content += "你目前第 **" + std::to_string(my_rank) + "** 名\n";
    content += "\n";
    int start = page * MAPLE_RANK_PAGE_SIZE;
    int end   = std::min(start + MAPLE_RANK_PAGE_SIZE, total);
    if (total == 0) {
        content += "-# 目前還沒有人建立角色。";
    } else {
        for (int i = start; i < end; i++) {
            auto& [lv, xp, u] = board[i];
            std::string rk = (i < 3) ? MEDALS[i] : (std::to_string(i + 1) + ".");
            content += rk + " <@" + std::to_string((uint64_t)u) + ">　**Lv." + std::to_string(lv)
                     + "**（" + std::to_string(xp) + " EXP）"
                     + (u == uid ? "　⬅ 你" : "") + "\n";
        }
    }

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF1, 0xC4, 0x0F));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));
    msg.add_component_v2(container);

    dpp::component sel_row; sel_row.set_type(dpp::cot_action_row);
    dpp::component sel;
    sel.set_type(dpp::cot_selectmenu).set_id("maple_ranksel_" + uid_s).set_placeholder("選擇職業分類");
    for (auto& f : MAPLE_RANK_FILTERS)
        sel.add_select_option(dpp::select_option(f.second, f.first).set_default(f.first == filter));
    sel_row.add_component(sel);
    msg.add_component_v2(sel_row);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("◀ 上一頁").set_id("maple_rank_" + uid_s + "_" + filter + "_" + std::to_string(page - 1))
        .set_style(dpp::cos_secondary).set_disabled(page <= 0));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶ 下一頁").set_id("maple_rank_" + uid_s + "_" + filter + "_" + std::to_string(page + 1))
        .set_style(dpp::cos_secondary).set_disabled(page >= pages - 1));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);

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
        if (sd.key == "angel_blessing") return "攻擊力 +" + s + "、爆擊率 +" + std::to_string(level * 5) + "%";
        if (sd.key == "mana_resist_ice") return "野外首領傷害 +" + s + "%、一般怪物傷害 +" + std::to_string(level) + "%";
        if (sd.key == "mana_boost_ice")
            return "攻擊力 +" + s + "、魔力爪技能係數 +" + std::to_string(level * 10) + "%"
                 + (level >= sd.max_level ? "、✅ 已解鎖：永久快一階攻速" : "");
        if (sd.key == "octopus_turret") return "額外攻擊力 +武器攻擊力 ×" + s + "%";
        if (sd.key == "energy_boost")
            return "攻擊力 +" + s + "、休息時間 -" + s + " 秒、攻擊間隔 -" + std::to_string(v * 0.5).substr(0,3) + " 秒";
        if (sd.key.rfind("we_", 0) == 0) return "主屬性 +" + s + "、副屬性 +" + std::to_string((int)(v/2)); // 領悟：主2副1
        if (sd.key.rfind("wq_", 0) == 0) return level >= sd.max_level ? "✅ 已解鎖：永久快一階攻速" : "滿級（5）後解鎖";
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
    std::string head = "## 🌟 技能\n剩餘技能點：**" + std::to_string(maple_sp_unspent(c)) + "**";
    bool is_tier2 = maple_job_of(c).tier == 2;
    if (maple_beginner_sp_spent(c) < MAPLE_TIER1_UNLOCK_BEGINNER_SP)
        head += "\n🔒 初心者技能投入滿 " + std::to_string(MAPLE_TIER1_UNLOCK_BEGINNER_SP) + " 點後才能點一轉技能";
    else if (!is_tier2)
        head += "\n🔒 轉職成二轉職業、且一轉技能投入滿 " + std::to_string(MAPLE_TIER2_UNLOCK_TIER1_SP) + " 點後才能點二轉技能";
    else if (maple_tier1_sp_spent(c) < MAPLE_TIER2_UNLOCK_TIER1_SP)
        head += "\n🔒 一轉技能投入滿 " + std::to_string(MAPLE_TIER2_UNLOCK_TIER1_SP) + " 點後才能點二轉技能";
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(head));

    // 轉職後之前職業的技能仍然顯示（初心者技能一律顯示，再加上目前一轉職業的技能）
    for (auto& skill_job : maple_visible_skill_jobs(c)) {
        const MapleJobDef* jd = maple_find_job(skill_job);
        container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
            .set_spacing(dpp::sep_small).set_divider(true));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("**── " + (jd ? jd->name : skill_job) + " ──**"));
        for (auto* sd : maple_skills_for_job(skill_job)) {
            // 二轉技能（含掛在一轉職業上顯示的共用武器技能）在真的轉職成二轉職業之前完全不顯示
            if (maple_skill_is_tier2(*sd) && !is_tier2) continue;
            int lvl = maple_skill_level(c, sd->key);
            bool maxed = lvl >= sd->max_level;
            bool unlockable = maple_skill_unlockable(c, *sd);
            std::string text = "**" + sd->name + "** Lv." + std::to_string(lvl) + "/" + std::to_string(sd->max_level) + "\n";
            text += sd->desc + "\n";
            text += "目前：" + maple_skill_value_text(*sd, lvl);
            if (!maxed) text += "\n下一級：" + maple_skill_value_text(*sd, lvl + 1);
            if (!unlockable) text += "\n🔒 尚未解鎖";
            container.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label("+1").set_id("maple_skadd_" + uid_s + "_" + sd->key)
                    .set_style(dpp::cos_success)
                    .set_disabled(maxed || maple_sp_unspent(c) <= 0 || !unlockable)));
        }
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 重製技能點數").set_id("maple_skreset_" + uid_s).set_style(dpp::cos_danger)
        .set_disabled(c.skill_reset_used));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_skill_reset_confirm_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚠️ 重製技能點數確認\n所有已學會的技能會全部歸零、點數全部退還重新分配，**這是你唯一一次免費重製的機會**，確定要繼續嗎？"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確定重製").set_id("maple_skresetok_" + uid_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("maple_skresetno_" + uid_s).set_style(dpp::cos_secondary));
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

    // 裝備欄要分開列「裸裝備」跟「每一個強化實例」——同一種類可能同時擁有好幾件不同強化結果的，
    // 不能只用「這個種類是不是已裝備」概括，不然除了裸的那份庫存被卡住選不到之外，
    // 好幾把強化程度不同的同款裝備也會只看得到其中一把，其他的完全選不到。
    auto item_common_text = [](const MapleItemDef& item) {
        std::string text;
        if (item.atk_bonus > 0) text += "　⚔️+" + std::to_string(item.atk_bonus);
        if (item.slot == "weapon")
            text += "　⚡" + maple_atk_speed_name(item.atk_speed_sec) + "(" + std::to_string(item.atk_speed_sec) + "秒)";
        text += "\n限制等級 " + std::to_string(item.level_req)
              + "　限制副屬性 " + std::to_string(item.secondary_req);
        const MapleJobDef* jr = item.job_req.empty() ? nullptr : maple_find_job(item.job_req);
        if (jr) text += "　限制職業 " + jr->name;
        if (item.str_bonus) text += "　力量+" + std::to_string(item.str_bonus);
        if (item.dex_bonus) text += "　敏捷+" + std::to_string(item.dex_bonus);
        if (item.int_bonus) text += "　智力+" + std::to_string(item.int_bonus);
        if (item.luk_bonus) text += "　幸運+" + std::to_string(item.luk_bonus);
        if (item.primary_generic)   text += "　主屬性+" + std::to_string(item.primary_generic);
        if (item.secondary_generic) text += "　副屬性+" + std::to_string(item.secondary_generic);
        if (!item.sellable) text += "　🚫無法售出";
        return text;
    };
    bool any = false;
    std::string cur_raw = maple_equipped_raw(c, slot);
    for (auto& item : MAPLE_ITEMS) {
        if (item.slot != slot) continue;
        int raw_qty = c.equipment.count(item.key) ? c.equipment.at(item.key) : 0;
        bool worn_is_raw = (cur_raw == item.key);
        std::vector<const MapleEnhItem*> enh_list;
        for (auto& e : c.enh_items) if (e.base_key == item.key) enh_list.push_back(&e);
        if (raw_qty <= 0 && !worn_is_raw && enh_list.empty()) continue; // 完全沒有這種裝備就跳過
        bool eligible = maple_meets_requirement(c, item);

        // 裸裝備那一行：有庫存、或正穿著裸的（庫存=0但穿著）才顯示
        if (raw_qty > 0 || worn_is_raw) {
            any = true;
            std::string text = "**" + item.name + "**" + (raw_qty > 0 ? "　×" + std::to_string(raw_qty) : "")
                              + item_common_text(item);
            container.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label(worn_is_raw ? "已裝備" : "裝備")
                    .set_id("maple_eqpickraw_" + uid_s + "_" + slot + "_" + item.key)
                    .set_style(worn_is_raw ? dpp::cos_secondary : dpp::cos_success)
                    .set_disabled(worn_is_raw || !eligible)));
        }
        // 每一個強化實例各自一行（不管有沒有穿著）
        for (auto* e : enh_list) {
            any = true;
            bool worn = maple_enh_is_equipped(c, e->id);
            std::string text = "**" + item.name + "** ✨+" + std::to_string(e->enh_count) + item_common_text(item);
            if (e->add_primary || e->add_secondary || e->add_atk) {
                text += "\n強化：";
                if (e->add_atk)       text += "攻擊力+" + std::to_string(e->add_atk) + " ";
                if (e->add_primary)   text += "主屬性+" + std::to_string(e->add_primary) + " ";
                if (e->add_secondary) text += "副屬性+" + std::to_string(e->add_secondary);
            }
            container.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label(worn ? "已裝備" : "裝備")
                    .set_id("maple_eqpickenh_" + uid_s + "_" + slot + "_" + std::to_string(e->id))
                    .set_style(worn ? dpp::cos_secondary : dpp::cos_success)
                    .set_disabled(worn || !eligible)));
        }
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

// 冒險區域用等級區間分頁：1~30 / 31~60 / 61~90 / 91~120 / 121~150
struct MapleAdvBracketDef { int lo, hi; std::string label; };
static const std::vector<MapleAdvBracketDef> MAPLE_ADV_BRACKETS = {
    {1,   30,  "Lv 1~30"},
    {31,  60,  "Lv 31~60"},
    {61,  90,  "Lv 61~90"},
    {91,  120, "Lv 91~120"},
    {121, 150, "Lv 121~150"},
};
static int maple_adv_bracket_of_level(int lvl) {
    for (size_t i = 0; i < MAPLE_ADV_BRACKETS.size(); i++)
        if (lvl >= MAPLE_ADV_BRACKETS[i].lo && lvl <= MAPLE_ADV_BRACKETS[i].hi) return (int)i;
    return (int)MAPLE_ADV_BRACKETS.size() - 1;
}
// 預設要顯示哪個區間：玩家正在冒險就用那個區域所在的區間，否則用上次瀏覽的區間，都沒有才用第一個
static int maple_adv_default_bracket(const MapleCharacter& c) {
    if (maple_is_adventuring(c)) {
        const MapleAdvRegionDef* r = maple_find_adv_region(c.adv_region);
        if (r) return maple_adv_bracket_of_level(r->suggested_level);
    }
    if (c.adv_last_bracket >= 0 && c.adv_last_bracket < (int)MAPLE_ADV_BRACKETS.size())
        return c.adv_last_bracket;
    return 0;
}

static dpp::message make_maple_adv_region_list_msg(dpp::snowflake uid, int bracket = -1) {
    MapleCharacter c = maple_get_or_create(uid);
    if (bracket < 0 || bracket >= (int)MAPLE_ADV_BRACKETS.size())
        bracket = maple_adv_default_bracket(c);
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🗺️ 冒險\n攻擊間隔取決於武器攻速，建議等級僅供參考、未達也能進入。"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));
    msg.add_component_v2(container);

    // 下拉選單：等級區間
    {
        dpp::component sel_row; sel_row.set_type(dpp::cot_action_row);
        dpp::component sel;
        sel.set_type(dpp::cot_selectmenu).set_id("maple_advbracket_" + uid_s)
            .set_placeholder("選擇等級區間");
        for (size_t i = 0; i < MAPLE_ADV_BRACKETS.size(); i++)
            sel.add_select_option(dpp::select_option(MAPLE_ADV_BRACKETS[i].label, std::to_string(i))
                .set_default((int)i == bracket));
        sel_row.add_component(sel);
        msg.add_component_v2(sel_row);
    }

    dpp::component list;
    list.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    bool any = false;
    for (auto& r : MAPLE_ADV_REGIONS) {
        if (maple_adv_bracket_of_level(r.suggested_level) != bracket) continue;
        any = true;
        std::string text = "**" + r.name + "**　建議 Lv. " + std::to_string(r.suggested_level) + "~";
        if (!r.open) text += "　🚧尚未開放";
        else {
            text += "\n" + r.monster.name + "：" + std::to_string(r.monster.hp) + " HP　"
                  + std::to_string(r.monster.exp) + " EXP　"
                  + std::to_string(r.monster.coin_min) + "~" + std::to_string(r.monster.coin_max) + " 幣";
            if (!r.bonus_job.empty()) {
                const MapleJobDef* bj = maple_find_job(r.bonus_job);
                text += "　✨" + (bj ? bj->name : r.bonus_job) + " 攻擊力×" + std::to_string(r.bonus_mult).substr(0, 3);
            }
        }
        list.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label(r.open ? "進入" : "尚未開放")
                .set_id("maple_advopen_" + uid_s + "_" + r.key)
                .set_style(r.open ? dpp::cos_success : dpp::cos_secondary)
                .set_disabled(!r.open)));
    }
    if (!any) {
        list.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("這個等級區間還沒有開放的區域。"));
    }
    msg.add_component_v2(list);

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
                 + std::to_string(region->monster.coin_min) + "~" + std::to_string(region->monster.coin_max) + " 幣\n";
        if (!region->bonus_job.empty()) {
            const MapleJobDef* bj = maple_find_job(region->bonus_job);
            content += "✨ " + (bj ? bj->name : region->bonus_job) + " 在這張圖攻擊力 ×"
                     + std::to_string(region->bonus_mult).substr(0, 3) + "\n";
        }
        {
            double ul = maple_adv_underlevel_mult(c, *region);
            if (ul < 1.0)
                content += "⚠️ 等級落差過大（低於建議等級 5 級以上），傷害 ×"
                         + std::to_string((int)llround(ul * 100)) + "%\n";
        }
        content += "\n";
        content += "**預估收益（每小時）**\n";
        content += "✨ 經驗值：約 **" + std::to_string((int64_t)llround(eph)) + "**";
        if (maple_exp_mult(c) > 1.0)
            content += "　🔰（未滿 " + std::to_string(MAPLE_ROOKIE_EXP_LEVEL) + " 級升級只要 1/"
                     + std::to_string((int)MAPLE_ROOKIE_EXP_MULT) + " 經驗）";
        content += "\n";
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
        .set_label("🍁 養成").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
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

// ─── 野外首領：畫面 ───────────────────────────────────────────────────────────

static dpp::message make_maple_wb_region_list_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    time_t now = time(nullptr);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🐉 野外首領\n全服共用一隻，先搶先贏！攻擊力越高打越快，但過程中被別人搶先擊殺就會無功而返。"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    for (auto& r : MAPLE_WB_REGIONS) {
        std::string respawn_text = (r.respawn_min_lo == r.respawn_min_hi)
            ? std::to_string(r.respawn_min_lo)
            : std::to_string(r.respawn_min_lo) + "~" + std::to_string(r.respawn_min_hi);
        std::string text = "**" + r.name + "**　建議 Lv. " + std::to_string(r.suggested_level)
                          + "~　（重生間隔 " + respawn_text + " 分鐘）\n";
        int round_kills;
        { std::lock_guard<std::mutex> lk(data_mutex); round_kills = maple_wb_state[r.key].round_kills; }
        text += "💀 這一輪已討伐 **" + std::to_string(round_kills) + "/" + std::to_string(MAPLE_WB_MAX_WINNERS) + "** 隻\n";
        std::string btn_label;
        bool enterable = false;
        if (!r.open) {
            text += "🚧 尚未開放";
            btn_label = "尚未開放";
        } else if (maple_wb_is_up(r.key, now)) {
            text += "😈 " + r.name + "似乎發生了奇怪的動靜...";
            btn_label = "進入";
            enterable = true;
        } else {
            text += "⏳ 尚未重生";
            btn_label = "尚未重生";
        }
        int hunters = maple_wb_hunters_count(r.key);
        if (hunters > 0) text += "　🔍 目前 " + std::to_string(hunters) + " 人正在尋找首領";
        container.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label(btn_label).set_id("maple_wbopen_" + uid_s + "_" + r.key)
                .set_style(enterable ? dpp::cos_danger : dpp::cos_secondary)
                .set_disabled(!enterable)));
    }
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_wb_status_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleWbRegionDef* region = maple_find_wb_region(c.wb_region);

    std::string content = "## ⚔️ 挑戰中 — " + (region ? region->name : c.wb_region) + "\n";
    if (region) content += "對手：**" + region->boss.name + "**\n";
    content += "\n你已經纏鬥了 **" + maple_fmt_duration(std::max((time_t)0, time(nullptr) - c.wb_started_at)) + "**...\n";
    content += "-# 勝負未定，用「查看戰況」確認結果。若中途被別人搶先討伐，這趟就會無功而返。";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔍 查看戰況").set_id("maple_wbcheck_" + uid_s).set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 中斷挑戰").set_id("maple_wbcancel_" + uid_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_wb_result_msg(dpp::snowflake uid, bool win, const std::string& boss_name,
                                             int64_t exp_gain, int64_t coin_gain, int level_ups,
                                             const std::vector<std::string>& drops = {}, int place = 0) {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string content;
    if (win) {
        content = "## 🎉 討伐成功！\n擊敗了野外首領 **" + boss_name + "**！";
        if (place > 0) content += "（本輪第 **" + std::to_string(place) + "/" + std::to_string(MAPLE_WB_MAX_WINNERS) + "** 位擊殺者）";
        content += "\n\n";
        content += "✨ 獲得經驗值 +**" + std::to_string(exp_gain) + "**\n";
        content += "🪙 獲得瘋幣 +**" + std::to_string(coin_gain) + "**\n";
        for (auto& d : drops) content += "🎁 " + d + "\n";
        if (level_ups > 0) content += "\n🆙 **升級！** 連升 **" + std::to_string(level_ups) + "** 級！";
    } else {
        content = "## 💨 無功而返\n晚了一步，**" + boss_name + "** 這一輪的名額已經被別人搶完了...下次動作要快！";
    }

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(win ? 0x2E : 0x95, win ? 0xCC : 0x95, win ? 0x71 : 0x95));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐉 野外首領").set_id("maple_wb_" + uid_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_wb_msg(dpp::snowflake uid) {
    MapleCharacter c = maple_get_or_create(uid);
    if (maple_is_wb_fighting(c)) return make_maple_wb_status_msg(uid);
    return make_maple_wb_region_list_msg(uid);
}

static dpp::message make_maple_ambush_boss_soon_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x95, 0x95, 0x95));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🚧 突襲首領\n尚未開放，敬請期待。"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

static dpp::message make_maple_guild_soon_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x95, 0x95, 0x95));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🚧 公會\n尚未開放，敬請期待。"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}

// ─── 商店 ───────────────────────────────────────────────────────────────────

static std::vector<std::string> maple_eqshop_categories(const MapleCharacter& c, const std::string& mode); // 定義在下方裝備商店區

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

    auto wcats = maple_eqshop_categories(c, "weapon");
    auto acats = maple_eqshop_categories(c, "armor");
    std::string wcat0 = wcats.empty() ? "gsword" : wcats.front();
    std::string acat0 = acats.empty() ? "earring" : acats.front();

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📜 卷軸商店").set_id("maple_scshop_" + uid_s + "_0").set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎁 特殊商店").set_id("maple_tokenshop_" + uid_s).set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗡️ 武器商店").set_id("maple_eqshop_" + uid_s + "_weapon_" + wcat0 + "_0").set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🛡️ 防具商店").set_id("maple_eqshop_" + uid_s + "_armor_" + acat0 + "_0").set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);

    return msg;
}

// ─── 特殊商店（籌碼 → 瘋幣兌換 ＋ 付費能力值重製）─────────────────────────────
static const int64_t MAPLE_TOKEN_WEEKLY_CAP = 20000; // 每週最多可用多少籌碼兌換
static const int     MAPLE_TOKEN_RATE_NUM   = 1;     // 瘋幣 = 籌碼 × num / den（之後可調比值）
static const int     MAPLE_TOKEN_RATE_DEN   = 1;
static const int64_t MAPLE_AP_BUYRESET_COST = 10000; // 付費能力值重製：每次 10000 籌碼，可重複購買
static const int64_t MAPLE_SP_BUYRESET_COST = 10000; // 付費技能點數重製：每次 10000 籌碼，可重複購買

// 每週二早上08:00(UTC+8) = 週二00:00 UTC 重置；1970-01-01(週四)往後推5天剛好是週二00:00 UTC
static int64_t maple_token_week_now()             { return ((int64_t)time(nullptr) - 5 * 86400) / 604800; }
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
    std::string body = "## 🎁 特殊商店\n"
        "💰 你的籌碼：**" + std::to_string(chips) + "**　🪙 你的瘋幣：**" + std::to_string(c.coins) + "**\n\n"
        "**🪙 籌碼兌換瘋幣**\n"
        "比值 " + std::to_string(MAPLE_TOKEN_RATE_NUM) + " : " + std::to_string(MAPLE_TOKEN_RATE_DEN) + "（籌碼 : 瘋幣）\n"
        "📅 本週已兌換：**" + std::to_string(spent) + " / " + std::to_string(MAPLE_TOKEN_WEEKLY_CAP) + "** 籌碼（剩 **" + std::to_string(remain) + "**）";
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(body));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));
    container.add_component_v2(dpp::component()
        .set_type(dpp::cot_section)
        .add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("**🔄 能力值重製**\n把已分配的力量／敏捷／智力／幸運全部歸零重新分配（可重複購買，不佔用免費的一次）\n💰 "
                         + std::to_string(MAPLE_AP_BUYRESET_COST) + " 籌碼"))
        .set_accessory(dpp::component().set_type(dpp::cot_button)
            .set_label("重製").set_id("maple_apbuyreset_" + uid_s)
            .set_style(dpp::cos_danger)
            .set_disabled(chips < MAPLE_AP_BUYRESET_COST || maple_is_adventuring(c))));
    container.add_component_v2(dpp::component()
        .set_type(dpp::cot_section)
        .add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("**🔄 技能點數重製**\n把已學會的技能全部歸零、點數全部退還重新分配（可重複購買，不佔用免費的一次）\n💰 "
                         + std::to_string(MAPLE_SP_BUYRESET_COST) + " 籌碼"))
        .set_accessory(dpp::component().set_type(dpp::cot_button)
            .set_label("重製").set_id("maple_spbuyreset_" + uid_s)
            .set_style(dpp::cos_danger)
            .set_disabled(chips < MAPLE_SP_BUYRESET_COST)));
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
        // 用獨立的 id（金額由伺服器端算），避免 mx 剛好等於上面某個預設值時 custom_id 撞號被 Discord 拒收
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(mx > 0 ? "兌換上限 " + std::to_string(mx) : "額度已用完")
            .set_id("maple_tokenexmax_" + uid_s)
            .set_style(dpp::cos_primary).set_disabled(mx <= 0));
    }
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回商店").set_id("maple_shop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);

    return msg;
}

static dpp::message make_maple_apbuyreset_confirm_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚠️ 付費能力值重製\n花費 **" + std::to_string(MAPLE_AP_BUYRESET_COST)
                     + "** 籌碼，把力量／敏捷／智力／幸運全部歸零重新分配。確定嗎？"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確定（-" + std::to_string(MAPLE_AP_BUYRESET_COST) + " 籌碼）")
        .set_id("maple_apbuyresetok_" + uid_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("maple_tokenshop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

static dpp::message make_maple_spbuyreset_confirm_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚠️ 付費技能點數重製\n花費 **" + std::to_string(MAPLE_SP_BUYRESET_COST)
                     + "** 籌碼，把已學會的技能全部歸零、點數全部退還重新分配。確定嗎？"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確定（-" + std::to_string(MAPLE_SP_BUYRESET_COST) + " 籌碼）")
        .set_id("maple_spbuyresetok_" + uid_s).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("maple_tokenshop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

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

// 卷軸商店實際上架的清單（排除 shop=false 的卷軸，那些要用其他方式取得）
static std::vector<const MapleScrollDef*> maple_scshop_list() {
    std::vector<const MapleScrollDef*> out;
    for (auto& s : MAPLE_SCROLLS) if (s.shop) out.push_back(&s);
    return out;
}

static dpp::message make_maple_scshop_msg(dpp::snowflake uid, int page) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    auto list = maple_scshop_list();
    int total = (int)list.size();
    int pages = (total + MAPLE_SCSHOP_PAGE_SIZE - 1) / MAPLE_SCSHOP_PAGE_SIZE;
    if (page < 0) page = 0;
    if (page >= pages) page = pages - 1;
    int start = page * MAPLE_SCSHOP_PAGE_SIZE;
    int end   = std::min(start + MAPLE_SCSHOP_PAGE_SIZE, total);

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 📜 卷軸商店（" + std::to_string(page + 1) + "/" + std::to_string(pages) + "）\n"
                     "🪙 瘋幣：**" + std::to_string(c.coins) + "**"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    for (int i = start; i < end; i++) {
        const MapleScrollDef& s = *list[i];
        int owned = c.scrolls.count(s.key) ? c.scrolls.at(s.key) : 0;
        std::string text = "**" + s.name + "**\n"
                         + "適用：" + s.applies_to + "　成功率 " + std::to_string(s.rate) + "%\n"
                         + "效果：" + maple_scroll_effect_text(s) + "\n"
                         + "💰 " + std::to_string(s.price) + " 瘋幣" + (owned > 0 ? "　（持有 " + std::to_string(owned) + "）" : "");
        container.add_component_v2(dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label("購買").set_id("maple_scbuy_" + uid_s + "_" + s.key)
                .set_style(dpp::cos_success).set_disabled(c.coins < s.price)));
    }
    msg.add_component_v2(container);

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
    MapleCharacter c = maple_get_or_create(uid);
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF1, 0xC4, 0x0F));
    int64_t max_afford = 0;
    if (s) {
        max_afford = s->price > 0 ? std::min((int64_t)999, c.coins / s->price) : 0;
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🛒 確認購買\n**" + s->name + "**\n效果：" + maple_scroll_effect_text(*s)
                        + "\n單價 **" + std::to_string(s->price) + "** 瘋幣　持有瘋幣：**" + std::to_string(c.coins) + "**\n"
                        + "選擇購買數量："));
    } else {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content("## ❌ 找不到這個卷軸"));
    }
    msg.add_component_v2(container);

    if (s) {
        static const int64_t QOPTS[] = {1, 5, 10, 50};
        dpp::component row; row.set_type(dpp::cot_action_row);
        for (int64_t q : QOPTS) {
            row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("×" + std::to_string(q) + "（" + std::to_string(q * s->price) + "）")
                .set_id("maple_scbuyok_" + uid_s + "_" + scroll_key + "_" + std::to_string(q))
                .set_style(dpp::cos_success)
                .set_disabled(q > max_afford));
        }
        msg.add_component_v2(row);

        dpp::component row2; row2.set_type(dpp::cot_action_row);
        // 「買到上限」用獨立 id、數量伺服器端算，避免剛好等於上面某個固定量時 custom_id 撞號
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(max_afford > 0 ? "買到上限 ×" + std::to_string(max_afford) : "瘋幣不足")
            .set_id("maple_scbuymax_" + uid_s + "_" + scroll_key)
            .set_style(dpp::cos_primary).set_disabled(max_afford <= 0));
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("❌ 取消").set_id("maple_scshop_" + uid_s + "_0").set_style(dpp::cos_secondary));
        msg.add_component_v2(row2);
    } else {
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回").set_id("maple_scshop_" + uid_s + "_0").set_style(dpp::cos_secondary));
        msg.add_component_v2(row);
    }

    return msg;
}

// ─── 裝備商店 ───────────────────────────────────────────────────────────────
// 分類切換：本職業的各武器類型 + 耳環（所有人）。之後防具再擴充。

// 裝備商店分兩館：mode="weapon" 顯示本職業武器類型；mode="armor" 顯示 耳環＋頭盔/套服/手套/鞋子
static std::vector<std::string> maple_eqshop_categories(const MapleCharacter& c, const std::string& mode) {
    (void)c;
    std::vector<std::string> cats;
    if (mode == "weapon") {
        // 不分職業，全部武器類型都看得到（能不能裝備由裝備當下的限制判斷）
        for (auto& wt : MAPLE_WEAPON_TYPES) cats.push_back(wt.type_key);
    } else {
        cats.push_back("earring");
        for (auto& ar : MAPLE_ARMORS) cats.push_back(ar.slot);
    }
    return cats;
}
// 某件裝備屬於哪一館
static std::string maple_eqshop_mode_of_item(const MapleItemDef& it) {
    return it.slot == "weapon" ? "weapon" : "armor";
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

static dpp::message make_maple_eqshop_msg(dpp::snowflake uid, const std::string& mode_in,
                                          const std::string& cat_in, int page) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);

    std::string mode = (mode_in == "weapon") ? "weapon" : "armor";
    auto cats = maple_eqshop_categories(c, mode);
    std::string cat = cat_in;
    if (std::find(cats.begin(), cats.end(), cat) == cats.end())
        cat = cats.empty() ? (mode == "weapon" ? "gsword" : "earring") : cats.front();

    auto list = maple_eqshop_list(cat);
    int total = (int)list.size();
    int pages = total > 0 ? (total + MAPLE_EQSHOP_PAGE_SIZE - 1) / MAPLE_EQSHOP_PAGE_SIZE : 1;
    if (page < 0) page = 0;
    if (page >= pages) page = pages - 1;

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    std::string shop_name = mode == "weapon" ? "🗡️ 武器商店" : "🛡️ 防具商店";
    dpp::component header;
    header.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    header.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## " + shop_name + " — " + maple_eqshop_cat_label(cat)
                     + "（" + std::to_string(page + 1) + "/" + std::to_string(pages) + "）\n"
                     "🪙 瘋幣：**" + std::to_string(c.coins) + "**"));
    msg.add_component_v2(header);

    // 下拉選單：選擇武器種類 / 裝備部位
    if (!cats.empty()) {
        dpp::component sel_row; sel_row.set_type(dpp::cot_action_row);
        dpp::component sel;
        sel.set_type(dpp::cot_selectmenu).set_id("maple_eqshopsel_" + uid_s + "_" + mode)
            .set_placeholder(mode == "weapon" ? "選擇武器種類" : "選擇裝備部位");
        for (auto& k : cats)
            sel.add_select_option(dpp::select_option(maple_eqshop_cat_label(k), k).set_default(k == cat));
        sel_row.add_component(sel);
        msg.add_component_v2(sel_row);
    }

    if (total == 0) {
        dpp::component box;
        box.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x95, 0x95, 0x95));
        box.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("這個分類目前沒有可購買的裝備。"));
        msg.add_component_v2(box);
    } else {
        dpp::component itembox;
        itembox.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
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
            itembox.add_component_v2(dpp::component()
                .set_type(dpp::cot_section)
                .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
                .set_accessory(dpp::component().set_type(dpp::cot_button)
                    .set_label("購買").set_id("maple_eqbuy_" + uid_s + "_" + it.key)
                    .set_style(dpp::cos_success).set_disabled(c.coins < it.price)));
        }
        msg.add_component_v2(itembox);
    }

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("◀ 上一頁").set_id("maple_eqshop_" + uid_s + "_" + mode + "_" + cat + "_" + std::to_string(page - 1))
        .set_style(dpp::cos_secondary).set_disabled(page <= 0));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶ 下一頁").set_id("maple_eqshop_" + uid_s + "_" + mode + "_" + cat + "_" + std::to_string(page + 1))
        .set_style(dpp::cos_secondary).set_disabled(page >= pages - 1));
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回商店").set_id("maple_shop_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);

    return msg;
}

static dpp::message make_maple_eqbuy_confirm_msg(dpp::snowflake uid, const std::string& item_key) {
    std::string uid_s = std::to_string((uint64_t)uid);
    const MapleItemDef* it = maple_find_item(item_key);
    std::string cat  = it ? maple_cat_of_item(*it) : "earring";
    std::string mode = it ? maple_eqshop_mode_of_item(*it) : "armor";
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
        .set_label("❌ 取消").set_id("maple_eqshop_" + uid_s + "_" + mode + "_" + cat + "_0").set_style(dpp::cos_secondary));
    msg.add_component_v2(row);

    return msg;
}

// ─── 背包 ───────────────────────────────────────────────────────────────────

// 卷軸分類：special＝純白系列（restore_slot）、wpn＝武器卷軸（key 開頭 sc_wpn_）、armor＝其餘（防具/耳環/戒指/項鍊）
static std::string maple_scroll_cat(const MapleScrollDef& s) {
    if (s.restore_slot) return "special";
    if (s.key.rfind("sc_wpn_", 0) == 0) return "wpn";
    return "armor";
}
// 裝備分類：wpn＝武器、acc＝飾品（耳環/戒指/項鍊）、armor＝其餘（頭盔/手套/套服/鞋子）
static std::string maple_item_cat(const std::string& slot) {
    if (slot == "weapon") return "wpn";
    if (slot == "earring" || slot == "ring" || slot == "necklace") return "acc";
    return "armor";
}

static dpp::message make_maple_bag_msg(dpp::snowflake uid, const std::string& tab, int page = 0, std::string subcat = "") {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    bool scroll_tab = (tab != "other");

    std::string head = "## 🎒 背包\n🪙 瘋幣：**" + std::to_string(c.coins) + "**";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE8, 0x7A, 0x41));

    if (scroll_tab) {
        if (subcat != "wpn" && subcat != "armor" && subcat != "special") subcat = "wpn";
        std::string cat_label = subcat == "wpn" ? "武器卷軸" : subcat == "armor" ? "裝備卷軸" : "特殊卷軸";
        std::string content = head + "\n\n**📜 " + cat_label + "**\n";
        bool any = false;
        for (auto& s : MAPLE_SCROLLS) {
            if (maple_scroll_cat(s) != subcat) continue;
            int n = c.scrolls.count(s.key) ? c.scrolls.at(s.key) : 0;
            if (n <= 0) continue;
            any = true;
            content += "• **" + s.name + "**　ID:`" + std::to_string(s.item_id) + "`　×" + std::to_string(n) + "\n"
                     + "　適用：" + s.applies_to + "　成功率 " + std::to_string(s.rate) + "%"
                     + (s.explode_pct > 0 ? "（失敗中 " + std::to_string(s.explode_pct) + "% 機率裝備直接爆炸消失）" : "") + "\n"
                     + "　效果：" + maple_scroll_effect_text(s) + "\n";
        }
        if (!any) content += "（沒有這個分類的卷軸）\n";
        content += "\n-# 可用 `!交易` 指令交換（帶上 ID）";
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));

        dpp::message msg;
        msg.set_flags(dpp::m_using_components_v2);
        msg.add_component_v2(container);

        dpp::component tabs; tabs.set_type(dpp::cot_action_row);
        tabs.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📜 卷軸").set_id("maple_bag_" + uid_s + "_scroll").set_style(dpp::cos_primary).set_disabled(true));
        tabs.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📦 其他").set_id("maple_bag_" + uid_s + "_other").set_style(dpp::cos_secondary));
        msg.add_component_v2(tabs);

        dpp::component cats; cats.set_type(dpp::cot_action_row);
        cats.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("武器卷軸").set_id("maple_bagcat_" + uid_s + "_scroll_wpn")
            .set_style(subcat == "wpn" ? dpp::cos_primary : dpp::cos_secondary).set_disabled(subcat == "wpn"));
        cats.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("裝備卷軸").set_id("maple_bagcat_" + uid_s + "_scroll_armor")
            .set_style(subcat == "armor" ? dpp::cos_primary : dpp::cos_secondary).set_disabled(subcat == "armor"));
        cats.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("特殊卷軸").set_id("maple_bagcat_" + uid_s + "_scroll_special")
            .set_style(subcat == "special" ? dpp::cos_primary : dpp::cos_secondary).set_disabled(subcat == "special"));
        msg.add_component_v2(cats);

        dpp::component nav; nav.set_type(dpp::cot_action_row);
        nav.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(nav);
        return msg;
    } else {
        if (subcat != "wpn" && subcat != "armor" && subcat != "acc") subcat = "wpn";
        std::string cat_label = subcat == "wpn" ? "武器" : subcat == "armor" ? "裝備" : "飾品";
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content(head + "\n\n**📦 " + cat_label + "（未穿在身上）**"));

        // 先收集全部項目再分頁顯示，避免裝備/強化實例太多時一次塞爆 Discord 訊息的元件上限（背包打不開）
        struct BagEntry { std::string text, sell_id; };
        std::vector<BagEntry> entries;
        for (auto& [k, n] : c.equipment) {
            if (n <= 0) continue;
            const MapleItemDef* it = maple_find_item(k);
            if (!it || maple_item_cat(it->slot) != subcat) continue;
            int64_t sell = maple_item_sell_price(*it);
            std::string text = "**" + it->name + "**　ID:`" + std::to_string(it->item_id) + "`　×" + std::to_string(n)
                              + "\n　售出單價：🪙" + std::to_string(sell);
            entries.push_back({text, "maple_sellconfirm_" + uid_s + "_item_" + k});
        }
        for (auto& e : c.enh_items) {
            if (maple_enh_is_equipped(c, e.id)) continue;
            const MapleItemDef* it = maple_find_item(e.base_key);
            if (!it || maple_item_cat(it->slot) != subcat) continue;
            int64_t sell = maple_item_sell_price(*it) + maple_enh_extra_sell_value(e);
            std::string text = "**" + it->name + "** ✨+" + std::to_string(e.enh_count) + "　🚫不可交易";
            if (e.add_atk || e.add_primary || e.add_secondary) {
                text += "（";
                if (e.add_atk)       text += "攻+" + std::to_string(e.add_atk) + " ";
                if (e.add_primary)   text += "主+" + std::to_string(e.add_primary) + " ";
                if (e.add_secondary) text += "副+" + std::to_string(e.add_secondary);
                text += "）";
            }
            text += "\n　售出單價：🪙" + std::to_string(sell);
            entries.push_back({text, "maple_sellconfirm_" + uid_s + "_enh_" + std::to_string(e.id)});
        }

        const int PAGE_SIZE = 8;
        int total_pages = std::max(1, (int)((entries.size() + PAGE_SIZE - 1) / PAGE_SIZE));
        page = std::max(0, std::min(page, total_pages - 1));
        int start = page * PAGE_SIZE, end = std::min((int)entries.size(), start + PAGE_SIZE);

        if (entries.empty()) {
            container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content("（沒有這個分類的備用裝備）"));
        } else {
            for (int i = start; i < end; i++) {
                container.add_component_v2(dpp::component()
                    .set_type(dpp::cot_section)
                    .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(entries[i].text))
                    .set_accessory(dpp::component().set_type(dpp::cot_button)
                        .set_label("賣出").set_id(entries[i].sell_id).set_style(dpp::cos_danger)));
            }
            if (total_pages > 1)
                container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
                    .set_content("-# 第 " + std::to_string(page + 1) + "/" + std::to_string(total_pages) + " 頁"));
        }
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("-# 純裝備可用 `!交易` 交換（帶上 ID）；**點過卷軸的裝備不可交易，只能賣出換瘋幣**"));

        dpp::message msg;
        msg.set_flags(dpp::m_using_components_v2);
        msg.add_component_v2(container);

        dpp::component tabs; tabs.set_type(dpp::cot_action_row);
        tabs.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📜 卷軸").set_id("maple_bag_" + uid_s + "_scroll")
            .set_style(dpp::cos_secondary));
        tabs.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("📦 其他").set_id("maple_bag_" + uid_s + "_other")
            .set_style(dpp::cos_primary).set_disabled(true));
        msg.add_component_v2(tabs);

        dpp::component cats; cats.set_type(dpp::cot_action_row);
        cats.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("武器").set_id("maple_bagcat_" + uid_s + "_other_wpn")
            .set_style(subcat == "wpn" ? dpp::cos_primary : dpp::cos_secondary).set_disabled(subcat == "wpn"));
        cats.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("裝備").set_id("maple_bagcat_" + uid_s + "_other_armor")
            .set_style(subcat == "armor" ? dpp::cos_primary : dpp::cos_secondary).set_disabled(subcat == "armor"));
        cats.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("飾品").set_id("maple_bagcat_" + uid_s + "_other_acc")
            .set_style(subcat == "acc" ? dpp::cos_primary : dpp::cos_secondary).set_disabled(subcat == "acc"));
        msg.add_component_v2(cats);

        if (total_pages > 1) {
            dpp::component pg; pg.set_type(dpp::cot_action_row);
            pg.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("◀ 上一頁").set_id("maple_bagpg_" + uid_s + "_" + std::to_string(page - 1) + "_" + subcat)
                .set_style(dpp::cos_secondary).set_disabled(page <= 0));
            pg.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("▶ 下一頁").set_id("maple_bagpg_" + uid_s + "_" + std::to_string(page + 1) + "_" + subcat)
                .set_style(dpp::cos_secondary).set_disabled(page >= total_pages - 1));
            msg.add_component_v2(pg);
        }

        dpp::component nav; nav.set_type(dpp::cot_action_row);
        nav.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(nav);
        return msg;
    }
}

// kind=="item"：ref 是道具 key（賣一個純裝備）；kind=="enh"：ref 是強化實例 id（賣掉那一件，整個消失）
static dpp::message make_maple_sell_confirm_msg(dpp::snowflake uid, const std::string& kind, const std::string& ref) {
    MapleCharacter c = maple_get_or_create(uid);
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string name; int64_t price = 0; bool found = false;

    if (kind == "enh") {
        int eid = 0; try { eid = std::stoi(ref); } catch (...) {}
        for (auto& e : c.enh_items) {
            if (e.id != eid || maple_enh_is_equipped(c, e.id)) continue;
            const MapleItemDef* it = maple_find_item(e.base_key);
            if (!it) break;
            name = it->name + " ✨+" + std::to_string(e.enh_count);
            price = maple_item_sell_price(*it) + maple_enh_extra_sell_value(e);
            found = true;
            break;
        }
    } else {
        int n = c.equipment.count(ref) ? c.equipment.at(ref) : 0;
        const MapleItemDef* it = maple_find_item(ref);
        if (it && n > 0) { name = it->name; price = maple_item_sell_price(*it); found = true; }
    }

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    if (!found) {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content("## ❌ 找不到這件裝備\n可能已經賣掉或穿上了。"));
        msg.add_component_v2(container);
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回背包").set_id("maple_bag_" + uid_s + "_other").set_style(dpp::cos_secondary));
        msg.add_component_v2(row);
        return msg;
    }
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚠️ 確認賣出\n**" + name + "**\n賣出後直接消失，換取 🪙**" + std::to_string(price) + "** 瘋幣。確定嗎？"));
    msg.add_component_v2(container);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確定賣出（+" + std::to_string(price) + " 瘋幣）")
        .set_id("maple_sellok_" + uid_s + "_" + kind + "_" + ref).set_style(dpp::cos_danger));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❌ 取消").set_id("maple_bag_" + uid_s + "_other").set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}
