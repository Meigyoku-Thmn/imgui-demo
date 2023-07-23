#include <Windows.h>

int useDX8();
int useDX9();

int main() {
    while (true) {
        auto rs = MessageBoxW(NULL, L"YES for DirectX 9\nNO for DirectX 8\nCANCEL to exit.", L"Dear ImGui Demo", MB_YESNOCANCEL | MB_ICONINFORMATION);
        if (rs == IDYES)
            useDX9();
        else if (rs == IDNO)
            useDX8();
        else
            break;
    }
    return 0;
}