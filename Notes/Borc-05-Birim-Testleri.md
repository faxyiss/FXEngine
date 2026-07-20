# 0.5 — Birim testleri (Catch2)

Faz 20'de planlanmıştı ama `SceneSerializer` ve `AssetManager` elle test
edilemeyecek kadar karmaşıklaştı — bu yüzden borç turuna alındı.

## Ne yapıldı

`Tests/` altında `FXTests` hedefi, Catch2 v3.5.4 (FetchContent).
26 test / 80 assertion, `ctest -C Debug` ile de çalışıyor.

| Dosya | Kapsam |
|---|---|
| `UUID.test.cpp` | Rastgelelik, 0'ın geçersizliği, değerden geri oluşturma, hash anahtarı |
| `Scene.test.cpp` | Entity oluşturma, UUID ile bulma, hiyerarşi, parent silinince çocuklar, `Scene::Copy` UUID koruması |
| `SceneSerializer.test.cpp` | Round-trip (transform, hiyerarşi, kamera), bozuk JSON, olmayan dosya, yükleme öncesi temizlik |
| `AssetManager.test.cpp` | GUID atama, ikinci taramada aynı GUID, taşıma, silme, çift kayıt, tanınmayan uzantı |

## Kararlar

**Sadece Engine test ediliyor, Editor değil.** Editör bir GUI uygulaması;
onu birim testiyle sürmek pahalı. Editör tarafı ekran görüntüsü + elle
doğrulama ile test ediliyor.

**Sahte (mock) dosya sistemi yok.** `TempProject` gerçek bir geçici
klasörde gerçek bir proje kuruyor ve yıkılırken siliyor. Test edilen şey
zaten "diske doğru yazıyor muyuz" — araya sahte katman koymak testi
kendi kurgusunu doğrulayan bir şeye çevirirdi.

**Testler pencere ve OpenGL bağlamı olmadan çalışıyor.** `SceneSerializer`
testlerinde `TextureLibrary*` olarak `nullptr` veriliyor. Bu mümkün, çünkü
serializer dokuyu kendisi yüklemiyor, kütüphaneye soruyor — Faz 12'deki
"bir sınıfın iki işi olmamalı" kararının bedava karşılığı.

**`FXENGINE_BUILD_TESTS` varsayılan AÇIK ama kapatılabilir.** Kapalıyken
Catch2 hiç indirilmiyor; motoru sadece derlemek isteyen biri test
bağımlılığı çekmek zorunda kalmasın.

**Log seviyesi testlerde `Warn`.** Motor her sahne kaydında INFO basıyor;
26 testte bu, başarısızlık çıktısını göremeyecek kadar gürültü demekti.
Uyarı ve hatalar duruyor — onları görmek istiyoruz.

## Test sonucu

```
All tests passed (80 assertions in 26 test cases)
100% tests passed out of 26   (ctest)
```

## Bilerek yapılmayanlar

- **`FileWatcher` testi yok.** Zamana ve işletim sistemine bağlı; güvenilir
  bir test yazmak debounce süresi kadar beklemeyi gerektirir ve kırılgan
  olur. 0.4'te gerçek projeyle doğrulandı.
- **Sürüm ≤3 sahne dönüşümü test edilmedi.** Elle yazılmış eski formatta
  fixture dosyaları gerekiyor; 0.1'de gerçek veriyle doğrulandı.
- **`Renderer2D` ve `TextureLibrary` test edilmiyor** — OpenGL bağlamı
  ister. Headless bağlam kurmak ayrı bir iş.
- **CI yok** — 20d'de.
