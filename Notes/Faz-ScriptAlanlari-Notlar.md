# Script alanlarının Inspector'dan ayarlanması

> Uygulandı: 2026-07-21. A1'den beri bekleyen, Aşama B'nin bilerek
> B sonrasına bıraktığı iş.

## Problem

Bir script'in (`Gezgin`) `m_Speed` alanını editörden ayarlamak istiyoruz.
Ama:

1. Script `Game.dll`'de yaşıyor — editör onun alanlarını derleme zamanında
   bilemez.
2. **DLL yeniden yüklendiğinde instance'lar yok olup yeniden yaratılıyor**
   → instance'a yazılan değer kaybolur.

Plan bu yüzden şunu şart koşmuştu: *"alan değerleri component'te veri
olarak durmak zorunda."*

## Çözüm: ziyaretçi (visitor) tabanlı reflection

`ScriptFields.h` (motor):

- **`ScriptFieldVisitor`** — soyut arayüz, tip başına `Visit(name, T&)`.
  Varsayılan gövdeler boş: bir ziyaretçi yalnız ilgilendiği tipi ezer.
- **`ScriptableEntity::OnReflect(ScriptFieldVisitor&)`** — script kendi
  alanlarını bildirir:
  ```cpp
  void OnReflect(FX::ScriptFieldVisitor& v) override {
      v.Visit("Hiz", m_Speed);
  }
  ```
- **`ScriptFieldValue` + `ScriptFieldMap`** — override deposu (ad → değer).
- **`ScriptFieldApplier`** — haritayı bir instance'a uygular.

**Tek gezi, üç kullanım:** uygula (harita→instance), çiz (Inspector),
say/doğrula. A1'in alan tablosuyla aynı felsefe — reflection kütüphanesi
değil, açık ve okunabilir.

## Değer nerede duruyor

`NativeScriptComponent::Fields` (`ScriptFieldMap`). Instance'ta **değil**:
DLL reload'dan sağ çıkması gereken şey bu.

- **Play'de:** `ScriptSystem::OnRuntimeStart` instance'ı yaratıp
  `m_Entity`'yi verdikten sonra, **`OnCreate`'ten ÖNCE** override'ları
  uyguluyor — script kendi `OnCreate`'inde bu değerlere güvenebilsin.
- **Serileştirme:** `ComponentMeta`'daki `Extra(save, load)` kancasıyla
  JSON'a (`"Fields"`) yazılıyor. Tip etiketi taşıyor (`f/i/b/v2/v3/v4/s`)
  ki yükleme tipi doğrulasın.

## Edit modunda alanlar nasıl görünüyor

Edit modunda instance **yok**. Alan listesini ve tiplerini öğrenmek için
Inspector **geçici bir probe instance** üretiyor:

1. `ScriptRegistry::Create(name)` → probe
2. `ScriptFieldApplier` ile mevcut override'ları uygula (yoksa script'in
   kendi varsayılanı kalır)
3. `ScriptFieldInspector` ile çiz — widget probe'un canlı üyesine bağlı;
   değişince değer **haritaya** yazılıyor
4. `delete probe`

Her karede probe üretilip yok ediliyor. Ucuz (birkaç alan) ve değer
haritada kalıcı olduğu için sürükleme sorunsuz çalışıyor.

**Override semantiği:** haritada olmayan alan script'in varsayılanını
kullanır. Kullanıcı bir alana dokunduğunda haritaya girer. Script'ten bir
alan kaldırılırsa haritadaki artık girdi sessizce yok sayılır (isim +
tip eşleşmesi aranıyor).

## DLL sınırı

`OnReflect` sanal; script'in vtable'ı `Game.dll`'i, ziyaretçininki
editörü işaret ediyor. Çapraz çağrı **ölçüldü** (geçici öz-test):

```
REFLECT-TEST Gezgin  -> 3 alan     (OnReflect var)
REFLECT-TEST Spin    -> 0 alan     (yok, varsayılan boş gövde)
REFLECT-TEST Takipci -> 0 alan
```

Probe `Game.dll`'de `new`, editörde `delete` ediliyor — her iki hedef de
aynı dinamik CRT'yi (`/MDd`) kullandığı için güvenli (mevcut
`OnRuntimeStop` zaten bu desene dayanıyor).

## ⚠ Uyumluluk uyarısı

`ScriptableEntity`'ye **yeni bir sanal eklendi** → vtable düzeni değişti.
Eski motorla derlenmiş bir `Game.dll` yeni editörle **uyumsuzdur**.
Bir projeyi güncelledikten sonra **bir kez "Derle"ye basmak gerekiyor.**
(Sürüm damgası ile otomatik tespit yapılmadı — ileride eklenebilir.)

## Bilerek yapılmayanlar

- **Undo/Redo kapsamıyor.** Script alanları A1'in `ComponentInfo` alan
  tablosunda olmadığı için A5'in generic alan komutu bunlara uygulanmıyor.
- **Play sırasında düzenleme canlı instance'a yansımıyor** — harita
  güncelleniyor, bir sonraki Play'de geçerli oluyor.
- Desteklenen tipler: `float, int, bool, vec2, vec3, vec4, string`.
  (Entity referansı / varlık slotu yok.)

## Doğrulama

Temiz derleme; `FXTests` 51/238; `Game.dll` yeni header'la derlendi;
cross-DLL reflection ölçüldü (yukarıdaki sayılar); editör açılıyor.
Inspector'da alan çizimi/düzenleme ve sahneye kaydetme görsel onayı
kullanıcıda.
