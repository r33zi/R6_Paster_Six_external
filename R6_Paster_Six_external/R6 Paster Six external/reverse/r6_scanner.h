#pragma once

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include "driver.h"
#include "offsets.h"

static std::vector<int> ParsePattern(const char* pat) {
    std::vector<int> v;
    const char* p = pat;
    while (*p) {
        while (*p==' ') ++p;
        if (!*p) break;
        if (p[0]=='?') { v.push_back(-1); ++p; if (*p=='?') ++p; }
        else { v.push_back((int)strtoul(p, const_cast<char**>(&p), 16)); }
    }
    return v;
}

static size_t ScanBufFirst(const uint8_t* buf, size_t sz, const std::vector<int>& pat) {
    if (pat.empty() || sz < pat.size()) return SIZE_MAX;
    size_t lim = sz - pat.size();
    for (size_t i = 0; i <= lim; ++i) {
        bool ok = true;
        for (size_t j = 0; j < pat.size(); ++j)
            if (pat[j] != -1 && buf[i+j] != (uint8_t)pat[j]) { ok = false; break; }
        if (ok) return i;
    }
    return SIZE_MAX;
}

struct TextSectionCache {
    std::vector<uint8_t> data;
    uint64_t textBase = 0, textSize = 0;
    bool valid = false;
};
static TextSectionCache g_textCache;

// Scan the cached .text section for a pattern and resolve a seven-byte
// RIP-relative instruction at the requested offset. Returns the absolute
// address referenced by its disp32 operand.
static uint64_t ScanSigRipRelative(const char* pattern, int matchOffsetInPattern, uint64_t moduleBase) {
    if (!g_textCache.valid || matchOffsetInPattern < 0) return 0;
    const uint8_t* t = g_textCache.data.data();
    size_t sz = (size_t)g_textCache.textSize;
    uint64_t tb = g_textCache.textBase;
    std::vector<int> pat = ParsePattern(pattern);
    size_t off = ScanBufFirst(t, sz, pat);
    if (off == SIZE_MAX) return 0;
    const size_t instructionOffset = off + (size_t)matchOffsetInPattern;
    if (instructionOffset > sz || sz - instructionOffset < 7) return 0;
    int32_t disp = 0;
    memcpy(&disp, t + instructionOffset + 3, sizeof(disp));
    uint64_t instrEnd = tb + off + matchOffsetInPattern + 7;
    uint64_t ptrAddr = instrEnd + disp;
    printf("[SIG] '%s' match at RVA 0x%llX -> ptr at 0x%llX\n",
        pattern, (unsigned long long)(tb + off - moduleBase),
        (unsigned long long)ptrAddr);
    return ptrAddr;
}

// GameManager / ViewData / CameraManager singleton pointers.
static uint64_t g_pGameManagerPtr = 0;
static uint64_t g_pViewDataPtr = 0;
static uint64_t g_pCameraManagerPtr = 0;
static uint64_t g_inGameFlagAddress = 0;

static bool ReadInGameFlag(bool& inGame) {
    if (!g_inGameFlagAddress) return false;
    static DWORD lastRead = 0;
    static bool cachedValue = false;
    static bool hasCachedValue = false;
    const DWORD now = GetTickCount();
    if (hasCachedValue && lastRead && now - lastRead < 100) {
        inGame = cachedValue;
        return true;
    }
    lastRead = now;
    uint32_t value = 0;
    if (driver->ReadProcessMemory(
            g_inGameFlagAddress, &value, sizeof(value)) != 0 || value > 1) {
        if (hasCachedValue) {
            inGame = cachedValue;
            return true;
        }
        return false;
    }
    cachedValue = value != 0;
    hasCachedValue = true;
    inGame = cachedValue;
    return true;
}

