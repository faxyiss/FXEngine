#pragma once

// ===========================================================================
// FXEngine - KeyCode / MouseButton
//
// Motor kullanicisi SDL bilmek zorunda degil: oyun kodu `Key::Space` yazar,
// altta ne oldugunu bilmez. Faz 13'un asil sorusu buydu - platform
// soyutlamasi nerede biter?
//
// DEGERLER SDL SCANCODE'LARIYLA AYNI. Bu bir tesaduf degil, bilincli bir
// tercih: ceviri katmani bir arama tablosu degil tek bir cast oluyor.
// "Sizinti" gibi gorunebilir ama sizan sey SDL'in KENDISI degil, sadece
// sayilari; SDL'i degistirirsek burada degisen tek sey bu dosya olur.
//
// SCANCODE, KEYCODE DEGIL: scancode tusun FIZIKSEL yeridir. AZERTY
// klavyede WASD'nin yeri QWERTY'deki ZQSD'dir; oyun icin dogru olan
// "sol ust harf" demektir, "W harfi" degil.
// ===========================================================================

#include <cstdint>

namespace FX
{
    enum class Key : std::uint16_t
    {
        Unknown = 0,

        A = 4, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        D1 = 30, D2, D3, D4, D5, D6, D7, D8, D9, D0,

        Enter     = 40,
        Escape    = 41,
        Backspace = 42,
        Tab       = 43,
        Space     = 44,

        Minus  = 45,
        Equals = 46,

        F1 = 58, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        Delete = 76,

        Right = 79,
        Left  = 80,
        Down  = 81,
        Up    = 82,

        LeftCtrl   = 224,
        LeftShift  = 225,
        LeftAlt    = 226,
        RightCtrl  = 228,
        RightShift = 229,
        RightAlt   = 230
    };

    enum class MouseButton : std::uint8_t
    {
        Left   = 1,
        Middle = 2,
        Right  = 3,
        X1     = 4,
        X2     = 5
    };
}
