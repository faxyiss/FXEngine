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

## Faz 0.x — Borç kapatma (fazlardan önce)

Bunlar faz değil; **ilerledikçe pahalılaşan** borçlar. Faz 13'e girmeden
kapatılıyorlar çünkü ikisi (0.2, 0.3) Faz 20'nin ön koşulu, biri (0.4)
Faz 22'nin bıraktığı tutarlılık açığı.

- [x] **0.1 — Faz 22 doğrulaması.** Gerçek `Game1` projesi açıldı, sahne
      sürüm 4'e dönüşmüş, `TextureHandle` ile `.meta` GUID'i eşleşiyor.
      `.gitignore`'da `*.meta` yok. Kaynakta eksik olan
      `circle.png.meta` eklendi — yoksa her temiz derlemede motor
      varlığı yeni GUID alıyordu.
- [x] **0.2 — `SelectionContext`.** Seçim `SceneHierarchyPanel`'den
      çıkarıldı; `EditorApp` tutuyor, paneller tüketici. API baştan
      çoklu seçime göre yazıldı, davranış şimdilik tek seçim.
      Ayrıntı: [Borc-02-SelectionContext.md](Borc-02-SelectionContext.md)
- [x] **0.3 — Entity çoklu seçimi.** Hierarchy'de Ctrl/Shift, viewport'ta
      Ctrl+tık, gizmoyla toplu dönüşüm (delta), toplu silme.
      Ayrıntı: [Borc-03-Coklu-Secim.md](Borc-03-Coklu-Secim.md)
- [x] **0.4 — Dosya izleyici.** `FX::FileWatcher`
      (`ReadDirectoryChangesW` + tek thread + debounce). Dışarıdan
      yeniden adlandırma/taşıma GUID'i koruyor, yeni dosyaya `.meta`
      üretiliyor, silinende temizleniyor.
      Ayrıntı: [Borc-04-Dosya-Izleyici.md](Borc-04-Dosya-Izleyici.md)
- [x] **0.5 — Catch2 iskeleti.** `Tests/` altında `FXTests`; `UUID`,
      `Scene`, `SceneSerializer`, `AssetManager` — 26 test / 80 assertion,
      `ctest` ile de çalışıyor.
      Ayrıntı: [Borc-05-Birim-Testleri.md](Borc-05-Birim-Testleri.md)
- [x] **0.6 — Faz 22 artıkları.** Inspector'da doku ayarları arayüzü,
      `StartScene` GUID'e geçti (`.fxproject` sürüm 2) + "Baslangic
      Sahnesi Yap" menüsü, `AssetDirectory` gerçekten kullanılıyor,
      görünüm tercihi kaydediliyor. Prefab referansları zaten GUID'miş
      (ortak `SerializeEntity`).
      Ayrıntı: [Borc-06-Faz22-Artiklari.md](Borc-06-Faz22-Artiklari.md)

> **Neden 0.2 ve 0.3 Faz 20'den önce?** Undo komutları "hangi entity'ler
> üzerinde?" sorusunu cevaplar. Tek seçim varsayımıyla yazılan her komut,
> çoklu seçim gelince yeniden yazılır.

---

## Faz 13 — Girdi soyutlaması + olay sistemi 🟡 13a + 13b TAMAM

- [x] **13a** `FX::Key` / `MouseButton` enum'ları + `FX::Input`
      (`IsKeyPressed`, `IsMouseButtonPressed`, `GetMousePosition`).
      Değerler SDL scancode'larıyla aynı; birim testleriyle korunuyor.
- [x] **13b** Motor içi `Event` sınıfları + `EventDispatcher` + SDL →
      motor olayı çeviri katmanı. `Application` iki kanca sunuyor:
      `OnRawEvent` (ImGui + OS sürükle-bırak) ve `OnEvent`. Editörde ham
      SDL girdi kullanımı sıfırlandı.
