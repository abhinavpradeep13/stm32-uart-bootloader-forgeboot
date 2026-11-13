
import argparse
import pathlib
import struct
import time
import zlib

import serial


# ============================================================
# FORGEBOOT CONFIGURATION
# ============================================================

SYNC_1 = 0xA5
SYNC_2 = 0x5A

CMD_BEGIN = 0x01
CMD_DATA = 0x02
CMD_END = 0x03
CMD_ABORT = 0x04
CMD_PING = 0x05

CMD_ACK = 0x80
CMD_NACK = 0x81

MAX_PAYLOAD = 256
OFFSET_SIZE = 4
MAX_DATA_SIZE = MAX_PAYLOAD - OFFSET_SIZE

INITIAL_SEQUENCE = 0
PING_SEQUENCE = 0xFFFF

APP_START_ADDR = 0x08008000
APP_END_ADDR = 0x08080000
APP_MAX_SIZE = APP_END_ADDR - APP_START_ADDR

DEFAULT_BAUDRATE = 115200

DEFAULT_RETRIES = 3
DEFAULT_TIMEOUT = 5
PING_TIMEOUT = 2


# ============================================================
# CRC32
# ============================================================

def calculate_crc32(data):
    """Calculate standard CRC-32."""
    return zlib.crc32(data) & 0xFFFFFFFF


# ============================================================
# PACKET GENERATION
# ============================================================

def build_packet(command, sequence, payload=b""):
    """
    Construct a ForgeBoot packet.

    Format:
        A5 5A | TYPE | SEQ16 | LEN16 | PAYLOAD | CRC32

    Multi-byte fields use little-endian encoding.
    CRC covers TYPE through PAYLOAD.
    """

    if not 0 <= command <= 0xFF:
        raise ValueError("Invalid command")

    if not 0 <= sequence <= 0xFFFF:
        raise ValueError("Invalid sequence number")

    if len(payload) > MAX_PAYLOAD:
        raise ValueError("Payload exceeds maximum length")

    header = struct.pack(
        "<BHH",
        command,
        sequence,
        len(payload)
    )

    crc_data = header + payload
    crc = calculate_crc32(crc_data)

    return (
        bytes([SYNC_1, SYNC_2])
        + crc_data
        + struct.pack("<I", crc)
    )


# ============================================================
# FIRMWARE VALIDATION
# ============================================================

def validate_firmware(image):
    """Validate application size and Cortex-M vector table."""

    image_size = len(image)

    if image_size < 8:
        raise ValueError("Firmware image is too small")

    if image_size > APP_MAX_SIZE:
        raise ValueError(
            "Firmware exceeds application Flash region"
        )

    initial_msp, reset_vector = struct.unpack_from(
        "<II", image, 0
    )

    reset_address = reset_vector & ~1

    # Validate initial stack pointer.
    msp_valid = (
        0x20000000 < initial_msp <= 0x20020000
        and initial_msp % 8 == 0
    )

    if not msp_valid:
        raise ValueError("Invalid initial stack pointer")

    # Validate Thumb bit and reset handler location.
    reset_valid = (
        (reset_vector & 1) == 1
        and APP_START_ADDR <= reset_address
        < APP_START_ADDR + image_size
    )

    if not reset_valid:
        raise ValueError("Invalid application reset vector")

    return initial_msp, reset_vector


# ============================================================
# FIRMWARE PACKET GENERATOR
# ============================================================

def generate_firmware_packets(image):
    """
    Generate BEGIN, DATA and END packets.

    DATA payload:
        4-byte offset + firmware data

    Maximum firmware data per packet: 252 bytes.
    """

    image_size = len(image)
    image_crc = calculate_crc32(image)

    sequence = INITIAL_SEQUENCE

    # BEGIN
    begin_payload = struct.pack(
        "<II",
        image_size,
        image_crc
    )

    yield build_packet(
        CMD_BEGIN,
        sequence,
        begin_payload
    )

    sequence = (sequence + 1) & 0xFFFF

    # DATA
    offset = 0

    while offset < image_size:

        chunk = image[offset:offset + MAX_DATA_SIZE]

        data_payload = (
            struct.pack("<I", offset)
            + chunk
        )

        yield build_packet(
            CMD_DATA,
            sequence,
            data_payload
        )

        offset += len(chunk)
        sequence = (sequence + 1) & 0xFFFF

    # END
    yield build_packet(
        CMD_END,
        sequence
    )


