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
