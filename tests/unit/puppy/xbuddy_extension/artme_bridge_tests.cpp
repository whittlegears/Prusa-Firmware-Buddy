#include <catch2/catch.hpp>
#include <string>
#include <xbuddy_extension_shared/artme_bridge.hpp>

using namespace xbuddy_extension_shared::artme_bridge;

static std::string command(uint16_t rpm_x100, uint16_t target) {
    char buffer[command_line_max_size];
    return std::string(buffer, format_command(buffer, rpm_x100, target));
}

TEST_CASE("ARTME bridge: command line") {
    // check value = (rpm * 100 + target) mod 251, the extruder firmware computes the same
    REQUIRE(command(123, 215) == "M951 S1.23 T215 K87\n");
    REQUIRE(command(5, 0) == "M951 S0.05 T0 K5\n");
    REQUIRE(command(0, 0) == "M951 S0.00 T0 K0\n");
    REQUIRE(command(65535, 65535).size() <= command_line_max_size);
}

TEST_CASE("ARTME bridge: status line") {
    Status status;

    REQUIRE(parse_status("PH T:215.3 /215 R:12.34 F:7", status));
    REQUIRE(status.temperature_x10 == 2153);
    REQUIRE(status.target == 215);
    REQUIRE(status.rpm_x100 == 1234);
    REQUIRE(status.flags == 7);

    REQUIRE(parse_status("PH T:21.0 /0 R:0.00 F:0\r", status));
    REQUIRE(status.temperature_x10 == 210);
    REQUIRE(status.rpm_x100 == 0);

    // Anything else on the line (other firmware output, noise, cut lines) is not a status
    for (const char *line : { "ok", "", "start", "PH T:215.3 /215 R:12.34", "PH T:-5.0 /0 R:0.00 F:0", "PH T:215.3 /215 R:12.34 F:7x", "PH T:99999.0 /0 R:0.00 F:0" }) {
        REQUIRE_FALSE(parse_status(line, status));
    }
    // ... and leaves the last good status alone
    REQUIRE(status.temperature_x10 == 210);
}
