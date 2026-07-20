# FXEngine v2 — Yol Haritası

MVP (Faz 0–7) tamamlandı. Bu belge bir sonraki sürümün planı.

**Kural aynı:** aynı anda tek faz, test edilmeden sonrakine geçilmez,
her faz sonunda commit + push.

---

## Sıralama neden böyle?

Faz sırası keyfi değil — bazıları diğerlerinin **önkoşulu**:

```
Faz 8 (UUID)  ──┬──> Faz 9  (parent/child)  ──> Faz 12 (prefab)
                └──> Faz 11 (seçim/gizmo)

Faz 10 (play modu) ✅ ──> Faz 16 (script)   — script'in "çalışıyor" hali olmalı
Faz 13 (input)     ──> Faz 16 (script)      — script girdi okuyacak
Faz 14 (subtexture)──> Faz 15 (animasyon)   — animasyon kareleri subtexture
```

Sonraya bıraktıklarım (ses, metin, fizik) bağımsız — sırasını sen değiştirebilirsin.

---

## Faz 8 — UUID + entity referansları ✅ TAMAM

**Neden ilk?** Şu an sahne dosyasında entity'ler birbirine referans veremiyor.
"Bu düşman şu hedefi takip etsin" yazamazsın, çünkü EnTT kimlikleri
**yükleme sırasında değişir**. Faz 7'de bunu bizzat yaşadık:
`m_PlayerEntity`'yi temizlemek zorunda kaldık.

Bu, sonraki her şeyin (hiyerarşi, prefab, script hedefleri) önkoşulu.

- [x] `UUID` sınıfı (64-bit rastgele, `std::random_device` tohumlu)
- [x] `IDComponent` — her entity'ye `CreateEntity`'de otomatik eklenir
- [x] `Scene::FindEntityByUUID(UUID)` + UUID → entity haritası
- [x] Serileştirmeye UUID yaz, yüklerken **aynı** UUID ile oluştur
- [x] `Entity` referansları için `EntityRef` (UUID tutar, geç çözümlenir)
- [x] `FollowComponent` + `FollowSystem` — referansların kanıt sistemi
- [x] Sürüm 2 dosya formatı, sürüm 1 ile geriye dönük uyumlu

Ayrıntılar: [Faz-08-Notlar.md](Faz-08-Notlar.md)

---

## Faz 9 — Parent / Child hiyerarşisi ✅ TAMAM

- [x] `RelationshipComponent` — parent UUID + çocuk UUID listesi
- [x] `Entity::SetParent / GetChildren / IsAncestorOf`
- [x] **Dünya transform'u = parent zinciri × yerel transform**
- [x] `TransformSystem` — kökten yaprağa, `WorldTransformComponent`
- [x] `Renderer2D::DrawQuad(mat4, …)` aşırı yükleri
- [x] Hierarchy ağaç görünümü + sürükle-bırak ile parent değiştirme
- [x] Parent silinince çocuklar da silinir (Unity/Godot davranışı)
- [x] Döngü koruması, gizmo dünya↔yerel dönüşümü
- [x] Sürüm 3 dosya formatı, iki geçişli yükleme
- [ ] Dirty flag optimizasyonu → gerekirse ileride

Ayrıntılar: [Faz-09-Notlar.md](Faz-09-Notlar.md)

---

## Faz 10 — Play / Stop modu ✅ TAMAM

- [x] `Scene::Copy()` — derin kopya, **UUID'ler korunur** (prefab'ın tersi),
      tek geçiş yeterli
