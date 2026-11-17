/// @file
/// Link to an ARTME 3D screw extruder used as the print head.
///
/// The extruder keeps its own controller (MKS Gen L) and firmware (artme/ in this
/// repository). It is wired to the MMU port of the xBuddy extension board and
/// spoken to in plain text lines:
///
///   printer  -> extruder:  "M951 S<rpm> T<target degC> K<check>\n"
///   extruder -> printer:   "PH T:<degC> /<target degC> R:<rpm> F:<flags>\n" followed by "ok\n"
///
/// The mainboard reaches the port through MODBUS registers of a virtual unit,
/// the same way it reaches the MMU.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace xbuddy_extension_shared::artme_bridge {

constexpr uint8_t modbusUnitNr = 221;

// Holding registers, written by the mainboard as one block. Writing the last one sends the command.
constexpr uint16_t commandRpmRegisterAddress = 0; ///< screw speed, rpm * 100
constexpr uint16_t commandTargetRegisterAddress = 1; ///< barrel target temperature, degC
constexpr uint16_t commandRegisterCount = 2;

// Input registers, read by the mainboard as one block.
constexpr uint16_t statusTemperatureRegisterAddress = 16; ///< barrel temperature, degC * 10
constexpr uint16_t statusTargetRegisterAddress = 17; ///< target the extruder is regulating to, degC
constexpr uint16_t statusRpmRegisterAddress = 18; ///< screw speed the extruder is running at, rpm * 100
constexpr uint16_t statusFlagsRegisterAddress = 19; ///< see Flag
constexpr uint16_t statusReplyCounterRegisterAddress = 20; ///< incremented with every valid status line; a counter that stands still means a dead link
constexpr uint16_t statusRegisterCount = 5;

enum Flag : uint16_t {
    flag_printhead_mode = 1, ///< extruder accepted our commands and is in printhead mode
    flag_hot = 2, ///< barrel is above the extruder's own minimum extrusion temperature
    flag_motor_switch = 4, ///< the motor switch on the extruder is on
};

struct Status {
    uint16_t temperature_x10 = 0;
    uint16_t target = 0;
    uint16_t rpm_x100 = 0;
    uint16_t flags = 0;
};

/// Check value protecting a command line, so that a garbled line is never taken for a speed or a temperature.
constexpr uint16_t command_check(uint16_t rpm_x100, uint16_t target) {
    return (static_cast<uint32_t>(rpm_x100) + target) % 251;
}

namespace detail {
    constexpr size_t put(char *out, size_t pos, char c) {
        out[pos] = c;
        return pos + 1;
    }

    constexpr size_t put_uint(char *out, size_t pos, uint32_t value, int min_digits = 1) {
        char digits[10] = {};
        int count = 0;
        do {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value);
        for (int i = count; i < min_digits; ++i) {
            pos = put(out, pos, '0');
        }
        while (count) {
            pos = put(out, pos, digits[--count]);
        }
        return pos;
    }

    constexpr bool is_digit(char c) {
        return c >= '0' && c <= '9';
    }

    /// Parses a non-negative decimal number with at most \p decimals digits kept after the point.
    /// Advances \p text. Returns false if there is no number or it does not fit.
    constexpr bool parse_scaled(const char *&text, int decimals, uint16_t &out) {
        if (!is_digit(*text)) {
            return false;
        }
        uint32_t value = 0;
        while (is_digit(*text)) {
            value = value * 10 + static_cast<uint32_t>(*text++ - '0');
            if (value > 65535) {
                return false;
            }
        }
        int kept = 0;
        if (*text == '.') {
            ++text;
            while (is_digit(*text)) {
                if (kept < decimals) {
                    value = value * 10 + static_cast<uint32_t>(*text - '0');
                    ++kept;
                }
                ++text;
            }
        }
        for (; kept < decimals; ++kept) {
            value *= 10;
        }
        if (value > 65535) {
            return false;
        }
        out = static_cast<uint16_t>(value);
        return true;
    }

    constexpr bool expect(const char *&text, const char *literal) {
        while (*literal) {
            if (*text++ != *literal++) {
                return false;
            }
        }
        return true;
    }
} // namespace detail

constexpr size_t command_line_max_size = 32;

/// Writes the command line (including the newline, without a terminating zero) into \p out,
/// which has to hold command_line_max_size characters. Returns the length.
constexpr size_t format_command(char *out, uint16_t rpm_x100, uint16_t target) {
    using namespace detail;
    size_t pos = 0;
    for (const char c : { 'M', '9', '5', '1', ' ', 'S' }) {
        pos = put(out, pos, c);
    }
    pos = put_uint(out, pos, rpm_x100 / 100);
    pos = put(out, pos, '.');
    pos = put_uint(out, pos, rpm_x100 % 100, 2);
    pos = put(out, pos, ' ');
    pos = put(out, pos, 'T');
    pos = put_uint(out, pos, target);
    pos = put(out, pos, ' ');
    pos = put(out, pos, 'K');
    pos = put_uint(out, pos, command_check(rpm_x100, target));
    pos = put(out, pos, '\n');
    return pos;
}

/// Parses one zero-terminated status line (without the newline). Anything that is
/// not exactly a status line is rejected.
constexpr bool parse_status(const char *line, Status &out) {
    using namespace detail;
    Status status;
    if (!expect(line, "PH T:") || !parse_scaled(line, 1, status.temperature_x10)) {
        return false;
    }
    if (!expect(line, " /") || !parse_scaled(line, 0, status.target)) {
        return false;
    }
    if (!expect(line, " R:") || !parse_scaled(line, 2, status.rpm_x100)) {
        return false;
    }
    if (!expect(line, " F:") || !parse_scaled(line, 0, status.flags)) {
        return false;
    }
    if (*line == '\r') {
        ++line;
    }
    if (*line != '\0') {
        return false;
    }
    out = status;
    return true;
}

} // namespace xbuddy_extension_shared::artme_bridge
