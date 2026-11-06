#pragma once
#include <cstdint>
namespace Netplay::ProtocolConfig {
constexpr std::uint8_t MagicGame='R'; // Native TH18 adapter; distinct wire contract.
constexpr std::uint8_t MaxAnalogMode=0; // Native poll already produces buttons.
}
