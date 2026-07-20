# FXEngine — Yol Haritası

Kural: aynı anda tek faz. Ben "test ettim, çalışıyor" demeden sonraki faza geçilmez.

| Faz | Konu | Durum |
|-----|------|-------|
| 0 | CMake, klasör yapısı, bağımlılıklar, boş uygulama | ✅ onaylandı |
| 0.5 | Log sistemi (FX_INFO/WARN/ERROR + FX_ASSERT) | ✅ onaylandı |
| 1 | SDL3 pencere + GL 3.3 context + glad + fixed-timestep loop | ✅ onaylandı |
| 2 | Shader sınıfı + VBO/VAO/EBO + tek renkli quad | ✅ onaylandı |
| 3 | stb_image texture + texture'lı quad + ortho 2D kamera | 🟡 test bekliyor |
| 4 | Batch renderer (dinamik VB, texture slot'ları) | ⬜ |
| 5 | EnTT (Transform, SpriteRenderer, Tag + Movement/Render system) | ⬜ |
| 6 | ImGui docking + framebuffer→Viewport + Hierarchy + Inspector | ⬜ |
| 7 | JSON sahne kaydet/yükle → MVP | ⬜ |

Durum kodları: ⬜ başlamadı · 🟡 test bekliyor · ✅ onaylandı

## Klasör yapısı
```
CMakeLists.txt          üst proje, global ayarlar
cmake/Dependencies.cmake tüm 3rd-party
Engine/  include/FXEngine/  → public API (Editor bunu görür)
         src/               → private implementasyon
Editor/  src/               → uygulama
Notes/                      → bu klasör (kod değil)
_deps/                      → indirilen bağımlılıklar (gitignore)
build/                      → derleme çıktısı (gitignore)
```

## Değişmez mimari kurallar
1. Engine, Editor'ı hiç bilmez. Tek yönlü bağımlılık.
2. Veri component'te, davranış system'de.
3. Erken optimizasyon yok.
