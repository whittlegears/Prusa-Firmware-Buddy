/// @file
#include <feature/artme_link/artme_link.hpp>

#include <gcode/gcode_parser.hpp>
#include <marlin_stubs/PrusaGcodeSuite.hpp>
#include <puppies/xbuddy_extension.hpp>

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M951: ARTME screw extruder link
 *
 * Only in firmware built with ARTME_LINK.
 *
 *#### Usage
 *
 *    M951 [ V ]
 *
 *#### Parameters
 *
 * - `V` - Melt volume pushed out per screw revolution in mm3. Stored permanently.
 *         Raise it if the printer over-extrudes, lower it if it under-extrudes.
 *
 * Without parameters reports the state of the link.
 */
void PrusaGcodeSuite::M951() {
    if (parser.seenval('V')) {
        artme_link::set_mm3_per_rev(parser.value_float());
    }

    SERIAL_ECHOPAIR("ARTME link: ", artme_link::is_online() ? "online" : "offline");
    SERIAL_ECHOPAIR_F(" mm3/rev:", artme_link::mm3_per_rev());
    SERIAL_ECHOPAIR_F(" commanded rpm:", artme_link::commanded_rpm());
    if (const auto state = buddy::puppies::xbuddy_extension.get_artme_state()) {
        SERIAL_ECHOPAIR_F(" T:", state->temperature);
        SERIAL_ECHOPAIR(" /", state->target);
        SERIAL_ECHOPAIR_F(" rpm:", state->rpm);
        SERIAL_ECHOPAIR(" flags:", state->flags);
        SERIAL_ECHOPAIR(" age ms:", state->age_ms);
    }
    SERIAL_EOL();
}

/** @}*/
