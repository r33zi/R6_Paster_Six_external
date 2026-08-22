#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
struct Vec3 { float x, y, z; };

#include "driver.h"
#include "r6_scanner.h"
#include "Imgui/imgui.h"
#include "skeleton_emu.h"
#include "antitamper.h"
#include "operator_esp.h"
#include "r6_health.h"
struct Matrix4x4 { float m[16]; };
enum class ActorStatus { VALID, DEAD_1, DEAD_2, TEAM, LOCAL, INVALID };

static bool IsValidAddr(uint64_t p){return p>0x10000ULL&&p<0x7FFFFFFFFFFFULL;}

static bool ValidateWorldCoord(Vec3 v){
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
        return false;
    if(fabsf(v.x)>=500000.f||fabsf(v.y)>=500000.f||fabsf(v.z)>=500000.f)
        return false;
    int s=(fabsf(v.x)>2.f?1:0)+(fabsf(v.y)>2.f?1:0)+(fabsf(v.z)>2.f?1:0);
    return s>=2;
}

// ============================================================================
// PhysicsWorld position + rotation quaternion resolvers.
// The character component carries a live world position (the PhysicsWorld
// transform) and a unit rotation quaternion. Both offsets drift between
// builds, so we auto-discover them once from a known-good char comp and
// cache the offsets globally. These must be defined BEFORE r6_bones.h
// because the skeleton paths call GetEntityRotQuat / GetPhysWorldPos /
// ResolveAnchor to orient and anchor the bones.
// ============================================================================

static uint32_t s_physWorldOff = 0;
static bool     s_physWorldFound = false;

static bool FindPhysWorldOffset(uint64_t charComp) {
    if (s_physWorldFound) return true;
    if (!IsValidAddr(charComp)) return false;

    FILE* df = fopen("C:\\r6_bones_diag.log", "a");

    // Reference: live position at +0xB00 on the char component.
    vec3 ref{};
    if (!SR(charComp + 0xB00, &ref, sizeof(ref))) {
        if (df) { fprintf(df, "[PHYS-DIAG] +0xB00 read FAIL\n"); fclose(df); }
        return false;
    }
    if (ref.x == 0.0f && ref.y == 0.0f && ref.z == 0.0f) {
        if (df) { fprintf(df, "[PHYS-DIAG] +0xB00 is zero\n"); fclose(df); }
        return false;
    }
    if (df) { fprintf(df, "[PHYS-DIAG] ref(0xB00) = (%.1f,%.1f,%.1f)\n", ref.x, ref.y, ref.z); fclose(df); }

    float bestDist = 3.4028235e38f;
    uint32_t bestOff = 0;
    for (uint32_t off = 0x80; off + 0xC <= 0x2F0; off += 4) {
        vec3 v{};
        if (!SR(charComp + off, &v, sizeof(v))) continue;
        if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) continue;
        float dx = v.x - ref.x, dy = v.y - ref.y, dz = v.z - ref.z;
        float dist = dx * dx + dy * dy + dz * dz;
        if (dist < bestDist) { bestDist = dist; bestOff = off; }
    }
    if (bestOff) {
        vec3 best{};
        SR(charComp + bestOff, &best, sizeof(best));
        s_physWorldOff = bestOff;
        s_physWorldFound = true;
        df = fopen("C:\\r6_bones_diag.log", "a");
        if (df) { fprintf(df, "[PHYS-DIAG] FOUND +0x%X = (%.1f,%.1f,%.1f) dist=%.2f\n", bestOff, best.x, best.y, best.z, sqrtf(bestDist)); fclose(df); }
        r6printf("[PHYS] PhysicsWorld offset: +0x%X (dist=%.2f)\n", bestOff, sqrtf(bestDist));
        return true;
    }
    df = fopen("C:\\r6_bones_diag.log", "a");
    if (df) { fprintf(df, "[PHYS-DIAG] NO MATCH found in 0x80..0x2F0\n"); fclose(df); }
    return false;
}

static uint32_t s_rotQuatOff = 0;
static bool     s_rotQuatFound = false;

static bool FindRotQuatOffset(uint64_t charComp) {
    if (s_rotQuatFound) return true;
    if (!IsValidAddr(charComp)) return false;

    FILE* df = fopen("C:\\r6_bones_diag.log", "a");
    if (df) { fprintf(df, "[QUAT-DIAG] scanning comp=0x%llX for rotation quaternion\n", (unsigned long long)charComp); fclose(df); }

    // Candidate offsets observed across builds.
    static const uint32_t candidates[] = { 0x660, 0x650, 0x670, 0x640, 0x680 };
    for (uint32_t off : candidates) {
        float q[4];
        if (!SR(charComp + off, q, 16)) {
            df = fopen("C:\\r6_bones_diag.log", "a"); if (df) { fprintf(df, "[QUAT-DIAG] +0x%X read FAIL\n", off); fclose(df); }
            continue;
        }
        df = fopen("C:\\r6_bones_diag.log", "a");
        if (df) {
            fprintf(df, "[QUAT-DIAG] +0x%X = (%.3f,%.3f,%.3f,%.3f)", off, q[0], q[1], q[2], q[3]);
            bool bad = false;
            for (int i = 0; i < 4; i++) {
                if (!std::isfinite(q[i]) || fabsf(q[i]) > 10.0f) { bad = true; break; }
            }
            if (bad) { fprintf(df, " REJECT (non-finite or >10)\n"); fclose(df); continue; }
            float mag2 = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
            fprintf(df, " mag2=%.3f", mag2);
            if (mag2 < 0.95f || mag2 > 1.05f) { fprintf(df, " REJECT (mag2 out of range)\n"); fclose(df); continue; }
            float mx = 0;
            for (int i = 0; i < 4; i++) mx = (fabsf(q[i]) > mx) ? fabsf(q[i]) : mx;
            if (mx < 0.1f) { fprintf(df, " REJECT (all <0.1)\n"); fclose(df); continue; }
            fprintf(df, " ACCEPT\n");
            fclose(df);
        }
        s_rotQuatOff = off;
        s_rotQuatFound = true;
        r6printf("[QUAT] Rotation quaternion at +0x%X (%.3f,%.3f,%.3f,%.3f)\n",
                       off, q[0], q[1], q[2], q[3]);
        return true;
    }
    df = fopen("C:\\r6_bones_diag.log", "a"); if (df) { fprintf(df, "[QUAT-DIAG] NO CANDIDATE ACCEPTED\n"); fclose(df); }
    return false;
}

static bool GetEntityRotQuat(uint64_t charComp, float* qOut) {
    if (!IsValidAddr(charComp)) return false;
    if (!s_rotQuatFound && !FindRotQuatOffset(charComp)) return false;
    if (!SR(charComp + s_rotQuatOff, qOut, 16)) return false;
    float mag2 = qOut[0]*qOut[0] + qOut[1]*qOut[1] + qOut[2]*qOut[2] + qOut[3]*qOut[3];
    if (mag2 < 0.8f || mag2 > 1.2f) return false;
    float invLen = 1.0f / sqrtf(mag2);
    qOut[0] *= invLen; qOut[1] *= invLen; qOut[2] *= invLen; qOut[3] *= invLen;
    return true;
}

static bool GetPhysWorldPos(uint64_t charComp, Vec3& out) {
    if (!IsValidAddr(charComp)) return false;
    if (!s_physWorldFound && !FindPhysWorldOffset(charComp)) return false;
    return SR(charComp + s_physWorldOff, &out, sizeof(out));
}

// Wire the quaternion + physics-world readers into skel::ReadWorldMatrix so
// it builds the world matrix from the authoritative sources (quaternion for
// rotation, PhysicsWorld for translation) instead of the stale +0x240 matrix.
// These function pointers are set before r6_bones.h is included.
static bool skel_readRotQuat_adapter(uint64_t comp, float* qOut) {
    return GetEntityRotQuat(comp, qOut);
}
static bool skel_readPhysPos_adapter(uint64_t comp, void* out) {
    return GetPhysWorldPos(comp, *(Vec3*)out);
}

#include "r6_bones.h"

struct OverlayVertex {
    uint64_t instance;
    Vec3 position;
    ActorStatus status;
    bool isPlayer;
    float distance;
    Vec3 screenPos;
    bool onScreen;
    bool hasBones;
    SkeletonBones bones;
    Vec3 headScreenPos;
    bool headOnScreen;
    char operatorName[32];
    int  hp;
};

struct RenderSyncEntry {
    Vec3 position{};
    Vec3 smoothed_position{};
    uint64_t filter_byte = 0;
    std::chrono::steady_clock::time_point last_seen{};
    std::chrono::steady_clock::time_point position_time{};
    uint16_t comp_index = 0xFFFF;
};

extern bool Aimbot;
extern bool fillbox;
extern float boxThickness;
extern float snaplineThickness;
extern float fovCircleThickness;
extern float crosshairSize;
extern float espBoxColor[4];
extern float espSnaplineColor[4];
extern float espDistanceColor[4];
extern float fovCircleColor[4];
extern float crosshairColor[4];
extern float aimbotTargetColor[4];
extern float filledBoxColor[4];
extern bool rainbowMode;
extern bool rainbowBox;
extern bool rainbowFov;
extern bool rainbowSnaplines;
extern bool Esp_skeleton;
extern bool skeletonAim;
extern float espSkeletonColor[4];
extern float skeletonThickness;

extern ImU32 GetBoxColor(float offset);
extern ImU32 GetSnaplineColor(float offset);
extern ImU32 GetFovColor();
extern ImU32 ColorToU32(const float* col);
extern void aimbot(float x, float y);

static FILE* g_log = nullptr;
static void DBG(const char* fmt, ...) {
    if (!g_log) g_log = fopen("C:\\r6_render_calib.log", "w");
    va_list a;
    if (g_log) { va_start(a,fmt); vfprintf(g_log,fmt,a); fprintf(g_log,"\n"); fflush(g_log); va_end(a); }
    va_start(a,fmt); vprintf(fmt,a); printf("\n"); va_end(a);
}

static uint64_t g_projectionAddr = 0;
static uint64_t g_frameSyncAddr = 0;
static uint64_t g_imageBase = 0;
static uint64_t g_ShellPage = 0;
static uint64_t g_RingAddr = 0;
static bool     g_frameSyncActive = false;
static uint8_t  g_OrigBytes[32] = {};
static int      g_PatchLen = 0;
static size_t   g_ShellSize = 0;
static DWORD    g_frameSyncStart = 0;
static bool     g_syncComplete = false;
static constexpr DWORD COLLECT_MS = 200;
static constexpr DWORD REHOOK_MIN_MS = 8000;
static constexpr DWORD REHOOK_JITTER_MS = 7000;

