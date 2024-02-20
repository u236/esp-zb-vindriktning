#ifndef SCD40_H
#define SCD40_H

#define SCD40_ADDRESS                       0x62

#define SCD40_WAKE_UP                       0x36F6
#define SCD40_REINIT                        0x3646
#define SCD40_GET_SERIAL_NUMBER             0x3682
#define SCD40_STOP_PERIODIC_MEASUREMENT     0x3F86
#define SCD40_START_PERIODIC_MEASUREMENT    0x21B1
#define SCD40_READ_MEASUREMENT              0xEC05
#define SCD40_PERFORM_FORCED_RECALIBRATION  0x362F

void scd40_init(void);

#endif
