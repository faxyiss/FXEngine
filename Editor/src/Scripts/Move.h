#pragma once

#include <FXEngine/Scene/ScriptableEntity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Core/Input.h>

namespace FXEd
{
    // Ok tuslariyla hareket eder. FX::Input'un (13a) script tarafindan
    // ilk gercek kullanimi: sorgu tabanli girdi tam olarak bunun icin.
    class Move : public FX::ScriptableEntity
    {
    protected:
        void OnUpdate(float dt) override
        {
            auto& tf = GetComponent<FX::TransformComponent>();

            if (FX::Input::IsKeyPressed(FX::Key::Left))  tf.Translation.x -= m_Speed * dt;
            if (FX::Input::IsKeyPressed(FX::Key::Right)) tf.Translation.x += m_Speed * dt;
            if (FX::Input::IsKeyPressed(FX::Key::Down))  tf.Translation.y -= m_Speed * dt;
            if (FX::Input::IsKeyPressed(FX::Key::Up))    tf.Translation.y += m_Speed * dt;
        }

    private:
        float m_Speed = 5.0f;
    };
}
