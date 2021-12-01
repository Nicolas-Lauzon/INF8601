#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include "heatsim.h"
#include "log.h"

/*
    QUESTIONS
    changer grid data directement
    envoyer double* dans struct
*/


int heatsim_init(heatsim_t* heatsim, unsigned int dim_x, unsigned int dim_y) {
    /*
     * TODO: Initialiser tous les membres de la structure `heatsim`. 
     *       Le communicateur doit être périodique. Le communicateur
     *       cartésien est périodique en X et Y.
     */
    
    int dimensions[2] = {dim_x, dim_y};
    int periods[2] = { true, true };

    int ret = 0;

    ret = MPI_Cart_create(MPI_COMM_WORLD, 2, dimensions, periods, false, &heatsim->communicator);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error Cart Create : ", ret);

        goto fail_exit;
    }

    if (heatsim->communicator == MPI_COMM_NULL) {
        LOG_ERROR("NULL CONNECTOR: %d", heatsim->rank);
    }

    MPI_Comm_size(heatsim->communicator, &heatsim->rank_count);
    MPI_Comm_rank(heatsim->communicator, &heatsim->rank);     

    ret = MPI_Cart_shift(heatsim->communicator, 1, 1, &heatsim->rank_east_peer, &heatsim->rank_west_peer);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Shift 1 : ", ret);

        goto fail_exit;
    }

    ret = MPI_Cart_shift(heatsim->communicator, 0, 1, &heatsim->rank_north_peer, &heatsim->rank_south_peer);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Shift 2 : ", ret);

        goto fail_exit;
    }

        
    // rank, ewns width height

    ret = MPI_Cart_coords(heatsim->communicator, heatsim->rank, 2, heatsim->coordinates);
     if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error : ", ret);

        goto fail_exit;
    }

    return 0;
fail_exit:

    return -1;
}

typedef struct dimensions {
    unsigned int width;
    unsigned int height;
    unsigned int padding;
} dim_t;

typedef struct data {
    int num;
    double data[];
} data_t;


MPI_Datatype grid_size_struct() {
    MPI_Datatype type;

    MPI_Datatype field_types[3] =
    { 
        MPI_UNSIGNED,
        MPI_UNSIGNED,
        MPI_UNSIGNED
    };

    MPI_Aint field_positions[3] =
    { 
        offsetof(dim_t, width), 
        offsetof(dim_t, height), 
        offsetof(dim_t, padding)
    };

    int field_lengths[3] = {1, 1, 1};

    MPI_Type_create_struct(3, field_lengths, field_positions, field_types, &type);
    
    return type;
}


MPI_Datatype grid_data_struct(unsigned int size) {
    MPI_Datatype type;

    MPI_Datatype field_types[1] =
    { 
        MPI_DOUBLE
    };

    MPI_Aint field_positions[1] =
    { 
        offsetof(data_t, data)
    };

    int field_lengths[1] = { size };

    MPI_Type_create_struct(1, field_lengths, field_positions, field_types, &type);
    
    return type;
}

int heatsim_send_grids(heatsim_t* heatsim, cart2d_t* cart) {
    /*
     * TODO: Envoyer toutes les `grid` aux autres rangs. Cette fonction
     *       est appelé pour le rang 0. Par exemple, si le rang 3 est à la
     *       coordonnée cartésienne (0, 2), alors on y envoit le `grid`
     *       à la position (0, 2) dans `cart`.
     *
     *       Il est recommandé d'envoyer les paramètres `width`, `height`
     *       et `padding` avant les données. De cette manière, le receveur
     *       peut allouer la structure avec `grid_create` directement.
     *
     *       Utilisez `cart2d_get_grid` pour obtenir la `grid` à une coordonnée.
     */

    MPI_Datatype message_type = grid_size_struct();
    MPI_Type_commit(&message_type);

    MPI_Request request;

    int ret = 0;
    int myCoords[2];
    for (int i = 1; i < heatsim->rank_count - 1; i++) {
        
        ret = MPI_Cart_coords(heatsim->communicator, i, 2, myCoords);
        grid_t* grid = cart2d_get_grid(cart, myCoords[0], myCoords[1]);

        struct dimensions dim_to_send = {
            grid->width,
            grid->height,
            grid->padding
        };

        ret = MPI_Isend(&dim_to_send, 1, message_type, i + 1, 6, heatsim->communicator, &request);
        if(ret != MPI_SUCCESS) {
            LOG_ERROR_MPI("Error send struct : ", ret);
            goto fail_exit;
        }
        
        ret = MPI_Wait(&request, MPI_STATUS_IGNORE);
        if(ret != MPI_SUCCESS) {
            LOG_ERROR_MPI("Error waiting : ", ret);
            goto fail_exit;
        }

        MPI_Datatype data_type = grid_data_struct(grid->width * grid->height);
        MPI_Type_commit(&data_type);

        data_t data;
        data.num = 12345;
        //memcpy(data.data, grid->data, grid->width * grid->height * sizeof(double));
        //data.data = malloc(grid->width * grid->height * sizeof(double));
        //data.data = grid->data;
        
        // NOTE: Pas de & car grid->data est deja un pointeur

        ret = MPI_Isend(grid->data, grid->width * grid->height, MPI_DOUBLE, i + 1, 7, heatsim->communicator, &request);
        if(ret != MPI_SUCCESS) {
            LOG_ERROR_MPI("Error send data : ", ret);
            goto fail_exit;
        }
        
        ret = MPI_Wait(&request, MPI_STATUS_IGNORE);
        if(ret != MPI_SUCCESS) {
            LOG_ERROR_MPI("Error waiting : ", ret);
            goto fail_exit;
        }
    }
    //LOG_ERROR("Rank %d done sending data", heatsim->rank);

    return 0;

fail_exit:
    return -1;
}

