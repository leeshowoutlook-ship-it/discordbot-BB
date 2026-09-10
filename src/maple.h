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

static int maple_stat_value(const MapleCharacter& c, const std::string& stat) {
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
};

static const std::vector<MapleSkillDef> MAPLE_SKILLS = {
    // 初心者
    {"baby_throw", "嫩寶投擲術", "將傷害固定為所示數值", "beginner", 3, "damage_fixed", {10,20,30}, ""},
    {"free_equip", "換裝自由",   "點滿三點後開放裝備系統", "beginner", 3, "unlock", {}, "equip"},
    {"free_ap",    "能力值自由", "點滿三點後開放能力值系統", "beginner", 3, "unlock", {}, "ap"},
    // 盜賊
    {"double_throw", "雙飛閃", "快速投擲兩枚飛鏢攻擊敵人，技能係數如上", "thief", 10, "damage_coef",
        {100,108,116,124,130,135,140,144,148,150}, ""},
    {"haste", "速度激發", "自己以及周圍的玩家激發潛能，提升效率", "thief", 10, "buff_pct",
        {1,2,3,4,5,6,7,8,9,10}, ""},
    // 法師
    {"magic_claw", "魔力爪", "運用自身魔力化成尖銳的爪擊攻擊敵人，技能係數如上", "mage", 10, "damage_coef",
        {120,140,160,175,180,190,200,210,220,230}, ""},
    {"teleport", "瞬間移動", "運用自身魔力進行短距離的區域移動，提升效率", "mage", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, ""},
    {"rage_slash", "魔天一擊", "粗暴地揮舞大劍攻擊敵人，技能係數如上", "warrior", 10, "damage_coef",
        {170,190,210,230,250,270,290,310,330,350}, ""},
    {"endurance", "自身強化", "強悍的體能使你能夠有更出彩的持久力，減少休息時間", "warrior", 10, "buff_pct",
        {2,4,6,8,10,12,14,16,18,20}, ""},
    // 海盜
    {"impact_punch", "衝擊拳", "粗暴地揮舞拳頭攻擊敵人，技能係數如上", "pirate", 10, "damage_coef",
        {162,174,186,198,210,222,234,246,258,270}, ""},
    {"charge", "衝鋒", "使用自身體能展現爆發力，提升效率", "pirate", 10, "buff_pct",
        {0.5,1.0,1.5,2.0,2.5,3.0,3.5,4.0,4.5,5.0}, ""},
    // 弓箭手
    {"double_shot", "二連箭", "發射特製的箭矢射擊敵人，技能係數如上", "archer", 10, "damage_coef",
        {196,202,208,214,220,226,234,240,250,260}, ""},
    {"eagle_eye", "霸王箭", "訓練使你更容易找到敵人的弱點，提升爆擊率", "archer", 10, "buff_pct",
        {4,8,12,16,20,24,28,32,36,40}, ""},
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
};

static const std::vector<MapleItemDef> MAPLE_ITEMS = {
    {"wooden_sword", "新手木劍", "weapon", 0, 0, 0, false, 10},
};

static const MapleItemDef* maple_find_item(const std::string& key) {
    if (key.empty()) return nullptr;
    for (auto& it : MAPLE_ITEMS) if (it.key == key) return &it;
    return nullptr;
}

static std::string maple_equipped_key(const MapleCharacter& c, const std::string& slot) {
    if (slot == "weapon")  return c.eq_weapon;
    if (slot == "earring") return c.eq_earring;
    if (slot == "helmet")  return c.eq_helmet;
    if (slot == "glove")   return c.eq_glove;
    if (slot == "clothes") return c.eq_clothes;
    if (slot == "shoes")   return c.eq_shoes;
    return "";
}

static void maple_set_equipped(MapleCharacter& c, const std::string& slot, const std::string& key) {
    if      (slot == "weapon")  c.eq_weapon  = key;
    else if (slot == "earring") c.eq_earring = key;
    else if (slot == "helmet")  c.eq_helmet  = key;
    else if (slot == "glove")   c.eq_glove   = key;
    else if (slot == "clothes") c.eq_clothes = key;
    else if (slot == "shoes")   c.eq_shoes   = key;
}

