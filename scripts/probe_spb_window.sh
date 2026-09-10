#!/bin/bash
# Does SPB get enough time, or does each round die in setup?
#
# Two measurements over the same instances:
#   hybrid    -- SPB called for W seconds inside the protocol, W = 3/10/30
#   standalone-- SPB alone with W seconds of *continuous* search (same cutoff)
# If the hybrid's per-round bound matches standalone's at the same cutoff, the
# window is simply too short and no per-round overhead is to blame. If the
# hybrid is much worse, each round is losing its budget to state rebuilding.
set -u
cd /home/fwkzj/HybridAlgorithm
B=build/HybridMaxSAT/hybridmaxsat
D=/data/dataset/Maxsat/Complete
OUT=runs/probe2
mkdir -p $OUT

INSTS="correlation-clustering_wt-Rounded_CorrelationClustering_Protein4_BINARY_N320
ParametricRBACMaintenance_mse20_wt-role_domino_multiple_0.6_8"

for inst in $INSTS; do
  for w in 3 10 30; do
    ( timeout 400 $B --config Hybrid --seed 20260909 --budget 300 \
        --cash-window 15 --spb-window $w --instance-id $inst \
        $D/MSE23W/$inst.wcnf $OUT/h_${inst}_w$w.jsonl \
        > $OUT/h_${inst}_w$w.out 2>&1 ) &
  done
  for t in 3 10 30 300; do
    ( timeout 400 $B --config SPB --seed 20260909 --budget $t \
        --instance-id $inst $D/MSE23W/$inst.wcnf $OUT/s_${inst}_t$t.jsonl \
        > $OUT/s_${inst}_t$t.out 2>&1 ) &
  done
done
wait
echo "ALL RUNS DONE"
