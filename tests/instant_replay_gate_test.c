#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../src/instant_replay_gate.c"

static void page(uintptr_t address) {
    assert(VirtualAlloc((void*)(address & ~(uintptr_t)4095), 4096, MEM_COMMIT, PAGE_READWRITE));
}

int main(void) {
    uintptr_t base = (uintptr_t)VirtualAlloc(NULL, 0xa000000, MEM_RESERVE, PAGE_NOACCESS);
    assert(base);
    InstantReplayGate gate = {0};
    assert(instant_replay_gate_tick(&gate, base, 1) == -1 && !gate.owned);
    page(base + GATE_RVA);page(base + REGISTRATION_RVA);page(base + NAME_RVA);
    unsigned char *code = (unsigned char*)(base + GATE_RVA);
    memset(code, 0xcc, 16);code[0] = 0x31;code[1] = 0xc0;code[2] = 0xc3;
    unsigned char *registration = (unsigned char*)(base + REGISTRATION_RVA);
    registration[0] = 0x48;registration[1] = 0x8d;registration[2] = 0x15;
    int32_t displacement = (int32_t)(GATE_RVA - REGISTRATION_RVA - 7);
    memcpy(registration + 3, &displacement, 4);
    unsigned char *name_ref = (unsigned char*)(base + NAME_REFERENCE_RVA);
    name_ref[0] = 0x48;name_ref[1] = 0x8d;name_ref[2] = 0x05;
    displacement = (int32_t)(NAME_RVA - NAME_REFERENCE_RVA - 7);
    memcpy(name_ref + 3, &displacement, 4);
    strcpy((char*)(base + NAME_RVA), "fe_enable_instant_replays");
    DWORD old = 0, unused = 0;
    assert(VirtualProtect(code, 16, PAGE_EXECUTE_READ, &old));
    assert(instant_replay_gate_tick(&gate, base, 1) == 1 && gate.owned && checked(base, 1));
    assert(instant_replay_gate_tick(&gate, base, 1) == 1 && checked(base, 1));
    assert(instant_replay_gate_tick(&gate, base, 0) == 0 && !gate.owned && checked(base, 0));
    assert(VirtualProtect(code, 16, PAGE_READWRITE, &old));
    code[4] = 0x90;
    assert(VirtualProtect(code, 16, PAGE_EXECUTE_READ, &unused));
    assert(instant_replay_gate_tick(&gate, base, 1) == -1 && !gate.owned && code[0] == 0x31);
    assert(VirtualFree((void*)base, 0, MEM_RELEASE));
    puts("PASS: exact instant-replay registration, reversible gate, missing-page and foreign-byte rejection.");
    return 0;
}
