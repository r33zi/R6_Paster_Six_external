#pragma once

//
// DamageComponent — R6 class definition (recovered from RTTI / vtable dump).
//
// Inheritance:  Component(0x28) -> ManagedObject(0x18) -> Object(0x8)
//   m_ClassID    : 0xDB261F38 | 0x3FF6CF4B   // was 0x1F899234
//   m_ClassSize  : 0x4C0 | 1216             // was 0x4E0
//   m_ClassFlags : cf_NoInit
//
// Descriptor:        RainbowSix.exe+0x10CC4D38
// PropertyAddress:   RainbowSix.exe+0x10952550
// Constructor:       RainbowSix.exe+0x43CBA00
// StaticCallBridge:  RainbowSix.exe+0x3DD9960
// Destruct:          RainbowSix.exe+0x413EB50
// ReleaseResources:  RainbowSix.exe+0x5CFC0
//
// vtable[0]  Initialize              args=4
// vtable[1]  VirtualFn_3C94680      args=4
// vtable[2]  ReleaseResources        args=1
// vtable[3]  VirtualFn_72620         args=1
// vtable[4]  VirtualFn_72620         args=1
// vtable[5]  VirtualFn_72620         args=1
// vtable[6]  Constructor             args=4
// vtable[7]  VirtualFn_5CFC0         args=1
// vtable[9]  Destruct                args=4
// vtable[10] StaticCallBridge         args=4   accesses[this+0x110]
// vtable[13] VirtualFn_6286D0         args=1   accesses[this+0x28]
// vtable[19] VirtualFn_3EDB290        args=4   accesses[this+0x110]
// vtable[30] VirtualFn_452A1E0        args=3
// vtable[31] VirtualFn_3DCE260        args=4
// vtable[37] VirtualFn_4019960        args=4   accesses[this+0x3D8,0x3E0,0x3E8]
// vtable[39] VirtualFn_26286C0        args=4   accesses[this+0x20]
// vtable[46] VirtualFn_3F3E0E0        args=4   accesses[this+0x378]
// vtable[48] VirtualFn_45407C0        args=4
// vtable[54] VirtualFn_3DF7A60        args=4   accesses[this+0x358]
// vtable[63] VirtualFn_44B0F20        args=3
//
// Health data lives behind this component (see r6_health.h):
//   entity -> component list -> DamageComp
//     -> entry tagged 0x183 at DamageComp + (i*8)
//       -> +0xE0 mid
//         -> +0x38 hpData
//           -> int32 hp at hpData + (0..0xC8)*4 (typically 1..150 for live players)
//
// This header is a reference document. The live reader is r6hp::ReadEntityHealth
// (r6_health.h), which uses these offsets directly.
//

#include <cstdint>

namespace r6dmg {

constexpr uint32_t kClassID_Primary   = 0xDB261F38;
constexpr uint32_t kClassID_Secondary = 0x3FF6CF4B;
constexpr uint32_t kClassSize          = 0x4C0;

// Component offsets (members 0x88..0x1B8).
constexpr uint64_t kOff_m_ApplyDamageFX                    = 0x88;
constexpr uint64_t kOff_m_ApplyDamageEvents                 = 0x90;  // RT confirmed
constexpr uint64_t kOff_m_SoundDamageParameters             = 0xA0;
constexpr uint64_t kOff_m_ArmorData                        = 0xA8;
constexpr uint64_t kOff_m_DamageEvents                     = 0xC8;  // RT confirmed
constexpr uint64_t kOff_m_InstigatorDamageEvents           = 0x128; // RT confirmed
constexpr uint64_t kOff_m_DamageData                       = 0x138;
constexpr uint64_t kOff_m_InvincibilityType                = 0x168;
constexpr uint64_t kOff_m_Float_8                          = 0x174;
constexpr uint64_t kOff_m_ResetHealthOnActivation          = 0x1B3;
constexpr uint64_t kOff_m_IgnoreFriendlyFireDamageModifier = 0x1B4;
constexpr uint64_t kOff_m_AlwaysApplyFeedbacks             = 0x1B5;
constexpr uint64_t kOff_m_Bool_12                         = 0x1B8; // RT confirmed

// Health object tag — the entry inside DamageComp whose 16-bit tag at
// (entry - 8) == 0x183 is the live health object.
constexpr uint16_t kHealthObjTag = 0x183;

// Health indirection offsets.
constexpr uint64_t kHealthObj_MidOff   = 0xE0; // healthObj + 0xE0 -> mid
constexpr uint64_t kMid_HpDataOff     = 0x38; // mid + 0x38 -> hpData
constexpr uint64_t kHpData_ScanMax   = 0xC8; // hpData scanned up to 0xC8 int32 slots

// CallBridge function names (for reference; not invoked externally).
//   SetExtraHealthThisPlayerDebugCommand  0x69FD7337
//   GodModeThisPlayerDebugCommand          0x73D4951E
//   DebugHitResponseDebugCommand           0x78BAFD1F
//   DisplayDamageOnConsoleInfoDebugCommand 0x7C41C40E
//   ArmorRatingDebugCommand               0xA8E71559
//   ResetHealthThisPlayerDebugCommand     0xE1C139B4
//   RemoveExtraHealthThisPlayerDebugCommand 0xEE114C51

enum class HealthCalculationMode : uint32_t {
    FullHealth = 0x0,
    NoDBNO    = 0x1,
};

} // namespace r6dmg