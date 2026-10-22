# CSI Radar — 1.0 → 1.5 → 2.0 Ultimate

Bu proje tam **üç yayımlanacak sürüm** üzerinden tamamlanır: **v1.0.0, v1.5.0,
v2.0.0 Ultimate**. 0.2 geliştirme geçmişidir; dördüncü ürün sürümü değildir.
Ara geliştirme çalışmaları branch/commit olarak yürütülür; ek özellik sürümü
veya ayrı roadmap oluşturulmaz. Yayımlanan etiket ve dosyalar sonradan değiştirilmez.
Bir sonraki sürüme geçmek, mevcut sürümün test sonucunu gizlemek için kullanılmaz.

**Hedef:** resmi ESP32-S2 Wi-Fi Devboard ve Flipper Zero ile kullanışlı,
ölçümü anlaşılır, iyi kaydedilebilen ve ayarlanabilen bir Wi-Fi hareket analiz aracı.
Ultimate, bu donanımdan kanıtlanabilir şekilde elde edilebilen en iyi deneyimi
hedefler; hayali bir hassas radar iddiasını değil.

## Üç sürümün özeti

| Sürüm | Ürün odağı | Durum |
| --- | --- | --- |
| **1.0 — Core** | Hareket algılama, kalibrasyon, canlı grafik, titreşim ve güvenilir seri iletişim | v1.0.0 yayımlandı; temel cihaz işlevleri kullanıcıda çalışıyor. |
| **1.5 — Pro** | Kolay kurulum, kaliteli ölçüm, profiller, SD kayıt ve bağlantı dayanıklılığı | Yazılım/yerel derlemeler hazır; gerçek kayıt ve cihaz kabul kapıları açık |
| **2.0 — Ultimate** | Çoklu analiz ekranı, oturum karşılaştırma, gelişmiş filtreleme ve bütünleşik alarm deneyimi | Zorunlu yazılım kapsamı uygulandı; fiziksel ve gerçek kayıt doğrulama kapıları açık |

## v1.0.0 — Core / mevcut temel

- [x] ESP32-S2 CSI genliğinden normalize edilmiş hareket skoru.
- [x] Sabit alt taşıyıcı eşlemesi, yumuşatma ve histerezis.
- [x] Flipper grafiği, görünür skor/eşik, STILL / MOVING, kalibrasyon yüzdesi.
- [x] Hareket başlangıcında titreşim/LED, sessiz mod.
- [x] Wi-Fi kurulumu ve ESP32 üzerinde eşik kaydı.
- [x] Wi-Fi, CSI ve UART sağlığının ayrılması; eski ölçümün geçersiz kılınması.
- [x] CRC16 protokolü, bozuk/uzun satır kontrolleri, USB ham CSI kayıt aracı.
- [x] İkon, kaynaklar, MIT lisansı, hazır FAP/firmware ve kurulum rehberi.
- [x] Host testleri ve yerel SDK derlemeleri.
- [x] Kullanıcı bildirimi: kurulum, Wi-Fi, kalibrasyon, hareket/durgunluk geçişi,
  titreşim ve eşik ayarı çalışıyor; ikonlu sürüm genel olarak çalışıyor bildirildi.
- [ ] Uzun durgunluk, eşik kalıcılığı, router yeniden bağlantısı ve UART kaynak
  devri testlerinin ayrı sonuçları kaydedilecek.

**Kapsam:** E11-patched Momentum d3f89dfe / API 87.1 ve resmi kart üzerinde
bildirilen sonuçlar. Başka firmware/API için ayrı derleme ve fiziksel doğrulama
olmadan genel uyumluluk etiketi konmaz. [Cihaz testleri](docs/DEVICE_TESTS_TR.md).

## v1.5.0 — Pro / günlük kullanım ve ölçüm kalitesi

**Geliştirme durumu:** Yazılım uygulandı; [madde bazlı durum](docs/PRO_1_5_STATUS.md) ve [cihaz testi rehberi](docs/PRO_1_5_TR.md). Gerçek oda kayıtları, 30 dakika/10 geçiş ve 2 saat kapıları açık. Kararlı v1.5 etiketi henüz yok.

### Zorunlu kapsam

- **Kurulum:** boşluklu SSID için sorunsuz giriş; ağ tarama/seçme; şifreyi gizleme;
  açık bağlantı/şifre hataları; USB kurulum yolunu koruma.
