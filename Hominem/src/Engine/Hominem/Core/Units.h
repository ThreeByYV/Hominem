#pragma once

namespace Hominem {

    // Unit convention: 1 engine unit = 1 metre (SI)
    //
    //   Distance  : 1.0  unit  = 1 m
    //   Mass      : 1.0  unit  = 1 kg
    //   Time      : 1.0  unit  = 1 s
    //   Gravity   : 9.81 units = 9.81 m/s²
    //
    // Quick reference
    //   Average person (5'9" / 175 cm)  ≈ 1.75 units tall
    //   Floor tile (1 ft)               ≈ 0.30 units
    //   Standard door (6'8")            ≈ 2.03 units tall
    //   Car length                      ≈ 4.50 units
    //

    inline constexpr float k_Gravity           =  9.81f;   // m/s²
    inline constexpr float k_MetresPerFoot     =  0.3048f;
    inline constexpr float k_MetresPerInch     =  0.0254f;
    inline constexpr float k_BlenderFBXToMetres = 0.01f;  // Blender FBX cm → m

    // Conversion helpers — readable at call sites
    constexpr float Metres(float m)        { return m; }
    constexpr float Centimetres(float cm)  { return cm  * 0.01f; }
    constexpr float Feet(float ft)         { return ft  * k_MetresPerFoot; }
    constexpr float Inches(float in)       { return in  * k_MetresPerInch; }
    constexpr float FeetInches(int ft, float in) { return Feet((float)ft) + Inches(in); }

}
