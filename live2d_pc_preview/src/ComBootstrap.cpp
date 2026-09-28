#include <objbase.h>

namespace
{
class ComBootstrap
{
public:
    ComBootstrap()
        : _shouldUninitialize(false)
    {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        _shouldUninitialize = (hr == S_OK || hr == S_FALSE);
    }

    ~ComBootstrap()
    {
        if (_shouldUninitialize)
        {
            CoUninitialize();
        }
    }

private:
    bool _shouldUninitialize;
};

ComBootstrap g_comBootstrap;
}