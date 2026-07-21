# Faz A5 — Undo / Redo

> Uygulandi: 2026-07-21. Yol haritasi 20a + 20b'nin ilk turu.

## Amaç

Editörde yapılan düzenlemeleri geri alınabilir kılmak. Yüksek öncelikli
teknik borç buydu: **"Gizmo ile yanlış sürükleme geri alınamıyor."**

## Tasarım

`Editor/src/Commands/CommandStack.h` — tek dosya, sade.

- **`EditCommand`**: `Name` + iki closure (`Undo`, `Redo`). Komutlar
  gereken durumu **kendileri yakalar**; yığın yalnızca sırayı tutar.
- **`CommandStack`**: undo + redo yığınları. **Push edilen komut ZATEN
  UYGULANMIŞ sayılır** — değer arayüzde/gizmoda çoktan değişti, o yüzden
  `Push` yeniden çalıştırmaz, yalnızca kaydeder. Yeni komut redo'yu
  temizler. 200 adım sınırı.

Neden closure tabanlı, sınıf hiyerarşisi değil: komut çeşidi az ve her
biri farklı veri yakalıyor (alan değeri / transform). Closure en az kod.

## Kapsanan işlemler

### 1. Inspector alan düzenlemeleri (`ComponentDrawer`)

A1'in alan tablosu sayesinde **generic**: her `FieldInfo` için değeri
tipten bağımsız bir `FieldValue`'ya okuyup yazabiliyoruz. Komut, entity +
component adı + alan adı + eski/yeni değeri tutuyor (ham işaretçi değil
**ad**, çünkü closure entity silinmiş olabilir diye kimliğe dayanmalı).

**Tek sürükleme = tek undo adımı.** ImGui'nin `IsItemActivated()` /
`IsItemDeactivatedAfterEdit()` deseni: sürükleme başlarken eski değer(ler)
saklanıyor, bırakılınca eski→yeni komut yazılıyor. `DragFloat`'ı sürüklemek
onlarca ara değer üretir ama tek undo adımı olur.

**Çoklu seçimle uyumlu:** değişiklik zaten tüm seçili entity'lere
uygulanıyordu (önceki iş); undo da hepsinin eski değerini bir arada
saklayıp topluca geri alıyor.

`EntityRef` (combo) istisna: seçim anında kesinleşir, o yüzden `changed`
olunca hemen komut yazılıyor (aktivasyon/deaktivasyon beklenmiyor).

### 2. Gizmo dönüşümü (`EditorApp_Viewport`)

`ImGuizmo::IsUsing()` geçişleri temiz: sürükleme **başlarken** tüm seçili
entity'lerin `TransformComponent`'i (konum/dönüş/ölçek) yakalanıyor,
**bırakılınca** eski→yeni tek komut yazılıyor. Gerçekten değişen yoksa
komut yazılmıyor.

Undo sonrası dünya matrisleri kendiliğinden tazeleniyor: `Scene::OnRender`
her karede `TransformSystem::Update` çağırıyor (Edit modu durağan ama
render yolu çalışıyor).

## Kısayollar

- **Ctrl+Z** geri al · **Ctrl+Shift+Z** / **Ctrl+Y** yeniden yap.
- Kısayol **global** (viewport'a bağlı değil): Inspector'da düzenledikten
  sonra da çalışsın. (Not: plain Z hâlâ viewport'ta gizmo'yu kapatıyor.)
- Menü: **Düzen → Geri Al / Yeniden Yap** (keşfedilebilirlik).

## Yığın ne zaman temizleniyor

Komutlar entity tutamakları tutuyor; sahne değişince geçersizleşir:
**NewScene, OpenScene, Sahneyi Sıfırla, Play, Stop** → `m_Commands.Clear()`.

## Bu turda YAPILMAYAN (bilerek)

Yapısal işlemler ayrı bir tur:
- **Entity silme/oluşturma geri alma** — silinen entity alt ağacını
  (UUID'leriyle) geri getirmek serileştirme gerektiriyor.
- **Component ekleme/silme geri alma** — ekleme→silme kolay, silme→geri
  ekleme component'in tüm alanlarını saklamayı gerektiriyor.

Şu an bunlar undo'ya girmiyor; alan düzenleme ve gizmo (en sık, sürekli
düzenleme işlemleri, ve belirtilen ağrı) kapsandı.

## Doğrulama

Temiz derleme, `FXTests` 51/238 yeşil, editör açılıyor. Görsel onay
(sürükle → Ctrl+Z → geri gelmeli; inspector'da değiştir → Ctrl+Z)
kullanıcıda.
