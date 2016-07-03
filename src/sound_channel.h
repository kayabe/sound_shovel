#pragma once

// Project headers
#include "sample_block.h"

// Contrib headers
#include "df_lib_plus_plus/darray.h"

// Standard headers
#include <stdint.h>


class SoundChannel
{
public:
    struct SoundPos
    {
        int m_block_idx;
        int m_sample_idx;

        SoundPos()
        {
	        m_block_idx = 0;
    	    m_sample_idx = 0;
        }

        SoundPos(int block_idx, int sample_idx) 
        {
            m_block_idx = block_idx;
            m_sample_idx = sample_idx;
        }
    };

private:
    void CalcMinMaxForRange(SoundPos *pos, unsigned num_samples, int16_t *result_min, int16_t *result_max);

public:
    SoundPos GetSoundPosFromSampleIdx(int64_t sample_idx);
    SampleBlock *IncrementSoundPos(SoundPos *pos, int64_t num_samples);

    DArray <SampleBlock *> m_blocks;

    unsigned GetLength();

    void CalcDisplayData(int start_sample_idx, int16_t *mins, int16_t *maxes, unsigned width_in_pixels, double samples_per_pixel);
};