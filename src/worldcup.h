#pragma once
#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <memory>
#include <algorithm>
#include <ctime>
#include <nlohmann/json.hpp>

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string wc_flag(const std::string& team) {
    static const std::map<std::string,std::string> f = {
        {"阿根廷","🇦🇷"},{"法國","🇫🇷"},{"巴西","🇧🇷"},{"英格蘭","🏴󠁧󠁢󠁥󠁮󠁧󠁿"},
        {"西班牙","🇪🇸"},{"德國","🇩🇪"},{"葡萄牙","🇵🇹"},{"荷蘭","🇳🇱"},
        {"摩洛哥","🇲🇦"},{"克羅埃西亞","🇭🇷"},{"烏拉圭","🇺🇾"},{"比利時","🇧🇪"},
        {"美國","🇺🇸"},{"墨西哥","🇲🇽"},{"加拿大","🇨🇦"},{"日本","🇯🇵"},
        {"南韓","🇰🇷"},{"澳洲","🇦🇺"},{"瑞士","🇨🇭"},{"塞內加爾","🇸🇳"},
        {"厄瓜多","🇪🇨"},{"哥倫比亞","🇨🇴"},{"智利","🇨🇱"},{"秘魯","🇵🇪"},
        {"波蘭","🇵🇱"},{"丹麥","🇩🇰"},{"奧地利","🇦🇹"},{"塞爾維亞","🇷🇸"},
        {"土耳其","🇹🇷"},{"斯洛伐克","🇸🇰"},{"巴拿馬","🇵🇦"},{"哥斯大黎加","🇨🇷"},
        {"喀麥隆","🇨🇲"},{"迦納","🇬🇭"},{"奈及利亞","🇳🇬"},{"突尼西亞","🇹🇳"},
        {"沙烏地阿拉伯","🇸🇦"},{"伊朗","🇮🇷"},{"卡達","🇶🇦"},{"南非","🇿🇦"},
        {"委內瑞拉","🇻🇪"},{"巴拉圭","🇵🇾"},{"義大利","🇮🇹"},{"蘇格蘭","🏴󠁧󠁢󠁳󠁣󠁴󠁿"},
        {"象牙海岸","🇨🇮"},{"伊拉克","🇮🇶"},{"烏克蘭","🇺🇦"},
        {"挪威","🇳🇴"},{"瑞典","🇸🇪"},{"剛果DR","🇨🇩"},{"波士尼亞","🇧🇦"},
        {"阿爾及利亞","🇩🇿"},{"埃及","🇪🇬"},{"維德角","🇨🇻"},{"宏都拉斯","🇭🇳"},
    };
    auto it = f.find(team);
    return it != f.end() ? it->second : "";
}

static std::string wc_team_cn(const std::string& en) {
    static const std::map<std::string,std::string> cn = {
        {"Argentina","阿根廷"},{"France","法國"},{"Brazil","巴西"},{"England","英格蘭"},
        {"Spain","西班牙"},{"Germany","德國"},{"Portugal","葡萄牙"},{"Netherlands","荷蘭"},
        {"Morocco","摩洛哥"},{"Croatia","克羅埃西亞"},{"Uruguay","烏拉圭"},{"Belgium","比利時"},
        {"United States","美國"},{"USA","美國"},{"Mexico","墨西哥"},{"Canada","加拿大"},
        {"Japan","日本"},{"South Korea","南韓"},{"Korea Republic","南韓"},{"Australia","澳洲"},
        {"Switzerland","瑞士"},{"Senegal","塞內加爾"},{"Ecuador","厄瓜多"},
        {"Colombia","哥倫比亞"},{"Chile","智利"},{"Peru","秘魯"},{"Poland","波蘭"},
        {"Denmark","丹麥"},{"Austria","奧地利"},{"Serbia","塞爾維亞"},
        {"Turkey","土耳其"},{"Türkiye","土耳其"},{"Slovakia","斯洛伐克"},
        {"Panama","巴拿馬"},{"Costa Rica","哥斯大黎加"},{"Cameroon","喀麥隆"},
        {"Ghana","迦納"},{"Nigeria","奈及利亞"},{"Tunisia","突尼西亞"},
        {"Saudi Arabia","沙烏地阿拉伯"},{"Iran","伊朗"},{"Qatar","卡達"},
        {"South Africa","南非"},{"Venezuela","委內瑞拉"},{"Paraguay","巴拉圭"},
        {"Italy","義大利"},{"Scotland","蘇格蘭"},{"Ivory Coast","象牙海岸"},
        {"Côte d'Ivoire","象牙海岸"},{"Iraq","伊拉克"},{"Ukraine","烏克蘭"},
        {"Norway","挪威"},{"Sweden","瑞典"},
        {"Congo DR","剛果DR"},{"DR Congo","剛果DR"},{"Congo, DR","剛果DR"},
        {"Bosnia & Herzegovina","波士尼亞"},{"Bosnia-Herzegovina","波士尼亞"},
        {"Bosnia and Herzegovina","波士尼亞"},
        {"Honduras","宏都拉斯"},{"Algeria","阿爾及利亞"},{"Egypt","埃及"},
        {"Cape Verde","維德角"},
    };
    auto it = cn.find(en);
    return it != cn.end() ? it->second : en;
}

