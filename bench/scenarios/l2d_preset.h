/**
 * Short board preset. Do not change these numbers to match a new sample.
 * A longer run is a different preset version.
 *
 * L2D_PC_COMMIT in the firmware metadata is the historical editor snapshot
 * 78fc634. It is not the identity of this runtime.
 */
#ifndef L2D_PRESET_H
#define L2D_PRESET_H

#define L2D_PRESET_SHORT_NAME "short-v1"
#define L2D_PRESET_SHORT_WARMUP 5
#define L2D_PRESET_SHORT_MEASURE 50
#define L2D_PRESET_SHORT_ROUNDS 1
#define L2D_PRESET_SHORT_SCALE 1.0f
#define L2D_PRESET_SHORT_DEADLINE_US 33333

static inline unsigned char l2d_preset_triangle_sample(unsigned frame_index,
                                                       unsigned phase_offset,
                                                       unsigned char min_sample,
                                                       unsigned char max_sample)
{
    unsigned span = (unsigned)(max_sample - min_sample);
    unsigned position;
    if (span == 0) {
        return min_sample;
    }
    position = (frame_index + phase_offset) % (span * 2u);
    if (position <= span) {
        return (unsigned char)(min_sample + position);
    }
    return (unsigned char)(max_sample - (position - span));
}

#endif
