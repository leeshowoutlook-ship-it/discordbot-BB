#pragma once
#include "chips.h"
#include "gacha.h"
#include "announcement.h"
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
    {"path_reincarnate", "轉生卡",       10000, "evolution", "使用後可轉生成其他品種的預設形態（保留階段與經驗值，需已進化）", 40007},
    {"path_rebirth",     "重生卡",        7000, "evolution", "使用後可重新選擇目前品種的分支路線（保留階段與經驗值，需二階以上）", 40008},
    // ── Talent items ─────────────────────────────────────────────────────────
    {"talent_scroll",  "天賦賦予卷軸",       15000, "talent", "100% 為寵物賦予一個天賦（可自選）",  50001},
    {"talent_class",   "送去上才藝班",        2000, "talent", "25% 為寵物發現一個隨機天賦",                            50002},
    {"talent_reroll",  "你不可以學畫畫!",    2000, "talent", "重新抽一個不同的天賦（需已有天賦）",                    50003},
    {"talent2_unlock", "第二天賦解鎖石",    50000, "talent", "使用後解鎖第二天賦欄位，之後才能用天賦道具賦予第二天賦（不可跟第一天賦相同）", 50004},
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
    {"orb_shard_speed",  "迅捷狼王的寶珠碎片", 0, "shard", "10 個可合成「迅捷狼王的寶珠」，單人必定先手；組隊：40%機率多行動一回合",  95001},
    {"orb_shard_athena", "雅典娜的寶珠碎片",   0, "shard", "10 個可合成「雅典娜的寶珠」，單人30%恢復8滴血；組隊20%全體恢復5滴血", 95002},
    {"orb_shard_bear",   "巨山狂熊的寶珠碎片", 0, "shard", "10 個可合成「巨山狂熊的寶珠」，單人：防禦降低怪物下兩次攻擊60%；組隊：防禦降低範圍攻擊傷害20%／集中攻擊傷害50%", 95003},
    {"orb_shard_viking", "維京的寶珠碎片",     0, "shard", "10 個可合成「維京的寶珠」，HP≤50% 傷害×1.4，HP≤25% 傷害×1.7（被動狂暴）", 95004},
    {"orb_shard_wargod",      "狂怒戰神的寶珠碎片", 0, "shard", "10 個可合成「狂怒戰神的寶珠」，裝備後攻擊力+10",                              95005},
    {"orb_shard_latus",      "拉圖斯的寶珠碎片",   0, "shard", "10 個可合成「拉圖斯的寶珠」，HP≤20%時回復至50%（每場一次）",               95006},
    {"orb_shard_darkdragon", "暗黑龍王的寶珠碎片", 0, "shard", "10 個可合成「暗黑龍王的寶珠」，攻擊後回復造成傷害的 1/10（最多回10HP）", 95007},
    // ── 特權道具 ─────────────────────────────────────────────────────────────
    {"vip_daily",            "尊爵VIP（每日）",  8000, "privilege", "使用後 24 小時內，每小時自動為你領取籌碼", 99001},
    {"pet_supervisor_daily", "寵物監工（每日）", 1000, "privilege", "使用後 24 小時內，寵物打工結束 10 分鐘後若未領取，自動以 0.6 倍收益再次出勤", 99002},
    {"pet_insurance",        "醫療保險",         1000, "privilege", "使用後三天內，寵物打工回來若生病（受傷除外），立即給付 4000 保險金並結束效果", 99003},
    // ── 探險收藏品 ───────────────────────────────────────────────────────────
    {"col_ms_handkerchief", "菇菇仔的小手帕",   0, "collectible", "從菇菇王國探險取得的蒐藏品", 90001},
    {"col_gm_beret",        "綠菇菇的貝雷帽",   0, "collectible", "從菇菇王國探險取得的蒐藏品", 90002},
    {"col_sm_spine",        "刺菇菇軟掉的刺",   0, "collectible", "從菇菇王國探險取得的蒐藏品", 90003},
    {"col_bm_tear",         "傷心藍菇菇的眼淚", 0, "collectible", "從菇菇王國探險取得的蒐藏品", 90004},
    {"col_zm_cheese",       "殭屍菇菇的起士",   0, "collectible", "從菇菇王國探險取得的蒐藏品", 90005},
    {"col_yaya_bounty",     "呀呀的懸賞令",     0, "collectible", "持有後每天多獲得 1 張怪物狩獵卷（與日常上限分開計算）", 90006},
    {"col_mushroom_head",   "蘑菇頭",           0, "collectible", "從菇菇王國探險取得的蒐藏品（稀有）", 90007},
    {"col_mb_crown",        "菇菇寶貝的王冠",   0, "collectible", "從菇菇王國探險取得的蒐藏品（稀有）", 90008},
    {"col_mb_staff",        "菇菇寶貝的手杖",   0, "collectible", "從菇菇王國探險取得的蒐藏品（稀有）", 90009},
    {"col_slim_wallet",     "皮包消瘦的皮包",   0, "collectible", "持有後每次打工金額增加 3%（可與膨脹皮包疊加）", 90010},
    {"col_fat_wallet",      "皮包膨脹的皮包",   0, "collectible", "持有後每次打工金額增加 7%（可與消瘦皮包疊加）", 90011},
    // ── 綠水靈洞窟 蒐藏品 ───────────────────────────────────────────────────────
    {"col_gwl_popsicle",    "綠水靈的情人果冰棒",  0, "collectible", "從綠水靈洞窟探險取得的蒐藏品", 90012},
    {"col_bwl_cake",        "藍水靈的藍莓蛋糕",    0, "collectible", "從綠水靈洞窟探險取得的蒐藏品", 90013},
    {"col_dwl_tiramisu",    "惡魔水靈的提拉米蘇",  0, "collectible", "從綠水靈洞窟探險取得的蒐藏品", 90014},
    {"col_rwl_velvet",      "紅水靈的紅絲絨",      0, "collectible", "從綠水靈洞窟探險取得的蒐藏品", 90015},
    {"col_awl_avocado",     "天使綠水靈的酪梨奶酪",0, "collectible", "從綠水靈洞窟探險取得的蒐藏品（稀有）", 90016},
    {"col_sqwl_brownie",    "方塊水靈的布朗尼",    0, "collectible", "從綠水靈洞窟探險取得的蒐藏品（稀有）", 90017},
    {"col_ywl_caramel",     "黃水靈的焦糖布丁",    0, "collectible", "從綠水靈洞窟探險取得的蒐藏品（稀有）", 90018},
    {"col_phone_tianxin",   "恬心貸的電話號碼",    0, "collectible", "借款上限增加 30000 碼，借款利率改為 2.98%/天（限定）", 90019},
    {"col_bath_huaxuan",    "華瑄的洗澡卡",        0, "collectible", "每週可迴避 3 次打工造成的負面狀態（限定）", 90020},
    {"col_rod_zoey",        "Zoey的黃金釣竿",      0, "collectible", "泡溫泉結束時：25% 獲得商城道具、25% 獲得 2500 碼、50% 無（限定）", 90021},
    // ── 亡魂墓地 蒐藏品 ──────────────────────────────────────────────────────────
    {"col_ghost_heels",     "幽魂的玻璃高跟鞋",    0, "collectible", "從亡魂墓地探險取得的蒐藏品", 90022},
    {"col_kappa_cucumber",  "河童奇形怪狀的小黃瓜",0, "collectible", "從亡魂墓地探險取得的蒐藏品", 90023},
    {"col_zombie_eyepatch", "礦山殭屍發霉的眼罩",  0, "collectible", "從亡魂墓地探險取得的蒐藏品", 90024},
    {"col_ghost_cloak",     "幽魂髒到不行的斗篷",  0, "collectible", "從亡魂墓地探險取得的蒐藏品", 90025},
    {"col_witch_broom",     "巫婆的飛天掃帚",      0, "collectible", "從亡魂墓地探險取得的蒐藏品", 90026},
    {"col_demon_tear",      "魔精靈的冰晶眼淚",    0, "collectible", "從亡魂墓地探險取得的蒐藏品（稀有）", 90027},
    {"col_demon_heart",     "魔精靈跳動的心臟",    0, "collectible", "從亡魂墓地探險取得的蒐藏品（稀有）", 90028},
    {"col_demon_horn",      "魔精靈破損的角",      0, "collectible", "從亡魂墓地探險取得的蒐藏品（稀有）", 90029},
    {"col_demon_costume",   "魔精靈的布偶裝",      0, "collectible", "從亡魂墓地探險取得的蒐藏品（稀有）", 90030},
    {"col_penguin_relic",   "企鵝幫的聖物",        0, "collectible", "持有後寵物防禦力 +1（限定）", 90031},
    {"col_shark_relic",     "鬼鯊隊的聖物",        0, "collectible", "持有後寵物攻擊力 +1（限定）", 90032},
    {"col_koala_relic",     "考拉幫的聖物",        0, "collectible", "持有後寵物生命值 +10（限定）", 90033},
    {"col_koala_autograph", "考拉的親筆簽名",      0, "collectible", "持有後打工時長減少 3%（限定）", 90034},
    // ── BB自然博物館 蒐藏品 ─────────────────────────────────────────────────────
    {"col_bb_pink_cup",       "皮卡啾的粉紅酒杯",       0, "collectible", "從BB自然博物館探險取得的蒐藏品", 90035},
    {"col_bb_desk_terror",    "戴斯克巨大的恐怖",       0, "collectible", "從BB自然博物館探險取得的蒐藏品", 90036},
    {"col_bb_signus_chalice", "西格諾斯惡魔的聖杯",     0, "collectible", "從BB自然博物館探險取得的蒐藏品", 90037},
    {"col_bb_mercury_staff",  "厄運死神的汞心石杖",     0, "collectible", "從BB自然博物館探險取得的蒐藏品", 90038},
    {"col_bb_risk_dice",      "园园的風險骰子",         0, "collectible", "特殊道具，背包「特殊」分頁可使用。一天可擲 2 次：1% -5000碼／10% +5000碼／89% -500碼（限定）", 90039},
    {"col_bb_horn",           "利里諾斯的角",           0, "collectible", "從BB自然博物館探險取得的蒐藏品", 90040},
    {"col_bb_death_ring",     "達納托斯的魔戒",         0, "collectible", "從BB自然博物館探險取得的蒐藏品", 90041},
    {"col_bb_ski",            "雪毛怪人的滑雪板",       0, "collectible", "從BB自然博物館探險取得的蒐藏品", 90042},
    {"col_bb_sian_cloak",     "Sian的隱形斗篷",         0, "collectible", "特殊道具（限定）。效果：戰鬥中每回合有 1% 機率完全閃避怪物攻擊", 90043},
    {"col_bb_lost_underwear", "觀觀遺失的胖次",         0, "collectible", "特殊道具（限定）。效果：每場戰鬥第一次攻擊，攻擊力 +5", 90044},
    {"col_bb_magnifier",      "Xu的探險放大鏡",         0, "collectible", "特殊道具（限定）。持有後探索時長 -5%", 90045},
    {"col_bb_blood_gem",      "殘暴炎魔血色秘石",       0, "collectible", "從BB自然博物館探險取得的蒐藏品（稀有）", 90046},
    {"col_bb_bracelet",       "深山鬼怪青光手鐲",       0, "collectible", "從BB自然博物館探險取得的蒐藏品（稀有）", 90047},
    {"col_bb_mirror",         "雪女的八咫鏡",           0, "collectible", "從BB自然博物館探險取得的蒐藏品（稀有）", 90048},
    {"col_bb_wig_broken",     "Zoey不正常的假髮（戰損版）",   0, "collectible", "特殊道具，全球限量 5 份。持有時探險收取結果有 1% 機率額外骰一次戰利品，可疊加機率；5 個可用 !合成 換成「Zoey散發氣味的秀髮」", 90049},
    {"col_bb_undies_broken",  "皮包的粉紅內衣碎片（戰損版）", 0, "collectible", "特殊道具，全球限量 5 份。持有時探險完成有 2% 機率返還該次探索花費的資金，可疊加機率；5 個可用 !合成 換成「皮包遺失的粉紅內衣」", 90050},
    {"col_bb_wig_full",       "Zoey散發氣味的秀髮",     0, "collectible", "由 5 個「Zoey不正常的假髮（戰損版）」合成。探險收取結果有 10% 機率額外骰一次戰利品，可疊加機率", 90051},
    {"col_bb_undies_full",    "皮包遺失的粉紅內衣",     0, "collectible", "由 5 個「皮包的粉紅內衣碎片（戰損版）」合成。探險完成有 20% 機率返還該次探索花費的資金，可疊加機率", 90052},
    // ── 赤龍山脈 蒐藏品 ─────────────────────────────────────────────────────────
    {"col_rd_amber",       "赤龍的血色琥珀",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90053},
    {"col_rd_claw",        "赤龍的燎原之爪",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90054},
    {"col_rd_azurescale",  "青龍的蒼玉龍鱗",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90055},
    {"col_rd_azuremarrow", "青龍的碧落龍髓",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90056},
    {"col_rd_earthbone",   "土龍的磐石龍骨",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90057},
    {"col_rd_earthblood",  "土龍的地堭龍血",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90058},
    {"col_rd_iceeye",      "冰龍的寒月之瞳",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90059},
    {"col_rd_icescale",    "冰龍的霜天之鱗",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90060},
    {"col_rd_blackwing",   "散發不詳氣息的黑龍之翼", 0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90061},
    {"col_rd_blackeye",    "黑龍的幽冥龍瞳",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90062},
    {"col_rd_demonclaw",   "魔龍的嗜魂魔爪",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90063},
    {"col_rd_demoneye",    "魔龍的混沌龍瞳",         0, "collectible", "從赤龍山脈探險取得的蒐藏品", 90064},
    {"col_rd_rainbow",     "龍族七彩石",             0, "collectible", "從赤龍山脈探險取得的蒐藏品（稀有）", 90065},
    {"col_rd_azureorb",    "青龍・蒼穹龍珠",         0, "collectible", "從赤龍山脈探險取得的蒐藏品（稀有）", 90066},
    {"col_rd_redorb",      "赤龍・焚世龍珠",         0, "collectible", "從赤龍山脈探險取得的蒐藏品（稀有）", 90067},
    {"col_rd_iceorb",      "冰龍・寒光龍珠",         0, "collectible", "從赤龍山脈探險取得的蒐藏品（稀有）", 90068},
    {"col_rd_blackorb",    "黑龍・闇夜龍珠",         0, "collectible", "從赤龍山脈探險取得的蒐藏品（稀有）", 90069},
    {"col_rd_demonorb",    "魔龍・幽冥龍珠",         0, "collectible", "從赤龍山脈探險取得的蒐藏品（稀有）", 90070},
    {"col_rd_earthorb",    "土龍・泰岳龍珠",         0, "collectible", "從赤龍山脈探險取得的蒐藏品（稀有）", 90071},
    // 赤龍山脈 限定收藏（全球僅1份）
    {"col_rd_campticket", "Sneaky的自願留營表", 0, "collectible",
        "特殊道具（限定）。持有時打工只剩「24小時／12000碼／20exp／必定憂鬱」單一選項，不會觸發監工代打，也不會觸發醫療保險", 90072},
    {"col_rd_lovebook",   "貓哥的戀愛教典",     0, "collectible",
        "特殊道具（限定）。虛擬商店購買 95 折、轉帳與交易免手續費。預設不可交易，需在背包「特殊」分頁付 2000 碼解鎖一次性可交易，交易完成後恢復不可交易", 90073},
    {"col_rd_simpmanual", "天元的舔狗密笈",     0, "collectible",
        "特殊道具（限定）。背包「特殊」分頁每天可領取 1 杯「高級強效咖啡」，但領取者本人無法使用或售出，須交易給別人後才能使用", 90074},
    {"col_rd_dogbook",    "左邊畫個龍的柴犬百科全書", 0, "collectible",
        "特殊道具（限定）。背包「特殊」分頁每週可用 4 次，隨機抽取虛擬商店 3000 碼以下道具（25% 失敗機率），次數全域計算、不因交易換人而重置", 90075},
    // 綠水靈洞窟 限定收藏（全球僅1份）
    {"col_cat_tears",    "貓哥的眼淚",   0, "collectible", "特殊道具（限定）。效果：戰鬥中受到傷害時，5% 機率恢復 5 點血量", 90076},
    {"col_golden_staff", "李秀的金箍棒", 0, "collectible", "特殊道具（限定）。效果：戰鬥中攻擊時，1% 機率額外多打一下", 90077},
    // 赤龍山脈 龍族寶箱（消耗品，非收藏品）
    {"dragon_chest_small", "小型龍族寶箱", 2500, "consumable", "使用後獲得 1500~4500 碼", 96001},
    {"dragon_chest_mid",   "中型龍族寶箱", 5500, "consumable", "使用後獲得 4500~7500 碼", 96002},
    {"dragon_chest_grand", "豪邁龍族寶箱", 12000, "consumable", "使用後獲得 10500~15000 碼", 96003},
    // 天元的舔狗密笈 專用：被詛咒的咖啡（跟一般高級強效咖啡效果相同，但領取者本人不可用/不可賣）
    {"recover_fatigue_cursed", "高級強效咖啡（被詛咒）", 0, "recovery",
        "由「天元的舔狗密笈」每日領取產生。目前持有者無法使用或售出，交易給別人後會變回一般的高級強效咖啡", 96004},
};

