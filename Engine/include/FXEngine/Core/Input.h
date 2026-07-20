#pragma once

// ===========================================================================
// FXEngine - Input
//
// SORGU tabanli girdi: "su an bu tus basili mi?" Olay tabanli girdi
// (Events/) ayri bir mekanizma ve ikisi farkli sorulari cevaplar:
//
//   Input  -> "ileri gidiyor mu?"        (her karede sorulur, surekli)
//   Event  -> "zipla tusuna BASTI mi?"   (bir kez olur, kacirilmamali)
//
// Oyun kodunun cogu birincisini ister; ikisini tek API'ye sikistirmak
// her ikisini de kotu yapardi.
//
// Statik: tek bir klavye ve tek bir fare var. Bunu bir nesneye
// sarmalamak, olmayan bir esnekligi taklit etmek olurdu.
// ===========================================================================

#include "FXEngine/Core/KeyCodes.h"

namespace FX
{
    class Input
    {
    public:
        static bool IsKeyPressed(Key key);
        static bool IsMouseButtonPressed(MouseButton button);

        // Fare konumu PENCEREYE goreli, piksel. Viewport'a goreli degil -
        // motor viewport diye bir sey bilmiyor, o editorun kavrami.
        static void  GetMousePosition(float& x, float& y);
        static float GetMouseX();
        static float GetMouseY();
    };
}
