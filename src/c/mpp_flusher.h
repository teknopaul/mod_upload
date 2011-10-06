#include <string.h>

/**
 * abstract the flush so unit tests can flush to stdout
 * return 0 for success
 */
int mpp_flush_data(char *buffer, int len, void *ctx);

