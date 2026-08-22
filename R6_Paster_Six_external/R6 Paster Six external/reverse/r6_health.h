#pragma once

//
// R6 health component resolver.
//
// DamageComponent inherits Component -> ManagedObject -> Object.
//   m_ClassID : 0xDB261F38 | 0x3FF6CF4B  (was 0x1F899234)
//   m_ClassSize: 0x4C0 | 1216            (was 0x4E0)
// Members (offsets 0x88..0x1B8):
//   +0x88  m_ApplyDamageFX           (Reference<FX>)
//   +0x90  m_ApplyDamageEvents        (ArrayPtr<EventSeed>)        RT confirmed
//   +0xA0  m_SoundDamageParameters    (Reference<SoundDamageParameters>)
//   +0xA8  m_ArmorData                (Reference<ArmorData>)
//   +0xC8  m_DamageEvents             (ArrayPtr<DamageTypeEventCollection>) RT confirmed
//   +0x128 m_InstigatorDamageEvents   (ArrayPtr<DamageTypeEventCollection>) RT confirmed
//   +0x138 m_DamageData               (Reference<DamageData>)
//   +0x168 m_InvincibilityType        (Enum)
//   +0x174 m_Float_8                  (Float)
//   +0x1B3 m_ResetHealthOnActivation  (Bool)
//   +0x1B4 m_IgnoreFriendlyFireDamageModifier (Bool)
//   +0x1B5 m_AlwaysApplyFeedbacks     (Bool)
//   +0x1B8 m_Bool_12                  (Bool)                       RT confirmed
//
// Health lives behind DamageComponent (entity -> component list -> DamageComp ->
// health object tagged 0x183 -> +0xE0 mid -> +0x38 hpData -> hpData[j] = int32 hp).
// All access is driver-backed (external process read). No in-process pointers.
//

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <mutex>

#include "driver.h"

struct vec3 { float x, y, z; };

// SEH in MSVC for x64 is only supported by the compiler on native types — we
// keep the __try/__except wrappers but route the actual reads through the
// driver so a bad remote pointer returns zero instead of faulting our process.

static inline bool IsVPtr(uint64_t p) {
    return p > 0x10000ULL && p < 0x7FFFFFFFFFFFULL;
}

static inline bool SR(uint64_t addr, void* out, size_t sz) {
    return driver->ReadProcessMemory(addr, out, (uint32_t)sz) == 0;
}

static inline void r6printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
}

