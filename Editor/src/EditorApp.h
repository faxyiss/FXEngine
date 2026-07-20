#pragma once

// ===========================================================================
// FXEditor - EditorApp
//
// FX::Application'dan tureyip motorun birakigi bosluklari doldurur.
// Motor "ne zaman" cagrilacagini bilir; bu sinif "ne yapilacagini" bilir.
// ===========================================================================

#include <FXEngine/Core/Application.h>
#include <FXEngine/Renderer/Shader.h>
#include <FXEngine/Renderer/VertexArray.h>

#include <memory>

namespace FXEd
{
    // Editor kodu FXEd namespace'inde. Neden ayri namespace?
    // Motor FX::, editor FXEd:: -> bir isim gordugunde hangi katmana ait
    // oldugunu aninda anlarsin.
    class EditorApp : public FX::Application
    {
    public:
        EditorApp();

    protected:
        void OnInit()               override;
        void OnShutdown()           override;
        void OnUpdate(float dt)     override;
        void OnRender(float alpha)  override;
        void OnEvent(const SDL_Event& event) override;

    private:
        // --- Faz 2: cizim kaynaklari ------------------------------------------
        // unique_ptr: Shader'in tek sahibi biziz.
        // shared_ptr: VertexArray API'si shared_ptr ile calisiyor (buffer'lar
        //             paylasilabilsin diye), tutarli olmak icin ayni turu tutuyoruz.
        std::unique_ptr<FX::Shader>      m_Shader;
        std::shared_ptr<FX::VertexArray> m_QuadVA;

        // --- Durum -------------------------------------------------------------
        float m_Time = 0.0f;

        std::uint64_t m_UpdateCount = 0;
        std::uint64_t m_FrameCount  = 0;

        // Hata ayiklama anahtarlari
        bool m_Wireframe = false;
        bool m_Animate   = true;
    };
}
