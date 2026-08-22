#include "pasterx.h"
#include "driver.h"
#include "d3d9_x.h"
#include "xor.hpp"
#include <dwmapi.h>
#include <vector>
#include <random>
#include <atomic>
#include "Keybind.h"
#include "color.hpp"
#include "json.hpp"
#include "utils.hpp"
#include "offsets.h"
#include "xstring"
#include "r6_entities.h"
#include "skeleton_emu.h"
#include "antitamper.h"
#include "auth.hpp"

std::atomic<bool> g_manualInMatch{false};


float BOG_TO_GRD(float BOG) { return (180.f / (float)M_PI) * BOG; }
float GRD_TO_BOG(float GRD) { return ((float)M_PI / 180.f) * GRD; }

bool ShowMenu = true;
bool Esp_box = false;
bool cornered_box = false;
bool Esp_line = false;
bool Aimbot = false;
bool playerTrail = false;
bool Esp_Distance = false;
bool fovcircle = false;
bool square_fov = false;
bool fovcirclefilled = false;
bool fillbox = false;
bool lineheadesp = false;
bool crosshair = false;
bool Esp_skeleton = false;
bool skeletonAim = false;
float espSkeletonColor[4] = { 1.0f, 1.0f, 1.0f, 0.85f };
float skeletonThickness = 1.5f;
bool rainbowMode = false;
bool rainbowBox = false;
bool rainbowTrail = false;
bool rainbowFov = false;
bool rainbowSnaplines = false;

float espBoxColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
float espSnaplineColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
float espTrailColor[4] = { 0.0f, 1.0f, 1.0f, 0.7f };
float espDistanceColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float fovCircleColor[4] = { 1.0f, 1.0f, 1.0f, 0.7f };
float crosshairColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
float aimbotTargetColor[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
float filledBoxColor[4] = { 1.0f, 0.0f, 0.0f, 0.15f };

float boxThickness = 1.5f;
float snaplineThickness = 1.0f;
float trailThickness = 1.5f;
int trailLength = 60;
float fovCircleThickness = 1.0f;
float crosshairSize = 8.0f;
bool sidewardsEnabled = false;
float sidewardsValue = 10.0f;
bool shaderLabelOverlay = false;
bool shaderIconOverlay = true;
bool depthVisualization = false;
int snaplineOrigin = 2;  
int trailUpdateMs = 33;
bool trailFade = true;
bool espDeathCheck = true;
bool espTeamCheck = true;
int g_weatherMode = WFX_NONE;
int aimTargetMode = 0;
float g_weatherIntensity = 1.0f;
float g_weatherWind = 0.3f;


float ChangerFOV = 80;
ImFont* m_pFont;
ImFont* m_pFontDisplay;

// ═══ new feature globals ═════════════════════════════════════════════
// Radar
bool  radarEnabled     = false;
float radarSize        = 220.0f;
float radarRange       = 60.0f;                                 // meters
int   radarCorner      = 1;                                     // 0=TL 1=TR 2=BL 3=BR
float radarBgColor[4]  = { 0.04f, 0.04f, 0.04f, 0.80f };
float radarEnemyColor[4] = { 0.85f, 0.30f, 0.30f, 1.0f };
float radarLocalColor[4] = { 0.79f, 0.66f, 0.43f, 1.0f };
// Streamer mode — hides wordmark; menu title becomes "system"
bool  streamerMode     = false;

// 3D Box ESP — world-axis-aligned bounding box (12 edges)
bool  box3dEnabled     = false;
float box3dColor[4]    = { 0.79f, 0.66f, 0.43f, 1.0f };
float box3dThickness   = 1.4f;
float box3dHalfWidth   = 0.35f;   // meters — R6 char capsule radius
float box3dHeight      = 1.75f;   // meters — R6 stand height

// Off-screen enemy arrows — points toward enemies not currently on screen
bool  offscreenArrows  = false;
float offscreenColor[4]= { 0.85f, 0.30f, 0.30f, 0.9f };
float offscreenRadius  = 220.0f;  // pixels from screen center

// Closest-target ring — pulsing highlight on the nearest enemy to crosshair
bool  closestRing      = false;
float closestRingColor[4] = { 0.79f, 0.66f, 0.43f, 1.0f };

// ═══ R6-appropriate features (no jumping — this is Siege) ═════════════
// Distance-based ESP fade — distant players don't visually spam
bool  distanceFade      = false;
float distanceFadeNear  = 30.0f;   // ≤ this = full alpha
float distanceFadeFar   = 200.0f;  // ≥ this = 25% alpha

// Priority highlight — closest enemy to crosshair gets distinctive color
bool  priorityHighlight = false;
float priorityColor[4]  = { 0.99f, 0.42f, 0.42f, 1.0f };

// Low-HP prioritization — lower health = brighter/hotter tint
bool  lowHpPriority     = false;
float lowHpColor[4]     = { 1.0f, 0.85f, 0.20f, 1.0f };

// DBNO / dead differentiation — recently-dead players drawn dimmer
bool  showDbno          = false;
float dbnoColor[4]      = { 0.45f, 0.45f, 0.45f, 0.6f };

// Enemy counter HUD — visible / alive corner text
bool  enemyCountHud     = false;
int   enemyCountCorner  = 0;                                 // 0=TL 1=TR 2=BL 3=BR

// Round change detection — brief toast when the sync buffer clears
bool  roundToasts       = false;

// Session stats HUD — uptime + peak players + inferred kills
bool  sessionStatsHud   = false;

// Multi-bone aim — tries head, falls back to chest → pelvis if head not on-screen
bool  multiBoneAim      = false;

// Sticky aim — once locked on a target, keeps aiming at the SAME entity
// even if a closer one appears (better for tracking single targets)
bool  stickyAim         = false;

// Aim curve — 0=linear (default), 1=ease-out (starts fast, slows), 2=ease-in (slow start, snaps at end)
int   aimCurve          = 0;
static const char* aimCurveNames[] = { "Linear", "Ease-out", "Ease-in" };

// Nearest-enemy vector — always draws a subtle line from crosshair toward the closest enemy
bool  nearestVector     = false;
float nearestVectorColor[4] = { 0.79f, 0.66f, 0.43f, 0.55f };

// Corpse/DBNO highlight — draw downed players so you know who was already hit
bool  corpseEsp         = false;
float corpseColor[4]    = { 0.55f, 0.32f, 0.32f, 0.7f };
float smooth = 5.0f;
static int VisDist = 250;
float AimFOV = 150.0f;
static int aimkey;
static int hitbox;
static int hitboxpos = 0;

DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
DWORD_PTR PlayerState;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Persistentlevel;
uintptr_t PlayerCameraManager;
Vector3 localactorpos;
uint64_t TargetPawn;
int localplayerID;

RECT GameRect = { NULL };
D3DPRESENT_PARAMETERS d3dpp;
DWORD ScreenCenterX;
DWORD ScreenCenterY;
Vector3 LocalRelativeLocation;

struct FBoxSphereBounds {
    struct Vector3 Origin;
    struct Vector3 BoxExtent;
    double SphereRadius;
};

static void xCreateWindow();
static void xInitD3d();
static void xMainLoop();
static void xShutdown();
void SubmitDrawCalls();
static LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND Window = NULL;
IDirect3D9Ex* p_Object = NULL;
static LPDIRECT3DDEVICE9 D3dDevice = NULL;
static LPDIRECT3DVERTEXBUFFER9 TriBuf = NULL;

typedef struct { float X, Y, Z; } FVector;
typedef struct { float X, Y; } FVector2D;

inline void K2_DrawLineXD(Vector3 ScreenPositionA, Vector3 ScreenPositionB, float Thickness, ImColor RenderColor) {
    ImGui::GetOverlayDrawList()->AddLine(ImVec2(ScreenPositionA.x, ScreenPositionA.y), ImVec2(ScreenPositionB.x, ScreenPositionB.y), RenderColor, Thickness);
}

struct HandleDisposer {
    using pointer = HANDLE;
    void operator()(HANDLE handle) const {
        if (handle != NULL || handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
    }
};
using unique_handle = std::unique_ptr<HANDLE, HandleDisposer>;

static std::uint32_t _GetProcessId(std::string process_name) {
    PROCESSENTRY32 processentry;
    const unique_handle snapshot_handle(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snapshot_handle.get() == INVALID_HANDLE_VALUE) return 0;
    processentry.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(snapshot_handle.get(), &processentry)) return 0;
    do {
        if (process_name.compare(processentry.szExeFile) == 0)
            return processentry.th32ProcessID;
    } while (Process32Next(snapshot_handle.get(), &processentry) == TRUE);
    return 0;
}

static DWORD g_toastStartTick = 0;
static bool  g_toastState = false;
static constexpr DWORD TOAST_DURATION_MS = 1500;

DWORD Menuthread(LPVOID in) {
    while (1) {
        if (MouseController::GetAsyncKeyState(VK_INSERT) & 1)
            ShowMenu = !ShowMenu;
        if (MouseController::GetAsyncKeyState(VK_F2) & 1) {
            bool cur = g_manualInMatch.load(std::memory_order_acquire);
            bool next = !cur;
            g_manualInMatch.store(next, std::memory_order_release);
            g_toastState = next;
            g_toastStartTick = GetTickCount();
        }
        Sleep(1);
    }
}


static float g_rainbowHue = 0.0f;
static DWORD g_lastRainbowTick = 0;

static ImU32 GetRainbowColor(float alpha = 1.0f, float offset = 0.0f) {
    float h = fmodf(g_rainbowHue + offset, 1.0f);
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, r, g, b);
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(alpha * 255));
}