static bool ScanGamePointers(uint64_t moduleBase) {
    if (!g_textCache.valid) return false;

    g_pGameManagerPtr = ScanSigRipRelative(
        OFFSETS::GameManagerSignature, 0, moduleBase);
    if (!g_pGameManagerPtr)
        g_pGameManagerPtr = ScanSigRipRelative(
        "48 8B 05 ?? ?? ?? ?? 48 8B 80 ?? ?? ?? ?? 48 85 C0", 0, moduleBase);
    if (!g_pGameManagerPtr)
        g_pGameManagerPtr = ScanSigRipRelative(
            "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 01", 0, moduleBase);

    g_pViewDataPtr = ScanSigRipRelative(
        OFFSETS::ViewMatrixSignature, 0, moduleBase);
    if (!g_pViewDataPtr)
        g_pViewDataPtr = ScanSigRipRelative(
        "48 8B 05 ?? ?? ?? ?? 4A 8B 04 00 41 80 F9 FB", 0, moduleBase);
    if (!g_pViewDataPtr)
        g_pViewDataPtr = ScanSigRipRelative(
            "48 8B 05 ?? ?? ?? ?? 48 8B 88 ?? ?? ?? ?? 0F 28", 0, moduleBase);

    g_pCameraManagerPtr = ScanSigRipRelative(
        "48 8B 05 ?? ?? ?? ?? 48 8B 80 10 01 00 00 48 8B 00 48 85 C0 75 30 45 31 F6",
        0, moduleBase);
    if (!g_pCameraManagerPtr)
        g_pCameraManagerPtr = ScanSigRipRelative(
            "48 8B 0D ?? ?? ?? ?? 48 8B 89 10 01 00 00 48 8B 09 48 85 C9 75 10 48 89 D8",
            0, moduleBase);

    g_inGameFlagAddress = ScanSigRipRelative(
        OFFSETS::InGameFlagSignature, OFFSETS::InGameFlagStoreOffset,
        moduleBase);

    printf("[GAME-PTR] GameManager=0x%llX ViewData=0x%llX CameraManager=0x%llX InGameFlag=0x%llX\n",
        (unsigned long long)g_pGameManagerPtr,
        (unsigned long long)g_pViewDataPtr,
        (unsigned long long)g_pCameraManagerPtr,
        (unsigned long long)g_inGameFlagAddress);
    return g_pGameManagerPtr != 0;
}

struct PESection { char name[9]; uint64_t va, vsz; };

static std::vector<PESection> GetPESections(uint64_t base) {
    std::vector<PESection> s;
    IMAGE_DOS_HEADER dos={}; driver->ReadProcessMemory(base, &dos, sizeof(dos));
    if (dos.e_magic != 0x5A4D) return s;
    IMAGE_NT_HEADERS64 nt={}; driver->ReadProcessMemory(base+dos.e_lfanew, &nt, sizeof(nt));
    if (nt.Signature != 0x4550) return s;
    uint64_t off = base+dos.e_lfanew+sizeof(DWORD)+sizeof(IMAGE_FILE_HEADER)+nt.FileHeader.SizeOfOptionalHeader;
    for (int i=0; i<nt.FileHeader.NumberOfSections; i++) {
        IMAGE_SECTION_HEADER sh={}; driver->ReadProcessMemory(off+i*sizeof(sh), &sh, sizeof(sh));
        PESection p={}; memcpy(p.name,sh.Name,8); p.name[8]=0; p.va=sh.VirtualAddress; p.vsz=sh.Misc.VirtualSize;
        s.push_back(p);
    }
    printf("[SCAN] %zu PE sections\n", s.size());
    return s;
}

static bool CacheTextSection(uint64_t base, const std::vector<PESection>& secs) {
    for (auto& s : secs) {
        if (strncmp(s.name,".text",5)) continue;
        g_textCache.textBase = base + s.va; g_textCache.textSize = s.vsz;
        printf("[SCAN] .text VA=0x%llX sz=0x%llX\n",(unsigned long long)g_textCache.textBase,(unsigned long long)g_textCache.textSize);
        g_textCache.data.resize((size_t)s.vsz);
        const size_t CH = 0x100000; size_t done = 0;
        while (done < (size_t)s.vsz) {
            size_t rd = min(CH, (size_t)s.vsz - done);
            if (driver->ReadProcessMemory(g_textCache.textBase+done, g_textCache.data.data()+done, (uint32_t)rd) != 0)
                { g_textCache.valid=false; return false; }
            done += rd;
        }
        g_textCache.valid = true;
        printf("[SCAN] Cached %zu bytes\n", g_textCache.data.size());
        return true;
    }
    return false;
}

struct CallTarget { uint64_t callVA, targetVA, anchorVA; bool hasTestAlAl; };

