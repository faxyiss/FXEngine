# Faz 8 — UUID + Kalıcı Entity Referansları

## Dosyalar
| Dosya | İş |
|---|---|
| `Core/UUID.h/.cpp` | 64-bit rastgele kalıcı kimlik |
| `Scene/Components.h` | `IDComponent`, `EntityRef`, `FollowComponent` |
| `Scene/Scene.h/.cpp` | UUID haritası, `FindEntityByUUID`, `CreateEntityWithUUID` |
| `Scene/Systems.h/.cpp` | `FollowSystem` — referansların kanıt sistemi |
| `Scene/SceneSerializer.cpp` | UUID yaz/oku, format sürüm 2 |
| `Editor/Panels/…` | UUID gösterimi, hedef seçici |

## İki tür kimlik

| | `entt::entity` | `FX::UUID` |
|---|---|---|
| Ömür | Bu çalıştırma | **Sonsuza dek** |
| Nerede yaşar | Bellekte | Diskte, referanslarda |
| İş | Hızlı erişim | Kalıcı kimlik |
| Diske yazılır mı | **Hayır** | Evet |

Faz 7'de bu ayrımın yokluğunu bizzat yaşadık: yüklemeden sonra
`m_PlayerEntity`'yi körlemesine temizlemek zorunda kaldık.

## Tasarım kararları

### Neden 64-bit, 128 değil?
"Gerçek" UUID'ler (RFC 4122) 128 bittir. Bizim çapımızda (tek kullanıcı,
tek sahne, binlerce entity) 64 bit çarpışma olasılığı pratik olarak sıfır
ve `uint64_t` taşımak, hash'lemek, JSON'a yazmak çok daha ucuz.

### 0 = geçersiz
`entt::null` gibi. Varsayılan yapıcı **rastgele üretir** — "boş UUID
istiyorum" demek için açıkça `UUID(0)` yazman gerekir. Yeni bir entity'nin
kimliksiz kalması kazara bile mümkün olmamalı.

Dağılım 0 üretebilir (olasılık 1/2⁶⁴). Tek satırlık `while` ile kapattık:

> **"Pratikte hiç olmaz" ile "olamaz" farklı şeylerdir.** Bulunması imkânsız
> hataların kaynağı tam olarak bu tür ihmallerdir.

### `IDComponent` neden component, `Entity` içinde alan değil?
`Entity` sadece bir **tutamak** — veri tutmaz, registry'de yaşamaz.
Kalıcı olması gereken her şey component olmalı; ancak o zaman
serileştirilir, kopyalanır, sorgulanır.

### `EntityRef` Scene'i bilmiyor
Sadece bir UUID taşıyor. Çözümleme `Scene::FindEntityByUUID` ile çağıran
tarafta yapılıyor — böylece `Components.h`, `Scene.h`'ı include etmek
zorunda kalmıyor (dairesel bağımlılık olurdu).

### UUID → entity haritası
`unordered_map<UUID, entt::entity>`. Registry'yi gezip `IDComponent`
karşılaştırmak da işe yarardı ama **O(n)** olurdu. Binlerce entity'nin
olduğu sahnede her karede takip hedefi aramak dayanılmazdı.

**Bedeli:** harita ile registry'nin senkron kalma sorumluluğu. Bu yüzden
entity oluşturma/silme **sadece Scene üzerinden** yapılmalı.
`GetRegistry()`'nin açık olması bu riski taşıyor — ileride daha dar bir
erişim gerekebilir. *(Teknik borç listesine eklendi.)*

## Geç çözümleme (lazy resolve) — iki ayrı faydası

`FollowSystem` UUID'yi **her karede** yeniden çözüyor. "Bir kez çözüp
saklasak daha hızlı olmaz mı?" Olurdu, ama:

**1. Ölü referansı anında görünür kılar.** Hedef silinirse elimizde ölü
bir tutamak kalırdı ve fark etmezdik. Geç çözümlemede arama başarısız
olur, sistem sessizce atlar.

