# A4 — Script dosyası oluşturma + şablon

> Karar K5'in uygulanması (bkz. [02-Mimari.md](02-Mimari.md)).

## Karar: script'ler `Editor/src/Scripts/`'te kalıyor

K5 "içerik panelinde sağ tık → Yeni Script" diyordu. Ama içerik paneli
**projenin `assets/` klasörünü** gösteriyor ve orası derlenmiyor. Oraya
bir şablon yazsaydık **hiçbir zaman çalışmayacak bir dosya** üretmiş
olurduk — tam olarak bu projenin yasakladığı sessiz yanlışlık.

Exe'ye derlenen tek yer `Editor/src/Scripts/`. O yüzden "Yeni Script"
**Dosya menüsünde** ve oraya yazıyor.

Script'lerin gerçekten projenin içine taşınması **Aşama B**'nin işi:
oyun kodu ayrı bir DLL'e alınınca hem doğru yerde durur hem derlenir.

## Asıl kazanç: kayıt artık otomatik

A4'ten önce yeni bir script yazmak **iki yere** dokunmayı gerektiriyordu:

| Yer | Unutulursa |
|---|---|
| `Scripts/<Ad>.h` — sınıf | — |
| `RegisterEditorScripts()` — kayıt satırı | Script Inspector listesinde **hiç görünmez** |

İkincisi unutulduğunda hata **sessizdi**. A1'de component'ler için
çözülen problemin aynısı.

Artık CMake klasörü tarayıp include'u ve kaydı kendisi üretiyor:

```cmake
file(GLOB FXEDITOR_SCRIPT_HEADERS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/Scripts/*.h")
```

→ `build/Editor/generated/ScriptRegistrations.h`

**`CONFIGURE_DEPENDS` kritik.** CMake'te `GLOB` normalde tehlikelidir
(yeni dosyayı fark etmez, "neden derlenmiyor?" diye saatler kaybettirir)
ve bu proje bu yüzden dosyaları elle listeliyor. `CONFIGURE_DEPENDS`
derleme sırasında yeniden tarayarak o sakıncayı kaldırıyor — **ölçüldü:**
dosya eklendiğinde de silindiğinde de kayıt kendiliğinden güncellendi.

### Konvansiyon: dosya adı = sınıf adı = kayıt adı

`Scripts/Foo.h` → `class FXEd::Foo` → `"Foo"` olarak kayıtlı.

Üç kimliği ayırmak, birini değiştirince diğerlerini bozmak demekti.
Sahne dosyası bu adı saklıyor (Faz 16b: script'in kimliği adıdır, C++
tipi değil).

### Geçiş: `SpinScript.h` ikiye bölündü

Tek dosyada iki sınıf + kayıt fonksiyonu vardı; konvansiyona uymuyordu.
`Spin.h` ve `Move.h` oldu, sınıflar `SpinScript`/`MoveScript` → `Spin`/`Move`.

**Kayıt adları değişmedi** (`"Spin"`, `"Move"`) — mevcut sahne
dosyalarındaki script referansları kırılmıyor. Sınıf adı bir uygulama
detayı, kayıt adı ise dosya formatının parçası.

## Arayüz

Dosya → **Yeni Script...** → modal:

- Ad doğrulaması: harf/`_` ile başlamalı, yalnızca harf-rakam-`_`
  (C++ sınıf adı olacak)
- Aynı adda script varsa uyarı (`ScriptRegistry::Contains`)
- **Derleme şartı bastan yazılı:** "Script'in çalışması için editörü
  YENİDEN DERLE." Sonradan şikâyet ettirmek yerine baştan söylüyoruz.
- Oluşturunca dosya sistem editöründe açılıyor (`OpenExternally`)

Var olan dosya **ezilmiyor** — yazılmış bir script'i sessizce silmek
geri alınamaz bir kayıp olurdu.

## Doğrulama

Şablonun gerçekten **derlenen** kod ürettiğini iddia etmek yerine
ölçtüm (proje notlarındaki geçici öz-test yöntemi):

1. `OnInit`'e geçici `CreateScriptFile("GeciciTest", …)` eklendi
2. Editör çalıştırıldı → `Scripts/GeciciTest.h` oluştu, içeriği doğru
3. Yeniden derlendi → CMake dosyayı yakaladı, `ScriptRegistrations.h`'ye
   `Register<FXEd::GeciciTest>("GeciciTest")` yazdı, **temiz derledi**
4. Öz-test ve geçici dosya kaldırıldı → kayıt da kendiliğinden kayboldu
5. Yeniden derlendi, `FXTests` yeşil

| | Sonuç |
|---|---|
| Derleme | temiz |
| `FXTests` | 51 test / 238 assertion (motor koduna dokunulmadı) |
| Şablon → derlenen kod | **Uçtan uca ölçüldü** |

**Görsel doğrulama yapılmadı** — modalin görünüşü elle test edilmeli.

## Elle test edilmesi gerekenler (GUI)

1. Dosya → **Yeni Script...** → modal açılmalı.
2. Boş ad, rakamla başlayan ad, boşluklu ad → **Oluştur pasif** ve sebep
   yazmalı.
3. `Spin` yaz → "Bu adda bir script zaten var" uyarısı.
4. Geçerli bir ad (`Zipla`) → Oluştur → `Editor/src/Scripts/Zipla.h`
   oluşmalı ve **sistem editöründe açılmalı**.
5. **Editörü yeniden derle** → Inspector'da Native Script açılır
   listesinde `Zipla` görünmeli.
6. Bir entity'ye Native Script ekle → `Zipla` seç → Play → nesne sağa
   kaymalı (şablonun varsayılan davranışı).
7. Sahneyi kaydet, editörü kapat/aç → script bağlantısı korunmalı.
8. `Zipla.h`'yi sil, yeniden derle → listede kalmamalı, sahnedeki
   component **silinmemeli** ama "bu derlemede kayıtlı değil" uyarısı
   vermeli (Faz 16b davranışı).

## Yapılmadı

- **İçerik panelinde sağ tık** (K5'in sözü) — script'ler orada
  yaşamadığı için anlamsız olurdu. Aşama B'de script'ler projeye
  taşınınca doğal yeri orası olacak.
- **Otomatik yeniden derleme.** "Oluştur"a basınca CMake'i çalıştırmak
  mümkün ama editör çalışırken kendi exe'sini derlemek kilitli dosya
  demek. Aşama B'nin DLL'i bunu çözecek.
- **Script alanlarının Inspector'dan ayarlanması** — hâlâ A1'in bıraktığı
  yerde; `m_Speed` koda gömülü. Değerlerin nerede yaşayacağı (component
  içinde serileştirilebilir bir değer deposu) tasarlanmalı ve bu yapı
  C# köprüsünün de dayanacağı yer.
