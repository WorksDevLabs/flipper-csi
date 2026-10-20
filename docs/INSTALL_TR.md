# CSI Radar — Türkçe kurulum

**Sürüm:** 1.0.0 · **Hedef:** E11 / API 87.1, resmi ESP32-S2 Wi-Fi Devboard.

Hazır dosyalar depodaki `ready/` klasöründedir. Workflow çalıştırmanız veya
kaynaktan derlemeniz gerekmez. [Ana sayfaya dön](../README.md).

## Mevcut çalışan sürümü ikonlu sürüme güncelleme

ESP32 firmware'i ve algılama kodu 0.2 ile aynıdır. Yalnızca FAP değişir.

1. [csi_radar.fap dosyasını indir](https://github.com/sehmusadsoy-ui/flipper-csi/blob/main/ready/csi_radar.fap).
   Bağlantı yerine GitHub dosya sayfasını kullanırsanız **Download raw file**
   düğmesine basın; web sayfasını HTML olarak kaydetmeyin.
2. Flipper'da CSI Radar ve Labmate gibi GPIO/UART uygulamalarını kapatın.
3. Flipper'ı USB ile bilgisayara bağlayıp qFlipper'ı açın.
4. Dosya yöneticisinde **SD Card → apps → GPIO** klasörünü açın.
5. Eski `csi_radar.fap` dosyasını isterseniz bilgisayarınıza yedekleyin;
   yeni dosyayı aynı adla üzerine kopyalayın.
6. Flipper'da **Apps → GPIO → CSI Radar** uygulamasını açın.
   Menüde yeni radar ikonu görünmeli.
7. Hareket edin: `MOVING` ve titreşim; durup bekleyin: `STILL` beklenir.

**E11 firmware'ini ve çalışan ESP32 firmware'ini yeniden yüklemeyin.** Wi-Fi
bilgileri ve kaydedilmiş eşik ESP32'de kalır. Yeni ikonlu FAP'ın cihazda açılışı
bu güncellemenin son kabul kontrolüdür.

## İlk kurulum — ESP32 henüz CSI Radar çalıştırmıyorsa

1. Depoda **Code → Download ZIP** seçip arşivi çıkarın.
2. Resmi Wi-Fi Devboard'u Flipper'dan çıkarıp USB ile bilgisayara bağlayın.
3. **BOOT** basılıyken **RESET**'e basıp bırakın, ardından **BOOT**'u bırakın.
4. Aygıt Yöneticisi'nden kartın COM numarasını bulun.
5. ESP-IDF 5.5 PowerShell'i `ready` klasöründe açın. Gerekirse:

```powershell
python -m pip install esptool==4.12.0 pyserial==3.5
```

Aşağıdaki **COM5** yerine kartın portunu yazıp tek satır olarak çalıştırın:

```powershell
python -m esptool --chip esp32s2 --port COM5 --baud 460800 write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 flipper_csi_radar.bin
```

Bu işlem ESP32 kartındaki mevcut yazılımı değiştirir; Flipper'ın E11 yazılımını
değiştirmez. Yükleme ve hash doğrulaması bitene kadar kabloyu çıkarmayın.
Başarılı işlemden sonra RESET'e basın. Normal çalışma modunda COM portu değişebilir.

## Wi-Fi kurulumu

Boşluklu SSID veya Flipper klavyesinde zor girilen karakterler için USB yöntemi:

1. Kart normal çalışırken USB ile bilgisayara bağlı olsun; BOOT'a basmayın.
2. Terminali depodan çıkardığınız ana klasörde açın.
3. **COM5** yerine normal çalışma portunu kullanın:

```powershell
python tools/csi_usb.py COM5 setup
```

Wi-Fi adını aynen yazın. Şifre girilirken terminalde görünmez. `ACK,WIFI`
ayarın kaydedildiğini belirtir; tek başına ağa bağlanıldığını göstermez.
USB'yi çıkarıp kartı Flipper'a takın ve uygulamayı açın.

Alternatif: uygulamada OK'a uzun basıp Flipper klavyesini kullanın.
Bu klavyede şifre görünür. Kimlik bilgileri ESP32 NVS'inde şifresiz saklanır.
Başlangıç testi için 2.4 GHz WPA2 uyumlu ağ ve 20 MHz kanal genişliği kullanın.

## Kontroller

| Tuş | İşlev |
| --- | --- |
| Kısa OK | 100 geçerli paketle kalibrasyon |
| Uzun OK | Wi-Fi kurulumu |
| Yukarı / aşağı | Eşik ayarı |
| Sol | Bilgi / grafik |
| Kısa sağ | Titreşim/LED uyarısını sessize al/aç |
| Uzun sağ | Aktif eşiği ESP32'ye kalıcı kaydet |
| Geri | Çıkış; Wi-Fi ekranında iptal |

Kalibrasyonda ortamı hareketsiz bırakın. 20 paket/saniyede yaklaşık 5 saniye
sürer; düşük paket hızında daha uzun sürebilir. Eşik başlangıçta 0.120'dir;
her odada uygun olduğu iddia edilmez. Sessiz mod uygulama yeniden açılınca sıfırlanır.

## Ekran durumları

| Durum | Anlamı |
| --- | --- |
| SETUP WIFI | Wi-Fi bilgisi girilmeli |
| WIFI OFF | Ağa bağlanılıyor / bağlantı yok |
| NO CSI | Wi-Fi bağlı, güncel geçerli CSI yok |
| LEARNING / CALIBRATE | Referans öğreniliyor; yüzde gösterilir |
| STILL / MOVING | Güncel skor eşiğin altında / hareket algılandı |
| NO LINK | İki saniyedir UART telemetrisi gelmedi |
| UART BUSY | Seri port başka bir servis tarafından kullanılıyor |

Uzun süreli yanlış alarm oranı, router yeniden bağlantısı, yeniden başlatma
sonrası ayar kalıcılığı ve UART kaynak devri henüz fiziksel olarak doğrulanmadı.
Mesafe, yön, kişi sayısı, nefes veya nabız ölçülmez.

## Ham kayıt ve destek

Kart USB ile PC'ye bağlıyken ana klasörden:

```powershell
python tools/csi_usb.py COM5 record still.log --seconds 60 --raw
python tools/csi_usb.py COM5 record moving.log --seconds 60 --raw
```

Araç mevcut dosyanın üzerine yazmaz. Destek isterken firmware/API sürümünü,
kart modelini, durumu ve skor/eşik değerlerini belirtin; Wi-Fi şifrenizi paylaşmayın.
