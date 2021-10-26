#include "helpers.cl"

/*
 * TODO
 *
 * Déclarez les structures partagées avec l'autre code si nécessaire selon votre énoncé.
 * Utilisez l'attribut `__attribute__((packed))` à vos déclarations.
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846264338328
#endif

/*
 * TODO
 *
 * Modifiez les paramètres du noyau avec ceux demandés par votre énoncé.
 */

 /*typedef struct sinoscope sinoscope_t;

 struct sinoscope{
    const char* name;
    sinoscope_handler handler;

    unsigned int buffer_size;
    unsigned char* buffer;

    unsigned int width;
    unsigned int height;
    unsigned int taylor;

    unsigned int interval;
    

 }

typedef struct sinoscope_float {
    float interval_inverse;
    float time;
    float max;
    float phase0;
    float phase1;
    float dx;
    float dy;
} sino_float;*/

typedef struct sinoscope sinoscope_t;

struct sinoscope {
	unsigned char *buf;
	int width;
	int height;
	int interval;
	int taylor;
	float interval_inv;
	float time;
	float max;
	float phase0;
	float phase1;
	float dx;
	float dy;
};



__kernel void sinoscope_kernel(__global unsigned char *buffer,
							   int width,
							   int taylor,
							   int interval,
							   float interval_inverse,
							   float time,
							   float phase0,
							   float phase1,
							   float dx,
							   float dy)
{
    /*
     * TODO
     *
     * En vous basant sur l'implémentation dans `sinoscope-serial.c`, effectuez le même calcul. Ce
     * noyau est appelé pour chaque coordonnée, alors vous ne devez pas spécifiez les boucles
     * extérieures.
     */
	/*if (sinoscope == NULL) {
        LOG_ERROR_NULL_PTR();
        goto fail_exit;
    }*/

    int x = get_global_id(0), y = get_global_id(1), index, t;	
    float px    = dx * x - 2 * M_PI;
    float py    = dy * y - 2 * M_PI;
    float value = 0;

    for (int k = 1; k <= taylor; k += 2) {
        value += sin(px * k * phase1 + time) / k;
        value += cos(py * k * phase0) / k;
    }

    value = (atan(value) - atan(-value)) / M_PI;
    value = (value + 1) * 100;

    pixel_t pixel;
    color_value(&pixel, value, interval, interval_inverse);

    index = (y*3) + (x*3) * width;

    buffer[index + 0] = pixel.bytes[0];
    buffer[index + 1] = pixel.bytes[1];
    buffer[index + 2] = pixel.bytes[2];

}

