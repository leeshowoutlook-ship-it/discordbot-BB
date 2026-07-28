# discordbot-BB

C++ 寫的 Discord Bot，使用 [DPP](https://dpp.dev/) 函式庫。  
功能包含：籌碼系統、寵物、各種賭博小遊戲（21點、射、輪盤、火箭）、組隊討伐、一夜狼人、狼人殺、虛擬商店等。

---

## 開發流程

```
開發機（寫程式）→ git push → 執行機（跑 bot）→ git pull → rebuild → 重啟
```

**開發機**：只負責寫程式、push。  
**執行機**：pull 後 rebuild，再重啟 bot。

---

## 執行機 — 首次設定

### 1. 安裝必要工具

- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)（勾選「使用 C++ 的桌面開發」）
- [CMake](https://cmake.org/download/)（安裝時勾選加入 PATH）
- [Git](https://git-scm.com/)
- vcpkg：

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "Machine")
# 重新開啟 terminal 讓環境變數生效
```

### 2. Clone 專案

```powershell
git clone <GitHub repo URL>
cd discordbot-BB
```

### 3. 建置

```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

第一次 build 會自動下載 dpp、nlohmann_json 等依賴，需要一段時間。

### 4. 設定 .env

在 `build\Release\` 裡建立 `.env`（不在 git 裡，要自己建）：

```
BOT_TOKEN=你的_Discord_Bot_Token
NOTIFY_USER_ID=你的_Discord_User_ID
IMG_NORMAL=普通拉圖斯圖片網址
IMG_HARD=困難拉圖斯圖片網址
IMG_FLAME=殘暴炎魔圖片網址
MIN_BET_THREAD_ID=最低下注討論串ID
ALLIN_THREAD_ID=全押討論串ID
```

### 5. 複製玩家資料

玩家資料存在 `build\Release\` 的 JSON 檔，不在 git 裡（隱私考量）。  
從舊電腦手動複製以下檔案到新電腦的 `build\Release\`：

```
chips.json              # 籌碼、VIP、領取時間
bank.json               # 銀行存款/借款
pet_data.json           # 寵物
inventory.json          # 背包道具
purchases.json          # 購買記錄（最多 50 筆）
shop.json               # 楓之谷商店庫存
gacha_pity.json         # 扭蛋保底計數
shootstats.json         # 射統計
rocketstats.json        # 火箭統計
bjstats.json            # 21點統計
onwstats.json           # 一夜狼人統計
wolfplayerstats.json    # 狼人殺統計
rlstats.json            # 輪盤統計
```

全新啟動不需複製，bot 會自動建立空檔案。

### 6. 啟動

```powershell
Start-Process -FilePath "build\Release\dcbot.exe" -WorkingDirectory "build\Release"
```

---

## 日常更新流程（每次 pull 後操作）

Bot 執行中時 exe 被鎖，直接 build 會報 LNK1104。正確步驟：

```powershell
# 1. 拉最新程式碼
git pull

# 2. 先改名讓 linker 可以寫入
Rename-Item "build\Release\dcbot.exe" "dcbot_old.exe"

# 3. Build
cmake --build build --config Release

# 4. 找 PID
Get-Process | Where-Object { $_.Name -like "dcbot*" }

# 5. 關舊的、開新的
Stop-Process -Id <PID> -Force
Start-Process -FilePath "build\Release\dcbot.exe" -WorkingDirectory "build\Release"

# 6. 確認、刪舊檔
Get-Process | Where-Object { $_.Name -like "dcbot*" }
Remove-Item "build\Release\dcbot_old.exe" -Force
```

---

## 專案結構

| 檔案 | 用途 |
|---|---|
| `src/main.cpp` | 主程式，所有事件處理入口 |
| `src/types.h` | 所有 struct / global 變數定義 |
| `src/handlers_bj.cpp` | 21點 |
| `src/handlers_roulette.cpp` | 俄羅斯輪盤 |
| `src/handlers_wolf.cpp` | 狼人殺按鈕 |
| `src/handlers_uc.cpp` | 臥底遊戲 |
| `src/handlers_hunt.cpp` | 怪物狩獵 |
| `src/handlers_raid.cpp` | 組隊討伐 |
| `src/handlers_dd.cpp` | 暗黑龍王 |
| `src/pet.h` | 寵物系統 |
| `src/shop.h` | 商店（楓之谷 + 虛擬） |
| `src/gacha.h` | 扭蛋 |
| `src/chips.h` | 籌碼 |
| `src/bank.h` | 銀行 |
| `src/wallet.h` | 錢包 UI |
| `src/onenight.h` | 一夜狼人 |
| `src/werewolf.h` | 狼人殺 |
| `src/helpers.h` | 工具函式、定期清理 |
| `CMakeLists.txt` | 建置設定 |
| `CLAUDE.md` | **Claude 工作規則，必讀** |

---

## 給 Claude 看的說明

這個 repo 的使用者在**兩台電腦**上維護：
- **開發機**：用 Claude Code 寫程式、push
- **執行機**：只負責 pull → rebuild → 重啟 bot

**請先讀 `CLAUDE.md`**，裡面有所有設計規則、bot 更新流程、新功能 checklist。

記憶資料在 `C:\Users\<使用者>\.claude\projects\...\memory\MEMORY.md`，如果存在請讀取以了解使用者偏好。

**所有回覆請使用繁體中文。**