static const VirtualShopItem* find_virtual_item(const std::string& key) {
    for (auto& vi : VIRTUAL_ITEMS)
        if (vi.key == key) return &vi;
    return nullptr;
}

// 回收（賣回商店）價格：一般道具為購買價的 40%，龍族寶箱等消耗品無購買管道，price 本身即為回收價
static int64_t vi_sell_price(const VirtualShopItem* vi) {
    if (!vi || vi->price <= 0 || vi->category == "hunt") return 0;
    if (vi->category == "consumable") return vi->price;
    return std::max((int64_t)1, (int64_t)(vi->price * 0.4));
}

static const VirtualShopItem* find_virtual_item_by_id(int id) {
    if (!id) return nullptr;
    for (auto& vi : VIRTUAL_ITEMS)
        if (vi.item_id == id) return &vi;
    return nullptr;
}

// ─── 天賦定義（唯一資料來源，其餘地方都引用這裡，不要各自複製一份清單）───────────

struct TalentDef { std::string key, effect_desc; };
static const std::vector<TalentDef> ALL_TALENTS = {
    {"迅捷",     "打工時間縮短 10%"},
    {"招人喜歡", "打工報酬 +10%"},
    {"幸運",     "5% 機率打工雙倍報酬"},
    {"天然呆",   "使用道具時有 5% 機率不消耗道具"},
    {"喜歡作夢", "每次打工完有 0.1% 機率將現有籌碼翻倍"},
    {"求生專家", "帶去探險時，探索度額外 +10"},
    {"膽小鬼",   "帶去探險時，探索時長 -15%"},
    {"尋寶專家", "帶去探險時，若沒有收穫會額外進行一次判定，但探索時長 +20%"},
};
static std::string talent_effect_desc(const std::string& t) {
    for (auto& d : ALL_TALENTS) if (d.key == t) return d.effect_desc;
    return "";
}
// 挑選天賦清單，排除掉一或兩個天賦（用於：兩個天賦欄位不能相同）
static std::vector<std::string> talent_pool_excluding(const std::string& e1, const std::string& e2 = "") {
    std::vector<std::string> out;
    for (auto& d : ALL_TALENTS) if (d.key != e1 && (e2.empty() || d.key != e2)) out.push_back(d.key);
    return out;
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

// ─── 轉生卡／重生卡：品種／分支切換 ─────────────────────────────────────────────

static const std::vector<std::string> ALL_PET_CHAINS = {"嫩寶", "菇菇仔", "肥肥", "小企鵝"};

static std::string chain_to_slug(const std::string& chain) {
    if (chain == "嫩寶")   return "nunbao";
    if (chain == "菇菇仔") return "gugu";
    if (chain == "肥肥")   return "feifei";
    if (chain == "小企鵝") return "penguin";
    return "";
}
static std::string slug_to_chain(const std::string& slug) {
    if (slug == "nunbao")  return "嫩寶";
    if (slug == "gugu")    return "菇菇仔";
    if (slug == "feifei")  return "肥肥";
    if (slug == "penguin") return "小企鵝";
    return "";
}

// 重生卡：同品種內可選的分支（variant slug、variant 值、顯示名稱）
struct RebirthOption { std::string slug, variant, label; };
static std::vector<RebirthOption> rebirth_options(const std::string& chain) {
    if (chain == "嫩寶") return {
        {"default", "",   "紅寶（預設路線）"},
        {"moss",    "苔蘚", "苔蘚（分支路線）"},
    };
    if (chain == "菇菇仔") return {
        {"default", "",   "菇菇寶貝（預設路線）"},
        {"zombie",  "殭屍", "殭屍菇菇（分支路線）"},
        {"bluemush","藍菇", "藍菇菇（分支路線）"},
    };
    if (chain == "肥肥") return {
        {"default", "",   "緞帶肥肥（預設路線）"},
        {"desert",  "沙漠", "黑肥肥（分支路線）"},
    };
    if (chain == "小企鵝") return {
        {"default",     "",     "槍企鵝（預設路線）"},
        {"penguinking", "企鵝王", "企鵝王（分支路線）"},
    };
    return {};
}

static dpp::message make_reincarnate_pick_msg(dpp::snowflake uid, const std::string& notice = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    Pet pet; bool has_pet = false; int card_count = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end()) { pet = it->second; has_pet = true; }
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) {
            auto ci = ii->second.find("path_reincarnate");
            if (ci != ii->second.end()) card_count = ci->second;
        }
    }
    dpp::message msg; msg.set_flags(dpp::m_using_components_v2);
    if (!has_pet || pet.stage == 0) {
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🔄 轉生卡 — 選擇品種\n❌ 需要已進化的寵物才能使用轉生卡！"));
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回").set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(ct); msg.add_component_v2(row);
        return msg;
    }
    std::string desc = "## 🔄 轉生卡 — 選擇品種\n";
    if (!notice.empty()) desc += notice + "\n\n";
    desc += "目前品種：**" + pet_name(pet.chain, pet.stage, pet.variant) + "**\n";
    desc += "持有轉生卡：**" + std::to_string(card_count) + "** 張\n\n";
    desc += (card_count > 0)
        ? "選擇要轉生成的品種（保留階段與經驗值，消耗 1 張轉生卡）："
        : "❌ 沒有轉生卡了！";
    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(desc));
    msg.add_component_v2(ct);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& chain : ALL_PET_CHAINS) {
        if (chain == pet.chain) continue;
        dpp::component btn;
        btn.set_type(dpp::cot_button)
           .set_label(pet_name(chain, pet.stage, ""))
           .set_id("pet_reincarnate_" + uid_s + "_" + chain_to_slug(chain))
           .set_style(dpp::cos_primary)
           .set_disabled(card_count <= 0);
        row.add_component(btn);
    }
    msg.add_component_v2(row);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

