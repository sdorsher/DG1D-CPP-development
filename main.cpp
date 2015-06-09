#include "TNT2.h"
#include "Grid.h"
#include "ReferenceElement.h"
#include "GridFunction.h"
#include "VectorGridFunction.h"
#include "Evolution.h"
#include "globals.h"
#include <cmath>
#include <fstream>
#include "ConfigParams.h"

//initial condition options
void initialGaussian(VectorGridFunction<double>& uh, Grid grd);
void initialSinusoid(VectorGridFunction<double>& uh, Grid grd);

//characterization of convergence, error using L2 norm
double LTwoerror(Grid thegrid, VectorGridFunction<double>& uh0, 
                 VectorGridFunction<double>& uhend);

int main()
{

  Grid thegrid(params.grid.elemorder, params.grid.numelems,
               params.grid.lowerlim, params.grid.upperlim);

  //declaration of calculation variables and 
  //initialization to either zero or value read from file
  VectorGridFunction<double> uh(params.waveeq.pdenum,
                                params.grid.numelems,
                                params.grid.elemorder+1,
                                0.0); 
  VectorGridFunction<double> uh0(params.waveeq.pdenum,
                                 params.grid.numelems,
                                 params.grid.elemorder + 1,
                                 0.0);
  //solution to PDE, possibly a vector 
  VectorGridFunction<double> RHSvgf(params.waveeq.pdenum,
                                    params.grid.numelems,
                                    params.grid.elemorder + 1,
                                    0.0); //right hand side of PDE

  GridFunction<double> nodes = thegrid.gridNodeLocations();

  //setup initial conditions
  if(params.waveeq.issinusoid){
    initialSinusoid(uh, thegrid);
  } else if(params.waveeq.isgaussian) {
    initialGaussian(uh, thegrid);
  }
  uh0 = uh;
  
  //set time based on smallest grid spacing
  double dt0 = nodes.get(0, 1) - nodes.get(0, 0);

  int nt = ceil(params.time.tmax / params.time.courantfac / dt0);

  double deltat;
  if(params.time.usefixedtimestep){
    deltat = params.time.dt;
  } else {
    deltat = (params.time.tmax - params.time.t0) / nt; 
    //make deltat go into tmax an integer number of times
  }
  cout << dt0 << " " << deltat << endl;
  

  //initialize loop variables to determine when output
  double output = deltat / 2.0;
  int outputcount = 0;

  ofstream fs;
  fs.open(params.file.pdesolution);

  for(double t = params.time.t0; t < params.time.tmax + deltat; t += deltat) {
    if(output > 0.0){
      //output in gnuplot format
      fs << endl << endl;
      fs << " #time = " << t << endl;
      for (int i = 0; i < uh.gridDim(); i++){
        for(int j = 0; j < uh.pointsDim(); j++){
          //print out at select time steps
          fs << thegrid.gridNodeLocations().get(i, j) << " " 
             << uh.get(0, i, j) << " " << uh.get(1, i, j) <<" " 
             << uh.get(2, i, j)<< endl;
        }
      }

      //output the difference in the waveforms between the 
      //oscillation initially and after one period
      if(outputcount == params.time.comparisoncount){
        cout << t << endl;
        ofstream fs2;
        fs2.open(params.file.oneperioderror);
        for(int i = 0; i < uh.gridDim(); i++){
          for (int j = 0; j < uh.pointsDim(); j++){
            fs2 << thegrid.gridNodeLocations().get(i, j) << " " 
                << uh.get(0, i, j) - uh0.get(0, i, j) << endl;
          }
        }
        fs2.close();

        //append the L2 error to that file, measured after one period
        ofstream fsconvergence;
        fsconvergence.open(params.file.L2error,ios::app);
            
        double L2;
        L2=LTwoerror(thegrid, uh0, uh);
        cout << "Order, deltat, num elems, L2 norm" << endl;
        cout << params.grid.elemorder << " " << deltat << " " 
            << params.grid.numelems << " " << L2 << endl;
        fsconvergence << params.grid.elemorder << " " << deltat 
                     << " " << params.grid.numelems << " " << L2 << endl;
        fsconvergence.close();
      }
      output -= params.time.outputinterval; 
      outputcount++;
    }
    //increment the timestep
    rk4lowStorage(thegrid, uh, RHSvgf, t, deltat);

    //increment the count to determine whether or not to output
    output += deltat;
  }
  //initial conditions, numerical fluxes, boundary conditions handled inside 
  //Evolution.cpp, in RHS.
}

//CURRENTLY UNTESTED
void initialSinusoid(VectorGridFunction<double>& uh, Grid grd){
  double omega = 2.0 * PI / params.sine.wavelength;
  
  GridFunction<double> nodes(uh.gridDim(), uh.pointsDim(), false);
  nodes=grd.gridNodeLocations();
  
  for(int i = 0; i < uh.gridDim(); i++){
    for (int j = 0; j < uh.pointsDim(); j++){
      double psi = params.sine.amp * sin(omega * nodes.get(i, j)
                                         + params.sine.phase);
      double pivar = omega * params.sine.amp * cos(omega * nodes.get(i, j)
                                           + params.sine.phase);
      double rho = -params.waveeq.speed * rho;
      //travelling wave
      uh.set(0, i, j, psi);
      uh.set(1, i, j, rho);
      uh.set(2, i, j, pivar);
    }
  }
}
 
void initialGaussian(VectorGridFunction<double>& uh, Grid grd){
  GridFunction<double> nodes(uh.gridDim(), uh.pointsDim(), false);
  nodes=grd.gridNodeLocations();
  
  for(int i = 0; i < uh.gridDim(); i++){
    for(int j = 0; j < uh.pointsDim(); j++){
      double gaussian = params.gauss.amp * exp(-pow((nodes.get(i, j)
                                                 - params.gauss.mu), 2.0)
                                               / 2.0 
                                               / pow(params.gauss.sigma, 2.0));
      double dgauss = -(nodes.get(i, j) - params.gauss.mu)
        / pow(params.gauss.sigma, 2.0) * gaussian;
      uh.set(0, i, j, gaussian);
      uh.set(1, i, j, 0.0); //time derivative is zero
                            //starts at center and splits
      uh.set(2, i, j, dgauss);
    }
  }
}

double LTwoerror(Grid thegrid, VectorGridFunction<double>& uh0, 
                 VectorGridFunction<double>& uhend)
{
  //the square root of the integral of the squared difference
  double L2;
  L2 = 0.0;
  Array1D<double> weights;
  weights = thegrid.refelem.getw();
  GridFunction<double> nodes(uh0.gridDim(), uh0.pointsDim(), false);
  nodes = thegrid.gridNodeLocations();
  for(int i = 0; i < uh0.gridDim(); i++){
    for(int j = 0; j < uh0.pointsDim(); j++){
      double added = weights[j] * pow(uh0.get(0, i, j)
                                   - uhend.get(0, i, j), 2.0)
        / thegrid.jacobian(i);
      L2 += added;
    }
  }
  L2 = sqrt(L2); 
  return L2;
}

