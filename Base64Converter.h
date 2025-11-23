#pragma once

#include <windows.h>
#include <string>
#include <vector>

// Handles Base64 encoding/decoding with a Win32 GUI
class Base64Converter {
private:
    HWND m_hWnd;
    HWND m_hRadioEncode;
    HWND m_hRadioDecode;
    HWND m_hEditInput;
    HWND m_hBtnBrowse;
    HWND m_hBtnConvert;
    HWND m_hProgress;
    HWND m_hStatus;

    volatile BOOL m_isProcessing;
    std::string m_inputPath;
    bool m_isEncoding; // true = File to Base64, false = Base64 to File
    
public:
    Base64Converter();
    ~Base64Converter();

    bool initialize(HINSTANCE hInstance);
    int run();

private:
    static LRESULT CALLBACK staticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void onBrowse();
    void onStart();
    void setMode(bool encode);
    void updateStatus(const std::string& text);
    void setProgress(int percent);

    // Core logic
    bool processFile(const std::string& inputPath, const std::string& outputPath, bool encrypt);
    
    // Helpers
    void pumpMessages();
    std::string getSavePath(const std::string& suggested);
};