static dpp::message make_rebirth_pick_msg(dpp::snowflake uid, const std::string& notice = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    Pet pet; bool has_pet = false; int card_count = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end()) { pet = it->second; has_pet = true; }
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) {
            auto ci = ii->second.find("path_rebirth");
            if (ci != ii->second.end()) card_count = ci->second;
        }
    }
    dpp::message msg; msg.set_flags(dpp::m_using_components_v2);
    if (!has_pet || pet.stage < 2) {
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🌀 重生卡 — 選擇分支路線\n❌ 需要二階以上的寵物才能使用重生卡！（一階分支尚未分歧）"));
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回").set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(ct); msg.add_component_v2(row);
        return msg;
    }
    std::string desc = "## 🌀 重生卡 — 選擇分支路線\n";
    if (!notice.empty()) desc += notice + "\n\n";
    desc += "目前路線：**" + pet_name(pet.chain, pet.stage, pet.variant) + "**\n";
    desc += "持有重生卡：**" + std::to_string(card_count) + "** 張\n\n";
    desc += (card_count > 0)
        ? "選擇要重生成的分支路線（保留階段與經驗值，消耗 1 張重生卡）："
        : "❌ 沒有重生卡了！";
    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(desc));
    msg.add_component_v2(ct);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto& o : rebirth_options(pet.chain)) {
        bool is_current = (o.variant == pet.variant);
        dpp::component btn;
        btn.set_type(dpp::cot_button)
           .set_label((is_current ? "✅ " : "") + o.label)
           .set_id("pet_rebirth_" + uid_s + "_" + o.slug)
           .set_style(is_current ? dpp::cos_secondary : dpp::cos_primary)
           .set_disabled(is_current || card_count <= 0);
        row.add_component(btn);
    }
    msg.add_component_v2(row);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
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
            p.talent2     = v.value("talent2",      std::string{});
            p.talent2_unlocked = v.value("talent2_unlocked", false);
            p.enh_atk     = v.value("enh_atk",       0);
            p.enh_def     = v.value("enh_def",       0);
            p.enh_hp      = v.value("enh_hp",        0);
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
                {"talent", p.talent}, {"talent2", p.talent2}, {"talent2_unlocked", p.talent2_unlocked},
                {"enh_atk", p.enh_atk}, {"enh_def", p.enh_def}, {"enh_hp", p.enh_hp},
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

// ─── Backpack home（Components V2：清單排版，右側貼一顆按鈕）─────────────────────
// 注意：Components V2 訊息不能混用傳統 embed，這則訊息全部改用新元件拼出來；
// 點進去之後的各分頁（裝備/消耗/其他/收藏/特殊）維持原本的 embed 畫面不變。

static dpp::message make_bag_home_msg(dpp::snowflake uid,
                                       const std::string& dn = "", const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);

    auto mk_section = [&](const std::string& text, const std::string& btn_id) {
        return dpp::component()
            .set_type(dpp::cot_section)
            .add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text))
            .set_accessory(dpp::component().set_type(dpp::cot_button)
                .set_label("查看").set_id(btn_id).set_style(dpp::cos_secondary));
    };

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x58, 0x65, 0xF2));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🎒 背包\n" + (dn.empty() ? uid_s : dn) + " 的背包，選擇要查看的分頁："));
    container.add_component_v2(dpp::component().set_type(dpp::cot_separator)
        .set_spacing(dpp::sep_small).set_divider(true));
    container.add_component_v2(mk_section("**⚔️ 裝備**\n武器、手套、套服、鞋子、靈魂寶珠", "bag_tab_equip_" + uid_s));
    container.add_component_v2(mk_section("**🎒 消耗**\n可以直接使用的道具", "bag_tab_items_" + uid_s));
    container.add_component_v2(mk_section("**📦 其他**\n狩獵卷、寶珠碎片等材料類道具", "bag_tab_other_" + uid_s));
    container.add_component_v2(mk_section("**📚 收藏**\n探險取得的收藏品", "adv_collection_" + uid_s));
    container.add_component_v2(mk_section("**🌟 特殊**\n限定道具與股票持有", "bag_tab_special_" + uid_s));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);

    return msg;
}

// ─── Pet view ─────────────────────────────────────────────────────────────────

static dpp::message make_lobby_msg(dpp::snowflake uid,
                                    const std::string& avatar_url = "",
                                    const std::string& display_name = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string user_tag = display_name.empty() ? uid_s : display_name;

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x58, 0x65, 0xF2));
    container.add_component_v2(v2_section("## 🏠 大廳\n" + announcement_lobby_line() + "請選擇要前往的頁面：\n\n-# 👤 " + user_tag, avatar_url));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    dpp::component row1; row1.set_type(dpp::cot_action_row);
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎒 背包").set_id("bag_home_" + uid_s).set_style(dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💼 錢包").set_id("wallet_home_" + uid_s).set_style(dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏪 商店").set_id("lobby_shop_" + uid_s).set_style(dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🐾 寵物").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_primary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔨 合成").set_id("craft_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row1);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⚔️ 裝備").set_id("equip_main_" + uid_s).set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💪 強化").set_id("enh_main_" + uid_s).set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗡️ 怪物狩獵").set_id("hunt_main_" + uid_s).set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗺️ 探險").set_id("adv_main_" + uid_s).set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📚 收藏").set_id("adv_collection_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);

    dpp::component row3; row3.set_type(dpp::cot_action_row);
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏦 銀行").set_id("wallet_bank_" + uid_s).set_style(dpp::cos_secondary));
    row3.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("📊 股票").set_id("stock_home_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row3);

    return msg;
}

