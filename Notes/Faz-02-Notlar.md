# Faz 2 — Shader + VBO/VAO/EBO + Tek Renkli Quad

## Dosyalar
| Dosya | İş |
|---|---|
| `Renderer/Shader.h/.cpp` | GLSL derle + link + uniform yönetimi |
| `Renderer/Buffer.h/.cpp` | VertexBuffer, IndexBuffer, BufferLayout |
| `Renderer/VertexArray.h/.cpp` | VAO — verinin nasıl okunacağının tarifi |
| `Renderer/RenderCommand.h/.cpp` | Tüm `gl*` çağrılarının tek kapısı |
| `Editor/assets/shaders/FlatColor.vert/.frag` | GLSL kaynakları |

## Dört nesne, dört farklı iş

| Nesne | Ne saklar |
|---|---|
| **VBO** | Ham köşe verisi — anlamsız sayı yığını |
| **EBO** | "Şu köşeleri şu sırayla kullan" listesi |
| **VAO** | Sayıların **anlamı** + hangi EBO |
| **Shader** | GPU'da çalışan program |

### ⚠️ En kritik kavram: VAO veri tutmaz
VAO, VBO'daki sayıların **nasıl okunacağını** tutar:
"0 numaralı attribute = 3 float, 12 bayt aralıkla, 0. bayttan başla".
Yeni başlayanların en çok karıştırdığı yer burası.

### Neden EBO?
Kare = 2 üçgen = 6 köşe, ama karenin 4 köşesi var. 2 köşe iki üçgende
de kullanılıyor. EBO olmadan %50 israf; köşe büyüdükçe (pos+uv+renk+normal)
tasarruf artar.

```
1 ----- 2      Üçgen A: 0, 1, 2
| \     |      Üçgen B: 2, 3, 0
|   \   |      → 6 indeks, 4 köşe verisi
0 ----- 3
```

## Öğrenilen kritik detaylar

### 1. OpenGL bir DURUM MAKİNESİDİR
Nesneyi fonksiyona parametre olarak vermezsin: önce `Bind()` ile "aktif"
yaparsın, sonra üzerinde işlem yaparsın. Bu yüzden **sıra kritiktir**.

### 2. `glVertexAttribPointer` aktif VAO'ya yazar
Önce VAO bind, **sonra** VBO bind, sonra attrib pointer.
VAO bağlı değilse ayar hiçbir yere kaydedilmez.

### 3. `GL_ELEMENT_ARRAY_BUFFER` bağlantısı VAO durumunun parçasıdır
VAO bağlıyken EBO bind edersen, o VAO artık o EBO'yu **hatırlar**.
VAO'nun az bilinen ama çok önemli özelliği.

### 4. `glEnableVertexAttribArray` unutulursa ekran boş kalır
Attribute'lar varsayılan olarak **kapalıdır**.

### 5. Shader hata logunu okumak ZORUNLU
`glGetShaderInfoLog` bloğunu yazmazsan shader hatası sessizce geçer,
ekran siyah kalır, sebebini asla bulamazsın.
**Shader hata ayıklamanın %90'ı o birkaç satırdır.**

### 6. `glGetUniformLocation` → -1 iki sebepten gelir
1. İsmi yanlış yazdın
2. Uniform shader'da tanımlı ama **kullanılmıyor** → derleyici onu atmış

İkincisi çok kafa karıştırır: kod doğru görünür ama uniform yoktur.

### 7. Uniform location önbelleklenir
`glGetUniformLocation` sürücüde string karşılaştırması yapar.
Her karede yüzlerce kez çağırmak israf → `unordered_map` ile cache.

### 8. `GL_STATIC_DRAW` bir zorunluluk değil, **ipucu**
Sürücüye "bir kez yazacağım, çok kez okuyacağım" der; belleği nereye
koyacağına buna göre karar verir. Faz 4'te `GL_DYNAMIC_DRAW` kullanacağız.

### 9. `vec4(a_Position, 1.0)` — son bileşen neden 1.0?
`w=1.0` → bu bir **konum**. `w=0.0` → bu bir **yön**, taşıma matrisi
etkilemez. Homojen koordinatların püf noktası.

### 10. `gl_FragColor` öldü
GL 3.3 Core'da fragment çıktısını kendin tanımlarsın: `out vec4 o_Color;`

