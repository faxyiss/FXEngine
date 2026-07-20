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
- [ ] **0.2 — `SelectionContext`.** Seçim `SceneHierarchyPanel`'den
      çıkarılıp ayrı bir nesneye taşınacak. Viewport, gizmo, inspector,
      prefab hepsi tüketici olacak.
- [ ] **0.3 — Entity çoklu seçimi.** Ctrl/Shift, gizmoyla toplu
      dönüşüm, toplu silme.
- [ ] **0.4 — Dosya izleyici.** `ReadDirectoryChangesW` + tek thread +
      debounce; `AssetManager` ve içerik paneli diskle senkron kalacak.
- [ ] **0.5 — Catch2 iskeleti.** `UUID`, `SceneSerializer`,
      `AssetManager` testleri.
- [ ] **0.6 — Faz 22 artıkları.** Inspector'da doku ayarları arayüzü,
      prefab / `StartScene` referanslarının GUID'e geçmesi,
      `AssetDirectory`'nin gerçekten kullanılması.

> **Neden 0.2 ve 0.3 Faz 20'den önce?** Undo komutları "hangi entity'ler
> üzerinde?" sorusunu cevaplar. Tek seçim varsayımıyla yazılan her komut,
> çoklu seçim gelince yeniden yazılır.

---

## Faz 13 — Girdi soyutlaması + olay sistemi

Şu an `EditorApp` doğrudan `SDL_GetKeyboardState` çağırıyor. Motor
kullanıcısının SDL bilmesi gerekmemeli.

- [ ] **13a** `FX::KeyCode` / `MouseButton` enum'ları + `FX::Input`
      (`IsKeyPressed`, `GetMousePosition`) — sorgu tabanlı katman
- [ ] **13b** Motor içi `Event` sınıfları (`WindowResize`, `KeyPressed`,
      `MouseMoved`) + SDL → motor olayı çeviri katmanı
- [ ] **13c** `Layer` / `LayerStack` — olayların katmanlar arası akışı
      ve tüketilmesi
- [ ] **13d** Girdi eşlemesi (action mapping): "Zıpla" = Space **veya**
      gamepad A

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

- [ ] **16a** `ScriptableEntity` taban sınıfı + `NativeScriptComponent`
      (`OnCreate / OnUpdate / OnDestroy`) + `ScriptSystem` (yalnız Play)
- [ ] **16b** Editör tarafı: script kaydı (factory), Inspector'da script
      seçimi, serileştirme
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
- [ ] Inspector'da doku ayarları arayüzü
- [ ] Prefab / `StartScene` referansları da GUID'e geçsin

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
| Dosya izleyici yok | Klasör dışarıdan değişirse panel **ve varlık tablosu** fark etmiyor; elle "Yenile" gerekiyor. | `ContentBrowserPanel.cpp`, `AssetManager.cpp` |
| Prefab bağlantısı yok | Örnek kaynağından bağımsız; kaynak değişince örnekler güncellenmiyor (override sistemi yok). | `PrefabSerializer.cpp` (Faz 12) |
| `GetRegistry()` çok açık | Registry'ye doğrudan `create`/`destroy` çağırmak UUID haritasını bozar. Daha dar bir erişim gerekebilir. | `Scene.h` (Faz 8) |
| `FollowSystem` Scene'e bağımlı | Diğer sistemler sadece registry alıyor, bu Scene alıyor (UUID haritası için). Test etmesi daha zor. | `Systems.cpp` (Faz 8) |
| Seçim `SceneHierarchyPanel` içinde | Viewport, gizmo, inspector, prefab hepsi okuyor; panel sahibi değil tüketicisi olmalı. Ayrı bir `SelectionContext` gerekiyor — çoklu seçimin ön adımı. | `SceneHierarchyPanel.h` (Faz 10) |
| Test yok | Hiç birim testi yok. Serializer ve matematik en çok fayda sağlayacak yerler. | — |
| Sadece Windows'ta denendi | Kod taşınabilir yazıldı ama Linux'ta hiç derlenmedi. | — |

---

# Güncel sıra

```
0.1 ✅ → 0.4 → 0.2 → 0.3 → 0.5
      → 13a-d → 16a-c → 0.6
      → 14 → 15 → 17a-d → 18b-d → 19 → 23 → 20a-d
```

**Neden bu sıra:**
- 0.4 önce, çünkü küçük ve varlık tablosunun diskle tutarlılığı şu an
  tamamen içerik panelinin doğru çağrı yapmasına bağlı — kırılgan.
- 0.2 → 0.3 → (…) → 20 zinciri kırılmamalı; Undo'yu tek seçim
  varsayımıyla yazmak tüm komutları çöpe atar.
- 0.6 (Faz 22 artıkları) Faz 16'dan sonraya bırakıldı: acil değil ve
  script fazı motoru gerçekten *oyun* çalıştırır hale getiriyor.
- 18b, 17c'nin ön koşulu (collider debug çizimi daire istiyor);
  18d, 0.4'ün dosya izleyicisini kullanıyor.