static dpp::message make_pet_work_select_msg(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    Pet pet;
    bool has_camp_ticket = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end()) pet = it->second;
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end() && ii->second.count("col_rd_campticket") && ii->second.at("col_rd_campticket") > 0)
            has_camp_ticket = true;
    }

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    if (has_camp_ticket) {
        dpp::component container;
        container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 💼 選擇打工時長\n持有「Sneaky的自願留營表」，只能選擇留營：\n"
                         "🏕️ **24小時**（+12000碼／+20exp／必定憂鬱，不觸發監工／醫療保險）"));
        msg.add_component_v2(container);

        dpp::component row1; row1.set_type(dpp::cot_action_row);
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏕️ 24小時留營（必定憂鬱）")
            .set_id("pet_work_" + uid_s + "_24").set_style(dpp::cos_danger));
        msg.add_component_v2(row1);

        dpp::component row2; row2.set_type(dpp::cot_action_row);
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("↩ 返回").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(row2);
        return msg;
    }

    auto opts = work_options(pet.stage);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 💼 選擇打工時長\n請選擇打工時長："));
    msg.add_component_v2(container);

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
    msg.add_component_v2(row1);

    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(row2);
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
    std::string user_tag = display_name.empty() ? uid_s : display_name;
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    if (!has_pet) {
        dpp::component container;
        container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🐾 我的寵物\n你還沒有寵物！\n前往 **商店 → 虛擬商店 → 寵物蛋** 購買蛋。\n\n-# 👤 " + user_tag));
        msg.add_component_v2(container);
        return msg;
    }

    std::string name = pet_name(pet.chain, pet.stage, pet.variant);
    std::string display_n = pet.custom_name.empty() ? name : (pet.custom_name + "（" + name + "）");

    if (pet.stage == 0) {
        dpp::component container;
        container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🐾 " + display_n + "\n這是一顆 **" + name + "**\n使用孵蛋工具來孵化它！\n\n**狀態** 🥚 未孵化\n\n-# 👤 " + user_tag));
        msg.add_component_v2(container);

        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🎒 背包").set_id("pet_open_use_" + uid_s).set_style(dpp::cos_primary));
        msg.add_component_v2(row);

        dpp::component rowlob; rowlob.set_type(dpp::cot_action_row);
        rowlob.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(rowlob);

        dpp::component rowrel; rowrel.set_type(dpp::cot_action_row);
        rowrel.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🕊️ 放生").set_id("pet_release_" + uid_s).set_style(dpp::cos_danger));
        msg.add_component_v2(rowrel);
        return msg;
    }

    // Stage 1-3: build content string
    int need = exp_needed(pet.stage);
    std::string exp_str = std::to_string(pet.exp);
    if (pet.stage < 3) exp_str += " / " + std::to_string(need);

    auto talent_line = [](const std::string& t) -> std::string {
        if (t.empty()) return "無";
        std::string d = talent_effect_desc(t);
        return d.empty() ? t : (t + "：" + d);
    };
    std::string talent1_display = talent_line(pet.talent);
    std::string talent2_display = pet.talent2_unlocked ? talent_line(pet.talent2) : "🔒 未解鎖（使用第二天賦解鎖石）";

    PetStats stats = calc_pet_stats(uid, pet);
    int max_hp = stats.hp;
    { std::lock_guard<std::mutex> lk(data_mutex);
      apply_pet_basic_set_bonus(uid, pet, stats.atk, stats.hp, max_hp, stats.def); }

    std::string content = "## 🐾 " + display_n + "\n";
    content += "📊 **階段** 第 " + std::to_string(pet.stage) + " 階　";
    content += "✨ **經驗值** " + exp_str + "\n";
    content += "✦ **天賦一** " + talent1_display + "\n";
    content += "✦ **天賦二** " + talent2_display + "\n";
    content += "⚔️ **攻擊力** " + std::to_string(stats.atk) + "　";
    content += "❤️ **生命值** " + std::to_string(stats.hp) + "　";
    content += "🛡️ **防禦力** " + std::to_string(stats.def) + "\n";

    if (!pet.statuses.empty()) {
        static const std::map<std::string,std::string> STATUS_DESC = {
            {"受傷",    "受傷：無法狩獵，打工報酬 -10%"},
            {"憂鬱",    "憂鬱：打工報酬 -20%，有機率隨機花錢"},
            {"肌肉緊繃","肌肉緊繃：狩獵時 30% 機率攻擊失敗"},
            {"疲勞",    "疲勞：打工時長 +30%"},
        };
        std::string status_str;
        for (auto& s : pet.statuses) status_str += "⚠️ **" + s + "**  ";
        content += "🩹 **狀態** " + status_str + "\n";
        for (auto& s : pet.statuses)
            if (STATUS_DESC.count(s)) content += "• " + STATUS_DESC.at(s) + "\n";
    }

    time_t now = time(nullptr);

    // Handle onsen completion: auto-clear debuffs
    bool onsen_done = (pet.onsen_end > 0 && pet.onsen_end <= now);
    std::string onsen_bonus_msg;
    if (onsen_done) {
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& p = pet_data[uid];
            p.statuses.clear();
            p.onsen_end = 0;
            pet = p;
        }
        save_pet_data();
        bool has_rod = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto wi = inventory_data.find(uid);
          if (wi != inventory_data.end()) {
              auto jt = wi->second.find("col_rod_zoey");
              has_rod = (jt != wi->second.end() && jt->second > 0);
          }
        }
        if (has_rod) {
            static std::mt19937 onsen_rng(std::random_device{}());
            int roll = std::uniform_int_distribution<int>(1,100)(onsen_rng);
            if (roll <= 25) {
                static const std::vector<std::string> SHOP_ITEMS = {
                    "recover_depress","recover_injury","recover_muscle","recover_fatigue"
                };
                std::string gift = SHOP_ITEMS[std::uniform_int_distribution<int>(0,3)(onsen_rng)];
                { std::lock_guard<std::mutex> lk(data_mutex); inventory_data[uid][gift]++; }
                save_inventory();
                auto* vi = find_virtual_item(gift);
                onsen_bonus_msg = "🎣 **Zoey的黃金釣竿**：獲得商城道具「**" + (vi ? vi->name : gift) + "**」×1！";
            } else if (roll <= 50) {
                add_chips(uid, 2500); save_chips();
                onsen_bonus_msg = "🎣 **Zoey的黃金釣竿**：獲得 **+2500 碼**！";
            } else {
                onsen_bonus_msg = "🎣 **Zoey的黃金釣竿**：這次什麼都沒有...";
            }
        }
    }

    bool in_onsen  = (pet.onsen_end > 0 && pet.onsen_end > now);
    bool working   = (pet.work_task > 0 && pet.work_end > now);
    bool work_done = (pet.work_task > 0 && pet.work_end <= now);

    auto add_lobby_row = [&]() {
        dpp::component r; r.set_type(dpp::cot_action_row);
        r.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(r);
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
        msg.add_component_v2(r);
    };
    std::string pet_img = pet_image_url(pet.chain, pet.stage, pet.variant);
    auto make_container = [&](const std::string& c) {
        dpp::component ct;
        ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x9B, 0x59, 0xB6));
        ct.add_component_v2(v2_section(c + "\n\n-# 👤 " + user_tag, pet_img));
        msg.add_component_v2(ct);
    };

    if (in_onsen) {
        int remain = (int)(pet.onsen_end - now);
        int h = remain/3600, m2 = (remain%3600)/60, s2 = remain%60;
        char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m2, s2);
        content += "🛀 **溫泉狀態** 🌊 泡溫泉中，剩餘 " + std::string(buf) + "\n結束後自動清除所有負面狀態\n";
        make_container(content);
        dpp::component row1; row1.set_type(dpp::cot_action_row);
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🔄 刷新").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary));
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("❌ 取消泡溫泉").set_id("pet_cancel_onsen_" + uid_s).set_style(dpp::cos_danger));
        msg.add_component_v2(row1);
        add_lobby_row();
        add_utility_row();
        return msg;
    }

    if (onsen_done) {
        std::string onsen_result = "✨ 溫泉療癒完成！所有負面狀態已清除！";
        if (!onsen_bonus_msg.empty()) onsen_result += "\n" + onsen_bonus_msg;
        content += "🛀 **泡溫泉結果** " + onsen_result + "\n";
    }

    if (working) {
        std::string status_label = pet.is_supervisor_work
            ? "⏳ 打工中（監工派出，收益×0.6）" : "⏳ 打工中";
        int remain = (int)(pet.work_end - now);
        int h = remain/3600, m2 = (remain%3600)/60, s2 = remain%60;
        char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m2, s2);
        struct tm end_tm{}; time_t work_end_t = pet.work_end; localtime_s(&end_tm, &work_end_t);
        char end_buf[32]; snprintf(end_buf, sizeof(end_buf), "%02d/%02d %02d:%02d",
            end_tm.tm_mon+1, end_tm.tm_mday, end_tm.tm_hour, end_tm.tm_min);
        content += "💼 **打工狀態** " + status_label + "，剩餘 " + std::string(buf) + "\n";
        content += "📅 完成時間：" + std::string(end_buf) + "\n";
        make_container(content);
        dpp::component row1; row1.set_type(dpp::cot_action_row);
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🔄 刷新").set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary));
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("❌ 取消打工").set_id("pet_cancel_work_" + uid_s).set_style(dpp::cos_danger));
        msg.add_component_v2(row1);
        add_lobby_row();
        add_utility_row();
        return msg;
    } else if (work_done) {
        content += "💼 **打工狀態** ✅ 打工完成！按下按鈕領取獎勵\n";
        make_container(content);
        dpp::component row1; row1.set_type(dpp::cot_action_row);
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("💰 領取打工獎勵").set_id("pet_claim_" + uid_s).set_style(dpp::cos_success));
        msg.add_component_v2(row1);
        add_lobby_row();
        add_utility_row();
        return msg;
    } else {
        content += "💼 **打工狀態** 😴 閒置\n";
        make_container(content);
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
        msg.add_component_v2(row1);
        add_lobby_row();
        add_utility_row();
        return msg;
    }
}

// ─── Refine star (stage 3 only) ───────────────────────────────────────────────

static dpp::message handle_pet_refine_star(dpp::snowflake uid) {
    std::string uid_s = std::to_string((uint64_t)uid);
    auto v2msg = [](uint32_t r, uint32_t g, uint32_t b, const std::string& text) {
        dpp::message m; m.set_flags(dpp::m_using_components_v2);
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb((int)r, (int)g, (int)b));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text));
        m.add_component_v2(ct); return m;
    };
    bool success = false;
    int new_exp = 0, star_count = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto pi = pet_data.find(uid);
        if (pi == pet_data.end() || pi->second.stage != 3)
            return v2msg(0xE7, 0x4C, 0x3C, "## ❌ 無法提煉\n需要三階段寵物才能提煉！");
        auto& pet = pi->second;
        if (pet.exp < 50)
            return v2msg(0xE7, 0x4C, 0x3C, "## ❌ 經驗值不足\n需要 **50** 經驗值才能提煉，目前只有 **" + std::to_string(pet.exp) + "**！");
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
    std::string content;
    uint32_t r, g, b;
    if (success) {
        r = 0xF1; g = 0xC4; b = 0x0F;
        content = "## ✨ 提煉成功！\n消耗 **50** 經驗值，成功提煉出一顆 ⭐ **未知的星星**！\n"
                  "不知道有什麼用的星星，可能在未來某一天會用到。\n\n"
                  "目前持有：**" + std::to_string(star_count) + "** 顆星星\n"
                  "剩餘經驗值：**" + std::to_string(new_exp) + "**";
    } else {
        r = 0x95; g = 0xA5; b = 0xA6;
        content = "## 💨 提煉失敗\n消耗 **50** 經驗值，但這次提煉失敗了...\n"
                  "剩餘經驗值：**" + std::to_string(new_exp) + "**";
    }
    dpp::message m; m.set_flags(dpp::m_using_components_v2);
    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb((int)r, (int)g, (int)b));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));
    m.add_component_v2(ct);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🏠 大廳")
        .set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    m.add_component_v2(row);
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

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    if (eq_entries.empty()) {
        dpp::component container;
        container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE6, 0x7E, 0x22));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## ⚔️ 背包 — 裝備\n還沒有任何裝備！\n使用 `!轉蛋` 來抽取裝備。"));
        msg.add_component_v2(container);
        add_bag_home_button(msg, uid);
        dpp::component nav; nav.set_type(dpp::cot_action_row);
        nav.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(nav);
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

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE6, 0x7E, 0x22));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚔️ 背包 — 裝備\n" + desc + "\n-# ✅ = 已裝備（批量售出會跳過已裝備的）"));
    msg.add_component_v2(container);

    add_bag_home_button(msg, uid);

    // Nav row — sell page
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    nav_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 售出").set_id("bag_sell_page_equip_" + uid_s).set_style(dpp::cos_danger));
    nav_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav_row);
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

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    if (eq_entries.empty()) {
        dpp::component container;
        container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 💰 售出裝備\n沒有可售出的裝備。"));
        msg.add_component_v2(container);
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

        dpp::component container;
        container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 💰 售出裝備\n點擊按鈕售出裝備（已裝備中的會跳過）。\n**回收價：** C=1碼 R=5碼 SR=10碼 UR=100碼\n\n-# ✅ = 已裝備，無法售出"));
        msg.add_component_v2(container);

        // Individual sell buttons (rows, max 15)
        dpp::component cur_row; cur_row.set_type(dpp::cot_action_row);
        int n = 0;
        for (auto& en : eq_entries) {
            if (n >= 15) break;
            auto* gi = find_gacha_item(en.key);
            if (!gi) continue;
            if (n > 0 && n % 5 == 0) {
                msg.add_component_v2(cur_row);
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
        if (n > 0) msg.add_component_v2(cur_row);

        // Bulk sell row
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
        msg.add_component_v2(bulk_row);
    }

    // Nav row
    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回裝備背包").set_id("bag_tab_equip_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

// ─── Backpack — tab classification ────────────────────────────────────────────
// 「其他」分頁：不透過此畫面直接使用的道具（狩獵卷、寶珠碎片、票券類等）。
// 蒐藏品（collectible）另外顯示在「收藏」分頁，這裡跟「消耗」都不列。
static bool bag_item_is_other(const VirtualShopItem& vi) {
    if (vi.category == "hunt")  return true;
    if (vi.category == "shard") return true;
    if (vi.category == "special" && vi.key != "orb_ticket") return true;
    return false;
}

// ─── Backpack — Consumables tab ───────────────────────────────────────────────

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
    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    // Gather virtual items (excluding eggs / collectibles / "其他" 分頁的道具)
    struct ItemEntry { std::string key; int count; };
    std::vector<ItemEntry> entries;
    for (auto& vi : VIRTUAL_ITEMS) {
        if (vi.category == "egg" || vi.category == "collectible") continue;
        if (bag_item_is_other(vi)) continue;
        auto it = inv.find(vi.key);
        if (it != inv.end() && it->second > 0)
            entries.push_back({vi.key, it->second});
    }

    if (entries.empty()) {
        dpp::component container;
        container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🎒 背包 — 消耗\n消耗欄是空的！\n前往 **商店 → 虛擬商店** 購買道具。"));
        msg.add_component_v2(container);
        add_bag_home_button(msg, uid);
        dpp::component nav; nav.set_type(dpp::cot_action_row);
        nav.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(nav);
        return msg;
    }

    std::string desc;
    for (auto& en : entries) {
        auto* vi = find_virtual_item(en.key);
        if (!vi) continue;
        std::string id_str = vi->item_id ? ("`" + std::to_string(vi->item_id) + "`  ") : "";
        desc += id_str + "**" + vi->name + "** ×" + std::to_string(en.count) + "　" + vi->desc + "\n";
    }

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🎒 背包 — 消耗\n" + desc));
    msg.add_component_v2(container);
    add_bag_home_button(msg, uid);

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
            if (pet.stage == 0) return true;
            bool slot1_has   = !pet.talent.empty();
            bool slot2_avail = pet.talent2_unlocked;
            bool slot2_has   = slot2_avail && !pet.talent2.empty();
            if (key == "talent2_unlock") return pet.talent2_unlocked;
            if (key == "talent_reroll")  return !(slot1_has || slot2_has);
            if (key == "talent_scroll")  return false;
            return !(!slot1_has || (slot2_avail && !slot2_has)); // talent_class：需要至少一個空欄位
        }
        if (vi->category == "collectible") return true; // 蒐藏品不可使用，僅展示
        if (key == "path_reincarnate") return pet.stage == 0;
        if (key == "path_rebirth")     return pet.stage < 2; // 一階分支尚未分歧，無得選
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
            msg.add_component_v2(cur_row);
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
    if (n > 0) msg.add_component_v2(cur_row);

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
        msg.add_component_v2(pg_row);
    }

    // Nav row — sell page is separate
    dpp::component nav_row; nav_row.set_type(dpp::cot_action_row);
    nav_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 售出").set_id("bag_sell_page_items_" + uid_s).set_style(dpp::cos_danger));
    nav_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    nav_row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗑️ 丟棄道具").set_id("pet_discard_mode_" + uid_s).set_style(dpp::cos_danger));
    msg.add_component_v2(nav_row);
    return msg;
}