static void UpdateRainbow() {
    DWORD now = GetTickCount();
    float dt = (now - g_lastRainbowTick) / 1000.0f;
    g_lastRainbowTick = now;
    g_rainbowHue += dt * 0.3f;
    if (g_rainbowHue > 1.0f) g_rainbowHue -= 1.0f;
}

static ImU32 ColorToU32(const float* col) {
    return IM_COL32((int)(col[0]*255), (int)(col[1]*255), (int)(col[2]*255), (int)(col[3]*255));
}

static ImU32 GetBoxColor(float offset = 0.0f) {
    if (rainbowMode && rainbowBox) return GetRainbowColor(espBoxColor[3], offset);
    return ColorToU32(espBoxColor);
}

static ImU32 GetSnaplineColor(float offset = 0.0f) {
    if (rainbowMode && rainbowSnaplines) return GetRainbowColor(espSnaplineColor[3], offset);
    return ColorToU32(espSnaplineColor);
}

static ImU32 GetTrailColor(float offset = 0.0f) {
    if (rainbowMode && rainbowTrail) return GetRainbowColor(espTrailColor[3], offset);
    return ColorToU32(espTrailColor);
}

static ImU32 GetFovColor() {
    if (rainbowMode && rainbowFov) return GetRainbowColor(fovCircleColor[3]);
    return ColorToU32(fovCircleColor);
}

static std::string ReadHiddenLine() {
    std::string value;
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    bool restore = GetConsoleMode(input, &mode) != FALSE;
    if (restore) SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT);
    std::getline(std::cin, value);
    if (restore) SetConsoleMode(input, mode);
    printf("\n");
    return value;
}

static bool RunAuthentication() {
    AuthFusion auth;
    const std::string hwid = AuthFusion::get_hwid();

    for (int attempt = 1; attempt <= 3; attempt++) {
        std::string username, password;
        printf("[*] Username: ");
        std::getline(std::cin, username);
        printf("[*] Password: ");
        password = ReadHiddenLine();

        printf("[.] Authenticating...\n");
        AuthFusion::result res = auth.login(username, password, hwid);
        if (res.success) {
            printf("[+] Welcome %s\n", res.username.empty() ? username.c_str() : res.username.c_str());
            if (!res.expiry.empty()) printf("[+] Subscription expires: %s\n", res.expiry.c_str());
            return true;
        }

        printf("[!] Login failed: %s\n", res.message.empty() ? "unknown error" : res.message.c_str());
        if (attempt < 3) printf("[.] Attempts left: %d\n\n", 3 - attempt);
    }
    return false;
}