static std::string wc_stage_cn(const std::string& s) {
    if (s.find("Round of 32") != std::string::npos) return "32強";
    if (s.find("Round of 16") != std::string::npos) return "16強";
    if (s.find("Quarter")     != std::string::npos) return "8強";
    if (s.find("Semi")        != std::string::npos) return "4強";
    if (s.find("Third")       != std::string::npos) return "季軍賽";
    if (s.find("Final")       != std::string::npos) return "決賽";
    return "淘汰賽";
}

// ─── Today + tomorrow schedule ────────────────────────────────────────────────

struct WCEventInfo {
    std::string team_a, team_b, stage;
    time_t kickoff = 0;
    std::string state;       // "pre" | "in" | "post"
    std::string score_a, score_b, clock_str;
    bool completed = false;
};

static void wc_show_today(dpp::cluster& bot, dpp::snowflake channel_id) {
    time_t now = time(nullptr);

    auto make_date = [](time_t t) -> std::string {
        char buf[16]; struct tm tmp = {};
        gmtime_s(&tmp, &t);
        strftime(buf, sizeof(buf), "%Y%m%d", &tmp);
        return buf;
    };

    auto events  = std::make_shared<std::vector<WCEventInfo>>();
    auto ev_mtx  = std::make_shared<std::mutex>();
    auto pending = std::make_shared<std::atomic<int>>(2);

    auto handle = [&bot, channel_id, events, ev_mtx, pending]
                  (const dpp::http_request_completion_t& res) {
        if (res.status == 200) {
            try {
                auto j = nlohmann::json::parse(res.body);
                std::lock_guard<std::mutex> lk(*ev_mtx);
                if (j.contains("events")) {
                    for (auto& ev : j["events"]) {
                        WCEventInfo info;
                        // Kickoff UTC
                        std::string ds = ev.value("date", "");
                        int yr=0,mo=0,dy=0,hr=0,mn=0,sc=0;
                        sscanf(ds.c_str(), "%d-%d-%dT%d:%d:%dZ", &yr,&mo,&dy,&hr,&mn,&sc);
                        struct tm tv = {};
                        tv.tm_year=yr-1900; tv.tm_mon=mo-1; tv.tm_mday=dy;
                        tv.tm_hour=hr; tv.tm_min=mn; tv.tm_sec=sc;
                        info.kickoff = _mkgmtime(&tv);

                        // Stage
                        auto try_stage = [&](auto& src) {
                            if (src.contains("notes") && src["notes"].is_array() && !src["notes"].empty())
                                info.stage = wc_stage_cn(src["notes"][0].value("headline",""));
                        };
                        try_stage(ev);
                        if (info.stage.empty() && ev.contains("competitions") && !ev["competitions"].empty())
                            try_stage(ev["competitions"][0]);
                        if (info.stage.empty()) info.stage = "淘汰賽";

                        // Teams + status
                        if (ev.contains("competitions") && !ev["competitions"].empty()) {
                            auto& comp = ev["competitions"][0];
                            if (comp.contains("competitors")) {
                                for (auto& c : comp["competitors"]) {
                                    std::string en = c.contains("team") ? c["team"].value("displayName","") : "";
                                    std::string scr = c.value("score","");
                                    if (c.value("homeAway","") == "home") { info.team_a=wc_team_cn(en); info.score_a=scr; }
                                    else                                   { info.team_b=wc_team_cn(en); info.score_b=scr; }
                                }
                            }
                            if (comp.contains("status")) {
                                auto& st = comp["status"];
                                info.state     = st.contains("type") ? st["type"].value("state","pre") : "pre";
                                info.completed = st.contains("type") ? st["type"].value("completed",false) : false;
                                info.clock_str = st.value("displayClock","");
                            }
                        }
                        events->push_back(info);
                    }
                }
            } catch (...) {}
        }

        if (--(*pending) == 0) {
            std::sort(events->begin(), events->end(), [](auto& a, auto& b){ return a.kickoff < b.kickoff; });

            dpp::embed e;
            e.set_title("⚽ 世界盃 2026").set_color(0x1a6b1a);

            if (events->empty()) {
                e.set_description("今明兩天沒有比賽。");
            } else {
                std::string desc;
                for (auto& info : *events) {
                    // Taiwan time = UTC+8
                    time_t tw = info.kickoff + 8*3600;
                    struct tm tmp = {}; gmtime_s(&tmp, &tw);
                    char tbuf[12]; strftime(tbuf, sizeof(tbuf), "%m/%d %H:%M", &tmp);

                    std::string fa = wc_flag(info.team_a);
                    std::string fb = wc_flag(info.team_b);
                    std::string line = "**" + std::string(tbuf) + "**  "
                        + fa + info.team_a + " vs " + fb + info.team_b
                        + "  `" + info.stage + "`";

                    if (info.completed)
                        line += "  **" + info.score_a + "-" + info.score_b + "** ✅";
                    else if (info.state == "in")
                        line += "  **" + info.score_a + "-" + info.score_b + "** 🔴"
                              + (info.clock_str.empty() ? "" : " `" + info.clock_str + "`");

                    desc += line + "\n";
                }
                e.set_description(desc);
            }

            bot.message_create(dpp::message(channel_id, "").add_embed(e));
        }
    };

    std::string base = "https://site.api.espn.com/apis/site/v2/sports/soccer/fifa.world/scoreboard?dates=";
    bot.request(base + make_date(now),         dpp::m_get, handle);
    bot.request(base + make_date(now + 86400), dpp::m_get, handle);
}