// ─── Backpack — Other tab (材料／票券，不從此畫面直接使用) ─────────────────────

static dpp::message make_pet_other_msg(dpp::snowflake uid) {
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
        if (!bag_item_is_other(vi)) continue;
        auto it = inv.find(vi.key);
        if (it != inv.end() && it->second > 0)
            entries.push_back({vi.key, it->second});
    }

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    std::string content = "## 📦 背包 — 其他\n";
    if (entries.empty()) {
        content += "這裡是空的！\n狩獵卷、寶珠碎片等道具會顯示在這裡。";
    } else {
        for (auto& en : entries) {
            auto* vi = find_virtual_item(en.key);
            if (!vi) continue;
            std::string id_str = vi->item_id ? ("`" + std::to_string(vi->item_id) + "`  ") : "";
            content += id_str + "**" + vi->name + "** ×" + std::to_string(en.count) + "　" + vi->desc + "\n";
        }
        content += "\n-# 寶珠碎片請用 !合成 兌換寶珠";
    }

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x7F, 0x8C, 0x8D));
    container.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));
    msg.add_component_v2(container);
    add_bag_home_button(msg, uid);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
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

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    if (entries.empty()) {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 💰 售出道具\n沒有可售出的道具。"));
    } else {
        container.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 💰 售出道具\n售出價格為購買價的 40%（龍族寶箱按原價回收）。狩獵卷不可售出。"));
    }
    msg.add_component_v2(container);

    if (!entries.empty()) {
        // Sell buttons (rows 1-2, max 10)
        dpp::component sell_row; sell_row.set_type(dpp::cot_action_row);
        int sn = 0;
        for (auto& en : entries) {
            if (sn >= 10) break;
            auto* vi = find_virtual_item(en.key);
            if (!vi) continue;
            if (sn > 0 && sn % 5 == 0) {
                msg.add_component_v2(sell_row);
                sell_row = dpp::component(); sell_row.set_type(dpp::cot_action_row);
            }
            int64_t sell_p = vi_sell_price(vi);
            bool can_sell = sell_p > 0;
            dpp::component sbtn;
            sbtn.set_type(dpp::cot_button)
                .set_label(vi->name + (can_sell ? (" +" + std::to_string(sell_p) + "碼") : "（不可售）"))
                .set_id("bag_sell_item_" + uid_s + "_" + en.key)
                .set_style(dpp::cos_danger).set_disabled(!can_sell);
            sell_row.add_component(sbtn); sn++;
        }
        if (sn > 0) msg.add_component_v2(sell_row);
    }

    // Nav row
    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回道具背包").set_id("bag_tab_items_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(nav);
    return msg;
}

// ─── Qty selection page ───────────────────────────────────────────────────────

static dpp::message make_pet_use_qty_msg(dpp::snowflake uid, const std::string& key) {
    int item_count = 0;
    std::string uid_s = std::to_string((uint64_t)uid);
    auto* vi = find_virtual_item(key);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) {
            auto ci = ii->second.find(key);
            if (ci != ii->second.end()) item_count = ci->second;
        }
    }
    dpp::message m; m.set_flags(dpp::m_using_components_v2);
    std::string content = "## 📦 " + (vi ? vi->name : key) + "\n";
    content += "目前持有 **" + std::to_string(item_count) + "** 個，選擇使用數量：";
    if (vi && !vi->desc.empty()) content += "\n\n**道具說明** " + vi->desc;
    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(content));
    m.add_component_v2(ct);
    dpp::component row; row.set_type(dpp::cot_action_row);
    for (int q : {1, 3, 5, 10}) {
        if (q > item_count) break;
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("×" + std::to_string(q))
            .set_id("pet_useqty_" + uid_s + "_" + std::to_string(q) + "_" + key)
            .set_style(q == 1 ? dpp::cos_primary : dpp::cos_success));
    }
    bool all_covered = (item_count == 1 || item_count == 3 || item_count == 5 || item_count == 10);
    if (!all_covered) {
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("全部 ×" + std::to_string(item_count))
            .set_id("pet_useqty_" + uid_s + "_" + std::to_string(item_count) + "_" + key)
            .set_style(dpp::cos_danger));
    }
    m.add_component_v2(row);
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回背包").set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
    m.add_component_v2(row2);
    return m;
}

// ─── Use item handler ─────────────────────────────────────────────────────────

// 天賦道具目前可以套用的欄位（1和/或2）；talent_scroll 可覆蓋，talent_class 需要空欄位，talent_reroll 需要已有天賦的欄位
static std::vector<int> talent_item_valid_slots(const std::string& key, const Pet& pet) {
    std::vector<int> slots;
    bool slot1_has   = !pet.talent.empty();
    bool slot2_avail = pet.talent2_unlocked;
    bool slot2_has   = slot2_avail && !pet.talent2.empty();
    if (key == "talent_scroll") {
        slots.push_back(1);
        if (slot2_avail) slots.push_back(2);
    } else if (key == "talent_class") {
        if (!slot1_has) slots.push_back(1);
        if (slot2_avail && !slot2_has) slots.push_back(2);
    } else if (key == "talent_reroll") {
        if (slot1_has) slots.push_back(1);
        if (slot2_avail && slot2_has) slots.push_back(2);
    }
    return slots;
}

