// Own header
#include "sound_view.h"

// Project headers
#include "main.h"
#include "sound.h"
#include "sound_channel.h"

// Contrib headers
#include "df_bitmap.h"
#include "df_common.h"
#include "df_input.h"
#include "df_hi_res_time.h"
#include "df_text_renderer.h"

// Standard headers
#include <math.h>
#include <stdlib.h>


// ***************************************************************************
// Private Methods
// ***************************************************************************

void SoundView::UpdateDisplaySize(int pixel_width)
{
    if (m_display_width == pixel_width)
        return;

    delete[] m_display_mins;
    delete[] m_display_maxes;

    m_display_width = pixel_width;
    m_display_mins = new int16_t[pixel_width];
    m_display_maxes = new int16_t[pixel_width];
}


void SoundView::AdvanceSelection()
{
    if (g_inputManager.lmbClicked)
    {
        m_selection_start = GetSampleIndexFromScreenPos(g_inputManager.mouseX);
    }
}


void SoundView::AdvancePlaybackPos()
{
    double target_idx = m_sound->m_playback_idx;
    if (target_idx < 0)
        return;
    
    if (target_idx > m_playback_pos + 10000.0)
    {
        m_playback_pos = target_idx;
    }
    else
    {
        m_playback_pos = target_idx * 0.1 + m_playback_pos * 0.9;
    }
}


void SoundView::RenderMarker(BitmapRGBA *bmp, int64_t sample_idx, RGBAColour col)
{
    double x = GetScreenPosFromSampleIndex(sample_idx);
    double fractional_x = x - (int64_t)x;
    float const gamma_correction_factor = 0.75;
    RGBAColour c = col;
    c.a = col.a * powf(1.0 - fractional_x, gamma_correction_factor);
    VLine(bmp, floor(x), 0, bmp->height, c);
    c.a = col.a * powf(fractional_x, gamma_correction_factor);
    VLine(bmp, ceil(x), 0, bmp->height, c);
}


void SoundView::RenderWaveform(BitmapRGBA *bmp, double v_zoom_ratio)
{
    RGBAColour sound_colour = Colour(52, 152, 219);
    int channel_height = bmp->height / m_sound->m_num_channels;

    for (int chan_idx = 0; chan_idx < m_sound->m_num_channels; chan_idx++)
    {
        SoundChannel *chan = m_sound->m_channels[chan_idx];

        chan->CalcDisplayData(m_h_offset, m_display_mins, m_display_maxes, m_display_width, m_h_zoom_ratio);

        int y_mid = channel_height * chan_idx + channel_height / 2;

        int prev_y = y_mid;
        for (unsigned x = 0; x < m_display_width; x++)
        {
            int vline_len = RoundToInt(m_display_maxes[x] - m_display_mins[x]) * v_zoom_ratio;
            int y = ceil(y_mid - m_display_maxes[x] * v_zoom_ratio);
            if (vline_len == 0)
                PutPix(bmp, x, y, sound_colour);
            else
                VLine(bmp, x, y, vline_len, sound_colour);
        }

        HLine(bmp, 0, y_mid - 32767 * v_zoom_ratio, m_display_width, Colour(255, 255, 255, 60));
        HLine(bmp, 0, y_mid, m_display_width, Colour(255, 255, 255, 60));
        HLine(bmp, 0, y_mid + 32767 * v_zoom_ratio, m_display_width, Colour(255, 255, 255, 60));
    }
}


void SoundView::RenderSelection(BitmapRGBA *bmp)
{
    RenderMarker(bmp, m_selection_start, Colour(255, 255, 50, 90));
}


// ***************************************************************************
// Public Methods
// ***************************************************************************

SoundView::SoundView(Sound *sound)
{
    m_sound = sound;

    m_h_offset = 0.0;
    m_target_h_offset = 0.0;
    m_h_zoom_ratio = m_target_h_zoom_ratio = -1.0;

    m_playback_pos = -1.0;

    m_display_width = -1;
    m_display_mins = NULL;
    m_display_maxes = NULL;

    m_selection_start = -1.0;
    m_selection_end = -1.0;
}


static bool NearlyEqual(double a, double b)
{
    double diff = fabs(a - b);
    return diff < 1e-5;
}