static std::vector<OverlayVertex> g_vertexBuffer;
static std::mutex g_Mtx;
static int g_vtxCount = 0, g_activeVtx = 0;
static std::unordered_set<uint64_t> g_capturedFrames;
static std::mutex g_frameMtx;
static std::atomic<uint64_t> g_totalFrames{0};
static uint64_t g_ReadIdx = 0;
static constexpr size_t RING_SZ = 256;
static uint64_t g_RoundPtr = 0;
static bool g_RoundFound = false;

static DWORD g_lastEntityUpdate = 0;
static constexpr DWORD ENTITY_UPDATE_INTERVAL = 16;


static std::unordered_map<uint64_t, RenderSyncEntry> g_syncMap;
static std::mutex g_syncMapMtx;
static constexpr auto k_syncMaxAge = std::chrono::seconds(30);
static constexpr float k_viewportHeight = 1.72f;
static constexpr float k_minRenderDist = 0.1f;

static bool DrvProtect(uint64_t a, size_t s, uint32_t p, uint32_t* old) {
    return driver->ProtectMemory(a, s, p, old);
}
static bool DrvWriteRaw(const void* src, uint64_t dst, size_t sz) {
    return driver->WriteProcessMemory((PVOID)src, (PVOID)dst, (DWORD)sz) == 0;
}
static bool DrvWriteExec(const void* src, uint64_t dst, size_t sz) {
    uint32_t old = 0;
    DrvProtect(dst, sz, PAGE_EXECUTE_READWRITE, &old);
    bool ok = DrvWriteRaw(src, dst, sz);
    uint32_t tmp = 0;
    DrvProtect(dst, sz, old ? old : PAGE_EXECUTE_READ, &tmp);
    return ok;
}

static int FindBoundary(const uint8_t* c, int minB) {
    int p = 0;
    while (p < 32) {
        uint8_t b = c[p];
        bool rex = (b >= 0x40 && b <= 0x4F);
        if (rex) { p++; b = c[p]; }
        if (b >= 0x50 && b <= 0x5F) { p++; if (p >= minB) return p; continue; }
        if (b == 0x90 || b == 0xCC || b == 0xC3) { p++; if (p >= minB) return p; continue; }
        if (b==0x83||b==0x81||b==0x89||b==0x8B||b==0x8D||b==0x01||b==0x29||
            b==0x31||b==0x33||b==0x39||b==0x3B||b==0x85||b==0x87) {
            uint8_t m = c[p+1]; uint8_t mod = (m>>6)&3; uint8_t rm = m&7;
            p += 2;
            if (mod!=3) { if (rm==4) p++; if (mod==0&&rm==5) p+=4; else if (mod==1) p++; else if (mod==2) p+=4; }
            if (b==0x83) p++; else if (b==0x81) p+=4;
            if (p >= minB) return p; continue;
        }
        if (b == 0x0F) { p++; uint8_t m2 = c[p+1]; uint8_t mod=(m2>>6)&3; uint8_t rm=m2&7;
            p += 2; if (mod!=3) { if(rm==4)p++; if(mod==0&&rm==5)p+=4; else if(mod==1)p++; else if(mod==2)p+=4; }
            if (p >= minB) return p; continue; }
        p++; if (p >= minB) return p;
    }
    return p;
}

static Matrix4x4 QueryProjectionMatrix() { return g_projectionAddr ? read<Matrix4x4>(g_projectionAddr+0x250) : Matrix4x4{}; }
static Vec3 QueryCameraOrigin() { return g_projectionAddr ? read<Vec3>(g_projectionAddr+0x190) : Vec3{}; }
static bool W2S(const Vec3& w, Vec3& s, int W, int H) {
    if (!g_projectionAddr) return false;
    Matrix4x4 v=QueryProjectionMatrix();
    float ww=v.m[3]*w.x+v.m[7]*w.y+v.m[11]*w.z+v.m[15];
    if (ww<0.001f) return false;
    s.x=(W*.5f)*(w.x*v.m[0]+w.y*v.m[4]+w.z*v.m[8]+v.m[12])/ww+W*.5f;
    s.y=-(H*.5f)*(w.x*v.m[1]+w.y*v.m[5]+w.z*v.m[9]+v.m[13])/ww+H*.5f;
    s.z=ww; return s.x>=0&&s.y>=0&&s.x<=W&&s.y<=H;
}

#include "weather_fx.h"

static bool ValidatePtr(uint64_t e){
    if(!IsValidAddr(e))return false;
    if(!IsValidAddr(read<uint64_t>(e)))return false;
    int id=read<int>(e+0x1C);
    if(id>0&&id<1000)return true;
    id=read<int>(e+0x20);
    if(id>0&&id<1000)return true;
    return id>0&&id<0x10000;
}
static uint64_t ReadStencilBuffer(uint64_t e){if(!IsValidAddr(e))return 0;return read<uint64_t>(e+0xB8);}
static uint8_t StencilByte4(uint64_t fb){return(uint8_t)((fb>>32)&0xFF);}
static uint8_t StencilByte3(uint64_t fb){return(uint8_t)((fb>>24)&0xFF);}
static bool IsActiveViewport(uint64_t e){return((ReadStencilBuffer(e)>>52)&0xFFF)==0x2C8;}
static bool IsActiveStencil(uint64_t fb){return ((fb>>52)&0xFFF)==0x2C8;}
static bool IsClearedStencil(uint64_t fb){
    uint8_t b4=StencilByte4(fb);
    return b4==0x84||b4==0x82||b4==0x80;
}
static bool IsStencilCleared(uint64_t e){
    uint64_t fb=ReadStencilBuffer(e);
    if(IsClearedStencil(fb))return true;
    uint64_t bf=read<uint64_t>(e+0xB0);
    if(((bf>>24)&0xFF)==0xFA)return true;
    return false;
}
static bool IsSharedStencil(uint64_t fb){uint8_t b=StencilByte4(fb);return b==0x00||b==0x02;}
static bool IsSharedViewport(uint64_t e){return IsSharedStencil(ReadStencilBuffer(e));}
static bool ValidateDepthStencil(uint64_t fb){
    float fmask = _rcal_get_filter_mask();
    if (fmask < 0.5f) {
        if (IsClearedStencil(fb)) return false;
        return true;
    }
    if(IsClearedStencil(fb))return false;
    if(StencilByte4(fb)==0x00)return false;
    if(IsSharedStencil(fb))return false;
    return true;
}
static bool ValidateStencilMask(uint64_t e){
    return ValidateDepthStencil(ReadStencilBuffer(e));
}

static uint64_t s_shaderArrayOff = 0;

static void FlushShaderCache() {
    s_shaderArrayOff = 0;
}


//
// Encrypted actor-position resolver — Ubisoft moved/encrypted the transform
// pointer in the current build. Each qword in the 4-level chain is packed
// with a 16-bit key in bits [16:31] and 16 junk bits in [48:63].
// Decode: masked = P & 0x0000_FFFF_FFFF_FFFF, key = HIWORD(P) = (P>>16)&0xFFFF,
//         real  = masked XOR (0x0001_0001_0001 * key).
// The key spreads across bits [0:15]/[16:31]/[32:47]; XORing with the masked
// P zeroes the encoded slot in [16:31] and unmasks bits [0:15] and [32:47].
// Root offset (0x30 or 0x20) and flag word (0x6E or 0x5E) drift between
// builds — try both. Flag word == 0 means encrypted; else plain vec3 lives
// at Actor + 0x50 or Actor + 0x60 (gadgets / non-player actors).
//
static inline uint64_t DecObfPtr(uint64_t p) {
    return (p & 0x0000FFFFFFFFFFFFULL) ^ (0x0000000100010001ULL * (p >> 48));
}

static std::atomic<int> g_encTraceBudget{0};
static bool TryEncryptedActorPos(uint64_t actor, Vec3& out) {
    if (!IsValidAddr(actor)) return false;

    static const uint64_t k_flagOffs[]  = { 0x6E, 0x5E };
    static const uint64_t k_rootOffs[]  = { 0x30, 0x20 };
    static const uint64_t k_plainOffs[] = { 0x50, 0x60 };

    bool trace = (g_encTraceBudget.load() > 0);
    if (trace) g_encTraceBudget--;

    for (uint64_t flagOff : k_flagOffs) {
        uint16_t flag = read<uint16_t>(actor + flagOff);
        if (trace) printf("[ENC] actor=0x%llX flag@0x%llX=0x%04X\n",
                          (unsigned long long)actor, (unsigned long long)flagOff, flag);

        if (flag != 0) {
            for (uint64_t po : k_plainOffs) {
                Vec3 p = read<Vec3>(actor + po);
                if (trace) printf("[ENC]   plain@0x%llX=(%.1f,%.1f,%.1f) valid=%d\n",
                                  (unsigned long long)po, p.x, p.y, p.z, (int)ValidateWorldCoord(p));
                if (ValidateWorldCoord(p)) { out = p; return true; }
            }
            continue;
        }

        for (uint64_t rootOff : k_rootOffs) {
            uint64_t outer_ptr = read<uint64_t>(actor + rootOff);
            if (trace) printf("[ENC]   root@0x%llX outer_ptr=0x%llX valid=%d\n",
                              (unsigned long long)rootOff, (unsigned long long)outer_ptr, (int)IsValidAddr(outer_ptr));
            if (!IsValidAddr(outer_ptr)) continue;

            uint64_t e1 = read<uint64_t>(outer_ptr);
            uint64_t a1 = DecObfPtr(e1);
            if (trace) printf("[ENC]     L1 raw=0x%llX dec=0x%llX valid=%d\n",
                              (unsigned long long)e1, (unsigned long long)a1, (int)IsValidAddr(a1));
            if (!e1 || !IsValidAddr(a1)) continue;

            uint64_t e2 = read<uint64_t>(a1);
            uint64_t a2 = DecObfPtr(e2);
            if (trace) printf("[ENC]     L2 raw=0x%llX dec=0x%llX valid=%d\n",
                              (unsigned long long)e2, (unsigned long long)a2, (int)IsValidAddr(a2));
            if (!e2 || !IsValidAddr(a2)) continue;

            uint64_t e3 = read<uint64_t>(a2);
            uint64_t a3 = DecObfPtr(e3);
            if (trace) printf("[ENC]     L3 raw=0x%llX dec=0x%llX valid=%d\n",
                              (unsigned long long)e3, (unsigned long long)a3, (int)IsValidAddr(a3));
            if (!e3 || !IsValidAddr(a3)) continue;

            uint64_t e4 = read<uint64_t>(a3);
            uint64_t pos_ptr = DecObfPtr(e4);
            if (trace) printf("[ENC]     L4 raw=0x%llX dec=0x%llX valid=%d\n",
                              (unsigned long long)e4, (unsigned long long)pos_ptr, (int)IsValidAddr(pos_ptr));
            if (!e4 || !IsValidAddr(pos_ptr)) continue;

            Vec3 p = read<Vec3>(pos_ptr);
            if (trace) printf("[ENC]     vec3@leaf=(%.1f,%.1f,%.1f) valid=%d\n",
                              p.x, p.y, p.z, (int)ValidateWorldCoord(p));
            if (ValidateWorldCoord(p)) { out = p; return true; }

            for (uint64_t inner : { 0x30ULL, 0x50ULL, 0x60ULL, 0xB00ULL }) {
                Vec3 q = read<Vec3>(pos_ptr + inner);
                if (trace) printf("[ENC]     vec3@leaf+0x%llX=(%.1f,%.1f,%.1f) valid=%d\n",
                                  (unsigned long long)inner, q.x, q.y, q.z, (int)ValidateWorldCoord(q));
                if (ValidateWorldCoord(q)) { out = q; return true; }
            }
        }
    }
    if (trace) printf("[ENC] returned false for actor 0x%llX\n", (unsigned long long)actor);
    return false;
}