// slot = 0：尚未指定（valid slot 只有一個時自動使用；有兩個時會回傳「請選擇欄位」畫面）
static dpp::message handle_pet_use_item(dpp::snowflake uid, const std::string& key, int qty = 1, int slot = 0) {
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
    dpp::embed e; dpp::message m; m.set_flags(dpp::m_using_components_v2);

    auto err = [&](const std::string& msg_text) {
        dpp::message em; em.set_flags(dpp::m_using_components_v2);
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## ❌ 無法使用\n" + msg_text));
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回")
            .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
        em.add_component_v2(ct); em.add_component_v2(row); return em;
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
        int actual_qty = std::min(qty, item_count);
        std::vector<std::string> got_names;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            for (int i = 0; i < actual_qty; i++) {
                int idx = std::uniform_int_distribution<int>(0, (int)ALL_ORBS2.size()-1)(orb_rng2);
                inventory_data[uid]["orb_ticket"]--;
                inventory_data[uid][ALL_ORBS2[idx].first]++;
                got_names.push_back(ALL_ORBS2[idx].second);
            }
        }
        save_inventory();
        std::string desc = "## 💎 寶珠兌換成功\n";
        for (auto& n : got_names) desc += "💎 獲得了 **✨UR " + n + "**！\n";
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xFF, 0xD7, 0x00));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(desc));
        m.add_component_v2(ct); return m;
    }

    // 特權道具：不需要寵物，提前處理
    if (vi && vi->category == "privilege") {
        if (item_count <= 0) return err("道具數量不足！");
        int actual_qty = std::min(qty, item_count);
        time_t now_p = time(nullptr);
        if (key == "vip_daily") {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key] -= actual_qty;
                auto& cd = chip_data[uid];
                cd.vip_until = std::max(cd.vip_until, now_p) + 86400LL * actual_qty;
            }
            save_inventory(); save_chips();
            e.set_title("👑  尊爵VIP 啟動！").set_color(0xF1C40F);
            std::string vdesc = "✅ 尊爵VIP 已啟動";
            if (actual_qty > 1) vdesc += "（×" + std::to_string(actual_qty) + "）";
            vdesc += "！\n接下來 **" + std::to_string(actual_qty * 24) + " 小時**內，每小時自動為你領取籌碼。";
            e.set_description(vdesc);
        } else if (key == "pet_supervisor_daily") {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key] -= actual_qty;
                auto& cd = chip_data[uid];
                cd.supervisor_until = std::max(cd.supervisor_until, now_p) + 86400LL * actual_qty;
            }
            save_inventory(); save_chips();
            e.set_title("🤖  寵物監工 啟動！").set_color(0x3498DB);
            std::string sdesc = "✅ 寵物監工 已啟動";
            if (actual_qty > 1) sdesc += "（×" + std::to_string(actual_qty) + "）";
            sdesc += "！\n接下來 **" + std::to_string(actual_qty * 24) + " 小時**內，寵物打工結束 **10 分鐘**後若未領取，自動以 **0.6 倍**收益再次出勤。";
            e.set_description(sdesc);
        } else if (key == "pet_insurance") {
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key] -= actual_qty;
                chip_data[uid].insurance_until = std::max(chip_data[uid].insurance_until, now_p) + 3LL * 86400 * actual_qty;
            }
            save_inventory(); save_chips();
            e.set_title("🏥  醫療保險 啟動！").set_color(0x2ECC71);
            std::string idesc = "✅ 醫療保險已生效";
            if (actual_qty > 1) idesc += "（×" + std::to_string(actual_qty) + "）";
            idesc += "！\n接下來 **" + std::to_string(actual_qty * 3) + " 天內**，寵物打工回來若生病（受傷除外），立即給付 **4000** 保險金並結束效果。";
            e.set_description(idesc);
        } else {
            return err("未知的特權道具！");
        }
        {
            uint32_t ecol = e.color.value_or(0xFFFFFF);
        uint8_t cr = (ecol >> 16) & 0xFF, cg = (ecol >> 8) & 0xFF, cb = ecol & 0xFF;
            dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(cr, cg, cb));
            ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
                .set_content("## " + e.title + "\n" + e.description));
            dpp::component row; row.set_type(dpp::cot_action_row);
            row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回")
                .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
            m.add_component_v2(ct); m.add_component_v2(row);
        }
        return m;
    }

    // 龍族寶箱：不需要寵物，開啟後隨機獲得籌碼
    if (vi && (key == "dragon_chest_small" || key == "dragon_chest_mid" || key == "dragon_chest_grand")) {
        if (item_count <= 0) return err("道具數量不足！");
        int lo, hi;
        if (key == "dragon_chest_small")      { lo = 1500;  hi = 4500; }
        else if (key == "dragon_chest_mid")   { lo = 4500;  hi = 7500; }
        else                                  { lo = 10500; hi = 15000; }
        int actual_qty = std::min(qty, item_count);
        static thread_local std::mt19937 chest_rng(std::random_device{}());
        int64_t total = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            inventory_data[uid][key] -= actual_qty;
            for (int i = 0; i < actual_qty; i++)
                total += std::uniform_int_distribution<int>(lo, hi)(chest_rng);
            chip_data[uid].chips += total;
        }
        save_inventory(); save_chips();
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF1, 0xC4, 0x0F));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🐉 開啟" + vi->name + (actual_qty > 1 ? "×" + std::to_string(actual_qty) : "") +
                         "\n獲得 **" + std::to_string(total) + "** 碼！"));
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回")
            .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
        m.add_component_v2(ct); m.add_component_v2(row);
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
        {
            std::string econt = "## 🥚 已放入寵物欄\n**" + (vi ? vi->name : key) + "** 已設為當前寵物蛋！\n使用孵蛋工具來孵化它。";
            dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
            ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(econt));
            dpp::component row; row.set_type(dpp::cot_action_row);
            row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回")
                .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
            m.add_component_v2(ct); m.add_component_v2(row); return m;
        }
    }

    // ── 孵蛋工具 ──────────────────────────────────────────────────────────────
    if (vi->category == "incubator") {
        if (pet.stage != 0) return err("寵物已經孵化了！");
        int pct = 0;
        if (key == "inc_100") pct = 100;
        else if (key == "inc_60") pct = 60;
        else if (key == "inc_30") pct = 30;
        else if (key == "inc_10") pct = 10;
        int used_inc = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            for (int i = 0; i < qty; i++) {
                auto& cnt = inventory_data[uid][key];
                if (cnt <= 0) break;
                success = roll(pct);
                if (consume_item) cnt--;
                used_inc++;
                if (success) { pet_data[uid].stage = 1; pet_data[uid].exp = 0; break; }
            }
        }
        if (success) {
            Pet updated;
            { std::lock_guard<std::mutex> lk(data_mutex); updated = pet_data[uid]; }
            result_desc = "🎉 孵化成功！**" + pet_name(updated.chain, 1) + "** 誕生了！";
            if (used_inc > 1) result_desc += "\n（共嘗試 " + std::to_string(used_inc) + " 次）";
            e.set_title("🎉  孵化成功！").set_color(0x2ECC71);
        } else {
            result_desc = "😢 孵化失敗...蛋還在，可以再試！";
            if (used_inc > 1) result_desc += "\n（共嘗試 " + std::to_string(used_inc) + " 次）";
            e.set_title("😢  孵化失敗").set_color(0xE74C3C);
        }
    }
    // ── 成長工具 ──────────────────────────────────────────────────────────────
    else if (vi->category == "growth") {
        if (pet.stage == 0) return err("蛋還沒孵化！");
        int total_exp = 0; int used_grow = 0; int success_count = 0; bool any_punished = false; int new_exp = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& p = pet_data[uid];
            for (int i = 0; i < qty; i++) {
                auto& cnt = inventory_data[uid][key];
                if (cnt <= 0) break;
                int eg = 0; bool suc = false; bool pun = false;
                if (key == "grow_1") { eg = 10; suc = true; }
                else if (key == "grow_2") { suc = roll(60); if (suc) eg = 15; }
                else if (key == "grow_3") { suc = roll(30); if (suc) eg = 30; }
                else if (key == "grow_4") { suc = roll(10); if (suc) eg = 50; }
                else if (key == "grow_5") { suc = roll(1);  if (suc) eg = 250; }
                else if (key == "grow_6") {
                    int r = std::uniform_int_distribution<int>(1,100)(rng);
                    if (r <= 50)      { eg = 30; suc = true; }
                    else if (r <= 75) { eg = 10; suc = true; }
                    else              { eg = -20; pun = true; }
                }
                if (consume_item) cnt--;
                total_exp += eg;
                used_grow++;
                if (suc) { success_count++; success = true; }
                if (pun) any_punished = true;
            }
            int raw = p.exp + total_exp;
            if (p.stage > 0 && p.stage < 3)
                p.exp = std::min(std::max(0, raw), exp_needed(p.stage));
            else
                p.exp = std::max(0, raw);
            new_exp = p.exp;
        }
        if (used_grow == 0) return err("背包中沒有足夠的道具！");
        bool punished = any_punished && !success;
        if (qty == 1) {
            if (punished)       result_desc = "😰 壓力太大！-20 經驗值";
            else if (!success)  result_desc = "😢 成長失敗...沒有獲得經驗值";
            else                result_desc = "✨ 成長！+" + std::to_string(total_exp) + " 經驗值";
        } else {
            std::string sign = total_exp >= 0 ? "+" : "";
            result_desc = "✨ 共使用 **" + std::to_string(used_grow) + "** 個，成功 **" + std::to_string(success_count) + "** 次\n經驗值 " + sign + std::to_string(total_exp);
        }
        result_desc += "\n目前經驗值：**" + std::to_string(new_exp) + "**";
        if (punished)       e.set_title("😰  成長懲罰").set_color(0xE74C3C);
        else if (success)   e.set_title("✨  成長成功！").set_color(0x2ECC71);
        else                e.set_title("😢  成長失敗").set_color(0xE74C3C);
    }
    // ── 天賦道具 ─────────────────────────────────────────────────────────────
    else if (vi->category == "talent") {
        if (pet.stage == 0) return err("蛋還沒孵化，無法賦予天賦！");

        if (key == "talent2_unlock") {
            if (pet.talent2_unlocked) return err("第二天賦欄位已經解鎖過了！");
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                inventory_data[uid][key]--;
                pet_data[uid].talent2_unlocked = true;
            }
            result_desc = "🔓 第二天賦欄位解鎖！現在可以用天賦道具賦予第二天賦了。";
            e.set_title("🔓  解鎖成功！").set_color(0x3498DB);
            success = true;
        } else {
            std::vector<int> valid_slots = talent_item_valid_slots(key, pet);
            if (valid_slots.empty()) {
                if (key == "talent_reroll") return err("寵物還沒有天賦，請先使用天賦賦予卷軸！");
                return err("已經沒有空的天賦欄位了！如需更換請使用「你不可以學畫畫!」或「天賦賦予卷軸」！");
            }
            // slot 是外部指定時（欄位選擇按鈕點回來），狀態可能在中途變了，重新確認仍然合法
            if (slot != 0 && std::find(valid_slots.begin(), valid_slots.end(), slot) == valid_slots.end())
                return err("這個天賦欄位的狀態已經改變了，請重新操作！");
            if (slot == 0 && valid_slots.size() > 1) {
                // 兩個欄位都可以套用，先讓玩家選欄位
                std::string sdesc = "## 🎯 選擇要套用的天賦欄位\n";
                sdesc += "・天賦一：" + (pet.talent.empty()  ? std::string("無") : pet.talent)  + "\n";
                sdesc += "・天賦二：" + (pet.talent2.empty() ? std::string("無") : pet.talent2) + "\n";
                dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF3, 0x9C, 0x12));
                ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(sdesc));
                dpp::component row; row.set_type(dpp::cot_action_row);
                row.add_component(dpp::component().set_type(dpp::cot_button).set_label("天賦一")
                    .set_id("talent_slot_" + key + "_1_" + std::to_string((uint64_t)uid)).set_style(dpp::cos_primary));
                row.add_component(dpp::component().set_type(dpp::cot_button).set_label("天賦二")
                    .set_id("talent_slot_" + key + "_2_" + std::to_string((uint64_t)uid)).set_style(dpp::cos_primary));
                dpp::message sm; sm.set_flags(dpp::m_using_components_v2);
                sm.add_component_v2(ct); sm.add_component_v2(row);
                return sm;
            }
            int target_slot = (slot != 0) ? slot : valid_slots[0];
            std::string  target_cur   = (target_slot == 1) ? pet.talent  : pet.talent2; // 目前套用中的天賦（讀取快照）
            std::string  other_talent = (target_slot == 1) ? pet.talent2 : pet.talent;  // 另一個欄位的天賦（不可重複）
            std::string  slot_label   = (target_slot == 1) ? "天賦一" : "天賦二";

            if (key == "talent_reroll") {
                std::vector<std::string> others = talent_pool_excluding(target_cur, other_talent);
                int idx = std::uniform_int_distribution<int>(0, (int)others.size()-1)(rng);
                std::string new_talent = others[idx];
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    inventory_data[uid][key]--; // 天然呆不影響天賦道具
                    if (target_slot == 1) pet_data[uid].talent  = new_talent;
                    else                  pet_data[uid].talent2 = new_talent;
                }
                result_desc = "🎨 " + slot_label + "改變！**" + target_cur + "** → **" + new_talent + "**\n" + talent_effect_desc(new_talent);
                e.set_title("🎨  天賦重置！").set_color(0x3498DB);
                success = true;
            } else if (key == "talent_scroll") {
                std::vector<std::string> pool = talent_pool_excluding(other_talent);
                std::string sdesc = "## ✨ 選擇" + slot_label + "\n";
                if (!target_cur.empty())
                    sdesc += "目前" + slot_label + "：**" + target_cur + "**\n選擇後將覆蓋現有天賦：\n\n";
                else
                    sdesc += "請選擇要賦予 **" + pet_name(pet.chain, pet.stage, pet.variant) + "** 的" + slot_label + "：\n\n";
                if (!other_talent.empty()) sdesc += "-# 已排除跟另一欄位相同的「" + other_talent + "」\n\n";
                for (auto& t : pool) sdesc += "・**" + t + "** — " + talent_effect_desc(t) + "\n";
                dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xF3, 0x9C, 0x12));
                ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(sdesc));
                dpp::message sm; sm.set_flags(dpp::m_using_components_v2);
                sm.add_component_v2(ct);
                dpp::component row; row.set_type(dpp::cot_action_row);
                int n = 0;
                for (auto& t : pool) {
                    if (n > 0 && n % 5 == 0) { sm.add_component_v2(row); row = dpp::component(); row.set_type(dpp::cot_action_row); }
                    row.add_component(dpp::component().set_type(dpp::cot_button).set_label(t)
                        .set_id("talent_pick_" + std::to_string(target_slot) + "_" + t + "_" + std::to_string((uint64_t)uid))
                        .set_style(dpp::cos_primary));
                    n++;
                }
                if (n > 0) sm.add_component_v2(row);
                return sm;
            } else {
                // talent_class: 25% random（只會在空欄位發生，且排除另一欄位的天賦）
                success = roll(25);
                std::string new_talent;
                {
                    std::lock_guard<std::mutex> lk(data_mutex);
                    inventory_data[uid][key]--; // 天然呆不影響天賦道具
                    if (success) {
                        std::vector<std::string> pool = talent_pool_excluding(other_talent);
                        int idx = std::uniform_int_distribution<int>(0, (int)pool.size()-1)(rng);
                        new_talent = pool[idx];
                        if (target_slot == 1) pet_data[uid].talent  = new_talent;
                        else                  pet_data[uid].talent2 = new_talent;
                    }
                }
                if (success) {
                    result_desc = "🌟 " + slot_label + "發現天賦！**" + new_talent + "**\n" + talent_effect_desc(new_talent);
                    e.set_title("🌟  天賦覺醒！").set_color(0xF39C12);
                } else {
                    result_desc = "😢 這次沒有發現天賦...可以再試！";
                    e.set_title("😢  未發現天賦").set_color(0xE74C3C);
                }
            }
        }
    }
    // ── 成長路徑道具（path）── 不可直接使用，提示 ────────────────────────────
    else if (vi->category == "recovery") {
        if (key == "recover_fatigue_cursed")
            return err("這杯被詛咒了！領取者本人無法使用，交易給別人後對方就能正常使用。");
        static const std::map<std::string,std::string> ITEM_STATUS = {
            {"recover_depress","憂鬱"}, {"recover_injury","受傷"},
            {"recover_muscle","肌肉緊繃"}, {"recover_fatigue","疲勞"},
        };
        if (!ITEM_STATUS.count(key)) return err("無效的恢復道具！");
        std::string target_status = ITEM_STATUS.at(key);
        int used_rec = 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& p = pet_data[uid];
            auto& ss = p.statuses;
            for (int i = 0; i < qty; i++) {
                auto& cnt = inventory_data[uid][key];
                if (cnt <= 0) break;
                auto it2 = std::find(ss.begin(), ss.end(), target_status);
                if (it2 == ss.end()) break;
                ss.erase(it2);
                if (consume_item) cnt--;
                used_rec++;
            }
        }
        if (used_rec == 0) return err("你的寵物目前沒有「" + target_status + "」狀態！");
        save_pet_data(); save_inventory();
        {
            std::string rdesc = "## 💊 恢復成功！\n解除了「**" + target_status + "**」狀態！";
            if (used_rec > 1) rdesc += "\n（共使用了 " + std::to_string(used_rec) + " 個）";
            dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
            ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(rdesc));
            dpp::component row; row.set_type(dpp::cot_action_row);
            row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🐾 查看寵物")
                .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_primary));
            m.add_component_v2(ct); m.add_component_v2(row); return m;
        }
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
    {
        uint32_t ecol = e.color.value_or(0xFFFFFF);
        uint8_t cr = (ecol >> 16) & 0xFF, cg = (ecol >> 8) & 0xFF, cb = ecol & 0xFF;
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(cr, cg, cb));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## " + e.title + "\n" + result_desc));
        m.add_component_v2(ct);
    }
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🎒 返回背包")
        .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🐾 查看寵物")
        .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary));
    m.add_component_v2(row);
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
    dpp::message msg; msg.set_flags(dpp::m_using_components_v2);

    if (!has_pet) {
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE6, 0x7E, 0x22));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🗑️ 選擇要丟棄的道具\n你還沒有寵物！"));
        msg.add_component_v2(ct); return msg;
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
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE6, 0x7E, 0x22));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## 🗑️ 選擇要丟棄的道具\n背包是空的，沒有可以丟棄的道具。"));
        dpp::component row; row.set_type(dpp::cot_action_row);
        row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回背包")
            .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component_v2(ct); msg.add_component_v2(row);
        return msg;
    }

    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE6, 0x7E, 0x22));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🗑️ 選擇要丟棄的道具\n點選道具丟棄 **1 個**，操作前會再次確認。"));
    msg.add_component_v2(ct);

    dpp::component cur_row; cur_row.set_type(dpp::cot_action_row);
    int n = 0;
    for (auto& en : entries) {
        if (n >= 20) break;
        auto* vi = find_virtual_item(en.key);
        if (!vi) continue;
        if (n > 0 && n % 5 == 0) {
            msg.add_component_v2(cur_row);
            cur_row = dpp::component(); cur_row.set_type(dpp::cot_action_row);
        }
        dpp::component btn;
        btn.set_type(dpp::cot_button)
           .set_label("🗑️ " + vi->name + " ×" + std::to_string(en.count))
           .set_id("pet_discard_confirm_" + uid_s + "_" + en.key)
           .set_style(dpp::cos_danger);
        cur_row.add_component(btn); n++;
    }
    if (n > 0) msg.add_component_v2(cur_row);

    dpp::component back_row; back_row.set_type(dpp::cot_action_row);
    back_row.add_component(dpp::component().set_type(dpp::cot_button).set_label("↩ 返回背包")
        .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component_v2(back_row);
    return msg;
}

