# CSI Radar — mevcut cihazda kısa kabul testleri

Temel hareket/durgunluk, titreşim ve eşik uygulaması kullanıcı tarafından
çalışıyor olarak bildirildi. Aşağıdaki testlerin sonuçları henüz alınmadı.
Aynı ortam ve kart yerleşiminde yapın; her test için PASS/FAIL ve gözlemi yazın.

| Test | Yapılacak işlem | Beklenen / kaydedilecek sonuç |
| --- | --- | --- |
| T1 — Durgunluk | Kartı sabit bırakın, 5 dakika odada hareket olmasın. | Kendiliğinden kaç MOVING başlangıcı oldu? İlk hedef 0; sayı kaydedilir. |
| T2 — Tekrarlanabilirlik | Router-kart yoluna yakın aynı yerden 10 kez yürüyün; aralarda STILL olmasını bekleyin. | Kaçı MOVING oldu, yaklaşık gecikme neydi? Hedef en az 9/10; bu bir doğruluk garantisi değildir. |
| T3 — Ayar kaydı | T değerini değiştirin, sağa uzun basıp onayı bekleyin; uygulamadan çıkıp kartın enerjisini kesip açın. | Aynı T ve Wi-Fi geri geliyor mu? Sessiz modun mevcut 1.0'da yeniden açılışta sıfırlanması normaldir. |
| T4 — Router kesintisi | Kendi router'ınızın Wi-Fi'sini yaklaşık 30 saniye kapatıp açın. | WIFI OFF / NO CSI ve sonra yeniden öğrenme görülmeli. Eski MOVING/STILL geçerli gibi kalmamalı. Toparlanma süresini yazın. |
| T5 — UART devri | CSI Radar'dan Back ile çıkıp Labmate UART'ı açın, sonra kapatıp CSI Radar'a dönün. | Port alınabiliyor ve CSI Radar yeniden veri alabiliyor mu? Labmate'tan CSI firmware'ine rastgele komut göndermeyin. |

İlk etapta T1–T3 yeterli kısa geri bildirimdir; T4–T5 ayrıca yapılabilir.
T1/T2 sırasında kalibrasyonu hareketsiz ortamda bir kez yapın. Her denemede eşiği
rastgele değiştirmeyin; eşik ve RSSI/paket hızını not edin.

Yanıt örneği: `T1: 0 alarm; T2: 10/10; T3: PASS; T=0.120; rate=20/s`.
Bu örnek sonuç değildir. Bir test FAIL ise gerçek bulgu olarak saklanır.
