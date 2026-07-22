# Faz C-4 — Prefab Apply (örnek → kaynak)

> Prefab sisteminin dördüncü parçası ve 16c'de asıl canı yakan eksiğin
> kapanışı: "`Star`'a script atadım, beş yıldızı tek tek düzeltmek
> gerekti." Artık bir örnekte yap → **Apply** → kaynak + diğer örnekler
> güncellenir.

Son güncelleme: 2026-07-23 · Önce oku: [Faz-C3-Notlar.md](Faz-C3-Notlar.md)

---

## Ne yapıldı

`PrefabOverrides::ApplyInstance(instanceRoot, library)` — iki iş:

### 1. Kaynağa yazma

Yeni entity listesi **eski dosyanın sırasıyla** kurulur:

- Eşleşen kayıt (SourceId ↔ örnek) örnekten yeniden serileştirilir;
  `ID` kaynak kimliğinde **kalır** — diğer örneklerin bağı kopmaz.
- `PrefabInstance` dosyaya girmez; kökün `Parent`'ı silinir.
- **`RemapJsonRefs`**: Parent + `EntityRef` alanları + script `"e"`
  alanları örnek kimliğinden **kaynak uzayına** çevrilir
  (`RemapReferences`'ın json düzeyindeki karşılığı — Apply canlı entity
  değil dosyaya yazılacak metin üzerinde çalışır).
- **Kök konumu kaynaktaki değerinde kalır**: örneğe özgü yerleştirme
  kaynağa sızmaz. Revert'in simetriği — Revert kök konumunu korur,
  Apply kök konumunu yazmaz.
- Örnekte **silinmiş** kaynak entity'leri dosyada korunur, örnekte
  **eklenen** entity'ler dosyaya girmez (yapısal apply C-5).

### 2. Diğer örnekleri tazeleme (dosya yazılmadan, bellekte)

Sahnedeki aynı prefab'ın diğer örnekleri `InstanceRootsOf` ile bulunur;
her eşleşen entity için `RefreshInstanceEntity`:

- Alan düzeyinde üçlü kıyas: **eski kaynak / yeni kaynak / örnek**.
  Kaynakta değişmeyen alan atlanır; örnek eski kaynaktan sapmışsa
  (**override**) dokunulmaz; sapmamışsa yeni değere çekilir. Unity
  davranışı: Apply başkalarının bilinçli değişikliklerini ezmez.
- `EntityRef` alanları iki uzay çevrimiyle: kıyas kaynak uzayında,
  yazma o örneğin **kendi** alt ağacındaki karşılığıyla.
- **Extra blob** (doku GUID'i, script alanları) tek parça kıyaslanır:
  örnek eski kaynakla aynı kaldıysa yeni blob `LoadExtra` ile yüklenir.
  16c senaryosu tam bu: script ataması + alan değerleri böyle yayılır.

Kıyas **eski** kaynağa karşı yapılmak zorunda olduğu için tazeleme dosya
yazılmadan önce, bellekteki eski/yeni kopyalarla yapılır.

## Editör

- Inspector prefab bloğu: **Apply** (tooltip'li) + Revert + Bağı Kır.
  Hepsi örnek kökünden çalışır.
- `InstanceRoot` motora taşındı (`PrefabOverrides::InstanceRoot`) —
  panel, komutlar ve motor aynı tanımı kullanıyor; paneldeki yerel kopya
  silindi.
- **Undo** (`Structural::ApplyPrefabInstance`): Apply'ın dokunduğu iki
  şey geri alınır — dosyanın **eski baytları** geri yazılır + diğer örnek
  köklerinin snapshot'ları destroy+restore edilir. Uygulanan örneğin
  kendisi Apply'da değişmediği için snapshot'a girmez.

## Testler

2 yeni test (`[prefab]`, toplam 10 / 94 assertion):

1. **Apply + yayılma**: A'da rotation+scale değişti, B'de rotation
   bilinçli farklıydı → Apply sonrası B'nin scale'i yeni kaynağa çekildi,
   rotation override'ı korundu; dosyada yeni değerler, kök konumu 0'da
   kaldı (A'nın yerleştirmesi sızmadı), `PrefabInstance` yok.
2. **İç referans kaynak uzayında**: örnekte eklenen `Follow` hedefi örnek
   köküydü → dosyaya kaynak kök kimliğiyle yazıldı; yeni örnekleme hedefi
   kendi köküne çözüyor.

Tüm paket: **87 test / 446 assertion** (öncesi 85/421).

## Bilerek yapılmayanlar

- **Yapısal apply** — örnekte eklenen/silinen çocuk entity'ler dosyaya
  işlenmiyor. Diğer örneklerde component ekleme/silme de yayılmıyor
  (yalnızca iki tarafta da var olan component'lerin alanları). C-5.
- **Alan başına Apply** — sağ tık menüsünde yalnız "kaynak değerine
  döndür" var; "bu alanı kaynağa uygula" eklenmedi (toplu Apply'ın
  varlığında ikincil; istenirse küçük iş).
- **Kaydedilmemiş sahne + Apply etkileşimi** — Apply dosyayı hemen
  yazıyor; sahne kaydedilmemiş olsa bile. Unity ile aynı ama fark
  edilmesi gereken davranış.
