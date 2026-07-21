#pragma once

// ===========================================================================
// Ornek script'ler ve kayitlari.
//
// KAYIT UYGULAMANIN ISI, motorun degil: motor hangi script'lerin var
// oldugunu bilemez. 16c'de gercek ornekler (PlayerController,
// FollowTarget) buraya eklenecek.
// ===========================================================================

#include <FXEngine/Scene/ScriptableEntity.h>
#include <FXEngine/Scene/ScriptRegistry.h>
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

    // Editor acilirken bir kez cagrilir. Yeni bir script yazdiginda
    // BURAYA da eklemen gerekiyor - kayit defterinin bedeli bu. Kars
    // ederi: Inspector listesi, serilestirme ve sahne dosyasinin
    // tasinabilirligi.
    inline void RegisterEditorScripts()
    {
        FX::ScriptRegistry::Register<SpinScript>("Spin");
        FX::ScriptRegistry::Register<MoveScript>("Move");
    }
}