// ─── Discard: confirmation view ───────────────────────────────────────────────

static dpp::message make_pet_discard_confirm_msg(dpp::snowflake uid, const std::string& key) {
    std::string uid_s = std::to_string((uint64_t)uid);
    const VirtualShopItem* vi = find_virtual_item(key);
    dpp::message msg; msg.set_flags(dpp::m_using_components_v2);
    if (!vi) {
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## ⚠️ 確認丟棄\n道具不存在！"));
        msg.add_component_v2(ct); return msg;
    }
    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## ⚠️ 確認丟棄\n確定要丟棄 **1 個** **" + vi->name + "** 嗎？\n此操作無法復原！"));
    msg.add_component_v2(ct);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component yes, no;
    yes.set_type(dpp::cot_button).set_label("✅ 確認丟棄")
       .set_id("pet_discard_do_" + uid_s + "_" + key).set_style(dpp::cos_danger);
    no.set_type(dpp::cot_button).set_label("❌ 取消")
       .set_id("pet_discard_mode_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(yes); row.add_component(no);
    msg.add_component_v2(row);
    return msg;
}

// ─── Discard: execute ─────────────────────────────────────────────────────────

static dpp::message handle_pet_discard_item(dpp::snowflake uid, const std::string& key) {
    std::string uid_s = std::to_string((uint64_t)uid);
    const VirtualShopItem* vi = find_virtual_item(key);
    dpp::message msg; msg.set_flags(dpp::m_using_components_v2);
    if (!vi) {
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## ❌ 錯誤\n道具不存在！"));
        msg.add_component_v2(ct); return msg;
    }
    bool ok = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& inv = inventory_data[uid];
        if (inv.count(key) && inv[key] > 0) { inv[key]--; ok = true; }
    }
    if (!ok) {
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
            .set_content("## ❌ 數量不足\n你沒有 **" + vi->name + "**！"));
        msg.add_component_v2(ct); return msg;
    }
    save_inventory();
    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x95, 0xA5, 0xA6));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🗑️ 已丟棄\n丟棄了 **1 個** **" + vi->name + "**。"));
    msg.add_component_v2(ct);
    dpp::component row; row.set_type(dpp::cot_action_row);
    dpp::component back_bag, back_pet;
    back_bag.set_type(dpp::cot_button).set_label("🎒 返回背包")
            .set_id("pet_open_use_" + uid_s).set_style(dpp::cos_secondary);
    back_pet.set_type(dpp::cot_button).set_label("🐾 查看寵物")
            .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_secondary);
    row.add_component(back_bag); row.add_component(back_pet);
    msg.add_component_v2(row);
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
    auto v2err = [](const std::string& txt) {
        dpp::message m; m.set_flags(dpp::m_using_components_v2);
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(txt));
        m.add_component_v2(ct); return m;
    };
    if (!has_pet || pet.stage == 0)
        return v2err("## ❌ 無法打工\n你沒有可以打工的寵物！");
    // 探險中（或探險完成未收取）寵物不可打工
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto ai = adv_games.find(uid);
        if (ai != adv_games.end() && ai->second.pet_along)
            return v2err("## ❌ 寵物探險中\n寵物正在探險，請先收取探險結果後才能打工！");
    }
    if (pet.work_task > 0)
        return v2err("## ❌ 已在打工\n你的寵物已經在打工中！");

    // Sneaky的自願留營表：固定24小時，跳過所有時長加成/懲罰，不計入監工資格
    if (task == 24) {
        bool has_ticket = false;
        { std::lock_guard<std::mutex> lk(data_mutex);
          auto ii = inventory_data.find(uid);
          if (ii != inventory_data.end() && ii->second.count("col_rd_campticket") && ii->second.at("col_rd_campticket") > 0)
              has_ticket = true;
        }
        if (!has_ticket) return v2err("## ❌ 無法選擇\n你沒有「Sneaky的自願留營表」！");
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            pet_data[uid].work_task          = 24;
            pet_data[uid].work_end           = time(nullptr) + 86400;
            pet_data[uid].work_notified      = false;
            pet_data[uid].is_supervisor_work = false;
        }
        save_pet_data();
        return make_pet_view_msg(uid);
    }

    int dur = (task == 1) ? 3600 : (task == 4) ? 14400 : 28800;
    // 打工時長乘區：加總後一次套用（無條件進位），不逐一連乘。
    // 天賦：迅捷 -10%
    double dur_delta = (pet.talent == "迅捷") ? -0.10 : 0.0;
    // 收藏品：考拉の親筆簽名 -3%；菇菇王國高級套組 -1%
    { std::lock_guard<std::mutex> lk(data_mutex);
      auto wi = inventory_data.find(uid);
      if (wi != inventory_data.end()) {
          auto jt = wi->second.find("col_koala_autograph");
          if (jt != wi->second.end() && jt->second > 0) dur_delta += -0.03;
      }
      if (col_set_mushroom_adv(uid)) dur_delta += -0.01;
    }
    // 負面狀態：疲勞 +30%
    for (auto& s : pet.statuses) if (s == "疲勞") { dur_delta += 0.30; break; }
    dur = (int)std::ceil(dur * std::max(0.0, 1.0 + dur_delta));
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
    dpp::message m; m.set_flags(dpp::m_using_components_v2);
    auto v2e = [](const std::string& txt) {
        dpp::message em; em.set_flags(dpp::m_using_components_v2);
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(txt));
        em.add_component_v2(ct); return em;
    };
    if (!has_pet || pet.stage == 0 || pet.work_task == 0)
        return v2e("## ❌ 沒有可領取的獎勵");
    if (pet.work_end > time(nullptr))
        return v2e("## ❌ 打工尚未完成\n請稍後再試！");

    // Sneaky的自願留營表：固定 12000碼／20exp／必定憂鬱，不觸發醫療保險與洗澡卡迴避
    if (pet.work_task == 24) {
        add_chips(uid, 12000);
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto& p = pet_data[uid];
            if (p.stage > 0) {
                if (p.stage < 3) p.exp = std::min(p.exp + 20, exp_needed(p.stage));
                else              p.exp += 20;
            }
            p.work_task = 0; p.work_end = 0; p.is_supervisor_work = false; p.work_notified = false;
            bool already = false;
            for (auto& s : p.statuses) if (s == "憂鬱") { already = true; break; }
            if (!already) p.statuses.push_back("憂鬱");
        }
        save_pet_data();
        std::string uid_s2 = std::to_string((uint64_t)uid);
        std::string pet_disp2 = pet_name(pet.chain, pet.stage, pet.variant);
        if (!pet.talent.empty()) pet_disp2 += " ✦" + pet.talent;
        if (pet.talent2_unlocked && !pet.talent2.empty()) pet_disp2 += " ✦" + pet.talent2;
        std::string wcontent2 = "## 🏕️ 留營結束！\n**" + pet_disp2 + "** 留營回來了！\n\n";
        wcontent2 += "💰 **獎勵** +12000 碼\n✨ **經驗** +20 exp\n💼 **餘額** " + std::to_string(get_chips(uid)) + " 碼";
        wcontent2 += "\n⚠️ **新增狀態** 「**憂鬱**」（自願留營表：必定觸發，不受洗澡卡/保險影響）";
        dpp::component ct2; ct2.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
        ct2.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(wcontent2));
        m.add_component_v2(ct2);
        dpp::component row2; row2.set_type(dpp::cot_action_row);
        row2.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🐾 返回").set_id("pet_refresh_" + uid_s2).set_style(dpp::cos_secondary));
        m.add_component_v2(row2);
        return m;
    }

    auto opts = work_options(pet.stage);
    int idx = (pet.work_task == 1) ? 0 : (pet.work_task == 4) ? 1 : 2;
    int64_t reward   = opts[idx].pay;
    int     exp_gain = opts[idx].exp_gain;

    // 打工報酬乘區：所有百分比加成/懲罰先加總，最後一次套用乘法（無條件進位），
    // 不逐一連乘取整，避免多個加成疊加時互相侵蝕或誤差累積。
    // 監工派出：-40%
    bool is_supervisor = pet.is_supervisor_work;
    double reward_delta = is_supervisor ? -0.40 : 0.0;

    static thread_local std::mt19937 claim_rng(std::random_device{}());
    auto roll_pct = [&](int pct) { return std::uniform_int_distribution<int>(1,1000)(claim_rng) <= pct*10; };

    // 負面狀態：受傷 -10%／憂鬱 -20%
    bool status_injured = false, status_depress = false;
    for (auto& s : pet.statuses) {
        if (s == "受傷")  status_injured  = true;
        if (s == "憂鬱")  status_depress  = true;
    }
    if (status_injured) reward_delta += -0.10;
    if (status_depress) reward_delta += -0.20;

    // 收藏品皮包加成 + 亡魂墓地高級套組 +1%
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto wi = inventory_data.find(uid);
        if (wi != inventory_data.end()) {
            if (wi->second.count("col_slim_wallet") && wi->second.at("col_slim_wallet") > 0)
                reward_delta += 0.03;
            if (wi->second.count("col_fat_wallet") && wi->second.at("col_fat_wallet") > 0)
                reward_delta += 0.07;
        }
        if (col_set_ghost_adv(uid)) reward_delta += 0.01;
    }
    // 天賦：招人喜歡 +10%
    if (pet.talent == "招人喜歡") reward_delta += 0.10;

    reward = (int64_t)std::ceil(reward * std::max(0.0, 1.0 + reward_delta));

    // 天賦：幸運 — 5% 雙倍報酬（機率事件，不併入乘區，套用在乘區結果之後）
    bool doubled_lucky = false;
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

    // 收藏品：華瑄の洗澡卡 — 每週可迴避 3 次打工負面狀態
    bool bath_blocked = false;
    if (!new_neg_status.empty()) {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto wi = inventory_data.find(uid);
        if (wi != inventory_data.end()) {
            auto jt = wi->second.find("col_bath_huaxuan");
            if (jt != wi->second.end() && jt->second > 0) {
                // 計算本週的 key（YYYYWW 格式）
                time_t now_t = time(nullptr); struct tm tmw = {}; localtime_s(&tmw, &now_t);
                char wbuf[8]; strftime(wbuf, sizeof(wbuf), "%G%V", &tmw);
                int cur_week = std::stoi(wbuf);
                int& stored_week = wi->second["_bath_week"];
                int& stored_uses = wi->second["_bath_uses"];
                if (stored_week != cur_week) { stored_week = cur_week; stored_uses = 3; }
                if (stored_uses > 0) { stored_uses--; bath_blocked = true; new_neg_status.clear(); }
            }
        }
    }

    // 醫療保險：打工回來生病時觸發（受傷不算，但工作負面狀態都不含受傷）
    int64_t insurance_payout = 0;
    bool supervisor_redispatched = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& p = pet_data[uid];
        if (p.stage > 0) {
            if (p.stage < 3)
                p.exp = std::min(p.exp + exp_gain, exp_needed(p.stage));
            else
                p.exp += exp_gain; // stage 3: no cap
        }
        int prev_task        = p.work_task;
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
        // 手動領取後，若監工仍有效，立即以同任務重新派出（收益仍 ×0.6）
        if (prev_task != 0 && prev_task != 24 && cd.supervisor_until > time(nullptr)) {
            int dur_sec = prev_task * 3600;
            if (p.talent == "迅捷" || (p.talent2_unlocked && p.talent2 == "迅捷"))
                dur_sec = (int)(dur_sec * 0.9);
            for (auto& s : p.statuses) if (s == "疲勞") { dur_sec = (int)(dur_sec * 1.3); break; }
            p.work_task          = prev_task;
            p.work_end           = time(nullptr) + dur_sec;
            p.is_supervisor_work = true;
            supervisor_redispatched = true;
        }
    }
    save_pet_data();
    if (insurance_payout > 0) save_chips();

    std::string uid_s = std::to_string((uint64_t)uid);
    std::string pet_disp = pet_name(pet.chain, pet.stage, pet.variant);
    if (!pet.talent.empty()) pet_disp += " ✦" + pet.talent;
    if (pet.talent2_unlocked && !pet.talent2.empty()) pet_disp += " ✦" + pet.talent2;
    std::string reward_str = "+" + std::to_string(reward) + " 碼";
    if (is_supervisor)   reward_str += " 🤖（監工派出 ×0.6）";
    if (doubled_lucky)   reward_str += " 🍀（幸運雙倍！）";
    if (status_injured)  reward_str += " ⚠️（受傷 -10%）";
    if (status_depress)  reward_str += " ⚠️（憂鬱 -20%）";
    if (depress_spend)   reward_str += " 💸（憂鬱花錢了）";
    std::string wcontent = "## 💰 打工完成！\n**" + pet_disp + "** 打工回來了！\n\n";
    wcontent += "💰 **獎勵** " + reward_str + "\n";
    wcontent += "✨ **經驗** +" + std::to_string(exp_gain) + " exp\n";
    wcontent += "💼 **餘額** " + std::to_string(get_chips(uid)) + " 碼";
    if (dream_triggered)         wcontent += "\n🌙 **喜歡作夢** 🎆 籌碼翻倍！！";
    if (!new_neg_status.empty()) wcontent += "\n⚠️ **新增狀態** 「**" + new_neg_status + "**」";
    if (insurance_payout > 0)      wcontent += "\n🏥 **醫療保險** +4000 碼 保險金理賠！效果已結束。";
    if (bath_blocked)              wcontent += "\n🛁 **華瑄的洗澡卡** ✨ 負面狀態已被迴避！（本週剩餘次數請查收藏）";
    if (supervisor_redispatched)   wcontent += "\n🤖 **監工** 已立即重新派出！（收益 ×0.6）";
    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2E, 0xCC, 0x71));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(wcontent));
    m.add_component_v2(ct);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🐾 查看寵物")
        .set_id("pet_refresh_" + uid_s).set_style(dpp::cos_primary));
    m.add_component_v2(row);
    return m;
}