grid_t* heatsim_receive_grid(heatsim_t* heatsim) {
    /*
     * TODO: Recevoir un `grid ` du rang 0. Il est important de noté que
     *       toutes les `grid` ne sont pas nécessairement de la même
     *       dimension (habituellement ±1 en largeur et hauteur). Utilisez 
     *       la fonction `grid_create` pour allouer un `grid`.
     *
     *       Utilisez `grid_create` pour allouer le `grid` à retourner.
     */

    MPI_Datatype message_type = grid_size_struct();
    MPI_Type_commit(&message_type);
    dim_t buf;
    int ret = 0;

    MPI_Request request;
    ret = MPI_Irecv(&buf, 1, message_type, 0, 6, heatsim->communicator, &request);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error Wait receive data : ", ret);
        goto fail_exit;
    }
    ret = MPI_Wait(&request, MPI_STATUS_IGNORE);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error Wait receive data : ", ret);
        goto fail_exit;
    }

    grid_t* newGrid = grid_create(buf.width, buf.height, buf.padding);
    
    MPI_Datatype data_type = grid_data_struct(newGrid->width * newGrid->height);
    MPI_Type_commit(&data_type);

    ret = MPI_Irecv(newGrid->data, 1, data_type, 0, 7, heatsim->communicator, &request);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error receive data : ", ret);
        goto fail_exit;
    }

    ret = MPI_Wait(&request, MPI_STATUS_IGNORE);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error Wait receive data : ", ret);
        goto fail_exit;
    }

    #warning Changer data directement ?? utiliser type struct et mettre direct dans newgrid data

    
    //LOG_ERROR("Received for rank: %d\n", heatsim->rank);

    return newGrid;


fail_exit:
    return NULL;
}

int heatsim_exchange_borders(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 1);

    /*
     * TODO: Échange les bordures de `grid`, excluant le rembourrage, dans le
     *       rembourrage du voisin de ce rang. Par exemple, soit la `grid`
     *       4x4 suivante,
     *
     *                            +-------------+
     *                            | x x x x x x |
     *                            | x A B C D x |
     *                            | x E F G H x |
     *                            | x I J K L x |
     *                            | x M N O P x |
     *                            | x x x x x x |
     *                            +-------------+
     *
     *       où `x` est le rembourrage (padding = 1). Ce rang devrait envoyer
     *
     *        - la bordure [A B C D] au rang nord,
     *        - la bordure [M N O P] au rang sud,
     *        - la bordure [A E I M] au rang ouest et
     *        - la bordure [D H L P] au rang est.
     *
     *       Ce rang devrait aussi recevoir dans son rembourrage
     *
     *        - la bordure [A B C D] du rang sud,
     *        - la bordure [M N O P] du rang nord,
     *        - la bordure [A E I M] du rang est et
     *        - la bordure [D H L P] du rang ouest.
     *
     *       Après l'échange, le `grid` devrait avoir ces données dans son
     *       rembourrage provenant des voisins:
     *
     *                            +-------------+
     *                            | x m n o p x |
     *                            | d A B C D a |
     *                            | h E F G H e |
     *                            | l I J K L i |
     *                            | p M N O P m |
     *                            | x a b c d x |
     *                            +-------------+
     *
     *       Utilisez `grid_get_cell` pour obtenir un pointeur vers une cellule.
     */
    MPI_Request request[8];
    MPI_Status status[8];
    int ret = 0;
    int width = grid->width;
    int height = grid->height;

    MPI_Datatype type;
    MPI_Type_vector(height, 1, grid->width_padded, MPI_DOUBLE, &type);
    MPI_Type_commit(&type);

    // RECEIVE WEST BORDER
    ret = MPI_Irecv(grid_get_cell(grid, -1, 0), 1, type, heatsim->rank_west_peer, 0, heatsim->communicator, &request[0]);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error receive west : ", ret);
        goto fail_exit;
    }

    // RECEIVE SOUTH BORDER
    ret = MPI_Irecv(grid_get_cell(grid, 0, height), width, MPI_DOUBLE, heatsim->rank_south_peer, 1, heatsim->communicator, &request[1]);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error receive south : ", ret);
        goto fail_exit;
    }

    // RECEIVE NORTH BORDER
    ret = MPI_Irecv(grid_get_cell(grid, 0, -1), width, MPI_DOUBLE, heatsim->rank_north_peer, 2, heatsim->communicator, &request[2]);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error receive north : ", ret);
        goto fail_exit;
    }

    // RECEIVE EAST BORDER
    ret = MPI_Irecv(grid_get_cell(grid, width, 0), 1, type, heatsim->rank_east_peer, 3, heatsim->communicator, &request[3]);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error receive east : ", ret);
        goto fail_exit;
    }

    // SEND EAST BORDER
    ret = MPI_Isend(grid_get_cell(grid, width, 0), 1, type, heatsim->rank_east_peer, 0, heatsim->communicator, &request[4]);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error send east border : ", ret);
        goto fail_exit;
    }

    // SEND NORTH BORDER
    ret = MPI_Isend(grid_get_cell(grid, 0, 0), width, MPI_DOUBLE, heatsim->rank_north_peer, 1, heatsim->communicator, &request[5]);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error send north border : ", ret);
        goto fail_exit;
    }

    // SEND SOUTH BORDER
    ret = MPI_Isend(grid_get_cell(grid, 0, height), width, MPI_DOUBLE, heatsim->rank_south_peer, 2, heatsim->communicator, &request[6]);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error send south border : ", ret);
        goto fail_exit;
    }

    //SEND WEST BORDER   
    ret = MPI_Isend(grid_get_cell(grid, 0, 0), 1, type, heatsim->rank_west_peer, 3, heatsim->communicator, &request[7]);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error send west border : ", ret);
        goto fail_exit;
    }

    ret = MPI_Waitall(8, request, MPI_STATUS_IGNORE);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error wait border : ", ret);
        LOG_ERROR("My rank  :  %d\n", heatsim->rank);
        LOG_ERROR("My North  :  %d\n", heatsim->rank_north_peer);
        LOG_ERROR("My South  :  %d\n", heatsim->rank_south_peer);
        LOG_ERROR("My East  :  %d\n", heatsim->rank_east_peer);
        LOG_ERROR("My West  :  %d\n", heatsim->rank_west_peer);
        printf("MPI process %d from rank %d, with tag %d and error code %d.\n", 
            heatsim->rank,
            status->MPI_SOURCE,
            status->MPI_TAG,
            status->MPI_ERROR);
        goto fail_exit;
    }

    return 0;