**2. Yükleme sırasını önemsizleştirir.** Hedef entity dosyada bizden
*sonra* gelebilir. İşaretçi çözseydik **iki geçişli** bir yükleyici
yazmak zorunda kalırdık (önce tüm entity'ler, sonra referanslar).

> Bu, "erken optimizasyon yapma" kuralının doğru uygulaması: önce doğru
> olanı yap, profiling darboğaz gösterirse önbellekle.

## Sistem sırası değişti

```cpp
FollowSystem::Update(...);    // hedefe doğru HIZ yazar
MovementSystem::Update(...);  // o hızı KONUMA uygular
```

Ters olsaydı takip her zaman bir kare gecikmeli çalışırdı.
Faz 5'te "Scene'in varlık sebebi sistem sırasıdır" demiştik — ilk somut
örneği bu.

`FollowSystem` diğerlerinden farklı olarak `registry` değil **`Scene`**
alıyor, çünkü UUID haritası Scene'de yaşıyor. Bu bir tasarım gerilimi:
sistemler ideal olarak sadece registry'ye bağımlı olmalı (test kolaylığı).
Kabul edip açıkça yazdık.

## Serileştirme

Format **sürüm 2**. Sürüm 1 dosyaları hâlâ açılıyor: `ID` alanı yoksa
yeni UUID üretilir, kullanıcı uyarılır.

> Faz 7'de `Version` alanını "bugün kullanmıyoruz ama sonradan eklemek
> çok zor" diye koymuştuk. İlk faydasını bir faz sonra gördük.

### Yükleme akışı
```cpp
m_Scene->Clear();                    // registry.clear() DEĞİL — harita da temizlenmeli
...
CreateEntityWithUUID(dosyadaki_id)   // CreateEntity() DEĞİL — kimlik korunmalı
```

İkisi de Faz 8'in getirdiği zorunluluklar. Birincisini atlarsak harita
ölü kayıtlar tutar; ikincisini atlarsak tüm referanslar kopar.

## Arayüz kararları

- **UUID salt okunur.** Değiştirmek referansları koparır. Ama *görünmesi*
  gerekiyor → tıklayınca panoya kopyalanıyor.
- **Hedef seçici entity'yi ADIYLA gösteriyor, UUID saklıyor.** Arayüz
  okunabilir olmalı, veri modeli kalıcı — ikisi aynı şey olmak zorunda değil.
- **Kayıp hedef açıkça gösteriliyor:** `<kayip: 90038...>`. Sessizce
  gizlemek kullanıcıyı neden çalışmadığını anlayamaz hâlde bırakırdı.
- **Kendini takip etme seçeneği hiç sunulmuyor.** Faz 6'daki ilkenin devamı:
  arayüz geçersiz eylemi mümkün kılmamalı.
- **"Follow" eklenince "Velocity" de otomatik ekleniyor.** `FollowSystem`
  üç component istiyor; eksik olursa takip *sessizce* çalışmaz.

## Test sonuçları

**1. Kaydet → yükle turu**
```
Sahne kaydedildi: ... (28 entity)
Sahne yuklendi:   ... (28 entity)
Oyuncu yuklemeden sonra UUID ile bulundu: 9003828664407198896
```
Son satır Faz 8'in tamamının kanıtı. Faz 7'de bu imkânsızdı.

**2. Dosyadaki referanslar**
```
Oyuncu UUID : 9003828664407198896
Takipci 0   hedef=9003828664407198896 -> ESLESIYOR
Takipci 1   hedef=9003828664407198896 -> ESLESIYOR
Takipci 2   hedef=9003828664407198896 -> ESLESIYOR
```

**3. Geriye dönük uyumluluk** (elle sürüm 1'e çevrilmiş dosya)
```
WARN  Sahne surumu 1 (Faz 7). UUID'ler yeniden uretilecek...
INFO  Sahne yuklendi: ... (28 entity)
WARN  Oyuncu bulunamadi - sahne dosyasinda yok olabilir.
```
Üç davranış da doğru: uyardı, yükledi, kaybı açıkça bildirdi.

## Onay
- [x] Derlendi (ilk denemede)
- [x] UUID'ler kaydediliyor ve korunuyor
- [x] Referanslar yüklemeden sağ çıkıyor
- [x] Sürüm 1 dosyaları hâlâ açılıyor
- [ ] Kullanıcı onayı → Faz 9'a geçiş
