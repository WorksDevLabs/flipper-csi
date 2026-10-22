# Cihaz doğrulama kaydı ve test formu

## Mevcut bildirim

1.5: kullanıcı önceki kontrol listesinin tamamını PASS bildirdi; sayısal skor,
10 yürüyüş sonucu, zaman damgası veya gerçek kayıt paylaşılmadı.
2.0: kullanıcı hareket/durgunluk, titreşim (Mute kapatılınca), canlı waterfall,
Klasik/Dayanıklı ve manuel/AUTO algılamasını bildirdi. Son arayüzü beğendiğini ve
bu hâli son sürüm istediğini belirtti. Bu, aşağıdaki tüm uzun testlerin geçtiği
anlamına gelmez. Yazılım testleri ayrı BUILD_EVIDENCE.md içindedir.

| Test | Yapılacak işlem | Beklenen / kaydedilecek sonuç |
|---|---|---|
| T1 Durgunluk | Yerleşim ve eşik sabit, 30 dakika hareketsiz ortam | Yeni hareket başlangıcı sayısı; önceki hedef en fazla 1 |
| T2 Geçiş | Aynı yoldan 10 yürüyüş | 2 saniyede algılanan sayısı; önceki hedef en az 9/10 |
| T3 Ayar kaydı | Sessiz/dil/profil/eşik değiştir; SD kaydını bekle; eşik için menüden karta kaydet; kapat/aç | 2.0'da sessiz/dil/profil SD'de korunmalı. Kaydedilmiş ESP eşik/Wi-Fi korunmalı. Açılışta HAZIR beklemesi ve alarmın kapalı olması normaldir |
| T4 Kesinti | Router ve ardından ESP RESET | Eski ölçüm güncel görünmemeli. ESP yeniden başladıysa tekrar kullanıcı kalibrasyonu gerekir |
| T5 UART devri | CSI'dan çık; kullandığın başka UART/devboard uygulamasını aç/kapat; CSI'a dön | Port düzgün bırakılmalı. Labmate yalnızca böyle bir uygulamaya örnektir; kurulu değilse şart değildir. CSI firmware'ine alakasız komut gönderme |
| T6 Alarm | Aç/kapat, gecikme/bekleme/bırakma/tekrar, sessiz/test, veri kesintisi | Sonuç ve görülen durumları ayrı yaz |
| T7 SD/A-B | İki kayıt, karşılaştırma, SD yok/dolu | Donma olmamalı; hata açık olmalı; güvenli yedek test kartı kullan |
| T8 Uzun çalışma | 1.5 için 2 saat; 2.0 için 8 saat ve 3 yerleşim | Donma/reset, belleğin başlangıç/bitişi, UART/DSP/kuyruk ve hata değerleri |

Her satıra PASS/FAIL/YAPILMADI, sürüm, yerleşim, filtre, T, RSSI ve süre yazın.
Gerçek sayılar paylaşılmadan bir sayı üretilmez. 2.0 ölçüm doğruluğu, 3 yerleşim,
8 saat, SD hata/migrasyon ve kapsamlı son alarm/A-B sonuçları açık tutulur.
