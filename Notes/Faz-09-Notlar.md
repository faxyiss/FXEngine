# Faz 9 — Parent / Child Hiyerarşisi

## Dosyalar
| Dosya | Değişiklik |
|---|---|
| `Scene/Components.h` | `RelationshipComponent`, `WorldTransformComponent` |
| `Scene/Entity.h` + **`Entity.cpp` (yeni)** | `SetParent`, `GetChildren`, `IsAncestorOf` |
| `Scene/Systems.*` | `TransformSystem`, render artık dünya matrisini kullanıyor |
| `Scene/Scene.cpp` | Özyinelemeli silme, sistem sırası |
| `Renderer/Renderer2D.*` | `DrawQuad(mat4, …)` aşırı yükleri |
| `SceneSerializer.cpp` | `Parent` alanı, iki geçişli yükleme, sürüm 3 |
| `Panels/SceneHierarchyPanel.*` | Ağaç görünümü, sürükle-bırak |

## Yerel / dünya uzay ayrımı

Bir entity'nin `TransformComponent`'i artık **yerel** dönüşümdür — parent'ına
göre. Ekranda göründüğü yer ise **dünya** dönüşümüdür:

```
dünya = parent.dünya × yerel
```

`TransformSystem` bunu her karede **kökten yaprağa** hesaplayıp
`WorldTransformComponent`'e yazar. Render sistemi artık `TransformComponent`'e
hiç bakmıyor.

### Neden kökten yaprağa (top-down)?
Bir çocuğun dünya matrisi, parent'ınki hesaplanmadan bulunamaz. Tüm
entity'ler üzerinde düz gezseydik bir çocuğu parent'ından önce
işleyebilirdik ve dünya matrisi **bir kare geride** kalırdı — sprite
titremesi olarak görünür.

Bu yüzden sadece köklerden başlayıp özyineleme ile iniyoruz.

### Neden `Renderer2D`'ye matris aşırı yükü eklendi?
`WorldTransformComponent` zaten bir matris. Onu pozisyon/ölçek/açıya geri
ayrıştırıp `DrawQuad(pos, size, rot)` çağırmak hem gereksiz hem **kayıplı**
olurdu (eğik/çarpık dönüşümler ayrıştırılamaz).

## `RelationshipComponent`

```cpp
UUID              Parent;
std::vector<UUID> Children;
```

**UUID tutuyoruz, `entt::entity` değil** — yüklemeden sağ çıkması gerekiyor.
Faz 8'in tam olarak var oluş sebebi bu.

Çocukları `vector` olarak saklamak bağlı listeden daha basit ve doğrudan
serileştirilebiliyor. (Hazel gibi motorlar "ilk çocuk + sonraki kardeş"
bağlı liste kullanır; bellek açısından daha sıkı ama okuması zor.)

## ⚠️ Döngü koruması

`A`'nın parent'ı `B`, `B`'nin parent'ı `A` olursa `TransformSystem`
özyinelemesi **hiç bitmez** — yığın taşar.

```cpp
if (parent == *this || (parent && IsAncestorOf(parent))) reddet;
```

`IsAncestorOf` parent zincirini yukarı yürür. Sürükle-bırakta kullanıcı
bunu kolayca deneyebilir (bir nesneyi kendi çocuğunun üstüne bırakmak).

## Silme davranışı: çocuklar da silinir

Alternatifi onları köke taşımaktı. Birlikte silmek daha sezgisel — bir
tankın namlusu tank yok olunca ortada kalmamalı. Unity ve Godot da böyle yapar.

**Dikkat:** Çocuk listesi **kopyalanarak** geziliyor. Özyineleme sırasında
parent'ın `Children` dizisi değişiyor; üzerinde gezerken değiştirmek
geçersiz yineleyiciye yol açardı.

## Serileştirme: tek yön yaz

Dosyaya sadece **`Parent`** yazılıyor, çocuk listesi değil. Çocuklar parent
bilgisinden yeniden kurulabilir.

> İki yönü de yazmak, ikisinin **tutarsız kalabileceği** bir dosya formatı
> demektir. Aynı bilgiyi iki yerde saklamamak genel bir serileştirme kuralı.

### İki geçişli yükleme
Dosyada bir çocuk parent'ından **önce** gelebilir. İlk geçiş entity'leri
oluşturur, ikinci geçiş parent bağlarını kurar.

Faz 8'de `FollowComponent` için buna gerek olmamıştı çünkü orada geç
çözümleme vardı (her karede aranıyor). Hiyerarşi ise kalıcı bir yapı,
yükleme anında kurulmalı.

Format **sürüm 3**. Sürüm 2 ve 1 dosyaları hâlâ açılıyor.

## Gizmo dünya uzayında çalışır

ImGuizmo dünya matrisini manipüle eder. Parent'ı olan bir entity'nin
**yerel** matrisini verirsek tutamaklar yanlış yerde çıkar.

```cpp
transform = parentWorld * tc.GetTransform();   // gizmo'ya ver
// ... kullanıcı sürükledi ...
local = inverse(parentWorld) * transform;      // geri çevir
```

Inspector'daki değerler **yerel** kalıyor; parent'ı olan entity'lerde
"(parent'a göre yerel)" notu gösteriliyor.

## Hierarchy paneli

- Ağaç görünümü, sadece kökler dolaşılıyor, çocuklar özyineleme ile
- **Sürükle-bırak** ile parent değiştirme; boşluğa bırakınca köke taşır
- Sağ tık: alt entity ekle / köke taşı / sil
- Ok tıklaması ile seçim ayrıldı (`IsItemToggledOpen`)

**Yapıyı değiştiren tüm işlemler döngü dışında** uygulanıyor — ağaç üzerinde
gezerken parent/child listelerini değiştirmek yineleyicileri bozar.
İstekler bir değişkende biriktirilip döngü bittikten sonra işleniyor.
(Faz 6'da silme için yaptığımızın genelleştirilmiş hâli.)

## `Scene::OnRender` içinde de `TransformSystem`

Editörde sahne duraklatılmış olabilir; o zaman `OnUpdate` hiç çalışmaz ama
Inspector'dan yapılan değişikliklerin görünmesi gerekir. Bu yüzden dünya
matrisleri çizimden önce de tazeleniyor.

> Bu, ileride "dirty flag" optimizasyonu geldiğinde tekrar gözden
> geçirilecek bir yer.

## Sistem sırası
```
Follow → Movement → Transform
```
`TransformSystem` **en sonda** olmalı: ondan önce çalışan her şey yerel
konumu değiştirebilir.

## Test sonuçları
```
Sahne kuruldu: 31 entity
Sahne kaydedildi: ... (31 entity)
Sahne yuklendi:   ... (31 entity, 2 hiyerarsi bagi)
GL HATA: 0
```

Dosya doğrulaması:
```
Tank Govde     parent = (kok)
Tank Kule      parent = Tank Govde
Tank Namlu     parent = Tank Kule
```

## Onay
- [x] Derlendi
- [x] Hiyerarşi kaydediliyor/yükleniyor (2 bağ korundu)
- [x] GL hatası yok
- [ ] Kullanıcı onayı → Faz 12

## Sonraki: Faz 12 (content browser + prefab)
