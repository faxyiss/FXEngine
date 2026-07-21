# B-7 — Oyun projesi gerçek bir C++ projesi

> 2026-07-21. Aşama B'nin bıraktığı kısıt: oyun kodu yalnızca header
> yazılabiliyordu ve her header bir script olmak zorundaydı.

---

## 1. Sorun

Aşama B'de üretilen CMake şunu yapıyordu:

```cmake
file(GLOB_RECURSE FXGAME_SCRIPT_HEADERS ... "${FXGAME_ASSETS_DIR}/*.h")
foreach(script_header ${FXGAME_SCRIPT_HEADERS})
    get_filename_component(script_name ${script_header} NAME_WE)
    ...Register<FXGame::${script_name}>("${script_name}")
endforeach()
```

Yani `assets/` altındaki **her `.h`**, adıyla aynı isimde bir
`FX::ScriptableEntity` türevi içermek zorundaydı. İki sonucu vardı:

1. **`.cpp` yazılamıyordu.** Her şey header'da inline; tanım/bildirim
   ayrımı yok, ortak yardımcı kod yok, her değişiklikte her şey yeniden
   derleniyor.
2. **Script olmayan header konulamıyordu.** Oyun kodunu düzenlemenin tek
   yolu "her dosya bir script"ti.

**Ölçüldü, iddia edilmedi.** `assets/scripts/Utils.h` (içinde yalnızca
bir `Clamp` fonksiyonu) konup derlendi:

```
GameRegistrations.h(22,46): error C2039: 'Utils': bir 'FXGame' üyesi değil
GameRegistrations.h(22,29): error C2672: 'FX::ScriptRegistry::Register':
                            eşleşen aşırı yüklenmiş işlev bulunamadı
```

---

## 2. Karar

**Script tespiti dosya adına değil içeriğe baksın.** CMake header'ı
okuyup `: public FX::ScriptableEntity` arıyor ve sınıf adını oradan
çıkarıyor.

Değerlendirilen alternatif: header'a `FX_REGISTER_SCRIPT(Player)` gibi
kendi kendini kaydeden bir makro koymak. Daha sağlam (regex yok) ama
**K2 kararına aykırı** — "makro sihri yok, açık kalsın". İçerik taraması
kullanıcıdan hiçbir şey istemiyor: script yazarsan kaydedilir, yazmazsan
kaydedilmez.

Regex'in kırılganlığı bilinçli kabul edildi. Yanlış pozitif üretmesi zor
(`: public FX::ScriptableEntity` yazan bir şey zaten script'tir); yanlış
negatif verirse (çok sıra dışı biçimlendirme) sonuç sessiz değil
görünür: script Inspector listesinde çıkmaz.

---

## 3. Yapılanlar

| Değişiklik | Sonuç |
|---|---|
| `*.cpp` de toplanıp `Game` hedefine ekleniyor | Oyun kodu `.h`/`.cpp` ayrılabiliyor |
| Header içeriği taranıyor, sınıf adı çıkarılıyor | Script olmayan header'lar sessizce geçiliyor |
| Kayıtlı ad = **sınıf adı** (dosya adı değil) | Bir header birden fazla script içerebilir |
| `assets/` kökü include yoluna eklendi | `#include "scripts/Utils.h"` yazılabiliyor |

Değişen tek dosya: `Editor/src/Game/GameProject.cpp` (üretilen CMake
şablonu). Motor, editör ve sahne formatı hiç etkilenmedi.

**Sahne dosyaları bozulmadı** çünkü script referansı zaten *adla*
tutuluyor (Faz 16b) ve mevcut projelerde dosya adı = sınıf adı, yani
kayıtlı adlar aynı kaldı.

---

## 4. Doğrulama

`TestGame1` projesinde:

1. `Utils.h` (script olmayan header) → **derleme geçiyor**, kayıt
   listesinde yok. Eskiden kırıyordu.
2. `_B7Test.h` (bildirim) + `_B7Test.cpp` (gövde) → derleniyor,
   `_B7Test` kayıt listesine giriyor, `.cpp` içinden
   `#include "scripts/Utils.h"` çözülüyor.
3. Mevcut `Movement`, `asd`, `asdasd` kayıtları değişmeden kaldı.

Test dosyaları sonra silindi.

---

## 5. Bilerek yapılmayanlar

- **"Yeni Script" şablonu hâlâ tek header üretiyor.** En basit hâl
  varsayılan kalmalı; `.cpp`'ye bölmek isteyen elle böler.
- **Namespace hâlâ `FXGame` varsayılıyor.** Namespace'i de regex'le
  çıkarmak iç içe/anonim durumlarda kırılgan olurdu; konvansiyon
  belgelenip bırakıldı.
- **`.cpp` içinde tanımlanan script kaydedilmiyor.** Kayıt üretilen
  header'dan geçiyor, dolayısıyla sınıfın *bildirimi* bir header'da
  olmalı. Zaten doğru C++ pratiği.
- **Derlenmemiş script uyarısı yok.** İçerik panelinde duran ama
  `Game.dll`'de olmayan script sessizce yok; ayrı bir iş.