static std::vector<CallTarget> FindEntityFunctionCalls(uint64_t moduleBase) {
    std::vector<CallTarget> res;
    if (!g_textCache.valid) return res;
    const uint8_t* t = g_textCache.data.data();
    size_t sz = (size_t)g_textCache.textSize;
    uint64_t tb = g_textCache.textBase;
    const uint8_t anc[] = {0xC7,0x05,0x00,0x00,0x01};
    printf("[ENTITY-SCAN] Scanning %zu bytes for anchor C7 05 00 00 01...\n", sz);
    int anchors = 0;
    for (size_t i = 0; i+5 <= sz; i++) {
        bool m = true;
        for (int j=0;j<5;j++) if(t[i+j]!=anc[j]){m=false;break;}
        if (!m) continue;
        anchors++;
        uint64_t aVA = tb + i;
        if (anchors<=5) printf("[ENTITY-SCAN]   Anchor #%d VA=0x%llX (RVA=0x%llX)\n",
            anchors,(unsigned long long)aVA,(unsigned long long)(aVA-moduleBase));
        size_t bt = 0x80, st = (i>bt)?(i-bt):0;
        for (size_t j=st; j+5<=i; j++) {
            if (t[j]!=0xE8) continue;
            int32_t r32 = *(int32_t*)&t[j+1];
            uint64_t cVA = tb+j, tgt = cVA+5+r32;
            if (tgt<moduleBase||tgt>moduleBase+0x20000000) continue;
            if (tgt<tb||tgt>=tb+sz) continue;
            bool ht = (j+7<sz && t[j+5]==0x84 && t[j+6]==0xC0);
            bool dup=false; for(auto&r:res) if(r.targetVA==tgt&&r.anchorVA==aVA){dup=true;break;}
            if(dup) continue;
            res.push_back({cVA,tgt,aVA,ht});
            if(res.size()<=20) printf("[ENTITY-SCAN]     CALL RVA=0x%llX -> RVA=0x%llX%s\n",
                (unsigned long long)(cVA-moduleBase),(unsigned long long)(tgt-moduleBase),
                ht?" [test al,al] <-- ENTITY FUNC":"");
        }
    }
    printf("[ENTITY-SCAN] %d anchors, %zu calls\n", anchors, res.size());
    std::sort(res.begin(),res.end(),[](auto&a,auto&b){return a.hasTestAlAl>b.hasTestAlAl;});
    return res;
}

static uint64_t ScanForViewTrans(uint64_t moduleBase, uint64_t moduleSize) {
    printf("[W2S] Scanning PAGE_READWRITE regions for ViewTranslation...\n");
    static const char* patterns[] = {
        OFFSETS::ViewAnchorSignature,
        "00 00 A0 40 00 00 A0 C0 00 00 00 00 00 00 00 00 "
        "CD CC 4C 3F 00 00 00 3F 00 00 80 3E",
        "CD CC 4C 3F 00 00 00 3F 00 00 80 3E 00 00 00 00 "
        "00 00 A0 40 00 00 A0 C0",
    };
    static const int patternOffsets[] = { 0x2A4, 0x2B4, 0x2C8 };
    int numPatterns = sizeof(patterns) / sizeof(patterns[0]);
    extern DWORD processID;
    HANDLE hP = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, processID);
    if (!hP) { printf("[W2S] OpenProcess failed\n"); return 0; }
    const size_t CH = 0x100000;
    std::vector<uint8_t> buf;
    size_t regs=0, bytes=0;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi={};
    std::vector<std::vector<int>> parsedPats;
    for (int i = 0; i < numPatterns; i++) parsedPats.push_back(ParsePattern(patterns[i]));
    while (VirtualQueryEx(hP,(LPCVOID)addr,&mbi,sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (mbi.State==MEM_COMMIT && (mbi.Protect==PAGE_READWRITE||mbi.Protect==PAGE_EXECUTE_READWRITE)
            && mbi.RegionSize>=64) {
            regs++;
            uint64_t rBase=(uint64_t)(uintptr_t)mbi.BaseAddress; size_t rSz=mbi.RegionSize;
            size_t done=0;
            while (done<rSz) {
                size_t rd=rSz-done; if(rd>CH)rd=CH;
                if(buf.size()<rd)buf.resize(rd);
                if(driver->ReadProcessMemory(rBase+done,buf.data(),(uint32_t)rd)!=0){done+=CH;continue;}
                for (int pi = 0; pi < numPatterns; pi++) {
                    size_t off=ScanBufFirst(buf.data(),rd,parsedPats[pi]);
                    if(off!=SIZE_MAX) {
                        uint64_t vt=rBase+done+off-patternOffsets[pi];
                        printf("[W2S] FOUND: vt=0x%llX pat#%d (%zu regions, %zu bytes)\n",
                            (unsigned long long)vt,pi,regs,bytes);
                        CloseHandle(hP); return vt;
                    }
                }
                bytes+=rd;
                size_t adv=(rd>64)?(rd-63):rd;
                done+=adv;
            }
        }
        if(next<=addr)break; addr=next;
    }
    CloseHandle(hP);
    printf("[W2S] NOT FOUND (%zu regions, %zu bytes)\n",regs,bytes);
    return 0;
}