## Mimari

### RenderCommand neden var?
Faz 1'de hem `Application` hem `EditorApp` `glClear` çağırıyordu — aynı iş
iki yerde, hangisi kazandığı belirsiz. Bu tipik bir **sızıntı**: grafik API
çağrıları kodun her yerine dağılırsa sonra hiçbir şey değiştirilemez.

→ `Application::Run()` içindeki `glClear` **kaldırıldı**. Temizleme artık
türetilen sınıfın kararı. Sebep: Faz 6'da framebuffer'a çizeceğiz ve
"her karede ekranı temizle" varsayımı yanlış olacak.

### BufferLayout neden ayrı bir soyutlama?
Alternatifi her yeni köşe formatında `glVertexAttribPointer` çağrılarını
elle yazmak, offset ve stride'ı **elle hesaplamak**. Bu hesaplar sessizce
yanlış yapılır (bir float kaydırırsın, model bozulur, sebebini bulamazsın).
Layout bunu tek yerde ve otomatik yapar.

### Shader'lar neden dosyada, koda gömülü değil?
Dosyada olunca shader'ı değiştirip **yeniden derlemeden** çalıştırabilirsin.
CMake `copy_directory_if_different` ile assets/ klasörünü exe'nin yanına kopyalar.

## Karşılaşılan hata: `Shader dosyasi acilamadi`

**Belirti:** `build\bin` içinden çalıştırınca çalışıyor, proje kökünden
çalıştırınca shader bulunamıyor.

**Kök sebep:** Göreceli yollar **çalışma dizinine** (current working directory)
göre çözülür, exe'nin yerine göre değil. Çalışma dizini ise programı nasıl
başlattığına göre değişir:

| Başlatma şekli | Çalışma dizini |
|---|---|
| `.\build\bin\FXEditor.exe` | `C:\FXEngine` ✗ |
| `cd build\bin; .\FXEditor.exe` | `C:\FXEngine\build\bin` ✓ |
| Visual Studio F5 | proje klasörü ✗ |
| Exe'ye çift tıklama | exe'nin klasörü ✓ |

Aynı program, aynı dosyalar — ama bazen çalışıyor bazen çalışmıyor.
**Oyun geliştirmede klasik tuzak.**

**Çözüm:** `Engine/Core/FileSystem.h/.cpp` eklendi.
`SDL_GetBasePath()` ile **exe'nin bulunduğu klasör** bulunur, tüm varlık
yolları ona göre çözülür. Exe nerede duruyorsa `assets/` de yanındadır —
bu her zaman doğrudur.

### Bu düzeltmede öğrenilenler

**1. "Fonksiyon içinde static" kalıbı**
```cpp
static std::string s_BaseDir = []() { ... }();
```
İlk çağrıda oluşur, program boyunca yaşar, C++11'den beri thread-güvenli.
Global değişken yerine bunu kullanmak *static initialization order fiasco*'yu
tamamen ortadan kaldırır: değişken ilk **kullanımda** oluşur, ne önce ne sonra.

**2. SDL2 → SDL3 farkı**
SDL2'de `SDL_GetBasePath()` sonucu `SDL_free()` edilmeliydi.
SDL3'te işaretçi SDL'e ait, **free ETME**. Geçişte kolayca gözden kaçar.

**3. Hata mesajı ne aradığını söylemeli**
Yeni mesaj artık aranan klasörün tam yolunu basıyor. "Bulunamadı" demek yetmez;
"şurada aradım" demek hatayı saniyeler içinde çözülebilir yapar.

## Bilinen sınırlama (kasıtlı)
Koordinatlar doğrudan **NDC**'de (-1..+1). Kamera yok → **en-boy oranı
düzeltilmiyor**, bu yüzden 1280×720 pencerede kare değil dikdörtgen görünür.
Faz 3'te orthographic kamera ile düzeltilecek.

## Test sonucu
```
Shader 'FlatColor' hazir (program id=3)
RenderCommand hazir (blend acik, derinlik testi acik)
166 guncelleme / 460 kare (VSync 165 Hz)
```
GL debug callback aktif ve **hiç hata basmadı** → çizim çağrıları temiz.

## Onay
- [x] Derlendi
- [x] Çalıştı, shader link oldu, GL hatası yok
- [ ] Kullanıcı onayı → Faz 3'e geçiş
