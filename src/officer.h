#pragma once
#include "types.h"
#include <string>
#include <ctime>

// 副會長權限檢查與時間格式解析（原本放在簽到系統，簽到移除後保留給經驗活動、抽獎等功能共用）

// ─── Permission: admin / 副會長 / 會長 ────────────────────────────────────────

static bool si_perm(dpp::snowflake uid, const std::vector<dpp::snowflake>& roles) {
    if (!cfg.notify_user_id.empty() && std::to_string(uid) == cfg.notify_user_id)
        return true;
    for (auto& rid : roles) {
        const dpp::role* r = dpp::find_role(rid);
        if (r && r->name.find("會長") != std::string::npos)
            return true;
    }
    return false;
}
static bool si_perm(const dpp::interaction& cmd) {
    return si_perm(cmd.member.user_id, cmd.member.get_roles());
}

// ─── 截止時間解析 ─────────────────────────────────────────────────────────────
// 支援：M/D HH:MM[:SS]（指定日期時間）、HH:MM（今天幾點）、Xm（X分鐘後）、Xh（X小時後）；無法解析回傳 0

static time_t parse_si_deadline(const std::string& s) {
    if (s.empty()) return 0;
    time_t now = time(nullptr);

    // M/D HH:MM 或 M/D HH:MM:SS（例：9/14 23:59 或 9/14 23:59:59）
    {
        size_t sp = s.find(' ');
        size_t sl = s.find('/');
        if (sp != std::string::npos && sl != std::string::npos && sl < sp) {
            try {
                int mon = std::stoi(s.substr(0, sl));
                int day = std::stoi(s.substr(sl + 1, sp - sl - 1));
                std::string tpart = s.substr(sp + 1);
                int h = 0, m = 0, sec = 0;
                size_t c1 = tpart.find(':'), c2 = tpart.rfind(':');
                if (c1 == std::string::npos) return 0;
                h = std::stoi(tpart.substr(0, c1));
                if (c1 == c2) {
                    m = std::stoi(tpart.substr(c1 + 1));
                } else {
                    m   = std::stoi(tpart.substr(c1 + 1, c2 - c1 - 1));
                    sec = std::stoi(tpart.substr(c2 + 1));
                }
                if (mon < 1 || mon > 12 || day < 1 || day > 31) return 0;
                if (h < 0 || h > 23 || m < 0 || m > 59 || sec < 0 || sec > 59) return 0;
                struct tm lt{}; localtime_s(&lt, &now);
                lt.tm_mon = mon - 1; lt.tm_mday = day;
                lt.tm_hour = h; lt.tm_min = m; lt.tm_sec = sec;
                time_t t = mktime(&lt);
                if (t <= 0) return 0;
                return t;
            } catch (...) { return 0; }
        }
    }

    // HH:MM
    if (s.size() == 5 && s[2] == ':') {
        try {
            int h = std::stoi(s.substr(0, 2));
            int m = std::stoi(s.substr(3, 2));
            if (h < 0 || h > 23 || m < 0 || m > 59) return 0;
            struct tm lt{}; localtime_s(&lt, &now);
            lt.tm_hour = h; lt.tm_min = m; lt.tm_sec = 0;
            time_t t = mktime(&lt);
            if (t <= now) t += 86400; // 已過則改為明天
            return t;
        } catch (...) { return 0; }
    }

    // Xm（分鐘）
    if (!s.empty() && (s.back() == 'm' || s.back() == 'M')) {
        try { int v = std::stoi(s.substr(0, s.size() - 1)); return v > 0 ? now + v * 60 : 0; }
        catch (...) { return 0; }
    }

    // Xh（小時）
    if (!s.empty() && (s.back() == 'h' || s.back() == 'H')) {
        try { int v = std::stoi(s.substr(0, s.size() - 1)); return v > 0 ? now + v * 3600 : 0; }
        catch (...) { return 0; }
    }

    return 0;
}
