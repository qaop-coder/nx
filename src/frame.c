//------------------------------------------------------------------------------
// Platform-agnostic frame functions
//------------------------------------------------------------------------------

#include "frame.h"

f64 frame_fps(Frame* f)
{
    f->frame_count++;
    TimePoint current_time = time_now();
    
    // Initialize on first call
    if (f->frame_count == 1) {
        f->last_time = current_time;
        f->fps = 0.0;
        return f->fps;
    }
    
    // Calculate FPS based on elapsed time since last update
    TimePeriod elapsed = time_period(f->last_time, current_time);
    f64 elapsed_secs = time_secs(elapsed);
    
    if (elapsed_secs > 0.0) {
        // Calculate instantaneous FPS (frames since last call / elapsed time)
        f->fps = 1.0 / elapsed_secs;
    }
    
    f->last_time = current_time;
    return f->fps;
}