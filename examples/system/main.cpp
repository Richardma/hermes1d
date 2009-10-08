#include "hermes1d.h"
#include "solver_umfpack.h"

// ********************************************************************
// This example solves a system of two linear second-order equations 
// - Laplace u + v - f_0 = 0
// - Laplace v + u - f_1 = 0
// in interval (A, B) equipped with Dirichlet bdy conditions 
// u(A) = u(B) = 0, v(A) = v(B) = 1. The exact solution is 
// u(x) = sin(x), v(x) = cos(x). 

// general input:
static int N_eq = 2;
int N_elem = 3;                         // number of elements
double A = 0, B = 2*M_PI;              // domain end points
int P_init = 2;                        // initial polynomal degree

// Tolerance for Newton's method
double TOL = 1e-5;

// load function for equation 0
double f_0(double x) {
  return -sin(x) - cos(x);
}

// load function for equation 1
double f_1(double x) {
  return -sin(x) - cos(x);
}

// ********************************************************************

// Jacobi matrix block 0, 0 (equation 0, solution component 0)
// Note: u_prev[c][i] contains the values of solution component c 
// in integration point x[i]. similarly for du_prevdx.
double jacobian_0_0(int num, double *x, double *weights, 
                double *u, double *dudx, double *v, double *dvdx, 
                double u_prev[10][100], double du_prevdx[10][100], void *user_data)
{
  double val = 0;
  for(int i = 0; i<num; i++) {
    val += dudx[i]*dvdx[i]*weights[i];
  }
  return val;
};

// Jacobi matrix block 0, 1 (equation 0, solution component 1)
double jacobian_0_1(int num, double *x, double *weights, 
                double *u, double *dudx, double *v, double *dvdx, 
                double u_prev[10][100], double du_prevdx[10][100], void *user_data)
{
  double val = 0;
  for(int i = 0; i<num; i++) {
    val += u[i]*v[i]*weights[i];
  }
  return val;
};

// Jacobi matrix block 1, 0 (equation 1, solution component 0)
double jacobian_1_0(int num, double *x, double *weights, 
                double *u, double *dudx, double *v, double *dvdx, 
                double u_prev[10][100], double du_prevdx[10][100], void *user_data)
{
  double val = 0;
  for(int i = 0; i<num; i++) {
    val += u[i]*v[i]*weights[i];
  }
  return val;
};

// Jacobi matrix block 1, 1 (equation 1, solution component 1)
double jacobian_1_1(int num, double *x, double *weights, 
                double *u, double *dudx, double *v, double *dvdx, 
                double u_prev[10][100], double du_prevdx[10][100], void *user_data)
{
  double val = 0;
  for(int i = 0; i<num; i++) {
    val += dudx[i]*dvdx[i]*weights[i];
  }
  return val;
};

// Residual part 0 (equation 0)
double residual_0(int num, double *x, double *weights, 
                double u_prev[10][100], double du_prevdx[10][100], double *v, double *dvdx,
                void *user_data)
{
  double val = 0;
  for(int i = 0; i<num; i++) {
    val += (du_prevdx[0][i]*dvdx[i] + u_prev[1][i]*v[i]  - f_0(x[i])*v[i])*weights[i];
  }
  return val;
};

// Residual part 1 (equation 1) 
double residual_1(int num, double *x, double *weights, 
                double u_prev[10][100], double du_prevdx[10][100], double *v, double *dvdx,
                void *user_data)
{
  double val = 0;
  for(int i = 0; i<num; i++) {
    val += (du_prevdx[1][i]*dvdx[i] + u_prev[0][i]*v[i]  - f_1(x[i])*v[i])*weights[i];
  }
  return val;
};

/******************************************************************************/
int main() {
  // create mesh
  Mesh mesh(N_eq);
  mesh.create(A, B, N_elem);
  mesh.set_uniform_poly_order(P_init);
  mesh.set_bc_left_dirichlet(0, 0);
  mesh.set_bc_left_dirichlet(0, 1);
  mesh.set_bc_right_dirichlet(0, 0);
  mesh.set_bc_right_dirichlet(0, 1);
  mesh.assign_dofs();

  // register weak forms
  DiscreteProblem dp(&mesh);
  dp.add_matrix_form(0, 0, jacobian_0_0);
  dp.add_matrix_form(0, 1, jacobian_0_1);
  dp.add_matrix_form(1, 0, jacobian_1_0);
  dp.add_matrix_form(1, 1, jacobian_1_1);
  dp.add_vector_form(0, residual_0);
  dp.add_vector_form(1, residual_1);

  // variable for the total number of DOF 
  int N_dof = mesh.get_n_dof();
  printf("N_dof = %d\n", N_dof);

  // allocate Jacobi matrix and residual
  Matrix *mat;
  double *y_prev = new double[N_dof];
  double *res = new double[N_dof];

  // zero initial condition for the Newton's method
  for(int i=0; i<N_dof; i++) y_prev[i] = 0; 

  // Newton's loop
  int newton_iterations = 0;
  while (1) {
    // zero the matrix:
    mat = new CooMatrix(N_dof);

    // construct residual vector
    dp.assemble_matrix_and_vector(mat, res, y_prev); 

    // calculate L2 norm of residual vector
    double res_norm = 0;
    for(int i=0; i<N_dof; i++) res_norm += res[i]*res[i];
    res_norm = sqrt(res_norm);
    printf("Residual L2 norm: %g\n", res_norm);

    // if residual norm less than TOL, quit
    // latest solution is in y_prev
    if(res_norm < TOL) break;

    // changing sign of vector res
    for(int i=0; i<N_dof; i++) res[i]*= -1;

    // solving the matrix system
    solve_linear_system_umfpack((CooMatrix*)mat, res);

    // updating y_prev by new solution which is in res
    for(int i=0; i<N_dof; i++) y_prev[i] += res[i];

    newton_iterations++;
    printf("Finished Newton iteration: %d\n", newton_iterations);
  }

  Linearizer l(&mesh);
  const char *out_filename = "solution.gp";
  l.plot_solution(out_filename, y_prev);

  printf("Done.\n");
  return 1;
}
