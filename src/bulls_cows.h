#pragma once
#include <string>
#include <algorithm>
#include <random>

// 幾A幾B：4 個不重複數字。!猜數字 遊戲與突襲首領（西格諾斯）共用。

static std::string guess_gen_secret() {
    std::string d = "0123456789";
    std::mt19937 rng(std::random_device{}());
    std::shuffle(d.begin(), d.end(), rng);
    return d.substr(0, 4);
}

// Returns "xAyB"
static std::string guess_calc_ab(std::string secret, std::string guess) {
    int a = 0, b = 0;
    for (int i = 0; i < 4; i++) {
        if (guess[i] == secret[i]) { a++; guess[i] = secret[i] = 'X'; }
    }
    for (int i = 0; i < 4; i++) {
        if (guess[i] == 'X') continue;
        for (int j = 0; j < 4; j++) {
            if (secret[j] == 'X') continue;
            if (guess[i] == secret[j]) { b++; secret[j] = 'X'; break; }
        }
    }
    return std::to_string(a) + "A" + std::to_string(b) + "B";
}

static bool guess_valid(const std::string& s) {
    if (s.size() != 4) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    // no repeating
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            if (s[i] == s[j]) return false;
    return true;
}