# ============================================================
# PACKET INFORMATION
# ============================================================

def get_packet_information(packet):

    command = packet[2]

    sequence = struct.unpack_from(
        "<H", packet, 3
    )[0]

    payload_length = struct.unpack_from(
        "<H", packet, 5
    )[0]

    return command, sequence, payload_length


def command_name(command):

    names = {
        CMD_BEGIN: "BEGIN",
        CMD_DATA: "DATA",
        CMD_END: "END",
        CMD_ABORT: "ABORT",
        CMD_PING: "PING",
        CMD_ACK: "ACK",
        CMD_NACK: "NACK"
    }

    return names.get(command, "UNKNOWN")


# ============================================================
# OFFLINE FIRMWARE INSPECTION
# ============================================================

def inspect_firmware(firmware_path):

    image = pathlib.Path(firmware_path).read_bytes()

    initial_msp, reset_vector = validate_firmware(image)

    image_size = len(image)
    image_crc = calculate_crc32(image)

    packets = list(generate_firmware_packets(image))

    print("\n========== FORGEBOOT IMAGE INSPECTION ==========")

    print(f"Firmware file : {firmware_path}")
    print(f"Image size    : {image_size} bytes")
    print(f"Image CRC32   : 0x{image_crc:08X}")

    print(f"Initial MSP   : 0x{initial_msp:08X}")
    print(f"Reset vector  : 0x{reset_vector:08X}")
    print(f"Reset address : 0x{reset_vector & ~1:08X}")

    counts = {
        CMD_BEGIN: 0,
        CMD_DATA: 0,
        CMD_END: 0
    }

    for packet in packets:

        command, _, _ = get_packet_information(packet)

        if command in counts:
            counts[command] += 1

    print("\n========== PACKET SUMMARY ==========")

    print(f"BEGIN packets : {counts[CMD_BEGIN]}")
    print(f"DATA packets  : {counts[CMD_DATA]}")
    print(f"END packets   : {counts[CMD_END]}")
    print(f"Total packets : {len(packets)}")

    print("\n========== PACKET DETAILS ==========")

    for index, packet in enumerate(packets):

        command, sequence, payload_length = (
            get_packet_information(packet)
        )

        print(
            f"Packet {index:03d} | "
            f"{command_name(command):5s} | "
            f"Sequence: {sequence:5d} | "
            f"Payload: {payload_length:3d} bytes | "
            f"Total: {len(packet):3d} bytes"
        )

    print("\n========== INSPECTION COMPLETE ==========")


# ============================================================
# SERIAL RESPONSE RECEPTION
# ============================================================

def read_exact(ser, length):
    """Read exactly the requested number of bytes."""

    data = bytearray()

    while len(data) < length:

        chunk = ser.read(length - len(data))

        if not chunk:
            raise TimeoutError("Serial response timed out")

        data.extend(chunk)

    return bytes(data)


def receive_response(ser):
    """
    Receive and validate one STM32 ACK/NACK packet.

    Synchronization: A5 5A
    """

    # Find synchronization bytes.
    previous_byte = None

    while True:

        current_byte = read_exact(ser, 1)[0]

        if previous_byte == SYNC_1 and current_byte == SYNC_2:
            break

        previous_byte = current_byte

    # Read header.
    header = read_exact(ser, 5)

    packet_type, sequence, payload_length = struct.unpack(
        "<BHH", header
    )

    if payload_length > MAX_PAYLOAD:
        raise ValueError("Invalid response payload length")

    # Read payload and CRC.
    payload = read_exact(ser, payload_length)

    received_crc = struct.unpack(
        "<I", read_exact(ser, 4)
    )[0]

    calculated_crc = calculate_crc32(
        header + payload
    )

    if received_crc != calculated_crc:
        raise ValueError("Response CRC mismatch")

    # Validate response.
    if packet_type == CMD_ACK:

        if payload_length != 0:
            raise ValueError("Invalid ACK payload")

        return {
            "type": "ACK",
            "sequence": sequence,
            "payload": payload
        }

    if packet_type == CMD_NACK:

        if payload_length != 1:
            raise ValueError("Invalid NACK payload")

        return {
            "type": "NACK",
            "sequence": sequence,
            "payload": payload
        }

    raise ValueError(
        f"Unexpected response command: 0x{packet_type:02X}"
    )