int main(int argc, const char* argv[]) {
    px33_init_antitamper();
    system("color 3");
    system("cls");
    SetConsoleTitleA("Oxium R6 external");
    printf("========================================\n");
    printf("  Oxium R6 external\n");
    printf("========================================\n\n");
    printf("[*] HWID: %s\n\n", AuthFusion::get_hwid().c_str());
    if (!RunAuthentication()) {
        printf("[!] Authentication failed.\n");
        system("pause");
        return 1;
    }
    printf("\n");
    printf("[+] Initializing mouse controller...\n");
    MouseController::Init();
    printf("[+] Mouse controller OK\n");
    CreateThread(NULL, NULL, Menuthread, NULL, NULL, NULL);
    printf("[+] Menu thread started\n");

    printf("\n[*] Step 1: Looking for game window...\n");
    const char* windowTitles[] = {
        "Rainbow Six", "R6Game", "RainbowSix",
        "Tom Clancy's Rainbow Six  Siege", "Tom Clancy's Rainbow Six Siege", NULL
    };
    int searchAttempts = 0;
    while (hwnd == NULL) {
        for (int i = 0; windowTitles[i] != NULL; i++) {
            hwnd = FindWindowA(0, windowTitles[i]);
            if (hwnd != NULL) { printf("[+] Found: '%s'\n", windowTitles[i]); break; }
        }
        if (hwnd == NULL) {
            hwnd = FindWindowA("UnrealWindow", NULL);
            if (hwnd) { char title[256] = {0}; GetWindowTextA(hwnd, title, 255); printf("[+] UnrealWindow: '%s'\n", title); }
        }
        if (hwnd == NULL) {
            searchAttempts++;
            if (searchAttempts % 10 == 1) printf("[.] Waiting for game... (attempt %d)\n", searchAttempts);
            Sleep(500);
        }
    }

    printf("\n[*] Step 2: Finding game process...\n");
    const char* processNames[] = { "RainbowSix.exe", "rainbowsix.exe", NULL };
    for (int attempt = 0; attempt < 30 && processID == 0; attempt++) {
        for (int i = 0; processNames[i] != NULL; i++) {
            processID = _GetProcessId(processNames[i]);
            if (processID != 0) { printf("[+] Found: '%s' (PID=%lu)\n", processNames[i], processID); break; }
        }
        if (processID == 0) { if (attempt % 5 == 0) printf("[.] Waiting... (%d/30)\n", attempt + 1); Sleep(1000); }
    }
    if (processID == 0) { printf("[!] Game not found!\n"); system("pause"); return 1; }

    printf("\n[*] Step 3: Initializing driver...\n");
    uint64_t module_size = 0;
    if (driver->Init(FALSE)) {
        printf("[+] Driver OK\n");
        driver->Attach(processID);
        base_address = driver->GetModuleBase(L"RainbowSix.exe", &module_size);
        printf("[+] Base: 0x%llX Size: 0x%llX\n", (unsigned long long)base_address, (unsigned long long)module_size);
        if (base_address == 0) {
            base_address = driver->GetModuleBase(L"", &module_size);
            printf("[+] Fallback base: 0x%llX\n", (unsigned long long)base_address);
        }
        if (base_address == 0) { printf("[!] Base address failed!\n"); system("pause"); return 1; }
        uint16_t dosHeader = read<uint16_t>(base_address);
        printf("[+] DOS: 0x%04X %s\n", (unsigned)dosHeader, dosHeader == 0x5A4D ? "(OK)" : "(BAD)");
    } else {
        printf("[!] Driver FAILED!\n"); system("pause"); return 1;
    }

    printf("\n[*] Step 3.5: Scanning R6 structures...\n");
    if (module_size == 0) module_size = 0x18000000;
    if (InitRenderPipeline(base_address, module_size)) printf("[+] R6 scanner OK!\n");
    else printf("[!] R6 scan failed\n");

    g_lastRainbowTick = GetTickCount();
    printf("\n[*] Step 4: Creating overlay...\n");
    xCreateWindow();
    printf("[+] Overlay created\n");
    printf("[*] Step 5: Init DirectX 9...\n");
    xInitD3d();
    printf("[+] DirectX 9 OK\n");
    printf("\n[*] ALL SYSTEMS GO\n");
    Sleep(3000);
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    xMainLoop();
    xShutdown();
    return 0;
}

void SetWindowToTarget() {
    while (true) {
        if (hwnd) {
            ZeroMemory(&GameRect, sizeof(GameRect));
            GetWindowRect(hwnd, &GameRect);
            Width = GameRect.right - GameRect.left;
            Height = GameRect.bottom - GameRect.top;
            DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
            if (dwStyle & WS_BORDER) { GameRect.top += 32; Height -= 39; }
            ScreenCenterX = Width / 2;
            ScreenCenterY = Height / 2;
            MoveWindow(Window, GameRect.left, GameRect.top, Width, Height, true);
        } else { exit(0); }
    }
}

const MARGINS Margin = { -1 };

void xCreateWindow() {
    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)SetWindowToTarget, 0, 0, 0);
    WNDCLASS windowClass = { 0 };
    windowClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.hInstance = NULL;
    windowClass.lpfnWndProc = WinProc;
    windowClass.lpszClassName = "notepad";
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClass(&windowClass);
    Window = CreateWindow("notepad", NULL, WS_POPUP, 0, 0,
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, NULL, NULL);
    ShowWindow(Window, SW_SHOW);
    DwmExtendFrameIntoClientArea(Window, &Margin);
    SetWindowLong(Window, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
    UpdateWindow(Window);
}

