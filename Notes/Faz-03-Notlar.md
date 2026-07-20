# Faz 3 — Texture + Orthographic 2D Kamera

## Dosyalar
| Dosya | İş |
|---|---|
| `Renderer/Texture.h/.cpp` | stb_image ile yükleme, GL texture nesnesi, filtre/wrap |
| `Renderer/OrthographicCamera.h/.cpp` | View + Projection matrisleri |
| `Editor/assets/shaders/Texture.vert/.frag` | UV'li shader, tint, tiling |
| `Editor/assets/textures/*.png` | Test dokuları (PowerShell ile üretildi) |
| `Core/Application.h` | `OnWindowResize` kancası eklendi |

## Kamera: neden gerekliydi?

Faz 2'de koordinatlar doğrudan NDC'deydi (-1..+1). NDC her zaman ekranın
tamamını kaplar, **en-boy oranını umursamaz** → kare, dikdörtgen görünüyordu.

### Üç uzay, üç matris
```
Yerel (model) --[Transform]--> Dünya --[View]--> Kamera --[Projection]--> Clip
```
Shader'da tek satır:
```glsl
gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
```

### Neden ortografik, perspektif değil?
Perspektifte uzaktaki nesne küçülür. 2D'de bunu istemeyiz — sprite,
kameradan ne kadar uzakta olursa olsun aynı boyutta çizilmeli.

### En-boy oranı düzeltmesi
```cpp
SetProjection(-aspect * zoom, aspect * zoom, -zoom, zoom);
```
Dikeyde sabit birim göster, yatayı `aspect` ile genişlet.
**Faz 2'deki bozukluk tam olarak bu çarpanın eksikliğiydi.**

Sonuç: pencereyi yatayda genişletince nesneler büyümez, **daha fazla alan**
görürsün. Çoğu 2D oyunun istediği davranış.

### View matrisi kamerayı değil DÜNYAYI hareket ettirir
Kamerayı sağa kaydırmak = dünyayı sola kaydırmak.
Bu yüzden kameranın transform'u kurulup **tersi** alınır (`glm::inverse`).

### ⚠️ `Projection * View` sırası
Matris çarpımı değişmeli değildir. Ters yazarsan derlenir ama
"ekranda hiçbir şey yok" ile karşılaşırsın.

## Texture

### UV koordinatları
```
(0,1) ┌─────┐ (1,1)      Her zaman 0..1 arası.
      │     │            Resmin boyutundan bağımsız —
(0,0) └─────┘ (1,0)      512x512 de olsa 4096x1024 de olsa aynı.
```
4 köşeye UV veriyoruz; aradaki milyonlarca pikseli GPU **interpolasyonla**
dolduruyor. Vertex shader'daki `out vec2 v_TexCoord` bunu sağlıyor.

### ⚠️ Dikey çevirme — en klasik 2D tuzağı
- PNG/JPG görüntüyü **üst** satırdan saklar (ekran mantığı, y aşağı artar)
- OpenGL texture'ı **alt** satırdan sayar — UV (0,0) sol **alt** köşe

Çevirmezsen her sprite baş aşağı çıkar. `stbi_set_flip_vertically_on_load(1)`.
Bu ayar **global** durumdur, texture başına değil.

> Test dokusunun sol üstünde kırmızı, sol altında yeşil kare var —
> ekranda kırmızı üstte görünüyorsa yönelim doğrudur.

### Filtreleme: Nearest vs Linear
| | Ne yapar | Uygun |
|---|---|---|
| `Nearest` | En yakın pikseli aynen al → keskin, bloklu | **pixel-art** |
| `Linear` | 4 komşunun ağırlıklı ortalaması → yumuşak | fotografik |

MIN filtresi küçültmede (mipmap devrede), MAG büyütmede kullanılır.
MAG için mipmap **anlamsızdır** — o yüzden `useMipmap=false` geçiyoruz.

### Wrap modu
- `Repeat` → desen tekrarlanır (zemin, duvar). Tiling bununla çalışır.
- `ClampToEdge` → kenar pikselini uzat. **Tek sprite için doğru seçim.**

