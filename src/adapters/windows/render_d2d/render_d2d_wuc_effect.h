#ifndef REACH_ADAPTERS_WINDOWS_RENDER_D2D_WUC_EFFECT_H
#define REACH_ADAPTERS_WINDOWS_RENDER_D2D_WUC_EFFECT_H

#include <utility>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <windows.graphics.effects.interop.h>

struct reach_wuc_effect
    : winrt::implements<reach_wuc_effect, winrt::Windows::Graphics::Effects::IGraphicsEffect,
                        winrt::Windows::Graphics::Effects::IGraphicsEffectSource,
                        ABI::Windows::Graphics::Effects::IGraphicsEffectD2D1Interop>
{
    using source = winrt::Windows::Graphics::Effects::IGraphicsEffectSource;
    using value = winrt::Windows::Foundation::IInspectable;

    GUID id;
    winrt::hstring name;
    std::vector<value> properties;
    std::vector<source> sources;

    reach_wuc_effect(GUID effect_id, winrt::hstring label, std::vector<value> values,
                     std::vector<source> inputs)
        : id(effect_id), name(std::move(label)), properties(std::move(values)),
          sources(std::move(inputs))
    {
    }
    winrt::hstring Name() const
    {
        return name;
    }
    void Name(winrt::hstring const &value)
    {
        name = value;
    }
    HRESULT __stdcall GetEffectId(GUID *out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = id;
        return S_OK;
    }
    HRESULT __stdcall GetNamedPropertyMapping(
        LPCWSTR, UINT *,
        ABI::Windows::Graphics::Effects::GRAPHICS_EFFECT_PROPERTY_MAPPING *) noexcept override
    {
        return E_INVALIDARG;
    }
    HRESULT __stdcall GetPropertyCount(UINT *out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = (UINT)properties.size();
        return S_OK;
    }
    HRESULT __stdcall GetProperty(UINT index,
                                  ABI::Windows::Foundation::IPropertyValue **out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (index >= properties.size())
            return E_INVALIDARG;
        try
        {
            auto property = properties[index].as<winrt::Windows::Foundation::IPropertyValue>();
            *out = reinterpret_cast<ABI::Windows::Foundation::IPropertyValue *>(
                winrt::get_abi(property));
            (*out)->AddRef();
            return S_OK;
        }
        catch (...)
        {
            return winrt::to_hresult();
        }
    }
    HRESULT __stdcall GetSourceCount(UINT *out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = (UINT)sources.size();
        return S_OK;
    }
    HRESULT __stdcall
    GetSource(UINT index,
              ABI::Windows::Graphics::Effects::IGraphicsEffectSource **out) noexcept override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (index >= sources.size())
            return E_INVALIDARG;
        *out = reinterpret_cast<ABI::Windows::Graphics::Effects::IGraphicsEffectSource *>(
            winrt::get_abi(sources[index]));
        (*out)->AddRef();
        return S_OK;
    }
};

#endif