struct SkelXrefInfo {
    uint64_t skelFuncVA;
    uint32_t compIdxOff;
    uint32_t compArrOff;
    bool valid;
};
static SkelXrefInfo g_SkelXref = {};

struct RtFunc { uint32_t begin, end, unwind; };
static std::vector<RtFunc> g_pdataEntries;
static bool g_pdataLoaded = false;

static bool LoadPdata(uint64_t base) {
    if (g_pdataLoaded) return !g_pdataEntries.empty();
    g_pdataLoaded = true;
    IMAGE_DOS_HEADER dos={}; driver->ReadProcessMemory(base, &dos, sizeof(dos));
    if (dos.e_magic != 0x5A4D) return false;
    IMAGE_NT_HEADERS64 nt={}; driver->ReadProcessMemory(base+dos.e_lfanew, &nt, sizeof(nt));
    if (nt.Signature != 0x4550) return false;
    uint32_t pdataRVA = nt.OptionalHeader.DataDirectory[3].VirtualAddress;
    uint32_t pdataSize = nt.OptionalHeader.DataDirectory[3].Size;
    if (!pdataRVA || !pdataSize) return false;
    int count = pdataSize / 12;
    g_pdataEntries.resize(count);
    const size_t CHUNK = 0x100000;
    size_t done = 0;
    while (done < (size_t)pdataSize) {
        size_t rd = pdataSize - done; if (rd > CHUNK) rd = CHUNK;
        driver->ReadProcessMemory(base + pdataRVA + done, ((uint8_t*)g_pdataEntries.data()) + done, (uint32_t)rd);
        done += rd;
    }
    printf("[SKEL-SCAN] Loaded %d .pdata entries\n", count);
    return true;
}

static bool LookupFuncBounds(uint64_t moduleBase, uint32_t rva, uint32_t& outBegin, uint32_t& outEnd) {
    int lo = 0, hi = (int)g_pdataEntries.size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (rva < g_pdataEntries[mid].begin) hi = mid - 1;
        else if (rva >= g_pdataEntries[mid].end) lo = mid + 1;
        else { outBegin = g_pdataEntries[mid].begin; outEnd = g_pdataEntries[mid].end; return true; }
    }
    for (auto& rf : g_pdataEntries) {
        if (rva >= rf.begin && rva < rf.end) {
            outBegin = rf.begin; outEnd = rf.end; return true;
        }
    }
    return false;
}

// Resolve the configured Actor_caller signature to the start of its owning
// function. This is only a last-resort candidate: unlike ActorPatchRva and the
// call-site scan, a function entry does not guarantee that RCX is an actor.
static uint64_t FindConfiguredActorFunction(uint64_t moduleBase) {
    if (!g_textCache.valid) return 0;

    const std::vector<int> pattern = ParsePattern(OFFSETS::ActorCallerSignature);
    const size_t match = ScanBufFirst(g_textCache.data.data(),
        (size_t)g_textCache.textSize, pattern);
    if (match == SIZE_MAX) {
        printf("[ENTITY-SCAN] Configured Actor_caller signature not found\n");
        return 0;
    }

    const uint64_t matchVA = g_textCache.textBase + match;
    uint32_t begin = 0, end = 0;
    if (!LoadPdata(moduleBase) ||
        !LookupFuncBounds(moduleBase, (uint32_t)(matchVA - moduleBase), begin, end)) {
        printf("[ENTITY-SCAN] Actor_caller matched at RVA 0x%llX but has no function bounds\n",
            (unsigned long long)(matchVA - moduleBase));
        return 0;
    }

    const uint64_t functionVA = moduleBase + begin;
    printf("[ENTITY-SCAN] Actor_caller matched at RVA 0x%llX -> fallback function RVA 0x%X\n",
        (unsigned long long)(matchVA - moduleBase), begin);
    return functionVA;
}

