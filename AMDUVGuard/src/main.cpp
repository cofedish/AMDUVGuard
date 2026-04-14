// AMDUVGuard - Win32 entry point.
#include "App.h"
#include <objbase.h>

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    HRESULT coRes = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int rc = 0;
    {
        uvg::App app(hInst);
        rc = app.Run();
    }
    if (SUCCEEDED(coRes)) CoUninitialize();
    return rc;
}
