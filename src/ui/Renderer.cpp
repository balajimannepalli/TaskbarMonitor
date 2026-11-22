#include "Renderer.h"
#include <algorithm>

Renderer::Layer::~Layer() {
    target.Reset();
    if (dc) DeleteDC(dc);
    if (bitmap) DeleteObject(bitmap);
}

HRESULT Renderer::Layer::create(ID2D1Factory* factory,D2D1_SIZE_U pixels) {
    size=pixels;
    dc=CreateCompatibleDC(nullptr);
    if (!dc) return E_FAIL;
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(info.bmiHeader);
    info.bmiHeader.biWidth=static_cast<LONG>(size.width);
    info.bmiHeader.biHeight=-static_cast<LONG>(size.height);
    info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    void* bits{};
    bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (!bitmap || !SelectObject(dc,bitmap)) return E_FAIL;
    const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
    HRESULT hr=factory->CreateDCRenderTarget(&properties,&target);
    const RECT rect{0,0,info.bmiHeader.biWidth,-info.bmiHeader.biHeight};
    if (SUCCEEDED(hr)) hr=target->BindDC(dc,&rect);
    return hr;
}

HRESULT Renderer::setCells(display::Cells cells,int spacing) {
    if (cells==cells_ && spacing==spacing_) return S_FALSE;
    layouts_.resize(cells.size());
    sizes_.resize(cells.size());
    spacing_=spacing;
    for (size_t i=0;i<cells.size();++i) {
        if (i<cells_.size() && cells[i]==cells_[i]) continue;
        auto& layout=layouts_[i];
        HRESULT hr=write_->CreateTextLayout(cells[i].data(),static_cast<UINT32>(cells[i].size()),
            format_.Get(),2048,128,layout.ReleaseAndGetAddressOf());
        if (SUCCEEDED(hr)) hr=layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        if (SUCCEEDED(hr)) hr=layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        if (SUCCEEDED(hr)) hr=layout->GetMetrics(&sizes_[i]);
        if (FAILED(hr)) return hr;
        ++layoutsCreated_;
    }
    cells_=std::move(cells);
    return S_OK;
}

void Renderer::colors(unsigned foreground,unsigned background,int padding) {
    foreground_=foreground; background_=background; padding_=padding;
    if(brush_) brush_->SetColor(D2D1::ColorF(foreground_));
}
HRESULT Renderer::initialize(float fontSize,const wchar_t* family,bool transparent) {
    transparent_=transparent;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf());
    if (FAILED(hr)) return hr;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(write_.GetAddressOf()));
    if (FAILED(hr)) return hr;
    hr = write_->CreateTextFormat(family, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &format_);
    if (SUCCEEDED(hr)) hr = format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (SUCCEEDED(hr)) hr = format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    if (SUCCEEDED(hr)) hr = format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return hr;
}

HRESULT Renderer::paint(HWND window) {
    RECT rect{};
    if (!GetClientRect(window, &rect)) return E_FAIL;
    const auto size = D2D1::SizeU(static_cast<UINT>(rect.right), static_cast<UINT>(rect.bottom));
    if (size.width == 0 || size.height == 0) return S_FALSE;
    HRESULT hr = S_OK;
    if (transparent_) {
        if (!layer_ || layer_->size.width!=size.width || layer_->size.height!=size.height) {
            brush_.Reset();
            layer_=std::make_unique<Layer>();
            hr=layer_->create(factory_.Get(),size);
            if (SUCCEEDED(hr)) hr=layer_->target->CreateSolidColorBrush(D2D1::ColorF(foreground_),&brush_);
        }
    } else if (!target_) {
        hr = factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE),
            D2D1::HwndRenderTargetProperties(window, size), &target_);
        if (SUCCEEDED(hr)) hr = target_->CreateSolidColorBrush(D2D1::ColorF(foreground_), &brush_);
    } else {
        const auto old = target_->GetPixelSize();
        if (old.width != size.width || old.height != size.height) hr = target_->Resize(size);
    }
    if (SUCCEEDED(hr)) {
        ID2D1RenderTarget& target=transparent_ ? static_cast<ID2D1RenderTarget&>(*layer_->target.Get())
                                              : static_cast<ID2D1RenderTarget&>(*target_.Get());
        const float dpi = static_cast<float>(GetDpiForWindow(window));
        target.SetDpi(dpi, dpi);
        target.BeginDraw();
        // The smallest nonzero alpha keeps the background hit-testable by Windows.
        target.Clear(transparent_ ? D2D1::ColorF(0u,1.f/255.f) : D2D1::ColorF(background_));
        draw(target);
        hr=target.EndDraw();
        if (SUCCEEDED(hr) && transparent_) {
            SIZE extent{static_cast<LONG>(size.width),static_cast<LONG>(size.height)};
            POINT origin{};
            BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            if (!UpdateLayeredWindow(window,nullptr,nullptr,&extent,layer_->dc,&origin,0,&blend,ULW_ALPHA))
                hr=HRESULT_FROM_WIN32(GetLastError());
        }
    }
    if (FAILED(hr)) {
        brush_.Reset();
        target_.Reset();
        layer_.reset();
    }
    return hr;
}

void Renderer::draw(ID2D1RenderTarget& target) {
    const auto bounds = target.GetSize();
    const size_t rows=layouts_.size()>1 ? 2 : 1;
    const size_t columns=(layouts_.size()+rows-1)/rows;
    for (size_t i=0;i<layouts_.size();++i) {
        const auto& text=sizes_[i];
        const float width=bounds.width/static_cast<float>(columns);
        const float outerY=std::min(bounds.height/6,static_cast<float>(padding_));
        const float height=(bounds.height-2*outerY)/static_cast<float>(rows);
        const float pad=std::min(width/4,padding_+spacing_/2.f);
        const float scale=std::min({1.f,(width-2*pad)/text.width,height/text.height});
        const float x=width*static_cast<float>(i/rows)+(columns==1 ? (width-scale*text.width)/2 : pad);
        const float y=outerY+height*static_cast<float>(i%rows)+(height-scale*text.height)/2;
        target.SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*D2D1::Matrix3x2F::Translation(x,y));
        target.DrawTextLayout(D2D1::Point2F(),layouts_[i].Get(),brush_.Get());
    }
    target.SetTransform(D2D1::Matrix3x2F::Identity());
}

