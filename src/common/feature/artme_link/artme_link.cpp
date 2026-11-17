/// @file
#include <feature/artme_link/artme_link.hpp>

#include <algorithm>
#include <atomic>
#include <bsod.h>
#include <config_store/store_instance.hpp>
#include <logging/log.hpp>
#include <module/planner.h>
#include <module/stepper.h>
#include <module/temperature.h>
#include <numbers>
#include <puppies/xbuddy_extension.hpp>
#include <timing.h>

LOG_COMPONENT_DEF(ArtmeLink, logging::Severity::info);

namespace artme_link {

namespace {

    constexpr uint32_t period_ms = 100;

    /// The extruder is asked ten times a second. Not hearing from it for this long means
    /// it is off, unplugged or stuck, and its temperature is unknown.
    constexpr uint32_t offline_after_ms = 3000;

    /// Slicers assume this filament when they compute E moves, so E length times its
    /// cross-section is the volume the screw has to deliver.
    constexpr float filament_area_mm2 = std::numbers::pi_v<float> * (DEFAULT_NOMINAL_FILAMENT_DIA / 2.f) * (DEFAULT_NOMINAL_FILAMENT_DIA / 2.f);

    /// A screw cannot pull melt back. Retractions up to this length, and the moves that
    /// undo them, are ignored instead of being turned into a burst of extra material.
    constexpr float retraction_window_mm = 5;

    std::atomic<float> celsius = 0;
    std::atomic<bool> online = false;
    std::atomic<float> rpm = 0;

    uint32_t last_step_ms = 0;
    bool e_tracking_started = false;
    float e_high_water_mm = 0;

    /// @returns filament length [mm] newly extruded since the last call
    float extruded_since_last_call() {
        const float e_mm = stepper.position_from_startup(E_AXIS) / planner.settings.axis_steps_per_mm[E_AXIS];
        if (!e_tracking_started) {
            e_tracking_started = true;
            e_high_water_mm = e_mm;
            return 0;
        }
        float extruded = 0;
        if (e_mm > e_high_water_mm) {
            extruded = e_mm - e_high_water_mm;
            e_high_water_mm = e_mm;
        } else {
            e_high_water_mm = std::min(e_high_water_mm, e_mm + retraction_window_mm);
        }
        return extruded;
    }

} // namespace

void step() {
    const uint32_t now = ticks_ms();
    const int32_t elapsed_ms = ticks_diff(now, last_step_ms);
    if (elapsed_ms < static_cast<int32_t>(period_ms)) {
        return;
    }
    last_step_ms = now;

    const auto state = buddy::puppies::xbuddy_extension.get_artme_state();
    const bool now_online = state && state->age_ms < offline_after_ms;
    if (now_online != online.load()) {
        log_info(ArtmeLink, "extruder %s", now_online ? "online" : "offline");
    }
    online = now_online;
    celsius = now_online ? state->temperature : 0.f;

    const auto target = thermalManager.degTargetHotend(0);
    if (!now_online && target > 0) {
        // Heating or printing with an extruder we know nothing about. The extruder shuts
        // its screw and heater down by itself when our commands stop.
        fatal_error("ARTME extruder is not responding. Check that it is switched on and its data cable is connected.", "ARTME link");
    }

    // Screw speed that delivers the volume the E axis moved in the last period,
    // smoothed over a few periods: the melt cannot follow anything faster anyway.
    const float flow_mm3_s = extruded_since_last_call() * filament_area_mm2 * 1000.f / elapsed_ms;
    const float wanted_rpm = now_online ? std::min(flow_mm3_s * 60.f / mm3_per_rev(), max_rpm) : 0.f;
    float new_rpm = 0.5f * rpm.load() + 0.5f * wanted_rpm;
    if (wanted_rpm == 0 && new_rpm < 0.02f) {
        new_rpm = 0;
    }
    rpm = new_rpm;

    buddy::puppies::xbuddy_extension.set_artme_command(new_rpm, static_cast<uint16_t>(std::max<int>(target, 0)));
}

float hotend_celsius() {
    return celsius;
}

bool is_online() {
    return online;
}

float commanded_rpm() {
    return rpm;
}

float mm3_per_rev() {
    return std::max(config_store().artme_mm3_per_rev.get(), 1.f);
}

void set_mm3_per_rev(float value) {
    config_store().artme_mm3_per_rev.set(std::max(value, 1.f));
}

} // namespace artme_link