void SoundView::Advance()
{
    if (m_h_zoom_ratio < 0.0)
        return;

    AdvanceSelection();

    double h_zoom_ratio_before = m_h_zoom_ratio;
    double max_h_offset = m_sound->GetLength() - m_display_width * m_h_zoom_ratio;
    max_h_offset = IntMax(0.0, max_h_offset);

    static double last_time = GetHighResTime();
    double now = GetHighResTime();
    double advance_time = now - last_time;
    last_time = now;


    //
    // Take mouse input

    double const ZOOM_INCREMENT = 1.2;
    if (g_inputManager.mouseVelZ < 0)
        m_target_h_zoom_ratio *= ZOOM_INCREMENT;
    else if (g_inputManager.mouseVelZ > 0)
        m_target_h_zoom_ratio /= ZOOM_INCREMENT;

    if (g_inputManager.mmb)
    {
        m_h_offset -= g_inputManager.mouseVelX * m_h_zoom_ratio;
        m_target_h_offset = m_h_offset - g_inputManager.mouseVelX * m_h_zoom_ratio;
    }
    else if (g_inputManager.mmbUnClicked)
    {
        m_target_h_offset -= g_inputManager.mouseVelX * m_h_zoom_ratio * 50.0;
    }


    //
    // Take keyboard input

    const double KEY_ZOOM_INCREMENT = 1.0 + 1.3 * advance_time;
    if (g_inputManager.keys[KEY_UP])
        m_target_h_zoom_ratio /= KEY_ZOOM_INCREMENT;
    if (g_inputManager.keys[KEY_DOWN])
        m_target_h_zoom_ratio *= KEY_ZOOM_INCREMENT;
    if (g_inputManager.keyDowns[KEY_PGUP])
        m_target_h_zoom_ratio /= KEY_ZOOM_INCREMENT * 8.0;
    if (g_inputManager.keyDowns[KEY_PGDN])
        m_target_h_zoom_ratio *= KEY_ZOOM_INCREMENT * 8.0;

    double const KEY_H_SCROLL_IMPLUSE = 1000.0 * m_h_zoom_ratio * advance_time;
    if (g_inputManager.keys[KEY_LEFT])
        m_target_h_offset -= KEY_H_SCROLL_IMPLUSE;
    if (g_inputManager.keys[KEY_RIGHT])
        m_target_h_offset += KEY_H_SCROLL_IMPLUSE;

    if (g_inputManager.keyDowns[KEY_HOME])
    {
        m_target_h_offset = 0;
    }
    else if (g_inputManager.keyDowns[KEY_END])
    {
        double samples_on_screen = m_display_width * m_h_zoom_ratio;
        m_target_h_offset = m_sound->GetLength() - samples_on_screen;
    }


    //
    // Enforce constraints

    double max_h_zoom_ratio = m_sound->GetLength() / (double)m_display_width;
    if (m_target_h_zoom_ratio > max_h_zoom_ratio)
        m_target_h_zoom_ratio = max_h_zoom_ratio;

    double min_h_zoom_ratio = 1.0;
    if (m_target_h_zoom_ratio < min_h_zoom_ratio)
        m_target_h_zoom_ratio = 1.0;


    //
    // Do physics

    // Zoom
    double zoom_blend_factor = advance_time * 8.0;
    m_h_zoom_ratio = (1.0 - zoom_blend_factor) * m_h_zoom_ratio + zoom_blend_factor * m_target_h_zoom_ratio;

    // H offset
    {
        double h_offset_delta = m_target_h_offset - m_h_offset;
        double factor1 = 3.0 * advance_time;
        double factor2 = 1.0 - factor1;
        m_h_offset = factor2 * m_h_offset + factor1 * m_target_h_offset;
    }


    //
    // Correct h_offset to compensate for zoom change this frame. 

    {
        // The aim is to keep some part of the visible waveform in the same
        // place on the display. If there is a selection start, we use that,
        // otherwise we just use the sample that is currently in the middle
        // of the display.
        double num_samples_visible = m_display_width * h_zoom_ratio_before;
        double keep_thing = (m_selection_start - m_h_offset) / num_samples_visible;
        if (keep_thing < 0.0 || keep_thing > 1.0)
            keep_thing = 0.5;

        double delta_h_zoom = h_zoom_ratio_before / m_h_zoom_ratio;

        if (delta_h_zoom < 1.0)
        {
            double tmp = (delta_h_zoom - 1.0) * num_samples_visible;
            tmp *= keep_thing / delta_h_zoom;
            m_target_h_offset += tmp;
            m_h_offset += tmp;
        }
        else
        {
            double tmp = -((1.0 - delta_h_zoom) * num_samples_visible);
            tmp *= keep_thing / delta_h_zoom;
            m_target_h_offset += tmp;
            m_h_offset += tmp;
        }
    }


    //
    // Enforce constraints

    m_h_offset = ClampDouble(m_h_offset, 0.0, max_h_offset);
    m_target_h_offset = ClampDouble(m_target_h_offset, 0.0, max_h_offset);

    if (!NearlyEqual(m_h_zoom_ratio, m_target_h_zoom_ratio) ||
        !NearlyEqual(m_h_offset, m_target_h_offset))
    {
        g_can_sleep = false;
    }

    AdvancePlaybackPos();
}


void SoundView::Render(BitmapRGBA *bmp)
{
    if (m_h_zoom_ratio < 0.0)
    {
        m_h_zoom_ratio = (double)m_sound->GetLength() / (double)bmp->width;
        m_target_h_zoom_ratio = m_h_zoom_ratio;
    }

    UpdateDisplaySize(bmp->width);

    double v_zoom_ratio = (double)bmp->height / (65536 * m_sound->m_num_channels);

    RenderWaveform(bmp, v_zoom_ratio);
    RenderSelection(bmp);

    if (m_sound->m_playback_idx)
        RenderMarker(bmp, m_playback_pos, Colour(255, 255, 255, 90));
    
    DrawTextRight(g_defaultTextRenderer, g_colourWhite, bmp, bmp->width - 5, bmp->height - 20, "Zoom: %.1f", m_h_zoom_ratio);
}


int64_t SoundView::GetSampleIndexFromScreenPos(int screen_x)
{
    return m_h_offset + (double)screen_x * m_h_zoom_ratio + 0.5;
}


double SoundView::GetScreenPosFromSampleIndex(int64_t sample_idx)
{
    return ((double)sample_idx - m_h_offset) / m_h_zoom_ratio;
}