void xInitD3d() {
    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &p_Object))) exit(3);
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.BackBufferWidth = Width;
    d3dpp.BackBufferHeight = Height;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.hDeviceWindow = Window;
    d3dpp.Windowed = TRUE;
    if (FAILED(p_Object->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &D3dDevice))) {
        p_Object->Release(); p_Object = nullptr; exit(4);
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplWin32_Init(Window);
    ImGui_ImplDX9_Init(D3dDevice);
    // ─── profound — editorial monochrome + amber accent ──────────────────
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha              = 1.0f;
    style.WindowPadding      = ImVec2(0.0f, 0.0f);   // custom padding via child panes
    style.WindowRounding     = 10.0f;
    style.WindowBorderSize   = 1.0f;
    style.WindowMinSize      = ImVec2(64.0f, 64.0f);
    style.WindowTitleAlign   = ImVec2(0.0f, 0.5f);
    style.ChildRounding      = 8.0f;
    style.ChildBorderSize    = 0.0f;
    style.FramePadding       = ImVec2(10.0f, 6.0f);
    style.FrameRounding      = 6.0f;
    style.FrameBorderSize    = 0.0f;
    style.ItemSpacing        = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing   = ImVec2(6.0f, 6.0f);
    style.IndentSpacing      = 18.0f;
    style.ScrollbarSize      = 10.0f;
    style.ScrollbarRounding  = 6.0f;
    style.GrabMinSize        = 12.0f;
    style.GrabRounding       = 6.0f;
    style.TabRounding        = 6.0f;
    style.PopupRounding      = 8.0f;
    style.PopupBorderSize    = 1.0f;

    ImVec4* colors = style.Colors;
    // core palette
    const ImVec4 bg        = ImVec4(0.039f, 0.039f, 0.039f, 0.985f);  // #0a0a0a
    const ImVec4 elevated  = ImVec4(0.063f, 0.063f, 0.063f, 1.000f);  // #101010
    const ImVec4 frame     = ImVec4(0.078f, 0.078f, 0.078f, 1.000f);  // #141414
    const ImVec4 frameHov  = ImVec4(0.110f, 0.110f, 0.110f, 1.000f);  // #1c1c1c
    const ImVec4 frameAct  = ImVec4(0.141f, 0.141f, 0.141f, 1.000f);  // #242424
    const ImVec4 border    = ImVec4(0.102f, 0.102f, 0.102f, 1.000f);  // #1a1a1a
    const ImVec4 text      = ImVec4(0.910f, 0.902f, 0.882f, 1.000f);  // #e8e6e1
    const ImVec4 textMute  = ImVec4(0.478f, 0.467f, 0.447f, 1.000f);  // #7a7772
    const ImVec4 textDim   = ImVec4(0.290f, 0.282f, 0.267f, 1.000f);  // #4a4844
    const ImVec4 accent    = ImVec4(0.788f, 0.663f, 0.431f, 1.000f);  // #c9a96e — muted brass
    const ImVec4 accentHov = ImVec4(0.831f, 0.710f, 0.478f, 1.000f);  // #d4b57a
    const ImVec4 accentDim = ImVec4(0.788f, 0.663f, 0.431f, 0.180f);  // ghosted accent

    colors[ImGuiCol_Text]                 = text;
    colors[ImGuiCol_TextDisabled]         = textDim;
    colors[ImGuiCol_WindowBg]             = bg;
    colors[ImGuiCol_ChildBg]              = ImVec4(0,0,0,0);
    colors[ImGuiCol_PopupBg]              = elevated;
    colors[ImGuiCol_Border]               = border;
    colors[ImGuiCol_BorderShadow]         = ImVec4(0,0,0,0);
    colors[ImGuiCol_FrameBg]              = frame;
    colors[ImGuiCol_FrameBgHovered]       = frameHov;
    colors[ImGuiCol_FrameBgActive]        = frameAct;
    colors[ImGuiCol_TitleBg]              = elevated;
    colors[ImGuiCol_TitleBgActive]        = elevated;
    colors[ImGuiCol_TitleBgCollapsed]     = elevated;
    colors[ImGuiCol_MenuBarBg]            = elevated;
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0,0,0,0);
    colors[ImGuiCol_ScrollbarGrab]        = frame;
    colors[ImGuiCol_ScrollbarGrabHovered] = frameHov;
    colors[ImGuiCol_ScrollbarGrabActive]  = frameAct;
    colors[ImGuiCol_CheckMark]            = accent;
    colors[ImGuiCol_SliderGrab]           = accent;
    colors[ImGuiCol_SliderGrabActive]     = accentHov;
    colors[ImGuiCol_Button]               = frame;
    colors[ImGuiCol_ButtonHovered]        = frameHov;
    colors[ImGuiCol_ButtonActive]         = frameAct;
    colors[ImGuiCol_Header]               = accentDim;
    colors[ImGuiCol_HeaderHovered]        = frameHov;
    colors[ImGuiCol_HeaderActive]         = frameAct;
    colors[ImGuiCol_Separator]            = border;
    colors[ImGuiCol_SeparatorHovered]     = accent;
    colors[ImGuiCol_SeparatorActive]      = accent;
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0,0,0,0);
    colors[ImGuiCol_ResizeGripHovered]    = accentDim;
    colors[ImGuiCol_ResizeGripActive]     = accent;
    colors[ImGuiCol_Tab]                  = ImVec4(0,0,0,0);
    colors[ImGuiCol_TabHovered]           = frameHov;
    colors[ImGuiCol_TabActive]            = frameAct;
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0,0,0,0);
    colors[ImGuiCol_TabUnfocusedActive]   = frame;
    colors[ImGuiCol_NavHighlight]         = accent;
    colors[ImGuiCol_DragDropTarget]       = accent;

    // Inter — modern grotesque used by Linear / Vercel / Substack. Falls back
    // through SegUIVariable (Win11) → Segoe UI (Win7+) → default.
    ImFontConfig cfg;
    cfg.OversampleH = 3; cfg.OversampleV = 2;
    cfg.PixelSnapH  = false;
    cfg.RasterizerMultiply = 1.05f;

    const char* bodyPaths[] = {
        "C:\\Windows\\Fonts\\Inter-Regular.ttf",
        "C:\\Windows\\Fonts\\SegUIVar.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        nullptr
    };
    const char* dispPaths[] = {
        "C:\\Windows\\Fonts\\Inter-SemiBold.ttf",
        "C:\\Windows\\Fonts\\SegUIVar.ttf",
        "C:\\Windows\\Fonts\\segoeuisb.ttf",
        "C:\\Windows\\Fonts\\seguisb.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
        nullptr
    };

    for (int i = 0; bodyPaths[i] && !m_pFont; ++i)
        m_pFont = io.Fonts->AddFontFromFileTTF(bodyPaths[i], 15.0f, &cfg, io.Fonts->GetGlyphRangesDefault());
    if (!m_pFont) m_pFont = io.Fonts->AddFontDefault();

    for (int i = 0; dispPaths[i] && !m_pFontDisplay; ++i)
        m_pFontDisplay = io.Fonts->AddFontFromFileTTF(dispPaths[i], 20.0f, &cfg, io.Fonts->GetGlyphRangesDefault());
    if (!m_pFontDisplay) m_pFontDisplay = m_pFont;

    p_Object->Release(); p_Object = nullptr;
}

void aimbot(float x, float y) {
    float ScreenCX = (Width / 2.0f);
    float ScreenCY = (Height / 2.0f);
    float AimSpeed = smooth;
    float TargetX = 0, TargetY = 0;
    if (x != 0) {
        if (x > ScreenCX) { TargetX = -(ScreenCX - x); TargetX /= AimSpeed; if (TargetX + ScreenCX > ScreenCX * 2) TargetX = 0; }
        if (x < ScreenCX) { TargetX = x - ScreenCX; TargetX /= AimSpeed; if (TargetX + ScreenCX < 0) TargetX = 0; }
    }
    if (y != 0) {
        if (y > ScreenCY) { TargetY = -(ScreenCY - y); TargetY /= AimSpeed; if (TargetY + ScreenCY > ScreenCY * 2) TargetY = 0; }
        if (y < ScreenCY) { TargetY = y - ScreenCY; TargetY /= AimSpeed; if (TargetY + ScreenCY < 0) TargetY = 0; }
    }
    MouseController::Move_Mouse(static_cast<int>(TargetX), static_cast<int>(TargetY));
}

static int g_menuTab = 0;

void SubmitDrawCalls() {
    FlushOverlayPipeline(Esp_box, cornered_box, Esp_line, Esp_Distance, VisDist,
                   playerTrail, Aimbot, AimFOV, smooth, hitboxpos,
                   fovcircle, square_fov, crosshair);
}

