#!/bin/bash

CLIENT_PATH="./client_test1" 
INPUT_CONN_OPT="a"     # 连接服务器
INPUT_ACTION_OPT="c"   # 获取100次时间
INPUT_EXIT_OPT="g"     # 退出

mkfifo /tmp/sync_barrier
exec 3<>/tmp/sync_barrier 

for i in 1 2; do
    { 
      # 1. 发送连接指令
      echo "$INPUT_CONN_OPT" 
      
      # 2. 等待同步信号 (确保两个客户端同时开始请求时间)
      read -u 3 line 
      
      # 3. 发送获取100次时间的指令
      echo "$INPUT_ACTION_OPT"
      
      # 4. 稍微等一下让数据传完
      sleep 5 
      
      # 5. 发送退出指令
      echo "$INPUT_EXIT_OPT"
      
    } | "$CLIENT_PATH" | sed "s/^/[Client $i] /" &
done

sleep 1
echo "[System] Signaling clients to start concurrent requests..."
echo >&3
echo >&3

exec 3>&- 
wait
rm /tmp/sync_barrier
echo "[System] Test complete."