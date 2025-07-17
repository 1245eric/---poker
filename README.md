這是個用C寫的多人德撲小遊戲,可供3-4人遊玩在LINUX下運作 

.
├── server.c      # 主程式：負責伺服器邏輯與德撲流程
├── client.c      # 客戶端程式（每位玩家執行一份）
└── README.md

編譯方式(linux)
```
gcc server.c -o server

gcc client.c -o client

```
🚀 功能特色
✅ 支援 3 或 4 位玩家同時連線

✅ 完整德州撲克發牌流程（Flop、Turn、River）

✅ 每位玩家可選擇留下或棄牌

✅ 自動判斷最強手牌（含順子、同花、葫蘆等組合）

✅ 終局宣布勝利者
