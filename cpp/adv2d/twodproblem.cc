#include "twodproblem.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cassert>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <string>
#include <iomanip> // for std::setprecision
#include <chrono>
#include <algorithm> // for std::min

using namespace std;
// You should set the CFL according to your problem.
#define SIGN(a) (((a)<0) ? -1:1)
//------------------------------------------------------------------------------
// Minmod limiter
// minmod ( 2(u0-ul), (ur-ul)/2, 2(ur-u0) )
//------------------------------------------------------------------------------
double minmod (const double& ul, const double& u0, const double& ur)
{
   double result;
   double beta=1.0;

   double db = beta*(u0 - ul);         // backward difference
   double df = beta*(ur - u0);         // forward difference
   double dc = 0.5 * (ur - ul); // central difference

   if ( (db*df > 0.0) & (dc*df >0.0) )
   {
      result = std::min(min(fabs(db), fabs(dc)), fabs(df) );
      result *= SIGN(db);
   }
   else
      result = 0.0;
   return result;
}

double TwoDProblem::reconstruct(const double& sol_ll,const double& sol_l,const double& sol_r)
{   
    if( scheme == "so"){
       double theta = 0.5;
       return  sol_l + 0.5 * 2.0 * theta * minmod(sol_ll, sol_l, sol_r);
    }
    else return sol_l;
}
// For file name
void createDirectory(const std::string& dirname) 
{
    struct stat info;
    if(stat(dirname.c_str(), &info) != 0 || !(info.st_mode & S_IFDIR)) {
        if(mkdir(dirname.c_str(), 0777) != 0) {
            std::cerr << "Error creating directory " << dirname << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << "Directory " << dirname << " is created" << std::endl;
    }
}
std::string getFilename(const std::string& basename, int id) {
    std::ostringstream oss;
    oss << basename << "_" << std::setfill('0') << std::setw(4) << id << ".plt";
    return oss.str();
}
//-------------------------------------------------------------------------------
// flux functions
//---------------------------------------------------------------------------------
double TwoDProblem::xflux(const double& u)
{
	return  u;
}
double TwoDProblem::yflux(const double& u)
{
	return u;
}
double  TwoDProblem::initial_data( const double& x, const double& y)
{
return sin(2.0 * M_PI * x)* sin(2.0 * M_PI * y);
}
// Numerical  flux in the x direction   across vertical wall
double TwoDProblem::xnumflux( const double& ul,
       const double& ur)
{
 return  ul;//0.5*( xflux(ul) + xflux(ur) - (ur-ul)/lam_x );
}

// numerical flux in the y direction  across  horizontal wall
double TwoDProblem::ynumflux( const double& ul,
       const double& ur)
{
return ul;//0.5*( yflux(ul) + yflux(ur) - (ur-ul)/lam_y);
}
//------------------------------------------------------------------------------
// Create cartesian grid
//------------------------------------------------------------------------------
void TwoDProblem::make_grid ()
{
   grid.xmax = 1.0;
   grid.xmin = 0.0;
   grid.ymax = 1.0;
   grid.ymin = 0.0;
   grid.nx = nx;
   grid.ny = ny;
   grid.dx = (grid.xmax-grid.xmin)/grid.nx;
   grid.dy = (grid.ymax-grid.ymin)/grid.ny;
   grid.allocate();
   cout<<"Making grid for 2Dclw problem ..." << endl;
   // write cell centers starting from left bottom corner
   for (unsigned int i = 0; i < grid.nx ; ++i) 
      {
      for (unsigned int j = 0; j < grid.ny ; ++j) 
         {
         grid.xc(i,j) = grid.xmin + (i + 0.5 ) * grid.dx;
         grid.yc(i,j) = grid.ymin + (j + 0.5) * grid.dy ;
        }
      }
   // write cell vertices
   for (unsigned int i = 0; i < grid.nx +1 ; ++i) 
      {
      for (unsigned int j = 0; j < grid.ny+1 ; ++j) 
         {
         grid.x(i,j) = grid.xmin + i * grid.dx;
         grid.y(i,j) = grid.ymin + j* grid.dy ;
         }
      }
      
}
//------------------------------------------------------------------------------
// allocate memory and set initial condition
//------------------------------------------------------------------------------
void TwoDProblem::initialize ()
{
   sol.allocate(grid.nx+4, grid.ny+4); // with two ghost cells each side
   // initialize only real cells
   for (unsigned int i = 2; i < grid.nx + 2; ++i) 
      {
        for (unsigned int j = 2; j < grid.ny + 2; ++j) 
          {
            sol(i,j) = initial_data( grid.xc(i-2,j-2), grid.yc(i-2,j-2));
          }
      }        
}
//------------------------------------------------------------------------------
// residual
//------------------------------------------------------------------------------
void TwoDProblem::compute_residual(Matrix& res)
{ 
    // set residual to zero; this is a potential point
    res = 0.0;
    lam_x = dt / grid.dx;
    lam_y = dt / grid.dy;
    // Loop over interior vertical faces includig the boundary
    //#pragma omp parallel for collapse(2)
    for (unsigned int i = 1; i < grid.nx + 2; ++i){// face between (i,j) and (i+1,j)
       for (unsigned int j = 2; j < grid.ny + 2; ++j){
          double sl = reconstruct(sol(i-1,j),sol(i,j), sol(i+1,j));
          double sr = reconstruct(sol(i+2,j), sol(i+1,j), sol(i,j));
          double Fn = xnumflux(sl, sr);
          res(i,j) +=  lam_x * Fn;
          res(i+1,j)-=  lam_x * Fn;
        }
    }
    // Loop over horizontal faces including the boundary faces
    //// face between (i,j) and (i,j+1)
    //#pragma omp parallel for collapse(2)
    for (unsigned int j = 1; j < grid.ny + 2; ++j){
       for (unsigned int i = 2; i < grid.nx + 2; ++i){
          double sl = reconstruct(sol(i,j-1), sol(i,j), sol(i,j+1));
          double sr = reconstruct(sol(i,j+2), sol(i,j+1), sol(i,j));
          double Gn = ynumflux(sl,sr);
          res(i,j) += lam_y * Gn;
          res(i,j+1) -= lam_y * Gn;
        }
    }
}
//------------------------------------------------------------------------------
// Update solution in ghost cells
// Change this according to your test case
//------------------------------------------------------------------------------
void TwoDProblem::updateGhostCells ()
{
    // left vertical 
    for (unsigned int j = 0; j <= grid.ny + 3; ++j) {
        sol(0,j) = sol(grid.nx, j );
        sol(1,j) = sol(grid.nx+1,j) ; 
    }
    // right ghost cell
    for (unsigned int j = 0; j <= grid.ny + 3; ++j) 
    {   sol(grid.nx+3,j) = sol(3,j);
        sol(grid.nx+2,j) = sol(2,j);
    }
    // bottom ghost cell
    for (unsigned int i = 0; i <= grid.nx + 3; ++i) {
        sol(i,0) = sol( i, grid.ny);
        sol(i,1) = sol(i, grid.ny+1);
    }
    // top ghost cell
    for (unsigned int i = 0; i <= grid.nx + 3; ++i) {
        sol(i, grid.ny+2) = sol( i, 2);
        sol(i, grid.ny+3) = sol(i,3);
    }
}
//------------------------------------------------------------------
// Save solution to file in the .dat format for gnuplot purpose.
//------------------------------------------------------------------
void TwoDProblem::savesol(double t, Matrix& sol) 
{   
    std::string dirname = "sol";
    createDirectory(dirname);
    if(fileid == 0) {
        std::cout << "The directory \"sol\" is going to be formatted!" << std::endl;
        /* use this to prevent erasing the existing directory
        std::string response;
        std::cout << "Do You Want To Continue? [y/n] ";
        std::cin >> response;
        if(response != "y") {
            std::cerr << "Execution is terminated" << std::endl;
            exit(EXIT_FAILURE);
        }
        */
        std::string pattern = "./sol/*";
        // Remove existing files
        system(("rm -f " + pattern).c_str());
    }
    std::string filename = getFilename("sol/sol", fileid);
    std::ofstream file(filename);
    file << "TITLE = \"Linear advection equation\"" << std::endl;
    file << "VARIABLES = \"x\", \"y\", \"sol\"" << std::endl;
    file << "ZONE STRANDID=1, SOLUTIONTIME=" << t << ", I=" << grid.nx << ", J=" << grid.ny << ", DATAPACKING=POINT" << std::endl;
    for(unsigned int i = 0; i < grid.nx ; ++i) {
        for(unsigned int j = 0; j < grid.ny ; ++j) {
            file << std::setprecision(8) << std::fixed << grid.xc(i,j) << ", " << grid.yc(i,j) << ", " << sol(i+2,j+2) << std::endl;
        }
    }
    file.close();
    fileid++;
}
std::vector<double> TwoDProblem::findMinMax() 
{
    std::vector<double> maxmin(2);
    double sol_max = -1.0e20;
    double sol_min = 1.0e20;
    //#pragma omp parallel for collapse(2)
    for (unsigned int i = 2; i < grid.nx+2; ++i)
      {
        for (unsigned int j = 2; j < grid.ny+2; ++j)
        {
         sol_max = max(sol(i,j), sol_max);
         sol_min = min( sol(i,j), sol_min);
        }
      }
    maxmin[0] = sol_max;
    maxmin[1] = sol_min;
    return maxmin;
}
//-----------------------------------------------------------------------------
// Compute error in the case of smooth test case
// Final time solution and initial condition
// are the same here
void TwoDProblem::compute_error(double& l1error)
//-----------------------------------------------------------------------------
{    l1error = 0.0;
    for (unsigned int i = 2; i < grid.nx+2; ++i)
      {
        for (unsigned int j = 2; j < grid.ny+2; ++j)
        {
            l1error += abs( sol(i,j)- initial_data( grid.xc(i-2,j-2), grid.yc(i-2,j-2) ) );
        } 
      } 
    l1error*=(grid.dx*grid.dy);
}
// ssprk2 time stepping
void TwoDProblem::apply_ssprk2(){
     sol_old = sol;
     updateGhostCells();
     compute_residual(res);
     sol = sol- res;
     updateGhostCells();
     compute_residual(res);
     sol = sol - res;
     sol = (sol_old + sol) * 0.5;  
}
// Forward euler time stepping
void TwoDProblem::apply_euler(){
     updateGhostCells();
     compute_residual(res);
     sol = sol- res;
    }
//------------------------------------------------------------------------------
// perform time stepping
//------------------------------------------------------------------------------
void TwoDProblem::solve(){
    double time  = 0.0; // initial time
    unsigned iter = 0;
    fileid = 0;
    dt  = cfl * grid.dx; // time step
    res.allocate(grid.nx+4, grid.ny+4);
    sol_old.allocate(grid.nx+4, grid.ny+4);
	savesol(0.0, sol); // save initial condition
    while (time < Tf)
    {
     if (time + dt >Tf){ dt = Tf-time;}
     //Update time solution
     if (scheme =="so") apply_ssprk2(); // update solution by rk time stepping 
     else apply_euler(); // update solution by fo scheme
     time +=dt;
     iter +=1;
     if (save_freq > 0){       
        if (iter % save_freq == 0){savesol(time, sol);}
        std::cout << std::left;
        std::cout << "iter = " << std::setw(8) << iter 
              << "time = " << std::setw(10) << time 
              << "Max = " << std::setw(15) << findMinMax()[0] 
              << "Min = " << std::setw(15) << findMinMax()[1] << std::endl;
     }
    }
    // save final time solution
    savesol(time,sol);
}
//------------------------------------------------------------------------------
// solve the whole problem
//------------------------------------------------------------------------------
void TwoDProblem::run ()
{
  make_grid();
  initialize();
  auto start_wall = std::chrono::system_clock::now();
  solve();
  auto end_wall = std::chrono::system_clock::now();
  std::chrono::duration<double> duration_wall = end_wall - start_wall;
  double l1error;
  compute_error(l1error);
  cout<<"#cells, h, l1error, WCT"<<endl;
  cout<< grid.nx * grid.ny <<" "<< grid.dx <<" "<< l1error<<" "<< duration_wall.count()<< endl;
  // WCT- Wall clock time
}