# ============================================================
# RELIABLE PACKET TRANSMISSION
# ============================================================

def transmit_packet(
    ser,
    packet,
    sequence,
    retries=DEFAULT_RETRIES,
    timeout=DEFAULT_TIMEOUT
):
    """
    Send one packet and wait for its matching ACK.

    Retransmits on timeout or malformed responses.
    NACK is treated as a definite packet rejection.
    """

    original_timeout = ser.timeout

    try:

        ser.timeout = timeout

        for attempt in range(1, retries + 1):

            print(
                f"[TX] Sequence {sequence} | "
                f"Attempt {attempt}/{retries}"
            )

            ser.write(packet)
            ser.flush()

            try:

                response = receive_response(ser)

                if response["sequence"] != sequence:

                    print(
                        "[WARN] Response sequence mismatch"
                    )

                    continue

                if response["type"] == "ACK":

                    print(f"[ACK] Sequence {sequence}")

                    return True

                if response["type"] == "NACK":

                    reason = response["payload"][0]

                    raise RuntimeError(
                        f"STM32 rejected packet. "
                        f"NACK reason: 0x{reason:02X}"
                    )

            except (TimeoutError, ValueError) as error:

                print(f"[WARN] {error}")

        raise RuntimeError(
            f"Packet {sequence} failed after {retries} attempts"
        )

    finally:

        ser.timeout = original_timeout


# ============================================================
# RECOVERY MODE PREFLIGHT
# ============================================================

def check_recovery_mode(ser):
    """
    Verify that ForgeBoot is in recovery mode.

    Sends PING with sequence 0xFFFF.

    Upload is allowed only after receiving a valid,
    matching ACK packet.
    """

    print("\n========== RECOVERY MODE CHECK ==========")

    ping_packet = build_packet(
        CMD_PING,
        PING_SEQUENCE
    )

    print("[PING] Checking bootloader recovery mode...")

    try:

        transmit_packet(
            ser,
            ping_packet,
            PING_SEQUENCE,
            retries=1,
            timeout=PING_TIMEOUT
        )

        print("[SUCCESS] ForgeBoot recovery mode detected.")

        return True

    except (RuntimeError, TimeoutError, ValueError) as error:

        print("\n[ERROR] Recovery mode was not detected.")
        print("[INFO] Firmware upload has been blocked.")
        print("[INFO] BEGIN was not transmitted.")
        print("[INFO] Application Flash has not been erased.")
        print(f"[DETAIL] {error}")

        return False


# ============================================================
# ABORT RECOVERY
# ============================================================

def attempt_abort(ser, sequence):
    """Attempt to abort an interrupted firmware transfer."""

    abort_packet = build_packet(
        CMD_ABORT,
        sequence
    )

    print(
        "\n[ABORT] Attempting to reset firmware update state..."
    )

    try:

        transmit_packet(
            ser,
            abort_packet,
            sequence,
            retries=2,
            timeout=2
        )

        print("[ABORT] Bootloader reset to IDLE.")

    except Exception as error:

        print(f"[WARN] ABORT attempt failed: {error}")


# ============================================================
# FIRMWARE UPLOAD
# ============================================================

