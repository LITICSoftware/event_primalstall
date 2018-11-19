# Primal Stalling for MIP Solvers (here: SCIP)

MIP solvers try to find an optimal solution for the given problem, using a
branch-and-bound search. During this process, they can improve the /primal
bound/ by finding improving feasible solutions. By solving more node
relaxations, they can improve the /dual bound/. When the two bounds meet, the
solver has proven optimality of the current incumbent solution.

In practice, the user is mostly interested in finding a very good solution
quickly. But even after finding an optimal solution, the solver typically spends
a lot of time proving its optimality.

Most solvers can be configured to stop the search early, when the gap between
primal and dual gap is not 0, but some given limit value (for example 0.05).
Here, we add another criterion to stop the search early, based on the (wall
clock) time spent and the relative improvement of the primal bound.
