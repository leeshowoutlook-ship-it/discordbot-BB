#pragma once
#include "pet.h"
#include <random>
#include <array>

// ─── 強化系統：攻擊力／防禦力／生命值，各自 0~10 層 ─────────────────────────────
// 攻擊力／生命值：每層 +1%（乘區，見 calc_pet_stats）
// 防禦力：每兩層 +1（見 calc_pet_stats）

static const int ENH_MAX_LEVEL = 10;

struct EnhTier { int stars; int64_t chips; int rate_pct; };
// index 0 = 第1層所需, index 9 = 第10層所需
static const std::array<EnhTier, ENH_MAX_LEVEL> ENH_TABLE = {{
    {0, 10000,  100},
    {1, 20000,  95},
    {1, 30000,  90},
    {2, 40000,  80},
    {2, 50000,  70},
    {3, 60000,  60},
    {3, 70000,  50},
    {4, 80000,  40},
    {4, 90000,  30},
    {5, 100000, 20},
}};

static std::mt19937& enh_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

static int enh_level_of(const Pet& pet, const std::string& stat) {
    if (stat == "atk") return pet.enh_atk;
    if (stat == "def") return pet.enh_def;
    return pet.enh_hp;
}
static std::string enh_stat_label(const std::string& stat) {
    if (stat == "atk") return "⚔️ 攻擊力";
    if (stat == "def") return "🛡️ 防禦力";
    return "❤️ 生命值";
}
static std::string enh_stat_effect(const std::string& stat, int level) {
    if (stat == "def") return "+" + std::to_string(level / 2) + " 防禦";
    return "+" + std::to_string(level) + "%";
}

// ─── 主頁 ──────────────────────────────────────────────────────────────────────

static dpp::message make_enhance_main_msg(dpp::snowflake uid,
                                           const std::string& dn = "",
                                           const std::string& av = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    Pet pet; bool has_pet = false;
    int stars = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end() && it->second.stage > 0) { pet = it->second; has_pet = true; }
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) {
            auto sit = ii->second.find("star_unknown");
            if (sit != ii->second.end()) stars = sit->second;
        }
    }

    dpp::embed e; e.set_title("💪  寵物強化").set_color(0xE74C3C);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }

    dpp::message msg;
    if (!has_pet) {
        e.set_description("❌ 需要已進化的寵物才能強化！");
        msg.add_embed(e);
        dpp::component nav; nav.set_type(dpp::cot_action_row);
        nav.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component(nav);
        return msg;
    }

    std::string desc = "消耗 **未知的星星** 與籌碼，有機率強化成功（失敗不退還素材）。\n";
    desc += "⭐ 目前持有：**" + std::to_string(stars) + "** 顆\n\n";
    desc += "⚔️ 攻擊力：Lv **" + std::to_string(pet.enh_atk) + "/" + std::to_string(ENH_MAX_LEVEL) + "**（" + enh_stat_effect("atk", pet.enh_atk) + "）\n";
    desc += "🛡️ 防禦力：Lv **" + std::to_string(pet.enh_def) + "/" + std::to_string(ENH_MAX_LEVEL) + "**（" + enh_stat_effect("def", pet.enh_def) + "）\n";
    desc += "❤️ 生命值：Lv **" + std::to_string(pet.enh_hp)  + "/" + std::to_string(ENH_MAX_LEVEL) + "**（" + enh_stat_effect("hp",  pet.enh_hp)  + "）\n";
    e.set_description(desc);
    msg.add_embed(e);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⚔️ 攻擊力").set_id("enh_pick_" + uid_s + "_atk").set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🛡️ 防禦力").set_id("enh_pick_" + uid_s + "_def").set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("❤️ 生命值").set_id("enh_pick_" + uid_s + "_hp").set_style(dpp::cos_primary));
    msg.add_component(row);

    dpp::component nav; nav.set_type(dpp::cot_action_row);
    nav.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(nav);
    return msg;
}

// ─── 單一屬性強化頁 ─────────────────────────────────────────────────────────────

