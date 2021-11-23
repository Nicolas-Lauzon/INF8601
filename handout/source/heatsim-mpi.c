#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include "heatsim.h"
#include "log.h"

int heatsim_init(heatsim_t* heatsim, unsigned int dim_x, unsigned int dim_y) {
    /*
     * TODO: Initialiser tous les membres de la structure `heatsim`. 
     *       Le communicateur doit être périodique. Le communicateur
     *       cartésien est périodique en X et Y.
     */

    int dimensions[2] = {dim_x, dim_y};
    int periods[2] = { true, true };

    MPI_Comm_size(MPI_COMM_WORLD, &heatsim->rank_count);
    MPI_Comm_rank(MPI_COMM_WORLD, &heatsim->rank);

    int ret;

    ret = MPI_Cart_create(MPI_COMM_WORLD, 2, dimensions, periods, false, &heatsim->communicator);
    if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Error Cart Create : ", ret);

        goto fail_exit;
    }

    ret = MPI_Cart_shift(heatsim->communicator, 1, 1, &heatsim->rank, &heatsim->rank_north_peer);
     if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Shift 1 : ", ret);

        goto fail_exit;
    }

    ret = MPI_Cart_shift(heatsim->communicator, 1, -1, &heatsim->rank, &heatsim->rank_south_peer);
     if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Shift 2 : ", ret);

        goto fail_exit;
    }

    ret = MPI_Cart_shift(heatsim->communicator, 0, 1, &heatsim->rank, &heatsim->rank_east_peer);
     if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Shift 3 : ", ret);

        goto fail_exit;
    }

    ret = MPI_Cart_shift(heatsim->communicator, 0, -1, &heatsim->rank, &heatsim->rank_west_peer);
     if(ret != MPI_SUCCESS) {
        LOG_ERROR_MPI("Shift 4 : ", ret);

        goto fail_exit;
    }

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
} e_t;

typedef struct data {
    double* data;
} data_t;

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


    MPI_Datatype ElementType;

    MPI_Datatype ElementFieldTypes[3] =
    { 
        MPI_UNSIGNED,
        MPI_UNSIGNED,
        MPI_UNSIGNED
    };

    MPI_Aint ElementFieldPosition[3] =
    { 
        offsetof(e_t, width), 
        offsetof(e_t, height), 
        offsetof(e_t, padding)
    };

    int ElementFieldLength[3] = {1, 1, 1};
    MPI_Type_create_struct(3, ElementFieldLength, ElementFieldPosition, ElementFieldTypes, &ElementType);
    MPI_Type_commit(&ElementType);

    grid_t* grid = cart2d_get_grid(cart, heatsim->coordinates[0], heatsim->coordinates[1]);

    struct dimensions dim_to_send = {
        grid->width,
        grid->height,
        grid->padding
    };

    MPI_Request request_size;

    int ret = MPI_Isend(&dim_to_send, 1, ElementType, heatsim->rank, 3, heatsim->communicator, &request_size);

    if(ret != MPI_SUCCESS) {
        goto fail_exit;
    }

    MPI_Wait(&request_size, MPI_STATUS_IGNORE);

    /////////////////////////////////////////////////

    MPI_Request request_data;

    ret = MPI_Isend(&grid->data, 1, MPI_DOUBLE, heatsim->rank, 3, heatsim->communicator, &request_data);
    
    if(ret != MPI_SUCCESS) {
        goto fail_exit;
    }

    MPI_Wait(&request_data, MPI_STATUS_IGNORE);

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
    MPI_Datatype ElementType;

    e_t buf;

    MPI_Request request;

    int ret = MPI_Irecv(&buf, 1, ElementType, 0, 3, heatsim->communicator, &request);

    if(ret != MPI_SUCCESS) {
        goto fail_exit;
    
    }


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

fail_exit:
    return -1;
}

int heatsim_send_result(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 0);

    /*
     * TODO: Envoyer les données (`data`) du `grid` résultant au rang 0. Le
     *       `grid` n'a aucun rembourage (padding = 0);
     */

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

fail_exit:
    return -1;
}
