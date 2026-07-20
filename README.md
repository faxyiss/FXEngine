# FXEngine

Sıfırdan yazılan bir **2D oyun motoru + editör**. Öğrenme amaçlı bir proje:
amaç hazır bir motoru kullanmak değil, motor mimarisinin *neden* öyle
kurulduğunu anlamak.

Kod yorumları bilinçli olarak yoğun ve Türkçe — her kararın kısa gerekçesi
yanında duruyor.

## Teknoloji

| Alan | Seçim |
|---|---|
| Dil | C++17 |
| Grafik API | OpenGL 3.3 Core Profile |
| Pencere / Input | SDL3 |
| GL loader | glad |
| UI | Dear ImGui (docking) |
| ECS | EnTT |
| Matematik | glm |
| Texture | stb_image |
| Serileştirme | nlohmann/json |
| Build | CMake + FetchContent |

## Mimari kurallar

1. **Engine, Editor'ı hiç bilmez.** Tek yönlü bağımlılık — ve bu bir yazılı
   kural değil, build sistemiyle zorunlu kılınmış durum: Engine statik bir
   kütüphane, Editor ona link olan bir uygulama.
2. **Veri component'te, davranış system'de.** ECS merkezli.
3. **Erken optimizasyon yok.** Önce çalışsın, sonra profiling'e göre hızlansın.

## Klasör yapısı

```
CMakeLists.txt              üst proje, global ayarlar
cmake/Dependencies.cmake    tüm 3rd-party bağımlılıklar
Engine/
  include/FXEngine/         public API  (Editor bunu görür)
  src/                      private implementasyon
Editor/
  src/                      editör uygulaması
  assets/shaders/           GLSL kaynakları
Notes/                      faz notları — kavramlar, hatalar, çözümler
```

`Engine/include/` **PUBLIC**, `Engine/src/` **PRIVATE** olarak işaretli.
Editor sadece `#include <FXEngine/...>` yazabilir; public API yüzeyi
fiziksel olarak zorlanır.

## Fazlar

| Faz | Konu | Durum |
|-----|------|-------|
| 0 | CMake, klasör yapısı, bağımlılıklar | ✅ |
| 0.5 | Log sistemi + assert | ✅ |
| 1 | SDL3 pencere + GL 3.3 context + fixed-timestep döngü | ✅ |
| 2 | Shader + VBO/VAO/EBO + tek renkli quad | ✅ |
| 3 | stb_image texture + orthographic 2D kamera | ⬜ |
| 4 | Batch renderer (dinamik vertex buffer, texture slot'ları) | ⬜ |
| 5 | EnTT entegrasyonu (Transform, SpriteRenderer, Tag + system'ler) | ⬜ |
| 6 | ImGui docking + Viewport / Hierarchy / Inspector panelleri | ⬜ |
| 7 | JSON ile sahne kaydet/yükle → MVP | ⬜ |

## Derleme

**Gereksinimler:** CMake 3.20+, C++17 derleyici, Python 3 (glad kod üreteci
için), OpenGL 3.3 destekleyen GPU.

```bash
git clone https://github.com/faxyiss/FXEngine.git
cd FXEngine
cmake -S . -B build
cmake --build build --config Debug
```

Windows'ta Visual Studio generator ile:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\bin\FXEditor.exe
```

> İlk `cmake -S . -B build` birkaç dakika sürer — 7 bağımlılık indirilir.
> Sonraki configure'lar saniyeler alır. Bağımlılıklar `_deps/` altında
> cache'lenir, `build/` silinse bile tekrar indirilmez.

### Kontroller (Faz 2)

| Tuş | İş |
|---|---|
| `ESC` | Çıkış |
| `W` | Tel kafes (wireframe) aç/kapa |
| `A` | Animasyonu durdur/başlat |
| `V` | VSync aç/kapa |
| `SPACE` | İstatistikler |

## Notlar

`Notes/` klasöründe her fazın kendi dosyası var: hangi kavram neden öyle,
hangi hata alındı, kök sebep neydi. Kodun kendisi kadar bu notlar da
projenin amacının parçası.

## Lisans

MIT
