#include "FXEngine/Core/Window.h"
#include "FXEngine/Core/Log.h"

// SIRA KRITIK: glad her zaman SDL'in OpenGL header'larindan ONCE gelmeli.
// glad GL fonksiyonlarini kendisi tanimlar; baska bir header once tanimlarsa
// "macro redefinition" ve tip cakismalari alirsin. Bu kural motor boyunca gecerli.
#include <glad/glad.h>
#include <SDL3/SDL.h>

namespace FX
{
    namespace
    {
        // -------------------------------------------------------------------
        // OpenGL debug output
        // -------------------------------------------------------------------
        // Normalde OpenGL hatalari SESSIZCE gecer: yanlis bir cagri yaparsin,
        // ekran siyah kalir, sebebini bilemezsin. glGetError() ile tek tek
        // sorgulamak zahmetlidir ve nerede patladigini gostermez.
        //
        // KHR_debug uzantisi bunu cozer: surucu, bir hata olustugu ANDA bizim
        // fonksiyonumuzu cagirir. Faz 2'den itibaren shader/buffer hatalarini
        // bu sayede saniyeler icinde bulacaksin. Simdi kurmak sonra kurmaktan
        // cok daha kolay - context olusturulurken acilmasi gerekiyor.
        void APIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id,
                                      GLenum severity, GLsizei /*length*/,
                                      const GLchar* message, const void* /*userParam*/)
        {
            // Bu id'ler NVIDIA/Intel surucularinin "bilgi amacli" gurultusu.
            // Susturmazsak her karede ekrani doldururlar.
            if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
                return;

            const char* sourceStr = "Bilinmiyor";
            switch (source)
            {
                case GL_DEBUG_SOURCE_API:             sourceStr = "API";        break;
                case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Pencere";    break;
                case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader";     break;
                case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "3rdParty";   break;
                case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Uygulama";   break;
                default: break;
            }

            const char* typeStr = "Bilinmiyor";
            switch (type)
            {
                case GL_DEBUG_TYPE_ERROR:               typeStr = "HATA";           break;
                case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Kullanimdan kalkmis"; break;
                case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "Tanimsiz davranis";   break;
                case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Tasinabilirlik"; break;
                case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performans";     break;
                default: break;
            }

            // Onem seviyesini kendi log seviyemize esliyoruz.
            switch (severity)
            {
                case GL_DEBUG_SEVERITY_HIGH:
                    FX_CORE_ERROR("[GL/%s/%s] %s", sourceStr, typeStr, message);
                    break;
                case GL_DEBUG_SEVERITY_MEDIUM:
                    FX_CORE_WARN("[GL/%s/%s] %s", sourceStr, typeStr, message);
                    break;
                case GL_DEBUG_SEVERITY_LOW:
                    FX_CORE_WARN("[GL/%s/%s] %s", sourceStr, typeStr, message);
                    break;
                default: // NOTIFICATION
                    FX_CORE_TRACE("[GL/%s/%s] %s", sourceStr, typeStr, message);
                    break;
            }
        }
    }

    Window::Window(const WindowProps& props)
        : m_Width(props.Width), m_Height(props.Height), m_VSync(props.VSync)
    {
        // -------------------------------------------------------------------
        // 1) SDL'in video alt sistemini baslat
        // -------------------------------------------------------------------
        // SDL_INIT_VIDEO tek basina yeterli; ses/joystick'i simdilik acmiyoruz
        // cunku her alt sistem baslatma suresi ve kaynak demektir.
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            FX_CORE_ERROR("SDL video alt sistemi baslatilamadi: %s", SDL_GetError());
            return;
        }

        // -------------------------------------------------------------------
        // 2) OpenGL context ozelliklerini ISTE
        // -------------------------------------------------------------------
        // DIKKAT: Bu cagrilar pencere OLUSTURULMADAN ONCE yapilmali.
        // SDL bunlari "bir sonraki GL context'i su ozelliklerle yarat" diye
        // saklar. Pencereden sonra ayarlarsan hicbir etkisi olmaz - yeni
        // baslayanlarin en sik dustugu tuzaklardan biri budur.
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        // CORE profile: eski (fixed-function) OpenGL API'sini tamamen kapatir.
        // glBegin/glVertex gibi 1990'lardan kalma cagrilar derlenmez bile.
        // Bunu bilerek istiyoruz: modern OpenGL ogrenmek istiyorsak eski yolun
        // kapali olmasi bizi dogru aliskanliklara zorlar.
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        // Cift tamponlama: bir kareyi gorunmeyen tampona ciziyoruz, bitince
        // ekrandakiyle takas ediyoruz. Olmasaydi cizim islemini canli izlerdin
        // (yirtilma / titreme).
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        // Renk ve derinlik tamponu bit derinlikleri.
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

