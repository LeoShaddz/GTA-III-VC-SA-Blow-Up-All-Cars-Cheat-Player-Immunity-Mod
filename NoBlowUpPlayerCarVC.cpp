// NoBlowUpPlayerCarVC - GTA Vice City 1.0 (PC) (ASI) - NOT COMPILED, NOT TESTED
// The "blow up all cars" cheat (BIGBANG) no longer blows up the vehicle the player is driving.
//
// How: the cheat code block inside the cheat handler (starts at 0x4AC1C5; VC keeps cheat strings encoded) loops over the vehicle pool and,
// for each vehicle, runs
//     6A 00                    push 0
//     FF 97 80 00 00 00        call dword ptr [edi+0x80]      ; CVehicle::BlowUpCar(0) through the vtable, ecx = vehicle
// at 0x4AC220. Only those 8 bytes are replaced by "call Thunk" + 3 nops. The thunk skips the call when ecx is the vehicle the player is in and
// otherwise does exactly what the original did. Nothing else in the game is touched, so the player's car can still be destroyed normally.
//
// Addresses read from the user's gta-vc.exe: cheat block 0x4AC1C5; vehicle pool 0xA0FDE4 (object size 0x5DC);
// CWorld::Players 0x94AD28 (stride 0x170, ped pointer first), PlayerInFocus 0xA10AFB; ped+0x3AC = bInVehicle, ped+0x3A8 = vehicle.
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>

static const uintptr_t VERSION_ADDR  = 0x667BF0;
static const uint32_t  VERSION_OK    = 0x53E58955;
static const uintptr_t CHEAT_FN      = 0x4AC1C5;
static const uintptr_t CALL_SITE     = 0x4AC220;
static const uint8_t   ORIG_SITE[8]  = { 0x6A, 0x00, 0xFF, 0x97, 0x80, 0x00, 0x00, 0x00 };
static const uint8_t   ORIG_FN[5]    = { 0xB9, 0x20, 0xB2, 0x94, 0x00 };
static const uintptr_t PLAYERS_BASE  = 0x94AD28;
static const uintptr_t PLAYER_FOCUS  = 0xA10AFB;
static const uintptr_t PLAYER_STRIDE = 0x170;

static bool g_log = false;
static char g_iniPath[MAX_PATH];
static volatile LONG g_calls = 0;

static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    FILE* f = fopen("NoBlowUpPlayerCarVC.log", "a");
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
    if (*(const uint8_t*)(ped + 0x3AC) == 0) return nullptr;
    return *(void* const*)(ped + 0x3A8);
}

static void __cdecl LogSkip(void* v) { if (g_calls < 5) { InterlockedIncrement(&g_calls); Log("cheat: skipped the player's vehicle %p", v); } }

// Replaces "push 0 / call [edi+0x80]". On entry: ecx = vehicle, edi = its vtable. Keeps ebx/esi/edi/ebp intact.
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
        call dword ptr [edi + 0x80]
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
    if (g_log) { FILE* f = fopen("NoBlowUpPlayerCarVC.log", "w"); if (f) fclose(f); }

    if (*(const uint32_t*)VERSION_ADDR != VERSION_OK) { Log("Unsupported exe (needs GTA VC 1.0 PC). Mod disabled."); return TRUE; }
    if (memcmp((const void*)CHEAT_FN, ORIG_FN, 5) != 0 || memcmp((const void*)CALL_SITE, ORIG_SITE, 8) != 0)
    { Log("Cheat function bytes differ from the expected ones. Mod disabled."); return TRUE; }
    DWORD old;
    uint8_t* p = (uint8_t*)CALL_SITE;
    if (!VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old)) { Log("VirtualProtect failed"); return TRUE; }
    p[0] = 0xE8; *(int32_t*)(p + 1) = (int32_t)((uintptr_t)&Thunk - (CALL_SITE + 5));
    p[5] = p[6] = p[7] = 0x90;
    VirtualProtect(p, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 8);
    Log("OK. Call site 0x%X patched.", (unsigned)CALL_SITE);
    return TRUE;
}
