#include <stdio.h>
#include <fdtd.h>

double get_time (){
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}

int main (void){
    simulation sim;
    sim.height = 1024;
    sim.width = 1024;
    sim.use_gpu = true;
    sim.time_step = 1e-12;
    sim.grid_cell_size = 1e-3;
    init_simulation(&sim);

    material optical_fiber { .epsilon = 3.6e-11, .sigma = 0 };
    material vacuum {};

    const int wwidth = 30;
    draw_rect (&sim, optical_fiber, 0, sim.height / 2 - wwidth / 2, sim.width, wwidth);

    const int radius = 107;
    const int dist = -30;
    draw_circle (&sim, optical_fiber, sim.width / 4, sim.height / 2 - wwidth / 2 - wwidth - dist - radius, radius);
    draw_circle (&sim, vacuum, sim.width / 4, sim.height / 2 - wwidth / 2 - wwidth - dist - radius, radius - wwidth);

    init_materials(&sim);

    puts ("init done");

    double fps;
    double t = get_time();
    while(1){
        for (int i = 3; i < wwidth - 3; i++){
            sim.field->H[o(&sim, 0, 1, sim.height / 2 - wwidth / 2 + i)] = sin(sim.it / 30.0f) * 10.0f;
        }

        step(&sim);
        

        if (sim.it % 3 == 0){
            plot(&sim);
        }

        fps -= (fps - (1000.0 / (get_time() - t))) * 0.01;
        printf("%ffps\n", fps);
        t = get_time();

        if (poll_quit(sim.p))
            break;
    }
    return 0;
}