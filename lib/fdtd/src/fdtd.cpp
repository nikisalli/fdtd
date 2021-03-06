#include <fdtd.h>

uint64_t o(simulation* s, uint32_t v, uint32_t i, uint32_t j){
    return v * s->width * s->height + i * s->height + j;
}

void init_field(simulation* s){
    s->field = (EM_field*) malloc(sizeof(EM_field));
    s->field->H = (double*) malloc(s->width * s->height * sizeof(double));
    s->field->E = (double*) malloc(s->width * s->height * 2 * sizeof(double));
    s->field->mat = (material*) malloc(s->width * s->height * sizeof(material));
    s->field->K = (double*) malloc(s->width * s->height * 4 * sizeof(double));
    s->field->out = (uint32_t*) malloc(s->width * s->height * sizeof(uint32_t));
    s->field->color_mask = (color*) malloc(s->width * s->height * sizeof(color));
    material vacuum;
    for(int i = 0; i < s->width; i++){
        for(int j = 0; j < s->height; j++){
            s->field->H[o(s,0,i,j)] = 0.0;
            s->field->E[o(s,0,i,j)] = 0.0;
            s->field->E[o(s,1,i,j)] = 0.0;
            s->field->mat[o(s,0,i,j)] = vacuum;
            s->field->color_mask[o(s,0,i,j)] = (color){0., 0., 0., 1.};
        }
    }
}

void destroy_field(simulation* s){
    free(s->field->H);
    free(s->field->E);
    free(s->field->mat);
    free(s->field->K);
    free(s->field->out);
    free(s->field);
}

void init_materials (simulation* s){
    for (int i = 0; i < s->width; i++){
        for (int j = 0; j < s->height; j++){
            double temp;
            temp = (s->field->mat[o(s,0,i,j)].sigma * s->time_step) / (s->field->mat[o(s,0,i,j)].epsilon * 2);
            s->field->K[o(s,0,i,j)] = (1 - temp) / (1 + temp);  // Ca
            s->field->K[o(s,1,i,j)] = (s->time_step / (s->field->mat[o(s,0,i,j)].epsilon * s->grid_cell_size)) / (1 + temp);  // Cb 
            temp = (s->field->mat[o(s,0,i,j)].sigma * s->time_step) / (s->field->mat[o(s,0,i,j)].mu * 2);
            s->field->K[o(s,2,i,j)] = (1 - temp) / (1 + temp);  // Da
            s->field->K[o(s,3,i,j)] = (s->time_step / (s->field->mat[o(s,0,i,j)].mu * s->grid_cell_size)) / (1 + temp);  // Db
        }
    }

    // copy computed materials to gpu
    if (s->use_gpu){
        vuda::memcpy(s->dev_H, s->field->H, s->width * s->height * sizeof(double), cudaMemcpyHostToDevice);  // H is a 1-dimensional vector
        vuda::memcpy(s->dev_E, s->field->E, s->width * s->height * 2 * sizeof(double), cudaMemcpyHostToDevice);  // E is a 2-dimensional vector
        vuda::memcpy(s->dev_K, s->field->K, s->width * s->height * 4 * sizeof(double), cudaMemcpyHostToDevice);  // K is a 4-dimensional vector
        vuda::memcpy(s->dev_color_mask, s->field->color_mask, s->width * s->height * sizeof(color), cudaMemcpyHostToDevice);
    }
}

void E_step (simulation* s){
    if (s->use_gpu){
        vuda::dim3 grid(s->width / 16, s->height / 16);
        vuda::launchKernel("E.spv", "main", 0, grid, s->width, s->height, s->dev_H, s->dev_E, s->dev_K);
        // cudaMemcpy(E, dev_E, s->width * s->height * 2 * sizeof(double), cudaMemcpyDeviceToHost);  // E is a 2-dimensional vector
    } else {
        int i, j;
        #pragma omp parallel for schedule(static,1) num_threads(12) private(j)
        for (i = 1; i < s->width; i++) {
            for (j = 1; j < s->height; j++) { 
                s->field->E[o(s,0,i,j)] = s->field->K[o(s,0,i,j)] * s->field->E[o(s,0,i,j)] + s->field->K[o(s,1,i,j)] * (s->field->H[o(s,0,i,j)] - s->field->H[o(s,0,i,j - 1)]);
                s->field->E[o(s,1,i,j)] = s->field->K[o(s,0,i,j)] * s->field->E[o(s,1,i,j)] + s->field->K[o(s,1,i,j)] * (s->field->H[o(s,0,i - 1,j)] - s->field->H[o(s,0,i,j)]);
            }
        }
    }
}

void H_step (simulation* s){
    if (s->use_gpu){
        vuda::dim3 grid(s->width / 16, s->height / 16);
        vuda::launchKernel("H.spv", "main", 0, grid, s->width, s->height, s->dev_H, s->dev_E, s->dev_K, s->dev_out, s->dev_color_mask);
        vuda::memcpy(s->field->out, s->dev_out, s->width * s->height * sizeof(uint32_t), cudaMemcpyDeviceToHost);
    } else {
        int i, j;
        #pragma omp parallel for schedule(static,1) num_threads(12) private(j)
        for (i = 1; i < s->width - 1; i++) {
            for (j = 1; j < s->height - 1; j++) { 
                s->field->H[o(s,0,i,j)] = s->field->K[o(s,2,i,j)] * s->field->H[o(s,0,i,j)] + s->field->K[o(s,3,i,j)] * (s->field->E[o(s,0,i,j + 1)] - s->field->E[o(s,0,i,j)] + s->field->E[o(s,1,i,j)] - s->field->E[o(s,1,i + 1,j)]);
            }
        }
    }
}

