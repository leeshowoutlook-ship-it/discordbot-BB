# discordbot-BB — Claude 工作規則

## Terminal 權限
PowerShell / Bash 指令**直接執行，不需每次詢問確認**。
破壞性操作（Stop-Process、git reset 等）仍須先說明意圖再執行。

---

## Bot 更新標準流程（每次都這樣做）

Bot 執行中時 exe 被鎖，直接 build 會報 LNK1104 錯誤。正確步驟：

```powershell
# 1. 先改名，讓 linker 可以寫入新檔
Rename-Item "build\Release\dcbot.exe" "dcbot_old.exe"

# 2. Build
cmake --build build --config Release

# 3. 關舊的、開新的
Stop-Process -Id <PID> -Force
Start-Process -FilePath "build\Release\dcbot.exe" -WorkingDirectory "build\Release"

# 4. 確認新的在跑，刪舊檔
Get-Process | Where-Object { $_.Name -like "dcbot*" }
Remove-Item "build\Release\dcbot_old.exe" -Force
```

找 PID 用：`Get-Process | Where-Object { $_.Name -like "dcbot*" }`

---

## 全局設計規則（所有功能都要遵守）

1. **三種指令格式**：每個玩家可用的指令都要支援 `/`（slash）、`!`（半形）、`！`（全形）三種呼叫方式，功能完全一致。除非使用者有特別說明例外。
   - 新增 `!` 指令時，必須同時將指令字串加入 `is_our_cmd()` 函式內的 `EXACT`（或 `PREFIX`）白名單，否則訊息會被提前過濾掉而無法執行。
2. **英文版指令**：每個玩家可用的 `/` slash 指令都要有對應的英文版（例如 `/骰子` → `/dice`），handler 中用 `cmd_name == "骰子" || cmd_name == "dice"` 覆蓋，registration 也要一起加入。除非使用者有特別說明例外（純管理員指令或彩蛋功能可免）。
3. **道具 ID 與交易**：所有道具（VirtualShopItem）和裝備（GachaItem）都要有唯一的數字 ID（item_id），且允許玩家透過 `!交易` 指令互相交換。除非使用者有特別說明例外（例如狩獵卷不可售出）。

---

## 新功能／重啟前 Checklist（四條，全部確認才算完成）

1. **持久化** — 所有遊戲資料、統計、狀態都要有對應的 JSON 存檔與載入。過去因沒持久化導致玩家籌碼和寵物資料遺失。
2. **三種指令格式同步** — 每個指令都要支援 `/`（slash）、`!`（半形）、`！`（全形）三種輸入，功能完全一致。
3. **開關不影響進行中的活動** — bot 重啟後，21點、骰子、抽獎、狼人殺等進行中狀態必須可從持久化資料還原。
4. **重啟前確認無殘留進程** — `Get-Process -Name "dcbot" -ErrorAction SilentlyContinue` 確認沒有舊進程，再啟動新的。

---

## 新遊戲固定規格

每次新增遊戲，以下全部必做：

1. **玩家身分顯示**：embed thumbnail 放頭像（avatar_url），footer 放「👤 顯示名稱」
2. **結算畫面**：「再來一局」按鈕（同押注）＋「雙倍下注」按鈕（×2，籌碼不足則 disabled）
3. **盈虧與勝率**：結算底部顯示累計盈虧與勝率（參考 `sh_stats_line` 模式）
4. **錢包整合**：`make_wallet_games_msg` 加入該遊戲的統計資料（勝/敗/盈虧）
5. **討論串限制**（`!指令` 和 `/指令` 都要加）：
   - `allin_thread_id`：強制 ALLIN（全額），持有 < 5000 碼則拒絕
   - `min_bet_thread_id`：最低下注 1000 碼，不足則拒絕
   - 參考 `!射` / `/射` 的寫法

---

## 專案結構

| 檔案 | 用途 |
|---|---|
| `src/main.cpp` | 主程式，所有 bot 事件處理（`!` 指令、slash 指令、按鈕） |
| `src/types.h` | 所有 struct / global 變數定義 |
| `src/bank.h` | 銀行系統（存款、借款、離散利息） |
| `src/chips.h` | 籌碼系統（領取、富豪榜） |
| `src/onenight.h` | 一夜狼人遊戲邏輯 |
| `src/werewolf.h` | 狼人殺遊戲邏輯 |
| `build\Release\dcbot.exe` | 執行檔 |

## 資料持久化

- `chips.json` — 籌碼、領取時間
- `bank.json` — 銀行存款、借款
- 所有寫入用 `atomic_write()` 確保安全（write → rename）

---

## 初次建置（只需做一次）

```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
cd build/Release && copy ..\..\build\Release\.env . && dcbot.exe
```

需要：Visual Studio 2022 Build Tools（C++ 桌面開發）、CMake、Git、vcpkg