Tek sprite'ta `Repeat` kullanırsan kenarlarda karşı taraftan sızıntı olur.

### ⚠️ `GL_UNPACK_ALIGNMENT` tuzağı
OpenGL varsayılan olarak her satırın **4 baytın katı** olduğunu varsayar.
3 kanallı (RGB) ve genişliği 4'ün katı olmayan bir görüntüde
(örn. 254×254 RGB) görüntü **kayarak, eğik çizgiler halinde** bozulur.
→ `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)`

### Texture slot'ları
```cpp
glActiveTexture(GL_TEXTURE0 + slot);  // 1) hangi slot aktif
glBindTexture(GL_TEXTURE_2D, id);     // 2) o slota hangi texture
```
`glActiveTexture`'ı unutursan her texture 0. slota gider, sadece sonuncusu
görünür. `sampler2D` uniform'u texture'ın kendisi değil, **slot numarasıdır**.

### stb_image
`STB_IMAGE_IMPLEMENTATION` **tek bir .cpp'de** tanımlanmalı.
İki yerde tanımlarsan "duplicate symbol" linker hatası.
Bellek `stbi_image_free()` ile bırakılır — `delete[]`/`free()` değil.

## Shader ayrıntıları

### `discard` neden gerekli?
Blending açıkken saydam pikseller **yine de derinlik tamponuna yazılır**.
Bu, arkalarındaki nesnelerin çizilmemesine yol açar — "görünmez dikdörtgen
diğerlerini kesiyor" hatası. `if (a < 0.01) discard;` pikseli tamamen iptal eder.

### Tint (`u_Color`) çarparak uygulanır
Tek bir gri sprite'ı kırmızı/yeşil/mavi olarak tekrar tekrar kullanabilirsin.
3 ayrı PNG tutmak yerine 1 tane tutup renklendirmek — bellek tasarrufu.

## Girdi: olay mı, durum mu?

| | Ne söyler | Uygun |
|---|---|---|
| `OnEvent` | "tuşa **basıldı**" (bir kez) | menü açma, zıpla, ateş et |
| `SDL_GetKeyboardState` | "tuş **şu an** basılı mı" | sürekli hareket |

Hareketi `OnEvent`'e yazsaydık işletim sisteminin tuş tekrar hızına bağımlı,
tutuk bir hareket olurdu. Hareket her karede `dt` ile ölçeklenmeli.

### İki küçük ama önemli detay
1. **Çapraz normalizasyonu:** `(1,1)` vektörünün uzunluğu 1.41. Düzeltmezsen
   çapraz hareket düz hareketten %41 hızlı olur. Klasik oyun hatası.
2. **Zoom çarparak ölçeklenir** (`*= 0.9`), çıkararak değil. Zoom logaritmik
   algılanır; sabit çıkarma yakınken çok hızlı, uzaktayken çok yavaş hissettirir.

## Sıfıra bölme koruması
Pencere simge durumuna küçültülünce yükseklik 0 gelebilir →
`w/h` = NaN → **tüm matris bozulur, ekran kararır**. Bulunması zor bir hata.
`UpdateCameraProjection()` içinde `if (h <= 0) return;`

## Bilinen durum: 5 draw call
Bu fazda kare başına 5 ayrı çizim çağrısı var. Her biri CPU-GPU arası ayrı
iletişim. 5 için sorun değil, **5000 sprite için felaket**.
Faz 4'te batch renderer ile hepsi tek çağrıya inecek.

## Test sonucu
```
Shader 'Texture' hazir (program id=3)
Texture yuklendi: checkerboard.png (256x256, 4 kanal, id=1)
Texture yuklendi: circle.png (256x256, 4 kanal, id=2)
162 guncelleme / 448 kare
```
GL debug callback aktif, **hiç hata veya uniform uyarısı basmadı**.

## Onay
- [x] Derlendi
- [x] Çalıştı, texture'lar yüklendi, GL hatası yok
- [ ] Kullanıcı onayı → Faz 4'e geçiş
