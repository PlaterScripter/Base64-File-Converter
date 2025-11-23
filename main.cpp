#include "Base64Converter.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Base64Converter app;
    
    if (!app.initialize(hInstance)) {
        MessageBoxA(nullptr, "Failed to initialize Base64 Converter.", "Critical Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    return app.run();
}
