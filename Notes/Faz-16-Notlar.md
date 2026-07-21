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

## 16b'ye kalanlar

- **Serileştirme yok.** `NativeScriptComponent` sahne dosyasına yazılmıyor;
  Play'de eklenen script Stop'ta kaybolur, kaydedilen sahnede hiç görünmez.
- **Script kaydı (factory) yok.** Inspector'daki liste elle yazılı
  (`SceneHierarchyPanel.cpp` içinde `Spin` ve `Move`). Bir kayıt defteri
  gelince liste kendini dolduracak ve ad→sınıf çözümü serileştirmeyi
  mümkün kılacak.
- Script'lerin editörde durması geçici: 16c'de gerçek örnekler
  (`PlayerController`, `FollowTarget`) motorun örnek oyununa taşınacak.
