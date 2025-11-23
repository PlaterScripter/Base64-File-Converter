#include "Base64Converter.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <wincrypt.h> // Used for robust Base64 conversion

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "crypt32.lib") // Required for CryptBinaryToStringA

#define IDC_RADIO_ENCODE 1001
#define IDC_RADIO_DECODE 1002
#define IDC_EDIT_INPUT   1003
#define IDC_BTN_BROWSE   1004
#define IDC_BTN_CONVERT  1005
#define IDC_PROGRESS     1006

Base64Converter::Base64Converter() 
    : m_hWnd(nullptr), m_isProcessing(FALSE), m_isEncoding(true) {}

Base64Converter::~Base64Converter() {
    m_isProcessing = FALSE;
}

bool Base64Converter::initialize(HINSTANCE hInstance) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXA wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = Base64Converter::staticWndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = "Base64ConverterClass";
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExA(&wcex)) return false;

    // Center window calculation
    int w = 500, h = 220;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    m_hWnd = CreateWindowExA(0, "Base64ConverterClass", "Base64 Converter",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             x, y, w, h, nullptr, nullptr, hInstance, this);

    return m_hWnd != nullptr;
}

int Base64Converter::run() {
    if (!m_hWnd) return 0;
    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK Base64Converter::staticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    Base64Converter* pThis = nullptr;
    if (message == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Base64Converter*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hWnd = hWnd;
    } else {
        pThis = (Base64Converter*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) return pThis->wndProc(hWnd, message, wParam, lParam);
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT Base64Converter::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowA("STATIC", "Mode:", WS_VISIBLE | WS_CHILD, 20, 20, 50, 20, hWnd, nullptr, nullptr, nullptr);
        
        m_hRadioEncode = CreateWindowA("BUTTON", "File to Base64 (Encode)", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            80, 20, 180, 20, hWnd, (HMENU)IDC_RADIO_ENCODE, nullptr, nullptr);
        
        m_hRadioDecode = CreateWindowA("BUTTON", "Base64 to File (Decode)", WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            270, 20, 180, 20, hWnd, (HMENU)IDC_RADIO_DECODE, nullptr, nullptr);

        SendMessage(m_hRadioEncode, BM_SETCHECK, BST_CHECKED, 0);

        CreateWindowA("STATIC", "File:", WS_VISIBLE | WS_CHILD, 20, 60, 40, 20, hWnd, nullptr, nullptr, nullptr);
        m_hEditInput = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            60, 58, 320, 24, hWnd, (HMENU)IDC_EDIT_INPUT, nullptr, nullptr);
        
        m_hBtnBrowse = CreateWindowA("BUTTON", "Browse...", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            390, 58, 80, 24, hWnd, (HMENU)IDC_BTN_BROWSE, nullptr, nullptr);

        m_hBtnConvert = CreateWindowA("BUTTON", "Convert", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            20, 100, 450, 30, hWnd, (HMENU)IDC_BTN_CONVERT, nullptr, nullptr);

        m_hProgress = CreateWindowA(PROGRESS_CLASSA, nullptr, WS_VISIBLE | WS_CHILD,
            20, 140, 450, 15, hWnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);

        m_hStatus = CreateWindowA("STATIC", "Ready", WS_VISIBLE | WS_CHILD,
            20, 160, 450, 20, hWnd, nullptr, nullptr, nullptr);
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_RADIO_ENCODE: setMode(true); break;
        case IDC_RADIO_DECODE: setMode(false); break;
        case IDC_BTN_BROWSE:   onBrowse(); break;
        case IDC_BTN_CONVERT:  onStart(); break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void Base64Converter::setMode(bool encode) {
    m_isEncoding = encode;
    // Clear input if switching modes to avoid confusion
    m_inputPath = "";
    SetWindowTextA(m_hEditInput, "");
    updateStatus("Mode changed. Please select a file.");
}

void Base64Converter::onBrowse() {
    char buffer[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = m_isEncoding ? "Select File to Encode" : "Select Base64 File to Decode";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        m_inputPath = buffer;
        SetWindowTextA(m_hEditInput, m_inputPath.c_str());
        updateStatus("File selected. Ready to convert.");
    }
}

void Base64Converter::onStart() {
    if (m_isProcessing) return;
    if (m_inputPath.empty()) {
        MessageBoxA(m_hWnd, "Please select an input file first.", "Input Missing", MB_OK | MB_ICONWARNING);
        return;
    }

    // Determine default output name
    std::string suggested = m_inputPath;
    if (m_isEncoding) suggested += ".b64";
    else {
        if (suggested.size() > 4 && suggested.substr(suggested.size() - 4) == ".b64")
            suggested = suggested.substr(0, suggested.size() - 4);
        else
            suggested += ".bin";
    }

    std::string outPath = getSavePath(suggested);
    if (outPath.empty()) return;

    m_isProcessing = TRUE;
    EnableWindow(m_hBtnConvert, FALSE);
    EnableWindow(m_hBtnBrowse, FALSE);
    
    // Run conversion
    if (processFile(m_inputPath, outPath, m_isEncoding)) {
        updateStatus("Conversion Complete!");
        MessageBoxA(m_hWnd, "Conversion completed successfully.", "Success", MB_OK | MB_ICONINFORMATION);
    } else {
        updateStatus("Conversion Failed.");
        MessageBoxA(m_hWnd, "An error occurred during conversion.", "Error", MB_OK | MB_ICONERROR);
    }

    m_isProcessing = FALSE;
    EnableWindow(m_hBtnConvert, TRUE);
    EnableWindow(m_hBtnBrowse, TRUE);
    setProgress(0);
}

std::string Base64Converter::getSavePath(const std::string& suggested) {
    char buffer[MAX_PATH];
    strcpy_s(buffer, suggested.c_str());
    
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Save Output As";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        return std::string(buffer);
    }
    return "";
}

