# UART v2
115200 8N1. Her iki yönde ASCII: `$PAYLOAD*CCCC\n`.
CCCC: payload üzerinde CRC16/CCITT-FALSE (polinom 0x1021, başlangıç 0xFFFF).
Eski checksumsuz v1 protokolü kabul edilmez; iki taraf birlikte güncellenmelidir.

Telemetri 10 Hz:
`CSI2,seq,score,rssi,state,pkts,thr,age,rate,drops,progress`
score/threshold: x1000. State 0 öğrenme, 1 durgun, 2 hareket, 3 kalibrasyon,
4 CSI yok, 5 Wi-Fi yok, 6 kurulum. Age milisaniye; henüz veri yoksa UINT32_MAX.
Rate işlenen geçerli paket/saniye; en fazla yaklaşık 20. Drops kuyruk kaybı +
algoritma tarafından reddedilen örnekler; tüm radyo paketlerini saymaz.
Progress 0..100. Paket ve seq sayacı uint32; taşma mümkündür.

Komut payloadları: PING, CAL [20..2000], THR [5..2000], SAVE, RAW 0, RAW 1,
WIFI <SSID_UTF8_HEX> <PASSWORD_UTF8_HEX>. CAL varsayılan 100.
Yanıt payloadları: ACK,CAL; ACK,THR; ACK,SAVE; ACK,WIFI; ACK,RAW; ACK,PING,2
veya ERR,<sebep>. ACK,WIFI ayarın uygulandığını belirtir; Wi-Fi bağlandığını değil.

USB özel ham kayıt: `$RAW,ms,rssi,skip,IQ_HEX\n`; 128 bayt IQ, CRC yok,
analiz kaydı içindir. Kontrol veya Flipper alarm kararında kullanılmaz.


## Pro 1.5 capability extension (protocol 2 envelope)

Both new FAP and ESP firmware are required. `PING` also emits
`HELLO,1.5.0,2,ESP32-S2`. A 1.0 ESP lacks this handshake and is not accepted
for Pro controls. Existing CSI2 fields stay unchanged.

`CFG <milli> <auto:0|1>` sets the threshold/mode; auto starts a 100-sample
calibration when fresh CSI is available. `THR` explicitly disables auto.
`SAVE` commits threshold and auto mode to ESP NVS.

`STAT,auto,noise_milli,quality,disconnect_reason,cal_error,calibrated`
is sent once a second. Quality is a link/sample heuristic, not a human-presence
probability. Calibration error: 0 none, 1 gap/interruption, 2 noisy/insufficient
bins, 3 low sample rate. Auto threshold is valid only after calibrated=1.

`SCAN` starts a nonblocking scan. Replies: ACK,SCAN; then
`AP,index,rssi,channel,auth,ssid_utf8_hex` (maximum 12); then APEND,count.
Scan errors are explicit. SSIDs/passwords are not logged to session CSV.

USB RAW schema 1 stays unchanged and is decimated to <=10 Hz. Offline replay
is diagnostic; live 20 Hz scores cannot be reconstructed exactly from it.
