# 0.6 — Faz 22 artıkları

Faz 22 `AssetManager`'ı getirdi ama üç ucu açık bıraktı. Bu adım onları
kapattı.

## 1. Inspector'da doku ayarları

`.meta` dosyasındaki `TextureImportSettings` (Nearest / Repeat / Mipmap)
artık Inspector'dan değiştirilebiliyor — daha önce `.meta`'yı elle
düzenlemek gerekiyordu. Altyapı (`UpdateTextureSettings`, `Reload`) Faz
22'de hazırdı, eksik olan sadece arayüzdü.

**Ayar sprite'a değil DOSYAYA ait** ve bu ekranda yazıyor. Aynı dokuyu
kullanan her entity aynı filtrelemeyi görür — Faz 22'nin kararı buydu.

**Yeniden yüklenen doku sahneye dağıtılıyor.** `TextureLibrary::Reload`
önbelleği tazeliyor ama entity'ler eski `shared_ptr`'ı tutuyor;
`ReplaceTextureInScene` aynı yolu kullanan tüm `SpriteRenderer`'ları
geziyor. Alternatifi `AssetHandle -> Texture` dolaylı katmanı eklemekti,
o da bütün çağrı yerlerini değiştirmek demekti.

## 2. `StartScene` artık GUID

`.fxproject` **sürüm 2**: `StartScene` bir yol değil `AssetHandle`.
Başlangıç sahnesinin dosyasını taşımak artık projeyi bozmuyor.

Sürüm 1 dosyaları açılıyor ve ilk açılışta sessizce dönüştürülüyor
(tarama sonrası yol → GUID, sonra `Save()`).

Ayrıca **"Sahne > Baslangic Sahnesi Yap"** eklendi: bu ayarı yapacak bir
yer hiç yoktu, `.fxproject`'i elle düzenlemek gerekiyordu.

> **Test bir hata yakaladı.** İlk yazdığım okuma kodu
> `j.value("StartScene", uint64_t{0})` çağırıyordu; eski dosyalarda değer
> string olduğu için nlohmann istisna atıyor ve **proje hiç açılmıyordu**.
> Elle test etseydim muhtemelen yeni bir projeyle deneyip fark etmezdim.
> 0.5'in bedelini ilk kullanışta çıkardığı yer burası.

## 3. `AssetDirectory` gerçekten kullanılıyor

İçerik paneli kökü ve olay yolları artık `.fxproject`'teki
`AssetDirectory`'den geliyor, sabit `"assets"` değil. `AssetManager`
zaten config'e bakıyordu; panel bakmayınca ikisi farklı klasörü doğru
sanabilirdi.

## 4. Görünüm tercihi kaydediliyor (bonus)

İçerik panelinin Izgara/Liste tercihi `editor.json`'a yazılıyor. Küçük
ama her proje açılışında tekrarlayan bir rahatsızlıktı.

## Zaten kapalıymış: prefab referansları

Yol haritasında "prefab referansları hâlâ yol tabanlı" yazıyordu ama
`PrefabSerializer` entity'leri ortak `Detail::SerializeEntity` ile
yazıyor — yani prefab içindeki doku referansları Faz 22'den beri GUID.
Madde eskimiş; kaldırıldı.

Prefab'ın **kaynağıyla bağlantısı** (örnek güncellenmesi, override
sistemi) hâlâ yok, ama o ayrı bir borç.

## Test

| Ne | Sonuç |
|---|---|
| Inspector'dan "Repeat" işaretleme | `.meta` diske yazıldı, doku yeniden yüklendi, geri alındı |
| "Baslangic Sahnesi Yap" | `.fxproject` sürüm 2 + GUID; sahnenin `.meta` GUID'i ile eşleşiyor |
| Projeyi yeniden açma | Başlangıç sahnesi otomatik yüklendi |
| Birim testleri | 29 test / 93 assertion (3'ü yeni: `Project.test.cpp`) |

`Project.test.cpp`, sahne taşındıktan sonra bile başlangıç sahnesinin
bulunduğunu doğruluyor — 0.6'nın asıl vaadi bu.
