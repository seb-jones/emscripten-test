#define ONE_SECOND 1000.0

typedef struct Timing
{
    double frame_start_time;
    double previous_frame_start_time;
    double fps_timer;
    int fps;
} Timing;

double update_timing(Timing *timing, double time)
{
    timing->previous_frame_start_time = timing->frame_start_time;
    timing->frame_start_time = time;

    double dt = timing->frame_start_time - timing->previous_frame_start_time;

    timing->fps_timer += dt;
    while (timing->fps_timer >= ONE_SECOND) {
        printf("FPS: %d; DT: %f\n", timing->fps, dt);
        timing->fps_timer -= ONE_SECOND;
        timing->fps = 0;
    }

    return dt;
}
