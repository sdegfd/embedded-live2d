#pragma once

#include <d3d11.h>

namespace DirectX
{
enum WIC_LOADER_FLAGS
{
    WIC_LOADER_DEFAULT = 0,
};

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
    ID3D11ShaderResourceView** textureView);
}