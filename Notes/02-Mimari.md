# FXEngine — Temel Mimari

> Bu belge motorun **nasıl kurulduğunu ve neden öyle kurulduğunu** anlatır.
> Devir notu (`00-Durum-ve-Plan.md`) "nerede kaldık" der; yol haritası
> (`01-Yol-Haritasi-v2.md`) "sırada ne var" der. Bu belge ise **kalıcı
> olanı** anlatır: bir şey eklerken uyman gereken kurallar.
>
> Son güncelleme: 2026-07-21

---

## 0. Motorun amacı ve sınırları

FXEngine bir **2D oyun motoru + editör**. Öğrenme amaçlı: hedef hazır bir
motoru kullanmak değil, motor mimarisinin *neden* öyle kurulduğunu anlamak.

Bu amaç şu kararları doğuruyor:

- **Hazır çözüm yerine açık çözüm.** Reflection kütüphanesi yerine elle
  yazılan meta tablo; ECS için EnTT gibi çekirdek bir parça alınıyor ama
  üstündeki her şey bize ait.
- **Her karar yazılıyor.** Kod yorumları *neden*i anlatır, `Notes/` altında
  faz notları kararların gerekçesini tutar.
- **Sessiz yanlışlık kabul edilmez.** Görünür uyarı, sessiz "çalışıyor
  gibi görünen" davranışa tercih edilir.

Kapsam dışı (bilinçli): 3D, ağ, konsol platformları, çoklu render API'si.

---

## 1. Katmanlar ve bağımlılık kuralı

```
┌──────────────────────────────────────────────┐
│  Editor/   FXEditor.exe                      │
│  paneller, gizmo, EditorCamera, dosya diyalog│
└───────────────────┬──────────────────────────┘
                    │ (tek yönlü)
┌───────────────────▼──────────────────────────┐
│  Engine/   FXEngine.lib                      │
│  Core · Renderer · Scene · Asset · Events    │
└──────────────────────────────────────────────┘

Tests/  FXTests.exe  ───► Engine  (Editor'ü bilmez)
```

**Değişmez kural:** `Engine/` içinde `Editor` kelimesi geçmez. Ne bir
`#include`, ne bir link satırı, ne bir kavram.

Bu kuralın karşılığı somut: `FXTests` motoru pencere ve OpenGL bağlamı
olmadan test edebiliyor. Motor editöre sızsaydı bu imkânsız olurdu.

### Engine'in iç katmanları

| Katman | Sorumluluk | Bilmediği |
|---|---|---|
| `Core/` | Ana döngü, pencere, girdi, olaylar, dosya sistemi, proje, UUID, log | Sahne, render detayları |
| `Renderer/` | Batch'li 2D çizim, doku, shader, framebuffer, kamera | Sahne, entity |
| `Scene/` | ECS, component'ler, sistemler, script, serileştirme | Pencere, editör |
| `Asset/` | GUID ↔ yol tablosu, `.meta` | Sahne içeriği |

Aşağıdaki yukarıyı bilmez. `Renderer2D`'ye quad verilir, o sahneyi
tanımaz. Bu yüzden `SpriteRenderSystem` sahnedeki veriyi okuyup
renderer'a *çeviren* yer olarak `Scene/` altında durur.

---

## 2. Ana döngü ve zaman

```
Application::Run()
  └── while (running)
        ├── ProcessEvents()        SDL kuyruğu → OnRawEvent + OnEvent
        ├── while (accumulator >= FIXED_DT)   ← sabit adım
        │     └── OnUpdate(FIXED_DT)
        ├── OnRender(alpha)
        └── SwapBuffers()
```

**Sabit adım (1/60)** bilinçli: fizik ve oyun mantığı her makinede birebir
aynı çalışsın. Değişken adım kullanılsaydı 144 Hz'de farklı, 60 Hz'de
farklı sonuç alınırdı — Faz 17'de (fizik) bunun karşılığı doğrudan
görünecek.

`OnRender(alpha)` interpolasyon payı alır; bugün kullanılmıyor ama
imzada duruyor çünkü sonradan eklemek her çağrı yerini değiştirmek olurdu.

---

## 3. Sahne modeli — ECS

EnTT üzerine kurulu, üç parçalı ayrım:

### Entity — kimlik, veri değil

`Entity` iki alan taşır: `entt::entity` + `Scene*`. Kopyalaması ucuz,
değer olarak geçirilir. Sahneye **sahip olmaz**, referans verir (ham
işaretçi; `shared_ptr` dairesel sahiplik yaratırdı).

### Component — saf veri

