# Faz 5 — EnTT Entegrasyonu (ECS)

## Dosyalar
| Dosya | İş |
|---|---|
| `Scene/Components.h` | Tag, Transform, SpriteRenderer, Velocity — **saf veri** |
| `Scene/Entity.h` | `entt::entity` + `Scene*` üzerine ince sarmalayıcı |
| `Scene/Scene.h/.cpp` | Registry sahipliği + sistem çalışma sırası |
| `Scene/Systems.h/.cpp` | MovementSystem, SpriteRenderSystem — **saf davranış** |

## ECS gerçekte neyi çözer?

Klasik OOP'de `class Player : public Character : public Entity` gibi bir
miras ağacı kurarsın. Sonra "uçan ama ateş edemeyen düşman" gerekir ve
ağaç çöker. **Kalıtım cehennemi.**

ECS bunu tersine çevirir: kalıtım yok, **kompozisyon** var.

| Parça | Ne | Kural |
|---|---|---|
| **Entity** | Sadece bir kimlik numarası | Veri yok, davranış yok |
| **Component** | Saf veri | **Sadece veri**, metot yok |
| **System** | Belirli component'ler üzerinde çalışan mantık | **Sadece davranış**, durum yok |

"Uçan ama ateş edemeyen düşman" artık bir sınıf değil, bir
**component kombinasyonu**: `Transform + Sprite + Velocity`, `Weapon` yok.

## Tasarım kararları

### Component'te metot olur mu?
`TransformComponent::GetTransform()` var. Bu bir **dönüşüm**, davranış değil:
aynı veriyi başka biçimde sunar, hiçbir şey değiştirmez, hiçbir sisteme
bağımlı değil.

> **Sınır çizgisi:** bir metot *başka* component'lere veya sahneye
> dokunuyorsa, o bir **system'e** aittir.

### Neden Velocity ayrı bir component?
Transform'un içine `velocity` alanı koyabilirdik. Koymadık çünkü
**her entity hareket etmez** — duvarlar, zemin, dekorlar sabittir.

Ayrı component olunca `MovementSystem` sadece hareket edenler üzerinde gezer.
10.000 duvarın arasındaki 5 hareketli nesneyi ararken hiçbir duvara
**dokunmaz bile**.

> ECS'te "hangi component'ler bir arada" sorusu bir **performans kararıdır**.