static bool ScanSkelXref(uint64_t moduleBase) {
    if (!g_textCache.valid) return false;
    const uint8_t* t = g_textCache.data.data();
    size_t sz = (size_t)g_textCache.textSize;
    uint64_t tb = g_textCache.textBase;
    LoadPdata(moduleBase);
    printf("[SKEL-SCAN] Scanning %zu bytes for skeleton xref (v2)...\n", sz);
    int candidates = 0;
    for (size_t i = 16; i + 50 < sz; i++) {
        if (t[i] != 0x48 || t[i+1] != 0x85 || t[i+2] != 0xC9) continue;
        int afterTest = (int)i + 3;
        if (t[afterTest] != 0x74 && !(t[afterTest] == 0x0F && t[afterTest+1] == 0x84)) continue;
        bool foundScale8 = false;
        int scale8End = -1;
        int scale8Pos = -1;
        for (int back = 4; back <= 10; back++) {
            int pos = (int)i - back;
            if (pos < 1) continue;
            if ((t[pos] & 0xF0) == 0x40 && t[pos+1] == 0x8B && (t[pos+2] & 0xC7) == 0x04) {
                uint8_t sib = t[pos+3];
                if ((sib & 0xC0) == 0xC0) {
                    foundScale8 = true;
                    scale8End = pos + 4;
                    scale8Pos = pos;
                    break;
                }
            }
        }
        if (!foundScale8) continue;
        int jzSkip = 2;
        if (t[afterTest] == 0x0F) jzSkip = 6;
        int callPos = -1;
        for (int j = afterTest + jzSkip; j < afterTest + 40 && j + 5 < (int)sz; j++) {
            if (t[j] == 0xE8) {
                int32_t rel = *(int32_t*)&t[j+1];
                uint64_t target = tb + j + 5 + (int64_t)rel;
                if (target < moduleBase || target > moduleBase + 0x20000000) continue;
                uint32_t targetRVA = (uint32_t)(target - moduleBase);
                uint32_t fb = 0, fe = 0;
                bool hasBounds = LookupFuncBounds(moduleBase, targetRVA, fb, fe);
                uint32_t funcSize = hasBounds ? (fe - fb) : 0;
                if (hasBounds && funcSize < 0x500) continue;
                callPos = j;
                break;
            }
        }
        if (callPos < 0) continue;
        uint32_t compIdxOff = 0;
        int compIdxPos = -1;
        bool foundIdx = false;
        int searchBack = (int)scale8Pos - 30;
        if (searchBack < 1) searchBack = 1;
        // Try to find compIdx (movzx byte ptr [reg+disp32]) — optional in newer builds.
        for (int j = searchBack; j < scale8Pos; j++) {
            if (t[j] != 0x0F || t[j+1] != 0xB6) continue;
            int mpos = j + 2;
            uint8_t modrm = t[mpos];
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm = modrm & 7;
            if (mod != 2) continue;
            int dpos = mpos + 1;
            if (rm == 4) dpos++;
            if (dpos + 4 > (int)sz) continue;
            uint32_t off = *(uint32_t*)&t[dpos];
            if (off >= 0x100 && off <= 0x2FF) {
                compIdxOff = off;
                compIdxPos = j;
                foundIdx = true;
                break;
            }
        }
        // compIdx is optional — newer builds extract the index from a register
        // shift (e.g., shr rax, 0x20) instead of reading a byte tag.
        uint32_t compArrOff = 0;
        bool foundArr = false;
        // Search for compArr (REX.W mov [reg+disp32]) between scale8 and 40 bytes before.
        // In newer builds, compArr can be at large offsets like +0x370.
        int arrSearchStart = foundIdx ? (int)compIdxPos + 1 : searchBack;
        int arrSearchEnd = scale8Pos;
        for (int j = arrSearchStart; j < arrSearchEnd; j++) {
            if (!((t[j] & 0xF0) == 0x40 && (t[j] & 0x08))) continue;
            if (t[j+1] != 0x8B) continue;
            uint8_t modrm = t[j+2];
            if ((modrm & 0xC7) == 0x04) continue;
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm = modrm & 7;
            if (mod != 2) continue;
            int dpos = j + 3;
            if (rm == 4) dpos++;
            if (dpos + 4 > (int)sz) continue;
            uint32_t off = *(uint32_t*)&t[dpos];
            // Extended range: 0x40..0x400 covers both old (0xC0) and new (0x370) builds.
            if (off >= 0x40 && off <= 0x400 && off != compIdxOff) {
                compArrOff = off;
                foundArr = true;
                // Don't break — keep searching to find the LAST match (closest to scale8),
                // which is more likely the actual compArr access.
            }
        }
        if (!foundArr) continue;
        int32_t callRel = *(int32_t*)&t[callPos+1];
        uint64_t skelFunc = tb + callPos + 5 + (int64_t)callRel;
        uint32_t skelRVA = (uint32_t)(skelFunc - moduleBase);
        uint32_t fb2=0, fe2=0;
        bool hb2 = LookupFuncBounds(moduleBase, skelRVA, fb2, fe2);
        candidates++;
        printf("[SKEL-SCAN] #%d: compIdx=+0x%X compArr=+0x%X skel=RVA 0x%X",
            candidates, compIdxOff, compArrOff, skelRVA);
        if (hb2) printf(" funcSz=0x%X", fe2-fb2);
        printf(" xref=RVA 0x%llX\n", (unsigned long long)(tb + i - moduleBase));
        g_SkelXref.skelFuncVA = skelFunc;
        g_SkelXref.compIdxOff = compIdxOff;
        g_SkelXref.compArrOff = compArrOff;
        g_SkelXref.valid = true;
        break;
    }
    if (g_SkelXref.valid) {
        printf("[SKEL-SCAN] FOUND: compIdx=+0x%X compArr=+0x%X skelFunc=0x%llX\n",
            g_SkelXref.compIdxOff, g_SkelXref.compArrOff, (unsigned long long)g_SkelXref.skelFuncVA);
    } else {
        printf("[SKEL-SCAN] NOT FOUND (%d candidates checked)\n", candidates);
    }
    return g_SkelXref.valid;
}

