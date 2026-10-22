# CSI Radar Ultimate 2.0 — kurulum ve kullanım

## Uyumluluk ve güncelleme

Flipper: E11-patched Momentum / API 87.1. Kart: resmi ESP32-S2 Wi-Fi Devboard.
Mevcut E11 sistem firmware'ini değiştirmeyin. FAP bu API için derlendi.

| Başlangıç | Gerekli işlem |
|---|---|
| İlk kurulum veya 1.0/1.5 | Bu paketin ESP firmware'i ve FAP'ı birlikte yüklenir |
| Çalışan 2.0 / protokol 3 | Yalnızca FAP yeterli; yeni ESP ikilisi sadece açılış sürüm metnini düzeltir |
| Başka firmware/API veya kart | Uyumluluk doğrulanmadı; bu paketi evrensel kabul etmeyin |

## ESP yükleme

Devboard'u Flipper'dan çıkarıp veri destekli USB kablosuyla PC'ye bağlayın.
BOOT basılı → RESET bas/bırak → BOOT bırak. ESP-IDF PowerShell'i açıp yeni
paketin `ready` klasörüne girin. Portu Aygıt Yöneticisi'nden kontrol edin.
Aşağıdaki COM8 **örnektir**; kendi portunuzu yazın. Flash sonrası USB portu değişebilir.

```powershell
python -m esptool --chip esp32s2 --port COM8 --baud 460800 write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 flipper_csi_radar.bin
```

Erase çalıştırmayın. Bölüm haritası değişmedi; NVS Wi-Fi ayarları korunur.
Hash doğrulaması ve bitişten sonra RESET; USB'yi çıkarın, kartı Flipper'a takın.

## FAP yükleme

Uygulamayı kapatın. qFlipper ile `ready/csi_radar.fap` dosyasını
`SD Card/apps/GPIO/csi_radar.fap` üzerine kopyalayın. Apps → GPIO → CSI Radar Ultimate.

## İlk ölçüm ve tuşlar

Açılışta HAZIR bekler. Cihazı sabit yerleştirip **OK** ile yeni kalibrasyonu başlatın.
Öğrenme boyunca hareketsiz kalın. Kalibrasyondan önceki geçici skorlar grafiğe,
hareket sayacına veya titreşim alarmına alınmaz. Grafik gerçek işlenmiş skoru gösterir;
ham radyo dalgası değildir. Noktalı çizgi eşiktir. `Y0.xxx` grafik üst sınırıdır.

- Sol: Menü. Sağ: Mod / sonraki ekran.
- Ana grafik, waterfall ve kurulum ekranında OK: kalibrasyon.
- Bilgi ekranında OK: Menü. Uzun OK: kalibrasyon kısayolu.
- Uzun sağ: titreşim aç/kapat. Yukarı/aşağı: manuel eşik, AUTO'dan çıkar.
- Geri: ana ekran/üst menü; ana ekranda çıkış.
- Uygulama açıkken ekran ışığı açık kalır; çıkışta normal zaman aşımı geri bırakılır.

Menüler: Alarm; Wi-Fi kurulumu; Kayıtlar; Ölçüm ayarları; Analiz ekranları; Yardım/bilgi.
Dil seçimi Yardım/bilgi içindedir. Türkçe cihaz yazıları ASCII kullanabilir.
Sessiz/dil/profil/eşik ayarları SD'de CRC ve yedekle saklanır. Karta eşiği kalıcı
yazmak için Ölçüm ayarları → Esigi karta kaydet. SD yoksa ayar kaydı garanti edilmez.

## Wi-Fi, filtre ve alarm

Menü → WiFi kurulumu: ağ tara/seç; şifre gizlenir, SSID boşlukları korunur.
2.4 GHz WPA2 veya desteklenen karma ağ kullanılır. Ağ bulunamaması ve parola
hataları bağlantı tanılarında ayrılır. USB kurulumu için `tools/csi_usb.py` de vardır.

Klasik filtre başlangıç yöntemidir. Dayanıklı filtre taşıyıcı medyan/MAD ve
zamansal medyan kullanır. Daha başarılı olduğu gerçek etiketli kayıtla henüz
kanıtlanmadı. AUTO gürültüye göre eşik seçer; kişinin varlık olasılığı değildir.

Menü → Alarm → Alarmi ac. Ölçüm hazır olmalı. Durum ekranında geri sayım görünür.
Bu ekranda OK alarmı kapatır/açar; sağ titreşimi aç/kapatır. Menüden süreler,
tekrar ve titreşim testi seçilir. Süre ayarını değiştirmek alarmı kapatır.
Ana ekrana dönmek alarmı kapatmaz; uygulamadan çıkmak kapatır. Alarm açılışta
kendiliğinden kurulmaz. Veri kesilince eski hareket üzerinden uyarı üretilmez.

## Kayıt ve analiz

Kayıtlar → Kayda başla/durdur. Dosyalar `/ext/apps_data/csi_radar/logs/` içindedir.
A/B taraması için kaydı durdurun; iki dosyayı A ve B olarak seçip karşılaştırın.
Mod ile kayıt kimliği/filtre/hata detayına geçilir. CSV şeması 2, `filter` sütunu
0 klasik / 1 dayanıklı; şema 1 kayıtları da okunur. Wi-Fi şifresi/SSID kaydedilmez.

```powershell
python tools/session_report.py A.csv B.csv --output oturum.html
python tools/replay.py oda.log --filter both --threshold 0.120 --output-dir analiz
python tools/capture_report.py oda.log --output oda.html
```

RAW kaydı için normal USB portunu seçin: `python tools/csi_usb.py COM9 record oda.log --seconds 60 --raw`.
COM9 da örnektir. RAW <=10 Hz; canlı motor 20 Hz. Yeniden oynatım tanısaldır,
birebir canlı eşdeğer veya doğruluk kanıtı değildir. Yerleşim/test sınırları ve
fiziksel kabul kaydı için DEVICE_TESTS_TR.md ve BUILD_EVIDENCE.md dosyalarını okuyun.

Önceki 1.0 kurulumu: doğrulanmış v1.0.0 etiketi altındaki belgeyi kullanın;
mevcut bu belgeyi eski FAP-only 1.0 yönergesiyle karıştırmayın.