static Vec3 ReadFramebufferOrigin(uint64_t actor_ptr) {
    if (!IsValidAddr(actor_ptr)) return {};
    Vec3 enc{};
    if (TryEncryptedActorPos(actor_ptr, enc)) return enc;
    Vec3 pos = read<Vec3>(actor_ptr + 0x50);
    if (ValidateWorldCoord(pos)) return pos;
    return {};
}


static uint64_t TraverseShaderGraph(uint64_t ent) {
    if (!IsValidAddr(ent)) return 0;

    if (s_shaderArrayOff) {
        uint64_t list_ptr = read<uint64_t>(ent + s_shaderArrayOff);
        if (IsValidAddr(list_ptr))
            return list_ptr;
        s_shaderArrayOff = 0;
    }

    
    for (uint64_t offset = 0xA0; offset <= 0xFC; offset += 8) {
        uint64_t list_ptr = read<uint64_t>(ent + offset);
        if (!IsValidAddr(list_ptr)) continue;

        for (uint64_t i = 0; i < 100; ++i) {
            uint64_t comp = read<uint64_t>(list_ptr + i * 8);
            if (!IsValidAddr(comp)) continue;

            
            uint16_t id = read<uint16_t>(comp - 0x08);
            if (id == 0x0c63) {
                s_shaderArrayOff = offset;
                return list_ptr;
            }
        }
    }
    return 0;
}


static Vec3 SampleShaderComponents(uint64_t ent, uint16_t& comp_index) {
    uint64_t list_ptr = TraverseShaderGraph(ent);
    if (!list_ptr) return {};

    
    if (comp_index != 0xFFFF) {
        uint64_t comp = read<uint64_t>(list_ptr + (uint64_t)comp_index * 8);
        if (IsValidAddr(comp)) {
            uint16_t id = read<uint16_t>(comp - 0x08);
            if (id == 0x0c63) {
                Vec3 pos = read<Vec3>(comp + 0xB00);
                if (ValidateWorldCoord(pos))
                    return pos;
            }
        }
        comp_index = 0xFFFF;
    }

    
    for (uint64_t i = 0; i < 100; ++i) {
        uint64_t comp = read<uint64_t>(list_ptr + i * 8);
        if (!IsValidAddr(comp)) continue;

        uint16_t id = read<uint16_t>(comp - 0x08);
        if (id == 0x0c63) {
            comp_index = (uint16_t)i;
            Vec3 pos = read<Vec3>(comp + 0xB00);
            if (ValidateWorldCoord(pos))
                return pos;
            return {};
        }
    }
    return {};
}

static Vec3 SampleTransformSlot(uint64_t actor_ptr, uint16_t& comp_index) {
    if (!IsValidAddr(actor_ptr)) return {};

    
    Vec3 pos = SampleShaderComponents(actor_ptr, comp_index);
    if (ValidateWorldCoord(pos))
        return pos;

    
    Vec3 pos50 = ReadFramebufferOrigin(actor_ptr);
    if (ValidateWorldCoord(pos50))
        return pos50;

    return {};
}


static Vec3 QueryTransformCache(uint64_t actor_ptr, uint16_t& comp_index) {
    if (!IsValidAddr(actor_ptr)) return {};

    
    if (comp_index != 0xFFFF && s_shaderArrayOff) {
        uint64_t list_ptr = read<uint64_t>(actor_ptr + s_shaderArrayOff);
        if (IsValidAddr(list_ptr)) {
            uint64_t comp = read<uint64_t>(list_ptr + (uint64_t)comp_index * 8);
            if (IsValidAddr(comp)) {
                uint16_t id = read<uint16_t>(comp - 0x08);
                if (id == 0x0c63) {
                    Vec3 pos = read<Vec3>(comp + 0xB00);
                    if (ValidateWorldCoord(pos))
                        return pos;
                }
            }
        }
        
        comp_index = 0xFFFF;
    }

    
    return SampleTransformSlot(actor_ptr, comp_index);
}


static Vec3 ProjectWorldCoordinate(uint64_t actor_ptr, uint16_t* comp_index_io = nullptr) {
    if (!IsValidAddr(actor_ptr)) return {};

    Vec3 encPos{};
    if (TryEncryptedActorPos(actor_ptr, encPos))
        return encPos;

    uint64_t fb = ReadStencilBuffer(actor_ptr);
    uint16_t local_comp_index = 0xFFFF;
    uint16_t& comp_index = comp_index_io ? *comp_index_io : local_comp_index;

    if (IsActiveStencil(fb)) {
        
        Vec3 pos = SampleShaderComponents(actor_ptr, comp_index);
        if (ValidateWorldCoord(pos))
            return pos;

        
        Vec3 pos50 = read<Vec3>(actor_ptr + 0x50);
        if (ValidateWorldCoord(pos50))
            return pos50;
    }

    
    Vec3 pos50{};
    Vec3 pos60{};

    if (IsValidAddr(actor_ptr + 0x50))
        pos50 = read<Vec3>(actor_ptr + 0x50);
    if (IsValidAddr(actor_ptr + 0x60))
        pos60 = read<Vec3>(actor_ptr + 0x60);

    bool v50 = ValidateWorldCoord(pos50);
    bool v60 = ValidateWorldCoord(pos60);

    if (v50 && v60) {
        float mag50 = pos50.x*pos50.x + pos50.y*pos50.y + pos50.z*pos50.z;
        float mag60 = pos60.x*pos60.x + pos60.y*pos60.y + pos60.z*pos60.z;
        return mag50 >= mag60 ? pos50 : pos60;
    }
    if (v50) return pos50;
    if (v60) return pos60;
    return {};
}


static Vec3 ResolveViewportOrigin(uint64_t e) {
    uint16_t comp_index = 0xFFFF;

    
    {
        std::lock_guard<std::mutex> lock(g_syncMapMtx);
        auto it = g_syncMap.find(e);
        if (it != g_syncMap.end())
            comp_index = it->second.comp_index;
    }

    Vec3 pos = QueryTransformCache(e, comp_index);

    
    if (comp_index != 0xFFFF) {
        std::lock_guard<std::mutex> lock(g_syncMapMtx);
        auto it = g_syncMap.find(e);
        if (it != g_syncMap.end())
            it->second.comp_index = comp_index;
    }

    if (ValidateWorldCoord(pos))
        return pos;

    
    return ProjectWorldCoordinate(e, &comp_index);
}

static void SyncFrameState(RenderSyncEntry& entry, const Vec3& new_pos, std::chrono::steady_clock::time_point now) {
    if (!ValidateWorldCoord(new_pos))
        return;
    entry.position = new_pos;
    entry.position_time = now;
    entry.smoothed_position = new_pos;
}

static Vec3 InterpolateFrameCoord(RenderSyncEntry& entry, float frame_dt) {
    (void)frame_dt;
    if (!ValidateWorldCoord(entry.position))
        return {};
    return entry.position;
}

static void FlushSyncBuffer() {
    std::lock_guard<std::mutex> lock(g_syncMapMtx);
    g_syncMap.clear();
    FlushShaderCache();
    FlushShaderSigCache();
}

static int ReadRound() {
    // Cache the 6-level pointer walk. Called every render frame; without the
    // cache this is 7 driver IOCTLs/frame × 240fps = 1680 game reads/sec just
    // to know if a round changed. Round state moves at ~0.1Hz — 500ms is fine.
    static DWORD s_lastRead = 0;
    static int   s_cachedRs = -1;
    DWORD now = GetTickCount();
    if (s_lastRead && (now - s_lastRead) < 500) return s_cachedRs;
    s_lastRead = now;

    if (!g_RoundPtr) { s_cachedRs = -1; return -1; }
    uint64_t b=read<uint64_t>(g_RoundPtr); if(!IsValidAddr(b)){s_cachedRs=-1;return -1;}
    uint64_t p1=read<uint64_t>(b+0x40); if(!IsValidAddr(p1)){s_cachedRs=-1;return -1;}
    uint64_t p2=read<uint64_t>(p1+0x48); if(!IsValidAddr(p2)){s_cachedRs=-1;return -1;}
    uint64_t p3=read<uint64_t>(p2+0x78); if(!IsValidAddr(p3)){s_cachedRs=-1;return -1;}
    uint64_t p4=read<uint64_t>(p3+0x18); if(!IsValidAddr(p4)){s_cachedRs=-1;return -1;}
    uint64_t p5=read<uint64_t>(p4+0x90); if(!IsValidAddr(p5)){s_cachedRs=-1;return -1;}
    uint64_t p6=read<uint64_t>(p5+0x38); if(!IsValidAddr(p6)){s_cachedRs=-1;return -1;}
    int st=read<int>(p6+0x348);
    s_cachedRs = (st>=0&&st<=5)?st:-1;
    return s_cachedRs;
}
static void FindRound() {
    if (g_RoundFound||!g_textCache.valid) return;
    const uint8_t* t=g_textCache.data.data(); size_t sz=(size_t)g_textCache.textSize;
    uint64_t tb=g_textCache.textBase;
    for (size_t i=0;i+15<sz;i++) {
        if(t[i]!=0xE8) continue; if((t[i+12]&0xF0)!=0x40) continue;
        int32_t r=*(int32_t*)&t[i+8]; uint64_t c=tb+i+12+(int64_t)r;
        if(!IsValidAddr(c)) continue;
        uint64_t b=read<uint64_t>(c); if(!b||!IsValidAddr(b)) continue;
        uint64_t p1=read<uint64_t>(b+0x40); if(!IsValidAddr(p1)) continue;
        uint64_t p2=read<uint64_t>(p1+0x48); if(!IsValidAddr(p2)) continue;
        uint64_t p3=read<uint64_t>(p2+0x78); if(!IsValidAddr(p3)) continue;
        uint64_t p4=read<uint64_t>(p3+0x18); if(!IsValidAddr(p4)) continue;
        uint64_t p5=read<uint64_t>(p4+0x90); if(!IsValidAddr(p5)) continue;
        uint64_t p6=read<uint64_t>(p5+0x38); if(!IsValidAddr(p6)) continue;
        int st=read<int>(p6+0x348);
        if(st>=0&&st<=5) { g_RoundPtr=c; g_RoundFound=true; return; }
    }
}

