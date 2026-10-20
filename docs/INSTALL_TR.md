> Mevcut 0.2 kullanıcıları: yalnızca yeni ready/csi_radar.fap dosyasını kopyalayın.
> ESP32 ve E11 firmware'ini yeniden yüklemeyin. Aşağıdaki flash adımları ilk kurulum içindir.
> Bu rehberin tarihsel “cihazda doğrulanmadı” notları için güncel README.md kanıt tablosunu esas alın.

# CSI Radar 1.0.0 — E11 / API 87.1 sürümü

Flipper Zero + resmi ESP32-S2 Wi-Fi Developer Board için.
Bu sürüm hareket skoru üretir; mesafe, yön, kişi sayısı veya hareketsiz insan varlığı ölçmez.

## Derleme durumu
- ready/csi_radar.fap: Momentum mntm-012 SDK, F7, API 87.1; derleme ve APPCHK geçti.
- ESP32 dosyaları: ESP-IDF v5.5, ESP32-S2, 4 MB flash, USB CDC; derleme geçti.
- Kullanıcının E11 ekranı: mntm-dev 16-10-2026, d3f89dfe, API 87.1.
  Bu kimlik Labmate E11 kayıtlarındaki tabanla eşleşiyor. FAP standart Momentum
  SDK'sıyla derlendi; özel E11 SDK'sıyla yeniden derlenmiş değildir. API numarası
  eşleşmesi cihaz kabul testi yerine geçmez. Açılış/UART/kapanış testi kullanıcıda.
- Mevcut E11 Flipper firmware'ini değiştirmeyin. Bu pakette Flipper firmware güncellemesi yok.

## Windows'ta yükleme
1. ready/csi_radar.fap dosyasını qFlipper dosya yöneticisiyle SD kartta
   /ext/apps/GPIO/csi_radar.fap konumuna kopyalayın. Labmate'ı kapatın.
2. ESP32 kartını Flipper'dan çıkarıp USB ile PC'ye bağlayın.
   BOOT'a basılıyken RESET'e basıp bırakın, sonra BOOT'u bırakın.
3. Terminalde esptool ve seri port aracını kurun:

   py -m pip install esptool==4.12.0 pyserial==3.5

4. Terminali bu paketin ready klasöründe açın. Aygıt Yöneticisi'nden kartın
   COM portunu bulun; aşağıdaki COM5 yerine o portu yazın:

   py -m esptool --chip esp32s2 --port COM5 --baud 460800 write_flash --flash_mode dio --flash_size 4MB --flash_freq 80m 0x1000 bootloader.bin 0x8000 partition-table.bin 0x10000 flipper_csi_radar.bin

   Bu işlem ESP32 kartının mevcut yazılımını değiştirir; Flipper'ın E11 yazılımını değiştirmez.
   Kartta korunacak başka firmware varsa önce onun yedeğini alın.
5. Başarılı yazma/doğrulama sonrası RESET'e basın. Kartı USB'den çıkarıp Flipper'a takın.
6. Flipper: Apps > GPIO > CSI Radar. OK'a uzun basıp Wi-Fi adını ve şifresini girin.
   Klavyedeki Save ile alanı onaylayın; ikinci alanı onaylayınca ESP32'ye gönderilir.
   Şifre giriş ekranında görünür; telemetriye yazılmaz. ESP32 NVS'inde şifresiz saklanır.
7. 2.4 GHz WPA2 veya uyumlu karma ağ kullanın. İlk test için router kanal genişliğini
   20 MHz seçin. Saf WPA3, açık ağ, captive portal ve kurumsal ağ bu paketle doğrulanmadı.
8. STILL görünene kadar bekleyin. Oda hareketsizken kısa OK ile 100 geçerli paketlik
   kalibrasyon yapın. 20 paket/saniyede yaklaşık 5 saniye; paket azsa daha uzun sürer.

## Tuşlar
| Tuş | İşlev |
| --- | --- |
| Kısa OK | Kalibrasyon |
| Uzun OK | Wi-Fi adı ve şifresi |
| Yukarı / aşağı | Eşik değiştir |
| Sol | Bilgi / grafik |
| Kısa sağ | Titreşim/LED uyarısını sessize al/aç |
| Uzun sağ | Aktif eşiği ESP32'ye kalıcı kaydet |
| Geri | Çıkış; Wi-Fi ekranında iptal |

Wi-Fi bilgisi ve kaydedilen eşik ESP32'dedir. Sessiz mod yeniden açılışta sıfırlanır.
SSID en çok 32 bayt; şifre 8–63 bayt. Flipper klavyesinde yazılamayan karakterler
varsa USB kurulum aracını kullanın. UART: ESP TX GPIO43 -> Flipper RX pin14;
ESP RX GPIO44 <- Flipper TX pin13; ortak GND. Resmi kart takılınca bağlantı hazırdır.

