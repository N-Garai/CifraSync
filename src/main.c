#include "cli/commands.h"
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        extern int main_interactive(void);
        return main_interactive();
    }
    return cs_run(argc, argv);
}
