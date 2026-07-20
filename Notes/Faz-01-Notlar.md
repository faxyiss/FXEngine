# Faz 1 — Pencere + OpenGL Context + Ana Döngü

## Dosyalar
| Dosya | İş |
|---|---|
| `Engine/include/FXEngine/Core/Window.h` + `src/Core/Window.cpp` | SDL3 penceresi + GL 3.3 context sahipliği |
| `Engine/include/FXEngine/Core/Application.h` + `src/Core/Application.cpp` | Ana döngü, fixed timestep, olay dağıtımı |
| `Editor/src/EditorApp.h` / `.cpp` | Application'dan türeyen editör uygulaması |
| `Editor/src/main.cpp` | Sadece giriş noktası, 40 satır |

## Mimari kararlar

### Window ve Application neden ayrı?
- **Window** = *kaynağı sahiplenen*: pencere tutamacı + GL context. Tek işi var etmek/yok etmek.
- **Application** = *akışı yöneten*: döngü, olaylar, zamanlama.

Ayırmasaydık pencere olmadan çalışan bir mod (test, headless) ya da iki
pencere imkânsız olurdu. **"Kaynağı sahiplenen" ile "akışı yöneten" ayrılır**
— motor mimarisinin en çok tekrar eden kalıbı.

### Application soyut taban sınıf (Template Method)
Motor döngüyü **kurar** ama içinde ne olacağını **bilmez** →
`OnInit / OnUpdate / OnRender / OnEvent / OnShutdown` kancalarını Editor doldurur.
Bağımlılık yalnızca yukarıdan aşağı akar; Engine hâlâ Editor'ı tanımıyor.

## Fixed timestep

Naif döngü (`dt = geçen süre`) makineden makineye farklı davranır ve
bir kare takılırsa nesne duvardan geçer.

```
biriktirici += geçenSüre
while (biriktirici >= 1/60):
    OnUpdate(1/60)          ← dt HER ZAMAN aynı, deterministik
    biriktirici -= 1/60
OnRender(biriktirici / (1/60))   ← alpha
```

**Sonuç:** `OnUpdate` bir karede 0, 1 veya 3 kez çağrılabilir. `OnRender` tam 1 kez.

### İki koruma katmanı
1. `frameTime > 0.25f` → kırp. Debugger'da durunca 10 saniyelik dt gelmesin.
2. `s_MaxUpdatesPerFrame = 5` → tavana çarpınca kalan zamanı **at**.
   Tutarsan bir sonraki kare yine çarpar, program hiç yetişemez → **spiral of death**.

### alpha nedir?
İki mantık adımı arasındaki oran (0..1). Eski/yeni konum arasında
interpolasyon yapıp 60 Hz mantıkla 144 Hz akıcı görüntü almak için.
Faz 1'de kullanmıyoruz ama imzada duruyor — sonradan her yeri değiştirmeyelim diye.

## OpenGL ayrıntıları

### SDL_GL_SetAttribute pencereden ÖNCE
SDL bunları "bir sonraki context'i şu özelliklerle yarat" diye saklar.
Pencereden sonra çağırırsan **hiçbir etkisi olmaz**. Klasik tuzak.

### glad sırası
`glad.h` her zaman diğer GL header'larından **önce**.
`gladLoadGLLoader()` çağrılmadan **hiçbir `gl*` çağrısı yapılamaz** —
fonksiyon işaretçileri sürücüden çalışma zamanında doldurulur.
Ayrıca geçerli bir context **etkin** olmalı.

### Yıkım ters sırada
Önce context, sonra pencere. Ters yaparsan context sahipsiz kalır.

### High-DPI: pencere boyutu ≠ piksel sayısı
`SDL_GetWindowSizeInPixels()` kullan, `SDL_GetWindowSize()` değil.
OpenGL **piksel** ile çalışır. Karıştırmak "görüntü ekranın çeyreğine
çiziliyor" hatasının klasik sebebi.
Olayda da `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` kullanıyoruz, `RESIZED` değil.

## Karşılaşılan hatalar

### 1. `GL_DEBUG_TYPE_ERROR: bildirimi yapılmamış tanımlayıcı`
**Kök sebep:** glad'i uzantısız üretmişiz. `gl=3.3` sadece **çekirdek** API'yi
verir; `glDebugMessageCallback` ise `GL_KHR_debug` **uzantısında** yaşar
(GL 4.3'te çekirdeğe alınmış).

> Öğrenilecek: OpenGL'de "çekirdek sürüm" ve "uzantılar" ayrı şeylerdir.
> Loader'a hangi uzantıları istediğini açıkça söylemelisin.
> glad'de boş liste = "hiçbiri", "hepsi" değil.

**Çözüm:** `set(GLAD_EXTENSIONS "GL_KHR_debug" ...)` + `_deps/glad-build` sil, yeniden configure.

Ayrıca **iki ayrı çalışma zamanı kontrolü** gerekiyor:
- `GLAD_GL_KHR_debug` → uzantı bu makinede var mı?
- `GL_CONTEXT_FLAG_DEBUG_BIT` → sürücü gerçekten debug context verdi mi?

Derleyebilmek, desteklendiği anlamına gelmez.

### 2. `SDL_SetMainReady: tanımlayıcı bulunamadı`
SDL2'den davranış değişikliği: SDL3'te `<SDL3/SDL.h>` artık `SDL_main.h`'ı
kendiliğinden dahil etmiyor. Açıkça `#include <SDL3/SDL_main.h>` gerekli.

## Test sonucu (bu makine)
```
Uretici : NVIDIA Corporation
GPU     : NVIDIA GeForce RTX 3060 Ti
Surum   : 3.3.0 NVIDIA 591.86
GLSL    : 3.30
Debug   : OpenGL debug output AKTIF
```
4 saniyelik koşu: **156 güncelleme / 418 kare**
→ Monitör 165 Hz, çizim ~163 FPS, mantık sabit 60 Hz. **Fixed timestep çalışıyor.**

Ayrıca `Guncelleme yetismedi, 177.9 ms atildi` uyarısı görüldü (pencere odak
değişimi) → spiral of death koruması da devrede.

## Bilinen geçici durum
`Application::Run()` ve `EditorApp::OnRender()` ikisi de `glClear` yapıyor.
Faz 2'de temizleme mantığı bir `Renderer` sınıfına taşınacak, bu ikilik kalkacak.

## Onay
- [x] Derlendi
- [x] Çalıştı (context + döngü + olaylar)
- [ ] Kullanıcı onayı → Faz 2'ye geçiş