## Durumlar ve ilk cihaz testi
- SETUP WIFI: ağ bilgisi girin.
- WIFI OFF: bağlantı kuruluyor; şifreyi ve 2.4 GHz ağını kontrol edin.
- NO CSI: Wi-Fi bağlı, geçerli CSI yok. Router ping yanıtı ve 20 MHz ayarını kontrol edin.
- LEARNING / CALIBRATE: referans öğreniliyor; yüzdesi gösterilir.
- STILL / MOVING: güncel ölçüm sonucu.
- NO LINK: ESP32 UART telemetrisi 2 saniyedir gelmiyor.
- UART BUSY: seri port başka bir servis tarafından kullanılıyor.

Test: açılış -> Wi-Fi -> STILL -> kalibrasyon -> hareket -> STILL.
Sonra router'ı kapatın: eski STILL/MOVING geçerli kalmamalı. Router'ı açın:
yeniden bağlantı ve öğrenme görülmeli. Uygulamadan çıkıp tekrar açın;
Labmate UART gibi başka bir seri uygulama portu alabilmeli.
İlk ölçümleri yazarken uygulama açılıyor mu, durum, paket/saniye, RSSI ve
hareketsiz/hareketli skor aralıklarını paylaşın. Eşik 0.120 başlangıç değeridir,
her ortamda doğru olduğu iddia edilmez.

## USB araçları
ESP32'yi USB ile PC'ye bağlayın; RESET sonrası COM numarası değişebilir.
Paketin ana klasöründen:

    py tools/csi_usb.py COM5 setup
    py tools/csi_usb.py COM5 ping
    py tools/csi_usb.py COM5 record still.log --seconds 60 --raw
    py tools/csi_usb.py COM5 record moving.log --seconds 60 --raw

setup şifreyi terminalde gizli sorar. Kayıt aracı mevcut dosyayı ezmez.
RAW, USB üzerinden en fazla 10 Hz IQ kaydıdır; Flipper UART'a gönderilmez.

## Kaynaktan derleme
ESP-IDF v5.5 ortamında esp32 klasöründe:

    idf.py set-target esp32s2
    idf.py build

Flipper: eşleşen SDK ile flipper_app klasöründe ufbt.
Bu paketteki FAP'ın SDK kaynağı:
https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-012/flipper-z-f7-sdk-mntm-012.zip

    ufbt update --hw-target f7 --url https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-012/flipper-z-f7-sdk-mntm-012.zip
    ufbt

Linux/macOS host testleri: tools/run_tests.sh
ASan/UBSan: SANITIZE=1 tools/run_tests.sh
Kısıtlı /proc ortamlarında LeakSanitizer çalışmıyorsa ASAN_OPTIONS=detect_leaks=0 kullanın.

## Yapılan değişiklikler ve sınırlar
- Ping API düzeltildi, Wi-Fi callback'i hesaplama yerine kuyruk kullanıyor.
- Sabit alt taşıyıcı indeksleri; geçersiz ilk kelime dalgalanmasında sürekli öğrenme sorunu giderildi.
- Paket genelindeki kazanç normalize ediliyor, pilot/DC/koruma taşıyıcıları dışlanıyor.
- Kalibrasyon tam istenen geçerli paket sayısında bitiyor.
- CSI akışı ile UART bağlantısının sağlık durumu ayrıldı; eski skor temizleniyor.
- İki yönlü CRC16, sıkı alan sınırları, uzun/bozuk satırdan güvenli toparlanma.
- Flipper kaynak kapatma sırası, seri port meşgul ekranı, komut yanıt/zaman aşımı.
- Wi-Fi kurulum ekranı, NVS eşik kaydı, ham USB kayıt aracı.

Mevcut sentetik testler ve yeni regresyon testleri geçti. ASan/UBSan denetiminde
hata görülmedi (ortam kısıtı nedeniyle leak detection kapalıydı). 200.000 baytlık
bozuk seri akış testi geçti. Gerçek RF başarımı, Flipper ekran yerleşimi, USB flash,
E11 üzerinde UART kaynak paylaşımı ve uzun süreli kullanım cihazda doğrulanmadı.

Kaynaklar:
- https://github.com/sehmusadsoy-ui/flipper-labmate/blob/dev/v1.8-completion/docs/V1_8_E11_FINAL_RELEASE_GATE_2026-10-17.md
- https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s2/api-guides/wifi.html
- https://cdn.flipperzero.one/Flipper_Zero_WI-FI_Module_V1_Schematic.PDF