fail_exit:
    return -1;
}

int heatsim_send_result(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 0);

    /*
     * TODO: Envoyer les données (`data`) du `grid` résultant au rang 0. Le
     *       `grid` n'a aucun rembourage (padding = 0);
     */
    // int ret = 0;
    // MPI_Request requests;
    // MPI_Status status;

    // LOG_ERROR("Sending result to rank 0 from rank %d", heatsim->rank);

    // ret = MPI_Isend(grid->data, grid->width * grid->height, MPI_DOUBLE, 0, 1, heatsim->communicator, &requests);
    // if(ret != MPI_SUCCESS) {
    //     LOG_ERROR_MPI("Error sending data to node 0 : ", ret);
    //     goto fail_exit;
    // }

    // // We could use a simple MPI_Wait
    // ret = MPI_Wait(&requests, MPI_STATUS_IGNORE);
    // if(ret != MPI_SUCCESS) {
    //     LOG_ERROR_MPI("Error wait results : ", ret);
    //     goto fail_exit;
    // }
    return 0;

fail_exit:
    return -1;
}

int heatsim_receive_results(heatsim_t* heatsim, cart2d_t* cart) {
    /*
     * TODO: Recevoir toutes les `grid` des autres rangs. Aucune `grid`
     *       n'a de rembourage (padding = 0).
     *
     *       Utilisez `cart2d_get_grid` pour obtenir la `grid` à une coordonnée
     *       qui va recevoir le contenue (`data`) d'un autre noeud.
     */
    // int ret = 0;
    // MPI_Request request;

    // for (unsigned int i = 0; i < heatsim->rank_count; i++) {
    //     LOG_ERROR("Rank 0 receiving results from %d", i + 1);

    //     grid_t* grid = cart2d_get_grid(cart, heatsim->coordinates[0], heatsim->coordinates[1]);

    //     double* data = malloc(grid->width * grid->height * sizeof(double));
    //     ret = MPI_Irecv(data, grid->width * grid->height, MPI_DOUBLE, i + 1, 1, heatsim->communicator, &request);
    //     if(ret != MPI_SUCCESS) {
    //         LOG_ERROR("Error receive data from %d", heatsim->rank);
    //         goto fail_exit;
    //     }
    //     ret = MPI_Wait(&request, MPI_STATUS_IGNORE);
    //     if(ret != MPI_SUCCESS) {
    //         LOG_ERROR_MPI("Error wait border : ", ret);
    //         LOG_ERROR("My rank  :  %d\n", heatsim->rank);
    //         LOG_ERROR("My North  :  %d\n", heatsim->rank_north_peer);
    //         LOG_ERROR("My South  :  %d\n", heatsim->rank_south_peer);
    //         LOG_ERROR("My East  :  %d\n", heatsim->rank_east_peer);
    //         LOG_ERROR("My West  :  %d\n", heatsim->rank_west_peer);
    //         goto fail_exit;
    //     }

    //     grid->data = data;
    // }

    return 0;

fail_exit:
    return -1;
}