# Pattern Table Generater System v1.2 Guide 

## 1. 生成呼吸燈光表 (gen_breath.py)
```
# 基本用法
python gen_breath.py -c <color> -i <interval_ms> -t <total_time_ms>

# 參數說明
-c, --color       顏色: g/r/b/rg/gb/rb/w
-f, --fade        fade效果: 0=關閉, 1=開啟 (預設1)
-i, --interval    幀間隔時間 (毫秒)
-t, --total_time  總時間 (毫秒)


# 範例
python gen_breath.py -c w -i 100 -t 950     # 白色, 100ms間隔, 總時間950ms
python gen_breath.py -c g -f 0 -i 200 -t 5000  # 綠色, 無fade, 200ms間隔, 5秒
```

## 2. 讀取光表 (read_dat.py)
```
python read_dat.py

# 會先顯示 control.dat 資訊，然後詢問是否讀取 frame.dat
# 輸入 y 繼續，n 結束
```
## 3. 查看原始二進制內容 (read_bytes.py)
```
python read_bytes.py
# 以16進制和10進制顯示每個byte
```