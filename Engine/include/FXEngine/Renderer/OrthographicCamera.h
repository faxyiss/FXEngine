#pragma once

// ===========================================================================
// FXEngine - OrthographicCamera
//
// NEDEN ORTOGRAFIK, PERSPEKTIF DEGIL?
// Perspektif projeksiyonda uzaktaki nesne kucuk gorunur. 2D'de bunu
// ISTEMEYIZ: bir sprite, kameradan ne kadar uzakta olursa olsun ayni
// boyutta cizilmelidir. Ortografik projeksiyon derinligi goz ardi eder.
//
// UC UZAY, UC MATRIS - bu zinciri anlamak 3D matematigin belkemigidir:
//
//   Yerel (model) uzay --[Transform]--> Dunya uzayi
//   Dunya uzayi        --[View]------> Kamera uzayi
//   Kamera uzayi       --[Projection]-> Clip uzayi (NDC'ye giden)
//
// Shader'da tek satirda birlesir:
//   gl_Position = Projection * View * Transform * vec4(pos, 1.0)
//
// Bu sinif View ve Projection'i yonetir; Transform nesnenin kendi isi.
// ===========================================================================

#include <glm/glm.hpp>

namespace FX
{
    class OrthographicCamera
    {
    public:
        // Gorulen dunya dikdortgeni. Ornek: (-16, 16, -9, 9) -> 32x18 birimlik
        // bir alan gorunur ve 16:9 oraninda kare kare cikar.
        OrthographicCamera(float left, float right, float bottom, float top);

        void SetProjection(float left, float right, float bottom, float top);

        // En-boy oranina gore projeksiyonu kurar - GUNLUK KULLANIMDA BU.
        //
        // zoomLevel = "dikeyde kac birim gorunsun"un yarisi.
        // zoom 1.0 -> dikeyde 2 birim gorunur (-1..+1)
        // zoom 5.0 -> dikeyde 10 birim gorunur -> daha genis alan, nesneler kucuk
        //
        // Yataydaki genislik aspectRatio ile CARPILIR. Boylece bir kare,
        // pencere ne olursa olsun kare kalir. Faz 2'deki bozukluk tam
        // olarak bu carpanin eksikligiydi.
        void SetProjectionFromAspect(float aspectRatio, float zoomLevel);

        const glm::vec3& GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }

        // Derece cinsinden, Z ekseni etrafinda (2D'de tek anlamli donme ekseni).
        float GetRotation() const { return m_Rotation; }
        void SetRotation(float rotationDegrees) { m_Rotation = rotationDegrees; RecalculateViewMatrix(); }

        const glm::mat4& GetProjectionMatrix()     const { return m_ProjectionMatrix; }
        const glm::mat4& GetViewMatrix()           const { return m_ViewMatrix; }
        const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

    private:
        // View ve Projection her karede degismez; sadece kamera hareket
        // edince degisir. Bu yuzden carpimi ONCEDEN hesaplayip sakliyoruz -
        // her sprite icin yeniden carpmak israf olurdu.
        void RecalculateViewMatrix();

        glm::mat4 m_ProjectionMatrix{ 1.0f };
        glm::mat4 m_ViewMatrix{ 1.0f };
        glm::mat4 m_ViewProjectionMatrix{ 1.0f };

        glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
        float     m_Rotation = 0.0f;   // derece
    };
}
