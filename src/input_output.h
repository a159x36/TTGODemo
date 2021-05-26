
#include "graphics3d.h"
typedef enum {
    NO_KEY,
    LEFT_DOWN,
    LEFT_UP,
    RIGHT_DOWN,
    RIGHT_UP
} key_type;

void input_output_init();
int demo_menu(char * title, int nentries, char *entries[], int select);
key_type get_input();
vec2 get_touchpads();
char *get_string(char *title);