/* DO NOT EDIT THIS FILE */

#include <unistd.h>

#include "grid.h"
#include "heatsim.h"
#include "image.h"
#include "log.h"

int heatsim_diffuse(grid_t* current, grid_t* next) {
    if (current == NULL || next == NULL) {
        LOG_ERROR_NULL_PTR();
        goto fail_exit;
    }

    if (current->width_padded != next->width_padded || current->height_padded != next->height_padded) {
        LOG_ERROR("mismatch in dimensions");
        goto fail_exit;
    }

    for (int j = 0; j < current->height; j++) {
        for (int i = 0; i < current->width; i++) {
            double* result = grid_get_cell(next, i, j);
            double center  = *grid_get_cell(current, i, j);
            double top     = *grid_get_cell(current, i, j + 1);
            double right   = *grid_get_cell(current, i + 1, j);
            double bottom  = *grid_get_cell(current, i, j - 1);
            double left    = *grid_get_cell(current, i - 1, j);

            *result = center + 0.25 * (top + right + bottom + left - 4 * center);
        }
    }

    return 0;

fail_exit:
    return -1;
}

static inline void swap_grid_ptrs(grid_t** ptr1, grid_t** ptr2) {
    grid_t* ptr_tmp = *ptr1;
    *ptr1           = *ptr2;
    *ptr2           = ptr_tmp;
}

static cart2d_t* load_image(char* filename, unsigned int dim_x, unsigned int dim_y) {
    image_t* image = image_create_from_png(filename);
    if (image == NULL) {
        LOG_ERROR("failed to load image '%s'", filename);
        goto fail_exit;
    }

    grid_t* grid = image_to_grid(image, 0);
    if (grid == NULL) {
        LOG_ERROR("failed to create grid from image");
        goto fail_destroy_image;
    }

    if (grid_multiply(grid, 1000) < 0) {
        LOG_ERROR("failed to multiply grid");
        goto fail_destroy_image;
    }

    cart2d_t* cart = cart2d_from_grid(grid, dim_x, dim_y);
    if (cart == NULL) {
        LOG_ERROR("failed to create cartesian grid");
        goto fail_destroy_grid;
    }

    grid_destroy(grid);
    image_destroy(image);

    return cart;

fail_destroy_grid:
    grid_destroy(grid);
fail_destroy_image:
    image_destroy(image);
fail_exit:
    return NULL;
}

static int save_image(char* filename, cart2d_t* cart) {
    grid_t* grid = cart2d_to_grid(cart);
    if (grid == NULL) {
        LOG_ERROR("failed to convert cart to grid");
        goto fail_exit;
    }

    image_t* image = image_from_grid(grid);
    if (image == NULL) {
        LOG_ERROR("failed to convert grid to image");
        goto fail_destroy_cart;
    }

    if (image_save_png(image, filename) < 0) {
        LOG_ERROR("failed to save image");
        goto fail_destroy_image;
    }

    image_destroy(image);

    return 0;

fail_destroy_image:
    image_destroy(image);
fail_destroy_cart:
    cart2d_destroy(cart);
fail_exit:
    return -1;
}

