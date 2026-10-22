> Historical Pro 1.5 implementation note. Current installation: INSTALL_TR.md / INSTALL_EN.md. Later owner reports: OWNER_TEST_REPORT.md.

# CSI Radar Pro 1.5 — kurulum ve cihaz kabulü

Bu paket cihaz testi adayıdır. 1.5 kararlı sürümü henüz yayımlanmadı.
Hem ESP32 firmware'i hem FAP değişti; yalnızca FAP kopyalamak yeterli değil.
E11 sistem firmware'inizi değiştirmeyin. Mevcut 1.0 FAP'ınızı bilgisayara yedekleyin.

## 1. ESP32'yi güncelleyin

Kartı Flipper'dan çıkarıp USB'ye takın. BOOT basılıyken RESET'e basıp bırakın,
sonra BOOT'u bırakın. Aygıt Yöneticisi'ndeki yükleme COM portunu bulun.
ESP-IDF PowerShell'i yeni paketin `ready` klasöründe açın.
Aşağıdaki COM12 yerine görünen yükleme portunu yazın:

```powershell
python -m esptool --chip esp32s2 --port COM12 --baud 460800 write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 flipper_csi_radar.bin
```

Hash doğrulaması ve işlem sonunu bekleyin. **erase_flash kullanmayın.** Partition
haritası 1.0 ile aynıdır; Wi-Fi bilgileri NVS'de korunmak üzere tasarlanmıştır.
Fiziksel ayar kalıcılığı kontrolü bu testin parçasıdır. RESET'e basın.

## 2. Flipper uygulamasını güncelleyin

USB'yi çıkarıp kartı Flipper'a takın. CSI Radar/Labmate gibi uygulamaları kapatın.
qFlipper ile yeni `ready/csi_radar.fap` dosyasını `/ext/apps/GPIO/csi_radar.fap`
üzerine kopyalayın. Menü adı **CSI Radar Pro** olur. Sürüm ekranında FAP/ESP 1.5.0
ve protocol 2 bilgilerini görün. Eski ESP yazılımı varsa uygulama güncelleme ister.

## 3. Wi-Fi ve tuşlar

Uzun OK ile ağ taraması yapın. Ağınızı seçip şifreyi girin. SSID boşluğu
listeden seçimde otomatik korunur. Manuel klavyede ilk `_` hücresi SPACE'tir;
son `#` hücresi kaydeder. Oklarla gezin, kısa OK ile karakter ekleyin.
Şifre ekranda yıldızlarla görünür. **Uzun OK alanı kaydeder**, Back son harfi siler,
uzun Back iptal eder. En az 8, en çok 63 şifre karakteri gerekir.

Tarama bağlantı kurulurken meşgul hatası verirse biraz bekleyip tekrar tarayın.
WPA2 veya WPA/WPA2 ve WPA2/WPA3 karma ağlar seçim için kabul edilir. 2.4 GHz ve
20 MHz ilk test ayarıdır. Saf WPA3/açık/kurumsal ağ desteği ilan edilmiyor.
USB alternatifi, normal çalışma portuyla:

```powershell
python tools/csi_usb.py COM9 setup
python tools/csi_usb.py COM9 info
python tools/csi_usb.py COM9 scan
```

## 4. Profiller ve otomatik eşik

Uzun Sol → Profiller: Sessiz 0.080, Normal 0.120, Hareketli 0.200 veya saklanan
özel eşik. Bu değerler başlangıç ayarlarıdır. Yukarı/aşağı elle T değiştirir ve
otomatik modu kapatır. Eşik düşerse hassasiyet artar.

Uzun Sol → AUTO seçildiğinde bağlantı/veri uygunsa 100 paketlik kalibrasyon
başlar. Odayı hareketsiz bırakın. Otomatik eşik gürültüden hesaplanır; sürekli
hareketi öğrenip gizleyen çevrimiçi bir ayarlama değildir. Kalibrasyon kirliyse,
çok yavaşsa veya veri kesilirse kabul edilmez. AUTO ? görünürse tekrar kalibre edin.
Q değeri insan olasılığı değildir; bağlantı/örnek kalitesidir.

Uzun Sağ ESP'ye aktif eşik/modu kaydeder. Dil, sessiz mod, profil ve özel eşik
Flipper SD kartına otomatik kaydedilir. SD yok/hatalıysa uyarı görülür.
Ayarlar `/ext/apps_data/csi_radar/settings.bin` ve `.bak` dosyalarındadır.
Wi-Fi parolası SD ayarlarına veya CSV'ye yazılmaz; ESP NVS'inde şifresiz tutulur.

## 5. SD kaydı ve PC analizi

Uzun Sol → SD log START. Ana ekranda R görünür. Aynı menüden STOP ile bitirin.
CSV dosyaları `/ext/apps_data/csi_radar/logs/` altındadır; 256 KiB'da yeni dosya
açılır. Eski kayıtlar otomatik silinmez. SD dolu/yoksa kayıt durur ve uyarı çıkar.
Queue kaybı bilgi ekranındaki SD sayacında görünür. Tarih/saat Flipper RTC ayarıdır;
UTC olduğu varsayılmaz. SD yazıcı ayrı görevde çalışır.

Ham kayıt için USB'de normal portu kullanın; Flipper uygulaması kapalı olsun:

```powershell
python tools/csi_usb.py COM9 record still.log --seconds 1800 --raw
python tools/csi_usb.py COM9 record moving.log --seconds 120 --raw
python tools/replay.py still.log moving.log --threshold 0.120 --output-dir comparison
```

Replay standart Python ile CSV ve summary.json üretir; var olan dosyaları ezmez.
Ham kayıt en çok 10 Hz olduğu için 20 Hz canlı sonuçla birebir aynı değildir.
Gerçek veri dosyaları henüz bu pakete dahil edilmedi; kullanıcı ölçümleri bekleniyor.

## İlk kısa testler

1. Ağ seçimi ve gizli klavye ile bağlanın; mevcut boşluklu SSID'niz çalışıyor mu?
2. Normal profilde kalibre edip 10 yürü-dur geçişini sayın; sonra AUTO'yu deneyin.
3. Sessiz modu ve özel T'yi kaydedip uygulama/kartı yeniden açın; geri geliyor mu?
4. 1 dakika SD kaydı alıp qFlipper'dan CSV'yi açın; yeniden oturum eski dosyayı koruyor mu?
5. Router'ı kapatıp açın; eski hareket bilgisi geçersiz oluyor ve veri geri geliyor mu?

Sonuçlar ayrı PASS/FAIL olarak yazılmalı. Sonraki kapılar: 30 dakika durgunlukta
hedef <=1 yanlış alarm, 10 geçişte >=9 tespit/2 saniye, 10 yeniden bağlantı/başlatma,
2 saat kayıt oturumu. Bu hedeflerin sağlandığı henüz iddia edilmez.

## Geri dönüş

[1.0.0 sürümündeki](https://github.com/sehmusadsoy-ui/flipper-csi/releases/tag/v1.0.0)
eski ESP firmware'ini aynı adreslerle ve eski FAP'ı yükleyebilirsiniz. Flash silme
kullanmayın; E11'i değiştirmeyin. 1.0 sessiz mod/profil SD dosyalarını kullanmaz.
Farklı Wi-Fi/ayar davranışı görülürse önce kaydedilen T'yi ve ağı yeniden kontrol edin.
