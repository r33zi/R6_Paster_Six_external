#pragma once

#include <cstdint>

namespace OFFSETS
{
    // RainbowSix.exe dump published 2026-09-03T21:51:38.106Z.
    // Addresses in the dump were absolute; only the rebased RVAs are used at
    // runtime so ASLR does not make a captured address point at the wrong data.
    static constexpr const char* Build = "latest";
    static constexpr const char* Updated = "2026-09-03T21:51:38.106Z";
    static constexpr const char* Source = "https://nohwid.win/offsets/api/r6";
    static constexpr const wchar_t* Module = L"RainbowSix.exe";

    static constexpr uintptr_t DumpGameBase = 0x7FF66C070000ULL;
    static constexpr uintptr_t ActorPatchRva = 0x00CFCE5B;
    static constexpr uintptr_t CameraPatchRva = 0x0E6A4795;
    static constexpr uintptr_t CodeCaveOneRva = 0x10D73294;
    static constexpr uintptr_t CodeCaveTwoRva = 0x10D78DF4;
    static constexpr uintptr_t ActorTrampolineRva = 0x000080D2;
    static constexpr uintptr_t ActorMovRva = 0x00CFCE57;
    static constexpr uintptr_t CameraMovRva = 0x0E6A4779;
    static constexpr uintptr_t CameraTrampolineRva = 0x00052192;
    static constexpr uintptr_t ViewMatrixRva = 0x11FB8EF0;

    static constexpr const char* ActorCallerSignature =
        "65 ? 8B ? 25 58 00 00 00 ? 8B ? ? ? 8D ? ? ? ? 00 ? C1 ? 03";
    static constexpr const char* CameraSignature =
        "C7 44 24 28 00 08 00 00 4C 89";
    static constexpr const char* CameraSignatureFallback =
        "C7 44 24 28 00 08 00 00";
    static constexpr const char* ViewMatrixSignature =
        "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 90 48 83 C4 58 41 5D 41 5C 41 5F 5E 5B 5F 5D";
    static constexpr const char* GameManagerSignature =
        "48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 84 ?? ?? ?? ?? 48 8B 01 FF 90";
    static constexpr const char* InGameFlagSignature =
        "F6 C1 07 45 0F B6 ?? 45 0F 44 ?? 41 83 E2 01 "
        "44 89 15 ?? ?? ?? ??";
    static constexpr uint32_t InGameFlagStoreOffset = 0x0F;

    // Position-record anchor. The three coordinates are the 12 wildcard
    // bytes beginning at +0x10; the supplied mask was shorter than the
    // 44-byte pattern, so the known 16-byte suffix is retained explicitly.
    static constexpr const char* PlayerPositionSignature =
        "00 00 00 00 00 00 00 00 00 00 80 3F 00 00 00 00 "
        "?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? "
        "00 00 00 00 9D 99 99 3E 00 00 00 00 00 00 00 00";
    static constexpr uint32_t PlayerPositionXOffset = 0x10;
    static constexpr uint32_t PlayerPositionYOffset = 0x14;
    static constexpr uint32_t PlayerPositionZOffset = 0x18;
    static constexpr uint32_t PlayerPositionSignatureSize = 0x2C;
    static constexpr const char* ViewAnchorSignature =
        "A4 70 7D BF 00 00 00 00 00 00 00 00 00 00 A0 40 "
        "00 00 A0 C0 00 00 00 00 00 00 00 00 CD CC 4C 3F "
        "00 00 00 3F 00 00 80 3E";

    // Bone-entry stores from the current AnvilNext layout. A store at +0x38
    // is the Z component of the vec3 beginning at +0x30; +0x3C is the
    // homogeneous W value. It must never be interpreted as a vec3 at +0x38.
    static constexpr const char* BoneZStoreSignature =
        "? 89 ? ? 38 ? C7 ? ? 3C 00 00 80 3F";
    static constexpr const char* BoneZStoreSignatureCompact =
        "89 ? ? 38 C7 ? ? 3C 00 00 80 3F";
    static constexpr const char* BoneYStoreSignature =
        "? 89 ? ? 34 ? C7 ? ? 3C 00 00 80 3F";
    static constexpr const char* BoneYStoreSignatureCompact =
        "89 ? ? 34 C7 ? ? 3C 00 00 80 3F";
    static constexpr const char* BoneXStoreSignature =
        "? 89 ? ? 30 ? C7 ? ? 3C 00 00 80 3F";
    static constexpr const char* BoneXStoreSignatureCompact =
        "89 ? ? 30 C7 ? ? 3C 00 00 80 3F";

    // DamageComponent/health structural signatures recovered for this build.
    // These are identifiers and indirection offsets, not module RVAs.
    static constexpr uint32_t DamageComponentClassId = 0xDB261F38;
    static constexpr uint32_t DamageComponentClassIdAlt = 0x3FF6CF4B;
    static constexpr uint32_t DamageComponentSize = 0x4C0;
    static constexpr uint16_t HealthObjectTag = 0x0183;
    static constexpr uintptr_t HealthObjectMidOffset = 0xE0;
    static constexpr uintptr_t HealthDataOffset = 0x38;
    static constexpr uint32_t HealthDataScanBytes = 0x320;

    // Runtime-resolved addresses of pointer variables (not singleton values).
    static uintptr_t pGameManagerPtr = 0;
    static uintptr_t pViewDataPtr = 0;
    static uintptr_t pCameraManagerPtr = 0;
    static uintptr_t inGameFlagAddress = 0;
}