int heatsim_run(char* input, char* output, unsigned int dim_x, unsigned int dim_y, unsigned int iterations) {
    heatsim_t heatsim;
    cart2d_t* cart = NULL;


////////////////////
    // int size;
    // MPI_Comm_size(MPI_COMM_WORLD, &size);
    // enum role_ranks { SENDER, RECEIVER };
    // int my_rank;
    // MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // if (my_rank == 0) {
    //     int buffer_sent[3] = { 111111, 222222, 333333 };
    //     MPI_Request request[3];
    //     for (unsigned int i = 0; i < 3; i++) {
    //         printf("MPI process %d sends value %d.\n", my_rank, buffer_sent[i]);
    //         MPI_Isend(&buffer_sent[i], 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD, &request[i]);

    //         // Do other things while the MPI_Isend completes
    //         // <...>

    //         // Let's wait for the MPI_Isend to complete before progressing further.
    //         MPI_Wait(&request[i], MPI_STATUS_IGNORE);
    //     }
        
    // } else {
        // int received;
        // MPI_Request request;
        // printf("[Process %d] I issue the MPI_Irecv to receive the message as a background task.\n", my_rank);
        // MPI_Irecv(&received, 1, MPI_INT, SENDER, 0, MPI_COMM_WORLD, &request);

        // // Do other things while the MPI_Irecv completes.
        // printf("[Process %d] The MPI_Irecv is issued, I now moved on to print this message.\n", my_rank);

        // // Wait for the MPI_Recv to complete.
        // MPI_Wait(&request, MPI_STATUS_IGNORE);
        // printf("[Process %d] The MPI_Irecv completed, therefore so does the underlying MPI_Recv. I received the value %d.\n", my_rank, received);
    // }
    

    // return 0;
/////////////////////////////////
    if (heatsim_init(&heatsim, dim_x, dim_y) < 0) {

        LOG_ERROR("simulation initialization failed");
        goto fail_exit;
    }

    printf("[%d] Heat simulation initialised, pid=%u\n", heatsim.rank, getpid());
    printf("[%d] (x,y)=(%u,%u), (S,W,N,E)=(%d,%d,%d,%d)\n", heatsim.rank, heatsim.coordinates[0],
           heatsim.coordinates[1], heatsim.rank_south_peer, heatsim.rank_west_peer, heatsim.rank_north_peer,
           heatsim.rank_east_peer);

    /* rank 0 loads and sends the image to the other ranks */

    grid_t* current_grid;

    if (heatsim.rank == 0) {
        cart = load_image(input, dim_x, dim_y);
        if (cart == NULL) {
            LOG_ERROR("simulation failed to load image (rank 0)");
            goto fail_exit;
        }

        grid_t* current_grid_no_padding = cart2d_get_grid(cart, heatsim.coordinates[0], heatsim.coordinates[1]);
        if (current_grid_no_padding == NULL) {
            LOG_ERROR("simulation failed to get grid at (%d, %d) (rank 0)", heatsim.coordinates[0],
                      heatsim.coordinates[1]);
            goto fail_destroy_cart;
        }

        current_grid = grid_clone_with_padding(current_grid_no_padding, 1);
        if (current_grid == NULL) {
            LOG_ERROR("simulation failed to clone starting grid");
            goto fail_destroy_cart;
        }

        printf("[%d] sending grids to other ranks\n", heatsim.rank);

        if (heatsim.rank_count > 1) {
            if (heatsim_send_grids(&heatsim, cart) < 0) {
                LOG_ERROR("simulation failed to send grids (rank 0)");
                goto fail_destroy_current_grid;
            }
        }

        printf("[%d] sent grids to other ranks\n", heatsim.rank);
    } else {
        printf("[%d] receiving grid from rank 0\n", heatsim.rank);

        grid_t* current_grid_no_padding = heatsim_receive_grid(&heatsim);
        if (current_grid_no_padding == NULL) {
            LOG_ERROR("simulation failed to receive image");
            goto fail_exit;
        }

        current_grid = grid_clone_with_padding(current_grid_no_padding, 1);
        grid_destroy(current_grid_no_padding);
        if (current_grid == NULL) {
            LOG_ERROR("simulation failed to clone starting grid");
            goto fail_exit;
        }

        printf("[%d] received grid from rank 0\n", heatsim.rank);
    }

    printf("[%d] grid config: padding=%u width=%d height=%d\n", heatsim.rank, current_grid->padding,
           current_grid->width, current_grid->height);

    /* create another grid and add padding to both of them */

    grid_t* next_grid = grid_clone(current_grid);
    if (next_grid == NULL) {
        LOG_ERROR("simulation failed to clone starting grid");
        goto fail_destroy_current_grid;
    }

    grid_t* heat_grid = grid_clone(current_grid);
    if (heat_grid == NULL) {
        LOG_ERROR("simulation failed to clone starting grid");
        goto fail_destroy_next_grid;
    }

    /* run the simulation for all rank  */

    printf("[%d] starting simulation\n", heatsim.rank);

    for (int i = 0; i < iterations; i++) {
        if (heatsim_exchange_borders(&heatsim, current_grid) < 0) {
            LOG_ERROR("simulation failed to exchange border");
            goto fail_destroy_heat_grid;
        }

        if (grid_set_min(current_grid, heat_grid) < 0) {
            LOG_ERROR("simulation failed to set grid minimum");
            goto fail_destroy_heat_grid;
        }

        if (heatsim_diffuse(current_grid, next_grid) < 0) {
            LOG_ERROR("simulation failed diffuse grid");
            goto fail_destroy_heat_grid;
        }

        swap_grid_ptrs(&current_grid, &next_grid);

        printf("[%d] iteration %d of %d done\n", heatsim.rank, i + 1, iterations);
    }

    int status = MPI_Barrier(MPI_COMM_WORLD);
    if (status != MPI_SUCCESS) {
        LOG_ERROR_MPI("MPI_Barrier", status);
        goto fail_destroy_heat_grid;
    }

    /* rank 0 gathers results from all other rank */

    if (heatsim.rank == 0) {
        printf("[%d] receiving results from other ranks\n", heatsim.rank);

        if (heatsim.rank_count > 1) {
            if (heatsim_receive_results(&heatsim, cart)) {
                LOG_ERROR("simulation failed to receive results (rank 0)");
                goto fail_destroy_heat_grid;
            }
        }

        printf("[%d] received results from other ranks\n", heatsim.rank);

        grid_t* result_grid = cart2d_get_grid(cart, heatsim.coordinates[0], heatsim.coordinates[1]);
        if (result_grid == NULL) {
            LOG_ERROR("simulation failed to get grid at (%d, %d) (rank 0)", heatsim.coordinates[0],
                      heatsim.coordinates[1]);
            goto fail_destroy_heat_grid;
        }

        if (grid_copy_data(current_grid, result_grid) < 0) {
            LOG_ERROR("simulation failed to copy grid (rank 0)");
            goto fail_destroy_heat_grid;
        }

        if (save_image(output, cart) < 0) {
            LOG_ERROR("failed to save image (rank 0)");
            goto fail_destroy_heat_grid;
        }
    } else {
        grid_t* grid_no_padding = grid_clone_with_padding(current_grid, 0);
        if (grid_no_padding == NULL) {
            LOG_ERROR("failed to copy grid without padding");
            goto fail_destroy_heat_grid;
        }

        printf("[%d] sending results to rank 0\n", heatsim.rank);

        status = heatsim_send_result(&heatsim, grid_no_padding);
        grid_destroy(grid_no_padding);
        if (status < 0) {
            LOG_ERROR("simulation failed to send result");
            goto fail_destroy_heat_grid;
        }

        printf("[%d] sent results to rank 0\n", heatsim.rank);
    }

    grid_destroy(heat_grid);
    grid_destroy(next_grid);
    grid_destroy(current_grid);

    if (cart != NULL) {
        cart2d_destroy(cart);
    }

    return 0;

fail_destroy_heat_grid:
    grid_destroy(heat_grid);
fail_destroy_next_grid:
    grid_destroy(next_grid);
fail_destroy_current_grid:
    grid_destroy(current_grid);
fail_destroy_cart:
    if (cart != NULL) {
        cart2d_destroy(cart);
    }
fail_exit:
    return -1;
}