#ifdef FX_DEBUG
        // Debug context: surucuye "bana ayrintili hata bildir" der.
        // Kucuk bir performans maliyeti var, o yuzden sadece Debug'da.
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

        // -------------------------------------------------------------------
        // 3) Pencereyi olustur
        // -------------------------------------------------------------------
        SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
        if (props.Resizable)
            flags |= SDL_WINDOW_RESIZABLE;

        // SDL3'te yuksek DPI ekran destegi: pencere boyutu ile piksel sayisi
        // farkli olabilir (orn. 150% olcekli ekran). Bu bayrak olmadan
        // gorunt bulanik olur.
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

        m_Window = SDL_CreateWindow(props.Title,
                                    static_cast<int>(props.Width),
                                    static_cast<int>(props.Height),
                                    flags);
        if (!m_Window)
        {
            FX_CORE_ERROR("Pencere olusturulamadi: %s", SDL_GetError());
            return;
        }

        // -------------------------------------------------------------------
        // 4) OpenGL context olustur ve etkinlestir
        // -------------------------------------------------------------------
        m_GLContext = SDL_GL_CreateContext(m_Window);
        if (!m_GLContext)
        {
            // En sik sebep: makine GL 3.3 Core desteklemiyor (cok eski GPU
            // veya surucu yuklu degil).
            FX_CORE_ERROR("OpenGL context olusturulamadi: %s", SDL_GetError());
            FX_CORE_ERROR("Makinen OpenGL 3.3 Core destekliyor mu? Ekran karti surucunu guncelle.");
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
            return;
        }

        SDL_GL_MakeCurrent(m_Window, static_cast<SDL_GLContext>(m_GLContext));

        // -------------------------------------------------------------------
        // 5) glad ile GL fonksiyonlarini yukle
        // -------------------------------------------------------------------
        // NEDEN GEREKLI? OpenGL fonksiyonlari isletim sisteminde hazir durmaz;
        // ekran karti surucusunden CALISMA ZAMANINDA adresleri sorulur.
        // glad tam olarak bunu yapar: yuzlerce fonksiyon isaretcisini doldurur.
        //
        // Bu satirdan ONCE hicbir gl* cagrisi yapilamaz - yaparsan null
        // isaretci cagirmis olursun ve program cokerd. Ayrica gecerli bir
        // context ETKIN olmadan da calismaz; sirasi bu yuzden burada.
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
        {
            FX_CORE_ERROR("glad: OpenGL fonksiyonlari yuklenemedi.");
            SDL_GL_DestroyContext(static_cast<SDL_GLContext>(m_GLContext));
            SDL_DestroyWindow(m_Window);
            m_GLContext = nullptr;
            m_Window    = nullptr;
            return;
        }

        // -------------------------------------------------------------------
        // 6) Ne aldigimizi raporla
        // -------------------------------------------------------------------
        // Istedigimiz 3.3 idi ama surucu daha yenisini verebilir (geriye
        // donuk uyumlu oldugu icin sorun degil). Ne aldigimizi bilmek,
        // ileride "bu makinede neden calismiyor" sorusunu cozmeyi kolaylastirir.
        FX_CORE_INFO("OpenGL context olusturuldu:");
        FX_CORE_INFO("  Uretici : %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
        FX_CORE_INFO("  GPU     : %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        FX_CORE_INFO("  Surum   : %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        FX_CORE_INFO("  GLSL    : %s", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

        // Gercekten 3.3+ aldik mi? Surucu sessizce daha dusugunu vermis olabilir.
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        if (major < 3 || (major == 3 && minor < 3))
        {
            FX_CORE_ERROR("OpenGL 3.3 gerekli ama %d.%d alindi.", major, minor);
        }

        // -------------------------------------------------------------------
        // 7) Debug output'u ac (varsa)
        // -------------------------------------------------------------------
#ifdef FX_DEBUG
        // IKI AYRI KONTROL gerekiyor, karistirmak kolay:
        //   1) GLAD_GL_KHR_debug -> uzanti bu MAKINEDE var mi? (calisma zamani)
        //      Derleyebilmek desteklendigi anlamina gelmez; eski bir GPU'da
        //      fonksiyon isaretcisi null kalir ve cagirirsan cokersin.
        //   2) GL_CONTEXT_FLAG_DEBUG_BIT -> surucu bize gercekten DEBUG
        //      context verdi mi? Istedik diye vermek zorunda degil.
        GLint contextFlags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &contextFlags);
        if (GLAD_GL_KHR_debug && (contextFlags & GL_CONTEXT_FLAG_DEBUG_BIT))
        {
            glEnable(GL_DEBUG_OUTPUT);
            // SYNCHRONOUS: hatanin ANINDA, sucu isleyen gl* cagrisiyla ayni
            // yigin izinde bildirilmesini saglar. Yavaslatir ama debug'da
            // "hangi satir bunu yapti" sorusunu dogrudan cevaplar.
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(GLDebugCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            FX_CORE_INFO("  Debug   : OpenGL debug output AKTIF");
        }
        else
        {
            FX_CORE_WARN("  Debug   : OpenGL debug output yok (KHR_debug=%d, debug bit=%d)",
                         GLAD_GL_KHR_debug ? 1 : 0,
                         (contextFlags & GL_CONTEXT_FLAG_DEBUG_BIT) ? 1 : 0);
        }
#endif

        // -------------------------------------------------------------------
        // 8) Gercek piksel boyutunu al ve viewport'u kur
        // -------------------------------------------------------------------
        // Yuksek DPI ekranlarda pencere boyutu (1280) ile piksel sayisi (2560)
        // farkli olabilir. OpenGL PIKSEL ile calisir, o yuzden pixel boyutunu
        // sormaliyiz. Bunu karistirmak "goruntunun ekranin sadece ceyregine
        // ciziliyor olmasi" seklindeki klasik hatanin sebebidir.
        int pw = 0, ph = 0;
        SDL_GetWindowSizeInPixels(m_Window, &pw, &ph);
        m_Width  = static_cast<std::uint32_t>(pw);
        m_Height = static_cast<std::uint32_t>(ph);
        glViewport(0, 0, pw, ph);

        SetVSync(m_VSync);

        FX_CORE_INFO("Pencere hazir: %s (%dx%d piksel, VSync %s)",
                     props.Title, pw, ph, m_VSync ? "acik" : "kapali");
    }

    Window::~Window()
    {
        // TERS SIRADA yikiyoruz: once context, sonra pencere.
        // Sebep: context pencereye bagli. Pencereyi once yok edersen
        // context sahipsiz kalir - bazi suruculerde cokme, bazilarinda
        // sessiz kaynak sizintisi.
        if (m_GLContext)
        {
            SDL_GL_DestroyContext(static_cast<SDL_GLContext>(m_GLContext));
            m_GLContext = nullptr;
        }
        if (m_Window)
        {
            SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        FX_CORE_INFO("Pencere kapatildi.");
    }

    void Window::SwapBuffers()
    {
        SDL_GL_SwapWindow(m_Window);
    }

    void Window::SetVSync(bool enabled)
    {
        // 1 = her ekran yenilemesinde bir kare (~60 FPS)
        // 0 = sinir yok, GPU ne kadar hizliysa o kadar
        if (!SDL_GL_SetSwapInterval(enabled ? 1 : 0))
            FX_CORE_WARN("VSync ayarlanamadi: %s", SDL_GetError());
        m_VSync = enabled;
    }

    void Window::OnResize(std::uint32_t width, std::uint32_t height)
    {
        m_Width  = width;
        m_Height = height;

        // glViewport, OpenGL'e "cizdigin -1..+1 aralikli koordinatlari
        // ekranin HANGI dikdortgenine esle" der. Pencere buyudugunde bunu
        // guncellemezsen goruntu eski boyutta kalir.
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
}
