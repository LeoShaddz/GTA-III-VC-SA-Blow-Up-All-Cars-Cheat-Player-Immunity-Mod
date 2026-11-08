// NoBlowUpPlayerCar - GTA SA 1.0 US (ASI) - NOT COMPILED, NOT TESTED
// The "blow up all cars" cheat no longer blows up the vehicle the player is driving.
//
// How it works: CVehicle::BlowUpCar is virtual (vtable slot 0xA4). The slot of every vehicle class is replaced by a
// small wrapper. The wrapper skips the call only when (a) the vehicle is the one the player is in and (b) the call comes
// from the cheat function (return address inside [CheatFunction, CheatFunction + CheatRange]). Every other explosion
// (rockets, fire, scripts...) is untouched, so the player's car can still be destroyed normally.
// The vtable values below were read from the user's gta_sa.exe; each slot is checked before it is patched.
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>

static const uintptr_t FIND_PLAYER_VEHICLE = 0x56E0D0;   // CVehicle* FindPlayerVehicle(int playerId, bool) - cdecl, -1 = current player
static const uintptr_t VERSION_FLOAT       = 0x586C9B;   // 180.0f in 1.0 US (same check as the other SA mods)
static const int       SLOT_BLOWUP         = 0xA4 / 4;

struct VT { uintptr_t vtable; uintptr_t expectOrig; };
static const VT VTS[] = {
    { 0x871120, 0x6B3780 }, { 0x871360, 0x6BEA10 }, { 0x871528, 0x6C0560 }, { 0x871680, 0x6C6D30 },
    { 0x8717D8, 0x6B3780 }, { 0x871948, 0x6CCCF0 }, { 0x871AE8, 0x6B3780 }, { 0x871C28, 0x6B3780 },
    { 0x871E80, 0x6D6340 }, { 0x8721A0, 0x6F21B0 }, { 0x872370, 0x6D6340 },
};
static const int NV = sizeof(VTS) / sizeof(VTS[0]);

static uintptr_t g_orig[NV] = {};
static bool      g_log = false;
static uintptr_t g_cheatFn = 0x439D80, g_cheatRange = 0x100;
static char      g_iniPath[MAX_PATH];
static volatile LONG g_logCount = 0;

static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    FILE* f = fopen("NoBlowUpPlayerCar.log", "a");
    if (!f) return;
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
}

typedef void* (__cdecl *FindVeh_t)(int, bool);
typedef void  (__thiscall *Blow_t)(void*, void*, bool);

template <int N>
static void __fastcall Wrap(void* self, void* /*edx*/, void* damager, bool hideExplosion)
{
    const uintptr_t ret = (uintptr_t)_ReturnAddress();
    void* pv = ((FindVeh_t)FIND_PLAYER_VEHICLE)(-1, false);
    if (pv && pv == self)
    {
        const bool fromCheat = ret >= g_cheatFn && ret < g_cheatFn + g_cheatRange;
        if (g_logCount < 20) { InterlockedIncrement(&g_logCount); Log("BlowUpCar on player vehicle, caller 0x%08X, from cheat=%d -> %s", (unsigned)ret, fromCheat ? 1 : 0, fromCheat ? "skipped" : "allowed"); }
        if (fromCheat) return;
    }
    ((Blow_t)g_orig[N])(self, damager, hideExplosion);
}

typedef void (__fastcall *Wrapper_t)(void*, void*, void*, bool);
static Wrapper_t g_wrappers[NV] = {
    Wrap<0>, Wrap<1>, Wrap<2>, Wrap<3>, Wrap<4>, Wrap<5>, Wrap<6>, Wrap<7>, Wrap<8>, Wrap<9>, Wrap<10>
};

BOOL APIENTRY DllMain(HMODULE hm, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(hm);
    GetModuleFileNameA(hm, g_iniPath, MAX_PATH);
    char* dot = strrchr(g_iniPath, '.'); if (dot) strcpy(dot, ".ini");
    g_log = GetPrivateProfileIntA("Settings", "EnableLog", 0, g_iniPath) != 0;
    if (g_log) { FILE* f = fopen("NoBlowUpPlayerCar.log", "w"); if (f) fclose(f); }
    char b[32];
    GetPrivateProfileStringA("Settings", "CheatFunction", "", b, sizeof(b), g_iniPath);
    if (b[0]) g_cheatFn = (uintptr_t)strtoul(b, nullptr, 0);
    GetPrivateProfileStringA("Settings", "CheatRange", "", b, sizeof(b), g_iniPath);
    if (b[0]) g_cheatRange = (uintptr_t)strtoul(b, nullptr, 0);

    if (*(const float*)VERSION_FLOAT != 180.0f) { Log("Unsupported exe (needs GTA SA 1.0 US). Mod disabled."); return TRUE; }

    int ok = 0;
    for (int i = 0; i < NV; ++i)
    {
        uintptr_t* slot = (uintptr_t*)VTS[i].vtable + SLOT_BLOWUP;
        if (*slot != VTS[i].expectOrig) { Log("vtable 0x%X: unexpected slot value 0x%X, skipped", (unsigned)VTS[i].vtable, (unsigned)*slot); continue; }
        DWORD old;
        if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) { Log("vtable 0x%X: VirtualProtect failed", (unsigned)VTS[i].vtable); continue; }
        g_orig[i] = *slot;
        *slot = (uintptr_t)g_wrappers[i];
        VirtualProtect(slot, 4, old, &old);
        ++ok;
    }
    Log("OK. %d/%d vehicle vtables patched. Cheat caller range 0x%X..+0x%X", ok, NV, (unsigned)g_cheatFn, (unsigned)g_cheatRange);
    return TRUE;
}