void draw_circle (simulation* s, material mymat, int center_x, int center_y, int radius, bool show_texture_on_plot, color plot_color){
    for (int i = 0; i < s->width; i++){
        for (int j = 0; j < s->height; j++){
            if (sqrt(powf64(i - center_x, 2) + powf64(j - center_y, 2)) < radius){
                s->field->mat[o(s,0,i,j)] = mymat;
                if(show_texture_on_plot) {
                    s->field->color_mask[o(s,0,i,j)] = plot_color;
                }
            }
        }
    }
}

void draw_rect (simulation* s, material mymat, int start_x, int start_y, int width, int height, bool show_texture_on_plot, color plot_color){
    for (int i = 0; i < s->width; i++){
        for (int j = 0; j < s->height; j++){
            if (i > start_x && i < start_x + width && j > start_y && j < start_y + height){
                s->field->mat[o(s,0,i,j)] = mymat;
                if(show_texture_on_plot) {
                    s->field->color_mask[o(s,0,i,j)] = plot_color;
                }
            }
        }
    }
}

void draw_from_img (simulation* s, material mymat, const char path[300], bool show_texture_on_plot, color plot_color){
    cv::String mypath = cv::String(path);
    cv::Mat im = cv::imread(mypath);
    cv::Mat gim;
    cv::cvtColor(im, gim, cv::COLOR_BGR2GRAY);

    if (im.cols != s->width || im.rows != s->height){
        printf("Image size is not equal to field size!\n");
        return;
    }

    for (int i = 0; i < s->width; i++){
        for (int j = 0; j < s->height; j++){
            if (gim.at<uchar>(j, i) < 10){
                s->field->mat[o(s,0,i,j)] = mymat;
                if(show_texture_on_plot) {
                    s->field->color_mask[o(s,0,i,j)] = plot_color;
                }
            }
        }
    }
}

double get_time (){
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}

double get_time_us (){
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000000 + t.tv_usec;
}

void step (simulation* s){
    s->it++;
    update_sources(s);
    H_step(s);
    E_step(s);
}

void plot (simulation* s){
    draw_field(s->p, s->field, s->use_gpu);
}

void init_simulation (simulation* s){
    init_field(s);
    init_plotter(&(s->p), s->width, s->height);
    init_sdl(s->p);

    s->dev_H = nullptr;
    s->dev_E = nullptr;
    s->dev_K = nullptr;
    s->dev_out = nullptr;

    // 65535 on x and y means not active
    for (auto& src : s->sources){
        src.x = 65535;
        src.y = 65535;
        src.value = 0;
    }

    if(s->use_gpu){
        // enumerate devices
        int count;
        vuda::getDeviceCount (&count);
        printf ("dev count: %d\n", count);
        for (int i = 0; i < count; i++) {
            vuda::deviceProp props;
            vuda::getDeviceProperties(&props, i);
            printf("dev %d: %s\n", i, props.name);
        }
        // set gpu to use
        vuda::setDevice(0);

        // allocate buffers
        puts ("allocating");

        vuda::malloc((void**)&(s->dev_H), s->width * s->height * sizeof(double));
        vuda::malloc((void**)&(s->dev_E), s->width * s->height * 2 * sizeof(double));
        vuda::malloc((void**)&(s->dev_K), s->width * s->height * 4 * sizeof(double));
        vuda::malloc((void**)&(s->dev_out), s->width * s->height * sizeof(uint32_t));
        vuda::malloc((void**)&(s->dev_sources), 4096 * sizeof(source));
        vuda::malloc((void**)&(s->dev_color_mask), s->width * s->height * sizeof(color));
    } else {
        omp_set_num_threads(12);
    }
}

void update_sources (simulation* s){
    if (s->use_gpu){
        vuda::memcpy(s->dev_sources, s->sources, 4096 * sizeof(source), cudaMemcpyHostToDevice);
        vuda::launchKernel("S.spv", "main", 0, 16, s->width, s->height, s->dev_H, s->dev_sources);
    } else {
        #pragma omp parallel for
        for (auto src : s->sources){
            if (src.x < 65535 && src.y < 65535){
                s->field->H[o(s,0,src.y, src.x)] = src.value;
            }
        }
    }
}

void destroy_simulation (simulation* s){
    destroy_field(s);
    destroy_sdl(s->p);
    destroy_plotter(&(s->p));

    if(s->use_gpu){
        vuda::free(s->dev_H);
        vuda::free(s->dev_E);
        vuda::free(s->dev_K);
        vuda::free(s->dev_out);
        vuda::free(s->dev_sources);
        vuda::free(s->dev_color_mask);
    }
}
