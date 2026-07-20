# Faz 4 — Batch Renderer

## Problem: draw call maliyeti

Modern bir GPU saniyede **milyonlarca üçgen** çizebilir ama ancak
**birkaç bin draw call** kaldırabilir. 10.000 sprite'ı tek tek çizersen
darboğaz GPU değil, **CPU'nun sürücüyle konuşması** olur — GPU boşta
beklerken CPU tıkanır.

Her `glDrawElements`: sürücü durum doğrulaması + komut tamponuna yazma +
bazen GPU senkronizasyonu.

## Çözüm ve üç sonucu

CPU'da büyük bir köşe dizisi doldur, kare sonunda hepsini bir kerede
gönder, **tek** draw call yap.

| Faz 3'te | Faz 4'te | Neden |
|---|---|---|
| `u_Transform` uniform | CPU'da hesaplanıp köşeye yazılır | Tek draw call'da binlerce quad aynı uniform'u paylaşamaz |
| `u_Color` uniform | Köşe verisine girer | Aynı sebep |
| `u_Texture` uniform | **Texture slot'ları** | 32 texture 32 slota bağlanır, köşe slot numarasını taşır |

### Köşe boyutu takası
Faz 3: 20 bayt (pos + uv)
Faz 4: **44 bayt** (pos + renk + uv + texIndex + tiling)

Bedeli bant genişliği, kazancı draw call. Bu takas neredeyse her zaman
lehimize — bant genişliği bol, draw call pahalı.

## Kilit fikirler

### 1. Index buffer BİR KEZ üretilir
Quad'ların indeks deseni her zaman aynı, sadece 4'er kayar:
```
quad 0 -> 0,1,2, 2,3,0
quad 1 -> 4,5,6, 6,7,4
quad 2 -> 8,9,10, 10,11,8
```
10.000 quad'lık indeks dizisi başta bir kez üretilip GPU'ya yüklenir,
**bir daha hiç dokunulmaz**. Her karede sadece köşe verisi değişir.

### 2. Beyaz texture hilesi
Düz renkli quad'lar için 1×1 beyaz texture. Beyazla çarpmak rengi
değiştirmez → düz renk elde edilir.

Alternatif shader'da `if (hasTexture)` diye dallanmaktı. **GPU'da dallanma
pahalıdır**; bu yöntem hem daha basit hem daha hızlı. Her quad tek yoldan geçer.

### 3. `GL_DYNAMIC_DRAW` + `glBufferSubData`
- `glBufferData(nullptr)` → sadece yer ayır
- `glBufferSubData` → aynı belleğe yaz

Her karede `glBufferData` çağırsaydık sürücü belleği **yeniden tahsis**
ederdi (eskisini boşalt, yenisini ayır) — pahalı.

### 4. Sadece dolu kısım gönderilir
Tampon 10.000 quad'lık ama 50 quad çizdiysek 50 quad'lık veri gider.
Tamamını göndermek her karede 1.7 MB boşuna trafik olurdu.

### 5. Batch'in iki kırılma noktası
1. **Dizi doldu** (`QuadIndexCount >= MAX_INDICES`)
2. **Slotlar doldu** (33. farklı texture)

İkisinde de otomatik `Flush()` + yeni batch. Kullanıcı kodu bunu hiç bilmez.

### 6. `flat` interpolasyon niteleyicisi
```glsl
flat out float v_TexIndex;
```
`TexIndex` bir slot **numarasıdır**, bir ölçüm değil. İnterpole edilirse
iki köşe arasında 1.5 gibi anlamsız değerler oluşur ve **yanlış texture**
okunur. `flat` kategorik değerler için şarttır.

### 7. ⚠️ Neden `switch`, neden `u_Textures[int(v_TexIndex)]` değil?
GLSL **3.30 spesifikasyonu** sampler dizilerinin yalnızca **sabit** ifadeyle
indekslenmesine izin verir. Değişkenle indeksleme GL 4.0+ ve
"dynamically uniform" koşuluyla geçerli.

Bazı sürücüler 3.3'te de kabul eder — **tam da bu yüzden tehlikeli**:
senin makinede çalışır, başkasında derlenmez. 32 case'lik `switch` çirkin
ama her yerde geçerli. Taşınabilirliğin bedeli.

### 8. Ham dizi + işaretçi, `push_back` değil
`push_back` her çağrıda boyut kontrolü yapar. Kare başına 40.000 kez
çağrılan bir yolda bu ölçülebilir. İşaretçi ilerletmek en ucuz yol.

> Bu, "erken optimizasyon yapma" kuralının makul bir istisnası:
> bu yol **tanımı gereği sıcak**.

### 9. Doğrusal arama (32 slot için) hash'ten hızlı
Önbellek dostu, dallanma az. "Erken optimizasyon yapma" kuralının doğru
uygulaması: basit olan zaten hızlı.

### 10. Döndürülmüş / döndürülmemiş ayrı fonksiyonlar
Çoğu sprite döndürülmez. Ayrı yol, `sin`/`cos` ve fazladan matris
çarpımını tamamen atlar. Binlerce quad'da ölçülebilir fark.

## Soyutlamanın ölçüsü

Faz 3'te `EditorApp.cpp` içinde ~60 satır GPU kurulumu vardı (VAO, VBO,
layout, EBO, shader). Faz 4'te tek satır:

```cpp
FX::Renderer2D::Init();
```

Editor artık VAO/VBO/shader **bilmiyor**. İyi bir soyutlamanın belirtisi budur.

## Test sahnesi
- `1` / `2` → ızgarayı küçült/büyüt (10×10 … 200×200)
- 50×50 = 2500 quad → **1 draw call**
- 150×150 = 22.500 quad → 10.000 sınırı aşılır → **3 draw call** (otomatik bölme)
- `X` → düz renk ↔ texture. İki texture kullanılsa bile tek batch
  (32 slot var, 2'si kullanılıyor)
- `V` ile VSync kapat → gerçek FPS görünür
- `TAB` → draw call sayısı

## Test sonucu
```
Shader 'Renderer2D' hazir (program id=3)
Renderer2D hazir (max 10000 quad/batch, 32 texture slotu)
Texture slotu (bu GPU): 32
```
RTX 3060 Ti 32 slot veriyor (GL 3.3 en az 16 garanti eder).
GL debug callback aktif, **hiç hata basmadı**.

## Onay
- [x] Derlendi
- [x] Çalıştı, GL hatası yok
- [ ] Kullanıcı onayı → Faz 5'e geçiş