static void PollFrameRing() {
    if (!g_RingAddr) return;
    uint64_t wi=read<uint64_t>(g_RingAddr);
    uint64_t new_captures = 0, rejected = 0;
    while (g_ReadIdx < wi) {
        uint64_t idx=g_ReadIdx&(RING_SZ-1);
        uint64_t ep=read<uint64_t>(g_RingAddr+0x10+idx*8);
        if (IsValidAddr(ep)&&ValidatePtr(ep)) { std::lock_guard<std::mutex> l(g_frameMtx); g_capturedFrames.insert(ep); g_totalFrames++; new_captures++; }
        else rejected++;
        g_ReadIdx++;
    }
    static DWORD s_lastRingDbg = 0;
    static int s_ringDbgCount = 0;
    DWORD nt = GetTickCount();
    if (s_ringDbgCount < 2 && (new_captures > 0 || (nt - s_lastRingDbg > 2000 && wi > 0))) {
        s_ringDbgCount++;
        printf("[RING] wi=%llu readIdx=%llu new=%llu rejected=%llu total_captured=%llu unique=%zu\n",
               (unsigned long long)wi, (unsigned long long)g_ReadIdx,
               (unsigned long long)new_captures, (unsigned long long)rejected,
               (unsigned long long)g_totalFrames.load(), g_capturedFrames.size());
        s_lastRingDbg = nt;
    }
}

static bool EnsureShellPage() {
    if (g_ShellPage) return true;
    KAllocRequest ar = {};
    ar.ProcessId = driver->ProcessId;
    ar.Size = 8192;
    ar.AllocationType = MEM_COMMIT | MEM_RESERVE;
    ar.Protect = PAGE_READWRITE;
    ULONG ret = 0;
    GetNtApi().Ioctl(driver->Handle(), IOCTL_ALLOC_MEMORY, &ar, sizeof(ar), &ar, sizeof(ar), &ret);
    g_ShellPage = ar.Address;
    if (!g_ShellPage) {
        printf("[MATCH] Shell page alloc FAILED\n");
        return false;
    }
    g_RingAddr = g_ShellPage + 0x100;
    printf("[MATCH] Shell page alloc OK at 0x%llX\n", (unsigned long long)g_ShellPage);
    return true;
}

static void ReleaseShellPage() {
    if (!g_ShellPage) return;
    uint8_t zeros[4096] = {};
    DrvWriteRaw(zeros, g_ShellPage, 4096);
    uint32_t old = 0;
    DrvProtect(g_ShellPage, 4096, PAGE_READWRITE, &old);
    driver->FreeMemory(g_ShellPage, 0, MEM_RELEASE);
    printf("[MATCH] Shell page released (0x%llX)\n", (unsigned long long)g_ShellPage);
    g_ShellPage = 0;
    g_RingAddr = 0;
    g_ReadIdx = 0;
}

static bool AttachFrameSync() {
    if (g_frameSyncActive || !g_frameSyncAddr || !g_ShellPage) {
        printf("[HOOK-FAIL] precheck: active=%d addr=0x%llX shell=0x%llX\n",
               (int)g_frameSyncActive, (unsigned long long)g_frameSyncAddr, (unsigned long long)g_ShellPage);
        return false;
    }
    for (int i=0;i<32;i++) g_OrigBytes[i]=read<uint8_t>(g_frameSyncAddr+i);
    g_PatchLen = FindBoundary(g_OrigBytes, 14);
    if (g_PatchLen<14||g_PatchLen>30) {
        printf("[HOOK-FAIL] FindBoundary returned %u (need 14-30) at 0x%llX. First 16 bytes: ",
               g_PatchLen, (unsigned long long)g_frameSyncAddr);
        for (int i=0;i<16;i++) printf("%02X ", g_OrigBytes[i]);
        printf("\n");
        return false;
    }
    uint8_t sc[80]={}; int p=0;
    sc[p++]=0x50; sc[p++]=0x52;
    sc[p++]=0x48; sc[p++]=0xB8;
    *(uint64_t*)&sc[p]=g_RingAddr; p+=8;
    sc[p++]=0x48; sc[p++]=0x8B; sc[p++]=0x10;
    sc[p++]=0x0F; sc[p++]=0xB6; sc[p++]=0xD2;
    sc[p++]=0x48; sc[p++]=0x89; sc[p++]=0x4C; sc[p++]=0xD0; sc[p++]=0x10;
    sc[p++]=0x48; sc[p++]=0xFF; sc[p++]=0x00;
    sc[p++]=0x5A; sc[p++]=0x58;
    memcpy(&sc[p],g_OrigBytes,g_PatchLen); p+=g_PatchLen;
    sc[p++]=0xFF; sc[p++]=0x25; sc[p++]=0; sc[p++]=0; sc[p++]=0; sc[p++]=0;
    *(uint64_t*)&sc[p]=g_frameSyncAddr+g_PatchLen; p+=8;
    g_ShellSize=p;
    if (!DrvWriteRaw(sc, g_ShellPage, p)) {
        printf("[HOOK-FAIL] DrvWriteRaw shellcode to 0x%llX (%d bytes) failed\n",
               (unsigned long long)g_ShellPage, p);
        return false;
    }
    uint32_t old=0; DrvProtect(g_ShellPage, 4096, PAGE_EXECUTE_READWRITE, &old);
    uint64_t zero=0; DrvWriteRaw(&zero, g_RingAddr, 8); g_ReadIdx=0;
    uint8_t hook[32]={};
    hook[0]=0xFF; hook[1]=0x25; hook[2]=0; hook[3]=0; hook[4]=0; hook[5]=0;
    *(uint64_t*)&hook[6]=g_ShellPage;
    for(int i=14;i<g_PatchLen;i++) hook[i]=0x90;
    if (!DrvWriteExec(hook, g_frameSyncAddr, g_PatchLen)) {
        printf("[HOOK-FAIL] DrvWriteExec hook stub to 0x%llX (%u bytes) failed\n",
               (unsigned long long)g_frameSyncAddr, g_PatchLen);
        return false;
    }
    g_frameSyncActive=true; g_frameSyncStart=GetTickCount();
    printf("[HOOK] INSTALLED - .text @ 0x%llX patched, shell @ 0x%llX ring @ 0x%llX (restore in %dms)\n",
           (unsigned long long)g_frameSyncAddr, (unsigned long long)g_ShellPage,
           (unsigned long long)g_RingAddr, COLLECT_MS);
    return true;
}

static bool DetachFrameSync() {
    if (!g_frameSyncActive) return false;
    if (!DrvWriteExec(g_OrigBytes, g_frameSyncAddr, g_PatchLen)) return false;
    g_frameSyncActive=false;
    printf("[HOOK] REMOVED - .text restored after %dms\n", GetTickCount()-g_frameSyncStart);
return true;
}


static constexpr int TRAIL_MAX_ENTITIES = 32;
static constexpr int TRAIL_MAX_POINTS = 200;
struct TrailBuffer {
    uint64_t entityId;
    Vec3 points[TRAIL_MAX_POINTS];
    int count;
    int writeIdx;
    DWORD lastUpdate;
};
static TrailBuffer g_trailBuffers[TRAIL_MAX_ENTITIES] = {};
static int g_trailBufCount = 0;

static TrailBuffer* AllocTrailBuffer(uint64_t entityId) {
    for (int i = 0; i < g_trailBufCount; i++)
        if (g_trailBuffers[i].entityId == entityId) return &g_trailBuffers[i];
    if (g_trailBufCount < TRAIL_MAX_ENTITIES) {
        TrailBuffer* t = &g_trailBuffers[g_trailBufCount++];
        t->entityId = entityId;
        t->count = 0;
        t->writeIdx = 0;
        t->lastUpdate = 0;
        return t;
    }
    int oldest = 0; DWORD oldestT = UINT_MAX;
    for (int i = 0; i < TRAIL_MAX_ENTITIES; i++)
        if (g_trailBuffers[i].lastUpdate < oldestT) { oldestT = g_trailBuffers[i].lastUpdate; oldest = i; }
    TrailBuffer* t = &g_trailBuffers[oldest];
    t->entityId = entityId;
    t->count = 0;
    t->writeIdx = 0;
    return t;
}

static void AppendTrailSample(TrailBuffer* t, Vec3 pos) {
    extern int trailUpdateMs;
    DWORD now = GetTickCount();
    if (now - t->lastUpdate < (DWORD)trailUpdateMs) return;
    t->lastUpdate = now;
    t->points[t->writeIdx] = pos;
    t->writeIdx = (t->writeIdx + 1) % TRAIL_MAX_POINTS;
    if (t->count < TRAIL_MAX_POINTS) t->count++;
}

static bool InitRenderPipeline(uint64_t base, uint64_t size) {
    g_imageBase=base;
    auto secs=GetPESections(base);
    if(secs.empty()) return false;
    if(!CacheTextSection(base,secs)) return false;

    // Scan for game singleton pointers (GameManager, ViewData, CameraManager).
    ScanGamePointers(base);
    OFFSETS::pGameManagerPtr = g_pGameManagerPtr;
    OFFSETS::pViewDataPtr = g_pViewDataPtr;
    OFFSETS::pCameraManagerPtr = g_pCameraManagerPtr;

    // If we have ViewData from sigs, use it as the projection source directly
    // instead of the slow heap scan. ViewData → +0x250 = view matrix,
    // +0x190 = camera origin (same offsets as the heap-scan result).
    if (g_pViewDataPtr) {
        uint64_t vd = read<uint64_t>(g_pViewDataPtr);
        if (IsValidAddr(vd)) {
            g_projectionAddr = vd;
            printf("[R6] ViewData from sig: 0x%llX → projection=0x%llX\n",
                (unsigned long long)g_pViewDataPtr, (unsigned long long)vd);
        }
    }
    if (!g_projectionAddr)
        g_projectionAddr = ScanForViewTrans(base, size);

    auto calls=FindEntityFunctionCalls(base);
    for(auto& c:calls) if(c.hasTestAlAl){g_frameSyncAddr=c.targetVA;break;}
    if(!g_frameSyncAddr&&!calls.empty()) g_frameSyncAddr=calls[0].targetVA;
    if(!g_frameSyncAddr) return false;
    FindRound();
    g_projectionAddr=ScanForViewTrans(base,size);
    if (ScanSkelXref(base)) {
        printf("[R6] Skeleton xref: compIdx=+0x%X compArr=+0x%X func=0x%llX\n",
            g_SkelXref.compIdxOff, g_SkelXref.compArrOff, (unsigned long long)g_SkelXref.skelFuncVA);
        
        if (g_SkelXref.compArrOff)
            s_shaderArrayOff = g_SkelXref.compArrOff;
    } else {
        printf("[R6] Skeleton xref not found - dxrainbow component scan 0xA0-0xFC active\n");
    }

    ScanBoneSigs(base);

    // Apply the bone-entry translation offset discovered by the sig scanner.
    // The sigs tell us whether position lives at +0x30 or +0x38 within each
    // 0x40-stride bone entry. Override the default kBoneTranslate.
    if (g_BoneSig.valid) {
        skel::kBoneTranslate = g_BoneSig.boneTransOff;
        printf("[SKEL] Bone sig applied: transOff=+0x%llX (was +0x30)\n",
            (unsigned long long)skel::kBoneTranslate);
    }

    // Wire the quaternion + physics-world readers into skel::ReadWorldMatrix.
    // This makes every skeleton path that calls ReadWorldMatrix use the
    // authoritative rotation (quaternion) and position (PhysicsWorld) instead
    // of the stale +0x240 matrix / laggy +0xB00 position.
    skel::g_readRotQuatFn = skel_readRotQuat_adapter;
    skel::g_readPhysPosFn = skel_readPhysPos_adapter;

    if (g_SkelXref.compArrOff)
        skel::g_componentArrayOffset = g_SkelXref.compArrOff;
    if (g_SkelXref.compIdxOff)
        skel::g_compIdxOff = g_SkelXref.compIdxOff;
    printf("[SKEL] compArrayOffset=+0x%llX compIdxOff=+0x%X\n",
        (unsigned long long)skel::g_componentArrayOffset, skel::g_compIdxOff);

    for (int attempt = 0; attempt < 6 && !skel::ScanRegistry(); ++attempt) {
        Sleep(200);
    }
    printf("[SKEL] Ready=%d registry=0x%llX\n", (int)skel::Ready(), (unsigned long long)skel::g_registryBase);

    CreateThread(NULL, 0, [](LPVOID) -> DWORD { ScanSidewards(); return 0; }, NULL, 0, NULL);

    printf("[R6] Position system: paster (EXTERNAL: encrypted-chain FIRST / 0x0c63@+0xB00 / 0x50 fallback + velocity smooth)\n");
    return true;
}