### Rotation neden radyan?
Matematik fonksiyonları (`sin`, `cos`, `glm::rotate`) radyan bekler.
İç veriyi radyan tutup **sadece arayüzde** dereceye çevirmek, her hesapta
dönüşüm yapmaktan hem hızlı hem hatasız. (Faz 6'da Inspector derece gösterecek.)

### Scale neden `vec2`?
2D'de Z ölçeği anlamsız. Her entity'de 4 bayt tasarruf, ve "bu 2D bir motor"
mesajını kodun kendisi veriyor.

### Entity neden ince bir sarmalayıcı?
entt'nin ham API'si her çağrıda registry + id taşımayı gerektirir:
```cpp
registry.emplace<TransformComponent>(id, ...);   // ham
entity.AddComponent<TransformComponent>(...);    // sarmalayıcı
```
İşlevsel bir şey eklemiyor, **sadece çağrı yerlerini okunur kılıyor**.
Motor mimarisinde bu tür ince sarmalayıcılar yaygın ve değerlidir.

`Scene*` **ham işaretçi**, akıllı işaretçi değil — Entity sahneye
*sahip olmaz*, referans verir. `shared_ptr` olsaydı dairesel sahiplik
oluşur ve sahne hiç yok edilemezdi.

### Neden her entity Tag + Transform alıyor?
Teknik olarak Transform'suz entity mümkün. Ama editörde her şeyin adı ve
konumu olması hayatı kolaylaştırır. Unity, Godot, Hazel de aynı tercihi yapar.
**Kural değil, yaygın ve pratik bir varsayım.**

### Scene neden var? (registry'yi doğrudan kullanabilirdik)
Çünkü Scene **sistemlerin çalışma sırasını** garanti eden yer:
```
Input -> Movement -> Physics -> Collision -> Script
```
Çarpışma, hareketten **sonra** çalışmalı; yoksa bir kare gecikmeli tespit
yaparsın ve nesneler duvarlara girer. Bu sıra bir yerde tanımlı olmalı.

### Sistemler neden `static` (durumsuz)?
Aynı girdi → aynı çıktı. Bu, onları **tek başlarına test edilebilir** kılar:
sahte bir registry oluşturup sistemi çalıştırmak yeterli, motorun geri
kalanına gerek yok.

## `view` — ECS'in en önemli aracı

```cpp
auto view = registry.view<TransformComponent, VelocityComponent>();
```

İki şey birden yapar:
1. **Filtreleme** — sadece ilgili entity'ler
2. **Önbellek dostu erişim** — EnTT bu component'leri bitişik dizilerde tutar,
   üzerlerinde gezmek RAM'de düz bir yürüyüştür

> Ama bunu **performans için değil, mimari için** yapıyoruz.
> Performans hoş bir yan etki. Erken optimizasyon kuralı hâlâ geçerli.

## Oyuncu özel bir sınıf değil

`UpdateCameraMovement` oyuncunun konumunu **doğrudan değiştirmiyor**;
sadece `VelocityComponent`'e yazıyor. Hareketi `MovementSystem` yapıyor —
tıpkı diğer 38 entity gibi.

Girdi sistemi veriyi yazar, hareket sistemi onu işler. Sistemler birbirini
**bilmez**, sadece aynı veriye dokunurlar. **ECS'te sistemler arası
iletişim böyle olur.**

## Karşılaşılan hata

### `'.in_use' sol tarafı bir sınıf/yapı/birleşim içermelidir`
İki ayrı sorun aynı satırda:
1. entt 3.13'te `registry.storage<T>()` **işaretçi** döndürür, referans değil
2. `in_use()` bu sürümde **kullanımdan kalkmış** → `free_list()`

```cpp
const auto* pool = m_Registry.storage<entt::entity>();
return pool ? static_cast<std::uint32_t>(pool->free_list()) : 0u;
```

`size()` değil `free_list()`: `size()`, silinmiş ama kimliği geri
dönüştürülmek üzere saklanan entity'leri de sayar. EnTT kimlikleri
yeniden kullanır.

## Editor kodu ne kadar sadeleşti?

| Faz | `OnRender` içeriği |
|---|---|
| 3 | ~60 satır: transform kur, uniform yaz, DrawIndexed × 5 |
| 4 | ~40 satır: iç içe döngü, DrawQuad çağrıları |
| 5 | **1 satır**: `m_Scene->OnRender(*m_Camera);` |

`OnUpdate` de tek satır. İleride Physics, Collision, Script sistemleri
eklenecek ve **bu satır değişmeyecek** — sadece `Scene::OnUpdate` genişleyecek.

## Test: ECS'in kalbi
- **H** → Oyuncunun `SpriteRenderer`'ını kaldır. Görünmez olur ama
  **var olmaya devam eder**, hareket etmeyi sürdürür.
- **M** → `Velocity`'yi kaldır. `MovementSystem` artık bu entity'yi
  **hiç görmez**, WASD etkisiz kalır. Ama hâlâ çizilir.

> OOP'de bunun karşılığı, nesnenin **sınıfını çalışırken değiştirmek**
> olurdu ki mümkün değildir.

## Test sonucu
```
Sahne kuruldu: 40 entity
```
(1 zemin + 1 oyuncu + 8 uydu + 30 gezgin). GL hatası yok, assert tetiklenmedi.

## Onay
- [x] Derlendi
- [x] Çalıştı, 40 entity, hata yok
- [ ] Kullanıcı onayı → Faz 6'ya geçiş
