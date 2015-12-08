#include "rtl_433.h"
#include "pulse_demod.h"
#include "util.h"
#include "data.h"

static int current_cost_callback(bitbuffer_t *bitbuffer) {
    bitrow_t *bb = bitbuffer->bb;
    uint8_t *b = bb[0];

    time_t time_now;
    char time_str[LOCAL_TIME_BUFLEN];
    data_t *data;
    time(&time_now);
    local_time_str(time_now, time_str);

    // Read data
    if(b[0] == 0x0d){
        //TODO: add check on packet len
        //TODO: add rolling code b[1] ? test needed
        uint16_t watt0 = (b[2] & 0x7F) << 8 | b[3] ;
        uint16_t watt1 = b[4] << 7 | b[5] >> 1;
        uint16_t watt2 = b[6] << 7 | b[7] >> 1;
        data = data_make("time",          "",       DATA_STRING, time_str,
                "model",         "",              DATA_STRING, "CurrentCost TX", //TODO: it may have different CC Model ? any ref ?
                //"rc",            "Rolling Code",  DATA_INT, rc,
                "power0",       "Power 0",       DATA_INT, watt0,
                "power1",       "Power 1",       DATA_INT, watt1,
                "power2",       "Power 2",       DATA_INT, watt2,
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
    .modulation     = FSK_PULSE_MANCHESTER_FRAMED,
    .short_limit    = 62,
    .long_limit     = 260, // not used
    .reset_limit    = 1000, // not used
    .json_callback  = &current_cost_callback,
    .disabled       = 0,
    .fields         = output_fields,
};
