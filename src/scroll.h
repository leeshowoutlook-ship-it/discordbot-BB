#pragma once
#include <dpp/dpp.h>
#include <random>
#include <sstream>

static std::mt19937& sc_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

// ─── Roll logic ───────────────────────────────────────────────────────────────
// 10%: 10% success, 90% fail
// 30%: 30% success, 35% fail, 35% explode
// 60%: 60% success, 40% fail
// 70%: 70% success, 15% fail, 15% explode

enum class ScResult { SUCCESS, FAIL, EXPLODE };

static ScResult sc_roll(int pct) {
    int r = std::uniform_int_distribution<int>(1, 100)(sc_rng());
    switch (pct) {
        case 10: return (r <= 10) ? ScResult::SUCCESS : ScResult::FAIL;
        case 30:
            if (r <= 30) return ScResult::SUCCESS;
            if (r <= 65) return ScResult::FAIL;
            return ScResult::EXPLODE;
        case 60: return (r <= 60) ? ScResult::SUCCESS : ScResult::FAIL;
        case 70:
            if (r <= 70) return ScResult::SUCCESS;
            if (r <= 85) return ScResult::FAIL;
            return ScResult::EXPLODE;
        default: return ScResult::FAIL;
    }
}

struct ScSummary { int success = 0, fail = 0, explode = 0; };

static ScSummary sc_roll_many(int pct, int count) {
    ScSummary s;
    for (int i = 0; i < count; i++) {
        switch (sc_roll(pct)) {
            case ScResult::SUCCESS: s.success++; break;
            case ScResult::FAIL:    s.fail++;    break;
            case ScResult::EXPLODE: s.explode++; break;
        }
    }
    return s;
}

// ─── Selection message ────────────────────────────────────────────────────────

static dpp::message make_scroll_sel_msg(dpp::snowflake uid) {
    std::string sid = std::to_string((uint64_t)uid);
    dpp::embed e;
    e.set_title("📜  卷軸使用模擬器").set_color(0x9B59B6);
    e.set_description(
        "選擇要模擬使用的卷軸類型：\n\n"
        "**10%** — 成功 10%，失敗 90%\n"
        "**30%** — 成功 30%，失敗 35%，💥 爆炸 35%\n"
        "**60%** — 成功 60%，失敗 40%\n"
        "**70%** — 成功 70%，失敗 15%，💥 爆炸 15%"
    );

    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("10% 卷軸").set_id("scroll_go_" + sid + "_10_1").set_style(dpp::cos_secondary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("30% 卷軸").set_id("scroll_go_" + sid + "_30_1").set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("60% 卷軸").set_id("scroll_go_" + sid + "_60_1").set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("70% 卷軸").set_id("scroll_go_" + sid + "_70_1").set_style(dpp::cos_success));

    dpp::message msg; msg.add_embed(e); msg.add_component(row);
    return msg;
}

// ─── Result message ───────────────────────────────────────────────────────────

static dpp::message make_scroll_result_msg(dpp::snowflake uid, int pct, int count) {
    std::string sid  = std::to_string((uint64_t)uid);
    std::string spct = std::to_string(pct);
    ScSummary sum    = sc_roll_many(pct, count);

    dpp::embed e;
    std::ostringstream desc;

    if (count == 1) {
        if (sum.success == 1) {
            e.set_title("✅  成功！").set_color(0x2ECC71);
            desc << "**" << pct << "% 卷軸** × 1 張\n\n🎉 **卷軸成功！**";
        } else if (sum.explode == 1) {
            e.set_title("💥  爆炸！！！").set_color(0xFF0000);
            desc << "**" << pct << "% 卷軸** × 1 張\n\n💀 **裝備炸毀！**";
        } else {
            e.set_title("❌  失敗…").set_color(0x7F8C8D);
            desc << "**" << pct << "% 卷軸** × 1 張\n\n😔 **卷軸失敗。**";
        }
    } else {
        e.set_title("📜  卷軸模擬結果");
        desc << "**" << pct << "% 卷軸** × " << count << " 張\n\n";
        desc << "✅ 成功 **" << sum.success << "** 張\n";
        desc << "❌ 失敗 **" << sum.fail << "** 張\n";
        if (sum.explode > 0)
            desc << "💥 爆炸 **" << sum.explode << "** 張\n";

        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f%%", sum.success * 100.0 / count);
        desc << "\n本次成功率 **" << buf << "**（期望 " << pct << "%）";

        // color based on outcome
        if (sum.explode > count / 2)
            e.set_color(0xFF0000);
        else if (sum.success >= count / 2)
            e.set_color(0x2ECC71);
        else if (sum.explode > 0)
            e.set_color(0xE67E22);
        else
            e.set_color(0x7F8C8D);
    }
    e.set_description(desc.str());

    // Row 1: retry with various counts
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("再試 1 次").set_id("scroll_go_" + sid + "_" + spct + "_1")
        .set_style(dpp::cos_primary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("5 次").set_id("scroll_go_" + sid + "_" + spct + "_5")
        .set_style(dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("10 次").set_id("scroll_go_" + sid + "_" + spct + "_10")
        .set_style(dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("20 次").set_id("scroll_go_" + sid + "_" + spct + "_20")
        .set_style(dpp::cos_secondary));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("100 次").set_id("scroll_go_" + sid + "_" + spct + "_100")
        .set_style(dpp::cos_secondary));

    // Row 2: change scroll type
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 換卷軸").set_id("scroll_sel_" + sid)
        .set_style(dpp::cos_secondary));

    dpp::message msg; msg.add_embed(e);
    msg.add_component(row1);
    msg.add_component(row2);
    return msg;
}
