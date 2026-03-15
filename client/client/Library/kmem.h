#pragma once

/*
 * kmem v6 — Client-side IOCTL driver interface
 * Device: \\.\KMemDrv5
 * Supports: read, write, pattern scan, physical read
 */

#include <Windows.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>

#pragma comment(lib, "Psapi.lib")

#define IOCTL_PING          CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ATTACH        CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ          CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE         CTL_CODE(0x8000, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_BASE      CTL_CODE(0x8000, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCAN_PATTERN  CTL_CODE(0x8000, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ_PHYS     CTL_CODE(0x8000, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_PHYS    CTL_CODE(0x8000, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAX_PATTERN_LEN 32
#define MAX_SCAN_RESULTS 32

#pragma pack(push, 1)
struct KMemReadReq {
    UINT64 Address;
    ULONG Size;
};

struct KMemScanReq {
    UINT64 ScanStart;
    UINT64 ScanSize;
    UCHAR Pattern[MAX_PATTERN_LEN];
    UCHAR Mask[MAX_PATTERN_LEN];
    ULONG PatternLen;
    ULONG ResultCount;
    struct {
        UINT64 Address;
        LONG RipDisp;
        UINT64 RipTarget;
    } Results[MAX_SCAN_RESULTS];
};
#pragma pack(pop)

namespace KMem
{
    inline HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    inline UINT64 TargetBase = 0;
    inline bool Connected = false;
    inline bool UsePhysical = false;  // true = EAC blocks virtual reads, use physical

