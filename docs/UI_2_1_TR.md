# CSI Radar 2.1 — arayüz güncellemesi

**Yalnızca FAP değişir.** ESP yazılımı, protokol 3, partition ve NVS aynı kalır.
`ready/csi_radar.fap` dosyasını `/ext/apps/GPIO/csi_radar.fap` üzerine kopyalayın.
Geri dönmek için 2.0 klasöründeki `ready/csi_radar.fap` yeterlidir.

## Ana ekran
- Üst şerit, odanın öbür ucundan okunacak tek bilgidir: DURGUN / HAREKET / ALARM!.
  Hareket varken şerit ters renge döner; alarm tetiklenince yanıp söner.
- Şeridin sağında durum etiketleri (SD!, REC, ALM, SES-) ve sinyal çubukları yer alır.
- Ölçüm hazır değilken grafik yerine dört adımlı liste görünür: Kart, WiFi, CSI,
  Kalibre. Alttaki satır sıradaki adımı söyler.
- Kalibrasyon sırasında bir ilerleme çubuğu gösterilir.
- Ölçüm sırasında alt satırda skor, eşik işaretli bir skor çubuğu ve eşik değeri
  (T0.120; otomatik modda sonuna A eklenir) yer alır.
- Alarm kuruluyken alt satır "Son hareket: 12sn" ve tetiklenme sayısını gösterir.

## Ekranlar ve tuşlar
- Sağ tuş 6 ekranı sırayla gezer; başlıkta 2/6 … 6/6 yazar. Sıra: Canlı →
  Waterfall → Bağlantı → Tanılar → Oturum → Son hareketler. Sağ tuşun etiketi
  "Mode" yerine "Ekran" oldu.
- Orta tuş yalnızca bir işi olduğunda görünür: Kal (kalibrasyon), Sifirla
  (Oturum ve Son hareketler ekranlarında), Kur / Kapat (Alarm panelinde).
- Son hareketler: RTC'ye göre gerçek saat ve hareketin süresi gösterilir. Süren
  hareketin sonunda "+" vardır.
- Tanılar: ham kodlar metne çevrildi (WiFi: Sifre? / AP yok, Kalibrasyon:
  Gurultulu / Kesinti / Hiz dusuk). SD kaybı olduğunda ilk satırda o görünür.
- Bildirimler ikinci ekranlarda tablonun üstünde kısa süre görünen bir kutuda çıkar.
- Yukarı/Aşağı ile eşik değiştirildiğinde "Esik 0.130 (elle)" bildirimi çıkar.

## Oturumun SD karta kaydedilmesi
- Oturum özeti (hareket sayısı, ölçüm süresi, hareketli süre, en yüksek skor) ve
  son 8 hareket `apps_data/csi_radar/session.bin` dosyasına yazılır; bir önceki
  kopya `session.bak` olarak durur. Dosya CRC16 korumalıdır.
- Kayıt: değişiklik varsa 30 saniyede bir, uygulama kapanırken ve "Sıfırla" ile hemen.
- Açılışta önceki oturum yüklenir ve "Önceki oturum yüklendi" bildirimi çıkar.
- Hareket saatleri RTC saniyesi olarak saklanır; Flipper yeniden başlasa da doğru
  görünür. Kapalıyken geçen süre ölçüm süresine eklenmez. Yüklenen oturum yeni
  verilerle "kesintisiz" sayılmaz.
- Sıfırlamak için Oturum ya da Son hareketler ekranında OK (Sıfırla).

## Alarm sesi
- Kurulu alarm tetiklenince yaklaşık 2,5 saniyelik bir ses çalar: iki kez yükselen
  do-mi-sol, ardından uzun bir sol (kırmızı LED ve çift titreşime ek olarak).
- Menu → Alarm → "Alarm sesi: açık/kapalı". Varsayılan açık. Ses düzeyi Flipper'ın
  kendi ses ayarına uyar; Flipper sessiz moddaysa ses çıkmaz.
- "Alarmı dene" artık gerçek tetiklenmenin aynısını (LED + titreşim + ses) verir.
- Alarm panelinde "Titr. / Ses: Var/Yok" satırı ikisini birlikte gösterir.
- Ana ekrandaki titreşim kapalı etiketi "SES-" yerine "TITR-" / "VIB-" oldu.
- Ayar, `settings.bin` bayrak baytının 4. bitindedir (8). 2.0 dosyaları değişmeden okunur.

## Menüler
- Ana menüden "Analiz ekranlari" kaldırıldı; bu ekranlar sağ tuşla geziliyor.
- Ölçüm ayarlarındaki yinelenen "WiFi adini elle gir" kaldırıldı; WiFi menüsünde duruyor.
- Eşik menü maddesi değeri de gösterir: "Esik: elle 0.120".
- Alarm menüsünde: "Alarm paneli" ve yeni "Hareket bildirimi: acik/kapali".

## Alarm ve bildirim ayrımı
- Alarm kurulu değilken her hareket başlangıcında verilen tek titreşim + mavi LED
  artık "Hareket bildirimi" ayarına bağlıdır. Varsayılan açıktır (2.0 ile aynı).
- Kurulu alarm tetiklenince çift titreşim + kırmızı LED verir. Titreşim kapalıyken
  (sessiz izleme) yalnızca kırmızı LED yanar.
- Ayar, `settings.bin` içindeki bayrak baytının 3. bitinde saklanır. 2.0 dosyaları
  değişmeden okunur. 2.1'de bu ayar kapatılırsa 2.0 FAP o dosyayı reddeder ve
  varsayılanları kullanır.

## Doğrulama
- ufbt / mntm-012 SDK, API 87.1: derleme ve APPCHK temiz.
- `tools/run_tests.sh` ASan/UBSan ile geçti. Yeni testler: olay süreleri ve
  ayar bitinin geriye uyumluluğu.
- Flipper'da (ESP kartı takılı değilken) tüm ekranlar EN ve TR olarak gezildi;
  görüntüler `docs/screens-2.1/` klasöründe.
- ESP kartı takılıyken canlı veriyle görüldü (`docs/screens-2.1/live/`):
  - hazırlık listesindeki işaretler, kalibrasyon ilerleme çubuğu,
  - DURGUN ve ters renkli HAREKET şeridi, grafik ve skor çubuğu,
  - waterfall, RTC saatli geçmiş, çözülmüş tanılar,
  - alarm kurma geri sayımı, ALM etiketi ve "Son hareket" satırı.
- Canlı testten sonra yapılan düzeltmeler:
  - WF/FILT/CFG gibi iç komutlar artık "Gönderiliyor / ESP32 onayladı" kutusu
    göstermiyor.
  - Süreler daha ayrıntılı gösteriliyor (1m40s, 1dk40sn).
  - "CALIBRATING" başlıkta kesilmiyor.
- **Açık kalanlar:** alarm tetiklenince yanıp sönen şerit ve kırmızı LED / çift
  titreşim, gerçek bir tetiklenmeyle henüz görülmedi.