```cpp
struct TransformComponent {
    glm::vec3 Translation{0.0f};
    float     Rotation = 0.0f;
    glm::vec2 Scale{1.0f};
};
```

İçinde mantık, işaretçi, `std::function` veya sisteme referans **yok**.
Bu kuralın bedeli var (bazen dolaylı yol gerekiyor) ama karşılığı büyük:
serileştirme mekanik bir işe indirgeniyor. Faz 7'de öğrenilen ders şuydu —
*iyi bir veri modelinin testi, serileştirilebilir olmasıdır.*

`NativeScriptComponent` bu kuralın sınav vakasıydı: script bir *nesne*,
veri değil. Çözüm, script'i component **yapmak** yerine component'in
**içinde taşımak** oldu; component yalnızca bir ad tutuyor.

### System — durumsuz mantık

```cpp
class MovementSystem {
public:
    static void Update(entt::registry& registry, float dt);
};
```

Statik fonksiyonlar, durum yok. Aynı girdi → aynı çıktı. Tek başına test
edilebilir: sahte bir registry kurup çağırmak yeterli.

**İstisna:** `ScriptSystem` durum yaratır (script örnekleri heap'te yaşar),
bu yüzden üç giriş noktası var: `OnRuntimeStart` / `Update` /
`OnRuntimeStop`. İstisna olduğu kodda açıkça yazılı.

### Sistem sırası — Scene'in varlık sebebi

```
Script → Follow → Movement → Transform
```

Script hızı/hedefi belirler, Follow hız yazar, Movement konuma uygular,
Transform dünya matrislerini hesaplar.

- **Script en önde:** sonda çalışsaydı yazdıkları bir kare gecikmeyle
  görünürdü.
- **Transform en sonda:** ondan önceki her şey yerel konumu değiştirebilir.

Bu sıra `Scene::OnUpdate` içinde tek bir yerde tanımlı. Sıranın kendisi
mimari bir karardır; her sistemin kendi başına çağrılması sırayı
belirsizleştirirdi.

### Hiyerarşi

`RelationshipComponent` (parent UUID + çocuk UUID listesi) ve
`WorldTransformComponent` (hesaplanmış dünya matrisi). Dünya transform'u
= parent zinciri × yerel transform, `TransformSystem` tarafından kökten
yaprağa hesaplanır.

Parent silinince çocuklar da silinir (Unity/Godot davranışı) — sahnede
öksüz kalan nesne, kullanıcının "sildim ama duruyor" demesine yol açardı.

---

## 4. Kimlik sistemleri — motorun taşıyıcı fikri

**Kimlik ile konum ayrı şeylerdir.** Bu fikir dört kez uygulandı:

| Ne | Kimlik (kalıcı, diske yazılır) | Konum (geçici) | Faz |
|---|---|---|---|
| Entity | `UUID` — 64-bit rastgele | `entt::entity` (yüklemede değişir) | 8 |
| Proje | `.fxproject`'in bulunduğu klasör | kurulu olduğun yer ≠ çalıştığın yer | 21 |
| Varlık | `AssetHandle` GUID (`.meta`'da) | dosya yolu (taşınabilir) | 22 |
| Script | adı (`"Spin"`) | C++ tipi (derlemeye özel) | 16b |

Ortak sebep: **diske yazılabilen tek şey kimliktir.** İşaretçi,
`entt::entity`, dosya yolu, fonksiyon adresi — hiçbiri yazılamaz veya
yazılırsa bir sonraki çalıştırmada yanlış olur.

Yeni bir referans türü eklerken sorulacak soru: *bunun kimliği ne, konumu
ne?*

---

## 5. Varlık sistemi

```
proje/
  assets/
    textures/kahraman.png
    textures/kahraman.png.meta   ← { Guid, Type, import ayarları }
    scenes/ana.fxscene
```

- `AssetManager` proje açılışında bir kez tarar, GUID ↔ yol tablosunu kurar
- `.meta` yoksa üretilir; **varsa asla değiştirilmez** (GUID sabit kalmalı)
- `FileWatcher` diski izler: dışarıdan taşıma/silme/ekleme tabloya işlenir
- Taşıma GUID'i korur → sahnedeki referanslar kopmaz

`.meta` dosyaları **sürüm kontrolüne girer**. Kaybolursa varlık yeni GUID
alır ve tüm referanslar kopar.

**Bilinen tuzak:** motor varlıklarının `.meta`'ları kaynakta durmalı;
`build/` altında üretilip kaynağa yazılmadığında her temiz derlemede yeni
GUID alınıyordu (0.1'de yakalandı).

---

## 6. Girdi ve olaylar

İki ayrı mekanizma, bilinçli olarak birleştirilmedi:

```
FX::Input   sorgu   "şu an basılı mı?"     → sürekli hareket (WASD)
FX::Event   olay    "az önce bastı mı?"    → bir kez olan (Ctrl+S)
```

Tek API'ye sıkıştırmak ikisini de kötü yapardı.

### SDL izolasyonu

```
SDL_Event → EventTranslator (Engine/src/) → FX::Event
```

`SDL_Event` yalnızca `Engine/src/` içinde görünür. `Application` iki kanca
sunar:

| Kanca | Kim kullanır |
|---|---|
| `OnRawEvent(const SDL_Event&)` | ImGui backend'i, OS sürükle-bırak |
| `OnEvent(FX::Event&)` | Diğer her şey |

`FX::Key` değerleri SDL scancode'larıyla **kasten aynı** — çeviri tek bir
cast. Sızan şey SDL'in kendisi değil sayıları; birim testleri bu eşleşmeyi
koruyor.

ImGui bir olayı tükettiğinde `MarkRawEventHandled()` çağrılır, motor olayı
`Handled=true` gelir. Bu olmadan Inspector'a `s` yazmak sahneyi kaydederdi.

---

## 7. Render

```
Renderer2D
  ├── quad batch     (10.000 quad, 32 texture slotu)
  └── çizgi batch    (2.000 çizgi, GL_LINES, index'siz)
```

- Batch: aynı doku setini kullanan quad'lar tek draw call'da
- Çizgi ayrı batch: farklı primitif, index buffer'ı yok
- `Framebuffer` iki renk eki taşır: renk + `R32I` entity ID (color picking)

**Bilinen borç:** `Renderer2D` global durum (`s_Data` statik). İkinci bir
renderer örneği imkânsız, test etmek zor. Batch bölme yolu da hacky
(`EndScene` çağırıp elle durum düzeltiliyor).

---

## 8. Editör mimarisi

```
EditorApp
├── SelectionContext      ← seçimin TEK sahibi
├── SceneHierarchyPanel   (+ Inspector)
├── ContentBrowserPanel   (+ FileWatcher)
├── EditorCamera
├── ImGuiLayer
└── Framebuffer (viewport)
```

**Paneller tüketicidir, sahip değil.** Seçim 0.2'ye kadar panelin içindeydi
ve viewport/gizmo/inspector/prefab hepsi ondan okuyordu — çıkarıldı.
Sahiplik `EditorApp`'te.

### Play/Stop — kopya sahne

```
m_EditorScene ── Scene::Copy() ──► m_RuntimeScene
   (dokunulmaz)                      (oyun burada)
```

Stop'ta kopya atılır. "Play modunda yaptığın değişiklikler kayboldu"
travmasının çözümü: orijinale hiç dokunulmaz, geri alma işlemi gerekmez.

`Scene::Copy` UUID'leri **korur** (prefab örneklemenin tersi — orada
kimlikler yeniden üretilir). Script örnekleri kopyalanmaz, yalnızca
bağlantıları; ham işaretçi kopyalansaydı iki sahne aynı nesneyi gösterip
ikisi de silmeye çalışırdı.

### Editörde tekrar eden kalıplar

- **Yapıyı değiştiren işlemler döngü dışında.** ImGui listesi gezilirken
  `rename`/`destroy` iteratörleri bozar; istekler biriktirilip çerçeve
  sonunda işlenir.
- **Modal diyalog ImGui çerçevesinin ortasında açılmaz.** Yerel diyalog
  programı bloklar; istek bayrağa yazılır, çerçeve bitince açılır.
- **Aynı bilgi iki yerde durmasın.** `AllComponents`, `EntitySerialization`,
  `DrawEntryInteractions` hep bu yüzden tek yerde.

---

## 9. Serileştirme ve dosya formatları

| Dosya | Sürüm | İçerik |
|---|---|---|
| `.fxscene` | 4 | Entity listesi, UUID'ler, component'ler, hiyerarşi |
| `.fxprefab` | 1 | Entity ağacı (örneklemede kimlikler yeniden üretilir) |
| `.fxproject` | 2 | Ad, varlık klasörü, `StartScene` (GUID) |
| `*.meta` | 1 | Varlık GUID'i + içe aktarma ayarları |

Kurallar:

- **Mutlak yol asla yazılmaz.** Proje taşınabilir olmalı.
- **Sürüm alanı her formatta var.** Eski sürümler okunur ve sessizce
  dönüştürülür.
- **Yeni alan eklemek sürüm artırmaz** (eski dosya yine açılır); anlam
  değişikliği artırır (`StartScene`: yol → GUID = sürüm 2).
- `SceneSerializer` dokuyu **yüklemez**, kütüphaneye sorar. Bu sayede
  testler pencere olmadan çalışıyor.

---

## 10. Script sistemi

```
ScriptRegistry     ad → sınıf tablosu     (kayıt uygulamanın işi)
ScriptableEntity   OnCreate/OnUpdate/OnDestroy  (protected — motor çağırır)
NativeScriptComponent   { ScriptName, Instance* }
ScriptSystem       Start/Update/Stop — örneklerin yaşam döngüsü
```

Yaşam döngüsü **Scene**'de (`OnRuntimeStart/Stop`), editörde değil:
"sahnenin çalışıyor olması" sahnenin durumu. `Scene` yıkıcısı da
temizliyor.

Script'ler yalnızca Play–Stop arasında yaşar. Edit modunda çalışsalardı
düzenlerken nesneler kaçardı (Faz 10'un dersi).

Bilinmeyen script adı component'i **silmez** — sahne başka bir derlemeden
gelmiş olabilir; ad korunur, görünür uyarı verilir.

---

## 11. Test stratejisi

| Katman | Nasıl |
|---|---|
| `Engine/` çekirdek | Catch2 birim testleri (`FXTests`, 41 test) |
| Renderer, `FileWatcher` | Test dışı (GL bağlamı / zamana bağlı) |
| Editör | Elle, madde madde test listesiyle |

Testler **gerçek dosya sistemi** kullanır (`TempProject`), sahte katman
yok — test edilen şey zaten "diske doğru yazıyor muyuz".

Testler penceresiz çalışır; `SceneSerializer` testlerinde `TextureLibrary`
olarak `nullptr` geçilir. Bu mümkün, çünkü serializer dokuyu kendi
yüklemiyor.

---

# Bölüm II — Alınan yön kararları (2026-07-21)

## K1. Script dili: C# ertelendi, mimari hazırlanıyor

**Karar:** C# scripting şimdi yapılmayacak; ama ona giden yol döşenecek.

**Gerekçe:**

- C# entegrasyonu (Mono/CoreCLR host etme, internal call köprüsü, GC ↔
  entity yaşam döngüsü, assembly hot reload, kullanıcı projesini derleme,
  Inspector'da C# alanları) 1–2 aylık bir alt sistem. Şimdi başlamak
  editörün temel araçlarını o kadar dondurur.
- **C#'ın ön koşulu component meta-veri tablosu.** C# tarafına component
  açmak için zaten o tabloya ihtiyaç var. Önce onu yapmak C#'ı ucuzlatır.
- Asıl acı nokta dil değil **iterasyon hızı**: bugün script yazmak motoru
  yeniden derlemeyi gerektiriyor. Bu, oyun kodunu ayrı bir DLL'e alarak
  C++'ta kalarak da büyük ölçüde çözülebilir.

**Sonuç:** Aşama A (editör temelleri) → Aşama B (oyun DLL'i + hot reload)
→ Aşama C (C# kararı yeniden değerlendirilir).

**Hazır tutulacaklar:** `ScriptRegistry` zaten ad tabanlı (dil-bağımsız).
Component meta tablosu C# köprüsünün dayanacağı yapı olarak tasarlanacak.

## K2. Component meta-veri sistemi — elle yazılan kayıt

**Sorun:** Yeni bir component eklemek dört yere dokunmayı gerektiriyor:
`Components.h`, `AllComponents`, `EntitySerialization`, Inspector'daki
elle yazılmış blok. Biri unutulunca hata **sessiz**: "Play'e geçince
kayboldu", "kaydedince ayar gitti".

**Karar:** Her component tek bir yerde tanımlanacak — ad, alan listesi
(ad/tip/aralık), serileştirme, Inspector çizimi. Yöntem: **elle yazılan
kayıt + alan listesi.** Makro sihri yok, harici reflection kütüphanesi yok.

**Gerekçe:** Açık ve okunabilir kalır (öğrenme amacı), C# köprüsüne
açılması kolaydır, hata mesajları anlaşılır olur.

**Bu tek iş beş şeyi çözüyor:**
1. Yeni component = tek yere yazmak
2. Inspector otomatik çizim
3. **Script alanlarının Inspector'dan ayarlanması** (bugün koda gömülü)
4. Çoklu seçimde çok hedefli düzenleme
5. Undo/Redo için generic "alanı değiştir" komutu

## K3. Game View / Scene View ayrımı

**Karar:** Ayrı bir Game View penceresi; sahne kamerasından render.

**Getireceği netlik:**
- "Hangi kamera çiziyor?" sorusu bugün belirsiz
- Gizmo, ızgara, seçim çerçevesi **yalnızca Scene View'da** — bugün oyun
  görüntüsünde de çiziliyor
- Hedef çözünürlük / aspect ratio kavramı doğar → Project Settings'in ilk
  gerçek müşterisi

**Kararlaştırılacak detaylar:** Play'e basınca otomatik Game View'e geçiş,
"Maximize on Play", sabit aspect mi serbest mi.

## K4. Project Settings ve Preferences ayrımı

Bugün ayarlar üç yere dağılmış: `.fxproject`, `editor.json`, koda gömülü
sabitler.

| | Project Settings | Preferences |
|---|---|---|
| Kime ait | **Projeye** | **Kullanıcıya/makineye** |
| Sürüm kontrolü | Girer | Girmez |
| Nerede | `ProjectSettings/*.json` | `editor.json` / `%APPDATA%` |
| İçerik | Başlangıç sahnesi, hedef çözünürlük, fizik, sıralama katmanları | Tema, kısayollar, kamera hızı, snap varsayılanları |

**Neden önemli:** Yanlış tarafa konan ayar takım çalışmasında ya sürekli
çakışır ya da kaybolur.

## K5. Script dosyası oluşturma

İçerik panelinde sağ tık → "Yeni Script" → şablon üret → sistem
editöründe aç.

**Dürüst sınır:** C++'ta yeni script'in derlenmesi motorun yeniden
derlenmesini gerektiriyor. Bu, Aşama B'deki oyun DLL'i ile çözülür
(DLL'in CMake'i klasörü tarar, yalnızca DLL derlenir).

## K6. Örnek oyun (16c) ertelendi

Amacı "hangi API'ler eksik" sorusunu çarparak öğrenmekti — ilk tur cevabı
zaten alındı (Game View yok, settings yok, component sistemi hantal,
script dosyası oluşturulamıyor). Araçlar bitince yazılacak; ikinci tur
eksikler (çarpışma, animasyon, ses, sahne geçişi) ancak orada görünür.

---

# Bölüm III — Sıradaki yol

## Aşama A — Editör temelleri

| # | İş | Neden bu sırada |
|---|---|---|
| ~~**A1**~~ | ✅ Component meta-veri sistemi — [Faz-A1-Notlar.md](Faz-A1-Notlar.md) | Diğer dördünü de ucuzlatıyor |
| ~~**A2**~~ | ✅ Game View / Scene View ayrımı — [Faz-A2-Notlar.md](Faz-A2-Notlar.md) | Kamera ve çözünürlük kavramlarını netleştirir |
| **A3** | Project Settings + Preferences | A2'nin doğurduğu ayarların yeri |
| **A4** | Script dosyası oluşturma + şablon | İterasyonun ilk adımı |
| **A5** | Undo/Redo (eski 20a-20b) | A1'den sonra çok ucuzlar |

## Aşama B — İterasyon hızı

Oyun kodunu ayrı DLL'e alma + hot reload. C++ kalır, script değişimi
~5 saniyeye iner.

## Aşama C — Script dili kararı

A ve B bittiğinde C# entegrasyonu hem daha ucuz olur hem de gerçekten
gerekli olup olmadığı belli olur.

## Sonra (mevcut yol haritasından)

`16c` örnek oyun → `14` sprite sheet → `15` animasyon → `18b` DrawCircle →
`17a-d` fizik → `18c` sıralama → `19` metin → `23` ses → `18d` shader hot
reload → `13c/13d` layer + action mapping → `20c/20d` profiling + CI

---

# Bölüm IV — Yeni bir şey eklerken

1. **Hangi katmana ait?** Motor mu editör mü? Motor editörü bilmez.
2. **Kimliği ne, konumu ne?** Diske yazılacaksa kimlik yazılır.
3. **Component ise saf veri mi?** İşaretçi/mantık taşıyorsa tasarımı
   yeniden düşün.
4. **Aynı bilgi ikinci bir yerde duruyor mu?** Duruyorsa er geç ayrışır.
5. **Sessizce yanlış olabilir mi?** Olabiliyorsa görünür uyarı ekle.
6. **Test edilebilir mi?** Motor çekirdeğine ekliyorsan birim testi yaz.
7. **Faz notu ve yol haritası güncellendi mi?**
