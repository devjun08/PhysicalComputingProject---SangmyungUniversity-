#ifndef INTEGRATION_CONFIG_H
#define INTEGRATION_CONFIG_H
/* Existing wiring is retained, including servo PB9 / TIM4_CH4. */
#define MOTOR_PWM_COUNTS 800U
#define MOTOR_HOME_DUTY 80U
#define MOTOR_PATROL_DUTY 50U
#define MOTOR_TIMEOUT_MS 4500U
#define MOTOR_TRAVEL_MIN_MS 1500U
#define MOTOR_TRAVEL_MAX_MS 4000U
#define MOTOR_STOP_MS 30U
#define MOTOR_LEAVE_MS 300U
#define MOTOR_CENTER_TRIM_MS 0
#define SENSOR_PERIOD_MS 10U
/* Conservative relative detection: compare one channel against the other eight. */
#ifndef HIT_DETECTION_ENABLED
#define HIT_DETECTION_ENABLED 1
#endif
#define HIT_RATIO_PERCENT 75U
#define HIT_RELEASE_PERCENT 90U
#define HIT_PEER_TOLERANCE_PERCENT 15U
#define HIT_CONFIRM_SAMPLES 3U
#define HIT_RELEASE_SAMPLES 3U
#define HIT_REFERENCE_MIN 128U
/* Original servo test only established 90 degrees. Confirm mechanical poses. */
#define SERVO_STAND_ANGLE 90U
#define SERVO_DOWN_ANGLE 90U
#define SERVO_POSES_CONFIRMED 0
#define HIT_BRAKE_MS 100U
#define TARGET_DOWN_MS 1500U
#define TARGET_RECOVER_MS 300U
/* 0 = no automatic game timeout, since no team duration was supplied. */
#define GAME_DURATION_MS 0U
#endif