- [x] `ComponentGroup`/`AllComponents` — kopyalanacak component'ler tek yerde
- [x] `SceneState { Edit, Play }` — `Simulate` ertelendi, aşağıya bak
- [x] ▶ / ⏹ / ⏸ araç çubuğu (viewport toolbar'ında)
- [x] Play'de runtime kopya güncellenir; Stop'ta kopya atılır
- [x] `CameraComponent` + `Scene::GetPrimaryCameraEntity()` + serileştirme
- [x] Editör kamerası `EditorCamera` sınıfına taşındı
- [x] Edit modu artık **durağan** — sahne sadece Play'de çalışıyor

> `Simulate` modu ertelendi: fizik (Faz 17) ve script (Faz 16) olmadan
> Play'den farkı olmazdı. Kullanılmayan bir durum eklemek, var olmayan
> bir özellik vaat etmek olurdu.

Ayrıntılar: [Faz-10-Notlar.md](Faz-10-Notlar.md)

**Öğrenilen:** Neden oyunu düzenlenen sahnede çalıştırmak felakettir
(Unity'de "play modunda yaptığın değişiklikler kayboldu" travması).

---

## Faz 11 — Viewport'ta seçim + gizmo ✅ TAMAM

Şu an entity'yi sadece Hierarchy listesinden seçebiliyorsun.

- [x] Framebuffer'a **ikinci renk eki**: `R32I` formatında entity ID
- [x] Fragment shader ikinci çıktıya entity ID yazsın
- [x] `glReadPixels` ile fare altındaki pikselin ID'sini oku → seçim
- [x] **ImGuizmo** entegrasyonu: taşı / döndür / ölçekle tutamakları
- [x] Gizmo kısayolları — Z/X/C/B (W/E/R kamera tuşlarıyla çakışıyordu)
- [x] Bonus: `glVertexAttribIPointer` hatası düzeltildi
- [x] Bonus: varsayılan panel düzeni (DockBuilder), `imgui.ini` exe yanına
- [x] Seçili entity'ye turuncu çerçeve → Faz 18'in çizgi altyapısıyla yapıldı

Ayrıntılar: [Faz-11-Notlar.md](Faz-11-Notlar.md)

---

## Faz 12 — Varlık yönetimi + prefab ✅ TAMAM

- [x] `Content Browser` paneli — ikonlar, arama, kırıntı yolu, klasör
      oluşturma, yeniden adlandırma, silme, Explorer'da gösterme
- [x] Dosyadan viewport'a **sürükle-bırak** ile sprite oluşturma
- [x] **Dışarıdan varlık aktarma** — pencereye sürükle-bırak (`SDL_EVENT_DROP_FILE`)
      veya Ctrl+I ile çoklu seçim
- [x] Yerel dosya diyalogları (Win32 `GetOpenFileName`/`GetSaveFileName`)
- [x] **Prefab**: bir entity ağacını dosyaya kaydet, sahneye örnekle
- [x] Son açılan sahneler listesi (`editor.json`)
- [ ] `AssetManager` — **ertelendi**, aşağıya bak

**Öğrenilen:** Varlık kimliği (asset handle) ile dosya yolu arasındaki fark;
prefab örneklemenin özü olan **ID remapping**; mutlak yolun asla dosyaya
yazılmaması.

> **`AssetManager` neden ertelendi:** yol yerine UUID kullanmak, her varlığın
> yanında bir `.meta` dosyası ve bir varlık veritabanı (proje taraması,
> GUID→yol tablosu, dosya izleyici) gerektiriyor. Kendi başına bir faz.
> Şimdilik yol = kimlik; taşıma/yeniden adlandırma referansları koparır.

Ayrıntı: `Faz-12-Notlar.md`

---

## Faz 13 — Girdi soyutlaması + olay sistemi

Şu an `EditorApp` doğrudan `SDL_GetKeyboardState` çağırıyor. Motor
kullanıcısının SDL bilmesi gerekmemeli.

- [ ] `FX::Input` — `IsKeyPressed(KeyCode)`, `GetMousePosition()`
- [ ] `FX::KeyCode` / `MouseButton` — SDL'den bağımsız enum'lar
- [ ] Motor içi `Event` sınıfları (`WindowResize`, `KeyPressed`, `MouseMoved`)
- [ ] SDL olayları → motor olaylarına çeviren katman
- [ ] `Layer` / `LayerStack` — olayların katmanlar arası akışı
- [ ] Girdi eşlemesi (action mapping): "Zıpla" = Space **veya** gamepad A

**Öğrenilecek:** Platform soyutlaması nerede biter. Aşırı soyutlamanın
maliyeti (her SDL enum'unu kopyalamak gerçekten değer mi?).

---

## Faz 14 — Sprite sheet / SubTexture

Tek tek PNG yerine tek atlas kullanmak hem batch'i hem belleği iyileştirir.

- [ ] `SubTexture2D` — bir texture'ın UV alt bölgesi
- [ ] `SubTexture2D::FromCoords(texture, {x,y}, {cellW,cellH}, {spanX,spanY})`
- [ ] `Renderer2D::DrawQuad` subtexture kabul etsin
- [ ] Inspector'da sprite sheet hücre seçici
- [ ] Serileştirmeye UV bölgesi eklensin

**Öğrenilecek:** Neden 100 ayrı PNG yerine tek atlas — draw call değil,
**texture bind sayısı** ve bellek hizalaması.

---

## Faz 15 — Animasyon

- [ ] `AnimationClip` — kare listesi + süre + döngü bayrağı
- [ ] `AnimatorComponent` — mevcut klip, geçen süre, hız
- [ ] `AnimationSystem` — süreyi ilerletip `SpriteRenderer`'ın subtexture'ını değiştirir
- [ ] Animasyon klip dosyası (JSON)
- [ ] Inspector'da klip seçimi + önizleme

**Öğrenilecek:** Sistemler arası veri akışı — `AnimationSystem` yazar,
`SpriteRenderSystem` okur, birbirlerini hiç bilmezler.

---

## Faz 16 — Native script component

- [ ] `NativeScriptComponent` — `OnCreate / OnUpdate / OnDestroy`
- [ ] `ScriptableEntity` taban sınıfı (`GetComponent` erişimi)
- [ ] `ScriptSystem` — Play modunda çalışır, Edit'te çalışmaz
- [ ] Örnek: `PlayerController`, `FollowTarget` (Faz 8'in `EntityRef`'ini kullanır)

**Önkoşul:** Faz 10 (play modu) + Faz 13 (input).

**Öğrenilecek:** ECS'e "davranış" eklemenin sınırı. Neden script'ler
component'lerin kendisi değil, onları **düzenleyen** nesnelerdir.

> İleride C# / Lua script eklenebilir ama o başlı başına bir proje —
> önce native ile kavramı oturt.

---

## Faz 17 — Çarpışma ve fizik

- [ ] `BoxCollider2D`, `CircleCollider2D` component'leri
- [ ] `Rigidbody2D` (Static / Dynamic / Kinematic)
- [ ] **Box2D** entegrasyonu (kendi fiziğini yazmak ayrı bir proje)
- [ ] `PhysicsSystem` — Play modunda dünyayı adımlar, Transform'ları günceller
- [ ] Debug çizim: collider sınırlarını göster
- [ ] Çarpışma geri çağrımları → script'e ulaşsın

**Dikkat:** Fizik **sabit adımda** çalışmalı — Faz 1'deki fixed timestep'in
gerçek karşılığını burada göreceksin.

---

## Faz 18 — Render iyileştirmeleri 🟡 KISMEN

Çizgi altyapısı ve editörün görsel eksikleri öne alındı (viewport'u
oturtmak için). Kalanı duruyor.

- [x] `Renderer2D::DrawLine` + ayrı çizgi batch'i (GL_LINES, index'siz)
- [x] `Renderer2D::DrawRect` — hem eksen hizalı hem matrisli
- [x] `RenderCommand::SetDepthTest` — debug çiziminde sırayı z değil
      çizim sırası belirlesin
- [x] Sonsuz ızgara (editör zemini) — 1-2-5-10 serisinde adaptif aralık
- [x] Seçim çerçevesi (dünya matrisiyle, döndürülmüş nesnede doğru)
- [x] `F` ile seçiliye odaklan, `G` ile ızgara aç/kapa
- [ ] Daire primitifi (`DrawCircle`) — collider debug'ı için gerekecek
- [ ] Debug çizim katmanı (collider, kamera sınırı)
- [ ] Sıralama katmanı (`SortingLayer` + `OrderInLayer`)
- [ ] Saydam nesneler için doğru sıralama (arkadan öne)
- [ ] Shader hot reload — dosya değişince yeniden derle

Ayrıntılar: [Faz-18-Notlar.md](Faz-18-Notlar.md)

**Öğrenilecek:** 2D'de derinlik tamponu neden yetmez, sıralama neden gerekir.

---

## Faz 19 — Metin ve ses

- [ ] Font atlası (stb_truetype veya msdf-atlas-gen)
- [ ] `Renderer2D::DrawString`
- [ ] `TextComponent`
- [ ] Ses: **miniaudio** (tek başlık, kolay) veya SDL_audio
- [ ] `AudioSourceComponent`, `AudioListenerComponent`
- [ ] `AudioSystem`

**Not:** Metin render, göründüğünden çok daha zordur (kerning, satır sarma,
ölçek bağımsız keskinlik). Bir fazı hak ediyor.

---

## Faz 21 — Proje sistemi ✅ TAMAM (AssetManager hariç)

Şu an editör `<exe>/assets`'i kök alıyor: varlıklar `build/` altında yaşıyor
ve sürüm kontrolüne girmiyor. **Bilinçli bir karar** — kalıcılık şimdilik
gerekmiyor, doğru çözüm ayrı bir faz.

- [x] `.fxproject` dosyası: proje adı, varlık klasörü, başlangıç sahnesi
- [x] "Proje Oluştur / Proje Aç" akışı
- [x] Content browser kökü = **proje** klasörü, exe'nin yanı değil
- [x] `FileSystem`: motor varlıkları (shader) ile proje varlıkları ayrımı
      — `ResolveEngineAsset` / `ResolveProjectAsset`
- [x] Son açılan **projeler**
- [x] Projesiz mod hâlâ çalışıyor ama artık uyarıyor
- [x] Açılışta proje seçme ekranı (launcher): son projeler, aç/oluştur, projesiz devam
- [ ] `AssetDirectory` gerçekten kullanılsın (şimdilik `assets` varsayılıyor)
- [ ] **`.meta` dosyaları + GUID tabanlı `AssetManager`** → kendi fazı olmalı

Ayrıntılar: [Faz-21-Notlar.md](Faz-21-Notlar.md)

**Öğrenilecek:** Bir editörün "kurulu olduğu yer" ile "üzerinde çalıştığı
yer" farklı şeylerdir. Bu ayrımı yapmayan araçlar taşınabilir olmaz.

> `AssetManager` (Faz 12'de ertelendi) buraya bağlı: GUID→yol tablosunun
> taranacağı bir *proje kökü* olmadan varlık veritabanı kurulamaz.

---

## Faz 20 — Kalite ve araçlar

- [ ] **Undo / Redo** — komut deseni (command pattern)
- [ ] Profiling: kapsam zamanlayıcıları, Chrome tracing çıktısı
- [ ] Birim testleri (Catch2) — özellikle `SceneSerializer`, `UUID`, matematik
- [ ] CI (GitHub Actions): Windows + Linux derleme
- [ ] Editör teması / stil ayarları
- [ ] Kilitlenme raporlama

> **Undo/Redo'yu hafife alma.** Her düzenleme işlemini geri alınabilir bir
> komuta çevirmek, editörün tamamına dokunan bir mimari değişikliktir.
> Erken düşünmek sonradan eklemekten çok daha ucuzdur.

---

# Teknik borç (fazlardan bağımsız, ne zaman olsa yapılabilir)

MVP sırasında bilinçli olarak bıraktıklarımız:

| Konu | Sorun | Nerede |
|---|---|---|
| `Renderer2D` global durum | `s_Data` dosya kapsamlı statik. İkinci bir renderer örneği imkânsız, test etmek zor. | `Renderer2D.cpp` |
| Batch bölme yolu hacky | Slotlar dolunca `EndScene()` çağırıp elle durum düzeltiliyor. Ayrı bir `FlushAndReset()` olmalı. | `Renderer2D.cpp` |
| `Shader::FromFiles` ham `new` | `unique_ptr` döndürmeli. | `Shader.cpp` |
| ~~Editör kamerası `EditorApp` içinde~~ | ✅ Faz 10'da `EditorCamera` sınıfına taşındı. | — |
| ~~Sahne yolu koda gömülü~~ | ✅ Faz 12'de dosya diyaloğuyla çözüldü. | — |
| Hata durumunda yedek doku yok | Texture yüklenemezse `nullptr`; mor "eksik doku" dokusu daha iyi olurdu. | `TextureLibrary.cpp` |
| Doku ayarı dosya başına saklanamıyor | Filtre/sarma yalnızca ilk yüklemede belirleniyor; sahne dosyası sadece *yolu* saklıyor. Kalıcı ayar `.fxmeta` gerektiriyor → Faz 21. Şimdilik farklı spec istenirse uyarı basılıyor. | `TextureLibrary.cpp` (Faz 12) |
| **Varlık kimliği = dosya yolu** | Dosyayı taşımak/yeniden adlandırmak tüm referansları koparır. Çözüm: `.meta` dosyaları + GUID tabanlı `AssetManager`. | `SceneSerializer`, `TextureLibrary` (Faz 12) |
| Dosya diyalogları sadece Win32 | Linux/macOS'ta boş gövde derleniyor; `nfd` veya portal gerekli. | `Editor/src/Platform/FileDialogs.cpp` |
| ~~Proje klasörü kavramı yok~~ | ✅ Faz 21'de `.fxproject` + iki ayrı kök ile çözüldü. | — |
| Dosya izleyici yok | Klasör dışarıdan değişirse panel fark etmiyor; elle "Yenile" gerekiyor. | `ContentBrowserPanel.cpp` (Faz 12) |
| Prefab bağlantısı yok | Örnek kaynağından bağımsız; kaynak değişince örnekler güncellenmiyor (override sistemi yok). | `PrefabSerializer.cpp` (Faz 12) |
| `GetRegistry()` çok açık | Registry'ye doğrudan `create`/`destroy` çağırmak UUID haritasını bozar. Daha dar bir erişim gerekebilir. | `Scene.h` (Faz 8) |
| `FollowSystem` Scene'e bağımlı | Diğer sistemler sadece registry alıyor, bu Scene alıyor (UUID haritası için). Test etmesi daha zor. | `Systems.cpp` (Faz 8) |
| Seçim `SceneHierarchyPanel` içinde | Viewport, gizmo, inspector, prefab hepsi okuyor; panel sahibi değil tüketicisi olmalı. Ayrı bir `SelectionContext` gerekiyor — çoklu seçimin ön adımı. | `SceneHierarchyPanel.h` (Faz 10) |
| Test yok | Hiç birim testi yok. Serializer ve matematik en çok fayda sağlayacak yerler. | — |
| Sadece Windows'ta denendi | Kod taşınabilir yazıldı ama Linux'ta hiç derlenmedi. | — |

---

# Önerilen başlangıç

Sırayla gitmek istersen: **Faz 8 → 9 → 10 → 11**.

Bu dördü birlikte motoru "oyuncak" olmaktan çıkarıp gerçek bir editöre
dönüştürür: kalıcı kimlik, hiyerarşi, oyunu çalıştırma, viewport'ta
doğrudan düzenleme.

Görsel sonuç en hızlı gelen: **Faz 11** (gizmo ile nesne sürüklemek).
En çok kavram öğreten: **Faz 8** ve **Faz 10**.
