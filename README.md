# Discord Bot (C++ / D++)

## 建置步驟(Windows + VSCode)

1. 安裝工具鏈(只做一次)
   - Visual Studio 2022 Build Tools(勾「使用 C++ 的桌面開發」)
   - CMake、Git
   - vcpkg:
     ```
     git clone https://github.com/microsoft/vcpkg
     cd vcpkg && bootstrap-vcpkg.bat
     ```
2. 設定環境變數 `VCPKG_ROOT` 指向 vcpkg 資料夾
3. 專案根目錄:複製 `.env.example` → `.env`,貼上 Bot Token
4. 建置:
   ```
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```
   (第一次會自動編譯 DPP,需要一段時間)
5. 執行(.env 要在執行目錄):
   ```
   cd build/Release && copy ..\..\.env . && dcbot.exe
   ```
6. 到 Discord 輸入 `/ping`,回 Pong 就成功了
   (全域指令首次註冊可能要等幾分鐘~1小時才出現,測試期可改用 guild_command_create 立即生效)

## VSCode 建議擴充
- C/C++ (Microsoft)、CMake Tools
