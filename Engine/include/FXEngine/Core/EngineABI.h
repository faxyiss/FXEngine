#pragma once

// ===========================================================================
// FXEngine - Motor/Game.dll ikili uyumluluk damgasi
//
// Game.dll motorun header'lariyla derlenir. ScriptableEntity'nin vtable'i
// ya da sinirdan gecen bir tipin bellek yerlesimi degistiginde eski DLL
// SESSIZCE uyumsuz kalir: cagri yanlis sanal fonksiyona gider, sonuc
// cokme ya da daha kotusu sessiz yanlislik olur. Kullaniciya dusen ipucu
// yoktur - "bir kez Derle'ye bas" bilgisi bugune kadar devir notunda
// yaziyordu, kodda degil.
//
// Bu sabit Game.dll'e derleme zamaninda gomuluyor (uretilen GameMain.cpp
// onu FXEngineABIVersion olarak disa aktariyor) ve editor DLL'i
// yuklemeden ONCE karsilastiriyor.
//
// SAYIYI ARTIR:
//   - ScriptableEntity'ye sanal fonksiyon eklenince/cikinca/siralari
//     degisince
//   - ScriptFieldVisitor arayuzu degisince
//   - DLL sinirindan gecen bir tipin (Entity, Scene, component'ler)
//     yerlesimi degisince
//
// Damgasi olmayan DLL "uyumsuz" sayilir: bu damgadan onceki her derleme
// oyle. Bedeli bir kez fazladan derleme, karsiligi sessiz cokmenin
// ortadan kalkmasi.
// ===========================================================================

#define FX_ENGINE_ABI_VERSION 1
