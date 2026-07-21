# Faz 16a — Native script çekirdeği

Motorun ilk defa gerçekten *oyun* çalıştırdığı yer. Ön koşulları hazırdı:
Faz 10 (play modu) ve 13a (`FX::Input`).

## Ne geldi

| Parça | Yer |
|---|---|
| `ScriptableEntity` — `OnCreate` / `OnUpdate` / `OnDestroy` | `Scene/ScriptableEntity.h` |
| `NativeScriptComponent` — bağlantıyı taşıyan **veri** | `Scene/Components.h` |
| `ScriptSystem` — örneklerin yaşam döngüsü | `Scene/Systems.h`, `.cpp` |
| `Scene::OnRuntimeStart/Stop`, `IsRunning()` | `Scene.h`, `.cpp` |
| Örnek script'ler (`Spin`, `Move`) + Inspector'da ekleme | `Editor/src/Scripts/` |

## Kararlar

**Script component'in kendisi değil, component'in *içinde*.** ECS'in
"component saf veri olmalı" kuralı bozulmasın diye: `NativeScriptComponent`
yalnızca iki fonksiyon işaretçisi, bir ad ve bir örnek işaretçisi taşıyor.
Script, component'leri **düzenleyen** bir nesne — verinin kendisi değil.

**`OnCreate`/`OnUpdate`/`OnDestroy` `protected`.** Bunları motor çağırır.
Dışarıdan çağrılabilir olsaydı "iki kez `OnCreate`" gibi hatalar mümkün
olurdu.

**Fonksiyon işaretçisi, `std::function` değil.** `Bind<T>()` derleme
zamanında biliniyor, kapatma durumu yok ve component her `Scene::Copy`'de
kopyalanıyor — hafif kalması kopyalamayı basit tutuyor.

**Script'ler sistem sırasında EN ÖNDE.** Oyun mantığı hızı ve hedefi bu
karede belirlesin, sonraki sistemler (Follow → Movement → Transform) onu
uygulasın. Sonda çalışsaydı yazdıkları bir kare gecikmeyle görünürdü.

**Yalnızca Play modunda.** Edit modunda script çalışması, düzenlerken
nesnelerin kaçması demek — Faz 10'un dersi. `Scene::OnUpdate` içinde
`m_Running` kontrolü var, yani "editör çağırmayı unutur" ihtimali yok.

**Yaşam döngüsü `Scene`'de, editörde değil.** "Sahnenin çalışıyor olması"
sahnenin durumu; her tüketicinin `ScriptSystem`'i ayrı ayrı çağırması
gerekseydi biri unuturdu. `Scene` yıkıcısı da `OnRuntimeStop` çağırıyor.

## Yakalanan tuzak: `Scene::Copy` ve ham işaretçi

`NativeScriptComponent`'i `AllComponents`'e eklemek zorunluydu (yoksa Play
moduna geçince script kaybolurdu) — ama bu, `Instance` işaretçisinin de
kopyalanması demekti. İki sahne aynı nesneyi gösterir ve **ikisi de silmeye
çalışırdı**.

Çözüm: `Scene::Copy` sonunda tüm `Instance`'lar `nullptr`'a çekiliyor.
Kopya sahne kendi örneklerini `OnRuntimeStart`'ta yaratır. Bunu doğrulayan
bir birim testi var.

## Test

38 test / 134 assertion (5'i yeni, `Script.test.cpp`):
- Script yalnızca Play–Stop arasında yaşıyor (Edit'te `OnUpdate` bile
  çağrılsa çalışmıyor)
- Script kendi entity'sinin component'ini değiştirebiliyor
- Bind edilmemiş component çökmüyor
- `Scene::Copy` örneği kopyalamıyor, bağlantıyı kopyalıyor
- Sahne yok edilince `OnDestroy` çağrılıyor (sızıntı yok)

---

# Faz 16b — Kayıt defteri ve serileştirme

16a'da script `Bind<T>()` ile **derleme zamanında** bağlanıyordu. Bu iki
şeyi imkânsız kılıyordu: sahne dosyasına fonksiyon işaretçisi yazılamaz,
ve Inspector'daki menü elle güncellenmeyi bekler.

## Ne değişti

**`ScriptRegistry`** (`Scene/ScriptRegistry.h`) — ad → sınıf eşlemesi.
`Register<T>("Spin")`, `Create(name)`, `Contains`, `GetNames()`.

**`NativeScriptComponent` sadeleşti:** fonksiyon işaretçileri kalktı,
geriye tek bir `ScriptName` (+ çalışma zamanı `Instance`) kaldı.
`ScriptSystem` örneği registry'den yaratıyor.

**Serileştirme geldi:** `EntitySerialization` script adını yazıyor ve
okuyor. Sahne formatı **kırılmadı** — yeni bir alan eklendi, eski dosyalar
olduğu gibi açılıyor, sürüm artırmaya gerek kalmadı.

**Inspector'da combo:** liste `ScriptRegistry::GetNames()`'ten geliyor,
yani yeni bir script kaydedildiğinde kendiliğinden görünüyor. Elle yazılmış
menü silindi.

## Kararlar

**Script'in kimliği ADIDIR, C++ tipi değil.** Faz 8'in "kimlik ile konum
ayrı şeylerdir" fikrinin bir başka yüzü: dosyaya yazılabilen tek şey
kimliktir, işaretçi değil.

**Kayıt uygulamanın işi, motorun değil.** Motor hangi script'lerin var
olduğunu bilemez; `RegisterEditorScripts()` editörde duruyor.

**Bilinmeyen ad component'i silmiyor.** Sahne başka bir derlemeden gelmiş
olabilir. Adı atsaydık, o sahneyi kaydeden kişi bağlantısını *sessizce*
kaybederdi. Bunun yerine: Inspector'da kırmızı uyarı + `ScriptSystem`'den
log. Bilgiyi korumak, sessiz temizlikten iyidir.

**Aynı ad ikinci kez kaydedilemez.** Sessizce ezmek hangi sınıfın
çalıştığını belirsiz kılardı.

**Script seçimi menüde değil Inspector'da.** "Component Ekle" boş bir
`NativeScript` ekliyor, seçim combo'dan yapılıyor — aynı seçim iki yerde
yaşamasın.

## Test

41 test / 145 assertion (3'ü yeni):
- Kayıtlı olmayan ad çökmüyor **ve ad korunuyor**
- Aynı ad iki kez kaydedilmiyor
- **Script adı sahne dosyasına yazılıp geri okunuyor** ve yüklenen sahne
  gerçekten çalışıyor — 16b'nin asıl vaadi bu

## 16c'ye kalanlar

- Gerçek örnekler (`PlayerController`, `FollowTarget`) ve küçük bir oyun.
- Script'lerin ayarlanabilir alanları (hız vb.) hâlâ koda gömülü;
  Inspector'dan düzenlemek ayrı bir iş (reflection ya da elle tanımlanan
  alan listesi).
