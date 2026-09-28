#include "WICTextureLoader.h"

#include <algorithm>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace
{
using Microsoft::WRL::ComPtr;

HRESULT LoadPixels(const wchar_t* fileName, size_t maxSize, std::vector<unsigned char>& pixels, UINT& width, UINT& height)
{
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        return hr;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        fileName,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr))
    {
        return hr;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
    {
        return hr;
    }

    UINT originalWidth = 0;
    UINT originalHeight = 0;
    hr = frame->GetSize(&originalWidth, &originalHeight);
    if (FAILED(hr))
    {
        return hr;
    }

    width = originalWidth;
    height = originalHeight;

    ComPtr<IWICBitmapSource> source;
    hr = frame.As(&source);
    if (FAILED(hr))
    {
        return hr;
    }

    if (maxSize > 0 && (width > maxSize || height > maxSize))
    {
        const double widthScale = static_cast<double>(maxSize) / static_cast<double>(width);
        const double heightScale = static_cast<double>(maxSize) / static_cast<double>(height);
        const double scale = widthScale < heightScale ? widthScale : heightScale;

        width = static_cast<UINT>(width * scale);
        height = static_cast<UINT>(height * scale);

        if (width == 0)
        {
            width = 1;
        }

        if (height == 0)
        {
            height = 1;
        }

        ComPtr<IWICBitmapScaler> scaler;
        hr = factory->CreateBitmapScaler(&scaler);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = scaler->Initialize(frame.Get(), width, height, WICBitmapInterpolationModeFant);
        if (FAILED(hr))
        {
            return hr;
        }

        source = scaler;
    }

    WICPixelFormatGUID pixelFormat;
    hr = source->GetPixelFormat(&pixelFormat);
    if (FAILED(hr))
    {
        return hr;
    }

    if (pixelFormat != GUID_WICPixelFormat32bppRGBA)
    {
        ComPtr<IWICFormatConverter> converter;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = converter->Initialize(
            source.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0f,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            return hr;
        }

        source = converter;
    }

    const UINT rowPitch = width * 4;
    pixels.resize(static_cast<size_t>(rowPitch) * static_cast<size_t>(height));

    return source->CopyPixels(
        nullptr,
        rowPitch,
        static_cast<UINT>(pixels.size()),
        pixels.data());
}

HRESULT CreateTexture(
    ID3D11Device* device,
    const std::vector<unsigned char>& pixels,
    UINT width,
    UINT height,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView)
{
    const UINT rowPitch = width * 4;

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = usage;
    textureDesc.BindFlags = bindFlags;
    textureDesc.CPUAccessFlags = cpuAccessFlags;
    textureDesc.MiscFlags = miscFlags;

    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = pixels.data();
    initialData.SysMemPitch = rowPitch;

    ComPtr<ID3D11Texture2D> texture2D;
    HRESULT hr = device->CreateTexture2D(&textureDesc, &initialData, &texture2D);
    if (FAILED(hr))
    {
        return hr;
    }

    if (textureView)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc = {};
        shaderResourceViewDesc.Format = textureDesc.Format;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(texture2D.Get(), &shaderResourceViewDesc, textureView);
        if (FAILED(hr))
        {
            return hr;
        }
    }

    if (texture)
    {
        *texture = texture2D.Detach();
    }

    return S_OK;
}
}

namespace DirectX
{
HRESULT CreateWICTextureFromFileEx(
    ID3D11Device* d3dDevice,
    ID3D11DeviceContext* d3dContext,
    const wchar_t* fileName,
    size_t maxSize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    WIC_LOADER_FLAGS loadFlags,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView)
{
    (void)d3dContext;

    if (!d3dDevice || !fileName || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (loadFlags != WIC_LOADER_DEFAULT)
    {
        return E_NOTIMPL;
    }

    if (texture)
    {
        *texture = nullptr;
    }

    if (textureView)
    {
        *textureView = nullptr;
    }

    std::vector<unsigned char> pixels;
    UINT width = 0;
    UINT height = 0;
    HRESULT hr = LoadPixels(fileName, maxSize, pixels, width, height);
    if (FAILED(hr))
    {
        return hr;
    }

    return CreateTexture(
        d3dDevice,
        pixels,
        width,
        height,
        usage,
        bindFlags,
        cpuAccessFlags,
        miscFlags,
        texture,
        textureView);
}
}