namespace r6hp {

inline std::mutex s_healthEntityMapMtx;
inline std::unordered_map<uint64_t, uint64_t> s_healthEntityMap;
inline uint64_t s_healthCompOffsetCache = 0;
inline int      s_typeTag183Off          = -1;
inline int64_t  s_healthDataOffset      = 0;

// Validate a DamageComponent-shaped pointer: the first 5 qwords should each be
// a valid pointer (component vtable + base fields).
static inline bool ValidateHealthComponent(uint64_t ptr) {
    if (!IsVPtr(ptr)) return false;
    for (int i = 0; i < 5; i++) {
        uint64_t val = 0;
        if (!SR(ptr + (uint64_t)i * 8, &val, sizeof(val))) return false;
        if (!IsVPtr(val)) return false;
    }
    return true;
}

static inline uint64_t FindHealthComponentSEH_Cached(uint64_t cached) {
    if (IsVPtr(cached)) {
        uint64_t v = 0;
        if (SR(cached, &v, sizeof(v)) && IsVPtr(v))
            return cached;
    }
    return 0;
}

static inline uint64_t FindHealthComponentSEH_Scan(uint64_t entity, uint64_t& outOff) {
    if (s_healthCompOffsetCache) {
        uint64_t comp = 0;
        if (SR(entity + s_healthCompOffsetCache, &comp, sizeof(comp)) &&
            IsVPtr(comp) && ValidateHealthComponent(comp))
        {
            outOff = s_healthCompOffsetCache;
            return comp;
        }
    }
    // DamageComponent sits in the entity's component array. The array pointer
    // has historically been at 0xA0..0xFC; widen the sweep to 0xA0..0x200 so we
    // survive layout drift between builds.
    for (uint64_t off = 0xA0; off <= 0x200; off += 8) {
        uint64_t comp = 0;
        if (!SR(entity + off, &comp, sizeof(comp))) continue;
        if (!IsVPtr(comp)) continue;
        if (ValidateHealthComponent(comp)) {
            outOff = off;
            return comp;
        }
    }
    return 0;
}

// Locate the DamageComponent for an entity. Cached per-entity; falls back to a
// component-array scan on miss. Returns 0 on failure.
static inline uint64_t FindHealthComponent(uint64_t entity) {
    if (!IsVPtr(entity)) return 0;
    {
        std::lock_guard<std::mutex> lk(s_healthEntityMapMtx);
        auto it = s_healthEntityMap.find(entity);
        if (it != s_healthEntityMap.end()) {
            uint64_t valid = FindHealthComponentSEH_Cached(it->second);
            if (valid) return valid;
            s_healthEntityMap.erase(it);
        }
    }
    uint64_t foundOff = 0;
    uint64_t comp = FindHealthComponentSEH_Scan(entity, foundOff);
    if (comp) {
        s_healthCompOffsetCache = foundOff;
        std::lock_guard<std::mutex> lk(s_healthEntityMapMtx);
        s_healthEntityMap[entity] = comp;
    }
    return comp;
}

// Read the live HP integer for an entity. Returns -1 when not found / invalid.
static inline int ReadEntityHealth(uint64_t entity) {
    uint64_t healthComp = FindHealthComponent(entity);
    if (!healthComp) return -1;

    // Step 1: find the health object — an entry inside the DamageComponent
    // whose 16-bit tag (at entry - 8) equals 0x183.
    uint64_t healthObj = 0;
    if (s_typeTag183Off >= 0) {
        uint64_t entry = 0;
        if (SR(healthComp + (uint64_t)s_typeTag183Off * 8, &entry, sizeof(entry)) &&
            IsVPtr(entry))
        {
            uint16_t typeTag = 0;
            if (SR(entry - 8, &typeTag, sizeof(typeTag)) && typeTag == 0x183)
                healthObj = entry;
        }
        if (!healthObj) s_typeTag183Off = -1;
    }
    if (!healthObj) {
        // Scan up to 0x190 qwords looking for an entry tagged 0x183.
        for (int i = 0; i < 0x190; i++) {
            uint64_t entry = 0;
            if (!SR(healthComp + (uint64_t)i * 8, &entry, sizeof(entry))) continue;
            if (!IsVPtr(entry)) continue;
            uint16_t typeTag = 0;
            if (!SR(entry - 8, &typeTag, sizeof(typeTag))) continue;
            if (typeTag == 0x183) {
                healthObj = entry;
                s_typeTag183Off = i;
                break;
            }
        }
    }
    if (!healthObj) return -1;

    // Step 2: healthObj + 0xE0 -> mid; mid + 0x38 -> hpData.
    uint64_t mid = 0;
    if (!SR(healthObj + 0xE0, &mid, sizeof(mid)) || !IsVPtr(mid)) return -1;
    uint64_t hpData = 0;
    if (!SR(mid + 0x38, &hpData, sizeof(hpData)) || !IsVPtr(hpData)) return -1;

    // Step 3: the integer HP sits at some int32 slot inside hpData. Try the
    // cached offset first; if it's out of range, brute-force 0xC8 int32 slots
    // and pick the largest value in [1..150] (R6 player HP range).
    if (s_healthDataOffset) {
        int32_t val = 0;
        if (SR(hpData + (uint64_t)s_healthDataOffset, &val, sizeof(val)) &&
            val >= 1 && val <= 150)
            return val;
        s_healthDataOffset = 0;
    }

    int bestHp = -1;
    int64_t bestOff = 0;
    for (uint64_t j = 0; j < 0xC8; j++) {
        int32_t val = 0;
        if (!SR(hpData + j * 4, &val, sizeof(val))) continue;
        if (val >= 1 && val <= 150) {
            if (val > bestHp) { bestHp = val; bestOff = (int64_t)(j * 4); }
        }
    }
    if (bestHp > 0 && bestOff) s_healthDataOffset = bestOff;
    return bestHp;
}

} // namespace r6hp