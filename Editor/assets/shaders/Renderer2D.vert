#version 330 core

// ===========================================================================
// BATCH RENDERER - vertex shader
//
// Faz 3'ten farki: u_Transform UNIFORM'U YOK.
// Cunku tek draw call'da binlerce farkli quad var; hepsi ayni uniform'u
// paylasamaz. Pozisyonlar CPU tarafinda zaten dunya koordinatina cevrildi.
// ===========================================================================

layout(location = 0) in vec3  a_Position;      // DUNYA koordinatinda
layout(location = 1) in vec4  a_Color;
layout(location = 2) in vec2  a_TexCoord;
layout(location = 3) in float a_TexIndex;      // hangi texture slotu
layout(location = 4) in float a_TilingFactor;

// Geriye kalan TEK uniform: kamera. O da tum sahne icin ortak.
uniform mat4 u_ViewProjection;

out vec4  v_Color;
out vec2  v_TexCoord;
// "flat" = INTERPOLE ETME, ilk kosenin degerini aynen kullan.
//
// Neden onemli? TexIndex bir slot NUMARASIDIR, bir olcum degil.
// Interpole edilirse iki kose arasinda 1.5 gibi anlamsiz degerler olusur
// ve yanlis texture okunur. flat, bu tur "kategorik" degerler icin sarttir.
flat out float v_TexIndex;
out float v_TilingFactor;

void main()
{
    v_Color        = a_Color;
    v_TexCoord     = a_TexCoord;
    v_TexIndex     = a_TexIndex;
    v_TilingFactor = a_TilingFactor;

    // Transform yok - pozisyon zaten dunya uzayinda.
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