static dpp::message make_enhance_stat_msg(dpp::snowflake uid, const std::string& stat,
                                           const std::string& dn = "", const std::string& av = "",
                                           const std::string& notice = "") {
    std::string uid_s = std::to_string((uint64_t)uid);
    Pet pet; bool has_pet = false;
    int stars = 0; int64_t chips = get_chips(uid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = pet_data.find(uid);
        if (it != pet_data.end() && it->second.stage > 0) { pet = it->second; has_pet = true; }
        auto ii = inventory_data.find(uid);
        if (ii != inventory_data.end()) {
            auto sit = ii->second.find("star_unknown");
            if (sit != ii->second.end()) stars = sit->second;
        }
    }

    dpp::embed e; e.set_title("💪  " + enh_stat_label(stat) + " 強化").set_color(0xE74C3C);
    {
        dpp::embed_footer f; f.text = "👤 " + (dn.empty() ? uid_s : dn);
        if (!av.empty()) f.icon_url = av; e.set_footer(f);
    }

    dpp::message msg;
    if (!has_pet) {
        e.set_description("❌ 需要已進化的寵物才能強化！");
        msg.add_embed(e);
        dpp::component nav; nav.set_type(dpp::cot_action_row);
        nav.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🏠 大廳").set_id("lobby_main_" + uid_s).set_style(dpp::cos_secondary));
        msg.add_component(nav);
        return msg;
    }

    int level = enh_level_of(pet, stat);
    std::string desc;
    if (!notice.empty()) desc += notice + "\n\n";
    desc += "目前等級：Lv **" + std::to_string(level) + "/" + std::to_string(ENH_MAX_LEVEL) + "**（" + enh_stat_effect(stat, level) + "）\n";
    desc += "⭐ 持有星星：**" + std::to_string(stars) + "**　💼 持有籌碼：**" + std::to_string(chips) + "**\n\n";

    bool maxed = level >= ENH_MAX_LEVEL;
    bool can_afford = false;
    if (!maxed) {
        const EnhTier& t = ENH_TABLE[level]; // 升到 level+1 所需
        desc += "**升到 Lv " + std::to_string(level + 1) + "** 需要：\n";
        desc += "⭐ 星星 ×" + std::to_string(t.stars) + "　💰 籌碼 " + std::to_string(t.chips) + "　🎲 成功率 " + std::to_string(t.rate_pct) + "%\n";
        can_afford = (stars >= t.stars && chips >= t.chips);
        if (!can_afford) desc += "\n❌ 素材或籌碼不足！";
    } else {
        desc += "✅ 已達最高等級！";
    }
    e.set_description(desc);
    msg.add_embed(e);

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔨 強化").set_id("enh_do_" + uid_s + "_" + stat)
        .set_style(dpp::cos_danger).set_disabled(maxed || !can_afford));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("↩ 返回").set_id("enh_main_" + uid_s).set_style(dpp::cos_secondary));
    msg.add_component(row);
    return msg;
}

// ─── 按鈕主路由 ────────────────────────────────────────────────────────────────

static void handle_enhance_button(const dpp::button_click_t& ev) {
    const std::string& cid = ev.custom_id;
    dpp::snowflake uid = ev.command.get_issuing_user().id;
    std::string dn = ev.command.member.get_nickname();
    if (dn.empty()) dn = ev.command.get_issuing_user().global_name.empty()
                         ? ev.command.get_issuing_user().username
                         : ev.command.get_issuing_user().global_name;
    std::string av = ev.command.get_issuing_user().get_avatar_url();
    std::string uid_s = std::to_string((uint64_t)uid);

    if (cid == "enh_main_" + uid_s) {
        ev.reply(dpp::ir_update_message, make_enhance_main_msg(uid, dn, av)); return;
    }

    if (cid.rfind("enh_pick_" + uid_s + "_", 0) == 0) {
        std::string stat = cid.substr(std::string("enh_pick_" + uid_s + "_").size());
        ev.reply(dpp::ir_update_message, make_enhance_stat_msg(uid, stat, dn, av)); return;
    }

    if (cid.rfind("enh_do_" + uid_s + "_", 0) == 0) {
        std::string stat = cid.substr(std::string("enh_do_" + uid_s + "_").size());
        std::string notice;
        bool no_pet = false, blocked = false; // blocked = maxed 或素材/籌碼不足
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto pit = pet_data.find(uid);
            if (pit == pet_data.end() || pit->second.stage == 0) {
                no_pet = true;
            } else {
                Pet& pet = pit->second;
                int level = (stat == "atk") ? pet.enh_atk : (stat == "def") ? pet.enh_def : pet.enh_hp;
                if (level >= ENH_MAX_LEVEL) {
                    blocked = true;
                } else {
                    const EnhTier& t = ENH_TABLE[level];
                    int stars = 0;
                    auto& inv = inventory_data[uid];
                    auto sit = inv.find("star_unknown");
                    if (sit != inv.end()) stars = sit->second;
                    int64_t chips = chip_data[uid].chips;
                    if (stars < t.stars || chips < t.chips) {
                        blocked = true;
                    } else {
                        // 消耗素材（無論成功失敗）
                        inv["star_unknown"] -= t.stars;
                        chip_data[uid].chips -= t.chips;
                        bool success = std::uniform_int_distribution<int>(1, 100)(enh_rng()) <= t.rate_pct;
                        if (success) {
                            if (stat == "atk") pet.enh_atk++; else if (stat == "def") pet.enh_def++; else pet.enh_hp++;
                            notice = "✅ 強化成功！" + enh_stat_label(stat) + " 升到 **Lv " + std::to_string(level + 1) + "**！";
                        } else {
                            notice = "❌ 強化失敗！素材與籌碼已消耗。";
                        }
                    }
                }
            }
        }
        if (no_pet) { ev.reply(dpp::ir_update_message, make_enhance_main_msg(uid, dn, av)); return; }
        if (blocked) { ev.reply(dpp::ir_update_message, make_enhance_stat_msg(uid, stat, dn, av)); return; }
        save_pet_data(); save_inventory(); save_chips();
        ev.reply(dpp::ir_update_message, make_enhance_stat_msg(uid, stat, dn, av, notice)); return;
    }
}
