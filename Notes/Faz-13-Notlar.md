# Faz 13 — Girdi soyutlaması (13a + 13b)

13c (`Layer`/`LayerStack`) ve 13d (action mapping) **bilerek ertelendi** —
gerekçe aşağıda.

## 13a — `FX::Input` ve tuş kodları

`FX::Key`, `FX::MouseButton` ([KeyCodes.h](../Engine/include/FXEngine/Core/KeyCodes.h))
ve sorgu tabanlı `FX::Input` ([Input.h](../Engine/include/FXEngine/Core/Input.h)).

**Enum değerleri SDL scancode'larıyla kasten aynı.** Böylece çeviri bir
arama tablosu değil tek bir `static_cast` oluyor. Sızan şey SDL'in
*kendisi* değil sadece sayıları; SDL değişirse değişen tek dosya bu.
Değerlerin sessizce kaymaması için 4 birim testi var (`Input.test.cpp`) —
bu tür bir hata "bazı tuşlar çalışmıyor" diye görünürdü, izi sürülmesi zor.

**Scancode, keycode değil.** Scancode tuşun *fiziksel* yeridir. AZERTY
klavyede WASD'nin yeri QWERTY'deki ZQSD'dir; oyun için doğru olan "sol üst
harf"tir, "W harfi" değil.

## 13b — Olay sistemi

[Events/](../Engine/include/FXEngine/Events/): `Event` + `EventDispatcher`,
`KeyPressed/Released`, `MouseMoved/Scrolled/ButtonPressed/Released`,
`WindowClose/Resize`. SDL → motor çevirisi `src/Events/EventTranslator.cpp`
içinde, yani **motor kullanıcısı `SDL_Event` görmüyor**.

`Application` artık iki kanca sunuyor:

| Kanca | Kim kullanır |
|---|---|
| `OnRawEvent(const SDL_Event&)` | ImGui backend'i, işletim sistemi sürükle-bırakması |
| `OnEvent(FX::Event&)` | Diğer her şey — kısayollar, kamera, ileride script |

Editörde ham SDL girdi kullanımı **sıfırlandı** (`SDL_GetKeyboardState`,
`SDL_SCANCODE_*`, `SDLK_*` — hiçbiri kalmadı).

## Kararlar

**Sorgu ve olay iki ayrı mekanizma, birleştirilmedi.** İkisi farklı soru
cevaplıyor: `Input` → "ileri gidiyor mu?" (her karede), `Event` → "zıpla
tuşuna *bastı* mı?" (bir kez olur, kaçırılmamalı). Tek API'ye sıkıştırmak
ikisini de kötü yapardı.

**Sanal fonksiyon, `std::variant` değil.** Variant daha hızlı olurdu ama
yeni bir olay tipi eklemek tüm ziyaretçileri değiştirmeyi gerektirirdi.
Olay tipleri zamanla artar; kare başına birkaç olayda bu maliyet ölçülebilir
değil.

**ImGui tüketimi `Handled` ile taşınıyor.** `OnRawEvent`'te ImGui olayı
tükettiyse `MarkRawEventHandled()` çağrılıyor; motor olayı `Handled=true`
geliyor ve `EventDispatcher` onu atlıyor. Bu olmadan Inspector'daki metin
kutusuna `s` yazmak sahneyi kaydederdi.

**`IsRepeat()` kısayollarda yok sayılıyor.** Ctrl+S'i basılı tutmak on kez
kaydetmemeli. Metin girişi ise tekrarı ister — bu yüzden bayrak olayda
taşınıyor, çeviride atılmıyor.

**Modifier durumu olayın yanında taşınıyor**, `Input`'a ayrıca sorulmuyor:
olay işlendiğinde tuşlar bırakılmış olabilir.

**Tekerlek `WantCaptureMouse`'a tabi değil.** Viewport'un kendisi bir ImGui
penceresi olduğu için fare üzerindeyken o bayrak hep `true` döner; genel
kontrole bırakılsaydı zoom hiç çalışmazdı. Ölçüt "ImGui fareyi istiyor mu"
değil, "fare viewport'ta mı".

## Neden 13c ve 13d ertelendi

**`LayerStack` şu an var olmayan bir soruna çözüm.** Editörün tek katmanı
var; ikinci bir katman (oyun katmanı) ancak Faz 16'da anlam kazanacak.
Şimdi yazmak "aşırı soyutlamanın maliyeti" dersini teoride öğrenmek olurdu.

**Action mapping'i gerçek bir oyun yazmadan tasarlamak körlemesine.**
"Zıpla = Space veya gamepad A" iyi bir örnek ama gerçek ihtiyaçları
(eksen mi buton mu, ölü bölge, yeniden atama) ancak 16c'deki örnek oyunda
görürüz.

İkisi de yol haritasının sonuna alındı.

## Test

Birim: 33 test / 117 assertion (4'ü yeni — enum ↔ SDL eşleşmesi).

Editörde elle doğrulandı: WASD kamera, tekerlekle zoom (viewport'ta zoom,
diğer panellerde kaydırma), tüm kısayollar, ImGui metin kutusunda harf
yazarken kısayolların tetiklenmemesi, Ctrl+S basılı tutma, sürükle-bırak,
gizmo snap.
