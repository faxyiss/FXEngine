#pragma once

#include <FXEngine/Scene/ScriptableEntity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Core/Input.h>
#include <FXEngine/Core/Log.h>

namespace FXEd
{
    // SINIF ADI DOSYA ADIYLA AYNI OLMALI: kayit CMake tarafindan buna
    // gore uretiliyor (bkz. Scripts/ScriptRegistrations.h.in).
    class Jump : public FX::ScriptableEntity
    {
    protected:
        // Play basladiginda bir kez.
        void OnCreate() override
        {
            FX_INFO("Jump: OnCreate (%s)", GetEntity().GetName().c_str());
        }

        // Her sabit adimda; dt her zaman ayni deger (1/60).
        void OnUpdate(float dt) override
        {
            auto& tf = GetComponent<FX::TransformComponent>();

            // Ornek: saniyede 1 birim saga.
            tf.Translation.x += 1.0f * dt;
        }

        // Stop'ta veya entity silindiginde bir kez.
        void OnDestroy() override
        {
        }
    };
}
