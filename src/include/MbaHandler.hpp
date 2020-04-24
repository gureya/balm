#include <sys/types.h>
#include <ctype.h>
/*
 * translates definition of single
 * allocation class of service
 * from args into internal configuration.
 */
void set_mba_parameters(const unsigned cos_value, const uint64_t mba_value);

/*
 *
 * Sets up allocation classes of service on selected MBA ids
 *
 */
int set_mba_allocation(const unsigned socket_id);

/*
 * Initialize mba
 */
void initialize_mba();

/*
 * Reset mba
 */
void reset_mba();