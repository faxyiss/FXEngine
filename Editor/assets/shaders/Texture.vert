#version 330 core

// ===========================================================================
// VERTEX SHADER - texture'li quad
// ===========================================================================

// Attribute'lar: kose BASINA degisen veriler.
// location numaralari, C++'ta BufferLayout'a ekleme sirasiyla eslesir.
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

// Kameranin Projection * View carpimi. Tum nesneler icin ayni -> uniform.
uniform mat4 u_ViewProjection;

// Bu nesnenin dunyadaki yeri/olcegi/donmesi.
uniform mat4 u_Transform;

// "out" = fragment shader'a gonderilen deger.
// ONEMLI: Bu deger koseler arasinda OTOMATIK INTERPOLE edilir.
// Ucgenin ortasindaki bir piksel, uc kosenin degerlerinin agirlikli
// ortalamasini alir. Texture'in yayilmasini saglayan sey tam olarak budur -
// biz sadece 4 koseye UV veriyoruz, aradaki milyonlarca pikseli GPU dolduruyor.
out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;

    // MATRIS CARPIM SIRASI SAGDAN SOLA OKUNUR:
    //   once Transform (yerel -> dunya)
    //   sonra ViewProjection (dunya -> clip)
    // Ters yazarsan derlenir ama goruntu tamamen bozulur.
    gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}
