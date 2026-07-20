# Faz 18 (kısmi) — Çizgi render, ızgara, seçim çerçevesi

Faz 18'in tamamı değil: **çizgi altyapısı + editörün görsel eksikleri**
öne alındı. Viewport'un "oturmamış" hissinin sebebi buydu — ölçek referansı
yoktu, seçili nesnenin sınırı görünmüyordu, sahnede kaybolunca dönüş yolu
yoktu.

Kalan Faz 18 maddeleri (daire primitifi, sıralama katmanı, shader hot
reload) yerinde duruyor.

## Dosyalar

| Dosya | Değişiklik |
|---|---|
| `Renderer/RenderCommand.*` | `DrawLines`, `SetLineWidth`, `SetDepthTest` |
| `Renderer/Renderer2D.*` | Ayrı çizgi batch'i, `DrawLine`, `DrawRect` ×2 |
| **`assets/shaders/Line.vert/.frag`** (yeni) | Çizgi shader'ı |
| `EditorApp.*` | `DrawGrid`, `DrawSelectionOutline`, `FocusOnSelection` |

---

## 1. Çizgiler neden ayrı bir batch?

Tek `glDrawElements` çağrısı **tek bir primitif tipi** çizer. `GL_TRIANGLES`
ile başlayıp aynı çağrıda `GL_LINES`'a geçemezsin. Yani çizgiler quad
batch'ine karışamaz — ikinci bir VAO/VBO/shader üçlüsü şart.

**Index buffer yok.** Quad'da indeksleme kazanç sağlıyor çünkü aynı köşe iki
üçgen arasında paylaşılıyor (4 köşe → 6 indeks). Bir çizgi parçasının iki
köşesi ise başka hiçbir yerde tekrarlanmaz; `glDrawArrays` yeterli.

Köşe yapısı da ayrı: `LineVertex` sadece pozisyon + renk + entityID tutuyor.
`QuadVertex`'i kullansaydık köşe başına UV ve tiling için 20 baytı boşuna
taşırdık.

### Sıra: çizgiler quad'lardan sonra

`EndScene` önce `Flush()` sonra `FlushLines()` çağırıyor. Debug çizimi
sahnenin **üstünde** olmalı — sprite'ın altında kalan bir seçim çerçevesi
işe yaramaz.

---

## 2. `o_EntityID` yazılmak zorunda

Line fragment shader'ı ikinci renk ekine `-1` yazıyor:

```glsl
layout(location = 1) out int o_EntityID;
...
o_EntityID = v_EntityID;   // cizgiler icin hep -1
```

Yazmasaydık o piksellerde **önceki karenin ID'si** kalırdı ve ızgara
çizgisinin üstüne tıklamak alakasız bir entity seçerdi. Faz 11'de
`ClearAttachment(1, -1)` ile çözdüğümüz problemin aynısı, farklı kaynaktan.

---

## 3. Derinlik testi: z değil, çizim sırası

Motor `GL_DEPTH_TEST`'i açık tutuyor. Izgarayı sahneden önce çizsek bile
z=0'da olduğu için sprite'larla çakışırdı (`GL_LESS` eşitlikte geçmez).

Çözüm z değeriyle oynamak **değil** — `RenderCommand::SetDepthTest(false)`:

```cpp
SetDepthTest(false);  DrawGrid();            // arkada
SetDepthTest(true);   m_Scene->OnRender();   // normal
SetDepthTest(false);  DrawSelectionOutline();// ustte
```

Test kapalıyken sıralamayı çizim sırası belirler, ki debug çizimi için
istenen tam olarak budur. z ile katmanlamaya çalışmak, sprite'ların kendi
z değerleriyle çakışan kırılgan bir sistem üretirdi.

---

## 4. Izgara aralığı: 1-2-5-10 serisi

Görünen yüksekliği ~16 parçaya bölen "yuvarlak" bir aralık seçiliyor:

```cpp
raw       = visibleHeight / 16;
magnitude = pow(10, floor(log10(raw)));
n         = raw / magnitude;
nice      = n<1.5 ? 1 : n<3.5 ? 2 : n<7.5 ? 5 : 10;
step      = nice * magnitude;
```

