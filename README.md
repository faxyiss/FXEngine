# FXEngine

Sıfırdan yazılan bir **2D oyun motoru + editör**. C++17, OpenGL 3.3, SDL3, EnTT.

> **Bu bir öğrenme projesidir.** Amaç hazır bir motoru kullanmak değil, motor
> mimarisinin *neden* öyle kurulduğunu anlamak. Üretimde kullanılmak üzere
> tasarlanmadı; Unity/Godot'a rakip olma iddiası yok. Değerli olan şey çıkan
> ikili dosya değil, o dosyaya giden yolda alınan kararlar ve onların yazılı
> gerekçeleri.

Bu amaç somut sonuçlar doğuruyor:

- **Hazır çözüm yerine açık çözüm.** Reflection kütüphanesi yerine elle yazılan
  meta tablo; ECS için EnTT gibi çekirdek bir parça alınıyor ama üstündeki her
  şey bize ait.
- **Her karar yazılıyor.** Kod yorumları *ne* yaptığını değil **neden** öyle
  yapıldığını anlatır. [`Notes/`](Notes/) altında 40+ faz notu her kararın
  gerekçesini, alınan hataları ve kök sebeplerini tutuyor.
- **Sessiz yanlışlık kabul edilmez.** Görünür uyarı, "çalışıyor gibi görünen"
  davranışa her zaman tercih edilir.

Kod yorumları ve notlar Türkçe. ~18.500 satır C++ (Engine + Editor + Tests).

**Kapsam dışı (bilinçli):** 3D, ağ, konsol platformları, çoklu render API'si.

---

## Bugün ne yapabiliyor

- **Proje sistemi** — `.fxproject`, karşılama ekranı, son projeler, GUID'li varlık
  veritabanı (`.meta`), dosya izleyici, taşıma referansları kırmıyor
- **Sahne düzenleme** — hiyerarşi, çoklu seçim, sürükle-bırak sıralama, gizmo,
  kopyala/yapıştır/çoğalt, tam Undo/Redo (alan + yapısal)
- **Ayrı Scene View / Game View** — gizmo ve ızgara yalnız Scene View'da
- **C++ script'ler ayrı DLL'de** — editörü kapatmadan **Derle** → saniyeler içinde
  yeni kod çalışıyor. Script alanları Inspector'dan ayarlanabiliyor
