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

## B-3 — (sırada) Editör DLL'i yüklüyor

İlk iş plandaki en ciddi risk: **EnTT tip kimliğini DLL sınırında ölçmek.**
DLL'den `GetComponent<TransformComponent>()` çağırıp gerçekten aynı veriye
eriştiğini görmeden devam edilmeyecek.