- **Arayüz:** okunaklı ana panel, Türkçe/İngilizce yardım, açık tuş ipuçları,
  skor/eşik açıklaması ve uygulama/ESP firmware/protokol sürümü ekranı.
- **Profiller:** sessiz oda / normal / hareketli ortam başlangıç profilleri ve
  kullanıcı profili; sessiz mod dahil ayarların tutarlı kaydı.
- **Kalibrasyon:** gürültü ve örnek kalitesini kontrol etme; yetersiz veriyi kabul
  etmeme; elle eşik ve isteğe bağlı otomatik eşik. Otomatik mod kullanıcıya
  görünür olmalı, ölçüm hatasını normal sonuç gibi göstermemeli.
- **Dayanıklılık:** Wi-Fi kopması, paket kesilmesi ve ESP yeniden başlamasından
  toparlanma; sürüm el sıkışması; komut onayı/zaman aşımı; güvenilir UART bırakma.
- **SD kayıt:** zaman damgalı oturum/olay CSV'si, başlat/durdur, dosya döndürme,
  SD yok/dolu hataları. Kayıt işi algılama ve UART alımını bekletmemeli.
- **Tekrarlanabilir analiz:** mevcut ham kayıtları yeniden oynatan PC aracı;
  sürümlü veri biçimi, gerçek durgun/hareketli örnekler, karşılaştırılabilir rapor.
- **Uyumluluk:** E11 profilini koruma; standart Momentum/resmi firmware için
  ancak ayrı derleme ve cihaz testi geçerse açıkça etiketlenmiş FAP seçenekleri.

### Yayın kapısı

Belirlenmiş tek test yerleşiminde hedef: **30 dakika durgunlukta en fazla 1 yanlış
hareket başlangıcı; 10 kontrollü geçişin en az 9'unu 2 saniye içinde algılama**.
Bunlar henüz elde edilmiş sonuç veya her oda için garanti değildir. Yerleşim,
eşik, paket hızı, mesafe ve gerçek sayılar rapora yazılır.

Wi-Fi/CSI kaybı güncel ölçüm sanılmamalı; 10 kapat/aç ve router yeniden bağlantı
senaryosu geçmeli. Eşik/profil kalıcılığı ve SD hata davranışı doğrulanmalı.
E11 + resmi kart üzerinde en az **2 saat** oturumda kilitlenme/taşma görülmemeli.
Gerekli bir test başarısızsa 1.5 geliştirme branch'inde düzeltilir; etiket atılmaz.

## v2.0.0 — Ultimate / bütünleşik analiz aracı

### Zorunlu kapsam

- **Analiz ekranları:** ana panel, skor trendi, gerçek alt taşıyıcı verisiyle
  waterfall, olay zaman çizelgesi ve bağlantı/örnek kalitesi ekranı. Animasyon
  ve dekoratif radar noktaları konum ölçümü gibi sunulmaz.
- **Gelişmiş algılama:** kayıtlar üzerinde karşılaştırılan medyan/MAD gibi
  dayanıklı filtreler, zamansal değişim özellikleri ve parazit bastırma;
  1.5 algoritmasına karşı önce/sonra ölçümü. Daha karmaşık olmak başarı sayılmaz.
- **Ölçüm güvenilirliği:** hareket skoru ile veri kalitesini ayrı gösterme;
  zayıf/eksik örneklerde açık uyarı. Kalibre edilmemiş bir yüzdeye "insan olasılığı"
  adı verilmez. Ham ve işlenmiş veri ilişkisi belgelenir.
- **Oturum merkezi:** cihazda kayıt listesi, kısa özet ve iki oturumun A/B
  karşılaştırması; PC'de dışa aktarma, yeniden oynatma ve grafik raporu.
- **Alarm modu:** ayarlanabilir bekleme/bırakma süreleri, tekrar uyarı aralığı,
  sessiz izleme, kullanıcı başlatmalı alarm testi ve Flipper'ın uyku davranışının
  açık kontrolü. Normal grafikten yanlışlıkla alarm kurulmaz.
- **Son ürün kalitesi:** tutarlı ikon/menüler, taşmayan ekranlar, kurulum asistanı,
  bellek/CPU/UART bütçesi, sınırlı kuyruklar ve görünür hata teşhisi.
