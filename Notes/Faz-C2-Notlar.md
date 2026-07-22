# Faz C-2 — Prefab Revert (kaynak → örnek)

> Prefab sisteminin (03-Yapilacaklar.md **C**) ikinci parçası. C-1 bağı
> kurmuştu; C-2 o bağı **okuma yönünde** kullanıyor: örneği kaynağına
> döndürmek. Apply (yazma yönü) ve override takibi hâlâ C-3/C-4.

Son güncelleme: 2026-07-22 · Önce oku: [Faz-C1-Notlar.md](Faz-C1-Notlar.md)

---

## Ne yapıldı

`PrefabSerializer::RevertInstance(Entity instanceRoot)` — örneğin
component'lerini kaynak `.fxprefab` dosyasından yeniden yazıyor.

### Çekirdek algoritma

1. Kök örneğin `PrefabInstanceComponent.Prefab` GUID'inden kaynak yolu
   çözülüyor (`AssetManager::GetPath`). Kayıpsa `false` döner (editör
   "Revert" düğmesini zaten göstermez).
2. Kaynak dosya okunuyor → `SourceId → json entity` haritası.
3. Örnek alt ağacı geziliyor; her damgalı entity `SourceId`'siyle kaynakta
   eşleştiriliyor. Eşleşende:
   - **Örnekte eklenmiş** (kaynakta olmayan) tablo-serileşen component'ler
     kaldırılıyor (`PrefabInstance` hariç — bağ revert'ten sağ çıkmalı).
   - `Detail::ApplyComponents` kaynak component'lerini üzerine yazıyor:
     örnekte silineni geri ekler, var olanı kaynak değeriyle değiştirir.
4. **İç referanslar** (`Follow.Target`, script `EntityRef` alanları)
   dosyadan kaynak kimliğiyle geldi; `Detail::RemapReferences` ile
   `SourceId → örnek UUID` haritasından geçirilip örnek kimliğine
   çevriliyor. Dışarıya bakan referanslara dokunulmuyor.

### Korunan üç şey

- **Her entity'nin UUID'i** — referanslar kopmasın.
- **Hiyerarşi + prefab bağı** — `ApplyComponents` bunlara dokunmuyor
  (structural alanlar entity düzeyinde, bağ dosyada yok çünkü `Save`
  siliyor).
- **Kök entity'nin `Translation`'ı** — `Instantiate`'in override ettiği
  **tek** şey buydu; Revert de onu koruyor ki örnek yerinde kalsın.
  Değişmez: *örneklenip hemen revert edilen bir örnek değişmez.* Kökün
  rotation/scale'i ve tüm çocuklar tamamen kaynağa döner.

### Undo — destroy + snapshot restore

Revert component **kümesini** değiştirebildiği için (eklenen kalkar,
silinen geri gelir) alan-değeri undo'su yetmiyor. Silme/çoğaltma ile aynı
`EntitySnapshot` kalıbı: `before` ve `after` tam snapshot alınıyor,
Undo/Redo alt ağacı **destroy + restore** ediyor. UUID'ler korunduğu için
arada ona bakan komutlar hedefini bulmaya devam ediyor.

### Editör — "Revert" düğmesi + kök çıkışı

Inspector prefab bloğunda **Revert** (yalnızca kaynak varken) + **Bağı
Kır**. Yeni yardımcı `InstanceRoot(e)`: aynı prefab handle'ına ait en
dıştaki atayı buluyor — bir **çocuk** seçiliyken de işlem tüm örneğe
uygulanıyor (Unity davranışı). Hem Revert hem Bağı Kır artık kökten
çalışıyor.

---

## Testler

`Tests/Prefab.test.cpp` — 2 yeni test (`[prefab]`, toplam 5):

- **Revert örneği kaynağına döndürüyor**: çocuk konumu/kök rotasyonu
  kaynağa döndü, kök konumu korundu, örnekte eklenen `CameraComponent`
  kalktı, bağ sağ kaldı.
- **Revert iç referansı örnek kimliğine yeniden eşliyor**: kaynakta çocuk
  kökü takip ediyor; revert sonrası hedef yine **örneğin** kökü (kaynağın
  değil).

Tüm paket: **82 test / 398 assertion** geçiyor (öncesi 80/380).

---

## Bilerek yapılmayanlar

- **Apply (örnek → kaynak)** — C-4. Bugün yalnızca kaynak → örnek.
- **Override takibi + Inspector'da kalın** — C-3. Revert şu an "hepsini
  geri al"; alan başına seçici revert yok.
- **Yapısal override** — örnekte **eklenen/silinen çocuk entity'ler**
  revert edilmiyor (yalnızca eşleşen entity'lerin component'leri). Örnek
  bir çocuk sildiyse geri gelmez, eklediyse silinmez. C-5.
- **Nested prefab** — `InstanceRoot` farklı handle sınırında duruyor ama
  iç içe kaynak zincirinin revert semantiği ele alınmadı.
