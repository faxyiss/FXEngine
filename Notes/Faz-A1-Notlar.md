# A1 — Component meta-veri sistemi

> Karar K2'nin uygulanması (bkz. [02-Mimari.md](02-Mimari.md)).
> Aşama A'nın ilk işi; A2–A5'i de ucuzlatıyor.

## Sorun

Yeni bir component eklemek **dört yere** dokunmayı gerektiriyordu ve
biri unutulduğunda hata **sessiz** oluyordu:

| Yer | Unutulursa |
|---|---|
| `Components.h` — struct | — |
| `AllComponents` tip listesi | "Play'e geçince component kayboldu" |
| `EntitySerialization.cpp` | "Kaydedip açınca ayar gitti" |
| `SceneHierarchyPanel.cpp` | Component Inspector'da görünmez |

Üç ayrı liste vardı ve er geç ayrışacaklardı.

## Çözüm

`ComponentRegistry` — her component **bir kez** tarif ediliyor; kopyalama,
serileştirme ve Inspector çizimi bu tarifden üretiliyor.

```cpp
ComponentRegistry::Register<VelocityComponent>("Velocity", "Velocity")
    .Field<&VelocityComponent::Linear>("Linear",  "Dogrusal").Speed(0.1f)
    .Field<&VelocityComponent::Angular>("Angular", "Acisal (der/s)")
        .Degrees().Speed(5.0f);
```

Tek dokunulan yer: `Engine/src/Scene/ComponentMeta.cpp` → `RegisterBuiltins`.

### Alan erişimi: neden `offsetof` değil

İlk tasarım alan offset'i saklıyordu. Ama `offsetof` yalnızca
**standart-layout** tiplerde tanımlı; `SpriteRendererComponent`
(`shared_ptr`) ve `TagComponent` (`std::string`) bunu garanti etmiyor.
Tanımsız davranışa güvenmek yerine üye işaretçisi **şablon argümanı**
olarak alınıyor:

```cpp
template<auto Member>
ComponentBuilder& Field(const char* name, const char* label);
```

Derleyici tipli ve tanımlı bir erişimci üretiyor (`void*(*)(void*)`),
yanlış üye verilirse **derlenmiyor**. C# köprüsünün ihtiyacı olan şey —
alanın adresi ve tipi — yine elde.

### Kanca noktaları ve gerekçeleri

| Kanca | Neden var |
|---|---|
| `SaveExtra` / `LoadExtra` | `SpriteRenderer.Texture` alan tablosuyla ifade edilemiyor: bellekte `shared_ptr`, dosyada GUID |
| `DrawExtraUI` | Doku slotu, kamera tekilliği, script listesi. `std::function` — editör kendi durumunu yakalamak zorunda |
| `OnAdded` | `Follow`, `Velocity` olmadan **sessizce** çalışmıyor; eksiği kendisi tamamlıyor |
| `CustomUI()` (alan) | Alan serileştirilir ama genel çizici çizmez (Camera `Primary`, script adı) |
| `Structural()` | `Tag`/`Relationship`/`WorldTransform`: dosyada entity düzeyinde yazılıyorlar, ama **kopyalamaya katılmak zorundalar** |

`Structural()` A1'in kendi kuralına uymanın sonucu: bu üçünü tablodan
çıkarsaydım `Scene::Copy` için ikinci bir liste doğardı — çözülen hatanın
ta kendisi.

### Kayıt otomatik

`GetAll()` / `Find()` ilk erişimde `RegisterBuiltins()`'i çağırıyor.
"Çağırmayı unuttun" diye yeni bir sessiz hata sınıfı yaratmamak için.

## Dosya formatı korundu

**Tek satır sürüm artışı yok.** Alan adları (`"Translation"`, `"Rotation"`,
`"TilingFactor"`…) dosya formatının parçası olarak tabloya yazıldı.

