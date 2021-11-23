#!/bin/bash
#
# DO NOT EDIT THIS FILE
#
#SBATCH --job-name=heatsim-04
#SBATCH --output=heatsim-04-%A.txt
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=8
#SBATCH --time=0

source /etc/profile.d/modules.sh

module purge
module load mpi/openmpi-x86_64

function run_heatsim() {
    local dim_x="$1"
    local dim_y="$2"
    local scripts_dir
    local heatsim_launch

    if [[ "${SLURM_JOBID}" == "" ]]; then
        echo 'SLURM_JOBID is not set'
        exit 1
    fi

    scripts_dir="$(dirname "$(scontrol show job "${SLURM_JOBID}" | awk -F= '/Command=/{print $2}')")"

    if readlink -e "${scripts_dir}/heatsim-launch.sh"; then
        heatsim_launch=$(readlink -e "${scripts_dir}/heatsim-launch.sh")
    else
        echo "heatsim-launch.sh not found"
        exit 1
    fi

    "${heatsim_launch}" --dim-x "${dim_x}" --dim-y "${dim_y}" --job-id "${SLURM_JOBID}" --node-count "${SLURM_JOB_NUM_NODES}"
}

run_heatsim 4 8