- [ ] **13c** `Layer` / `LayerStack` — **ertelendi**: editörün tek
      katmanı var, ikincisi ancak Faz 16'da doğacak
- [ ] **13d** Girdi eşlemesi (action mapping) — **ertelendi**: gerçek
      ihtiyaçlar 16c'deki örnek oyunda görülecek

Ayrıntılar: [Faz-13-Notlar.md](Faz-13-Notlar.md)

**Öğrenilen:** Platform soyutlaması nerede biter. Enum değerlerini SDL ile
aynı tutmak "sızıntı" gibi görünür ama sızan şey SDL'in kendisi değil
sayıları — çeviri tek bir cast'e iniyor. Asıl aşırı soyutlama riski
`LayerStack`'teydi ve o yüzden ertelendi.

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

- [x] **16a** `ScriptableEntity` + `NativeScriptComponent` + `ScriptSystem`
      + `Scene::OnRuntimeStart/Stop`. Örnek script'ler (Spin, Move)
      Inspector'dan eklenebiliyor.
      Ayrıntı: [Faz-16-Notlar.md](Faz-16-Notlar.md)
- [x] **16b** `ScriptRegistry` (ad → sınıf), Inspector'da combo ile
      seçim, sahne dosyasına serileştirme. Bilinmeyen ad component'i
      silmiyor — bilgi korunuyor, uyarı veriliyor.
