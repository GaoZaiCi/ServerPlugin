//
// Created by ASUS on 2022/8/24.
//

#ifndef TEMPLATEPLUGIN_VFTABLEHOOK_H
#define TEMPLATEPLUGIN_VFTABLEHOOK_H


//https://www.cnblogs.com/Chary/p/15633846.html
#include <memoryapi.h>

#define EXE_32_ADDRESS 0x400000
#define EXE_64_ADDRESS 0x140000000
#define DLL_32_ADDRESS 0x10000000
#define DLL_64_ADDRESS 0x180000000

extern uintptr_t imageBaseAddr;

namespace VH {
    template<class T, class U>
    void VHook(T *pT, int Tidx, U *pU, int Uidx) {
        auto pTVtAddr = (size_t *) *(size_t *) pT;
        auto pUVtAddr = (size_t *) *(size_t *) pU;

        DWORD dwProct = 0;
        VirtualProtect(pTVtAddr, sizeof(size_t), PAGE_READWRITE, &dwProct);
        pTVtAddr[Tidx] = pUVtAddr[Uidx];
        VirtualProtect(pTVtAddr, sizeof(size_t), dwProct, nullptr);
    };

    void VHookFun(void *pT, int Tidx, uintptr_t funcAdd, uintptr_t *ret) {
        auto pTVtAddr = (uintptr_t *) *(uintptr_t *) pT;
        DWORD dwProct = 0;
        *ret = pTVtAddr[Tidx];
        VirtualProtect(pTVtAddr, sizeof(size_t), PAGE_READWRITE, &dwProct);
        pTVtAddr[Tidx] = funcAdd;
        VirtualProtect(pTVtAddr, sizeof(size_t), dwProct, &dwProct);
    }

    void VHookFunEx(void *vtable, int Tidx, uintptr_t funcAdd, uintptr_t *ret) {
        auto pTVtAddr = (uintptr_t *) vtable;
        DWORD dwProct = 0;
        *ret = pTVtAddr[Tidx];
        VirtualProtect(pTVtAddr, sizeof(uintptr_t) * Tidx, PAGE_READWRITE, &dwProct);
        pTVtAddr[Tidx] = funcAdd;
        VirtualProtect(pTVtAddr, sizeof(uintptr_t) * Tidx, dwProct, &dwProct);
    }
}


#endif //TEMPLATEPLUGIN_VFTABLEHOOK_H
