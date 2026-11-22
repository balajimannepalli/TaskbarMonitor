#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <memory>
#include "MetricDisplay.h"

class Renderer {
public:
    HRESULT initialize(float fontSize = 12.f,const wchar_t* family=L"Segoe UI",bool transparent=false);
    void colors(unsigned foreground,unsigned background,int padding);
    HRESULT paint(HWND window);
    HRESULT setCells(display::Cells cells,int spacing);
    size_t layoutsCreated() const { return layoutsCreated_; }
private:
    struct Layer {
        HDC dc{};
        HBITMAP bitmap{};
        D2D1_SIZE_U size{};
        Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> target;
        ~Layer();
        HRESULT create(ID2D1Factory* factory,D2D1_SIZE_U pixels);
    };
    void draw(ID2D1RenderTarget& target);
    display::Cells cells_;
    size_t layoutsCreated_{};
    std::vector<Microsoft::WRL::ComPtr<IDWriteTextLayout>> layouts_;
    std::vector<DWRITE_TEXT_METRICS> sizes_;
    int spacing_{};
    int padding_=2;
    unsigned foreground_=0xf3f3f3,background_=0x252525;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> write_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    std::unique_ptr<Layer> layer_;
    bool transparent_{};
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};