void render() {
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    UpdateRainbow();

    {
        static bool s_iconsInitAttempted = false;
        if (!s_iconsInitAttempted && D3dDevice) {
            LoadShaderResources(D3dDevice);
            s_iconsInitAttempted = true;
        }
    }

    if (ShowMenu) {
        const char* boneItems[] = { "Head", "Neck", "Chest", "Pelvis", "Feet" };
        static const char* snapOriginItems[]  = { "Bottom", "Center", "Top" };
        static const char* cornerItems[]      = { "Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right" };

        // ─── profound — main window ─────────────────────────────────────
        ImGui::SetNextWindowSize(ImVec2(920.0f, 680.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(840.0f, 560.0f), ImVec2(1400.0f, 900.0f));
        ImGui::Begin("##profound_main", &ShowMenu,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoTitleBar);

        const ImVec4 kAccent    = ImVec4(0.788f, 0.663f, 0.431f, 1.0f);
        const ImVec4 kText      = ImVec4(0.910f, 0.902f, 0.882f, 1.0f);
        const ImVec4 kTextMute  = ImVec4(0.478f, 0.467f, 0.447f, 1.0f);
        const ImVec4 kTextDim   = ImVec4(0.290f, 0.282f, 0.267f, 1.0f);
        const ImVec4 kGood      = ImVec4(0.482f, 0.627f, 0.357f, 1.0f);  // sage
        const ImVec4 kBad       = ImVec4(0.788f, 0.431f, 0.431f, 1.0f);  // rust
        const ImVec4 kElevated  = ImVec4(0.063f, 0.063f, 0.063f, 1.0f);

        auto SectionLabel = [&](const char* txt) {
            ImGui::Dummy(ImVec2(0, 4));
            ImGui::PushStyleColor(ImGuiCol_Text, kTextMute);
            // Clean uppercase, no letter-spacing hack — Inter handles caps cleanly on its own.
            char buf[64]; int j = 0;
            for (int i = 0; txt[i] && j < 63; i++) {
                char c = txt[i];
                if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
                buf[j++] = c;
            }
            buf[j] = 0;
            ImGui::TextUnformatted(buf);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 2));
        };

        // ─── SIDEBAR ────────────────────────────────────────────────────
        const float kSidebarW = 200.0f;
        const float kPadLeft  = 22.0f;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kElevated);
        ImGui::BeginChild("##profound_nav", ImVec2(kSidebarW, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Wordmark (streamer mode = neutral label instead of brand)
        ImGui::Dummy(ImVec2(0, 26));
        ImGui::Indent(kPadLeft);
        if (m_pFontDisplay) ImGui::PushFont(m_pFontDisplay);
        ImGui::PushStyleColor(ImGuiCol_Text, kText);
        ImGui::TextUnformatted(streamerMode ? "system" : "profound");
        ImGui::PopStyleColor();
        if (m_pFontDisplay) ImGui::PopFont();
        ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
        ImGui::TextUnformatted(streamerMode ? "utilities" : "by rac0");
        ImGui::PopStyleColor();
        ImGui::Unindent(kPadLeft);

        ImGui::Dummy(ImVec2(0, 28));

        // Nav items — 8 tabs, distributed so no single tab needs scroll
        const char* navItems[] = { "Aim", "Box", "Skeleton", "Info", "Awareness", "Radar", "Colors", "System" };
        const int   navCount   = 8;
        for (int i = 0; i < navCount; i++) {
            const bool selected = (g_menuTab == i);
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.788f, 0.663f, 0.431f, 0.10f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.035f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.788f, 0.663f, 0.431f, 0.16f));
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? kText : kTextMute);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(kPadLeft, 7.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(0, 1));
            char label[32]; snprintf(label, 32, "%s##nav%d", navItems[i], i);
            if (ImGui::Selectable(label, selected, 0, ImVec2(kSidebarW, 0))) g_menuTab = i;
            if (selected) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 mn = ImGui::GetItemRectMin();
                ImVec2 mx = ImGui::GetItemRectMax();
                dl->AddRectFilled(ImVec2(mn.x, mn.y + 6), ImVec2(mn.x + 2, mx.y - 6),
                                  ImGui::ColorConvertFloat4ToU32(kAccent), 1.0f);
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
        }

        // Status pill anchored properly to bottom
        {
            const float kPillH   = 46.0f;
            const float kPillBot = 22.0f;
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - kPillH - kPillBot);
            ImGui::Indent(kPadLeft);

            bool inMatch = g_manualInMatch.load(std::memory_order_acquire);
            ImVec4 dot = inMatch ? kGood : kBad;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();

            // Invisible click target covering the whole pill
            if (ImGui::InvisibleButton("##engagedtoggle", ImVec2(kSidebarW - kPadLeft - 8, kPillH))) {
                bool next = !inMatch;
                g_manualInMatch.store(next, std::memory_order_release);
                g_toastState = next;
                g_toastStartTick = GetTickCount();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Toggle engagement (F2)\nON before round starts. OFF returning to menu.");

            // Overlay: dot + status text + hint
            dl->AddCircleFilled(ImVec2(origin.x + 4, origin.y + 10), 4.0f,
                                ImGui::ColorConvertFloat4ToU32(dot));
            dl->AddText(ImVec2(origin.x + 16, origin.y + 2),
                        ImGui::ColorConvertFloat4ToU32(inMatch ? kText : kTextMute),
                        inMatch ? "engaged" : "detached");
            dl->AddText(ImVec2(origin.x + 16, origin.y + 22),
                        ImGui::ColorConvertFloat4ToU32(kTextDim),
                        "F2 to toggle");
            ImGui::Unindent(kPadLeft);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Vertical hairline between sidebar and content
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            float wh = ImGui::GetWindowHeight();
            dl->AddLine(ImVec2(wp.x + kSidebarW, wp.y + 12),
                        ImVec2(wp.x + kSidebarW, wp.y + wh - 12),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(0.102f, 0.102f, 0.102f, 1.0f)));
        }

        // ─── CONTENT PANE ───────────────────────────────────────────────
        // Two nested children: outer (padding wrapper), inner (scroll area).
        // Right padding is achieved by making the inner child narrower than the outer.
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginChild("##profound_content_outer", ImVec2(0, 0), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const float kPadX = 26.0f;
        const float kPadTop = 26.0f;
        const float kPadRight = 20.0f;

        // Header (fixed, above the scroll area)
        ImGui::Dummy(ImVec2(0, kPadTop));
        ImGui::Indent(kPadX);
        {
            const char* titles[]   = { "Aim", "Box", "Skeleton", "Info", "Awareness", "Radar", "Colors", "System" };
            const char* subtitles[] = {
                "target selection, curves, prediction-free tracking",
                "2D corner box, 3D box, distance fade",
                "bone rendering & aim-point",
                "distance, health, operator, names, priority",
                "off-screen arrows, closest ring, nearest vector, counters",
                "top-down enemy tracker",
                "palette",
                "session, weather, diagnostics"
            };
            if (m_pFontDisplay) ImGui::PushFont(m_pFontDisplay);
            ImGui::TextUnformatted(titles[g_menuTab]);
            if (m_pFontDisplay) ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, kTextMute);
            ImGui::TextUnformatted(subtitles[g_menuTab]);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 12));
            // Header hairline drawn at CURRENT cursor position (fixed from prior version)
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 p = ImGui::GetCursorScreenPos();
                float w = ImGui::GetContentRegionAvail().x - kPadRight;
                dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y),
                            ImGui::ColorConvertFloat4ToU32(ImVec4(0.102f, 0.102f, 0.102f, 1.0f)));
            }
            ImGui::Dummy(ImVec2(0, 10));
        }
        ImGui::Unindent(kPadX);

        // Scrollable inner area — where all the widgets live. Right-padded via a
        // narrower child. Also sets PushItemWidth for cleaner alignment.
        float availW = ImGui::GetContentRegionAvail().x - kPadRight;
        ImGui::Indent(kPadX);
        ImGui::BeginChild("##profound_content_scroll", ImVec2(availW - kPadX, 0), false,
            ImGuiWindowFlags_AlwaysUseWindowPadding);

        // Widget width: consistent slider width, sane against long labels
        const float kWideW = ImGui::GetContentRegionAvail().x * 0.55f;   // sliders/combos
        const float kNarrowW = ImGui::GetContentRegionAvail().x * 0.35f; // secondary sliders
        ImGui::PushItemWidth(kWideW);

        // Route to section by tab index.

        // ═══ 0. AIM ═══
        if (g_menuTab == 0) {
            SectionLabel("Aimbot");
            ImGui::Checkbox("Enable", &Aimbot);
            HotkeyButton(hotkeys::aimkey, ChangeKey, keystatus); ImGui::SameLine();
            ImGui::TextColored(kTextMute, "aim key");

            ImGui::Dummy(ImVec2(0, 6));
            ImGui::SliderFloat("FOV", &AimFOV, 20.0f, 800.0f, "%.0f");
            ImGui::SliderFloat("Smoothness", &smooth, 1.0f, 20.0f, "%.1f");
            ImGui::Combo("Hitbox", &hitboxpos, boneItems, IM_ARRAYSIZE(boneItems));
            static const char* aimModeItems[] = { "Bone", "Closest (Z scan)" };
            ImGui::Combo("Target mode", &aimTargetMode, aimModeItems, 2);
            ImGui::Combo("Aim curve",   &aimCurve, aimCurveNames, 3);

            SectionLabel("Modifiers");
            ImGui::Checkbox("Multi-bone fallback", &multiBoneAim);
            ImGui::TextColored(kTextMute, "head → chest → pelvis if head off-screen");
            ImGui::Checkbox("Sticky target", &stickyAim);
            ImGui::TextColored(kTextMute, "keeps aiming at same entity while key held");

            SectionLabel("FOV Visualization");
            ImGui::Checkbox("Circle", &fovcircle);
            if (fovcircle) { square_fov = false; fovcirclefilled = false; }
            ImGui::SameLine();
            ImGui::Checkbox("Square", &square_fov);
            if (square_fov) { fovcircle = false; fovcirclefilled = false; }
            ImGui::ColorEdit4("FOV color", fovCircleColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::PushItemWidth(kNarrowW);
            ImGui::SliderFloat("FOV thickness", &fovCircleThickness, 0.5f, 5.0f, "%.1f");
            ImGui::PopItemWidth();

            SectionLabel("Crosshair");
            ImGui::Checkbox("Enable crosshair", &crosshair); ImGui::SameLine();
            ImGui::ColorEdit4("Xhair color", crosshairColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            if (crosshair) {
                ImGui::PushItemWidth(kNarrowW);
                ImGui::SliderFloat("Xhair size", &crosshairSize, 3.0f, 30.0f, "%.0f");
                ImGui::PopItemWidth();
            }
            ImGui::ColorEdit4("Aim target dot", aimbotTargetColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        }

        // ═══ 1. BOX ═══
        if (g_menuTab == 1) {
            SectionLabel("2D Box");
            ImGui::Checkbox("Enable", &Esp_box); ImGui::SameLine();
            ImGui::Checkbox("Cornered", &cornered_box); ImGui::SameLine();
            ImGui::Checkbox("Filled", &fillbox);
            ImGui::ColorEdit4("Box color", espBoxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::PushItemWidth(kNarrowW);
            ImGui::SliderFloat("Box thickness", &boxThickness, 0.5f, 5.0f, "%.1f");
            ImGui::PopItemWidth();
            if (fillbox)
                ImGui::ColorEdit4("Fill color", filledBoxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

            SectionLabel("3D Box");
            ImGui::Checkbox("Enable 3D box", &box3dEnabled);
            if (box3dEnabled) {
                ImGui::ColorEdit4("3D box color", box3dColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::PushItemWidth(kNarrowW);
                ImGui::SliderFloat("3D thickness", &box3dThickness, 0.5f, 4.0f, "%.1f");
                ImGui::SliderFloat("3D width",     &box3dHalfWidth, 0.20f, 0.60f, "%.2f m");
                ImGui::SliderFloat("3D height",    &box3dHeight,    1.00f, 2.20f, "%.2f m");
                ImGui::PopItemWidth();
                ImGui::TextColored(kTextMute, "shrink height for crouch, width for prone");
            }

            SectionLabel("Distance Fade");
            ImGui::Checkbox("Fade with distance", &distanceFade);
            if (distanceFade) {
                ImGui::PushItemWidth(kNarrowW);
                ImGui::SliderFloat("Full at", &distanceFadeNear, 5.0f, 100.0f, "%.0f m");
                ImGui::SliderFloat("Faded at", &distanceFadeFar, 50.0f, 300.0f, "%.0f m");
                ImGui::PopItemWidth();
                ImGui::TextColored(kTextMute, "player boxes softer as they get further");
            }
        }

        // ═══ 2. SKELETON ═══
        if (g_menuTab == 2) {
            SectionLabel("Skeleton");
            ImGui::Checkbox("Enable skeleton", &Esp_skeleton);
            if (Esp_skeleton) {
                ImGui::ColorEdit4("Skeleton color", espSkeletonColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::PushItemWidth(kNarrowW);
                ImGui::SliderFloat("Skeleton thickness", &skeletonThickness, 0.5f, 5.0f, "%.1f");
                ImGui::PopItemWidth();
            }
            SectionLabel("Aim Point");
            ImGui::Checkbox("Aim at head bone", &skeletonAim);
            ImGui::TextColored(kTextMute, "aimbot targets the head bone when skeleton is on");
        }

        // ═══ 3. INFO ═══
        if (g_menuTab == 3) {
            SectionLabel("Info Overlays");
            ImGui::Checkbox("Distance", &Esp_Distance);   ImGui::SameLine();
            ImGui::Checkbox("Line to head", &lineheadesp); ImGui::SameLine();
            ImGui::Checkbox("Health bar", &depthVisualization);
            ImGui::ColorEdit4("Distance color", espDistanceColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

            SectionLabel("Snaplines");
            ImGui::Checkbox("Enable snaplines", &Esp_line);
            if (Esp_line) {
                ImGui::PushItemWidth(kNarrowW);
                ImGui::Combo("Origin", &snaplineOrigin, snapOriginItems, 3);
                ImGui::PopItemWidth();
            }
            ImGui::ColorEdit4("Snapline color", espSnaplineColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::PushItemWidth(kNarrowW);
            ImGui::SliderFloat("Snapline thickness", &snaplineThickness, 0.5f, 5.0f, "%.1f");
            ImGui::PopItemWidth();

            SectionLabel("Operators");
            ImGui::Checkbox("Show names", &shaderLabelOverlay);
            if (shaderLabelOverlay) {
                ImGui::SameLine();
                ImGui::Checkbox("Icons", &shaderIconOverlay);
                ImGui::TextColored(g_shaderResReady ? kGood : ImVec4(1,0.6f,0.2f,1),
                    g_shaderResReady ? "icons  %d loaded" : "icons  not loaded",
                    g_shaderResLoaded);
            }

            SectionLabel("Priority Coloring");
            ImGui::Checkbox("Closest-to-crosshair", &priorityHighlight);
            if (priorityHighlight)
                ImGui::ColorEdit4("Priority color", priorityColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::Checkbox("Low HP tint", &lowHpPriority);
            if (lowHpPriority)
                ImGui::ColorEdit4("Low-HP color", lowHpColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

            SectionLabel("Corpses / DBNO");
            ImGui::Checkbox("Show downed players", &corpseEsp);
            if (corpseEsp)
                ImGui::ColorEdit4("Corpse tint", corpseColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

            SectionLabel("Filters & Range");
            ImGui::Checkbox("Team check", &espTeamCheck); ImGui::SameLine();
            ImGui::Checkbox("Death check", &espDeathCheck);
            ImGui::SliderInt("Render distance", &VisDist, 20, 500, "%d m");
        }

        // ═══ 4. AWARENESS ═══
        if (g_menuTab == 4) {
            SectionLabel("Off-Screen Arrows");
            ImGui::Checkbox("Enable arrows", &offscreenArrows);
            if (offscreenArrows) {
                ImGui::ColorEdit4("Arrow color", offscreenColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::PushItemWidth(kNarrowW);
                ImGui::SliderFloat("Arrow radius", &offscreenRadius, 120.0f, 400.0f, "%.0f px");
                ImGui::PopItemWidth();
            }

            SectionLabel("Closest-Target Ring");
            ImGui::Checkbox("Enable ring", &closestRing);
            if (closestRing)
                ImGui::ColorEdit4("Ring color", closestRingColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

            SectionLabel("Nearest-Enemy Vector");
            ImGui::Checkbox("Draw line to nearest", &nearestVector);
            if (nearestVector)
                ImGui::ColorEdit4("Vector color", nearestVectorColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
            ImGui::TextColored(kTextMute, "subtle line from crosshair to closest enemy");

            SectionLabel("Counters");
            ImGui::Checkbox("Enemy count HUD", &enemyCountHud);
            if (enemyCountHud)
                ImGui::Combo("Count corner", &enemyCountCorner, cornerItems, 4);
            ImGui::Checkbox("Session stats HUD", &sessionStatsHud);
            ImGui::TextColored(kTextMute, "uptime, peak players, inferred kills");
            ImGui::Checkbox("Round-change toast", &roundToasts);
            ImGui::TextColored(kTextMute, "brief notice when the sync buffer clears");
        }

        // ═══ 5. RADAR ═══
        if (g_menuTab == 5) {
            SectionLabel("Radar");
            ImGui::Checkbox("Enable radar", &radarEnabled);
            ImGui::TextColored(kTextMute, "top-down enemy tracker anchored to a screen corner");

            if (radarEnabled) {
                SectionLabel("Layout");
                ImGui::Combo("Corner", &radarCorner, cornerItems, 4);
                ImGui::SliderFloat("Size", &radarSize, 120.0f, 400.0f, "%.0f px");
                ImGui::SliderFloat("Range", &radarRange, 15.0f, 200.0f, "%.0f m");

                SectionLabel("Colors");
                ImGui::ColorEdit4("Background", radarBgColor, ImGuiColorEditFlags_AlphaBar);
                ImGui::ColorEdit4("You",        radarLocalColor, ImGuiColorEditFlags_AlphaBar);
                ImGui::ColorEdit4("Enemies",    radarEnemyColor, ImGuiColorEditFlags_AlphaBar);
            }
        }

        // ═══ 6. COLORS ═══
        if (g_menuTab == 6) {
            SectionLabel("Rainbow");
            ImGui::Checkbox("Rainbow mode", &rainbowMode);
            if (rainbowMode) {
                ImGui::Indent(16.0f);
                ImGui::Checkbox("Box",       &rainbowBox);       ImGui::SameLine();
                ImGui::Checkbox("Snaplines", &rainbowSnaplines);
                ImGui::Checkbox("FOV",       &rainbowFov);       ImGui::SameLine();
                ImGui::Checkbox("Trail",     &rainbowTrail);
                ImGui::Unindent(16.0f);
            }

            SectionLabel("Palette");
            ImGui::ColorEdit4("Box",         espBoxColor,        ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Fill",        filledBoxColor,     ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Snaplines",   espSnaplineColor,   ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Trail",       espTrailColor,      ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Distance",    espDistanceColor,   ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Skeleton",    espSkeletonColor,   ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("FOV circle",  fovCircleColor,     ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Crosshair",   crosshairColor,     ImGuiColorEditFlags_AlphaBar);
            ImGui::ColorEdit4("Aim target",  aimbotTargetColor,  ImGuiColorEditFlags_AlphaBar);
        }

        // ═══ 7. SYSTEM ═══
        if (g_menuTab == 7) {
            SectionLabel("Presentation");
            ImGui::Checkbox("Streamer mode", &streamerMode);
            ImGui::TextColored(kTextMute, "hides the brand from the menu wordmark");

            SectionLabel("Trails (movement history)");
            ImGui::Checkbox("Enable player trails", &playerTrail);
            if (playerTrail) {
                ImGui::ColorEdit4("Trail color", espTrailColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::PushItemWidth(kNarrowW);
                ImGui::SliderFloat("Thickness", &trailThickness, 0.5f, 8.0f, "%.1f");
                ImGui::SliderInt("Length", &trailLength, 10, 200, "%d frames");
                ImGui::PopItemWidth();
                ImGui::Checkbox("Fade", &trailFade); ImGui::SameLine();
                ImGui::Checkbox("Rainbow", &rainbowTrail);
                if (ImGui::Button("Clear all trails", ImVec2(140, 22))) {
                    g_trailBufCount = 0;
                    memset(g_trailBuffers, 0, sizeof(g_trailBuffers));
                }
            }

            SectionLabel("Weather FX");
            static const char* weatherItems[] = { "Snow", "Rain", "Fire", "Off" };
            int wm = g_weatherMode;
            if (ImGui::Combo("Weather", &wm, weatherItems, 4)) {
                g_weatherMode = wm;
                g_wfxInited = false;
            }
            if (g_weatherMode != WFX_NONE) {
                ImGui::SliderFloat("Intensity", &g_wfxIntensity, 0.1f, 1.0f, "%.1f");
                g_wfxIntensity = g_weatherIntensity;
                ImGui::SliderFloat("Wind", &g_wfxWindX, -2.0f, 2.0f, "%.1f");
                g_weatherWind = g_wfxWindX;
            }

            SectionLabel("Diagnostics");
            ImGui::TextColored(kTextMute, "build  %s  %s", __DATE__, __TIME__);
            ImGui::TextColored(kTextMute, "pid    %lu   base 0x%llX", processID, (unsigned long long)base_address);
            ImGui::TextColored(kTextMute, "cache  %d entities   players %d", (int)g_syncMap.size(), g_activeVtx);
            if (g_shaderResReady)
                ImGui::TextColored(kTextMute, "icons  %d / %d", g_shaderResLoaded, g_opIconCount);
        }

        ImGui::PopItemWidth();
        ImGui::EndChild();       // ##profound_content_scroll
        ImGui::Unindent(kPadX);
        ImGui::EndChild();       // ##profound_content_outer
        ImGui::End();
    }

    SubmitDrawCalls();

    // ─── persistent status dot + F2 toast (always visible, even with menu closed) ───
    {
        ImDrawList* dl = ImGui::GetOverlayDrawList();
        bool engaged = g_manualInMatch.load(std::memory_order_acquire);
        ImU32 dotCol = engaged ? IM_COL32(123, 160, 91, 200) : IM_COL32(201, 110, 110, 120);
        float dotX = (float)Width - 18.0f, dotY = 18.0f;
        dl->AddCircleFilled(ImVec2(dotX, dotY), 5.0f, dotCol);

        DWORD now = GetTickCount();
        if (g_toastStartTick && (now - g_toastStartTick) < TOAST_DURATION_MS) {
            float elapsed = (float)(now - g_toastStartTick);
            float alpha = 1.0f - (elapsed / (float)TOAST_DURATION_MS);
            alpha = alpha * alpha;
            const char* label = g_toastState ? "ESP  ON" : "ESP  OFF";
            ImVec2 tsz = ImGui::CalcTextSize(label);
            float tx = (float)Width * 0.5f - tsz.x * 0.5f;
            float ty = (float)Height * 0.18f;
            ImU32 bgCol = IM_COL32(10, 10, 10, (int)(180 * alpha));
            ImU32 txtCol = g_toastState
                ? IM_COL32(123, 160, 91, (int)(255 * alpha))
                : IM_COL32(201, 110, 110, (int)(255 * alpha));
            float pad = 12.0f;
            dl->AddRectFilled(ImVec2(tx - pad, ty - 6), ImVec2(tx + tsz.x + pad, ty + tsz.y + 6), bgCol, 8.0f);
            dl->AddText(ImVec2(tx, ty), txtCol, label);
        } else {
            g_toastStartTick = 0;
        }
    }

    ImGui::EndFrame();
    D3dDevice->SetRenderState(D3DRS_ZENABLE, false);
    D3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
    D3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
    D3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
    if (D3dDevice->BeginScene() >= 0) {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        D3dDevice->EndScene();
    }
    HRESULT result = D3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
    if (result == D3DERR_DEVICELOST && D3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        D3dDevice->Reset(&d3dpp);
        ImGui_ImplDX9_CreateDeviceObjects();
    }
}

MSG Message = { NULL };
void xMainLoop() {
    static RECT old_rc;
    ZeroMemory(&Message, sizeof(MSG));
    while (Message.message != WM_QUIT) {
        if (PeekMessage(&Message, Window, 0, 0, PM_REMOVE)) {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
        HWND hwnd_active = GetForegroundWindow();
        if (hwnd_active == hwnd) {
            HWND hwndtest = GetWindow(hwnd_active, GW_HWNDPREV);
            SetWindowPos(Window, hwndtest, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        if (GetAsyncKeyState(0x23) & 1) exit(8);
        RECT rc; POINT xy;
        ZeroMemory(&rc, sizeof(RECT));
        ZeroMemory(&xy, sizeof(POINT));
        GetClientRect(hwnd, &rc);
        ClientToScreen(hwnd, &xy);
        rc.left = xy.x; rc.top = xy.y;
        ImGuiIO& io = ImGui::GetIO();
        io.ImeWindowHandle = hwnd;
        io.DeltaTime = 1.0f / 60.0f;
        POINT p; GetCursorPos(&p);
        io.MousePos.x = p.x - xy.x;
        io.MousePos.y = p.y - xy.y;
        if (GetAsyncKeyState(VK_LBUTTON)) {
            io.MouseDown[0] = true; io.MouseClicked[0] = true;
            io.MouseClickedPos[0].x = io.MousePos.x;
            io.MouseClickedPos[0].y = io.MousePos.y;
        } else io.MouseDown[0] = false;
        if (rc.left != old_rc.left || rc.right != old_rc.right || rc.top != old_rc.top || rc.bottom != old_rc.bottom) {
            old_rc = rc;
            Width = rc.right; Height = rc.bottom;
            d3dpp.BackBufferWidth = Width; d3dpp.BackBufferHeight = Height;
            SetWindowPos(Window, (HWND)0, xy.x, xy.y, Width, Height, SWP_NOREDRAW);
            D3dDevice->Reset(&d3dpp);
        }
        render();
    }
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(Window);
}

LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam)) return true;
    switch (Message) {
    case WM_DESTROY: xShutdown(); PostQuitMessage(0); exit(4); break;
    case WM_SIZE:
        if (D3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            d3dpp.BackBufferWidth = LOWORD(lParam); d3dpp.BackBufferHeight = HIWORD(lParam);
            HRESULT hr = D3dDevice->Reset(&d3dpp);
            if (hr == D3DERR_INVALIDCALL) IM_ASSERT(0);
            ImGui_ImplDX9_CreateDeviceObjects();
        } break;
    default: return DefWindowProc(hWnd, Message, wParam, lParam); break;
    }
    return 0;
}

void xShutdown() {
    ShutdownRenderPipeline();
    if (TriBuf) TriBuf->Release();
    if (D3dDevice) D3dDevice->Release();
    if (p_Object) p_Object->Release();
    DestroyWindow(Window);
    UnregisterClass("notepad", NULL);
}
