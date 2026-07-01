// Fixed GUIDs for the ZhiPin text service. These are permanent identifiers:
// changing them orphans existing registrations.
#pragma once

#include <guiddef.h>

// {7C4B4C5A-9A3E-4E62-B2F1-8D0A5C1E2B31} — COM class of the text service
DEFINE_GUID(CLSID_ZhiPinTextService,
            0x7c4b4c5a, 0x9a3e, 0x4e62, 0xb2, 0xf1, 0x8d, 0x0a, 0x5c, 0x1e, 0x2b, 0x31);

// {A3F5D6E1-2B4C-4D8F-9E70-1C2A3B4D5E6F} — TSF language profile (zh-CN)
DEFINE_GUID(GUID_ZhiPinProfile,
            0xa3f5d6e1, 0x2b4c, 0x4d8f, 0x9e, 0x70, 0x1c, 0x2a, 0x3b, 0x4d, 0x5e, 0x6f);

// {E1B2C3D4-5F60-4718-A9B0-C1D2E3F40506} — display attribute for composition text
DEFINE_GUID(GUID_ZhiPinDisplayAttributeInput,
            0xe1b2c3d4, 0x5f60, 0x4718, 0xa9, 0xb0, 0xc1, 0xd2, 0xe3, 0xf4, 0x05, 0x06);
