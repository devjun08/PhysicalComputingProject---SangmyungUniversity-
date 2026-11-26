#include "hit_detector.h"
#include "config.h"
/* ================= 연속 판정 상태 ================= */
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
    /* 1) 9채널의 최솟값을 후보로 고르고 나머지 8채널을 기준으로 삼는다. */
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
    /* 2) 기준 채널들이 평균 근처에 모여 있는지 확인한다. */
    for (i = 0U; i < 9U; ++i) {
        if (i != lowest &&
            ((uint32_t)values[i] * 800U < peers * (100U - HIT_PEER_TOLERANCE_PERCENT) ||
             (uint32_t)values[i] * 800U > peers * (100U + HIT_PEER_TOLERANCE_PERCENT))) {
            uniform = 0U;
        }
    }
    /* 3) 연속으로 정상 범위가 확인되면 다음 피격 판정을 허용한다. */
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
    /* 4) 같은 채널의 하락이 연속 확인될 때 피격을 한 번 보고한다. */
    if (candidate != lowest + 1U) { candidate = lowest + 1U; confirmations = 0U; }
    if (++confirmations < HIT_CONFIRM_SAMPLES) return 0U;
    armed = 0U; confirmations = 0U;
    return candidate;
}