// ─── Cancel work ─────────────────────────────────────────────────────────────

static dpp::message handle_pet_cancel_work(dpp::snowflake uid) {
    auto v2msg = [](uint32_t r, uint32_t g, uint32_t b, const std::string& text) {
        dpp::message m; m.set_flags(dpp::m_using_components_v2);
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb((int)r, (int)g, (int)b));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text));
        m.add_component_v2(ct); return m;
    };
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it == pet_data.end() || it->second.work_task == 0)
            return v2msg(0xE7, 0x4C, 0x3C, "## ❌ 沒有進行中的打工");
        auto& p = it->second;
        p.work_task = 0; p.work_end = 0; p.is_supervisor_work = false; p.work_notified = false;
    }
    save_pet_data();
    dpp::message m = v2msg(0x95, 0xA5, 0xA6, "## ✅ 已取消打工\n寵物已立即返回，本次打工無獎勵。");
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🐾 查看寵物")
        .set_id("pet_refresh_" + std::to_string((uint64_t)uid)).set_style(dpp::cos_primary));
    m.add_component_v2(row);
    return m;
}

// ─── Start onsen ──────────────────────────────────────────────────────────────

static dpp::message handle_pet_start_onsen(dpp::snowflake uid) {
    auto v2e = [](const std::string& text) {
        dpp::message m; m.set_flags(dpp::m_using_components_v2);
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0xE7, 0x4C, 0x3C));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text));
        m.add_component_v2(ct); return m;
    };
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it == pet_data.end() || it->second.stage == 0)
            return v2e("## ❌ 沒有寵物");
        auto& p = it->second;
        bool has_zoey_rod = false;
        { auto wi = inventory_data.find(uid); if (wi != inventory_data.end()) { auto ji = wi->second.find("col_rod_zoey"); if (ji != wi->second.end() && ji->second > 0) has_zoey_rod = true; } }
        if (p.statuses.empty() && !has_zoey_rod)
            return v2e("## ❌ 寵物沒有負面狀態\n泡溫泉的用途是清除負面狀態，但你的寵物目前狀態很好！");
        if (p.work_task > 0 && p.work_end > time(nullptr))
            return v2e("## ❌ 打工中無法泡溫泉\n寵物正在打工，請先取消或等打工結束。");
        int64_t onsen_secs = 7200LL;
        if (col_set_water_adv(uid)) onsen_secs = (int64_t)std::ceil(7200.0 * 0.95);
        p.onsen_end      = time(nullptr) + onsen_secs;
        p.onsen_notified = false;
        p.work_task = 0; p.work_end = 0; p.is_supervisor_work = false;
    }
    save_pet_data();
    dpp::message m; m.set_flags(dpp::m_using_components_v2);
    dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x34, 0x98, 0xDB));
    ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display)
        .set_content("## 🛀 已開始泡溫泉！\n寵物將在 **2 小時**後療癒完畢，自動清除所有負面狀態。"));
    m.add_component_v2(ct);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🐾 查看寵物")
        .set_id("pet_refresh_" + std::to_string((uint64_t)uid)).set_style(dpp::cos_primary));
    m.add_component_v2(row);
    return m;
}

// ─── Cancel onsen ─────────────────────────────────────────────────────────────

static dpp::message handle_pet_cancel_onsen(dpp::snowflake uid) {
    auto v2msg = [](uint32_t r, uint32_t g, uint32_t b, const std::string& text) {
        dpp::message m; m.set_flags(dpp::m_using_components_v2);
        dpp::component ct; ct.set_type(dpp::cot_container).set_accent(dpp::utility::rgb((int)r, (int)g, (int)b));
        ct.add_component_v2(dpp::component().set_type(dpp::cot_text_display).set_content(text));
        m.add_component_v2(ct); return m;
    };
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it == pet_data.end() || it->second.onsen_end == 0)
            return v2msg(0xE7, 0x4C, 0x3C, "## ❌ 沒有進行中的溫泉");
        it->second.onsen_end      = 0;
        it->second.onsen_notified = false;
    }
    save_pet_data();
    dpp::message m = v2msg(0x95, 0xA5, 0xA6, "## ✅ 已取消泡溫泉\n寵物已立即離開溫泉，負面狀態未清除。");
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("🐾 查看寵物")
        .set_id("pet_refresh_" + std::to_string((uint64_t)uid)).set_style(dpp::cos_primary));
    m.add_component_v2(row);
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

