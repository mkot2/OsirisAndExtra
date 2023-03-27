#include "pcg.h"

pcg_extras::seed_seq_from<std::random_device> seedSource;
pcg32_c1024_fast PCG::generator = seedSource;