static void ShutdownRenderPipeline() {
    // Unpatch .text first, then wait, then free the shell page. Same ordering
    // as the F2-OFF path — game threads may still be executing the trampoline.
    if(g_frameSyncActive) DetachFrameSync();
    RestoreSidewards();
    Sleep(400);
    FlushSyncBuffer();
    ReleaseShellPage();
    if(g_log){fclose(g_log);g_log=nullptr;}
    skel::CloseLog();
}

static void PollSyncBuffer(int W, int H, int maxD) {
    DWORD now_tick = GetTickCount();
    if (now_tick - g_lastEntityUpdate < ENTITY_UPDATE_INTERVAL) return;
    g_lastEntityUpdate = now_tick;

    std::lock_guard<std::mutex> lk(g_Mtx);
    g_vertexBuffer.clear(); g_vtxCount = 0; g_activeVtx = 0;

    static DWORD s_posDbg = 0;
    bool posDbg = false;
    if (posDbg) s_posDbg = now_tick;
    if (!g_RoundFound) FindRound();
    int rs = g_RoundFound ? ReadRound() : 3;

    static int s_lastRoundState = -2;
    extern bool sidewardsEnabled;
    extern float sidewardsValue;

    extern std::atomic<bool> g_manualInMatch;
    bool isGameplay = g_manualInMatch.load(std::memory_order_acquire);
    static bool s_wasManual = false;
    bool wasGameplay = s_wasManual;
    bool newRound = (isGameplay && !wasGameplay);

    if (newRound) {
        printf("[MATCH] Manual IN MATCH toggle ON (rs=%d)\n", rs);
        EnsureShellPage();
        if (!g_Sidewards.found) {
            CreateThread(NULL, 0, [](LPVOID) -> DWORD { ScanSidewards(); return 0; }, NULL, 0, NULL);
        }
        if (sidewardsEnabled && g_Sidewards.found) {
            SetSidewardsValue(sidewardsValue);
        }
        g_syncComplete = false;
        FlushSyncBuffer();
        { std::lock_guard<std::mutex> l(g_frameMtx); g_capturedFrames.clear(); }
    }

    if (!isGameplay && wasGameplay) {
        printf("[MATCH] Manual OUT OF MATCH toggle OFF - tearing down\n");

        // Step 1: unpatch .text FIRST. No new hook fires can begin from this moment.
        if (g_frameSyncActive) DetachFrameSync();
        RestoreSidewards();

        // Step 2: capture the page pointer but DO NOT null the globals yet.
        // The globals stay live so the delayed-free thread can still see them,
        // and so any in-flight shellcode execution can still complete its
        // trampoline back into game code (the trampoline lives IN the shell page).
        uint64_t doomedPage = g_ShellPage;

        g_syncComplete = false;
        FlushSyncBuffer();
        { std::lock_guard<std::mutex> l(g_frameMtx); g_capturedFrames.clear(); }
        s_wasManual = false;
        s_lastRoundState = -2;

        if (doomedPage) {
            CreateThread(NULL, 0, [](LPVOID p) -> DWORD {
                // Step 3: wait long enough for any game thread still executing
                // the shell trampoline to complete. 400ms >> worst-case scheduler
                // gap + frame time. Only THEN zero + unmap + null globals.
                Sleep(400);
                uint64_t page = (uint64_t)p;
                uint8_t zeros[4096] = {};
                DrvWriteRaw(zeros, page, 4096);
                uint32_t old = 0;
                DrvProtect(page, 4096, PAGE_READWRITE, &old);
                driver->FreeMemory(page, 0, MEM_RELEASE);
                g_ShellPage = 0;
                g_RingAddr  = 0;
                g_ReadIdx   = 0;
                printf("[MATCH] Shell page freed (delayed, 0x%llX)\n", (unsigned long long)page);
                return 0;
            }, (LPVOID)doomedPage, 0, NULL);
        }
    }

    // While OFF, do NOT touch game memory at all. Early-return before any
    // camera / round / frame-ring reads. This is what makes F2 OFF actually
    // "off" instead of a quieter version of on.
    if (!isGameplay) {
        return;
    }

    if (isGameplay && !g_ShellPage) {
        EnsureShellPage();
    }

    s_wasManual = isGameplay;
    s_lastRoundState = rs;

    static DWORD s_hookDelayStart = 0;
    static DWORD s_hookDelayMs = 0;

    if (newRound) {
        s_hookDelayStart = now_tick;
        s_hookDelayMs = 500 + (rand() % 1500);
        printf("[HOOK] Delay %dms before patching\n", s_hookDelayMs);
    }

    bool delayPassed = (s_hookDelayStart > 0 && (now_tick - s_hookDelayStart) >= s_hookDelayMs);
    if (!g_frameSyncActive && !g_syncComplete && isGameplay && delayPassed) {
        AttachFrameSync();
        s_hookDelayStart = 0;
    }
    if (g_frameSyncActive) { PollFrameRing(); if (GetTickCount() - g_frameSyncStart > COLLECT_MS) { DetachFrameSync(); g_syncComplete = true; } }

    static DWORD s_lastHookEnd = 0;
    if (g_syncComplete && !g_frameSyncActive && isGameplay) {
        if (s_lastHookEnd == 0) s_lastHookEnd = now_tick;
        if (now_tick - s_lastHookEnd > (REHOOK_MIN_MS + (rand() % REHOOK_JITTER_MS))) {
            g_syncComplete = false;
            s_lastHookEnd = 0;
        }
    }

    std::vector<uint64_t> cap;
    {
        std::lock_guard<std::mutex> l(g_frameMtx);
        for (auto it = g_capturedFrames.begin(); it != g_capturedFrames.end(); ++it) {
            if (IsValidAddr(*it)) cap.push_back(*it);
        }
    }

    Vec3 cam = QueryCameraOrigin();
    auto now = std::chrono::steady_clock::now();
    float frame_dt = 1.f / 60.f;
    {

        static DWORD s_prevTick = 0;
        if (s_prevTick != 0) {
            float raw = (float)(now_tick - s_prevTick) / 1000.f;
            if (raw > 0.f && raw < 0.25f) frame_dt = raw;
        }
        s_prevTick = now_tick;
    }


    {
        std::lock_guard<std::mutex> clock(g_syncMapMtx);


        for (uint64_t ea : cap) {
            uint64_t fb = ReadStencilBuffer(ea);
            if (!IsActiveStencil(fb) && !ValidateDepthStencil(fb)) {
                g_syncMap.erase(ea);
                continue;
            }

            auto& entry = g_syncMap[ea];
            entry.filter_byte = fb;
            entry.last_seen = now;

            Vec3 position = QueryTransformCache(ea, entry.comp_index);
            if (ValidateWorldCoord(position))
                SyncFrameState(entry, position, now);
        }


        static size_t s_rrIndex = 0;
        constexpr int k_maxReadsPerTick = 4;
        int readsThisTick = 0;

        std::vector<uint64_t> syncKeys;
        for (auto it = g_syncMap.begin(); it != g_syncMap.end(); ) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_seen);
            if (age > k_syncMaxAge) {
                it = g_syncMap.erase(it);
                continue;
            }
            syncKeys.push_back(it->first);
            ++it;
        }

        if (!syncKeys.empty()) {
            if (s_rrIndex >= syncKeys.size()) s_rrIndex = 0;
            for (int i = 0; i < k_maxReadsPerTick && i < (int)syncKeys.size(); ++i) {
                size_t idx = (s_rrIndex + i) % syncKeys.size();
                uint64_t addr = syncKeys[idx];
                auto sit = g_syncMap.find(addr);
                if (sit == g_syncMap.end()) continue;
                if (!IsValidAddr(addr)) continue;
                uint64_t fb = ReadStencilBuffer(addr);
                if (IsActiveStencil(fb) || ValidateDepthStencil(fb)) {
                    sit->second.filter_byte = fb;
                    Vec3 fresh = QueryTransformCache(addr, sit->second.comp_index);
                    if (ValidateWorldCoord(fresh))
                        SyncFrameState(sit->second, fresh, now);
                }
            }
            s_rrIndex = (s_rrIndex + k_maxReadsPerTick) % syncKeys.size();
        }


        for (auto it = g_syncMap.begin(); it != g_syncMap.end(); ++it) {
            uint64_t ea = it->first;
            auto& entry = it->second;

            if (!ValidateDepthStencil(entry.filter_byte))
                continue;


            Vec3 draw_pos = entry.position;
            if (!ValidateWorldCoord(draw_pos))
                continue;

            float d = sqrtf(
                (draw_pos.x - cam.x) * (draw_pos.x - cam.x) +
                (draw_pos.y - cam.y) * (draw_pos.y - cam.y) +
                (draw_pos.z - cam.z) * (draw_pos.z - cam.z));

            if (d < k_minRenderDist || d >(float)maxD)
                continue;

            Vec3 sp = {};
            bool on = W2S(draw_pos, sp, W, H);

            OverlayVertex e = {};
            e.instance = ea;
            e.position = draw_pos;
            e.status = IsClearedStencil(entry.filter_byte) ? ActorStatus::DEAD_1 : ActorStatus::VALID;
            e.isPlayer = IsActiveStencil(entry.filter_byte);
            e.distance = d;
            e.screenPos = sp;
            e.onScreen = on;
            e.hasBones = false;
            e.hp = 100;
            e.operatorName[0] = '\0';
            e.headOnScreen = false;

            if (Esp_skeleton) {
                ReadSkeleton(ea, draw_pos.x, draw_pos.y, draw_pos.z, e.bones);
                if (e.bones.total > 0) {
                    e.hasBones = true;
                    Vec3B hb = GetHeadBonePos(e.bones);
                    Vec3 hp = { hb.x, hb.y, hb.z };
                    e.headOnScreen = W2S(hp, e.headScreenPos, W, H);
                }
            }


            const char* opName = ResolveShaderLabel(ea);
            if (opName) {
                strncpy(e.operatorName, opName, sizeof(e.operatorName) - 1);
                e.operatorName[sizeof(e.operatorName) - 1] = '\0';
            }

            // Live HP from the DamageComponent (entity -> DamageComp -> 0x183
            // tag -> +0xE0 -> +0x38 -> int32 hp). Falls back to 100 on miss.
            {
                int hp = r6hp::ReadEntityHealth(ea);
                if (hp > 0) e.hp = hp;
            }

            if (posDbg && g_vtxCount < 3) {
                printf("[POS] entity=0x%llX pos=(%.1f,%.1f,%.1f) smooth=(%.1f,%.1f,%.1f) comp=%u dist=%.0f on=%d\n",
                    (unsigned long long)ea,
                    entry.position.x, entry.position.y, entry.position.z,
                    draw_pos.x, draw_pos.y, draw_pos.z,
                    (unsigned)entry.comp_index, d, on ? 1 : 0);
            }

            g_vertexBuffer.push_back(e);
            g_vtxCount++;
            if (e.isPlayer) g_activeVtx++;
        }
    }

    static DWORD s_lastW2SRescan = 0;
    if (g_projectionAddr && (cam.x == 0.f && cam.y == 0.f && cam.z == 0.f) && now_tick - s_lastW2SRescan > 5000) {
        s_lastW2SRescan = now_tick;
        uint64_t newVt = ScanForViewTrans(g_imageBase, 0x18000000);
        if (newVt) { g_projectionAddr = newVt; }
    }

    static DWORD s_lastSummary = 0;
    static int s_summaryCount = 0;
    if (now_tick - s_lastSummary > 1000) {
        s_lastSummary = now_tick;
        if (s_summaryCount < 3) {
            s_summaryCount++;
            int onScreenCnt = 0;
            for (auto& v : g_vertexBuffer) if (v.onScreen) onScreenCnt++;
            printf("[ESP-DBG] rs=%d gameplay=%d syncActive=%d syncComplete=%d cap=%zu syncMap=%zu vtxBuf=%d onScreen=%d active=%d\n",
                   rs, (int)isGameplay, (int)g_frameSyncActive, (int)g_syncComplete,
                   cap.size(), g_syncMap.size(), g_vtxCount, onScreenCnt, g_activeVtx);
        }
    }
}

