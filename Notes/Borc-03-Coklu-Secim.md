# 0.3 — Entity çoklu seçimi

0.2'de `SelectionContext` çoklu seçime göre yazılmıştı ama davranış tek
seçimdi. Bu adım davranışı getirdi.

## Ne yapıldı

- **Hierarchy:** Ctrl+tık ekler/çıkarır, Shift+tık aralık seçer, düz tık
  tek seçime döner.
- **Viewport:** Ctrl+tık ekler/çıkarır. Ctrl basılıyken boşluğa tıklamak
  seçimi silmiyor.
- **Gizmo:** birincil üzerinde duruyor, uyguladığı dünya-uzayı deltası
  diğer seçililere de uygulanıyor.
- **Seçim çerçevesi:** hepsi çiziliyor; birincil parlak, diğerleri soluk.
- **Silme:** sağ tık menüsünde "Secilenleri Sil", ayrıca viewport
  üzerindeyken `Delete` tuşu.
- **Inspector:** çoklu seçimde "N entity secili - duzenlenen: birincil".

## Kararlar

**Gizmo mutlak konum değil DELTA uyguluyor.** Diğer entity'leri birincilin
konumuna taşımak yerine hepsini aynı miktarda oynatıyoruz — ImGuizmo'nun
`deltaMatrix` çıktısı zaten bunu veriyor.

**Atası seçili olan entity atlanıyor.** Parent hareket edince çocuk zaten
onunla geliyor; deltayı bir kez daha uygulasaydık çocuk iki kat hareket
ederdi.

**Tıklama isteği biriktirilip çerçeve sonunda işleniyor.** Shift aralığı
"görünen sıra"ya ihtiyaç duyuyor ve o sıra ancak ağaç tamamen çizilince
belli oluyor. Ağaç gezilirken seçim değiştirmemenin ayrıca faydası var.

**Shift çıpası değişmiyor.** Aralık seçtikten sonra başka bir satıra
Shift+tık aralığı büyütüp küçültüyor — çıpa her Shift'te güncellenseydi
bu mümkün olmazdı. Düz tık ve Ctrl+tık çıpayı taşıyor.

**Çoklu seçimde Inspector yalnızca birincili düzenliyor.** Çok hedefli
alan düzenleme (Unity'nin "—" gösteren alanları) ayrı bir iş; yanıltıcı
olmaması için durum ekranda yazıyor.

## Yol boyunca çıkan hata

Hierarchy'de "boş alana tık → seçimi kaldır" kontrolü iki yönden yanlıştı:

- `IsWindowHovered()` bir satırın üzerindeyken de `true` dönüyor, yani
  kontrol satıra yapılan tıklamada da çalışıyordu.
- `IsMouseDown` kullanıyordu: fare basılı kaldığı **her karede** seçimi
  siliyordu. Tıklama isteğinin işlendiği ilk kareden sonraki kareler
  seçimi geri alıyordu — tek seçimde fark edilmemişti, Shift aralığında
  ortaya çıktı.

Düzeltme: `IsMouseClicked` + `!IsAnyItemHovered()`.

## Test (gerçek `Game1` projesi)

| Ne | Sonuç |
|---|---|
| Hierarchy'de Ctrl+tık | 2 entity seçili, Inspector "2 entity secili" |
| Hierarchy'de Shift+tık aralık | 4 satır birden seçildi |
| Gizmoyla 4 entity'yi sürükleme | Hepsi `(-3.887, 3.013)` oldu — ayrı ayrı doğrulandı |
| Çoklu seçim çerçevesi | Birincil parlak, diğerleri soluk |
| Sağ tık menüsü | "2 entity secili" başlığı + "Secilenleri Sil" |
| Çoklu seçimde "Prefab Olarak Kaydet" | Tek entity üzerinde çalıştı (elle doğrulandı) |

`Delete` kısayolu otomatik simüle **edilemedi**: `keybd_event` Delete'i
numpad-nokta olarak gönderiyor, `PostMessage` ile gerçek `SDLK_DELETE`
ulaşıyor ama o sırada viewport hover koşulu sağlanamadı. Kod yolu
menüdekiyle aynı mantık, elle doğrulanmalı.

## Bilerek yapılmayanlar

- **Kutu (marquee) seçimi** — viewport'ta sürükleyerek alan seçme.
- **Çok hedefli Inspector düzenleme.**
- **Çoklu prefab kaydetme** — prefab bir kök entity ağacı; birden fazla
  kökü tek dosyaya yazmak prefab formatını değiştirmek demek.
