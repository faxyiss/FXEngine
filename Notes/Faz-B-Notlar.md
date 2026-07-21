# Aşama B — Uygulama notları

> Plan: [Asama-B-Plan.md](Asama-B-Plan.md). Bu dosya **uygulandıkça**
> büyüyor; her adımın ne yapıldığı, neyin kırıldığı ve nasıl doğrulandığı
> burada.

---

## B-1 — `FXEngine` statikten `SHARED`'e (2026-07-21) ✅

### Yapılan

`Engine/CMakeLists.txt`:

```cmake
add_library(FXEngine SHARED ...)
set_target_properties(FXEngine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
```

Başka bir dosyaya dokunulmadı — **bir istisna dışında**.

### Tek kırılan yer: `Project::s_Active`

`WINDOWS_EXPORT_ALL_SYMBOLS` **veri sembollerini dışa aktarmaz.** Plan bunu
öngörmüştü ("motorun paylaşılan durumu dosya kapsamlı, yalnızca fonksiyon
üzerinden erişiliyor") ve bir tanesi hariç doğruydu:

```cpp
// Project.h — ESKI
static const std::shared_ptr<Project>& GetActive() { return s_Active; }
static bool HasActive() { return s_Active != nullptr; }
```

Bu iki gövde **header'da inline** olduğu için Editor ve FXTests kendi
içlerinde derleniyor ve `s_Active` veri sembolüne doğrudan başvuruyorlardı:

```
error LNK2001: cozumlenmemis dis sembol
  "private: static class std::shared_ptr<class FX::Project> FX::Project::s_Active"
```

**Çözüm:** iki gövde `Project.cpp`'ye taşındı. Artık `s_Active`'e yalnızca
DLL'in içindeki kod dokunuyor; dışarısı dışa aktarılan fonksiyonu çağırıyor.

Diğer paylaşılan durumlar (`ScriptRegistry::s_Factories`,
`ComponentRegistry::Entries()`, `FileSystem::s_ProjectDir`,
`Renderer2D::s_Data`) zaten `.cpp` kapsamlıydı ve hepsine erişim dışa
aktarılan fonksiyonlardan geçiyordu — hiçbiri kırılmadı.

**Genel kural, bundan sonra geçerli:** motorun `static` **veri**'sine
header'daki bir gövdeden dokunma. Erişimci `.cpp`'de tanımlanmalı.

### Kırılmayan, kırılabilir sanılan şeyler

| Şüphe | Neden sorun çıkmadı |
|---|---|
| Editor'ün glad'ın ikinci kopyasını alması | Editor tek bir `gl*` sembolüne başvurmuyor; linker glad'ı exe'ye hiç çekmiyor |
| `static constexpr` üyeler (`kExtension`, `s_FixedDeltaTime`) | C++17'de implicit `inline`, her TU kendi kopyasını üretir, veri sembolü aranmıyor |
| CRT uyuşmazlığı | Her iki hedef de MSVC'nin varsayılan dinamik CRT'sini (`/MDd`) kullanıyor |
| DLL'in bulunamaması | `CMAKE_RUNTIME_OUTPUT_DIRECTORY` zaten `build/bin`; `FXEngine.dll`, `FXEditor.exe` ve `FXTests.exe` aynı klasörde doğuyor. Ek kopyalama adımı **gerekmedi** |

### Doğrulama

- `cmake --build build --config Debug` → **temiz**, sıfır uyarı/hata
- `build/bin/FXTests.exe` → **51 test / 238 assertion, hepsi yeşil**
  (B-1'in ölçütü buydu: davranış değişikliği sıfır)
- `build/bin/FXEngine.dll` üretiliyor (5.4 MB)
- `FXEditor.exe` açılıyor, 6 sn ayakta kalıyor, stderr boş

### Bilerek yapılmayanlar

- **Sürüm/SOVERSION ayarlanmadı.** DLL yalnızca kendi exe'lerimizin yanında
  taşınıyor, sistem geneline kurulmuyor.
- **Açık `FX_API` makrosu yazılmadı.** `WINDOWS_EXPORT_ALL_SYMBOLS` yetiyor.
  Veri sembolü dışa aktarmak gerekirse (bugün gerekmiyor) o zaman bakılır.
- **Linux `-fvisibility=hidden`** düşünülmedi; Linux'ta zaten hiç
  derlenmedi.

### Bedeli

`Faz 0`'ın "tek exe" sadeliği gitti. Editörü elle taşıyan biri artık
`FXEngine.dll`'i de götürmek zorunda. Kabul edildi — Aşama B'nin tamamı
buna bağlı.

---

## B-2 — `Game.dll` hedefi + proje script taraması (2026-07-21) ✅

### Cevaplanması gereken soru: `Game.dll`'in CMake projesi nerede yaşar?

Plan bunu açık bırakmıştı. İki seçenek vardı:

| Seçenek | Neden hayır / evet |
|---|---|
| FXEngine deposunda sabit bir `Game/` hedefi | Tek bir projeye çivilenirdi. Her projenin kendi oyun kodu var; ikinci projeyi açtığında hangi DLL? |
| **Her proje kendi `Game.dll`'ini derler** | ✅ Seçilen. Oyun kodu projeye ait, tıpkı sahneler ve dokular gibi |

Yani `Game.dll`'in derlemesi **ayrı bir CMake projesi** ve kullanıcının
proje klasöründe duruyor:

```
<proje>/
  assets/scripts/*.h      ← KULLANICININ sahiplendiği tek yer
  .fxbuild/               ← URETILEN iskele, elle düzenlenmez
    CMakeLists.txt
    GameMain.cpp
    GameRegistrations.h.in
    cmake/                ← CMake'in kendi çıktı ağacı
    out/Game.dll
```

`.fxbuild/` **üretiliyor, elle yazılmıyor** — proje her açıldığında
`GameProject::WriteBuildFiles()` tarafından. Gerekçe: motorun include
yolları veya kayıt mekanizması değiştiğinde altı ay önce oluşturulmuş bir
proje kırılmasın. Kullanıcının dokunduğu tek yer `assets/scripts/`.

### İkinci soru: oyun projesi motoru nasıl buluyor?

Naif çözüm, `.fxbuild/CMakeLists.txt` içine include yollarını elle
yazmaktı. Bu **aynı bilgiyi iki yerde tutmak** olurdu (§7) ve bir bağımlılık
eklendiği gün ayrışırdı.

Bunun yerine motor **kendini tarif eden bir dosya üretiyor**:

```cmake
# Engine/CMakeLists.txt
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/FXEngineConfig-$<CONFIG>.cmake" ...)
```

İçinde `FXEngine` adında bir `IMPORTED SHARED` hedef var; oyun projesi
onu `include()` edip `target_link_libraries(Game PRIVATE FXEngine)`
diyor. Bilgi tek yerden, hedefin kendisinden okunuyor.

Üç ayrıntı:

- **`INCLUDE_DIRECTORIES`, `INTERFACE_INCLUDE_DIRECTORIES` değil.** Yalnızca
  ilki transitif olarak hesaplanıyor; SDL3/glm/EnTT/json/glad yolları
  ancak böyle geliyor. Bedeli: `PRIVATE` olan `Engine/src` ve `stb` de
  listeye giriyor. `Engine/src` tüketen tarafta `list(FILTER ... EXCLUDE)`
  ile eleniyor.
- **Boş liste girdileri** (`;;`) kapalı generator ifadelerinden kalıyor ve
  CMake boş include yolunu hata sayıyor — onlar da eleniyor.
- **Dosya adında `$<CONFIG>`.** Debug ile Release'in hem `FX_DEBUG`'ı hem
  `.lib` yolu farklı. Editör kendi config'ine ait olanı seçiyor
  (`FX_BUILD_CONFIG` derleme tanımı).

### Kayıt: A4'ün mekanizması taşındı, yeniden yazılmadı

`.fxbuild/CMakeLists.txt`, A4'ün `Editor/CMakeLists.txt`'te yaptığının
aynısını yapıyor: `CONFIGURE_DEPENDS` ile klasörü tarar, her `<Ad>.h`
için bir include + bir `Register` satırı üretir. Planın öngördüğü gibi
üreteç **taşındı, silinmedi**.

**Namespace `FXEd` değil `FXGame`.** Proje script'i editöre değil oyuna
ait; `FXEd` (= FXEditor) orada yanlış isim olurdu. B-3'te `Spin`/`Move`
taşınırken bir kereye mahsus yeniden adlandırılacaklar.

### DLL'in dışa aktardığı tek sembol

```cpp
extern "C" __declspec(dllexport) void FXRegisterScripts();
```

`extern "C"`: `GetProcAddress`'e C++ mangling'i ile ad vermek derleyiciye
bağımlı olurdu. Bu sembol B-2'de yazıldı ama **editör onu henüz
yüklemiyor** — yükleme B-3.

### Yan kazanç: komut satırından proje açma

`FXEditor.exe <yol.fxproject>` artık karşılama ekranını atlayıp projeyi
açıyor. B-2'yi doğrulamanın tek yolu buydu (aksi halde her denemede GUI'den
proje seçmek gerekiyordu); dosya ilişkilendirmesi için de hazır duruyor.

### Doğrulama

| Ne | Sonuç |
|---|---|
| Editör projeyi açınca `.fxbuild/` iskelesi yazılıyor mu | ✅ üç dosya da yerinde |
| Script'i **olmayan** projede DLL derleniyor mu | ✅ boş `Game.dll` üretildi |
| `assets/scripts/Spin.h` eklenince | ✅ `GameRegistrations.h` içinde `Register<FXGame::Spin>("Spin")` |
| DLL motora gerçekten bağlanıyor mu | ✅ `Spin` motorun `ScriptableEntity`/`TransformComponent`/`FX_INFO`'sunu kullanıyor, link temiz |
| Sembol dışa aktarılmış mı | ✅ `dumpbin /exports` → `FXRegisterScripts` (bozulmamış ad) |
| Editör DLL'i yüklüyor mu | ✅ **hayır** — B-2'nin ölçütü buydu |
| `FXTests` | ✅ 51 test / 238 assertion |

### Bilerek yapılmayanlar

- **Editörden derleme tetiklenmiyor.** `cmake` elle çalıştırıldı; düğme B-5.
- **`.fxbuild/` proje `.gitignore`'una eklenmiyor.** Projelerin henüz
  `.gitignore`'u yok; proje şablonu konusu ayrı bir iş.
- **Kurulu (installed) dağıtım düşünülmedi.** `FX_ENGINE_BUILD_DIR`
  motorun build ağacını gösteriyor. Editör zaten derleyici şart koşuyor;
  gerçek bir dağıtımda yerine install ağacı geçer.

### Test artığı

`FXTest2` projesinin `assets/scripts/Spin.h`'i bu doğrulamadan kaldı.
B-3'ün başlangıç noktası olarak bilerek bırakıldı.

---

## B-3 — Editör DLL'i yüklüyor + EnTT risk ölçümü (2026-07-21) ✅

### En ciddi risk ölçüldü ve GEÇTİ

Aşama B'nin en ciddi riski EnTT tip kimliğinin DLL sınırında tutup
tutmadığıydı (`registry.get<T>()` tip kimliğine dayanıyor; iki tarafta
farklı çıkarsa component'ler **sessizce bulunamaz**). Plan "teoriye
güvenmeyeceğiz" diyordu. Ölçüm bir **gidiş-dönüş** olarak yazıldı:

```
Editör: entity + TransformComponent(x=42) yaratır, DLL'e Scene* + UUID gönderir
DLL:    FindEntityByUUID → GetComponent<TransformComponent>() → 42 görür → 99 yazar
Editör: aynı component'ten 99 okuyabiliyorsa geçti
```

Tek çağrı iki şeyi birden ölçüyor: **tip kimliği** (`get<TransformComponent>`
doğru storage'ı buluyor mu) ve **ortak bellek** (aynı heap). Sonuç:

```
Game.dll: EnTT tip kimligi ve veri erisimi DLL sinirinda dogrulandi.
Game.dll yuklendi, 1 script kayitli.
```

`FXEngineSelfTest`, `Game.dll`'in kalıcı bir export'u (`GameMain.cpp`
şablonunda). Her yüklemede çalışıyor — bir gün derleyici/EnTT/CRT
değişip kimlik bozulursa **sessiz component kaybı yerine görünür bir
hata** verecek (§7 "sessiz yanlışlık yerine görünür uyarı").

### DLL yükleme: `GameLibrary`

`Editor/src/Game/GameLibrary.{h,cpp}`. `LoadLibraryA` → `GetProcAddress`
→ `FreeLibrary`. `windows.h` header'a sızmıyor: `HMODULE` `.cpp`'de
`void*` olarak tutuluyor.

Yükleme sırası (B-4/B-5 bunu genişletecek):
```
Unload()  → ScriptRegistry::Clear() + FreeLibrary
LoadLibrary(<proje>/.fxbuild/out/Game.dll)
GetProcAddress FXRegisterScripts → çağır
```

Bu adımda DLL **doğrudan** yükleniyor (gölge kopya B-4). Windows dosyayı
kilitler; yeniden derleme henüz mümkün değil.

### Yükleme ne zaman

Proje açılışında (`OpenProject` ve `NewProject`), **sahne yüklenmeden
önce**: sahne dosyası script adlarını çözmeye çalışıyor, o an kayıtlı
olmalılar. `Game.dll` yoksa (henüz derlenmemiş proje) sessizce değil ama
hata da değil — `FX_INFO` ile bilgi, çünkü Derle düğmesi (B-5) gelene
kadar bu normal.

### Boşaltma sırası tuzağı (planın "sarkan örnek" riski)

Script örneklerinin vtable'ları `Game.dll`'i işaret ediyor. DLL sahneden
**önce** boşaltılırsa sahne yıkıcısı geçersiz vtable'lara dokunur. Üye
yıkım sırası bunu **yanlış** yapardı: `m_GameLibrary` sahnelerden sonra
tanımlı, dolayısıyla önce yıkılırdı. Bu yüzden `OnShutdown`'da **açıkça**
önce sahneler bırakılıyor, sonra `m_GameLibrary.Unload()`. Play sırasında
yeniden yükleme koruması ayrıca B-6'da.

### Küçük ama gerekli düzeltme

`RunSelfTest` ilk denemede assert patlattı: `CreateEntity` zaten bir
`TransformComponent` ekliyor, üzerine `AddComponent` "zaten var" assert'i.
`AddOrReplaceIfMissing` ile düzeltildi. (Motorun kendi davranışını
doğrulamak, onu kullanırken varsayımlarımızı da test ediyor.)

### Doğrulama

Temiz bir geçici proje (scratchpad/BTest, tek `Spin.h`) üzerinde:

| Ne | Sonuç |
|---|---|
| Self-test | ✅ "DLL sinirinda dogrulandi" |
| DLL script'i kayıtlı mı | ✅ 1 script (`Spin`, `FXGame::Spin`) |
| `FXTests` | ✅ 51 / 238 |
| Projesiz açılış | ✅ hâlâ açılıyor, builtin script'ler duruyor |

> **Not — FXTest2/assets/scripts/Spin.h şu an derlenmiyor** (kullanıcının
> düzenlemesi: eksik `;` ve `CameraComponent::Size` diye üye yok, doğrusu
> `OrthographicSize`). Doğrulama bu yüzden ayrı bir temiz projeyle yapıldı.
> Bu bozuk dosya B-5'in **derleme hatasını editörde göster** adımı için
> gerçek bir test örneği; bilerek dokunulmadı.

### Bilerek bu adımda YAPILMAYANLAR

- **Gölge kopya + Yeniden Yükle** → B-4.
- **Editörden Derle düğmesi** → B-5 (şimdilik `cmake` elle çalıştırıldı).
- **Editörün builtin script'lerinin kaldırılması** (Spin/Move) → B-6.
  Şu an projesiz modda hâlâ duruyorlar; proje açılınca `Clear()` ile
  yerlerini DLL'inkiler alıyor. "New Script"i projeye yönlendirmek de
  B-6'nın işi.

---

## B-6 — Editör builtin script'leri kaldırıldı (2026-07-21) ✅

> Not: plan B-6'yı "Play koruması + otomatik Stop" olarak numaralandırıyor;
> o iş **B-4 (gölge kopya) ile birlikte** yapılacak, çünkü ikisi de
> "açıkken yeniden yükleme" senaryosuna ait. Bu adım planın §B6
> "yerleşik script'ler örnek projeye taşınır" kararı.

### Yapılan

`Editor/src/Scripts/` **tamamen kaldırıldı** (Spin, Move, Jump ve A4
testinden kalan asd/asddsa). Onunla birlikte:

- `RegisterEditorScripts()` çağrısı ve `<ScriptRegistrations.h>` include'u
  `EditorApp.cpp`'den çıktı.
- A4'ün CMake kayıt üreteci ve `FX_EDITOR_SCRIPTS_DIR` `Editor/CMakeLists.txt`'ten
  çıktı. (Bu mekanizmanın kendisi silinmedi — B-2'de `.fxbuild/`'e taşınmıştı.)
- "Yeni Script" artık `<proje>/assets/scripts/<Ad>.h` yazıyor, namespace
  `FXGame`, ve **açık proje yoksa engelliyor** (script'in yazılacağı yer yok).
  Modal metni "editörü yeniden derle" yerine "oyunu (Game.dll) derle" diyor.

### Sonuç: editör artık hiç oyun kodu taşımıyor

Motor editörü bilmez (§7), editör de artık oyun kodunu bilmez. Üç katman
net ayrıldı: **motor → editör → oyun (DLL)**. Kavramsal olarak §B6'nın
savunduğu şey buydu.

### Kabul edilen sonuç: projesiz modda script yok

Karşılama ekranındaki "projesiz devam et" ile açılan örnek sahnede
`NativeScriptComponent` seçenekleri boş. Doğrulandı: projesiz açılışta
hiç script kaydı yok. Plan bunu bilerek kabul etmişti — o mod geçici bir
kolaylık.

### Doğrulama

| Ne | Sonuç |
|---|---|
| Derleme (Scripts/ ve üreteç olmadan) | ✅ temiz |
| `FXTests` | ✅ 51 / 238 |
| Projesiz açılış | ✅ açılıyor, **0 script** |
| BTest projesi | ✅ Game.dll yüklendi, self-test geçti, `Spin` kayıtlı |

### Not

`build/Editor/generated/ScriptRegistrations.h` eski bir üretim artığı
olarak build ağacında kalmış olabilir; artık hiçbir yerden include
edilmediği için zararsız, temiz derlemede zaten üretilmiyor.

---

## B-4 — Gölge kopya (2026-07-21) ✅

Windows yüklü bir DLL'i kilitler; üzerine yeniden derlenemez. `GameLibrary::Load`
artık orijinali doğrudan yüklemiyor: bir **kopyasını** yükleyip orijinali
serbest bırakıyor.

```
<proje>/.fxbuild/out/Game.dll         ← cmake bunu yazar (hiç kilitli değil)
<proje>/.fxbuild/out/loaded/Game.dll  ← editörün LoadLibrary ettiği kopya
```

Kopya **aynı adla bir alt klasörde** (`loaded/`), `Game.loaded.dll` gibi
uzantı-oynaması yapılmadı. Sebep: MSVC `.dll` içine gömdüğü PDB yolunu
`"Game.pdb"` olarak yazıyor; aynı adla kopyalarsak (ve `Game.pdb`'yi de
yanına koyarsak) debugger sembolleri buluyor — VS'ten editöre attach edip
script'e breakpoint koymak çalışır (planın "hata ayıklama sınırı" notu).

**Doğrulama (headless):** BTest editörde açık ve gölge kopyadan yüklüyken
(`loaded/Game.dll` + `Game.pdb` oluştu, self-test geçti), **orijinal
`Game.dll` editör kapanmadan yeniden derlendi** — `LNK1168` (yazma için
açılamıyor) yok. Gölge kopyanın tüm amacı buydu.

---

## B-5 — Editörden "Derle" düğmesi + konsol + Play koruması (2026-07-21) ✅

### Akış

Oynatma şeridinde sağda **"Derle"** düğmesi (ve "Oyun" menüsünde "Derle
ve Yeniden Yükle"). `BuildGame()`:

```
Play modundaysan → önce Stop (kullanıcıya söylenerek)
GameProject::Build(config)  → cmake configure (gerekirse) + cmake --build
  başarılıysa → LoadGameLibrary() (gölge kopyayı tazele + kayıtları oku)
  başarısızsa → konsolda hata, editör ayakta
```

**Play koruması** planın kesin sırası: Play sırasında yeniden yükleme
vtable'ları geçersiz kılar, o yüzden Derle önce Stop ediyor. Gölge kopya
sayesinde **derlemenin kendisi** için boşaltma gerekmiyor (orijinal zaten
kilitli değil); yalnızca **yeniden yükleme** Play'i bekleyemez.

### cmake alt süreci

`GameProject::Build` → `RunCommand` (`CreateProcessA` + `CREATE_NO_WINDOW`,
tek boruya stdout+stderr). Konsol penceresi açılıp kapanmıyor. Boru okuma
ucu miras alınmıyor (yoksa çocuk kapanmadan `ReadFile` EOF görmez ve
kilitleniriz), yazma ucunu `CreateProcess`'ten sonra ebeveyn kapatıyor.

**Senkron:** derleme bitene kadar editör bloklanıyor (~birkaç sn). İlk
sürüm için kabul; async + ilerleme çubuğu sonraya.

### Derleme konsolu

`Derleme Konsolu` paneli: cmake çıktısını olduğu gibi, kaydırılabilir,
üstte BAŞARILI/BAŞARISIZ renkli durum. Derleme bitince otomatik açılıp
en alta kayıyor. "Oyun" menüsünden de açılıp kapanabiliyor.

### Doğrulama

| Ne | Sonuç |
|---|---|
| Derleme (B-4+B-5) | ✅ temiz |
| `FXTests` | ✅ 51 / 238 |
| Gölge kopya yükleniyor + self-test | ✅ (BTest) |
| Editör açıkken orijinal rebuild | ✅ kilit yok |

**GUI adımları kullanıcıda** (motorun kuralı: GUI testini simüle etmem):
- BTest aç → `Spin.h`'ta `m_Speed` değerini değiştir → **Derle** → ~sn →
  Play'de yeni hız (editör kapanmadı).
- Play'deyken Derle → önce Stop olmalı, sonra derleme.
- **FXTest2 aç → Derle** → konsolda derleme hatası görünmeli (senin
  `Spin.h`'ın eksik `;` ve `CameraComponent::Size` — doğrusu
  `OrthographicSize`). Editör çökmemeli. Bu, B-5'in hata-raporlama testi.

---

## Aşama B tamamlandı

`B-1` (SHARED) · `B-2` (Game.dll hedefi) · `B-3` (DLL yükleme + EnTT
ölçümü) · `B-6` (builtin temizlik) · `B-4` (gölge kopya) · `B-5` (Derle
düğmesi). Başarı ölçütü: **editör açıkken script'i değiştir → Derle →
sahne/seçim kaybolmadan yeni kod çalışsın** — makinede son GUI adımı
kullanıcının onayını bekliyor.

Bilerek yapılmayanlar (planın "kapsam dışı"ı korunuyor): otomatik derleme
(dosya izleyiciyle), script alanlarının Inspector'dan ayarlanması,
script'lerin kendi component'lerini tanımlaması, Linux `dlopen`.

---

## B sonrası kullanım düzeltmeleri (2026-07-21)

Kullanıcı ilk gerçek script'i yazıp test ederken üç düzeltme istedi:

1. **Space artık oyunu duraklatmıyor.** `OnKeyPressed`'te Space →
   `m_ScenePaused` kısayolu kaldırıldı; script'ler Space'i girdi olarak
   kullanıyor (zıplama vb.). Duraklat/Stop yalnızca oynatma şeridindeki
   düğmelerle. (Space + sol tuş kamera kaydırma `EditorCamera`'da, durdu.)

2. **Game panelinin araç çubuğu görünmüyordu.** Eski satır-içi sürüm
   (`SetCursorPos` + inline widget'lar) panelde hiç çizilmiyordu. Scene'in
   çalışan **yüzen overlay** desenine geçirildi: `DrawGameViewToolbar(anchorX,
   anchorY)` artık yarı saydam bir child pencere, görüntünün üstünde, panel
   sol üstünde. En/boy oranı combo'su + "Ayarlar..." artık görünüyor.

3. **İçerik panelinde istediğin klasörde script.** Boş alana sağ tık →
   "Yeni Script..." → o an gezilen klasöre oluşturuyor
   (`ContentBrowserPanel::TakeNewScriptRequest` → `m_NewScriptDir`). Modal
   hedef klasörü gösteriyor. Menüdeki "Dosya → Yeni Script" varsayılan
   `assets/scripts`'e yazmaya devam ediyor.

   **Bunu mümkün kılan asıl değişiklik:** `Game.dll` derlemesindeki script
   taraması artık **özyinelemeli** (`GLOB_RECURSE`, `assets/` ağacının
   tamamı). Önceden yalnız `assets/scripts/*.h` taranıyordu; alt klasördeki
   bir script sessizce derlenmezdi. Konvansiyon: `assets/` altındaki her
   `.h`, `FXGame::<DosyaAdi>` adında bir `ScriptableEntity` türevi içermeli.

**Doğrulama:** temiz derleme; `assets/dusmanlar/Takipci.h` (alt klasör)
+ `assets/scripts/` script'leri birlikte tarandı, **3 script** kayıtlı,
Game.dll temiz derlendi, self-test geçti, `FXTests` 51/238. Space/Game
toolbar/sağ-tık davranışının görsel onayı kullanıcıda.