static void FlushOverlayPipeline(bool box, bool corner, bool line, bool dist, int visDist,
                            bool trail, bool aimEnabled, float aimFov, float aimSmooth,
                            int hitboxSel, bool fovCircle, bool squareFov, bool xhair) {
    int W = GetSystemMetrics(SM_CXSCREEN), H = GetSystemMetrics(SM_CYSCREEN);
    PollSyncBuffer(W, H, visDist);
    std::lock_guard<std::mutex> lk(g_Mtx);
    ImDrawList* dl = ImGui::GetOverlayDrawList();
    if (!dl) return;


    float bestAimDist = 1e30f;
    float bestAimX = 0, bestAimY = 0;
    bool hasAimTarget = false;

    extern float boxThickness;
    extern float snaplineThickness;
    extern float fovCircleThickness;
    extern float crosshairSize;
    extern float espBoxColor[4];
    extern float espSnaplineColor[4];
    extern float espDistanceColor[4];
    extern float fovCircleColor[4];
    extern float crosshairColor[4];
    extern float aimbotTargetColor[4];
    extern float filledBoxColor[4];
    extern float espTrailColor[4];
    extern float trailThickness;
    extern int trailLength;
    extern bool fillbox;
    extern bool lineheadesp;
    extern bool rainbowMode;
    extern bool rainbowBox;
    extern bool rainbowFov;
    extern bool rainbowSnaplines;
    extern bool rainbowTrail;

    // ═══ pre-loop: priority-target index (closest to crosshair on-screen) ═══
    extern bool  priorityHighlight;
    extern float priorityColor[4];
    extern bool  lowHpPriority;
    extern float lowHpColor[4];
    extern bool  corpseEsp;
    extern float corpseColor[4];
    extern bool  distanceFade;
    extern float distanceFadeNear;
    extern float distanceFadeFar;

    const float cxx = W * 0.5f, cyy = H * 0.5f;
    int priorityIdx = -1;
    float priorityD2 = 1e18f;
    if (priorityHighlight) {
        for (size_t i = 0; i < g_vertexBuffer.size(); ++i) {
            const auto& e = g_vertexBuffer[i];
            if (!e.isPlayer || !e.onScreen) continue;
            float dx = e.screenPos.x - cxx, dy = e.screenPos.y - cyy;
            float d2 = dx*dx + dy*dy;
            if (d2 < priorityD2) { priorityD2 = d2; priorityIdx = (int)i; }
        }
    }

    for (size_t idx = 0; idx < g_vertexBuffer.size(); ++idx) {
        auto& e = g_vertexBuffer[idx];
        if (!e.onScreen) continue;
        float sx = e.screenPos.x, sy = e.screenPos.y;
        float entOffset = (float)(e.instance & 0xFF) / 255.0f;

        ImU32 boxCol = GetBoxColor(entOffset);
        ImU32 snapCol = GetSnaplineColor(entOffset);

        // ═══ per-entity color overrides ═══
        // Corpse tint applies first (dead entities aren't priority targets).
        bool isCorpse = (e.status == ActorStatus::DEAD_1 || e.status == ActorStatus::DEAD_2);
        if (isCorpse) {
            if (!corpseEsp) continue;  // don't draw the dead at all if user disabled
            boxCol = ColorToU32(corpseColor);
        } else {
            if (priorityHighlight && (int)idx == priorityIdx)
                boxCol = ColorToU32(priorityColor);
            else if (lowHpPriority && e.hp > 0 && e.hp <= 30)
                boxCol = ColorToU32(lowHpColor);
        }

        // Distance fade — scale alpha by distance falloff. Applied last so it
        // affects overrides too.
        if (distanceFade && !isCorpse) {
            float lo = distanceFadeNear, hi = distanceFadeFar;
            if (hi > lo) {
                float t = (e.distance - lo) / (hi - lo);
                if (t < 0.f) t = 0.f;
                if (t > 1.f) t = 1.f;
                float mul = 1.0f - t * 0.75f;  // fades to 25% alpha
                uint32_t a = (boxCol >> 24) & 0xFF;
                a = (uint32_t)(a * mul);
                boxCol = (boxCol & 0x00FFFFFF) | (a << 24);
                a = (snapCol >> 24) & 0xFF;
                a = (uint32_t)(a * mul);
                snapCol = (snapCol & 0x00FFFFFF) | (a << 24);
            }
        }

        
        Vec3 headPos = {e.position.x, e.position.y, e.position.z + k_viewportHeight};
        Vec3 headScr = {};
        bool headOn = W2S(headPos, headScr, W, H);

        if (box && headOn) {
            float boxH = fabsf(sy - headScr.y);
            if (boxH < 4.f) continue;
            float boxW = boxH * 0.45f;
            float top = fminf(sy, headScr.y);
            float bot = fmaxf(sy, headScr.y);
            float left = sx - boxW * 0.5f;
            float right = sx + boxW * 0.5f;

            if (fillbox) {
                dl->AddRectFilled({left, top}, {right, bot}, ColorToU32(filledBoxColor));
            }
            if (corner) {
                
                float w = right - left, h = bot - top, f = 0.25f;
                dl->AddLine({left,top},{left+w*f,top},boxCol,boxThickness);
                dl->AddLine({left,top},{left,top+h*f},boxCol,boxThickness);
                dl->AddLine({right,top},{right-w*f,top},boxCol,boxThickness);
                dl->AddLine({right,top},{right,top+h*f},boxCol,boxThickness);
                dl->AddLine({left,bot},{left+w*f,bot},boxCol,boxThickness);
                dl->AddLine({left,bot},{left,bot-h*f},boxCol,boxThickness);
                dl->AddLine({right,bot},{right-w*f,bot},boxCol,boxThickness);
                dl->AddLine({right,bot},{right,bot-h*f},boxCol,boxThickness);
            } else {
                dl->AddRect({left, top}, {right, bot}, boxCol, 0, 0, boxThickness);
            }
        }

        if (line) {
            extern int snaplineOrigin;
            ImVec2 from;
            if (snaplineOrigin == 0) from = ImVec2((float)W/2, 0);           
            else if (snaplineOrigin == 1) from = ImVec2((float)W/2, (float)H/2); 
            else from = ImVec2((float)W/2, (float)H);                        
            dl->AddLine(from, {sx, sy}, snapCol, snaplineThickness);
        }

        if (lineheadesp && headOn) {
            dl->AddLine({sx, sy}, {(float)headScr.x, (float)headScr.y}, IM_COL32(255,255,0,200), 1.0f);
        }

        if (Esp_skeleton && e.hasBones) {
            ImU32 skCol = ColorToU32(espSkeletonColor);
            float skTh = skeletonThickness;
            const SkeletonBones& b = e.bones;
            auto boneLine = [&](int a, int c) {
                if (!b.hasBone[a] || !b.hasBone[c]) return;
                Vec3 wa = { b.bones[a].x, b.bones[a].y, b.bones[a].z };
                Vec3 wb = { b.bones[c].x, b.bones[c].y, b.bones[c].z };
                Vec3 sa = {}, sb = {};
                if (W2S(wa, sa, W, H) && W2S(wb, sb, W, H))
                    dl->AddLine({sa.x, sa.y}, {sb.x, sb.y}, skCol, skTh);
            };
            for (int i = 0; i < skel::kNumConnections; i++)
                boneLine(skel::kConnections[i].first, skel::kConnections[i].second);
        }

        if (dist) {
            char dt[32]; snprintf(dt, 32, "%.0fm", e.distance);
            dl->AddText({sx - 10 + 1, sy + 3 + 1}, IM_COL32(0, 0, 0, 200), dt);
            dl->AddText({sx - 10, sy + 3}, ColorToU32(espDistanceColor), dt);
        }

        extern bool shaderLabelOverlay;
        extern bool shaderIconOverlay;
        extern bool depthVisualization;
        if (shaderLabelOverlay && e.isPlayer && headOn) {
            float cx = sx;
            float ty = fminf(sy, headScr.y);
            float boxH = fabsf(sy - headScr.y);
            if (boxH < 4.f) boxH = 16.f;

            if (shaderIconOverlay && e.operatorName[0]) {
                
                float iconSz = boxH * 0.35f;
                if (iconSz < 16.0f) iconSz = 16.0f;
                if (iconSz > 48.0f) iconSz = 48.0f;
                ImTextureID iconTex = QueryShaderResource(e.operatorName);
                if (iconTex) {
                    float iconX = cx - iconSz * 0.5f;
                    float iconY = ty - iconSz - 3.0f;
                    dl->AddImage(iconTex, ImVec2(iconX, iconY), ImVec2(iconX + iconSz, iconY + iconSz));
                } else {
                    
                    ImVec2 tsz = ImGui::CalcTextSize(e.operatorName);
                    float textX = cx - tsz.x * 0.5f;
                    float textY = ty - 15.0f;
                    dl->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0,0,0,200), e.operatorName);
                    dl->AddText(ImVec2(textX, textY), IM_COL32(255,200,50,255), e.operatorName);
                }
            } else if (e.operatorName[0]) {
                
                ImVec2 tsz = ImGui::CalcTextSize(e.operatorName);
                float textX = cx - tsz.x * 0.5f;
                float textY = ty - 15.0f;
                dl->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0,0,0,200), e.operatorName);
                dl->AddText(ImVec2(textX, textY), IM_COL32(255,200,50,255), e.operatorName);
            } else {
                
                char opLabel[32];
                snprintf(opLabel, 32, "P%d", (int)(e.instance & 0xFF));
                ImVec2 tsz = ImGui::CalcTextSize(opLabel);
                float textX = cx - tsz.x * 0.5f;
                float textY = ty - 15.0f;
                dl->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0,0,0,200), opLabel);
                dl->AddText(ImVec2(textX, textY), IM_COL32(200,200,200,200), opLabel);
            }
        }

        
        if (depthVisualization && headOn) {
            float top_y = fminf(sy, headScr.y);
            float bot_y = fmaxf(sy, headScr.y);
            float boxH_hp = bot_y - top_y;
            if (boxH_hp >= 4.f) {
                float boxW_hp = boxH_hp * 0.45f;
                float barX = sx - boxW_hp * 0.5f - 6.0f;
                float barW = 3.0f;
                float hpFrac = (float)e.hp / 100.0f;
                if (hpFrac > 1.0f) hpFrac = 1.0f;
                if (hpFrac < 0.0f) hpFrac = 0.0f;
                float filledH = boxH_hp * hpFrac;
                int rr = (int)(255.0f * (1.0f - hpFrac));
                int gg = (int)(255.0f * hpFrac);
                dl->AddRectFilled(ImVec2(barX, top_y), ImVec2(barX + barW, bot_y), IM_COL32(20,20,20,180));
                dl->AddRectFilled(ImVec2(barX, bot_y - filledH), ImVec2(barX + barW, bot_y), IM_COL32(rr,gg,0,220));
                dl->AddRect(ImVec2(barX, top_y), ImVec2(barX + barW, bot_y), IM_COL32(0,0,0,200));
            }
        }

        if (trail && e.isPlayer) {
            TrailBuffer* tr = AllocTrailBuffer(e.instance);
            AppendTrailSample(tr, e.position);
            int maxPts = (trailLength < tr->count) ? trailLength : tr->count;
            if (maxPts > 1) {
                for (int ti = 0; ti < maxPts - 1; ti++) {
                    int idx0 = (tr->writeIdx - maxPts + ti + TRAIL_MAX_POINTS) % TRAIL_MAX_POINTS;
                    int idx1 = (idx0 + 1) % TRAIL_MAX_POINTS;
                    Vec3 s0 = {}, s1 = {};
                    if (W2S(tr->points[idx0], s0, W, H) && W2S(tr->points[idx1], s1, W, H)) {
                        extern bool trailFade;
                        float alpha = trailFade ? (float)(ti + 1) / (float)maxPts : 1.0f;
                        ImU32 tc;
                        if (rainbowMode && rainbowTrail) {
                            float hue = fmodf((float)ti / (float)maxPts + entOffset, 1.0f);
                            float r, g, b;
                            ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, r, g, b);
                            tc = IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),(int)(alpha*espTrailColor[3]*255));
                        } else {
                            tc = IM_COL32((int)(espTrailColor[0]*255),(int)(espTrailColor[1]*255),
                                         (int)(espTrailColor[2]*255),(int)(alpha*espTrailColor[3]*255));
                        }
                        dl->AddLine(ImVec2(s0.x, s0.y), ImVec2(s1.x, s1.y), tc, trailThickness);
                    }
                }
            }
        }

        if (aimEnabled && e.isPlayer) {
            float effectiveFov = aimFov;
            float fovOvr = _rcal_get_fov_override();
            if (fovOvr >= 0.0f) effectiveFov = fovOvr;

            extern int aimTargetMode;
            if (aimTargetMode == 1) {
                for (float zOff = 0.2f; zOff <= 1.5f; zOff += 0.1f) {
                    Vec3 scanWorld = {e.position.x, e.position.y, e.position.z + zOff};
                    Vec3 scanScr = {};
                    if (!W2S(scanWorld, scanScr, W, H)) continue;
                    float dx = scanScr.x - W / 2.0f;
                    float dy = scanScr.y - H / 2.0f;
                    float crossDist = sqrtf(dx * dx + dy * dy);
                    if (crossDist < effectiveFov && crossDist < bestAimDist) {
                        bestAimDist = crossDist;
                        bestAimX = scanScr.x;
                        bestAimY = scanScr.y;
                        hasAimTarget = true;
                    }
                }
            } else {
                Vec3 aimWorld;
                bool aimOn = false;
                Vec3 aimScr = {};
                if (skeletonAim && e.hasBones && e.headOnScreen) {
                    Vec3B hb = GetHeadBonePos(e.bones);
                    aimWorld = { hb.x, hb.y, hb.z };
                    aimOn = W2S(aimWorld, aimScr, W, H);
                }
                if (!aimOn) {
                    AimTarget at = GetAimPosition(e.instance, (float)e.position.x, (float)e.position.y, (float)e.position.z, hitboxSel);
                    aimWorld = {at.x, at.y, at.z};
                    aimOn = W2S(aimWorld, aimScr, W, H);
                }
                if (aimOn) {
                    float dx = (float)aimScr.x - W / 2.0f;
                    float dy = (float)aimScr.y - H / 2.0f;
                    float crossDist = sqrtf(dx * dx + dy * dy);
                    if (crossDist < effectiveFov && crossDist < bestAimDist) {
                        bestAimDist = crossDist;
                        bestAimX = (float)aimScr.x;
                        bestAimY = (float)aimScr.y;
                        hasAimTarget = true;
                    }
                }
            }
        }
    }

    if (aimEnabled && hasAimTarget) {
        dl->AddCircle(ImVec2(bestAimX, bestAimY), 6.0f, ColorToU32(aimbotTargetColor), 12, 2.0f);
        bool aimKeyPressed = false;
        extern int hotkeys_aimkey_val();
        if (hotkeys::aimkey > 0)
            aimKeyPressed = (GetAsyncKeyState(hotkeys::aimkey) & 0x8000) != 0;
        else
            aimKeyPressed = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        if (aimKeyPressed) {
            extern void aimbot(float x, float y);
            aimbot(bestAimX, bestAimY);
        }
    }

    if (fovCircle) {
        extern float AimFOV;
        dl->AddCircle(ImVec2((float)W/2, (float)H/2), AimFOV, GetFovColor(), 64, fovCircleThickness);
    }
    if (squareFov) {
        extern float AimFOV;
        float half = AimFOV;
        dl->AddRect(ImVec2(W/2.0f-half, H/2.0f-half), ImVec2(W/2.0f+half, H/2.0f+half), GetFovColor(), 0, 0, fovCircleThickness);
    }
    if (xhair) {
        float cx = W/2.0f, cy = H/2.0f, cs = crosshairSize;
        ImU32 xc = ColorToU32(crosshairColor);
        dl->AddLine(ImVec2(cx-cs, cy), ImVec2(cx+cs, cy), xc, 1.5f);
        dl->AddLine(ImVec2(cx, cy-cs), ImVec2(cx, cy+cs), xc, 1.5f);
    }


    {
        extern int g_weatherMode;
        if (g_weatherMode != WFX_NONE && g_projectionAddr) {
            g_wfxMode = g_weatherMode;
            Vec3 cam_w = QueryCameraOrigin();
            if (fabsf(cam_w.x) > 0.1f || fabsf(cam_w.y) > 0.1f) {
                float wdt = 1.f / 60.f;
                {
                    static DWORD s_wprev = 0;
                    DWORD wnow = GetTickCount();
                    if (s_wprev) {
                        float raw = (float)(wnow - s_wprev) / 1000.f;
                        if (raw > 0.f && raw < 0.25f) wdt = raw;
                    }
                    s_wprev = wnow;
                }
                WfxRender3D(cam_w, wdt, W, H, dl);
            }
        } else {
            g_wfxMode = WFX_NONE;
            g_wfxInited = false;
        }
    }

    // ═══ RADAR ═══
    extern bool  radarEnabled;
    extern float radarSize;
    extern float radarRange;
    extern int   radarCorner;
    extern float radarBgColor[4];
    extern float radarEnemyColor[4];
    extern float radarLocalColor[4];

    if (radarEnabled) {
        const float R = radarSize;
        const float M = 20.0f;  // margin from screen edge
        float rx, ry;
        switch (radarCorner) {
            case 0: rx = M;             ry = M;             break;  // TL
            case 1: rx = (float)W-R-M;  ry = M;             break;  // TR
            case 2: rx = M;             ry = (float)H-R-M;  break;  // BL
            default: rx = (float)W-R-M; ry = (float)H-R-M;  break;  // BR
        }
        const float cx = rx + R * 0.5f;
        const float cy = ry + R * 0.5f;

        // Panel bg with subtle border
        dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + R, ry + R), ColorToU32(radarBgColor), 8.0f);
        dl->AddRect       (ImVec2(rx, ry), ImVec2(rx + R, ry + R), IM_COL32(30, 30, 30, 220), 8.0f, 0, 1.0f);

        // Crosshair guides
        ImU32 kGuide = IM_COL32(80, 76, 70, 90);
        dl->AddLine(ImVec2(rx, cy), ImVec2(rx + R, cy), kGuide, 1.0f);
        dl->AddLine(ImVec2(cx, ry), ImVec2(cx, ry + R), kGuide, 1.0f);

        // Range ring
        dl->AddCircle(ImVec2(cx, cy), R * 0.5f - 4.0f, IM_COL32(60, 56, 50, 120), 48, 1.0f);

        // Local player dot in the middle
        dl->AddCircleFilled(ImVec2(cx, cy), 3.5f, ColorToU32(radarLocalColor));

        // Range legend (small text bottom-left of radar)
        char rangeTxt[24]; snprintf(rangeTxt, 24, "%.0fm", radarRange);
        dl->AddText(ImVec2(rx + 6, ry + R - 16), IM_COL32(120, 116, 108, 200), rangeTxt);

        // Enemy dots — world XY delta from camera, scaled to radar
        Vec3 cam = QueryCameraOrigin();
        const float scale = (R * 0.5f - 4.0f) / radarRange;
        ImU32 kEnemy = ColorToU32(radarEnemyColor);
        for (const auto& e : g_vertexBuffer) {
            if (!e.isPlayer) continue;
            float dx = e.position.x - cam.x;
            float dy = e.position.y - cam.y;
            float dd = sqrtf(dx*dx + dy*dy);
            if (dd > radarRange) continue;  // beyond range
            float px = cx + dx * scale;
            float py = cy - dy * scale;  // world +Y up on radar
            dl->AddCircleFilled(ImVec2(px, py), 3.0f, kEnemy);
            // Height indicator: thin bar right of dot showing z difference
            float dz = e.position.z - cam.z;
            if (dz > 1.5f)      dl->AddText(ImVec2(px + 4, py - 8), IM_COL32(200, 200, 200, 180), "^");
            else if (dz < -1.5f) dl->AddText(ImVec2(px + 4, py - 8), IM_COL32(200, 200, 200, 180), "v");
        }
    }

    // ═══ 3D BOX ═══
    extern bool  box3dEnabled;
    extern float box3dColor[4];
    extern float box3dThickness;
    extern float box3dHalfWidth;
    extern float box3dHeight;

    if (box3dEnabled) {
        ImU32 col3d = ColorToU32(box3dColor);
        const float hw = box3dHalfWidth;
        const float ht = box3dHeight;
        for (const auto& e : g_vertexBuffer) {
            if (!e.isPlayer) continue;
            const Vec3& p = e.position;
            // 8 corners of world-axis-aligned bounding box centred on feet
            Vec3 corners[8] = {
                {p.x-hw, p.y-hw, p.z},      {p.x+hw, p.y-hw, p.z},
                {p.x+hw, p.y+hw, p.z},      {p.x-hw, p.y+hw, p.z},
                {p.x-hw, p.y-hw, p.z+ht},   {p.x+hw, p.y-hw, p.z+ht},
                {p.x+hw, p.y+hw, p.z+ht},   {p.x-hw, p.y+hw, p.z+ht}
            };
            Vec3 scr[8];
            bool on[8];
            for (int i = 0; i < 8; i++) on[i] = W2S(corners[i], scr[i], W, H);
            // 12 edges: bottom rect, top rect, four verticals
            static const int edges[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}
            };
            for (int i = 0; i < 12; i++) {
                if (!on[edges[i][0]] || !on[edges[i][1]]) continue;
                dl->AddLine(ImVec2(scr[edges[i][0]].x, scr[edges[i][0]].y),
                            ImVec2(scr[edges[i][1]].x, scr[edges[i][1]].y),
                            col3d, box3dThickness);
            }
        }
    }

    // ═══ OFF-SCREEN ARROWS ═══
    extern bool  offscreenArrows;
    extern float offscreenColor[4];
    extern float offscreenRadius;

    if (offscreenArrows) {
        ImU32 arrCol = ColorToU32(offscreenColor);
        const float cx = W * 0.5f, cy = H * 0.5f;
        for (const auto& e : g_vertexBuffer) {
            if (!e.isPlayer) continue;
            if (e.onScreen) continue;
            // Direction from screen center to the projected point (if valid), otherwise
            // fall back to world-XY delta from camera.
            float dx, dy;
            if (e.screenPos.z > 0.0f) {
                dx = e.screenPos.x - cx;
                dy = e.screenPos.y - cy;
            } else {
                Vec3 cam = QueryCameraOrigin();
                dx = e.position.x - cam.x;
                dy = -(e.position.y - cam.y);  // world-y up → screen-y down
            }
            float len = sqrtf(dx*dx + dy*dy);
            if (len < 1e-3f) continue;
            dx /= len; dy /= len;
            // Behind camera → flip direction so arrow points to actual bearing
            if (e.screenPos.z <= 0.0f) { dx = -dx; dy = -dy; }
            float ax = cx + dx * offscreenRadius;
            float ay = cy + dy * offscreenRadius;
            // Triangle pointing outward
            float perpX = -dy, perpY = dx;
            const float sz = 9.0f, back = 7.0f;
            ImVec2 tip = { ax + dx * sz,          ay + dy * sz };
            ImVec2 l   = { ax - dx * back + perpX * 6, ay - dy * back + perpY * 6 };
            ImVec2 r   = { ax - dx * back - perpX * 6, ay - dy * back - perpY * 6 };
            dl->AddTriangleFilled(tip, l, r, arrCol);
            dl->AddTriangle      (tip, l, r, IM_COL32(0, 0, 0, 220), 1.0f);
        }
    }

    // ═══ CLOSEST-TARGET RING ═══
    extern bool  closestRing;
    extern float closestRingColor[4];

    const float cx = W * 0.5f, cy = H * 0.5f;
    const OverlayVertex* nearestOnScreen = nullptr;
    float bestScreenD2 = 1e18f;
    for (const auto& e : g_vertexBuffer) {
        if (!e.isPlayer || !e.onScreen) continue;
        float dx = e.screenPos.x - cx, dy = e.screenPos.y - cy;
        float d2 = dx*dx + dy*dy;
        if (d2 < bestScreenD2) { bestScreenD2 = d2; nearestOnScreen = &e; }
    }
    if (closestRing && nearestOnScreen) {
        float t = (float)(GetTickCount() % 1200) / 1200.0f;
        float pulse = 14.0f + 4.0f * sinf(t * 6.2831853f);
        dl->AddCircle(ImVec2(nearestOnScreen->screenPos.x, nearestOnScreen->screenPos.y),
                      pulse, ColorToU32(closestRingColor), 32, 1.5f);
    }

    // ═══ NEAREST-ENEMY VECTOR ═══
    extern bool  nearestVector;
    extern float nearestVectorColor[4];
    if (nearestVector) {
        Vec3 cam = QueryCameraOrigin();
        const OverlayVertex* nearestWorld = nullptr;
        float bestWorldD = 1e18f;
        for (const auto& e : g_vertexBuffer) {
            if (!e.isPlayer) continue;
            float dx = e.position.x - cam.x, dy = e.position.y - cam.y, dz = e.position.z - cam.z;
            float dd = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dd < bestWorldD) { bestWorldD = dd; nearestWorld = &e; }
        }
        if (nearestWorld && nearestWorld->onScreen) {
            ImU32 vc = ColorToU32(nearestVectorColor);
            dl->AddLine(ImVec2(cx, cy), ImVec2(nearestWorld->screenPos.x, nearestWorld->screenPos.y), vc, 1.0f);
        }
    }

    // ═══ ENEMY COUNT HUD ═══
    extern bool enemyCountHud;
    extern int  enemyCountCorner;
    if (enemyCountHud) {
        int visible = 0, alive = 0;
        for (const auto& e : g_vertexBuffer) {
            if (!e.isPlayer) continue;
            alive++;
            if (e.onScreen) visible++;
        }
        char buf[64];
        snprintf(buf, 64, "%d visible  %d alive", visible, alive);
        ImVec2 sz = ImGui::CalcTextSize(buf);
        const float pad = 8.0f, edge = 16.0f;
        float bx, by;
        switch (enemyCountCorner) {
            case 0: bx = edge;                          by = edge; break;
            case 1: bx = W - sz.x - pad*2 - edge;       by = edge; break;
            case 2: bx = edge;                          by = H - sz.y - pad*2 - edge; break;
            default: bx = W - sz.x - pad*2 - edge;      by = H - sz.y - pad*2 - edge; break;
        }
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + sz.x + pad*2, by + sz.y + pad*2),
                          IM_COL32(10, 10, 10, 200), 6.0f);
        dl->AddText(ImVec2(bx + pad, by + pad), IM_COL32(232, 230, 225, 230), buf);
    }

    // ═══ SESSION STATS HUD ═══
    extern bool sessionStatsHud;
    static DWORD s_sessionStart = 0;
    static int   s_peakPlayers = 0;
    static int   s_prevPlayers = 0;
    static int   s_inferredKills = 0;
    if (s_sessionStart == 0) s_sessionStart = GetTickCount();
    // Track peak + infer kills from drops in alive count
    {
        int alive = 0;
        for (const auto& e : g_vertexBuffer) if (e.isPlayer) alive++;
        if (alive > s_peakPlayers) s_peakPlayers = alive;
        if (alive < s_prevPlayers) s_inferredKills += (s_prevPlayers - alive);
        s_prevPlayers = alive;
    }
    if (sessionStatsHud) {
        DWORD uptimeSec = (GetTickCount() - s_sessionStart) / 1000;
        char buf[96];
        snprintf(buf, 96, "%02lu:%02lu  peak %d  kills %d", uptimeSec / 60, uptimeSec % 60, s_peakPlayers, s_inferredKills);
        ImVec2 sz = ImGui::CalcTextSize(buf);
        const float pad = 8.0f, edge = 16.0f;
        // Bottom-left by default (opposite of enemy count HUD which defaults TL)
        float bx = edge, by = H - sz.y - pad*2 - edge;
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + sz.x + pad*2, by + sz.y + pad*2),
                          IM_COL32(10, 10, 10, 200), 6.0f);
        dl->AddText(ImVec2(bx + pad, by + pad), IM_COL32(180, 176, 168, 220), buf);
    }

    // ═══ ROUND-CHANGE TOAST ═══
    extern bool roundToasts;
    static size_t s_prevCacheSize = 0;
    static DWORD  s_toastUntil = 0;
    {
        size_t nowSize = g_syncMap.size();
        // Round change heuristic: cache had significant contents and dropped near-zero
        if (s_prevCacheSize >= 3 && nowSize == 0) {
            s_toastUntil = GetTickCount() + 2500;
        }
        s_prevCacheSize = nowSize;
    }
    if (roundToasts && GetTickCount() < s_toastUntil) {
        const char* msg = "round reset";
        ImVec2 sz = ImGui::CalcTextSize(msg);
        float tx = W * 0.5f - sz.x * 0.5f;
        float ty = H * 0.16f;
        const float pad = 10.0f;
        dl->AddRectFilled(ImVec2(tx - pad, ty - pad), ImVec2(tx + sz.x + pad, ty + sz.y + pad),
                          IM_COL32(10, 10, 10, 220), 6.0f);
        dl->AddText(ImVec2(tx, ty), IM_COL32(201, 169, 110, 240), msg);
    }
}
