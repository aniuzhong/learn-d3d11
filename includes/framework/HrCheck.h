#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>

// Unified output of HRESULT failure information
inline void D3dLogHrFailure(HRESULT hr, const char* operation)
{
    std::cerr << operation << " failed (HRESULT 0x" << std::hex
              << static_cast<unsigned int>(static_cast<ULONG>(hr)) << std::dec << ")\n";
}

// Returns true on success; prints and returns false on failure
inline bool D3dHrOk(HRESULT hr, const char* operation)
{
    if (SUCCEEDED(hr)) {
        return true;
    }
    D3dLogHrFailure(hr, operation);
    return false;
}
