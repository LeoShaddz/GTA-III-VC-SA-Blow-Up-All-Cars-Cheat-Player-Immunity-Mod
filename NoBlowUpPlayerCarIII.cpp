// NoBlowUpPlayerCarIII - GTA III 1.0 (PC) (ASI) - NOT COMPILED, NOT TESTED
// The "blow up all cars" cheat (BANGBANGBANG) no longer blows up the vehicle the player is driving.
//
// How: the cheat function (0x491040, called when "GNABGNABGNAB" is typed) loops over the vehicle pool and, for each vehicle, runs
//     6A 00            push 0
//     FF 57 74         call dword ptr [edi+0x74]      ; CVehicle::BlowUpCar(0) through the vtable, ecx = vehicle
// at 0x491091. Only those 5 bytes are replaced by "call Thunk". The thunk skips the call when ecx is the vehicle the player is in and
// otherwise does exactly what the original did. Nothing else in the game is touched, so the player's car can still be destroyed normally.
//
// Addresses read from the user's gta3.exe: cheat function 0x491040; vehicle pool 0x9430DC (object size 0x5A8);
// CWorld::Players 0x9412F0 (stride 0x13C, ped pointer first), PlayerInFocus 0x95CD61; ped+0x314 = bInVehicle, ped+0x310 = vehicle.
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>

static const uintptr_t VERSION_ADDR  = 0x5C1E70;
static const uint32_t  VERSION_OK    = 0x53E58955;
static const uintptr_t CHEAT_FN      = 0x491040;
static const uintptr_t CALL_SITE     = 0x491091;
static const uint8_t   ORIG_SITE[5]  = { 0x6A, 0x00, 0xFF, 0x57, 0x74 };
static const uint8_t   ORIG_FN[5]    = { 0xB9, 0x20, 0x15, 0x94, 0x00 };
static const uintptr_t PLAYERS_BASE  = 0x9412F0;
static const uintptr_t PLAYER_FOCUS  = 0x95CD61;
static const uintptr_t PLAYER_STRIDE = 0x13C;

static bool g_log = false;
static char g_iniPath[MAX_PATH];
static volatile LONG g_calls = 0;

static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    FILE* f = fopen("NoBlowUpPlayerCarIII.log", "a");
    if (!f) return;
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
}

// vehicle the player is in (or null). cdecl, no arguments.
static void* __cdecl PlayerVehicle()
{
    const uint8_t focus = *(const volatile uint8_t*)PLAYER_FOCUS;
    if (focus > 1) return nullptr;
    const uint8_t* ped = *(const uint8_t* const*)(PLAYERS_BASE + PLAYER_STRIDE * focus);
    if ((uintptr_t)ped < 0x10000) return nullptr;
    if (*(const uint8_t*)(ped + 0x314) == 0) return nullptr;
    return *(void* const*)(ped + 0x310);
}

static void __cdecl LogSkip(void* v) { if (g_calls < 5) { InterlockedIncrement(&g_calls); Log("cheat: skipped the player's vehicle %p", v); } }

// Replaces "push 0 / call [edi+0x74]". On entry: ecx = vehicle, edi = its vtable. Keeps ebx/esi/edi/ebp intact.
__declspec(naked) static void Thunk()
{
    __asm {
        push ecx
        call PlayerVehicle
        pop  ecx
        test eax, eax
        jz   do_blow
        cmp  eax, ecx
        jne  do_blow
        push ecx
        push ecx
        call LogSkip
        add  esp, 4
        pop  ecx
        ret
    do_blow:
        push 0
        call dword ptr [edi + 0x74]
        ret
    }
}

BOOL APIENTRY DllMain(HMODULE hm, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(hm);
    GetModuleFileNameA(hm, g_iniPath, MAX_PATH);
    char* dot = strrchr(g_iniPath, '.'); if (dot) strcpy(dot, ".ini");
    g_log = GetPrivateProfileIntA("Settings", "EnableLog", 0, g_iniPath) != 0;
    if (g_log) { FILE* f = fopen("NoBlowUpPlayerCarIII.log", "w"); if (f) fclose(f); }

    if (*(const uint32_t*)VERSION_ADDR != VERSION_OK) { Log("Unsupported exe (needs GTA III 1.0 PC). Mod disabled."); return TRUE; }
    if (memcmp((const void*)CHEAT_FN, ORIG_FN, 5) != 0 || memcmp((const void*)CALL_SITE, ORIG_SITE, 5) != 0)
    { Log("Cheat function bytes differ from the expected ones. Mod disabled."); return TRUE; }
    DWORD old;
    uint8_t* p = (uint8_t*)CALL_SITE;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) { Log("VirtualProtect failed"); return TRUE; }
    p[0] = 0xE8; *(int32_t*)(p + 1) = (int32_t)((uintptr_t)&Thunk - (CALL_SITE + 5));
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    Log("OK. Call site 0x%X patched.", (unsigned)CALL_SITE);
    return TRUE;
}
