#pragma once

// ===========================================================================
// Faz 16a'nin KANIT script'i.
//
// Editorde duruyor cunku script kaydi (factory) ve Inspector arayuzu
// 16b'nin isi; su an script'i baglayabilen tek yer editor kodu.
// 16c'de gercek ornekler (PlayerController, FollowTarget) motorun
// ornek oyununa tasinacak.
// ===========================================================================

#include <FXEngine/Scene/ScriptableEntity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Core/Input.h>
#include <FXEngine/Core/Log.h>

namespace FXEd
{
    // Surekli doner. Play'de donmesi, Stop'ta durmasi ve baslangic
    // acisina geri donmesi - uc seyi birden gosteriyor.
    class SpinScript : public FX::ScriptableEntity
    {
    protected:
        void OnCreate() override
        {
            FX_INFO("SpinScript: OnCreate (%s)", GetEntity().GetName().c_str());
        }

        void OnUpdate(float dt) override
        {
            GetComponent<FX::TransformComponent>().Rotation += m_Speed * dt;
        }

        void OnDestroy() override
        {
            FX_INFO("SpinScript: OnDestroy");
        }

    private:
        float m_Speed = 1.5f;   // radyan/saniye
    };

    // Ok tuslariyla hareket eder. FX::Input'un (13a) script tarafindan
    // ilk gercek kullanimi: sorgu tabanli girdi tam olarak bunun icin.
    class MoveScript : public FX::ScriptableEntity
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