struct SidewardsInfo {
    uint64_t addr;
    float origValue;
    bool found;
    bool patched;
};
static SidewardsInfo g_Sidewards = {};

static bool ScanSidewards() {
    printf("[SIDEWARDS] Scanning heap for sidewards value...\n");
    g_Sidewards.found = false;
    g_Sidewards.patched = false;
    const uint8_t pattern[] = { 0x66, 0x66, 0x26, 0x3F, 0x33, 0x33, 0xB3, 0x3E, 0x00, 0x00 };
    extern DWORD processID;
    HANDLE hP = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (!hP) return false;
    const size_t CH = 0x100000;
    std::vector<uint8_t> buf(CH);
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi = {};
    while (VirtualQueryEx(hP, (LPCVOID)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && 
            (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_EXECUTE_READWRITE) &&
            mbi.RegionSize >= sizeof(pattern)) {
            uint64_t rBase = (uint64_t)(uintptr_t)mbi.BaseAddress;
            size_t rSz = mbi.RegionSize;
            size_t done = 0;
            while (done < rSz) {
                size_t rd = rSz - done; if (rd > CH) rd = CH;
                if (driver->ReadProcessMemory(rBase + done, buf.data(), (uint32_t)rd) != 0) { done += CH; continue; }
                for (size_t j = 0; j + sizeof(pattern) <= rd; j++) {
                    bool match = true;
                    for (size_t k = 0; k < sizeof(pattern); k++) {
                        if (buf[j + k] != pattern[k]) { match = false; break; }
                    }
                    if (match) {
                        g_Sidewards.addr = rBase + done + j;
                        g_Sidewards.origValue = *(float*)&buf[j];
                        g_Sidewards.found = true;
                        printf("[SIDEWARDS] FOUND at 0x%llX (value=%.4f)\n",
                            (unsigned long long)g_Sidewards.addr, g_Sidewards.origValue);
                        CloseHandle(hP);
                        return true;
                    }
                }
                done += (rd > sizeof(pattern)) ? (rd - sizeof(pattern) + 1) : rd;
            }
        }
        if (next <= addr) break;
        addr = next;
    }
    CloseHandle(hP);
    printf("[SIDEWARDS] NOT FOUND\n");
    return false;
}

static void SetSidewardsValue(float value) {
    if (!g_Sidewards.found) return;
    uint8_t buf[4];
    memcpy(buf, &value, 4);
    driver->WriteProcessMemory((PVOID)buf, (PVOID)g_Sidewards.addr, 4);
    g_Sidewards.patched = true;
}

