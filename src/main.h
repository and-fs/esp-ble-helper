# pragma once
/*
Important: number of services is restricted to 8 by default.
For adding more you have to change the GATT_MAX_SR_PROFILES definition
in common/bt_target.h to a higher value and rebuild your project.
*/

extern "C" void app_main(void);