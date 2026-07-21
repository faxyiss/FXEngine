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

## B-2 — (sırada) Boş `Game.dll` hedefi
