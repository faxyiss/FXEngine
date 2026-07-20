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

Faz 10 (play modu) ──> Faz 16 (script)      — script'in "çalışıyor" hali olmalı
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

## Faz 9 — Parent / Child hiyerarşisi

Şu an tüm entity'ler düz bir liste. Bir tank + top namlusu yapmak istersen
namlunun tanka bağlı olması gerekir.

- [ ] `RelationshipComponent` — parent UUID, ilk çocuk, kardeş bağlantıları
- [ ] `Entity::SetParent / AddChild / GetChildren`
- [ ] **Dünya transform'u = parent zinciri × yerel transform**
- [ ] `TransformSystem` — hiyerarşiyi gezip dünya matrislerini hesaplar
- [ ] Hierarchy paneli ağaç olarak çizsin, sürükle-bırak ile parent değiştirme
- [ ] Parent silinince çocuklara ne olacak? (birlikte sil / köke taşı — **karar ver**)

**Öğrenilecek:** Yerel/dünya uzay ayrımı. Neden her karede zinciri
yeniden hesaplamak yerine "kirli bayrağı" (dirty flag) kullanılır.

**Dikkat:** Döngüsel parent (A'nın parent'ı B, B'nin parent'ı A) kontrolü şart —
sonsuz döngüye girer.

---

## Faz 10 — Play / Stop modu

Şu an editör ve oyun aynı şey. Gerçek bir editörde ▶ tuşuna basınca
oyun **kopya bir sahnede** çalışır, ⏹ deyince düzenleme hali geri gelir.

- [ ] `Scene::Copy()` — registry'nin derin kopyası
- [ ] `SceneState { Edit, Play, Simulate }`
- [ ] ▶ / ⏹ / ⏸ araç çubuğu
- [ ] Play'de: runtime kopya güncellenir; Stop'ta orijinale dönülür
- [ ] `CameraComponent` + `Scene::GetPrimaryCamera()`
      — Play modunda **sahnenin** kamerası, Edit'te editör kamerası
- [ ] Editör kamerası ayrı bir sınıfa (`EditorCamera`) taşınsın

**Öğrenilecek:** Neden oyunu düzenlenen sahnede çalıştırmak felakettir
(Unity'de "play modunda yaptığın değişiklikler kayboldu" travması).

---

## Faz 11 — Viewport'ta seçim + gizmo

Şu an entity'yi sadece Hierarchy listesinden seçebiliyorsun.

- [ ] Framebuffer'a **ikinci renk eki**: `R32I` formatında entity ID
- [ ] Fragment shader ikinci çıktıya entity ID yazsın
- [ ] `glReadPixels` ile fare altındaki pikselin ID'sini oku → seçim
- [ ] **ImGuizmo** entegrasyonu: taşı / döndür / ölçekle tutamakları
- [ ] `Q/W/E/R` kısayolları (gizmo modu)
- [ ] Seçili entity'ye turuncu çerçeve

**Öğrenilecek:** "Color picking" tekniği — GPU'yu bir *sorgu* aracı olarak
kullanmak. Framebuffer'ın çoklu ek (MRT) yeteneği.

**Bağımlılık:** ImGuizmo yeni bir bağımlılık (header-only, FetchContent).

---

## Faz 12 — Varlık yönetimi + prefab

Şu an sahne yolu **koda gömülü** (`assets/scenes/Sahne.fxscene`), texture
seçimi Inspector'da iki seçenekli bir combo.

- [ ] `Content Browser` paneli — `assets/` klasörünü gezen dosya ağacı
- [ ] Dosyadan viewport'a **sürükle-bırak** ile sprite oluşturma
- [ ] Yerel dosya diyalogları (Windows: `IFileDialog`, taşınabilir: `nfd`)
- [ ] `AssetManager` — `TextureLibrary`'nin genelleştirilmiş hâli
- [ ] **Prefab**: bir entity ağacını dosyaya kaydet, sahneye örnekle
- [ ] Son açılan sahneler listesi

**Öğrenilecek:** Varlık kimliği (asset handle) ile dosya yolu arasındaki fark.
Neath yol yerine UUID kullanmak taşımayı/yeniden adlandırmayı güvenli kılar.

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

## Faz 18 — Render iyileştirmeleri

- [ ] Çizgi ve daire primitifleri (`Renderer2D::DrawLine / DrawCircle`)
- [ ] Debug çizim katmanı (collider, ızgara, kamera sınırı)
- [ ] Sıralama katmanı (`SortingLayer` + `OrderInLayer`)
- [ ] Saydam nesneler için doğru sıralama (arkadan öne)
- [ ] Sonsuz ızgara (editör zemini)
- [ ] Shader hot reload — dosya değişince yeniden derle

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
| Editör kamerası `EditorApp` içinde | Ayrı `EditorCamera` sınıfına taşınmalı (Faz 10'da zaten gerekecek). | `EditorApp.cpp` |
| Sahne yolu koda gömülü | Faz 12'de dosya diyaloğuyla çözülecek. | `EditorApp.h` |
| Hata durumunda yedek doku yok | Texture yüklenemezse `nullptr`; mor "eksik doku" dokusu daha iyi olurdu. | `TextureLibrary.cpp` |
| `GetRegistry()` çok açık | Registry'ye doğrudan `create`/`destroy` çağırmak UUID haritasını bozar. Daha dar bir erişim gerekebilir. | `Scene.h` (Faz 8) |
| `FollowSystem` Scene'e bağımlı | Diğer sistemler sadece registry alıyor, bu Scene alıyor (UUID haritası için). Test etmesi daha zor. | `Systems.cpp` (Faz 8) |
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