def upload_firmware(port, firmware_path):

    firmware_path = pathlib.Path(firmware_path)

    if not firmware_path.is_file():
        raise FileNotFoundError(
            f"Firmware file not found: {firmware_path}"
        )

    image = firmware_path.read_bytes()

    # Validate before opening serial port.
    validate_firmware(image)

    packets = list(generate_firmware_packets(image))

    print("\n========== FORGEBOOT UPLOAD ==========")

    print(f"Serial port : {port}")
    print(f"Firmware    : {firmware_path}")
    print(f"Image size  : {len(image)} bytes")
    print(f"Packets     : {len(packets)}")

    with serial.serial_for_url(
        port,
        baudrate=DEFAULT_BAUDRATE,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=1,
        write_timeout=5
    ) as ser:

        time.sleep(0.5)

        # Remove startup messages and stale serial data.
        ser.reset_input_buffer()

        # ----------------------------------------------------
        # RECOVERY MODE PREFLIGHT
        # ----------------------------------------------------

        if not check_recovery_mode(ser):

            raise RuntimeError(
                "Upload cancelled. Hold PB12 while resetting "
                "the STM32 to enter ForgeBoot recovery mode."
            )

        # ----------------------------------------------------
        # FIRMWARE TRANSFER
        # ----------------------------------------------------

        print("\n========== STARTING FIRMWARE TRANSFER ==========")

        last_sequence = INITIAL_SEQUENCE

        try:

            for index, packet in enumerate(packets):

                _, sequence, _ = get_packet_information(packet)

                last_sequence = sequence

                # BEGIN performs application Flash erase.
                if index == 0:
                    timeout = 30
                else:
                    timeout = DEFAULT_TIMEOUT

                transmit_packet(
                    ser,
                    packet,
                    sequence,
                    retries=DEFAULT_RETRIES,
                    timeout=timeout
                )

                progress = (
                    (index + 1) / len(packets)
                ) * 100

                print(
                    f"[PROGRESS] {progress:.1f}% "
                    f"({index + 1}/{len(packets)})"
                )

        except (Exception, KeyboardInterrupt):

            attempt_abort(ser, last_sequence)
            raise

    print("\n[FORGEBOOT] Transfer completed successfully.")


# ============================================================
# OFFLINE SERIAL SIMULATION
# ============================================================

class MockSerial:
    """Simulate an STM32 serial connection."""

    def __init__(self, response=b""):

        self.response = bytearray(response)
        self.timeout = 10
        self.last_written = None

    def reset_input_buffer(self):
        self.response.clear()

    def write(self, data):

        self.last_written = data

        sequence = struct.unpack_from(
            "<H", data, 3
        )[0]

        self.response.extend(
            build_packet(CMD_ACK, sequence)
        )

        return len(data)

    def flush(self):
        pass

    def read(self, size=1):

        data = self.response[:size]

        del self.response[:size]

        return bytes(data)


class LostAckMockSerial(MockSerial):
    """Simulate losing the first ACK."""

    def __init__(self):

        super().__init__()
        self.transmission_count = 0

    def write(self, data):

        self.transmission_count += 1
        self.last_written = data

        sequence = struct.unpack_from(
            "<H", data, 3
        )[0]

        # Deliberately lose the first ACK.
        if self.transmission_count == 1:
            return len(data)

        # Respond normally to retransmission.
        self.response.extend(
            build_packet(CMD_ACK, sequence)
        )

        return len(data)


# ============================================================
# OFFLINE TESTS
# ============================================================

def test_crc32():

    result = calculate_crc32(b"123456789")

    assert result == 0xCBF43926

    print("[PASS] Standard CRC32 verification")


def test_ack_receiver():

    sequence = 5

    ack_packet = build_packet(
        CMD_ACK,
        sequence
    )

    mock = MockSerial(ack_packet)

    response = receive_response(mock)

    assert response["type"] == "ACK"
    assert response["sequence"] == sequence
    assert response["payload"] == b""

    print("[PASS] ACK packet parsing")
    print("[PASS] ACK sequence validation")
    print("[PASS] Empty ACK payload validation")


def test_nack_receiver():

    sequence = 3
    reason = 0x05

    nack_packet = build_packet(
        CMD_NACK,
        sequence,
        bytes([reason])
    )

    mock = MockSerial(nack_packet)

    response = receive_response(mock)

    assert response["type"] == "NACK"
    assert response["sequence"] == sequence
    assert response["payload"] == bytes([reason])

    print("[PASS] NACK packet parsing")
    print("[PASS] NACK sequence validation")
    print("[PASS] NACK reason extraction")


