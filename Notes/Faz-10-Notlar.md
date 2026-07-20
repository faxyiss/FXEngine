# Faz 10 — Play / Stop modu

Bu faza kadar editör ve oyun aynı şeydi: sahne sürekli çalışıyordu, bir
sprite'ı yerleştirmeye çalışırken kaçıyordu. Artık ▶ ile **kopya bir
sahnede** oyun çalışıyor, ⏹ ile düzenleme hali geri geliyor.

## Dosyalar

| Dosya | Değişiklik |
|---|---|
| **`Editor/src/EditorCamera.*`** (yeni) | Editör kamerası ayrı sınıf |
| `Scene/Scene.*` | `Copy()`, `GetPrimaryCameraEntity()` |
| `Scene/Components.h` | `CameraComponent`, `ComponentGroup`/`AllComponents` |
| `Scene/EntitySerialization.cpp` | Kamera serileştirme |
| `EditorApp.*` | İki sahne, `SceneState`, Play/Stop araç çubuğu |
| `Panels/SceneHierarchyPanel.cpp` | Inspector'da kamera |

---

## 1. Neden kopya sahne?

Oyun çalışırken nesneler hareket eder, silinir, oluşur. Düzenlenen sahnede
çalıştırsaydık ⏹'e bastığında sahnen **geri dönüşü olmayan biçimde**
değişmiş olurdu.

Unity'nin meşhur "play modunda yaptığın değişiklikler kayboldu" travması
bunun tam tersidir: orada kullanıcı play modunda düzenleme yapar ve
kaybeder. Bizde tersi olurdu — **çalışmaların** kaybolurdu, ki daha kötü.

Çözüm ikisinde de aynı: iki ayrı sahne.

```cpp
std::unique_ptr<Scene> m_EditorScene;    // duzenlenen, Play'de dokunulmaz
std::unique_ptr<Scene> m_RuntimeScene;   // kopya, oyun burada calisir
FX::Scene* m_Scene;                      // ikisinden birini gosterir
```

`m_Scene` artık **sahip değil işaretçi**. Kodun geri kalanı (42 kullanım
yeri) değişmedi; sadece hangi sahneye baktığı Play/Stop ile değişiyor.

Stop'ta geri alma işlemi **yok**: kopyayı atmak yeterli. Bu, "değişiklikleri
geri sar" yaklaşımından çok daha ucuz ve hatasız.

---

## 2. `Scene::Copy` — UUID'ler KORUNUR

Prefab örneklemesinin (Faz 12) tam tersi:

| | Prefab örnekleme | Sahne kopyalama |
|---|---|---|
| UUID | **yeni üretilir** | **korunur** |
| Geçiş sayısı | 3 (remap tablosu gerekir) | **1** |
| Anlamı | şablondan yeni nesneler | aynı sahnenin başka bir anı |

Kimlik korunduğu için `eski → yeni` tablosuna gerek yok: hiyerarşi ve
`FollowComponent` hedefleri zaten UUID üzerinden çalışıyor ve o UUID'ler
değişmiyor. Tek geçiş yetiyor.

Korunmasaydı play moduna geçer geçmez takipçiler hedeflerini kaybederdi.

### Component listesi tek yerde

```cpp
using AllComponents = ComponentGroup<
    TagComponent, TransformComponent, RelationshipComponent,
    WorldTransformComponent, SpriteRendererComponent,
    VelocityComponent, FollowComponent, CameraComponent>;
```

```cpp
template<typename... C>
void CopyComponents(ComponentGroup<C...>, Entity dst, Entity src)
{
    (CopyComponentIfExists<C>(dst, src), ...);   // paket acilimi
}
```

Kopyalama listesini elle yazsaydık, yeni bir component eklendiğinde
"play'e geçince Velocity kayboldu" hatası doğardı. Faz 12'de
`EntitySerialization` için çözdüğümüz problemin aynısı, aynı gerekçeyle.

`IDComponent` listede yok: kimlik kopyalanmaz, hedef entity oluşturulurken
açıktan veriliyor.

---

## 3. `CameraComponent` — kamera da sahnenin parçası

Konum ve dönme `TransformComponent`'ten geliyor; component'te sadece
projeksiyon verisi var (`OrthographicSize`, `Primary`).

Kamerayı entity'ye bağlamanın kazancı test sahnesinde görünüyor: **kamera
oyuncuya parent yapıldı**, takip için tek satır kod yazılmadı — hiyerarşi
(Faz 9) zaten yapıyor. Kamerayı sahnenin dışında tutan motorlarda bu özel
kod ister.

`OrthographicSize` saklanıyor ama **en-boy oranı saklanmıyor**: o
viewport'tan gelir. Aynı sahne farklı pencere boyutlarında açılabilmeli.

### Tek birincil kamera

Inspector'da bir kamerayı birincil işaretlemek diğerlerinin işaretini
kaldırıyor. Yapmasaydık "hangisi kazanır" sorusunun cevabı registry'deki
sıraya kalırdı — kullanıcının göremediği bir şey.

Sahnede işaretli kamera yoksa editör kamerasına düşülüyor ve uyarı
basılıyor. Siyah ekran göstermektense çalışmaya devam etmek daha faydalı.

---

## 4. Edit modu artık DURAĞAN

```cpp
if (IsPlaying() && !m_ScenePaused)
    m_Scene->OnUpdate(dt);
```

Önceden editörde de nesneler hareket ediyordu. Bir editörde Edit modunun
duran olması bir eksiklik değil, **tanımın kendisi**.

Play'de düzenleme araçları (gizmo, seçim çerçevesi, ızgara) kapalı:
kopyada yapılan düzenleme Stop'ta zaten kaybolurdu, kullanıcıya bunu
yaptırmak yanıltıcı olur.

---

