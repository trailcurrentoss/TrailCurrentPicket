#!/bin/bash
# Build all Picket address variants (0-7).
# Produces merged binaries (bootloader + partition table + OTA data + app)
# that can be flashed at offset 0x0:
#   build/picket_addr0.bin .. picket_addr7.bin
set -e

MAX_ADDR=7
OUTPUT_DIR="build"

for addr in $(seq 0 $MAX_ADDR); do
    echo "========================================"
    echo "Building Picket address $addr ..."
    echo "========================================"
    idf.py build -DPICKET_ADDRESS=$addr

    # Create merged binary (flashable at 0x0, includes all partitions)
    esptool.py --chip esp32s3 merge_bin -o "$OUTPUT_DIR/picket_addr${addr}.bin" \
        --flash_mode dio --flash_size 4MB \
        0x0 "$OUTPUT_DIR/bootloader/bootloader.bin" \
        0x8000 "$OUTPUT_DIR/partition_table/partition-table.bin" \
        0xe000 "$OUTPUT_DIR/ota_data_initial.bin" \
        0x10000 "$OUTPUT_DIR/picket.bin"
    echo ""
done

echo "========================================"
echo "Build complete"
echo "========================================"
ls -lh "$OUTPUT_DIR"/picket_addr*.bin