- **Tek paket:** eşleşen FAP + ESP firmware + kurulum + bütünlük değerleri +
  sürümlü protokol/migrasyon notları; mevcut çalışan ayarların korunması.

### Donanım ve kapasiteye bağlı seçenekler

1.5 ölçümleriyle kapsam dondurulurken aşağıdakilerden yalnızca test edilebilenler
Ultimate'a alınır; ana işlevler bunlara bağımlı olmaz:

- Kullanıcı tarafından açılan, yerel ağda kimlik doğrulamalı, salt okunur canlı
  web paneli; Flipper/SD kaydıyla birlikte yük altında doğrulama.
- İkinci ESP32 ile kontrollü gönderici/alıcı modu. Ek donanım gerektirir ve
  router trafik değişkenliğini azaltmayı amaçlar; tek kartta varmış gibi sunulmaz.

Bu seçenekler başarı kanıtı olmadan "tamamlandı" işaretlenmez ve yeni bir sürüm
numarası açılarak kapsam uzatılmaz. Çıkarılan seçenek ve nedeni belgelenir.

### Ultimate yayın kapısı

Aynı kayıtlar ve yerleşimlerde 1.5 ile karşılaştırma: yanlış alarm azalırken
algılama başarımı korunmalı; tek bir örneğe göre iyileşme ilan edilmemeli.
En az **3 farklı yerleşimde** durgun/hareketli test, **8 saatlik** kayıt/alarm
soak testi, SD dolu/yok, router/ESP yeniden başlatma ve ayar migrasyonu kontrolleri.
UART bant genişliği, bellek üst sınırı, örnek kaybı ve ekran güncelleme hızı ölçülür.
Her desteklenen firmware/kart için sonuçlar ayrı tutulur. GitHub Actions olmadan
aynı yerel komutlar, dosya hash'leri ve cihaz sonuçlarıyla yayın hazırlanabilir.

## Bilinçli sınırlar

Tek ESP32-S2 + router ile gerçek menzil/açı, kesin kişi sayısı, duvar arkasının
haritası, kimlik tespiti veya hareketsiz kişi varlığı vaat edilmeyecek.
Nabız/nefes ölçümü bu üç sürümün teslim kapsamına dahil değildir.
Çoklu ESP kullanılması tek başına hassas konumlama kanıtı değildir.
Mevcut E11 firmware'i değiştirilmeyecek; cihaz sahibi testi ve açık uyumluluk
kaydı olmadan yeni firmware/API destekleniyor denmeyecek.

## Yayın yöntemi ve tamamlanma tanımı

Her sürüm: dondurulmuş kaynak commit'i, eşleşen binariler, SHA256, değişiklik
notları, uyumluluk tablosu ve gerçek test sonuçları içerir. Kaynak dosyasının depoda
bulunması, bir GitHub Release kaydı oluşturulduğu anlamına gelmez. Workflow
zorunlu değildir; [manuel yayın adımları](docs/MANUAL_RELEASE_TR.md) kullanılabilir.

Bu roadmap ilerideki işler için tek bağlayıcı listedir. İşaretli maddeler gerçek
kanıta, işaretsiz maddeler plana karşılık gelir; 1.5 kaynağı ayrı dalda korunur ve kullanıcı tüm testleri PASS bildirmiştir, 2.0 zorunlu özellikleri uygulanmış son özellik sürümüdür; fiziksel kabul kapsamı BUILD_EVIDENCE.md dosyasında açıklanır. 2.0 kapıları tamamlandığında üç sürümlük ürün hedefi tamamlanır.



## 2.0 uygulama durumu

Zorunlu özellik grupları uygulandı: [durum matrisi](docs/ULTIMATE_STATUS.md).
Gerçek kayıtlarla performans karşılaştırması ve fiziksel yayın kapıları açık.
İsteğe bağlı web/ikinci kart kapsam dışı kararı kullanım rehberinde belgelendi.

## Son yayın kapsamı

Kullanıcı mevcut 2.0 hâlini son sürüm olarak kabul etti. Bu karar, açık fiziksel
hedefleri gerçekleşmiş saymaz; sayısal sonuç olmadan performans artışı veya
8 saat/3 yerleşim başarısı yazılmaz. Ayrı doğrulama kaydı korunur.
