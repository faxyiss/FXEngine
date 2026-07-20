# Faz 0.5 — Log sistemi

Faz listesinde yoktu, sonradan eklendi. Sebep: Faz 1'den itibaren gelecek
hataları (`pencere açılmadı`, `shader derlenmedi`, `texture yok`) `printf`
ile kovalamak acı verir.

## Dosyalar
- `Engine/include/FXEngine/Core/Log.h` — enum'lar, sınıf, makrolar
- `Engine/src/Core/Log.cpp` — implementasyon

## Öğrenilecek kavramlar

### 1. Header'ı hafif tut
`Log.h` motorun her .cpp'sine girecek. Bu yüzden içinde `<string>`,
`<iostream>` **yok** — sadece bir enum + bildirim. Ağırlık .cpp'de.
Kural: bir header ne kadar çok yerden include ediliyorsa, o kadar yalın olmalı.

### 2. Neden makro, fonksiyon değil?
- `__FILE__` / `__LINE__` gibi *çağıran yerin* bilgisine sadece makro erişir
- Release'de makroyu boşa çevirip çağrıyı koddan tamamen **silebiliriz**

### 3. `FX_CORE_*` vs `FX_*`
Motor kodu `FX_CORE_INFO`, editör kodu `FX_INFO` kullanır → çıktıda
`[ENGINE]` / `[EDITOR]`. Uzun log akışında hatanın kaynağını okumadan görürsün.

### 4. Log vs Assert — felsefe farkı
- **Log (`FX_ERROR`)** → *kullanıcı/ortam* hatası. "Dosya bulunamadı."
  Program devam edebilmeli.
- **Assert (`FX_ASSERT`)** → *programcı* hatası. "Renderer başlatılmadan
  çizim yapıldı." Kod yanlış, hemen dur.

⚠️ Assert içine **yan etkisi olan kod yazma**: Release'de koşul hiç
değerlendirilmez. `FX_ASSERT(Init(), "...")` YANLIŞ.

### 5. `do { } while(0)` sarmalı
Makronun tek bir ifade gibi davranmasını sağlar, `if/else` bozulmaz. Klasik C hilesi.

### 6. Generator expression: `$<$<CONFIG:Debug>:FX_DEBUG>`
Visual Studio multi-config generator'dır → hangi config'in derlendiği
**configure zamanında belli değil**, build zamanında belli olur.
Bu yüzden `if(CMAKE_BUILD_TYPE)` çalışmaz. Generator expression tam olarak
bu sorunu çözer.

### 7. Windows'ta ANSI renk
Windows konsolu ANSI kodlarını varsayılan olarak anlamaz, ekrana çöp basar.
`SetConsoleMode(..., ENABLE_VIRTUAL_TERMINAL_PROCESSING)` ile açılır.
`Log::Init()` bunu yapar → **main()'in en başında çağrılmalı.**

### 8. `WIN32_LEAN_AND_MEAN` + `NOMINMAX`
`Windows.h` devasa. İlki alt sistemlerin çoğunu dışarıda bırakır (derleme hızı).
İkincisi Windows'un `min`/`max` **makrolarını** engeller — yoksa `std::min`,
`glm::max` çağrıları bozulur. Windows'ta bu ikisi neredeyse her zaman gerekli.

## Çıktı formatı
```
[04:28:47] INFO  [EDITOR] mesaj
 └ gri     └ seviye └ kaynak
```
Seviye renkleri: TRACE gri · INFO beyaz · WARN sarı · ERROR kalın kırmızı
Kaynak renkleri: ENGINE mavi · EDITOR yeşil

## Doğrulanan davranışlar
- [x] 4 seviye de farklı renkte basılıyor
- [x] ENGINE/EDITOR ayrımı çalışıyor
- [x] `SetLevel(Warn)` → Trace ve Info bastırıldı
- [x] `FX_ASSERT` doğru koşulda programı durdurmuyor
- [x] Debug modunda `FX_DEBUG` tanımlı

## Onay
- [x] Derlendi
- [x] Çalıştı
- [ ] Kullanıcı onayı → Faz 1'e geçiş
