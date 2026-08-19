#include "pch.h"
#include "RetroScreenRenderer.h"
#include "Common\DirectXHelper.h"

using namespace dosbox_uwp;
using namespace Microsoft::WRL;

RetroScreenRenderer::RetroScreenRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources)
    : m_deviceResources(deviceResources)
{
    spdlog::debug("[Render] ctor");
}

void RetroScreenRenderer::CreateDeviceDependentResources()
{
    spdlog::debug("[Render] CreateDeviceDependentResources");
}

void RetroScreenRenderer::ReleaseDeviceDependentResources()
{
    spdlog::debug("[Render] ReleaseDeviceDependentResources");
    m_videoBitmap.Reset();
    m_frameWidth = 0;
    m_frameHeight = 0;
}

void RetroScreenRenderer::UpdateVideoFrame(const uint8_t* data, unsigned width, unsigned height, unsigned pitch)
{
    if (!data || width == 0 || height == 0)
    {
        spdlog::warn("[Render] UpdateVideoFrame: invalid params");
        return;
    }

    spdlog::debug("[Render] UpdateVideoFrame: {}x{} pitch={}", width, height, pitch);

    auto d2dContext = m_deviceResources->GetD2DDeviceContext();

    if (!m_videoBitmap || m_frameWidth != width || m_frameHeight != height)
    {
        spdlog::debug("[Render]   RecreateBitmap needed: old={}x{} new={}x{}", m_frameWidth, m_frameHeight, width, height);
        RecreateBitmap(width, height);
    }

    if (m_videoBitmap)
    {
        D2D1_RECT_U rect = { 0, 0, width, height };
        HRESULT hr = m_videoBitmap->CopyFromMemory(&rect, data, pitch);
        if (FAILED(hr))
        {
            spdlog::error("[Render]   CopyFromMemory FAILED hr=0x{:08X}", (unsigned)hr);
        }
    }
    else
    {
        spdlog::error("[Render]   m_videoBitmap is NULL after RecreateBitmap!");
    }
}

void RetroScreenRenderer::Render()
{
    if (!m_videoBitmap)
    {
        spdlog::debug("[Render] no bitmap, skip");
        return;
    }

    auto d2dContext = m_deviceResources->GetD2DDeviceContext();
    auto logicalSize = m_deviceResources->GetLogicalSize();

    spdlog::debug("[Render] frame={}x{} logical={:.0f}x{:.0f}", m_frameWidth, m_frameHeight, logicalSize.Width, logicalSize.Height);

    d2dContext->BeginDraw();

    float scaleX = logicalSize.Width / (float)m_frameWidth;
    float scaleY = logicalSize.Height / (float)m_frameHeight;
    float scale = min(scaleX, scaleY);

    float drawW = m_frameWidth * scale;
    float drawH = m_frameHeight * scale;
    float offsetX = (logicalSize.Width - drawW) * 0.5f;
    float offsetY = (logicalSize.Height - drawH) * 0.5f;

    D2D1_RECT_F destRect = D2D1::RectF(offsetX, offsetY, offsetX + drawW, offsetY + drawH);

    spdlog::debug("[Render]   DrawBitmap dest=({:.0f},{:.0f})-({:.0f},{:.0f})", destRect.left, destRect.top, destRect.right, destRect.bottom);

    d2dContext->DrawBitmap(
        m_videoBitmap.Get(),
        destRect,
        1.0f,
        m_interpolationMode,
        nullptr
    );

    HRESULT hr = d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        spdlog::warn("[Render]   EndDraw: D2DERR_RECREATE_TARGET");
        m_videoBitmap.Reset();
        m_frameWidth = 0;
        m_frameHeight = 0;
    }
    else if (FAILED(hr))
    {
        spdlog::error("[Render]   EndDraw FAILED hr=0x{:08X}", (unsigned)hr);
    }
}

void RetroScreenRenderer::RecreateBitmap(unsigned width, unsigned height)
{
    auto d2dContext = m_deviceResources->GetD2DDeviceContext();

    m_videoBitmap.Reset();

    float dpiX, dpiY;
    d2dContext->GetDpi(&dpiX, &dpiY);

    spdlog::debug("[Render] RecreateBitmap: {}x{} dpi={:.0f}x{:.0f}", width, height, dpiX, dpiY);

    D2D1_BITMAP_PROPERTIES1 props = {};
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    props.dpiX = dpiX;
    props.dpiY = dpiY;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;

    ComPtr<ID2D1Bitmap1> bitmap;
    HRESULT hr = d2dContext->CreateBitmap(
        D2D1::SizeU(width, height),
        nullptr,
        0,
        props,
        &bitmap
    );

    if (SUCCEEDED(hr))
    {
        m_videoBitmap = bitmap;
        m_frameWidth = width;
        m_frameHeight = height;
        spdlog::debug("[Render]   CreateBitmap OK");
    }
    else
    {
        spdlog::error("[Render]   CreateBitmap FAILED hr=0x{:08X}", (unsigned)hr);
    }
}