Bunu iddia etmek yerine **teste sabitledim**:
`Tests/ComponentMeta.test.cpp` → *"Uretilen JSON bilinen sahne formatiyla
ayni"* her alanı tek tek kontrol ediyor. Bu test kırmızı yanarsa dosya
formatı bozulmuş demektir.

Bir davranış **iyileşti**: eksik alanların varsayılanı artık
component'in kendi varsayılanından geliyor (`AddOrReplace` ile
default-construct edip üzerine yazıyoruz). Eskiden fallback değerleri
serializer'da ikinci kez yazılıydı (`t.value("Scale", …, vec2(1))`) ve
component'in varsayılanıyla ayrışabilirdi.

## Doğrulama

| | Önce | Sonra |
|---|---|---|
| `FXTests` | 41 test / 145 assertion | **48 test / 223 assertion** |
| Derleme | temiz | temiz |
| Editör açılışı | — | 6 sn ayakta, çökme yok |

Eklenen 7 testin en önemlisi A1'in **asıl iddiasını** ölçüyor:
test içinde `HealthComponent` diye bir component tanımlanıp **yalnızca
tabloya** kaydediliyor; serileştirme, geri yükleme ve `Scene::Copy`
üçü de fazladan tek satır yazılmadan çalışıyor.

## Satır sayıları

| Dosya | Değişim |
|---|---|
| `EntitySerialization.cpp` | 254 → 154 (elle yazılmış bloklar gitti) |
| `SceneHierarchyPanel.cpp` | 845 → 466 |
| `ComponentMeta.h` + `.cpp` | +466 (yeni) |
| `ComponentDrawer.cpp` | +406 (panelden taşınan + genel çizici) |
| `Components.h` | `AllComponents` tip listesi silindi |

Toplam satır arttı — ama tekrar eden üç liste tek tarife indi ve yeni
component eklemenin maliyeti dört yerden **bire** düştü.

## Elle test edilmesi gerekenler (GUI)

1. Bir entity seç → Inspector'da Transform, Sprite Renderer, Velocity,
   Camera, Follow, Native Script başlıkları eskisi gibi mi?
2. **Transform'da 'X' düğmesi olmamalı** (kaldırılamaz), diğerlerinde olmalı.
3. Dönme alanı **derece** göstermeli, kaydedilen dosyada **radyan** olmalı.
4. Sprite Renderer → renk seçici, tiling, doku slotu; içerik panelinden
   doku sürükle-bırak; "Doku Ayarları" ağacı.
5. Camera → "Birincil"i işaretle; **diğer kameraların işareti kalkmalı**.
6. Follow → hedef seçici entity adlarını listelemeli; Velocity'yi silince
   turuncu uyarı ve "Velocity Ekle" düğmesi çıkmalı.
7. Native Script → açılır liste kayıtlı script'leri göstermeli.
8. "Component Ekle" menüsü **zaten olanları göstermemeli**; Follow
   eklenince Velocity de gelmeli.
9. Bir component'i 'X' ile kaldır → çökme olmamalı.
10. **Eski bir sahneyi aç, kaydet, tekrar aç** — hiçbir ayar kaybolmamalı.

## Yapılmadı

- **Script alanlarının Inspector'dan ayarlanması** (planın 5. maddesi).
  `ScriptableEntity` türevlerinin alan bildirmesi ayrı bir iş; altyapı
  (alan tablosu) artık hazır ama script örnekleri yalnızca Play'de
  yaşadığı için "Edit modunda neyi düzenliyoruz?" sorusunun cevabı
  tasarlanmalı. A4/Aşama B ile birlikte ele alınmalı.
- **Çoklu seçimde çok hedefli düzenleme.** Tablo bunu artık mümkün
  kılıyor (aynı alanı N component'e yazmak) ama Inspector hâlâ yalnızca
  birincili düzenliyor. Teknik borç listesinde duruyor.
- `Int` ve `Vec4` alan tipleri yazıldı ama motor component'lerinde
  kullanılmıyor; `HealthComponent` testi `Int`'i kapsıyor.