- [ ] **16c** Örnekler: `PlayerController`, `FollowTarget` (Faz 8'in
      `EntityRef`'ini kullanır) + küçük bir örnek oyun

**Önkoşul:** Faz 10 (play modu) + Faz 13 (input).

**Öğrenilecek:** ECS'e "davranış" eklemenin sınırı. Neden script'ler
component'lerin kendisi değil, onları **düzenleyen** nesnelerdir.

> İleride C# / Lua script eklenebilir ama o başlı başına bir proje —
> önce native ile kavramı oturt.

---

## Faz 17 — Çarpışma ve fizik

- [ ] **17a** **Box2D** entegrasyonu + `Rigidbody2D` (Static / Dynamic /
      Kinematic) + `PhysicsSystem` — sabit adımda dünyayı adımlar,
      Transform'ları günceller
- [ ] **17b** `BoxCollider2D`, `CircleCollider2D` + malzeme (sürtünme,
      esneklik)
- [ ] **17c** `Simulate` modu + collider debug çizimi (18b'nin
      `DrawCircle`'ını kullanır)
- [ ] **17d** Çarpışma geri çağrımları → script'e ulaşsın (16a'ya bağlı)

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
- [ ] **18b** Daire primitifi (`DrawCircle`) + debug çizim katmanı
      (collider, kamera sınırı)
- [ ] **18c** Sıralama katmanı (`SortingLayer` + `OrderInLayer`) +
      saydam nesneler için arkadan öne sıralama
- [ ] **18d** Shader hot reload — dosya değişince yeniden derle
      (0.4'ün dosya izleyicisini kullanır)

Ayrıntılar: [Faz-18-Notlar.md](Faz-18-Notlar.md)

**Öğrenilecek:** 2D'de derinlik tamponu neden yetmez, sıralama neden gerekir.

---

## Faz 19 — Metin

- [ ] Font atlası (stb_truetype veya msdf-atlas-gen)
- [ ] `Renderer2D::DrawString`
- [ ] `TextComponent`

**Not:** Metin render, göründüğünden çok daha zordur (kerning, satır sarma,
ölçek bağımsız keskinlik). Tek başına bir fazı hak ediyor — bu yüzden ses
ayrıldı (Faz 23), ikisinin birbiriyle hiçbir ilgisi yok.

---

## Faz 23 — Ses

- [ ] **miniaudio** (tek başlık, kolay) veya SDL_audio
- [ ] `AudioSourceComponent`, `AudioListenerComponent`
- [ ] `AudioSystem` — Play'de çalar, Stop'ta susar

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
- [x] **`.meta` dosyaları + GUID tabanlı `AssetManager`** → ✅ Faz 22

Ayrıntılar: [Faz-21-Notlar.md](Faz-21-Notlar.md)

**Öğrenilecek:** Bir editörün "kurulu olduğu yer" ile "üzerinde çalıştığı
yer" farklı şeylerdir. Bu ayrımı yapmayan araçlar taşınabilir olmaz.

> `AssetManager` (Faz 12'de ertelendi) buraya bağlı: GUID→yol tablosunun
> taranacağı bir *proje kökü* olmadan varlık veritabanı kurulamaz.

---

## Faz 22 — AssetManager (GUID tabanlı varlık kimliği) ✅ TAMAM

Faz 12'den beri açık duran borç: varlığın kimliği dosya yoluydu.

- [x] `AssetHandle` / `AssetType` / `AssetMetadata`
- [x] Her varlığın yanında `.meta` (GUID + içe aktarma ayarları)
- [x] `AssetManager`: proje taraması, GUID↔yol tablosu
- [x] Sahne sürümü 4 — `TextureHandle` (GUID); sürüm ≤3 otomatik dönüşüyor
- [x] Doku ayarları `.meta`'da → `TextureLibrary` spec çakışması bitti
- [x] İçerik paneli: `.meta` taşıma/gizleme, taşımada referans korunuyor
- [x] Inspector'da doku ayarları arayüzü → 0.6
- [x] `StartScene` GUID'e geçti (`.fxproject` sürüm 2) → 0.6
      (prefab referansları zaten GUID'di: ortak `SerializeEntity`)

Ayrıntılar: [Faz-22-Notlar.md](Faz-22-Notlar.md)

**Öğrenilen:** Kimlik ile konum ayrı şeylerdir — üçüncü kez (Faz 8 entity,
Faz 21 proje, Faz 22 varlık).

---

## Faz 20 — Kalite ve araçlar

- [ ] **20a** Undo / Redo çekirdeği — komut deseni, `CommandStack`
- [ ] **20b** Komutların editöre yayılması: transform, component
      ekle/sil, hiyerarşi değişikliği, silme (0.2 + 0.3 ön koşul)
- [ ] **20c** Profiling: kapsam zamanlayıcıları, Chrome tracing çıktısı
- [ ] **20d** CI (GitHub Actions, Windows + Linux), editör teması,
      kilitlenme raporlama

> Birim testleri (Catch2) bu fazdan **0.5'e alındı** — `SceneSerializer`
> ve `AssetManager` elle test edilemeyecek kadar karmaşıklaştı.

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
| ~~Doku ayarı dosya başına saklanamıyor~~ | ✅ Faz 22'de `.meta` dosyalarına taşındı; spec çakışması ve uyarısı ortadan kalktı. | — |
| **Varlık kimliği = dosya yolu** | Dosyayı taşımak/yeniden adlandırmak tüm referansları koparır. Çözüm: `.meta` dosyaları + GUID tabanlı `AssetManager`. | `SceneSerializer`, `TextureLibrary` (Faz 12) |
| Dosya diyalogları sadece Win32 | Linux/macOS'ta boş gövde derleniyor; `nfd` veya portal gerekli. | `Editor/src/Platform/FileDialogs.cpp` |
| ~~Proje klasörü kavramı yok~~ | ✅ Faz 21'de `.fxproject` + iki ayrı kök ile çözüldü. | — |
| ~~Dosya izleyici yok~~ | ✅ 0.4'te `FX::FileWatcher` ile çözüldü (Windows). Linux/macOS gövdesi hâlâ boş. | `Core/FileWatcher.cpp` |
| Prefab bağlantısı yok | Örnek kaynağından bağımsız; kaynak değişince örnekler güncellenmiyor (override sistemi yok). | `PrefabSerializer.cpp` (Faz 12) |
| `GetRegistry()` çok açık | Registry'ye doğrudan `create`/`destroy` çağırmak UUID haritasını bozar. Daha dar bir erişim gerekebilir. | `Scene.h` (Faz 8) |
| `FollowSystem` Scene'e bağımlı | Diğer sistemler sadece registry alıyor, bu Scene alıyor (UUID haritası için). Test etmesi daha zor. | `Systems.cpp` (Faz 8) |
| ~~Seçim `SceneHierarchyPanel` içinde~~ | ✅ 0.2'de `SelectionContext`'e taşındı; sahibi `EditorApp`, paneller tüketici. | `SelectionContext.h` |
| ~~Test yok~~ | ✅ 0.5'te Catch2 geldi (`UUID`, `Scene`, `SceneSerializer`, `AssetManager`). Renderer ve `FileWatcher` hâlâ test dışı. | `Tests/` |
| Sadece Windows'ta denendi | Kod taşınabilir yazıldı ama Linux'ta hiç derlenmedi. | — |

---

# Güncel sıra

**2026-07-21'de yön kararı alındı** (ayrıntı: [02-Mimari.md](02-Mimari.md)
Bölüm II). Editör temel araçları örnek oyunun önüne alındı, C# scripting
ertelendi.

```
Aşama A:  A1 component meta → A2 Game View → A3 Settings
          → A4 script dosyası → A5 Undo/Redo
Aşama B:  oyun DLL'i + hot reload
Aşama C:  C# kararı yeniden değerlendirilir
Sonra:    16c → 14 → 15 → 18b → 17a-d → 18c → 19 → 23 → 18d
          → 13c → 13d → 20c → 20d
```

## Aşama A — Editör temelleri

- [ ] **A1 — Component meta-veri sistemi.** Her component tek yerde:
      ad, alan listesi, serileştirme, Inspector çizimi. Elle yazılan
      kayıt (makro/reflection kütüphanesi yok). Script alanlarının
      Inspector'dan ayarlanmasını ve Undo'yu da bu açar.
- [ ] **A2 — Game View / Scene View ayrımı.** Sahne kamerasından ayrı
      pencere; gizmo/ızgara yalnızca Scene View'da.
- [ ] **A3 — Project Settings + Preferences.** Projeye ait ayarlar
      (`ProjectSettings/`) ile kullanıcıya ait ayarlar (`editor.json`)
      ayrılır.
- [ ] **A4 — Script dosyası oluşturma + şablon.** İçerik panelinden
      "Yeni Script" → şablon → sistem editöründe aç.
- [ ] **A5 — Undo/Redo** (eski 20a + 20b). A1'den sonra çok ucuzlar.

## Aşama B — İterasyon hızı

- [ ] Oyun kodunu ayrı DLL'e alma + hot reload (C++ kalır, script
      değişimi ~5 sn)

## Aşama C — Script dili

- [ ] C# (Mono/CoreCLR) kararı yeniden değerlendirilir. Ön koşulu A1.

**Neden bu sıra:**
- **A1 en önde:** component meta tablosu Inspector çizimini,
  serileştirmeyi, script alanlarını, çok hedefli düzenlemeyi ve Undo
  komutlarını birden ucuzlatıyor. Ayrıca ileride C# köprüsünün
  dayanacağı yapı bu.
- **A5 (Undo) A1'den sonra:** generic "alanı değiştir" komutu ancak alan
  tablosu varken yazılabilir.
- **16c örnek oyun ertelendi:** ilk tur eksikler zaten belli oldu
  (Game View, settings, component sistemi, script dosyası). Araçlar
  bitince yazılacak; ikinci tur eksikler orada görünecek.
- **18b (`DrawCircle`) fizikten önce:** 17c'nin collider debug çizimi
  daireyi bekliyor.
- **13c/13d ve 18d en sona:** üçü de şu an var olmayan bir soruna çözüm.
