#pragma once

#include <FXEngine/Scene/ScriptableEntity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Core/Log.h>

namespace FXEd
{
    // Surekli doner. Play'de donmesi, Stop'ta durmasi ve baslangic
    // acisina geri donmesi - uc seyi birden gosteriyor.
    class Spin : public FX::ScriptableEntity
    {
    protected:
        void OnCreate() override
        {
            FX_INFO("Spin: OnCreate (%s)", GetEntity().GetName().c_str());
        }

        void OnUpdate(float dt) override
        {
            GetComponent<FX::TransformComponent>().Rotation += m_Speed * dt;
        }

        void OnDestroy() override
        {
            FX_INFO("Spin: OnDestroy");
        }

    private:
        float m_Speed = 1.5f;   // radyan/saniye
    };
}