// 玩家目前是否符合裝備該道具的需求（等級／主屬性／副屬性，依目前職業判斷）
static bool maple_meets_requirement(const MapleCharacter& c, const MapleItemDef& item) {
    const MapleJobDef& j = maple_job_of(c);
    if (c.level < item.level_req) return false;
    if (maple_stat_value(c, j.primary_stat)   < item.primary_req)   return false;
    if (maple_stat_value(c, j.secondary_stat) < item.secondary_req) return false;
    return true;
}

// 武器等裝備提供的攻擊力加總（未裝備武器時為0）
static int maple_total_atk(const MapleCharacter& c) {
    int total = 0;
    for (auto& slot : MAPLE_SLOTS) {
        auto* it = maple_find_item(maple_equipped_key(c, slot.key));
        if (it) total += it->atk_bonus;
    }
    return total;
}

// 取得玩家目前選擇、且實際可用（已學會、轉職後仍看得到）的攻擊技能；回傳 nullptr＝普通攻擊
static const MapleSkillDef* maple_current_atk_skill(const MapleCharacter& c) {
    if (c.adv_atk_skill.empty()) return nullptr;
    const MapleSkillDef* sd = maple_find_skill(c.adv_atk_skill);
    if (!sd) return nullptr;
    if (sd->type != "damage_fixed" && sd->type != "damage_coef") return nullptr;
    if (!maple_skill_visible(c, sd->job)) return nullptr;
    int lvl = maple_skill_level(c, sd->key);
    if (lvl <= 0) return nullptr;
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
    if (sd->type == "damage_fixed") return (int64_t)sd->values[lvl-1];
    return (int64_t)llround(base * sd->values[lvl-1] / 100.0);
}
static int64_t maple_atk_power_min(const MapleCharacter& c) {
    const MapleJobDef& j = maple_job_of(c);
    double primary   = maple_stat_value(c, j.primary_stat);
    double secondary = maple_stat_value(c, j.secondary_stat);
    int64_t base = (int64_t)llround((j.primary_coef * 0.9 * c.weapon_mastery * primary + j.secondary_coef * secondary) * maple_total_atk(c) / 100.0);
    const MapleSkillDef* sd = maple_current_atk_skill(c);
    if (!sd) return base;
    int lvl = maple_skill_level(c, sd->key);
    if (sd->type == "damage_fixed") return (int64_t)sd->values[lvl-1];
    return (int64_t)llround(base * sd->values[lvl-1] / 100.0);
}
static double maple_atk_power_avg(const MapleCharacter& c) {
    return (maple_atk_power_min(c) + maple_atk_power_max(c)) / 2.0;
}

// ─── 冒險 ───────────────────────────────────────────────────────────────────
// 攻速固定：每 30 秒攻擊一次。瘋幣掉落＝經驗值 × 0.8~1.5 倍（估算用平均倍率 1.15）。

static const int MAPLE_ADV_ATTACK_INTERVAL_SEC = 30;
static const double MAPLE_ADV_COIN_AVG_MULT = 1.15;

struct MapleAdvMonsterDef { std::string name; int hp; int64_t exp; };
struct MapleAdvRegionDef {
    std::string key, name;
    int suggested_level;
    bool open; // 是否已經開放（目前只開放弓箭手訓練場）
    MapleAdvMonsterDef monster;
};

static const std::vector<MapleAdvRegionDef> MAPLE_ADV_REGIONS = {
    {"archer_range", "弓箭手訓練場", 1,  true,  {"訓練用稻草人", 40, 8}},
    {"trapdoor",     "小心掉落",     10, false, {"", 0, 0}},
    {"blue_mushroom_forest", "藍菇菇森林", 15, false, {"", 0, 0}},
};

static const MapleAdvRegionDef* maple_find_adv_region(const std::string& key) {
    for (auto& r : MAPLE_ADV_REGIONS) if (r.key == key) return &r;
    return nullptr;
}

