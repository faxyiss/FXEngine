# 0.2 — SelectionContext

Seçim `SceneHierarchyPanel`'in içinde bir `FX::Entity` üyesiydi. Viewport,
gizmo, inspector ve prefab kaydetme hepsi o panelden okuyordu: panel,
kendisiyle ilgisi olmayan bir durumun sahibiydi ve ondan bağımsız çalışması
gereken her şey ona bağımlı hale gelmişti.

## Ne yapıldı

`FXEd::SelectionContext` (`Editor/src/SelectionContext.h`) — seçimin tek
sahibi. `EditorApp` tutuyor, panellere işaretçisi veriliyor.

`SceneHierarchyPanel::GetSelectedEntity/SetSelectedEntity` kaldırıldı;
yerine `SetSelection(SelectionContext*)`. Panel artık tüketici.

## Kararlar

**API baştan çoklu seçime göre yazıldı** (`Add`, `Remove`, `Toggle`,
`IsSelected`, `GetAll`) ama davranış 0.2'de tek seçim olarak kaldı.
Sebep: 0.3'te panelin ve viewport'un tıklama mantığı değişecek, bu sınıf
değişmeyecek.

**`vector`, `set` değil.** Kullanıcının seçtiği sıra anlamlı — ilk seçilen
"birincil"dir; gizmo ve Inspector onu kullanıyor (Unity de böyle yapıyor).
Seçim listeleri her zaman küçük, arama maliyeti önemsiz.

**`GetPrimary()` geçersiz entity dönebiliyor.** Çağıranların ayrıca
"seçim var mı" sorusu sorması gerekmiyor; `if (entity)` zaten yeterli.
Eski `GetSelectedEntity()` de aynı sözleşmeye sahipti, çağıran tarafta
hiçbir şey değişmedi.

**Silmeden önce ayıklama.** Bir entity silinirken seçim listesinden hem
kendisi hem altındakiler çıkarılıyor — silindikten *sonra* `IsAncestorOf`
sormak geçersiz entity'ye soru sormak olurdu.

## Test (gerçek `Game1` projesi)

| Yol | Sonuç |
|---|---|
| Hierarchy'den entity seçme | Inspector doldu, viewport'ta seçim çerçevesi çizildi |
| Viewport'ta boşluğa tıklama | Seçim temizlendi, Inspector "bir entity seç"e döndü |
| Viewport'ta entity'ye tıklama (color picking) | Seçildi, Inspector doldu |
| Sahne açma | Seçim temizlendi (eski sahnenin kimliği yeni sahneye sızmıyor) |

## Bilerek yapılmayanlar

- **Çoklu seçim davranışı yok** — 0.3'ün işi. `Add`/`Toggle` şu an hiçbir
  yerden çağrılmıyor; ölü kod gibi görünüyorlar ama bir sonraki adımın
  tamamı bu üç fonksiyon üzerine kurulacak.
- **`Prune()` çağrılmıyor.** Sahne dışından entity silinmesi mümkün değil;
  ihtiyaç doğunca (script fazı) tek satır.
