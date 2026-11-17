/// @file
#pragma once

#include "modbus.hpp"
#include <cstddef>
#include <cstdint>
#include <xbuddy_extension_shared/artme_bridge.hpp>

/// Bridges the MODBUS registers of the virtual ARTME unit to the text protocol
/// of an ARTME 3D screw extruder wired to the MMU port.
///
/// Nothing is sent to the port unless the mainboard addresses this unit, so a
/// printer with an MMU (or nothing) on the port is not affected.
class Artme final : public modbus::Callbacks {
public:
    Status read_register(uint8_t, uint16_t address, uint16_t &out) override;
    Status write_register(uint8_t, uint16_t address, uint16_t value) override;

private:
    /// Takes everything received so far and parses complete lines.
    void drain_rx();

    uint16_t command_rpm_x100 = 0;
    xbuddy_extension_shared::artme_bridge::Status status;
    uint16_t reply_counter = 0;

    char line[48];
    size_t line_length = 0;
    bool line_overflow = false;
};
