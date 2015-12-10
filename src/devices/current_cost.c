#include "rtl_433.h"
#include "pulse_demod.h"
#include "util.h"
#include "data.h"

/// Seek a bit pattern in one row of a bitbuffer
///
/// @param bitbuffer: The bitbuffer to work on
/// @param row: Only the given row will be analyzed
/// @param pattern: The pattern to seek
/// @param pattern_bits_len: Size of the pattern (in number of bits)
/// @return 0 if pattern wasn't found else number of the bit just after the pattern
static unsigned int _seek_init_pattern(bitbuffer_t *bitbuffer, unsigned int row, uint8_t * pattern, unsigned int pattern_bits_len){
    uint8_t *bits = bitbuffer->bb[row];
    unsigned int len = bitbuffer->bits_per_row[row];

    unsigned int ipos = 0;  // cursor on input bits
    unsigned int ppos = 0;  // cursor on init pattern

    while(ipos < len && ppos < pattern_bits_len){
        uint8_t ibit = (bits[ipos>>3] >> (7-(ipos&0x07))) & 0x01;
        //uint8_t ibit = (bits[ipos/8] >> (7-ipos%8)) & 0x01;
        uint8_t pbit = (pattern[ppos>>3] >> (7-(ppos&0x07))) & 0x01;
        //uint8_t pbit = (pattern[ppos/8] >> (7-ppos%8)) & 0x01;
        if(ibit == pbit){ // pattern match current bit
            ppos++;
            ipos++;
        } else { // mismatch
            ipos += -ppos + 1;
            ppos = 0;
        }
    }
    return (ppos == pattern_bits_len) ? ipos : 0;
}

/// Seek a bit pattern in one row of a bitbuffer
/// 
/// Falling edges => 0
/// Rising edges  => 1
///
/// @param bitbuffer: The bitbuffer to work on
/// @param row: Only the given row will be analyzed
/// @param start: Bit number where to start decoding
/// @param buffer: Output buffer
/// @param buffer_size: Maximum capacity (in bytes) of output buffer
/// @return number of decoded bytes
static int _decode_manchester(bitbuffer_t *bitbuffer, unsigned int row, unsigned int start, uint8_t *buffer, unsigned int buffer_size){
    uint8_t *bits = bitbuffer->bb[row];
    unsigned int len = bitbuffer->bits_per_row[row];

    unsigned int ipos = start;
    unsigned int bpos = 0;
    while(ipos < len && bpos < buffer_size*8){
        if(bpos%8 == 0){ // raz next bytes of buffer
            buffer[bpos/8] = 0x00;
        }
        uint8_t bit1 = (bits[ipos/8] >> (7-ipos%8)) & 0x01;
        ipos++;
        uint8_t bit2 = (bits[ipos/8] >> (7-ipos%8)) & 0x01;
        ipos++;
        if(bit1 == 1 && bit2 == 0){ // Falling edge
            bpos++;
        } else if (bit1 == 0 && bit2 == 1) { // Rising edge
            buffer[bpos/8] |= 0x1 << (7-bpos%8);
            bpos++;
        } else {
            // end of the data or err
            break;
        }
    }
    return (7+bpos)/8; // number of "started" bytes
}

static int current_cost_callback(bitbuffer_t *bitbuffer) {
    bitrow_t *bb = bitbuffer->bb;
    uint8_t *b = bb[0];

    time_t time_now;
    char time_str[LOCAL_TIME_BUFLEN];
    data_t *data;
    time(&time_now);
    local_time_str(time_now, time_str);

    uint8_t init_pattern[] = {
      0b11001100, //8
      0b11001100, //16
      0b11001100, //24
      0b11001110, //32
      0b10010001, //40
      0b01011101, //45 (! last 3 bits is not init)
    };
    unsigned int start_pos = _seek_init_pattern(bitbuffer, 0, init_pattern, 45);

    if(start_pos == 0){
        return 0;
    }

    uint8_t packet[8];
    unsigned int nb_bytes = _decode_manchester(bitbuffer, 0, start_pos, packet, 8);
    // Read data
      if(nb_bytes >= 7 && packet[0] == 0x0d){
        //TODO: add rolling code b[1] ? test needed
        uint16_t watt0 = (packet[2] & 0x7F) << 8 | packet[3] ;
        uint16_t watt1 = (packet[4] & 0x7F) << 8 | packet[5] ;
        uint16_t watt2 = (packet[6] & 0x7F) << 8 | packet[7] ;
        data = data_make("time",          "",       DATA_STRING, time_str,
                "model",         "",              DATA_STRING, "CurrentCost TX", //TODO: it may have different CC Model ? any ref ?
                //"rc",            "Rolling Code",  DATA_INT, rc,
                "power0",       "Power 0",       DATA_FORMAT, "%d W", DATA_INT, watt0,
                "power1",       "Power 1",       DATA_FORMAT, "%d W", DATA_INT, watt1,
                "power2",       "Power 2",       DATA_FORMAT, "%d W", DATA_INT, watt2,
                //"battery",       "Battery",       DATA_STRING, battery_low ? "LOW" : "OK", //TODO is there some low battery indicator ?
                NULL);
        data_acquired_handler(data);
        return 1;
    }
    return 0;
}

static char *output_fields[] = {
    "time",
    "model",
    "rc",
    "power0",
    "power1",
    "power2",
    NULL
};

r_device current_cost = {
    .name           = "CurrentCost Sensor",
    .modulation     = FSK_PULSE_PCM,
    .short_limit    = 62,
    .long_limit     = 62, // NRZ
    .reset_limit    = 2000,
    .json_callback  = &current_cost_callback,
    .disabled       = 0,
    .fields         = output_fields,
};