    inline bool Init()
    {
        DeviceHandle = CreateFileA("\\\\.\\KMemDrv5", GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

        if (DeviceHandle == INVALID_HANDLE_VALUE)
        {
            printf("[!] Cannot open \\\\.\\KMemDrv5 (error %u)\n", GetLastError());
            printf("[!] Run load_kmem.bat first\n");
            return false;
        }

        UINT64 ping = 0;
        DWORD ret = 0;
        if (!DeviceIoControl(DeviceHandle, IOCTL_PING, nullptr, 0, &ping, sizeof(ping), &ret, nullptr)
            || ping != 0xDEADC0DE)
        {
            CloseHandle(DeviceHandle);
            DeviceHandle = INVALID_HANDLE_VALUE;
            return false;
        }

        Connected = true;
        return true;
    }

    inline UINT64 GetBase()
    {
        UINT64 base = 0;
        DWORD ret = 0;
        DeviceIoControl(DeviceHandle, IOCTL_GET_BASE, nullptr, 0, &base, sizeof(base), &ret, nullptr);
        return base;
    }

    inline bool ReadPhysMemory(UINT64 address, void* buffer, size_t size)
    {
        if (!Connected || !address || !buffer || !size) return false;
        size_t offset = 0;
        while (offset < size) {
            size_t chunk = size - offset;
            if (chunk > 0x1000) chunk = 0x1000;
            KMemReadReq req;
            req.Address = address + offset;
            req.Size = (ULONG)chunk;
            DWORD ret = 0;
            if (!DeviceIoControl(DeviceHandle, IOCTL_READ_PHYS, &req, sizeof(req),
                (BYTE*)buffer + offset, (DWORD)chunk, &ret, nullptr))
                return false;
            if (ret == 0) return false;
            offset += chunk;
        }
        return true;
    }

    inline bool ReadMemory(UINT64 address, void* buffer, size_t size)
    {
        if (!Connected || !address || !buffer || !size) return false;
        if (UsePhysical) return ReadPhysMemory(address, buffer, size);
        size_t offset = 0;
        while (offset < size)
        {
            size_t chunk = size - offset;
            if (chunk > 0x10000) chunk = 0x10000;
            KMemReadReq req;
            req.Address = address + offset;
            req.Size = (ULONG)chunk;
            DWORD ret = 0;
            if (!DeviceIoControl(DeviceHandle, IOCTL_READ, &req, sizeof(req),
                (BYTE*)buffer + offset, (DWORD)chunk, &ret, nullptr))
                return false;
            if (ret == 0) return false;
            offset += chunk;
        }
        return true;
    }

    inline bool WritePhysMemory(UINT64 address, void* buffer, size_t size)
    {
        if (!Connected || !address || !buffer || !size || size > 0xF000) return false;
        size_t reqSize = sizeof(UINT64) + sizeof(ULONG) + size;
        BYTE* req = (BYTE*)alloca(reqSize);
        *(UINT64*)req = address;
        *(ULONG*)(req + 8) = (ULONG)size;
        memcpy(req + 12, buffer, size);
        UINT64 result = 0;
        DWORD ret = 0;
        return DeviceIoControl(DeviceHandle, IOCTL_WRITE_PHYS, req, (DWORD)reqSize,
            &result, sizeof(result), &ret, nullptr) && result > 0;
    }

    inline bool WriteMemory(UINT64 address, void* buffer, size_t size)
    {
        if (!Connected || !address || !buffer || !size || size > 0xF000) return false;
        if (UsePhysical) return WritePhysMemory(address, buffer, size);
        size_t reqSize = sizeof(UINT64) + sizeof(ULONG) + size;
        BYTE* req = (BYTE*)alloca(reqSize);
        *(UINT64*)req = address;
        *(ULONG*)(req + 8) = (ULONG)size;
        memcpy(req + 12, buffer, size);
        UINT64 result = 0;
        DWORD ret = 0;
        return DeviceIoControl(DeviceHandle, IOCTL_WRITE, req, (DWORD)reqSize,
            &result, sizeof(result), &ret, nullptr) && result > 0;
    }

    inline bool ReadPhys(UINT64 address, void* buffer, size_t size)
    {
        if (!Connected || !address || !buffer || !size) return false;
        size_t offset = 0;
        while (offset < size) {
            size_t chunk = size - offset;
            if (chunk > 0x1000) chunk = 0x1000;
            KMemReadReq req;
            req.Address = address + offset;
            req.Size = (ULONG)chunk;
            DWORD ret = 0;
            if (!DeviceIoControl(DeviceHandle, IOCTL_READ_PHYS, &req, sizeof(req),
                (BYTE*)buffer + offset, (DWORD)chunk, &ret, nullptr))
                return false;
            if (ret == 0) return false;
            offset += chunk;
        }
        return true;
    }

    template<typename T>
    T ReadP(UINT64 address)
    {
        T value = T();
        ReadPhys(address, &value, sizeof(T));
        return value;
    }

    template<typename T>
    T Read(UINT64 address)
    {
        T value = T();
        ReadMemory(address, &value, sizeof(T));
        return value;
    }

    template<typename T>
    void Write(UINT64 address, T value)
    {
        WriteMemory(address, &value, sizeof(T));
    }

    inline KMemScanReq ScanPattern(UINT64 start, UINT64 size,
        const UCHAR* pattern, const UCHAR* mask, ULONG patLen)
    {
        KMemScanReq req = {};
        req.ScanStart = start;
        req.ScanSize = size;
        req.PatternLen = patLen;
        memcpy(req.Pattern, pattern, patLen);
        memcpy(req.Mask, mask, patLen);

        DWORD ret = 0;
        DeviceIoControl(DeviceHandle, IOCTL_SCAN_PATTERN, &req, sizeof(req),
            &req, sizeof(req), &ret, nullptr);
        return req;
    }

    inline KMemScanReq ScanSig(UINT64 start, UINT64 size, const char* sig)
    {
        UCHAR pattern[MAX_PATTERN_LEN] = {};
        UCHAR mask[MAX_PATTERN_LEN] = {};
        ULONG len = 0;

        const char* p = sig;
        while (*p && len < MAX_PATTERN_LEN)
        {
            while (*p == ' ') p++;
            if (!*p) break;
            if (*p == '?')
            {
                pattern[len] = 0;
                mask[len] = 0;
                len++;
                p++;
                if (*p == '?') p++;
            }
            else
            {
                char hex[3] = { p[0], p[1], 0 };
                pattern[len] = (UCHAR)strtoul(hex, nullptr, 16);
                mask[len] = 0xFF;
                len++;
                p += 2;
            }
        }

        return ScanPattern(start, size, pattern, mask, len);
    }

    // === AttachToProcess (at end so all helpers are available) ===

    inline void EnableDebugPrivilege()
    {
        HANDLE token;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
        {
            TOKEN_PRIVILEGES tp = {};
            LookupPrivilegeValueA(nullptr, "SeDebugPrivilege", &tp.Privileges[0].Luid);
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(token, FALSE, &tp, 0, nullptr, nullptr);
            CloseHandle(token);
        }
    }

    inline UINT64 GetBaseFromToolhelp(UINT32 pid)
    {
        EnableDebugPrivilege();

        // Method 1: Toolhelp snapshot
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap != INVALID_HANDLE_VALUE)
        {
            MODULEENTRY32W me = {};
            me.dwSize = sizeof(me);
            if (Module32FirstW(snap, &me))
            {
                CloseHandle(snap);
                return (UINT64)me.modBaseAddr;
            }
            CloseHandle(snap);
        }
        // Method 2: OpenProcess + EnumProcessModulesEx
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProc)
            hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc)
        {
            HMODULE mods[1];
            DWORD needed = 0;
            if (EnumProcessModulesEx(hProc, mods, sizeof(mods), &needed, LIST_MODULES_64BIT))
            {
                CloseHandle(hProc);
                return (UINT64)mods[0];
            }
            CloseHandle(hProc);
        }

        return 0;
    }

    inline bool AttachToProcess(UINT32 pid)
    {
        ULONG pidVal = pid;
        UINT64 base = 0;
        DWORD ret = 0;
        if (!DeviceIoControl(DeviceHandle, IOCTL_ATTACH, &pidVal, sizeof(pidVal),
            &base, sizeof(base), &ret, nullptr))
            return false;
        TargetBase = base;
        if (base != 0) return true;

        UsePhysical = true;
        TargetBase = GetBaseFromToolhelp(pid);
        if (TargetBase != 0) return true;

        // Fallback: scan for MZ headers via physical read

        // Scan typical 64-bit ASLR ranges with 64KB alignment (PE allocation granularity)
        const struct { UINT64 start; UINT64 end; UINT64 step; } ranges[] = {
            { 0x140000000ULL, 0x150000000ULL, 0x10000 },     // Default PE base
            { 0x7FF600000000ULL, 0x7FF800000000ULL, 0x10000 }, // Typical ASLR range
        };
        for (const auto& range : ranges)
        {
            for (UINT64 addr = range.start; addr < range.end; addr += range.step)
            {
                UINT16 mz = 0;
                if (!ReadPhys(addr, &mz, sizeof(mz))) continue;
                if (mz != 0x5A4D) continue;

                IMAGE_DOS_HEADER dos = {};
                ReadPhys(addr, &dos, sizeof(dos));
                if (dos.e_lfanew < 0 || dos.e_lfanew > 0x1000) continue;

                IMAGE_NT_HEADERS64 nt = {};
                ReadPhys(addr + dos.e_lfanew, &nt, sizeof(nt));
                if (nt.Signature != IMAGE_NT_SIGNATURE) continue;
                if (nt.OptionalHeader.SizeOfImage > 0x1000000)
                {
                    TargetBase = addr;
                    return true;
                }
            }
        }

        return false;
    }
}