// 用平均傷害估算每小時可擊殺數／經驗／瘋幣（region 必須是已開放且有怪物資料）
static void maple_adv_estimate(const MapleCharacter& c, const MapleAdvRegionDef& region,
                               double& kills_per_hour, double& exp_per_hour, double& coins_per_hour) {
    double avg_dmg = std::max(1.0, maple_atk_power_avg(c));
    double hits_to_kill = region.monster.hp / avg_dmg;
    double seconds_per_kill = hits_to_kill * MAPLE_ADV_ATTACK_INTERVAL_SEC;
    kills_per_hour = seconds_per_kill > 0 ? 3600.0 / seconds_per_kill : 0.0;
    exp_per_hour   = kills_per_hour * region.monster.exp;
    coins_per_hour = kills_per_hour * region.monster.exp * MAPLE_ADV_COIN_AVG_MULT;
}

static bool maple_is_adventuring(const MapleCharacter& c) { return !c.adv_region.empty(); }

// 目前這場冒險已累積多少經驗／瘋幣（用平均值×經過時間計算，不是逐隻模擬）
static void maple_adv_progress(const MapleCharacter& c, int64_t& exp_out, int64_t& coins_out, int64_t& seconds_out) {
    exp_out = 0; coins_out = 0; seconds_out = 0;
    if (!maple_is_adventuring(c)) return;
    const MapleAdvRegionDef* region = maple_find_adv_region(c.adv_region);
    if (!region) return;
    seconds_out = std::max((time_t)0, time(nullptr) - c.adv_started_at);
    double kph, eph, cph;
    maple_adv_estimate(c, *region, kph, eph, cph);
    double hours = seconds_out / 3600.0;
    exp_out   = (int64_t)llround(eph * hours);
    coins_out = (int64_t)llround(cph * hours);
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
                {"adv_atk_skill",     c.adv_atk_skill},
                {"adv_region",        c.adv_region},
                {"adv_started_at",    (int64_t)c.adv_started_at},
                {"monsters_defeated", c.monsters_defeated},
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
            c.adv_atk_skill     = v.value("adv_atk_skill",     std::string());
            c.adv_region        = v.value("adv_region",        std::string());
            c.adv_started_at    = (time_t)v.value("adv_started_at", (int64_t)0);
            c.monsters_defeated = v.value("monsters_defeated", (int64_t)0);
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
    content += "主屬性：" + maple_stat_name(job.primary_stat) + " **" + std::to_string(maple_stat_value(c, job.primary_stat)) + "**　"
             + "副屬性：" + maple_stat_name(job.secondary_stat) + " **" + std::to_string(maple_stat_value(c, job.secondary_stat)) + "**\n";
    content += "攻擊力 **" + std::to_string(maple_atk_power_min(c)) + " ~ " + std::to_string(maple_atk_power_max(c))
             + "**　防禦力 **" + std::to_string(c.def) + "**　生命值 **" + std::to_string(c.max_hp) + "**\n";
    {
        const MapleSkillDef* atk_sd = maple_current_atk_skill(c);
        content += "⚔️ 目前攻擊方式：**" + (atk_sd ? atk_sd->name : std::string("普通攻擊")) + "**\n";
    }
    content += "🗡️ 累計擊敗怪物：**" + std::to_string(c.monsters_defeated) + "** 隻\n";
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
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);

    return msg;
}

