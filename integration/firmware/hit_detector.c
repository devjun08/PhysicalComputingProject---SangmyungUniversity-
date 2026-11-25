#include "hit_detector.h"
#include "config.h"
static uint8_t armed, candidate, confirmations, releases, sampled;
static uint32_t last_sample;
void HitDetector_Reset(void)
{
    armed = candidate = confirmations = releases = sampled = 0U;
    last_sample = 0U;
}
uint8_t HitDetector_Update(const uint16_t values[9], uint32_t now)
{
    uint32_t total = 0U, peers;
    uint8_t i, lowest = 0U, uniform = 1U;
    if (sampled && now - last_sample < SENSOR_PERIOD_MS) return 0U;
    /* A missed scan breaks consecutive confirmation. */
    if (sampled && now - last_sample > SENSOR_PERIOD_MS * 2U) {
        confirmations = releases = 0U;
    }
    sampled = 1U; last_sample = now;
    for (i = 0U; i < 9U; ++i) {
        if (values[i] > 4095U) {
            confirmations = releases = 0U; return 0U;
        }
        total += values[i];
        if (values[i] < values[lowest]) lowest = i;
    }
    peers = total - values[lowest]; /* Eight times the reference average. */
    if (peers < HIT_REFERENCE_MIN * 8U) {
        confirmations = releases = 0U; return 0U;
    }
    for (i = 0U; i < 9U; ++i) {
        if (i != lowest &&
            ((uint32_t)values[i] * 800U < peers * (100U - HIT_PEER_TOLERANCE_PERCENT) ||
             (uint32_t)values[i] * 800U > peers * (100U + HIT_PEER_TOLERANCE_PERCENT))) {
            uniform = 0U;
        }
    }
    if (uniform && (uint32_t)values[lowest] * 800U >= peers * HIT_RELEASE_PERCENT) {
        confirmations = 0U;
        if (releases < HIT_RELEASE_SAMPLES) ++releases;
        if (releases >= HIT_RELEASE_SAMPLES) armed = 1U;
        return 0U;
    }
    releases = 0U;
    if (!uniform || !armed ||
        (uint32_t)values[lowest] * 800U > peers * HIT_RATIO_PERCENT) {
        confirmations = 0U; return 0U;
    }
    if (candidate != lowest + 1U) { candidate = lowest + 1U; confirmations = 0U; }
    if (++confirmations < HIT_CONFIRM_SAMPLES) return 0U;
    armed = 0U; confirmations = 0U;
    return candidate;
}
