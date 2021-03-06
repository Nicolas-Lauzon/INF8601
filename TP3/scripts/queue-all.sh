#!/bin/bash
#
# DO NOT EDIT THIS FILE
#

set -e
DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)

sbatch "${DIR}/heatsim-01.sh"
sbatch "${DIR}/heatsim-02.sh"
sbatch "${DIR}/heatsim-04.sh"
sbatch "${DIR}/heatsim-08.sh"
sbatch "${DIR}/heatsim-12.sh"
sbatch "${DIR}/heatsim-12-cart.sh"
