// Display attribute (composition underline) provider objects.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msctf.h>

// The single attribute we expose: plain solid underline, TF_ATTR_INPUT.
class CZhiPinDisplayAttributeInfo : public ITfDisplayAttributeInfo {
public:
    CZhiPinDisplayAttributeInfo() = default;

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP GetGUID(GUID* pguid) override;
    STDMETHODIMP GetDescription(BSTR* pbstrDesc) override;
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) override;
    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE* pda) override;
    STDMETHODIMP Reset() override;

private:
    virtual ~CZhiPinDisplayAttributeInfo() = default;
    LONG _refCount = 1;
};

class CZhiPinEnumDisplayAttributeInfo : public IEnumTfDisplayAttributeInfo {
public:
    CZhiPinEnumDisplayAttributeInfo() = default;

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP Next(ULONG ulCount, ITfDisplayAttributeInfo** rgInfo,
                      ULONG* pcFetched) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Skip(ULONG ulCount) override;

private:
    virtual ~CZhiPinEnumDisplayAttributeInfo() = default;
    LONG _refCount = 1;
    ULONG _index = 0;  // 0 = not yet returned, 1 = done
};
