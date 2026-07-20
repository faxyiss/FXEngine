# Faz 22 — AssetManager: GUID tabanlı varlık kimliği

Faz 12'den beri açık duran ve her fazda biraz daha büyüyen borç:
**bir varlığın kimliği dosya yoluydu.**

İçerik paneline taşıma özelliği eklendiğinde bu somut bir tuzağa dönüştü —
kullanıcıya bir araç verip aynı anda "kullanırsan bir şeyler bozulur"
demek zorunda kaldık. Bu faz o çelişkiyi kapatıyor.

## Dosyalar

| Dosya | Değişiklik |
|---|---|
| **`Asset/Asset.h`** (yeni) | `AssetHandle`, `AssetType`, `AssetMetadata`, `TextureImportSettings` |
| **`Asset/AssetManager.h/.cpp`** (yeni) | Varlık veritabanı, `.meta` okuma/yazma |
| `Core/Project.cpp` | Proje açılınca tara, kapanınca temizle |
| `Scene/EntitySerialization.cpp` | Yol yerine GUID |
| `Scene/SceneSerializer.cpp` | Sürüm 4 |
| `Renderer/TextureLibrary.*` | Ayarlar `.meta`'dan |
| `Panels/ContentBrowserPanel.*` | `.meta` taşıma/gizleme, olay bildirimi |

---

## 1. Kimlik ile konum ayrı şeylerdir

Bu ilkeyi **üçüncü kez** uyguluyoruz:

| Faz | Nerede | Ayrım |
|---|---|---|
| 8 | Entity | `UUID` ≠ `entt::entity` |
| 21 | Proje | kurulu olduğun yer ≠ çalıştığın yer |
| 22 | Varlık | **GUID ≠ dosya yolu** |

Her varlığın yanında bir `.meta` dosyası duruyor:

```json
{
    "Version": 1,
    "Guid": 7311204458821736401,
    "Type": "Texture",
    "Texture": { "Nearest": true, "Repeat": true, "GenerateMipmaps": true }
}
```

Dosya taşındığında `.meta` de onunla gidiyor; GUID değişmiyor. Sahne
dosyası GUID sakladığı için referans sağlam kalıyor.

Unity'nin `.meta` dosyaları tam olarak budur; Unreal aynı bilgiyi
`.uasset` başlığında tutar.

---

## 2. Neden tam tarama, tembel arama değil?

Proje açılırken varlık klasörü bir kez baştan sona geziliyor.

Tembel yükleme (istendiğinde ara) daha ucuz görünüyor ama **"GUID 123
hangi dosya?"** sorusuna cevap vermek için yine tüm ağacı taramak
gerekirdi. Bir kez tara, tabloda tut.

`.meta`'sı olmayan varlığa tarama sırasında GUID atanıp dosyası yazılıyor.
Tanımadığımız uzantılara (`.txt`, `.ini`) `.meta` **üretilmiyor** — her
dosyaya GUID atamak `.meta` çöplüğü yaratırdı.

### Aynı GUID iki dosyada

Kullanıcı bir varlığı `.meta`'sıyla birlikte kopyalarsa iki dosya aynı
kimliği taşır. Sessizce birini kaybetmek yerine ikincisine yeni kimlik
verip uyarıyoruz.

---

## 3. Sürüm 4: eski sahneler kırılmıyor

```json
"SpriteRenderer": {
    "TextureHandle": 2276166757417568440,
    "TexturePath": "assets/textures/zemin.png"
}
```

`TexturePath` **sadece insan okusun diye** yazılıyor; yükleme onu
kullanmıyor. Dosyayı elle inceleyen biri "GUID 8412..." yerine ne olduğunu
görebilsin.

Sürüm ≤3 dosyalar aynen açılıyor: eski `"Texture"` alanındaki yol okunup
kütüphaneye veriliyor. Kaydedildiğinde dosya yeni biçime geçiyor —
**sessiz, kademeli geçiş.** Toplu bir dönüştürme aracı gerekmedi.

GUID bulunamazsa `TexturePath` ipucusuna düşülüyor: sessizce düz renk
göstermektense elimizdeki en iyi bilgiyi kullanmak daha faydalı.

---

## 4. Doku ayarları artık dosyaya ait

Bu fazın ikinci kazancı. Önceden ayarlar **ilk yükleyen çağırandan**
geliyordu:

```cpp
Load("checkerboard.png", tilingSpec);   // Repeat isteyen
Load("checkerboard.png");               // varsayilan isteyen -> CAKISMA
```

İkinci çağıran sessizce ilkininkini alıyordu ve `TextureLibrary` bunu
uyarı basarak haber vermek zorunda kalıyordu. İçerik paneli bir klasöre
her girişinde bu uyarı çıkıyordu (yanlış alarm olarak).

Ayar `.meta`'ya taşınınca **"kim önce istedi" sorusu tümden ortadan
kalktı**: bir dosyanın ayarı her zaman kendi `.meta`'sında yazılı olandır.

`TextureLibrary::Load` iki imzadan tek imzaya indi, uyarı silindi.
`checkerboard.png.meta` artık `"Repeat": true` taşıyor — zemin döşemesi
için gereken ayar, dokunun kalıcı bir özelliği.

---

## 5. Kaldırılan uyarılar

Bu fazın en somut göstergesi, **silinen** satırlar:

- Taşımada: *"bu öğeleri kullanan sahneler onları bulamayacak"*
- Yeniden adlandırmada: *"Bu dosyayı kullanan sahneler onu bulamayacak."*
- `TextureLibrary`: *"farklı bir spec ile istendi"*

> Doğru olmayan bir uyarı, uyarı olmamasından daha kötüdür.

Yeniden adlandırma modalinde yerine şu yazıyor: *"Referanslar korunur
(kimlik .meta'da)."*

---

## 6. `.meta` senkronizasyonu içerik panelinin sorumluluğu

Tablo ile disk arasındaki tutarlılık bu çağrılara bağlı:

| İşlem | Yapılan |
|---|---|
| Taşıma | `.meta` birlikte taşınır + `OnAssetMoved` |
| Yeniden adlandırma | aynısı |
| Silme | `.meta` silinir + `OnAssetDeleted` |
| İçe aktarma | `Register` (GUID hemen hazır olsun) |

Unutulan bir çağrı "GUID var ama dosya başka yerde" durumuna yol açar.
Bu kırılganlık gerçek: doğrusu bir dosya izleyici (`ReadDirectoryChangesW`)
— teknik borç listesinde duruyor.

`.meta` dosyaları panelde **gizleniyor**: onlar varlığın kendisi değil,
hakkındaki bilgi. Ama diskte duruyorlar ve **sürüm kontrolüne girmeliler**
— `.meta` kaybolursa varlık yeni kimlik alır ve referanslar kopar.

---

## Test sonuçları

**Kimlik kalıcılığı** (geçici öz-test):
```
tarama          : 2 varlik, 2 yeni .meta
player.png GUID : 2252418160778898138
tasima sonrasi  : GUID ayni, yol=assets/karakterler/player.png
KIMLIK KORUNDU  : EVET
yeniden tarama  : GUID .meta'dan geri okundu, ayni
silme           : kayit + .meta temizlendi
```

**Uçtan uca referans** (asıl sınav):
```
zemin.png sahneye baglandi
dosyada         : "TextureHandle": 2276166757417568440   (Version 4)
textures/ -> kaynaklar/ tasindi
sahne yeniden yuklendi
===> TEXTURE BULUNDU MU: EVET   (yeni yol=assets/kaynaklar/zemin.png)
```

**Doku ayarları:**
```
AssetManager: 5 varlik (4 yeni .meta)
"farkli bir spec" uyarisi: YOK
```

## Onay
- [x] Derlendi (uyarısız)
- [x] `.meta` oluşturma, okuma, taşıma, silme
- [x] Taşıma referansı kırmıyor (uçtan uca)
- [x] Sürüm ≤3 sahneler açılıyor, kaydedince 4'e geçiyor
- [x] Doku ayarları `.meta`'dan, spec uyarısı yok
- [ ] Kullanıcı onayı

## Kalanlar
- **Inspector'da doku ayarları** — `.meta` şu an elle düzenleniyor.
  `AssetManager::UpdateTextureSettings` ve `TextureLibrary::Reload`
  hazır, sadece arayüz eksik.
- **Dosya izleyici** — dışarıdan (Explorer'da) yapılan değişiklikleri
  tablo görmüyor.
- **Prefab ve sahne referansları** hâlâ yol tabanlı (`StartScene`,
  prefab içindeki sahne bağları). Aynı mekanizma onlara da uygulanabilir.
- **Varlık tarayıcıda GUID gösterimi** — hata ayıklama için faydalı olur.