bool Base64Converter::processFile(const std::string& inputPath, const std::string& outputPath, bool encode) {
    std::ifstream inFile(inputPath, std::ios::binary | std::ios::ate);
    std::ofstream outFile(outputPath, std::ios::binary);

    if (!inFile.is_open() || !outFile.is_open()) return false;

    // Get file size for progress
    std::streamsize totalSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);

    if (totalSize == 0) return true; // Empty file

    // Buffer sizes
    // For encoding: Read in multiples of 3 (binary) -> produces multiples of 4 (text)
    // For decoding: Read in multiples of 4 (text) -> produces multiples of 3 (binary)
    const DWORD CHUNK_SIZE = 3 * 1024; // 3KB chunks
    std::vector<BYTE> inBuffer(CHUNK_SIZE);
    std::vector<char> outBuffer(CHUNK_SIZE * 2); // Plenty of space for expansion

    std::streamsize processed = 0;
    
    while (inFile && m_isProcessing) {
        inFile.read((char*)inBuffer.data(), CHUNK_SIZE);
        std::streamsize bytesRead = inFile.gcount();
        if (bytesRead == 0) break;

        DWORD outLen = 0;
        BOOL result = FALSE;

        if (encode) {
            // Binary -> Base64
            // Get size first
            CryptBinaryToStringA(inBuffer.data(), (DWORD)bytesRead, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &outLen);
            if (outLen > outBuffer.size()) outBuffer.resize(outLen);
            
            result = CryptBinaryToStringA(inBuffer.data(), (DWORD)bytesRead, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, outBuffer.data(), &outLen);
        } else {
            // Base64 -> Binary
            // Note: CryptStringToBinary automatically handles whitespace skipping usually, 
            // but strict blocking is better. We rely on valid B64 input.
            CryptStringToBinaryA(reinterpret_cast<LPCSTR>(inBuffer.data()), (DWORD)bytesRead, CRYPT_STRING_BASE64_ANY, nullptr, &outLen, nullptr, nullptr);
            if (outLen > outBuffer.size()) outBuffer.resize(outLen);

            result = CryptStringToBinaryA(reinterpret_cast<LPCSTR>(inBuffer.data()), (DWORD)bytesRead, CRYPT_STRING_BASE64_ANY, (BYTE*)outBuffer.data(), &outLen, nullptr, nullptr);
        }

        if (result) {
            outFile.write(outBuffer.data(), outLen);
        }

        processed += bytesRead;
        setProgress((int)((processed * 100) / totalSize));
        
        // Keep UI responsive
        pumpMessages();
    }

    return true;
}

void Base64Converter::updateStatus(const std::string& text) {
    SetWindowTextA(m_hStatus, text.c_str());
}

void Base64Converter::setProgress(int percent) {
    SendMessage(m_hProgress, PBM_SETPOS, percent, 0);
}

void Base64Converter::pumpMessages() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
