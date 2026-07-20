#include "FXEngine/Core/Version.h"

namespace FX
{
    const char* GetEngineVersion()
    {
        // String literal'in omru program boyuncadir (static storage), bu yuzden
        // adresini disari dondurmek guvenlidir.
        return "FXEngine 0.1.0 (Faz 0)";
    }
}