static std::string maple_skill_value_text(const MapleSkillDef& sd, int level) {
    if (level <= 0) return "尚未學習";
    if (sd.type == "damage_fixed") return "固定造成 " + std::to_string((int64_t)sd.values[level-1]) + " 傷害";
    if (sd.type == "damage_coef")  return "技能係數 " + std::to_string((int64_t)sd.values[level-1]) + "%";
    if (sd.type == "buff_pct") {
        double v = sd.values[level-1];
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f", v);
        std::string s(buf);
        if (s.size() > 2 && s.substr(s.size()-2) == ".0") s = s.substr(0, s.size()-2);
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

    auto row_for = [&](const std::string& name, const std::string& desc, bool current, const std::string& skill_key) {
        std::string text = "**" + name + "**\n" + desc;
        if (current) text += "\n✅ 目前使用中";
        return dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label(current ? "使用中" : "選擇")
                .set_id("maple_atkpick_" + uid_s + "_" + skill_key)
                .set_style(current ? dpp::cos_secondary : dpp::cos_success)
                .set_disabled(current));
    };

    container.add_component_v2(row_for("🔹 普通攻擊", "基礎攻擊力，不套用任何技能係數。",
        cur_sd == nullptr, "normal"));

    for (auto& skill_job : maple_visible_skill_jobs(c)) {
        for (auto* sd : maple_skills_for_job(skill_job)) {
            if (sd->type != "damage_fixed" && sd->type != "damage_coef") continue;
            int lvl = maple_skill_level(c, sd->key);
            if (lvl <= 0) continue; // 尚未學習，不能選
            container.add_component_v2(row_for(sd->name,
                "Lv." + std::to_string(lvl) + "／" + maple_skill_value_text(*sd, lvl),
                cur_sd == sd, sd->key));
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

    auto stat_text = [&](const std::string& stat, int value) {
        return dpp::component().set_type(dpp::cot_text_display)
            .set_content("**" + maple_stat_name(stat) + "**：" + std::to_string(value));
    };
    container.add_component_v2(stat_text("str", c.str_stat));
    container.add_component_v2(stat_text("dex", c.dex_stat));
    container.add_component_v2(stat_text("int", c.int_stat));
    container.add_component_v2(stat_text("luk", c.luk_stat));
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
        std::string text = "**" + slot.icon + " " + slot.name + "**：";
        if (item) {
            text += item->name;
            if (item->atk_bonus > 0) text += "（+" + std::to_string(item->atk_bonus) + " ATK）";
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
        .set_label("↩ 返回").set_id("maple_home_" + uid_s).set_style(dpp::cos_secondary));
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
        any = true;
        bool equipped = (item.key == cur_key);
        bool eligible = maple_meets_requirement(c, item);
        std::string text = "**" + item.name + "**";
        if (item.atk_bonus > 0) text += "　⚔️+" + std::to_string(item.atk_bonus);
        text += "\n限制等級 " + std::to_string(item.level_req)
              + "　限制主屬性 " + std::to_string(item.primary_req)
              + "　限制副屬性 " + std::to_string(item.secondary_req)
              + (item.sellable ? "" : "　🚫無法售出");
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
            .set_content("目前沒有可裝備的" + slot_name + "道具。"));
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
        .set_content("## 🗺️ 冒險\n攻擊間隔固定 " + std::to_string(MAPLE_ADV_ATTACK_INTERVAL_SEC) + " 秒／次，建議等級僅供參考、未達也能進入。"));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));

    for (auto& r : MAPLE_ADV_REGIONS) {
        std::string text = "**" + r.name + "**　建議 Lv. " + std::to_string(r.suggested_level) + "~";
        if (!r.open) text += "　🚧尚未開放";
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
                 + std::to_string(region->monster.exp) + " EXP\n\n";
        content += "**預估收益（每小時）**\n";
        content += "✨ 經驗值：約 **" + std::to_string((int64_t)llround(eph)) + "**\n";
        content += "🪙 瘋幣：約 **" + std::to_string((int64_t)llround(cph)) + "**\n";
        content += "-# 依目前攻擊力平均值估算，實際會因隨機浮動而略有差異";
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

    std::string content = "## 🗺️ 冒險中 — " + (region ? region->name : c.adv_region) + "\n";
    content += "已經過 **" + maple_fmt_duration(secs) + "**\n\n";
    content += "**目前累積**\n";
    content += "✨ 經驗值 +**" + std::to_string(exp_now) + "**\n";
    content += "🪙 瘋幣 +**" + std::to_string(coins_now) + "**\n";
    content += "-# 冒險期間無法調整裝備與能力值";

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