- **Prefab sistemi** — bağ, Revert, override takibi (sapan alanlar vurgulu),
  Apply (kaynağa yaz → tüm örnekler güncellenir, override'lar korunur)
- **Play/Stop** — kopya sahnede çalışır, düzenlenen sahne bozulmaz
- **Kurulabilir dağıtım** — `cmake --install` + CPack ZIP

**Testler:** 87 test / 446 assertion (Catch2, penceresiz çalışır).

## Neler yok

Fizik, animasyon, ses, ekranda metin ve render sıralama katmanı **henüz yok**.
Sıradaki işler: [`Notes/03-Yapilacaklar.md`](Notes/03-Yapilacaklar.md).

---

## Teknoloji

| Alan | Seçim | Neden |
|---|---|---|
| Dil | C++17 | Motor mimarisini "elle" görmek için |
| Grafik API | OpenGL 3.3 Core | Yaygın, öğrenmesi açık, tek API (soyutlama katmanı yok) |
| Pencere / Girdi | SDL3 | Platform katmanı; motorun içine **sızdırılmıyor** |
| GL loader | glad | GL 3.3 core + `GL_KHR_debug` |
| Editör UI | Dear ImGui (docking) + ImGuizmo | Immediate mode; editör aracı, motorun parçası değil |
| ECS | EnTT | Çekirdek parça; üstündeki sahne modeli bize ait |
| Matematik | glm | |
| Doku yükleme | stb_image | |
| Serileştirme | nlohmann/json | Okunabilir, elle düzenlenebilir dosya formatları |
| Test | Catch2 | |
| Build | CMake + FetchContent | Bağımlılıklar `_deps/` altında cache'lenir |

---

## Mimari

### Katmanlar ve değişmez kural

```
┌──────────────────────────────────────────────┐
│  Editor/   FXEditor.exe                      │
│  paneller, gizmo, EditorCamera, komutlar     │
└───────────────────┬──────────────────────────┘
                    │ (tek yönlü)
┌───────────────────▼──────────────────────────┐
│  Engine/   FXEngine.dll                      │
│  Core · Renderer · Scene · Asset · Events    │
└──────────────────────────────────────────────┘

Tests/  FXTests.exe  ───►  Engine   (Editor'ü bilmez)
Game.dll (kullanıcı projesi) ───► Engine
```

**`Engine/` içinde `Editor` kelimesi geçmez** — ne bir `#include`, ne bir link
satırı, ne bir kavram. Bu kuralın somut karşılığı: `FXTests` motoru pencere ve
OpenGL bağlamı olmadan test edebiliyor. Motor editöre sızsaydı bu imkânsız olurdu.

`Engine/include/` **PUBLIC**, `Engine/src/` **PRIVATE** olarak işaretli — public
API yüzeyi fiziksel olarak zorlanıyor.

### Taşıyıcı fikir: kimlik ile konum ayrı şeylerdir

Bu fikir dört kez, birbirinden bağımsız olarak uygulandı:

| Ne | Kimlik (kalıcı, diske yazılır) | Konum (geçici) |
|---|---|---|
| Entity | `UUID` — 64-bit | `entt::entity` (yüklemede değişir) |
| Proje | `.fxproject`'in klasörü | kurulu olduğun yer ≠ çalıştığın yer |
| Varlık | `AssetHandle` GUID (`.meta`'da) | dosya yolu (taşınabilir) |
| Script | adı (`"Spin"`) | C++ tipi (derlemeye özel) |

Ortak sebep: **diske yazılabilen tek şey kimliktir.** İşaretçi, `entt::entity`,
dosya yolu, fonksiyon adresi — hiçbiri yazılamaz veya yazılırsa bir sonraki
çalıştırmada yanlış olur.

### ECS ve component kuralı

Component **saf veridir**: metot yok, mantık yok, işaretçi yok, `std::function`
yok. Bedeli var (bazen dolaylı yol gerekiyor) ama karşılığı büyük: serileştirme
mekanik bir işe indirgeniyor. *İyi bir veri modelinin testi, serileştirilebilir
olmasıdır.*

Sistemler durumsuz statik fonksiyonlar; sıra tek bir yerde tanımlı:

```
Script → Follow → Movement → Transform
```

Script en önde (sonda olsaydı yazdıkları bir kare gecikirdi), Transform en sonda
(ondan öncekiler yerel konumu değiştirebilir).

### Component meta-veri tablosu

Yeni bir component eklemek eskiden dört yere dokunmayı gerektiriyordu ve biri
unutulunca hata **sessizdi** ("Play'e geçince kayboldu"). Artık her component
[`ComponentMeta.cpp`](Engine/src/Scene/ComponentMeta.cpp)'de **bir kez** tarif
ediliyor; serileştirme, kopyalama, Inspector çizimi ve Undo o tarifden üretiliyor.

```cpp
ComponentRegistry::Register<TransformComponent>("Transform", "Transform")
    .Field<&TransformComponent::Translation>("Translation", "Konum")
    .Field<&TransformComponent::Rotation>("Rotation", "Dönme (der)")
        .Degrees().Speed(1.0f)
    .NotRemovable().HiddenInAddMenu();
```

Makro sihri veya harici reflection kütüphanesi yok — açık ve okunabilir kalması,
hata mesajlarının anlaşılır olması ve bir gün bir dil köprüsünden (C#) geçebilmesi
için bilinçli seçim.

### Sabit adım (fixed timestep)

Oyun mantığı 1/60 sabit adımda çalışır: fizik ve oyun davranışı her makinede
birebir aynı olsun. `OnRender(alpha)` interpolasyon payını alır — bugün
kullanılmıyor ama imzada duruyor, çünkü sonradan eklemek her çağrı yerini
değiştirmek olurdu.

### Oyun kodu ayrı DLL (hot reload)

Asıl engel "DLL nasıl yüklenir" değil, **motor durumunun tekliği**ydi. `FXEngine`
statik olsaydı exe ve `Game.dll`, `ScriptRegistry`/`ComponentMeta`/`Project`
durumlarının **ayrı kopyalarını** taşırdı; DLL kendi kopyasına kaydeder, editör
hiç görmezdi. Çözüm: motoru `SHARED` yapmak.

Her proje kendi `Game.dll`'ini derliyor; editör onu gölge kopyayla yüklüyor
(dosya kilidi aşılıyor). Uyumsuz bir DLL **yüklenmeden önce** ABI damgasıyla
yakalanıyor (`FX_ENGINE_ABI_VERSION`) — eski bir DLL'in script'ini çalıştırmak
vtable uyuşmazlığı yüzünden çökme demek.

Script tespiti dosya adına değil **içeriğe** bakar: `FX::ScriptableEntity`'den
türeyen her sınıf adıyla kaydedilir, yardımcı header'lar derlemeyi kırmaz.

### Dosya formatları

| Dosya | Sürüm | İçerik |
|---|---|---|
| `.fxscene` | 5 | Entity listesi, UUID'ler, component'ler, hiyerarşi, prefab bağları |
| `.fxprefab` | 1 | Entity ağacı (örneklemede kimlikler yeniden üretilir) |
| `.fxproject` | 3 | Ad, varlık klasörü, `StartScene` (GUID), hedef çözünürlük |
| `*.meta` | 1 | Varlık GUID'i + içe aktarma ayarları |

Kurallar: **mutlak yol asla yazılmaz** (proje taşınabilir olmalı), her formatta
sürüm alanı var, eski sürümler okunup sessizce dönüştürülür. Hepsi okunabilir
JSON — metin editörüyle açılıp elle düzenlenebilir.

Ayrıntılı mimari: **[`Notes/02-Mimari.md`](Notes/02-Mimari.md)**

---

## Klasör yapısı

```
CMakeLists.txt              üst proje, kurulum + CPack
cmake/Dependencies.cmake    tüm 3rd-party bağımlılıklar
Engine/
  include/FXEngine/         public API  (Editor ve Game.dll bunu görür)
  src/                      private implementasyon
Editor/
  src/                      editör uygulaması (paneller, komutlar, gizmo)
  assets/shaders/           GLSL kaynakları
Tests/                      Catch2 birim testleri (yalnızca Engine'i test eder)
Notes/                      faz notları — kararlar, gerekçeler, hatalar
```

---

## Derleme

**Gereksinimler:** CMake 3.20+, C++17 derleyici, Python 3 (glad kod üreteci için),
OpenGL 3.3 destekleyen GPU. Windows'ta geliştiriliyor; **Linux/macOS'ta hiç
derlenmedi** (dosya diyalogları ve dosya izleyici o platformlarda boş gövde).

```powershell
git clone https://github.com/faxyiss/FXEngine.git
cd FXEngine
cmake -S . -B build
cmake --build build --config Debug
.\build\bin\FXEditor.exe
```

> İlk configure birkaç dakika sürer — 9 bağımlılık indirilir. Sonrakiler saniyeler
> alır; bağımlılıklar `_deps/` altında cache'lenir, `build/` silinse bile tekrar
> indirilmez.

### Testler

```powershell
cmake --build build --config Debug --target FXTests
.\build\bin\FXTests.exe
```

Testler **gerçek dosya sistemi** kullanır (geçici proje klasörleri), sahte katman
yok — test edilen şey zaten "diske doğru yazıyor muyuz".

### Kurulabilir paket

```powershell
cmake --build build --config Release
cmake --install build --config Release --prefix "C:\FXEngine-Kurulum"
# ya da ZIP:
cd build; cpack -G ZIP -C Release
```

Kurulum ağacı iki bölümlü: `bin/` (editör + DLL'ler + MSVC runtime) ve `sdk/`
(motor başlıkları, import lib ve **taşınabilir** CMake tarifi). Editör motor
tarifini önce `exe/../sdk/`'da arar, bulamazsa geliştirme ağacına düşer — aynı
exe iki düzende de çalışır.

**Hedef makinede CMake + MSVC Build Tools gerekir**: script'ler C++ ve `Game.dll`
orada derleniyor (Unreal'in Visual Studio şartıyla aynı durum). vcredist gerekmez.

Ayrıntı: [`Notes/Faz-Kurulum-Notlar.md`](Notes/Faz-Kurulum-Notlar.md)

---

## Kullanım

Editör panelleri sürüklenip yeniden düzenlenebilir; düzen `imgui.ini`'ye kaydedilir.

| Panel | İş |
|---|---|
| **Scene** | Düzenlenen sahne — gizmo, ızgara, seçim çerçevesi |
| **Game** | Sahne kamerasından oyun görüntüsü |
| **Hierarchy** | Entity ağacı, seçim, sürükle-bırak sıralama |
| **Inspector** | Component'ler, script alanları, prefab bağı |
| **Icerik** | Proje varlıkları — sürükle-bırak, kopyala/kes/yapıştır |
| **Derleme Konsolu** | `Game.dll` çıktısı; hatalar dosya + satır ile en üstte |
| **Istatistikler** | Draw call, quad sayısı, FPS, kamera, VSync |

Ayrıca **Proje Ayarları** (projeye ait: başlangıç sahnesi, çözünürlük) ve
**Tercihler** (kullanıcıya ait: tema, kamera hızı) ayrı pencereler — biri sürüm
kontrolüne girer, diğeri girmez.

### Kısayollar

| Tuş | İş |
|---|---|
| `Ctrl+N` / `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` | Yeni / aç / kaydet / farklı kaydet |
| `Ctrl+Z` / `Ctrl+Shift+Z` (veya `Ctrl+Y`) | Geri al / yinele |
| `Ctrl+C` `Ctrl+V` `Ctrl+D` | Kopyala / yapıştır / çoğalt |
| `Ctrl+I` | Varlık içe aktar (dosyayı pencereye sürüklemek de olur) |
| `X` `C` `B` | Gizmo: taşı / döndür / ölçekle (`Z` kapat, `Ctrl` kademeli) |
| `F` / `G` | Seçime odaklan / ızgarayı aç-kapa |
| `Delete` | Seçili entity'leri sil |
| `W` `A` `S` `D` / `Q` `E` / tekerlek | Kamera: hareket / döndür / yakınlaş |
| `ESC` | Çıkış |

> Gizmo ve kamera tuşları yalnızca imleç viewport üzerindeyken çalışır.
> `Ctrl+Z` **global**: Inspector'da düzenledikten sonra da geri alınabilir.
> `Space` duraklatmaz — oyun script'leri onu girdi olarak kullanıyor;
> duraklat/durdur oynatma şeridindeki düğmelerle.

### Script yazmak

İçerik panelinde sağ tık → **Yeni Script** (tek header ya da `.h` + `.cpp`).
Script `FX::ScriptableEntity`'den türeyen bir sınıftır:

```cpp
namespace FXGame
{
    class Player : public FX::ScriptableEntity
    {
    public:
        void OnReflect(FX::ScriptFieldVisitor& v) override
        {
            v.Visit("Hiz", m_Speed);      // Inspector'da görünür
            v.Visit("Hedef", m_Target);   // entity seçici
        }

    protected:
        void OnUpdate(float dt) override
        {
            auto& tf = GetComponent<FX::TransformComponent>();
            if (FX::Input::IsKeyPressed(FX::Key::D))
                tf.Translation.x += m_Speed * dt;
        }

    private:
        float         m_Speed = 5.0f;
        FX::EntityRef m_Target;
    };
}
```

Sonra oynatma şeridindeki **Derle**'ye bas — editör kapanmadan, sahne ve seçim
kaybolmadan yeni kod çalışır. Script'ten entity silme (`Destroy()`), spawn
(`Instantiate(prototip)`) ve entity referansı desteklenir.

---

## Notlar

[`Notes/`](Notes/) klasörü projenin amacının parçası — koddan daha az önemli değil:

| Dosya | İçerik |
|---|---|
| [`00-Durum-ve-Plan.md`](Notes/00-Durum-ve-Plan.md) | Devir notu: nerede kaldık, sıradaki iş |
| [`02-Mimari.md`](Notes/02-Mimari.md) | Mimarinin tamamı ve alınan yön kararları |
| [`03-Yapilacaklar.md`](Notes/03-Yapilacaklar.md) | Sıradaki somut işler |
| [`01-Yol-Haritasi-v2.md`](Notes/01-Yol-Haritasi-v2.md) | Faz planı |
| `Faz-*.md` | Her fazın kendi notu: kararlar, gerekçeler, bilerek yapılmayanlar |

Her faz notunda **"bilerek yapılmayanlar"** bölümü var — bir şeyin *neden*
yapılmadığı da bir karardır ve yazılmayı hak eder.

## Yol haritası

**Tamamlanan:** MVP (Faz 0–7) · UUID + hiyerarşi · Play/Stop · gizmo + seçim ·
varlık yönetimi + GUID · proje sistemi · girdi/olay ayrımı · script sistemi ·
editör temelleri (component meta, Game View, ayarlar, Undo/Redo) · oyun DLL'i +
hot reload · prefab sistemi (bağ → Revert → override → Apply) · kurulabilir dağıtım

**Sırada:** ekranda metin → render sıralama katmanı → fizik (Box2D) →
sprite sheet + animasyon → ses

**Ertelenen:** C# scripting. Ön koşulları tamamlandı ama karar "şimdi değil":
iterasyon hızı sorununu ayrı DLL zaten çözdü, oyun yapmayı engelleyen şey dil
değil eksik sistemler (fizik, animasyon, ses, metin). Gerekçe:
[`02-Mimari.md`](Notes/02-Mimari.md) K1.

## Lisans

MIT