static void RestoreSidewards() {
    if (!g_Sidewards.found || !g_Sidewards.patched) return;
    uint8_t buf[4];
    memcpy(buf, &g_Sidewards.origValue, 4);
    driver->WriteProcessMemory((PVOID)buf, (PVOID)g_Sidewards.addr, 4);
    g_Sidewards.patched = false;
}

struct BoneSigInfo {
    uint64_t addr;          // VA of the sig match
    uint32_t boneTransOff;  // offset of position vec3 within each bone entry (0x30)
    uint32_t boneWOff;      // offset of 1.0f (w component) within each bone entry (0x3C)
    bool    valid;
};
static BoneSigInfo g_BoneSig = {};

// ScanBoneSigs — finds the skeleton bone-entry layout signatures.
//
// The signature variants cover X/Y/Z stores in both observed instruction
// layouts. Each variant identifies code that writes a bone position component
// into the skeleton array and encodes:
//   - A mov [reg+<componentOff>], reg instruction
//   - A mov dword [reg+0x3C], 1.0f instruction (writes the w=1.0f component)
//
// A component store at +0x38 is Z, not the start of another vec3: +0x3C is
// immediately overwritten with W=1.0f. Both variants therefore prove the
// translation vec3 begins at +0x30 in a 0x40-stride entry.
//
// We record the translation offset so the bone readers know exactly where
// the position lives in each bone entry. No backward instruction scanning —
// the sigs tell us the layout directly.
static bool ScanBoneSigs(uint64_t moduleBase) {
    if (!g_textCache.valid) return false;
    g_BoneSig = {};
    const uint8_t* t = g_textCache.data.data();
    size_t sz = (size_t)g_textCache.textSize;
    uint64_t tb = g_textCache.textBase;
    printf("[BONE-SIG] Scanning %zu bytes for skeleton bone-entry sigs...\n", sz);

    struct SigPat { const char* pattern; const char* name; uint32_t componentOff; };
    static const SigPat pats[] = {
        { OFFSETS::BoneZStoreSignature,        "Z-store",         0x38 },
        { OFFSETS::BoneZStoreSignatureCompact, "Z-store compact", 0x38 },
        { OFFSETS::BoneYStoreSignature,        "Y-store",         0x34 },
        { OFFSETS::BoneYStoreSignatureCompact, "Y-store compact", 0x34 },
        { OFFSETS::BoneXStoreSignature,        "X-store",         0x30 },
        { OFFSETS::BoneXStoreSignatureCompact, "X-store compact", 0x30 },
    };

    std::vector<int> parsed[sizeof(pats) / sizeof(pats[0])];
    for (size_t p = 0; p < sizeof(pats) / sizeof(pats[0]); ++p)
        parsed[p] = ParsePattern(pats[p].pattern);

    for (size_t i = 0; i < sz; i++) {
        int pi = -1;
        for (size_t p = 0; p < sizeof(pats) / sizeof(pats[0]); p++) {
            if (parsed[p].empty() || i + parsed[p].size() > sz) continue;
            bool match = true;
            for (size_t j = 0; j < parsed[p].size(); j++) {
                if (parsed[p][j] != -1 && t[i+j] != (uint8_t)parsed[p][j]) { match = false; break; }
            }
            if (match) { pi = (int)p; break; }
        }
        if (pi < 0) continue;

        const SigPat& hit = pats[pi];
        uint64_t hitVA = tb + i;
        printf("[BONE-SIG] %s match at RVA 0x%llX (component=0x%X, transOff=0x30, wOff=0x3C)\n",
            hit.name, (unsigned long long)(hitVA - moduleBase), hit.componentOff);

        // The sig directly tells us the bone-entry translation offset.
        // The 1.0f store at +0x3C confirms a 0x40-stride entry with position
        // at +transOff and w=1.0f at +0x3C.
        g_BoneSig.addr         = hitVA;
        g_BoneSig.boneTransOff = 0x30;
        g_BoneSig.boneWOff     = 0x3C;
        g_BoneSig.valid        = true;

        printf("[BONE-SIG] DONE: boneTransOff=+0x%X boneWOff=+0x%X\n",
            g_BoneSig.boneTransOff, g_BoneSig.boneWOff);
        return true;
    }

    printf("[BONE-SIG] NOT FOUND\n");
    return false;
}
