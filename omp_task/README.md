# Principle

## Firstly
Execute an OpenMP program creating every tasks but executing them in-order and sequentially on a single-thread.
Each task as a unique identifier constant against executions.
Build a task dependency graph (TDG1) upon load/store from such a valid sequential execution.

## Secondly
Execute the same OpenMP program creating every tasks, and executing them out-of-order on a single-thread.
Build a task dependency graph (TDG2) upon load/store from such a valid sequential execution.

## Finally
Compare TDG1 and TDG2
If they differ, notify programmer on TDG2 issues as TDG1 is correct
