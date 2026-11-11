#ifndef MOON_VEHICLE_INPUT_H
#define MOON_VEHICLE_INPUT_H
/* Per-FixedUpdate value snapshot. Never retain it or a vehicle pointer. */
typedef struct {unsigned int valid;float steer,throttle;unsigned int brake,boost;} MoonVehicleInput;
_Static_assert(sizeof(MoonVehicleInput)==20,"vehicle input ABI v1");
static inline int moon_vehicle_input_valid(const MoonVehicleInput *input){
    return input&&input->valid==1&&input->steer>=-1&&input->steer<=1&&
        input->throttle>=-1&&input->throttle<=1&&input->brake<=1&&input->boost<=1;
}
#endif
