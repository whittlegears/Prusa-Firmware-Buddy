/// @file
/// ARTME 3D screw extruder used as the print head.
///
/// The extruder has its own controller that heats the barrel and turns the screw.
/// This module is what the rest of the printer firmware sees of it:
///  - the hotend temperature is the barrel temperature reported by the extruder,
///  - the hotend target temperature is forwarded to the extruder,
///  - movement of the (not connected) E axis is turned into screw speed.
///
/// See artme/README.md for wiring and calibration.
#pragma once

namespace artme_link {

/// Screw speed limit sent to the extruder (it also enforces its own EXTRUDER_RPM_MAX).
inline constexpr float max_rpm = 25;

/// Exchanges data with the extruder. Called from the marlin task (idle loop).
void step();

/// Barrel temperature in degC, 0 while the extruder is not answering.
/// Safe to call from any context.
float hotend_celsius();

/// @returns whether the extruder answered recently
bool is_online();

/// Screw speed currently requested from the extruder.
float commanded_rpm();

/// Volume of melt pushed out per screw revolution [mm3]. Stored in the config store.
float mm3_per_rev();
void set_mm3_per_rev(float value);

} // namespace artme_link
