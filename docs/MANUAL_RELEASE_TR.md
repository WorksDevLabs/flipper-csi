# Workflow olmadan v1.0.0 GitHub Release yayını

Depodaki hazır dosyalar workflow olmadan indirilebilir. Bu belge, mevcut
uygulama sürümünün ayrıca GitHub Releases bölümünde yayımlanması içindir.
GitHub API bağlantısı bu oturumda Release oluşturma/yükleme işlemi sunmadığından
Release kaydının oluşturulduğu iddia edilmez.

1. GitHub'da oturum açıp depoda **Releases → Draft a new release** seçin.
2. **Choose a tag → v1.0.0 → Create new tag**. Hedef mevcut `main` olsun.
   Etiket zaten varsa değiştirmeyin; önce mevcut sürümü kontrol edin.
3. Başlık: **CSI Radar v1.0.0 — E11 / API 87.1**.
4. Açıklamaya depodaki [RELEASE_NOTES.md](../RELEASE_NOTES.md) içeriğini koyun.
5. [ready/csi_radar.fap](../ready/csi_radar.fap) ve hazırlanmış dağıtım ZIP'i ile
   bunlara ait SHA256 listesini ekleyin. ZIP hazır değilse kaynak+hazır binariler
   depodadır; yerelde `python tools/package_release.py` ile paketlenebilir.
   Önce kökteki `SHA256SUMS.txt` dosyasını kontrol edin.
6. **Set as a pre-release** seçeneğini işaretlemeyin: bu sürümün kararlı kapsamı
   yalnızca notlarda belirtilen E11/API 87.1 + resmi kart kurulumudur.
7. İçeriği kontrol edip **Publish release** seçin. Eski etiket/dosyalar değiştirilmez.

Actions erişimindeki engel manuel yayını da etkiliyorsa yayın beklemede kalır;
yol haritası ve kaynaklara erişim ayrı bir durumdur. Parola/token paylaşmayın.
