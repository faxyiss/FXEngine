#include "FXEngine/Renderer/OrthographicCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace FX
{
    OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top)
    {
        SetProjection(left, right, bottom, top);
        RecalculateViewMatrix();
    }

    void OrthographicCamera::SetProjection(float left, float right, float bottom, float top)
    {
        // glm::ortho, dunyadaki bir dikdortgen prizmayi NDC kupune (-1..+1)
        // esleyen matrisi uretir.
        //
        // Son iki parametre yakin/uzak duzlem. 2D'de -1..1 yeterli:
        // z=0'daki her sey gorunur. Aralik cok darsa sprite'lari
        // katmanlamak icin z kullanamayiz, cok genis olmasinin da zarari yok.
        //
        // DIKKAT: glm::ortho'da bottom < top verirsek Y YUKARI artar
        // (matematik konvansiyonu). Tersini verseydik ekran koordinati gibi
        // Y asagi artardi. Biz matematik konvansiyonunu seciyoruz cunku
        // glm ve OpenGL'in geri kalani da boyle dusunuyor.
        m_ProjectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
    }

    void OrthographicCamera::SetProjectionFromAspect(float aspectRatio, float zoomLevel)
    {
        // Dikeyde sabit sayida birim goster, yatayi en-boy oraniyla genislet.
        //
        // Bunun anlami: pencereyi yatayda genislettiginde nesneler BUYUMEZ,
        // sadece DAHA FAZLA ALAN gorursun. Cogu 2D oyunun istedigi davranis bu.
        //
        // Faz 2'de bu carpan yoktu; o yuzden 1280x720 pencerede kare
        // dikdortgen gorunuyordu. Duzelen sey tam olarak burasi.
        SetProjection(-aspectRatio * zoomLevel,  aspectRatio * zoomLevel,
                      -zoomLevel,                zoomLevel);
    }

    void OrthographicCamera::RecalculateViewMatrix()
    {
        // View matrisi kafa karistiricidir, cunku KAMERAYI degil DUNYAYI
        // hareket ettirir. Kamerayi saga kaydirmak = dunyayi sola kaydirmak.
        //
        // Bu yuzden once kameranin donusumunu kuruyoruz, sonra TERSINI aliyoruz.
        const glm::mat4 transform =
            glm::translate(glm::mat4(1.0f), m_Position) *
            glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1));

        // inverse() burada dogru ama pahali bir islemdir. Kamera her karede
        // hareket etse bile bu yilda bir kez degil kare basina bir kez
        // calisir - sorun degil. Erken optimizasyon yapmiyoruz; gerekirse
        // profiling sonrasi elle ters matris kurariz.
        m_ViewMatrix = glm::inverse(transform);

        // SIRA KRITIK: Projection * View. Ters yazarsan matris carpimi
        // degismeli olmadigi icin tamamen yanlis sonuc alirsin ve
        // "ekranda hicbir sey yok" ile karsilasirsin.
        m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
    }
}
