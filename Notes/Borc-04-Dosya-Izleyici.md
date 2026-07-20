# 0.4 — Dosya izleyici

Faz 22'nin bıraktığı açık: varlığın kimliği artık GUID, GUID→yol tablosu
`AssetManager`'da. Explorer'dan yapılan bir değişiklik bu tabloyu
bayatlatıyordu ve tutarlılık tamamen içerik panelinin doğru çağrı
yapmasına bağlıydı.

## Ne yapıldı

`FX::FileWatcher` (`Engine/include/FXEngine/Core/FileWatcher.h`) — bir
klasörü alt klasörleriyle izler, değişiklikleri biriktirir.

İçerik paneli her karede `Poll()` edip olayları `AssetManager`'ın zaten
var olan `Register` / `OnAssetMoved` / `OnAssetDeleted` çağrılarına
bağlıyor. Yeni bir varlık mantığı yazılmadı — sadece diskteki gerçek,
tabloya iletildi.

## Kararlar

**Olaylar arka thread'de işlenmiyor.** Thread yalnızca kuyruğa yazıyor;
`AssetManager` ve paneller thread-safe değil ve öyle olmalarını
istemiyoruz. Kilit yalnızca kuyruğun kendisinde.

**Sessizlik bekleme (debounce, 0.25 sn).** Tek bir dosya kopyalaması bile
birden fazla olay üretiyor (created + modified + modified...). Son
olaydan sonra kısa bir sessizlik oluşmadan hiçbir şey işlenmiyor; yarım
yazılmış dosyayı taramaya çalışmıyoruz.

**Removed + Added çifti taşıma sayılıyor.** Windows aynı klasördeki ad
değişikliğini `RENAMED_OLD_NAME`/`NEW_NAME` olarak veriyor ama klasörler
arası taşımayı bazen iki ayrı olaya bölüyor. Aynı yığında aynı dosya adı
için bir Removed ve bir Added varsa bunu taşıma sayıyoruz — yoksa dosya
yeni bir GUID alır ve sahnedeki referans kopardı. İzleyicinin var olma
sebebi tam olarak bu.

**Tampon taşarsa tam tarama.** `ReadDirectoryChangesW` sıfır uzunlukta
bildirim verirse ne değiştiğini bilmiyoruz demektir; o durumda
`ScanProject()` çağrılıyor. Sessizce yanlış olmaktansa pahalı olmak iyi.

**`.meta` olayları yok sayılıyor.** `.meta` dosyalarını zaten varlığın
yanında biz yönetiyoruz; olaylarını işleseydik varlığı silinmiş sanıp
GUID'i atardık.

**Panel gizliyken de işleniyor.** Varlık tablosunun diskle tutarlılığı,
bir ImGui penceresinin görünür olmasına bağlı olamaz.

## Test (gerçek `Game1` projesi, editör açıkken)

| Dışarıdan yapılan | Sonuç |
|---|---|
| `Sprite-0001.png` → `Kahraman.png` yeniden adlandırma | `.meta` yeni adla yazıldı, **GUID aynı** (`6643441972245405231`), eski `.meta` silindi |
| Sahneyi aç | Inspector `assets/textures/Kahraman.png` gösteriyor — sahne dosyası hiç değişmedi, GUID yeni yola çözüldü |
| Yeni `.png` kopyalama | `.meta` otomatik üretildi |
| O dosyayı silme | Öksüz `.meta` temizlendi |
| Eski adına geri döndürme | GUID yine korundu |

## Bilerek yapılmayanlar

- **Linux/macOS gövdesi boş.** `inotify` / `FSEvents` ayrı bir iş;
  izleyici olmadan da editör çalışıyor, sadece "Yenile" gerekiyor.
- **Modified olayları kullanılmıyor.** Shader hot reload (18d) ve doku
  yeniden yükleme bunları tüketecek; şu an tüketici yok.
- **Kök `assets` varsayılıyor** — `AssetDirectory` borcu 0.6'da.
