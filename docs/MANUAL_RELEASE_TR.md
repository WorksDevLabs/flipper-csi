# GitHub Release — son yayın adımı

Kaynak commit ve dosyaları doğrulayın. Yayımlanmış v1.0.0 etiketini veya eski
asset'leri değiştirmeyin. 1.5 kaynağı dev/v1.5-pro dalında korunur.

2.0 için Releases → Draft a new release:

1. Yeni etiket: v2.0.0. Target: yeni final main commit'i.
2. Başlık: CSI Radar Ultimate v2.0.0 — E11 / API 87.1.
3. Açıklama: kökteki RELEASE_NOTES.md dosyasının tamamı.
4. Ekler: release-output içindeki csi_radar.fap, flipper-csi-v2.0.0.zip ve SHA256SUMS.txt.
5. Previous tag: v1.0.0 (veya arada gerçekten yayımlanmış 1.5 varsa v1.5.0).

Kaynak dosyalarını yüklemek, tag veya Release oluşturmak değildir. Son yayın
notundaki fiziksel test sınırlarını kaldırmayın; tüm uzun testler geçmiş gibi
yazmayın. Açık kabul kapıları nedeniyle ön sürüm olarak yayımlamak uygundur;
kararlı olarak işaretlemek cihaz sonuçlarının kapsamını genişletmez.

Actions kullanılabiliyorsa Actions → CI build and tests → Run workflow, main.
Bu oturumda workflow başlatılmadı; daha önceki hesap engeli nedeniyle yerel
kanıtlar kullanılabilir. Run sonucu yoksa başarılı CI rozeti/iddiası eklemeyin.
