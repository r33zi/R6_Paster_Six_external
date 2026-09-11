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
#include <array>
#include <unordered_map>
#include <mutex>

#include "driver.h"
#include "offsets.h"

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
struct HealthCacheEntry {
    uint64_t component = 0;
    uint64_t healthObject = 0;
};

inline std::unordered_map<uint64_t, HealthCacheEntry> s_healthEntityMap;
inline std::unordered_map<uint64_t, uint64_t> s_healthRetryAfter;
inline uint64_t s_componentArrayOffsetCache = 0;
inline int      s_healthObjectFieldOffset   = -1;
inline int64_t  s_healthDataOffset          = -1;

static inline bool HasHealthTag(uint64_t object) {
    if (!IsVPtr(object)) return false;
    uint16_t tag = 0;
    return SR(object - sizeof(uint64_t), &tag, sizeof(tag)) &&
        tag == OFFSETS::HealthObjectTag;
}

// Match the DamageComponent by its current structural signature: a normal
// component vtable plus a field pointing to the object tagged 0x183. Reading
// the component in one block avoids hundreds of driver round trips.
static inline uint64_t FindHealthObject(uint64_t component) {
    if (!IsVPtr(component)) return 0;
    uint64_t vtable = 0;
    if (!SR(component, &vtable, sizeof(vtable)) || !IsVPtr(vtable)) return 0;

    if (s_healthObjectFieldOffset >= 0) {
        uint64_t object = 0;
        if (SR(component + (uint64_t)s_healthObjectFieldOffset, &object, sizeof(object)) &&
            HasHealthTag(object))
            return object;
        // A miss usually means this is another component in the same array,
        // not that the shared DamageComponent field offset became stale.
    }

    static constexpr size_t kFieldCount = OFFSETS::DamageComponentSize / sizeof(uint64_t);
    std::array<uint64_t, kFieldCount> fields{};
    if (!SR(component, fields.data(), sizeof(fields))) return 0;

    // Skip the Object/ManagedObject base and stay within the recovered class.
    for (size_t i = 0x28 / sizeof(uint64_t); i < fields.size(); ++i) {
        if (!HasHealthTag(fields[i])) continue;
        s_healthObjectFieldOffset = (int)(i * sizeof(uint64_t));
        return fields[i];
    }
    return 0;
}

static inline HealthCacheEntry FindInComponentArray(uint64_t entity, uint64_t arrayOffset) {
    if (!arrayOffset) return {};
    uint64_t list = 0;
    if (!SR(entity + arrayOffset, &list, sizeof(list)) || !IsVPtr(list)) return {};

    static constexpr size_t kMaxComponents = 100;
    std::array<uint64_t, kMaxComponents> components{};
    if (!SR(list, components.data(), sizeof(components))) {
        // Some arrays end on an unreadable page. Preserve the bounded scan in
        // that case instead of rejecting an otherwise valid component list.
        for (size_t i = 0; i < components.size(); ++i)
            SR(list + i * sizeof(uint64_t), &components[i], sizeof(uint64_t));
    }

    for (uint64_t component : components) {
        uint64_t healthObject = FindHealthObject(component);
        if (healthObject) return { component, healthObject };
    }
    return {};
}

// Locate the DamageComponent through the entity's component array. The old
// reader treated entity+offset itself as a component, producing false matches.
static inline HealthCacheEntry FindHealthComponent(uint64_t entity, uint64_t preferredArrayOffset) {
    if (!IsVPtr(entity)) return {};
    const uint64_t now = GetTickCount64();
    {
        std::lock_guard<std::mutex> lk(s_healthEntityMapMtx);
        auto it = s_healthEntityMap.find(entity);
        if (it != s_healthEntityMap.end()) {
            if (HasHealthTag(it->second.healthObject)) return it->second;
            s_healthEntityMap.erase(it);
        }
        auto retry = s_healthRetryAfter.find(entity);
        if (retry != s_healthRetryAfter.end() && now < retry->second) return {};
    }

    const uint64_t candidates[] = {
        preferredArrayOffset, s_componentArrayOffsetCache,
        0x370, 0xD8, 0xC0, 0xB8, 0xA0
    };
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        const uint64_t off = candidates[i];
        if (!off) continue;
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j)
            if (candidates[j] == off) { duplicate = true; break; }
        if (duplicate) continue;

        HealthCacheEntry found = FindInComponentArray(entity, off);
        if (!found.component) continue;
        s_componentArrayOffsetCache = off;
        std::lock_guard<std::mutex> lk(s_healthEntityMapMtx);
        s_healthRetryAfter.erase(entity);
        s_healthEntityMap[entity] = found;
        return found;
    }
    {
        std::lock_guard<std::mutex> lk(s_healthEntityMapMtx);
        s_healthRetryAfter[entity] = now + 1000;
    }
    return {};
}

static inline void FlushHealthCache() {
    std::lock_guard<std::mutex> lk(s_healthEntityMapMtx);
    s_healthEntityMap.clear();
    s_healthRetryAfter.clear();
    s_healthDataOffset = -1;
}

// Read the live HP integer for an entity. Returns -1 when not found / invalid.
static inline int ReadEntityHealth(uint64_t entity, uint64_t componentArrayOffset = 0) {
    HealthCacheEntry health = FindHealthComponent(entity, componentArrayOffset);
    if (!health.healthObject) return -1;

    // Step 2: healthObj + 0xE0 -> mid; mid + 0x38 -> hpData.
    uint64_t mid = 0;
    if (!SR(health.healthObject + OFFSETS::HealthObjectMidOffset, &mid, sizeof(mid)) || !IsVPtr(mid)) return -1;
    uint64_t hpData = 0;
    if (!SR(mid + OFFSETS::HealthDataOffset, &hpData, sizeof(hpData)) || !IsVPtr(hpData)) return -1;

    // Step 3: the integer HP sits at some int32 slot inside hpData. Try the
    // cached offset first; if it's out of range, scan 0xC8 int32 slots
    // and pick the largest value in [1..150] (R6 player HP range).
    if (s_healthDataOffset >= 0) {
        int32_t val = 0;
        if (SR(hpData + (uint64_t)s_healthDataOffset, &val, sizeof(val)) &&
            val >= 1 && val <= 150)
            return val;
        s_healthDataOffset = -1;
    }

    int bestHp = -1;
    int64_t bestOff = 0;
    static constexpr size_t kHealthSlots = OFFSETS::HealthDataScanBytes / sizeof(int32_t);
    std::array<int32_t, kHealthSlots> values{};
    if (!SR(hpData, values.data(), sizeof(values))) return -1;
    for (size_t j = 0; j < values.size(); j++) {
        int32_t val = values[j];
        if (val >= 1 && val <= 150) {
            if (val > bestHp) { bestHp = val; bestOff = (int64_t)(j * 4); }
        }
    }
    if (bestHp > 0) s_healthDataOffset = bestOff;
    return bestHp;
}

} // namespace r6hp
