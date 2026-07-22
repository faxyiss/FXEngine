# Faz — Kurulabilir Dağıtım (install + CPack)

> Amaç: FXEditor'ü geliştirme ağacından bağımsız, herhangi bir makineye
> kopyalanabilir/kurulabilir hale getirmek. `cmake --install` taşınabilir
> bir ağaç üretiyor, CPack onu ZIP'liyor.

Son güncelleme: 2026-07-23

---

## Sorun neydi

Editör, oyun projelerini `Game.dll`'e derlerken motoru **build ağacından**
buluyordu: `FX_ENGINE_BUILD_DIR` derleme zamanında gömülüyor ve
`FXEngineConfig-<Config>.cmake` kaynak/build ağacına mutlak yollar
taşıyordu. Editörü başka bir makineye kopyalasan açılırdı ama **Derle**
düğmesi ilk basışta kırılırdı.

## Çözüm — iki düzen, tek exe

### Kurulum ağacı (`cmake --install`)

```
<kurulum>/
  bin/   FXEditor.exe · FXEngine.dll · SDL3.dll · assets/ · MSVC runtime DLL'leri
  sdk/   FXEngineConfig.cmake        ← TAŞINABİLİR tarif
         include/FXEngine/...        ← motor public başlıkları
         lib/FXEngine.lib            ← import lib
         third_party/{glm, entt, json}
```

- **`sdk/FXEngineConfig.cmake`** (`cmake/FXEngineConfigInstalled.cmake`):
  her yol `CMAKE_CURRENT_LIST_DIR`'e göre — klasör nereye taşınırsa
  taşınsın çalışır. Yalnızca Release: `FX_RELEASE` + `/MD` eşleşir.
- **Üçüncü parti başlıklar:** public motor başlıkları yalnızca **glm,
  EnTT, nlohmann_json** içeriyor (grep ile doğrulandı) — SDL/glad
  sızmamış, SDK'ya girmiyorlar. `FILES_MATCHING` ile yalnız başlıklar
  kopyalanıyor.
- **MSVC runtime** (`InstallRequiredSystemLibraries`) `bin/`'e giriyor:
  hedef makine vcredist kurmak zorunda değil.

### Editör tarafı (`GameProject.cpp` → `EngineConfigPath`)

Önce `exe/../sdk/FXEngineConfig.cmake` denenir (kurulu düzen), yoksa
`FX_ENGINE_BUILD_DIR` (geliştirme). Aynı exe iki düzende de çalışır;
yol exe konumuna göre çözüldüğü için kurulum klasörü taşınabilir.

### Paket

`cpack -G ZIP -C Release` → `FXEngine-0.1.0-win64.zip` (~4,7 MB).
NSIS kuruluysa `-G NSIS` ile setup.exe da üretilebilir (yapılandırma
hazır: başlat menüsü kısayolu `bin/FXEditor.exe`).

## Doğrulama

1. `cmake --install build --config Release --prefix <geçici>` → ağaç tam
2. **Kurulu SDK'dan gerçek `Game.dll` derlendi**: editör şablonunun
   birebir kopyası bir `.fxbuild` iskelesi, `ENGINE_CONFIG` kurulu
   `sdk/`'ya işaret edecek şekilde configure+build edildi → başarılı
3. Kurulu `FXEditor.exe` açılış smoke testi (4 sn ayakta) → geçti
4. ZIP üretildi; Debug ortamı tam derlemeyle geri getirildi, 87 test geçiyor

## Hedef makine gereksinimleri

- **CMake + MSVC Build Tools** (ya da Visual Studio): script'ler C++,
  `Game.dll` hedef makinede derleniyor — Unreal'in VS şartıyla aynı durum.
  Editör açılır ama bunlar yoksa **Derle** çalışmaz (konsolda
  "cmake çalıştırılamadı" görünür).
- OpenGL 3.3 destekleyen GPU.
- vcredist **gerekmez** (runtime DLL'leri pakette).

## Bilinen sınırlar / dikkat

- **Debug/Release aynı `bin/`-`lib/`'i paylaşıyor** (bilinçli düzleştirme).
  Release paketledikten sonra Debug'a dönerken **tam Debug derlemesi** yap
  (`cmake --build build --config Debug`), yoksa karışık CRT lib'leri
  LNK4098 üretir.
- Exe simgesi yok (`.rc` dosyası eklenmedi) — varsayılan Windows simgesi.
- `.fxproject` dosya ilişkilendirmesi kurulumda yapılmıyor (NSIS'e
  eklenebilir; `FXEditor.exe <yol>` zaten çalışıyor).
- Kurulu dağıtım yalnızca Release taşır; Debug oyun derlemesi kurulu
  düzende desteklenmiyor (lib yok).
