# Findings

- CASH is a command-line exact solver; its currently exposed interface has no verified runtime UB injection or pause/resume API.
- SPB exposes `LocalSearchSolver` for synchronous, single-threaded in-process use and returns a model only through the library API.
- SPB's current WCNF parser must be repaired and independently validated before public MSE instances are used.