## 5. Seçim sahne değişince temizleniyor

`Entity` bir tutamak: `entt::entity` + `Scene*`. Sahne değişince eski
tutamak **anlamsız** hale gelir. Play/Stop'ta seçim sıfırlanıyor.

Faz 8'in dersinin bir başka görünümü: **tutamak sahneye bağlı, kimlik
değil.** İstenirse UUID üzerinden seçimi taşımak mümkün (kopyada aynı
UUID var) — şimdilik gerekmedi.

---

## 6. `EditorCamera` ayrı sınıfa taşındı

Teknik borç listesindeki madde. Play modunda sahnenin kamerası devreye
girince iki kamera birlikte var oluyor; aynı değişkenler üzerinden
yönetilemezler.

Sınır çizgisi: kamera **"fare viewport'ta mı", "gizmo kullanılıyor mu"
bilmiyor.** Bunlar `EditorApp`'te kalıp kameraya bayrak olarak geçiyor:

```cpp
const bool canPan = m_ViewportHovered && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver();
m_EditorCamera.OnImGuiInteract(canPan, m_ImGuiLayer.WantsKeyboard());
```

Kamerayı ImGui'nin panel durumundan haberdar etseydik editörün her
detayına bağımlı hale gelirdi.

---

## 7. Kamera gizmosu — görünmeyen bir nesneyi düzenlemek

İlk halinde kameranın sahnede hiçbir izi yoktu: sprite'ı olmadığı için
ne nereye baktığı görünüyordu ne de viewport'tan seçilebiliyordu.
Hierarchy'den seçmek tek yoldu.

Çizilen iki şey var:

**Görüş dikdörtgeni** — `OrthographicSize` yüksekliği, genişliği
viewport'un en-boy oranından. Play'de kullanılan hesabın **aynısı**;
farklı hesaplasaydık çerçeve yalan söylerdi.

**Kamera ikonu** — gövde + öne bakan huni, kameranın kendi konumunda.
Çerçevenin merkezi boş kalsaydı kameranın nerede durduğu (ve hangi
çerçevenin ona ait olduğu) belirsiz olurdu.

İkon dünya uzayında çizildiği için boyutu zoom'a bağlı:

```cpp
const float iconHalf = m_EditorCamera.GetZoom() * 0.035f;
```

Sabit birim verseydik uzaklaşınca kaybolur, yakınlaşınca ekranı kaplardı.

### Çizgiler entity ID taşıyor

```cpp
FX::Renderer2D::DrawLine(c0, c1, color, id);   // <- entity ID
```

Faz 18'de line shader'ına `o_EntityID` çıktısını eklemiştik; orada amaç
çizgilerin `-1` yazıp seçimi bozmamasıydı. Burada aynı mekanizma ters
yönde çalışıyor: kamera çizgilerine **kendi ID'sini** verince kamera
viewport'tan tıklanarak seçilebiliyor.

Bu yüzden `DrawCameraGizmos()` sahneden sonra ama `PickEntity()`'den
**önce** çağrılıyor — ID ekine yazması gerekiyor.

Birincil kamera sarı, diğerleri gri; seçili olan tam opak. Görünüm
menüsünden ve toolbar'dan kapatılabiliyor.

## Test sonuçları

**Play/Stop döngüsü** (ekran görüntüsü + simüle edilmiş tıklama):
```
Edit : cizgi 53, draw call 2, Mod=Edit,  izgara+cerceve var
Play : cizgi  0, draw call 1, Mod=Play,  gizmo devre disi, secim temiz
       "Play: sahne kamerasi 'Ana Kamera' (size=6.0)"
Stop : izgara geri geldi, takipciler ORIJINAL konumlarinda
```
Son satır asıl kanıt: Play sırasında takipçiler oyuncuya toplanmıştı,
Stop'ta düzenli sıralarına döndüler — düzenlenen sahne hiç bozulmamış.

**Kamera yuvarlak yolculuğu** (geçici öz-test, sonra kaldırıldı):
```
kaydet -> yukle : 32 entity, kamera=Ana Kamera, size=6.0, parent=Oyuncu
Scene::Copy     : 32 entity, kamera=Ana Kamera, parent=Oyuncu
```
İkincisi `Copy`'nin hiyerarşiyi de doğru taşıdığını gösteriyor.

## Onay
- [x] Derlendi (uyarısız)
- [x] `Scene::Copy` — UUID, hiyerarşi, component'ler korunuyor
- [x] Play/Stop döngüsü, orijinal sahne bozulmuyor
- [x] Sahne kamerası Play'de devrede
- [x] Kamera serileştirme (kaydet/yükle)
- [x] `EditorCamera` ayrıldı
- [x] Kamera gizmosu: görüş alanı + ikon, viewport'tan seçilebiliyor
- [ ] Kullanıcı onayı

## Yapılmayanlar
- **`Simulate` modu** — roadmap'te vardı. Fizik (Faz 17) ve script (Faz 16)
  olmadan Play'den farkı olmazdı; kullanılmayan bir durum eklemek var
  olmayan bir özellik vaat etmek olurdu. Faz 17'de gelecek.
- **Ayrı "Game" penceresi** — Play modunda viewport oyunu gösteriyor.
  Gerçek editörlerde Scene ve Game ayrı panellerdir; ikisini aynı anda
  görmek gerektiğinde eklenir.
- **Seçimin UUID ile taşınması** — Play/Stop'ta seçim sıfırlanıyor.
  Kopyada aynı UUID olduğu için taşınabilir; gerekirse eklenir.
- **`SelectionContext`** — seçim hâlâ `SceneHierarchyPanel` içinde yaşıyor
  ama viewport, gizmo, inspector hepsi okuyor. Çoklu seçim işiyle birlikte
  ayrılmalı.
