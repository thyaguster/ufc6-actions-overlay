#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "instant_replay_gate.h"

#define GATE_RVA 0x5740cc0ULL
#define REGISTRATION_RVA 0x572ef1aULL
#define NAME_REFERENCE_RVA 0x572ef2cULL
#define NAME_RVA 0x92c31e4ULL

static int read_guest(uintptr_t address, void *output, size_t count) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), (const void*)address, output, count, &read) && read == count;
}

static int checked(uintptr_t base, int patched) {
    static const unsigned char registration_head[] = {0x48, 0x8d, 0x15};
    static const unsigned char name_head[] = {0x48, 0x8d, 0x05};
    static const char label[] = "fe_enable_instant_replays";
    unsigned char registration[7], name_reference[7], actual[16], expected[16];
    char name[sizeof(label)];
    int32_t displacement;
    MEMORY_BASIC_INFORMATION memory;
    uintptr_t address = base + GATE_RVA;
    if (!VirtualQuery((const void*)address, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
        (memory.Protect & PAGE_GUARD) ||
        !(memory.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return 0;
    if (!read_guest(base + REGISTRATION_RVA, registration, sizeof(registration)) ||
        memcmp(registration, registration_head, sizeof(registration_head))) return 0;
    memcpy(&displacement, registration + 3, sizeof(displacement));
    if (base + REGISTRATION_RVA + 7 + displacement != address) return 0;
    if (!read_guest(base + NAME_REFERENCE_RVA, name_reference, sizeof(name_reference)) ||
        memcmp(name_reference, name_head, sizeof(name_head))) return 0;
    memcpy(&displacement, name_reference + 3, sizeof(displacement));
    if (base + NAME_REFERENCE_RVA + 7 + displacement != base + NAME_RVA) return 0;
    if (!read_guest(base + NAME_RVA, name, sizeof(name)) || memcmp(name, label, sizeof(label))) return 0;
    memset(expected, 0xcc, sizeof(expected));
    expected[0] = patched ? 0xb0 : 0x31;
    expected[1] = patched ? 0x01 : 0xc0;
    expected[2] = 0xc3;
    return read_guest(address, actual, sizeof(actual)) && !memcmp(actual, expected, sizeof(expected));
}

int instant_replay_gate_tick(InstantReplayGate *gate, uintptr_t base, int enabled) {
    int wanted = enabled != 0;
    if (wanted == gate->owned) return gate->state = wanted;
    if (!checked(base, gate->owned)) return gate->state = -1;
    void *address = (void*)(base + GATE_RVA);
    DWORD old = 0, unused = 0;
    if (!VirtualProtect(address, 2, PAGE_EXECUTE_READWRITE, &old)) return gate->state = -1;
    unsigned short before = gate->owned ? 0x01b0 : 0xc031;
    unsigned short after = wanted ? 0x01b0 : 0xc031;
    SHORT observed = InterlockedCompareExchange16((volatile SHORT*)address, (SHORT)after, (SHORT)before);
    FlushInstructionCache(GetCurrentProcess(), address, 2);
    BOOL restored = VirtualProtect(address, 2, old, &unused);
    if ((unsigned short)observed != before || !restored) return gate->state = -1;
    gate->owned = wanted;
    return gate->state = wanted;
}