def test_transmission():

    sequence = 7

    packet = build_packet(
        CMD_DATA,
        sequence,
        struct.pack("<I", 0) + b"\x12\x34\x56\x78"
    )

    mock = MockSerial()

    result = transmit_packet(
        mock,
        packet,
        sequence,
        retries=3,
        timeout=2
    )

    assert result is True
    assert mock.last_written == packet

    print("[PASS] Packet transmission")
    print("[PASS] Matching ACK received")
    print("[PASS] Transmission completed successfully")


def test_retransmission():

    sequence = 8

    packet = build_packet(
        CMD_DATA,
        sequence,
        struct.pack("<I", 4) + b"\x11\x22\x33\x44"
    )

    mock = LostAckMockSerial()

    result = transmit_packet(
        mock,
        packet,
        sequence,
        retries=3,
        timeout=2
    )

    assert result is True
    assert mock.transmission_count == 2
    assert mock.last_written == packet

    print("[PASS] Lost ACK detection")
    print("[PASS] Automatic packet retransmission")
    print("[PASS] Recovery after retransmission")


def test_nack_rejection():

    sequence = 9
    reason = 0x05

    class NackMockSerial(MockSerial):

        def write(self, data):

            self.last_written = data

            self.response.extend(
                build_packet(
                    CMD_NACK,
                    sequence,
                    bytes([reason])
                )
            )

            return len(data)

    packet = build_packet(CMD_DATA, sequence)

    mock = NackMockSerial()

    try:

        transmit_packet(
            mock,
            packet,
            sequence,
            retries=3,
            timeout=2
        )

    except RuntimeError as error:

        assert "NACK reason: 0x05" in str(error)

        print("[PASS] NACK rejection detected")
        print("[PASS] Flash error reason identified")

        return

    raise AssertionError("NACK was not handled correctly")


def test_recovery_mode_check():

    mock = MockSerial()

    result = check_recovery_mode(mock)

    assert result is True

    command, sequence, payload_length = (
        get_packet_information(mock.last_written)
    )

    assert command == CMD_PING
    assert sequence == PING_SEQUENCE
    assert payload_length == 0

    print("[PASS] Recovery-mode PING packet")
    print("[PASS] Recovery-mode ACK validation")


def test_recovery_mode_rejection():

    class NoResponseMock(MockSerial):

        def write(self, data):
            self.last_written = data
            return len(data)

    mock = NoResponseMock()

    result = check_recovery_mode(mock)

    assert result is False

    command, sequence, _ = (
        get_packet_information(mock.last_written)
    )

    assert command == CMD_PING
    assert sequence == PING_SEQUENCE

    print("[PASS] Recovery-mode timeout detection")
    print("[PASS] Upload preflight rejection")


def run_self_tests():

    print("\n========== FORGEBOOT OFFLINE TESTS ==========\n")

    test_crc32()
    test_ack_receiver()
    test_nack_receiver()
    test_transmission()
    test_retransmission()
    test_nack_rejection()
    test_recovery_mode_check()
    test_recovery_mode_rejection()

    print("\n========== ALL TESTS PASSED ==========\n")


# ============================================================
# COMMAND LINE INTERFACE
# ============================================================

def main():

    parser = argparse.ArgumentParser(
        description="ForgeBoot v0.2 STM32 Firmware Uploader"
    )

    parser.add_argument(
        "firmware",
        nargs="?",
        help="Path to application BIN file"
    )

    parser.add_argument(
        "--port",
        help="Serial COM port, e.g. COM19"
    )

    parser.add_argument(
        "--upload",
        action="store_true",
        help="Upload firmware to STM32"
    )

    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Run offline protocol tests"
    )

    args = parser.parse_args()

    if args.self_test:

        run_self_tests()
        return

    if not args.firmware:

        parser.error("Firmware BIN file is required")

    if args.upload:

        if not args.port:
            parser.error("--upload requires --port")

        upload_firmware(
            args.port,
            args.firmware
        )

    else:

        inspect_firmware(args.firmware)


if __name__ == "__main__":
    main()