Doğrudan `visibleHeight/16` kullansaydık aralık 0.37 gibi bir sayı olurdu.
İnsan gözü 1'in katlarını bekler; zoom yaparken aralık **sürekli değil
kademeli** değişmeli — yoksa ızgara sürekli kayan, okunamaz bir dokuya
dönüşür.

Her 5 adımda bir kalın çizgi var. Referans noktası olmadan ızgarada sayı
saymak imkânsız.

`floor(v/step + 0.5)` kullanılıyor, `(int)` değil: negatif tarafta C'nin
sıfıra doğru kesmesi orijin çevresinde deseni bozardı.

---

## 5. Seçim çerçevesi dünya matrisiyle çiziliyor

`DrawRect(mat4, color)` birim quad'ın dört köşesini transform'dan geçirip
birleştiriyor. Pozisyon/boyuta ayrıştırıp eksen hizalı kutu çizseydik
**döndürülmüş nesnelerde çerçeve üstüne oturmazdı.**

Kullanılan matris `WorldTransformComponent` (Faz 9), yerel transform değil —
aksi halde parent'ı olan entity'lerin çerçevesi yanlış yerde çıkardı.

**Doğrulama:** 726° döndürülmüş "Uydu 3" entity'sinde çerçeve nesneyle
birlikte dönmüş halde oturuyor (ekran görüntüsüyle bakıldı).

---

## 6. F ile odaklan

Kamerayı seçili entity'ye getirip ekrana sığdırıyor.

```cpp
m_CameraPosition = { world[3][0], world[3][1], 0 };   // 4. sutun = konum
extent = max(length(world[0]), length(world[1]));     // dunya olcegi
m_ZoomLevel = clamp(extent * 2.5f, 1, 40);
```

Matrisin 4. sütunu zaten konumdur; `DecomposeMatrixToComponents` çağırmaya
gerek yok. Ölçek de ilk iki sütunun uzunluğundan okunuyor.

`* 2.5` pay bırakmak için: nesnenin tam sınırına zoom yapmak onu ekranın
kenarına yapıştırırdı.

**Doğrulama:** Uydu 3 (konum -2.828, 2.828 / ölçek 0.6) → kamera
(-2.83, 2.83), zoom 1.5. Beklenen değerler.

---

## Kısayollar

| Tuş | İş |
|---|---|
| `F` | Seçiliye odaklan |
| `G` | Izgarayı aç/kapa |

İkisi de viewport üzerindeyken çalışıyor, ayrıca Görünüm menüsünde ve
viewport araç çubuğunda.

## Test sonuçları

Otomatik (ekran görüntüsü + simüle edilmiş girdi):
```
izgara acik   : cizgi 52-58, draw call 3
izgara kapali : cizgi 4 (sadece cerceve), draw call 2
F odaklan     : "Odaklanildi: Uydu 3", kamera (-2.83, 2.83), zoom 1.5
secim cercevesi: 726 derece donmus nesnede uzerine oturuyor
```

## Onay
- [x] Derlendi (uyarısız)
- [x] Izgara çiziliyor, zoom'la kademeli değişiyor
- [x] Seçim çerçevesi döndürülmüş nesnede doğru
- [x] F odaklan, G aç/kapa
- [x] GL hatası yok
- [ ] Kullanıcı onayı

## Kalan Faz 18 maddeleri
- Daire primitifi (`DrawCircle`) — collider debug çizimi için gerekecek
- Sıralama katmanı (`SortingLayer` + `OrderInLayer`)
- Saydam nesneler için arkadan öne sıralama
- Shader hot reload
- Collider debug çizimi → Faz 17'ye bağlı

> `glLineWidth` 1.0'dan büyük değerleri **garanti etmez** — çekirdek
> profilde çoğu sürücü yok sayar. Seçim çerçevesinde 2.0 isteniyor ama
> alınmayabilir. Gerçekten kalın çizgi gerekirse yol, çizgiyi quad olarak
> çizmektir. Şimdilik gerek yok.
