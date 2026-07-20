# Faz 11 — Viewport'ta Seçim + Gizmo

## Dosyalar
| Dosya | Değişiklik |
|---|---|
| `Renderer/Framebuffer.*` | Çoklu ek (MRT), `ReadPixel`, `ClearAttachment` |
| `Renderer/VertexArray.cpp` | **Tamsayı attribute düzeltmesi** |
| `Renderer/Renderer2D.*` | Quad başına `entityID` |
| `assets/shaders/Renderer2D.*` | İkinci çıktı: `o_EntityID` |
| `Editor/ImGuiLayer.*` | ImGuizmo, varsayılan panel düzeni, ini yolu |
| `Editor/EditorApp.cpp` | `PickEntity()`, `DrawGizmo()` |

## Color picking nasıl çalışıyor?

GPU'yu bir **sorgu aracı** olarak kullanıyoruz:

1. Framebuffer'a ikinci bir renk eki ekle: `R32I` (32-bit tamsayı)
2. Fragment shader her pikselde o quad'ın entity ID'sini bu eke yazsın
3. Fare tıklayınca `glReadPixels` ile o pikseldeki tamsayıyı oku
4. Değer doğrudan entity kimliği

Işın-kesişim (raycast) matematiği yok. Zaten çizilen şeyi okuyoruz, dolayısıyla
**tam olarak görünen şeyi** seçiyoruz — üst üste binen sprite'larda hangisi
öndeyse o seçilir, ekstra çaba gerekmeden.

### `ClearAttachment(1, -1)` neden şart?
`glClear` ID tamponunu **0** yapardı ve `0` geçerli bir `entt::entity`.
Boş alana tıklayınca ilk entity seçilirdi.

### Y ekseni çevrimi
OpenGL aşağıdan yukarı, ImGui yukarıdan aşağı sayar:
```cpp
my = height - my;
```
Faz 3 (stb_image) ve Faz 6 (viewport UV) ile aynı problem, üçüncü kez.

## ⚠️ Bulunan gerçek hata: tamsayı attribute'ları

`BufferLayout`'ta `Int` tipleri `glVertexAttribPointer` ile gidiyordu.
Bu fonksiyon değeri **float'a çevirir**; shader'da `in int` okuyunca çöp gelir.

Doğrusu **`glVertexAttribIPointer`** (I = integer). Faz 2'den beri kodda
duran bir hataydı — o zamana kadar hiç tamsayı attribute kullanmamıştık,
o yüzden ortaya çıkmamıştı.

> Belirtisi sinsi: sayılar makul görünür ama yanlıştır.

## ⚠️ GL debug callback'in ilk ciddi yakalayışı

Faz 1'de kurduğumuz `KHR_debug` çıktısı, her karede şunu bastı:

```
[GL/API/id=131140] Blending is enabled, but is not supported for
integer framebuffers.
```

**Kök sebep:** `glEnable(GL_BLEND)` *tüm* çizim hedeflerini açar; tamsayı
ekinde blending desteklenmez.

**Denenen çözüm:** `glDisablei(GL_BLEND, 1)` — GL 3.0'dan beri çekirdekte,
tek bir çizim hedefi için blending'i kapatır. Ama uyarı geçmedi.

**Neden geçmedi:** ImGui'nin OpenGL backend'i her karede blend durumunu
kaydedip geri yüklüyor ve `glEnable(GL_BLEND)` ile tüm indeksleri yeniden
açıyor. Tek seferlik ayar tutmuyor → `Framebuffer::Bind()` içine taşındı,
her karede uygulanıyor.

**Uyarı yine de sürdü:** NVIDIA bu kontrolü **kaba** yapıyor — global
`GL_BLEND` bayrağına bakıyor, per-buffer disable'ı dikkate almıyor.
Yanlış pozitif olduğu için `131140` filtre listesine eklendi.

Doğru olduğunu **ölçerek** doğruladık: seçim çalışıyor, yani ID tamponu
bozulmuyor.

> Debug callback'i mesaj id'sini de basacak şekilde güncelledik —
> bir mesajı susturmak gerektiğinde numarayı log'dan okuyabilmek için.

## ImGuizmo entegrasyonu

### CMake tuzağı
`FetchContent_MakeAvailable`, depoda `CMakeLists.txt` varsa
`add_subdirectory` çağırır. ImGuizmo'nun kendi CMake'i bizim `imgui`
target'ımızı bilmediği için derlenmedi.

**Çözüm:** `SOURCE_SUBDIR src` — FetchContent, belirtilen alt dizinde
`CMakeLists.txt` **yoksa** `add_subdirectory` çağırmaz. "Sadece indir"
demenin belgelenmiş yolu.

> Depo yakın zamanda yeniden düzenlenmiş: kaynaklar artık `src/` altında.

### Kısayollar W/E/R değil Z/X/C/B
Standart gizmo kısayolları W/E/R'dir ama bizde bunlar **kamera** tuşları.
Çakışmayı fark edip Z (kapalı) / X (taşı) / C (döndür) / B (ölçekle) seçtik.

`Ctrl` basılıyken kademeli hareket (snap): 0.5 birim, dönmede 15°.

### Matris ayrıştırma
ImGuizmo 4×4 matris döndürür, bizim `TransformComponent` ayrı alanlar tutar:
```cpp
ImGuizmo::DecomposeMatrixToComponents(...);
tc.Rotation = glm::radians(rotation[2]);   // sadece Z, 2D
tc.Scale    = { scale[0], scale[1] };
```

## Bonus: varsayılan panel düzeni

İlk çalıştırmada `imgui.ini` yoktu ve tüm paneller içeriğe göre boyutlanıp
**32×32 piksel** olarak açılıyordu. Kullanılamaz bir ilk izlenim.

- `DockBuilder` API'siyle varsayılan düzen kuruluyor (Hierarchy sol,
  İstatistikler sol-alt, Inspector sağ, Viewport orta)
- Görünüm menüsünde "Panel Düzenini Sıfırla"
- **`imgui.ini` artık exe'nin yanına yazılıyor**, çalışma dizinine değil —
  Faz 2'de shader yollarında yaşadığımız sorunun aynısı

## Test sonuçları

**Otomatik tıklama testi:** Viewport merkezine (ekran 1266,705) tıklandı
```
Secildi: Oyuncu
GL uyarisi: 0
```

**Panel düzeni:** `Viewport Pos=289,19 Size=994,881 DockId=0x3` — docked.

## Onay
- [x] Derlendi
- [x] Seçim çalışıyor (otomatik tıklama ile doğrulandı)
- [x] GL uyarıları sıfır
- [x] Varsayılan panel düzeni kuruluyor
- [ ] Kullanıcı onayı (gizmo elle test edilmeli) → Faz 9

## Sonraki: Faz 9 (parent/child), sonra Faz 12 (content browser)
