/// @file
#include "artme.hpp"

#include "hal.hpp"
#include <span>

using namespace xbuddy_extension_shared::artme_bridge;

void Artme::drain_rx() {
    std::byte buffer[16];
    for (;;) {
        const auto received = hal::mmu::receive(buffer);
        if (received.empty()) {
            return;
        }
        for (const std::byte byte : received) {
            const char c = static_cast<char>(byte);
            if (c == '\n') {
                line[line_length] = '\0';
                if (!line_overflow && parse_status(line, status)) {
                    ++reply_counter;
                }
                line_length = 0;
                line_overflow = false;
            } else if (line_length + 1 < sizeof(line)) {
                line[line_length++] = c;
            } else {
                line_overflow = true;
            }
        }
    }
}

modbus::Callbacks::Status Artme::read_register(uint8_t, uint16_t address, uint16_t &out) {
    drain_rx();
    switch (address) {
    case statusTemperatureRegisterAddress:
        out = status.temperature_x10;
        return Status::Ok;
    case statusTargetRegisterAddress:
        out = status.target;
        return Status::Ok;
    case statusRpmRegisterAddress:
        out = status.rpm_x100;
        return Status::Ok;
    case statusFlagsRegisterAddress:
        out = status.flags;
        return Status::Ok;
    case statusReplyCounterRegisterAddress:
        out = reply_counter;
        return Status::Ok;
    }
    return Status::IllegalAddress;
}

modbus::Callbacks::Status Artme::write_register(uint8_t, uint16_t address, uint16_t value) {
    switch (address) {
    case commandRpmRegisterAddress:
        command_rpm_x100 = value;
        return Status::Ok;
    case commandTargetRegisterAddress: {
        // Last register of the command block: both values are here, send them.
        // The answer is picked up by a later read, we do not wait for it.
        drain_rx();
        char command[command_line_max_size];
        const size_t length = format_command(command, command_rpm_x100, value);
        hal::mmu::transmit(std::as_bytes(std::span { command, length }));
        return Status::Ok;
    }
    }
    return Status::IllegalAddress;
}
