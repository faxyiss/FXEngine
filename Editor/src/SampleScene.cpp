#include "SampleScene.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/Entity.h>

#include <cmath>
#include <random>
#include <string>

namespace FXEd::SampleScene
{
    namespace
    {
        // Sabit tohum: ornek sahne her acilista ayni gorunsun.
        std::mt19937 s_Rng{ 12345 };

        float RandFloat(float min, float max)
        {
            std::uniform_real_distribution<float> dist(min, max);
            return dist(s_Rng);
        }
    }

    void SpawnMover(FX::Scene& scene, const TexturePtr& circle)
    {
        auto e = scene.CreateEntity("Gezgin");

        auto& tf = e.GetComponent<FX::TransformComponent>();
        tf.Translation = { RandFloat(-8.0f, 8.0f), RandFloat(-8.0f, 8.0f), 0.0f };
        const float s = RandFloat(0.2f, 0.6f);
        tf.Scale = { s, s };

        if (RandFloat(0.0f, 1.0f) > 0.5f)
            e.AddComponent<FX::SpriteRendererComponent>(
                circle, glm::vec4{ RandFloat(0.3f, 1.0f), RandFloat(0.3f, 1.0f),
                                   RandFloat(0.3f, 1.0f), 0.9f });
        else
            e.AddComponent<FX::SpriteRendererComponent>(
                glm::vec4{ RandFloat(0.3f, 1.0f), RandFloat(0.3f, 1.0f),
                           RandFloat(0.3f, 1.0f), 0.9f });

        e.AddComponent<FX::VelocityComponent>(
            glm::vec2{ RandFloat(-1.5f, 1.5f), RandFloat(-1.5f, 1.5f) },
            RandFloat(-2.0f, 2.0f));
    }

    FX::UUID Build(FX::Scene& scene, const TexturePtr& checkerboard,
                   const TexturePtr& circle)
    {
        FX::UUID playerUuid{ 0 };

        {
            auto floor = scene.CreateEntity("Zemin");
            auto& tf = floor.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, -0.5f };
            tf.Scale       = { 30.0f, 30.0f };

            auto& sprite = floor.AddComponent<FX::SpriteRendererComponent>(checkerboard);
            sprite.TilingFactor = 15.0f;
            sprite.Color = { 0.55f, 0.58f, 0.70f, 1.0f };
        }

        {
            auto player = scene.CreateEntity("Oyuncu");
            auto& tf = player.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, 0.1f };

            player.AddComponent<FX::SpriteRendererComponent>(
                circle, glm::vec4{ 1.0f, 0.85f, 0.3f, 1.0f });
            player.AddComponent<FX::VelocityComponent>();

            // Tutamak degil KIMLIK sakliyoruz - yuklemeden sag cikar.
            playerUuid = player.GetComponent<FX::IDComponent>().ID;
        }

        // --- Takipciler --------------------------------------------------------
        // Faz 8'in kanit sahnesi: bu uc entity oyuncuyu UUID uzerinden
        // takip ediyor. Sahneyi kaydedip yukledikten sonra da takip
        // ETMEYE DEVAM etmeliler - iste kalici kimligin butun mesele.
        for (int i = 0; i < 3; ++i)
        {
            auto e = scene.CreateEntity("Takipci " + std::to_string(i));

            auto& tf = e.GetComponent<FX::TransformComponent>();
            tf.Translation = { -6.0f + static_cast<float>(i) * 1.5f, -6.0f, 0.05f };
            tf.Scale = { 0.7f, 0.7f };

            e.AddComponent<FX::SpriteRendererComponent>(
                circle, glm::vec4{ 1.0f, 0.35f, 0.35f, 1.0f });
            e.AddComponent<FX::VelocityComponent>();

            auto& fc = e.AddComponent<FX::FollowComponent>();
            fc.Target       = playerUuid;
            fc.Speed        = 1.5f + static_cast<float>(i) * 0.5f;
            fc.StopDistance = 1.0f + static_cast<float>(i) * 0.6f;
        }

        // --- Hiyerarsi ornegi ---------------------------------------------------
        // Govde doner; kule ve namlu onun cocugu oldugu icin birlikte doner.
        // Kule kendi de doner -> namlu iki donmeyi birden alir.
        {
            auto body = scene.CreateEntity("Tank Govde");
            auto& btf = body.GetComponent<FX::TransformComponent>();
            btf.Translation = { -6.0f, 4.0f, 0.2f };
            btf.Scale       = { 2.0f, 2.0f };
            body.AddComponent<FX::SpriteRendererComponent>(
                checkerboard, glm::vec4{ 0.45f, 0.75f, 0.45f, 1.0f });
            body.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 0.4f);

            auto turret = scene.CreateEntity("Tank Kule");
            auto& ttf = turret.GetComponent<FX::TransformComponent>();
            ttf.Translation = { 0.0f, 0.0f, 0.05f };
            ttf.Scale       = { 0.55f, 0.55f };
            turret.AddComponent<FX::SpriteRendererComponent>(
                circle, glm::vec4{ 1.0f, 0.9f, 0.4f, 1.0f });
            turret.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 1.5f);
            turret.SetParent(body);

            auto barrel = scene.CreateEntity("Tank Namlu");
            auto& natf = barrel.GetComponent<FX::TransformComponent>();
            natf.Translation = { 0.9f, 0.0f, 0.05f };
            natf.Scale       = { 1.4f, 0.35f };
            barrel.AddComponent<FX::SpriteRendererComponent>(
                glm::vec4{ 0.9f, 0.35f, 0.25f, 1.0f });
            barrel.SetParent(turret);
        }

        for (int i = 0; i < 8; ++i)
        {
            auto e = scene.CreateEntity("Uydu " + std::to_string(i));

            const float angle = static_cast<float>(i) / 8.0f * 6.2831853f;
            auto& tf = e.GetComponent<FX::TransformComponent>();
            tf.Translation = { std::cos(angle) * 4.0f, std::sin(angle) * 4.0f, 0.0f };
            tf.Scale       = { 0.6f, 0.6f };
            tf.Rotation    = angle;

            e.AddComponent<FX::SpriteRendererComponent>(
                checkerboard,
                glm::vec4{ 0.4f + RandFloat(0.0f, 0.6f), 0.4f + RandFloat(0.0f, 0.6f), 0.9f, 1.0f });
            e.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 1.2f);
        }

        for (int i = 0; i < 15; ++i)
            SpawnMover(scene, circle);

        // Play modunun bakacagi kamera. Oyuncuya parent yapiliyor:
        // takip mantigi icin ozel bir koda gerek yok, hiyerarsi
        // (Faz 9) zaten bunu yapiyor.
        {
            auto cam = scene.CreateEntity("Ana Kamera");
            cam.AddComponent<FX::CameraComponent>(6.0f, true);
            if (FX::Entity player = scene.FindEntityByUUID(playerUuid))
                cam.SetParent(player);
        }

        FX_INFO("Ornek sahne kuruldu: %u entity", scene.GetEntityCount());
        return playerUuid;
    }
}
