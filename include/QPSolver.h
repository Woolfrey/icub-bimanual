#ifndef QPSOLVER_H_
#define QPSOLVER_H_

#include <math.h>
#include <vector>                                                                                    // std::vector

class QPSolver
{
	public:
		QPSolver() {}
		
		// These functions can be called without creating a QPSolver object:
		static Eigen::VectorXd solve(const Eigen::MatrixXd &H,                              // Solve a generic QP problem
		                             const Eigen::VectorXd &f,
		                             const Eigen::VectorXd &x0);
		                             
		static Eigen::VectorXd least_squares(const Eigen::VectorXd &y,                      // Solve an unconstrained least squares problem
		                                     const Eigen::MatrixXd &A,
		                                     const Eigen::MatrixXd &W,
		                                     const Eigen::VectorXd &x0);
		                                     
		static Eigen::VectorXd least_squares(const Eigen::VectorXd &xd,                     // Solve least squares with equality constraints
		                                     const Eigen::MatrixXd &W,
		                                     const Eigen::VectorXd &y,
		                                     const Eigen::MatrixXd &A,
		                                     const Eigen::VectorXd &x0);
		                                     
		// These functions require an object to be created since they use the
		// interior point solver:
		Eigen::VectorXd solve(const Eigen::MatrixXd &H,                                     // Solve a QP problem with inequality constraints
		                      const Eigen::VectorXd &f,
		                      const Eigen::MatrixXd &B,
		                      const Eigen::VectorXd &z,
		                      const Eigen::VectorXd &x0);
		                                                   
		Eigen::VectorXd least_squares(const Eigen::VectorXd &y,                             // Solve a constrained least squares problem
		                              const Eigen::MatrixXd &A,
		                              const Eigen::MatrixXd &W,
		                              const Eigen::VectorXd &xMin,
		                              const Eigen::VectorXd &xMax,
		                              const Eigen::VectorXd &x0);
		                              
		Eigen::VectorXd least_squares(const Eigen::VectorXd &xd,                            // Solve a constrained least squares problem
		                              const Eigen::MatrixXd &W,
		                              const Eigen::VectorXd &y,
		                              const Eigen::MatrixXd &A,
		                              const Eigen::VectorXd &xMin,
		                              const Eigen::VectorXd &xMax,
		                              const Eigen::VectorXd &x0);
		                         
	private:
		// These are variables used by the interior point method:
		double alpha0    = 1.0;                                                             // Modify size of Newton Step
		double alphaMod  = 0.5;                                                             // Modify step size when constraint violated
		double beta0     = 0.01;                                                            // Rate of decreasing barrier function
		double tol       = 1e-02;                                                           // Tolerance on step size
		double u0        = 1.0;                                                             // Scalar on barrier function
		double uMod      = 10.0;                                                            // Value to increase barrier function
		int    steps     = 20;                                                              // No. of steps to run interior point method
		                         
};                                                                                                  // Semicolon needed after class declaration

 ///////////////////////////////////////////////////////////////////////////////////////////////////
 //                        Solve a generic QP problem min 0.5*x'*H*x + x'*f                       //
///////////////////////////////////////////////////////////////////////////////////////////////////
Eigen::VectorXd QPSolver::solve(const Eigen::MatrixXd &H,
                                const Eigen::VectorXd &f,
                                const Eigen::VectorXd &x0)
{
	// Check the inputs are sound
	int n = x0.size();
	if(H.rows() != n or H.cols() != n or f.size() != n)
	{
		std::cerr << "[ERROR] [QPSOLVER] solve(): "
		          << "Dimensions of input arguments do not match. "
		          << "H matrix was " << H.rows() << "x" << H.cols() << ", "
		          << "f vector was " << f.size() << "x1, and "
		          << "x0 vector was " << x0.size() << "x1." << std::endl;
		
		return x0;
	}
	else	return H.partialPivLu().solve(-f);
}


  ///////////////////////////////////////////////////////////////////////////////////////////////////
 //          Solve a constrained QP problem: min 0.5*x'*H*x + x'*f subject to: B*x >= c           //
///////////////////////////////////////////////////////////////////////////////////////////////////
Eigen::VectorXd QPSolver::solve(const Eigen::MatrixXd &H,                                          // Solve a QP problem with inequality constraints
                                const Eigen::VectorXd &f,
                                const Eigen::MatrixXd &B,
                                const Eigen::VectorXd &z,
                                const Eigen::VectorXd &x0)
{
	// Check the inputs are sound
	int n = x0.size();
	if(H.rows() != n or H.cols() != n or f.size() != n or B.rows() != z.size())
	{
		std::cout << "[ERROR] [QPSOLVER] solve(): "
		          << "Dimensions of input arguments do not match. "
		          << "H matrix was " << H.rows() << "x" << H.cols() << ", "
		          << "f vector was " << f.size() << "x1, "
		          << "B matrix was " << B.rows() << "x" << B.cols() << ", "
		          << "z vector was " << z.size() << "x1, and "
		          << "x0 vector was " << n << "x1." << std::endl;
		          
		return x0;
	}
	else
	{
		// Solve the following optimization problem with Guass-Newton method:
		//
		//    min f(x) = 0.5*x'*H*x + x'*f - u*sum(log(d_i))
		//
		// where d_i = b_i*x - z_i is the distance to the constraint
		//
		// Then the gradient and Hessian are:
		//
		//    g(x) = H*x + f - u*sum((1/d_i)*b_i')
		//
		//    I(x) = H + u*sum((1/(d_i^2))*b_i'*b_i)
		
		// Local variables
	
		Eigen::MatrixXd I;                                                                  // Hessian matrix
		Eigen::VectorXd g(n);                                                               // Gradient vector
		Eigen::VectorXd dx = Eigen::VectorXd::Zero(n);                                      // Newton step = -I^-1*g
		Eigen::VectorXd x = x0;                                                             // Assign initial state variable
		
		double alpha;                                                                        // Scalar for Newton step
		double beta  = this->beta0;                                                          // Shrinks barrier function
		double u     = this->u0;                                                             // Scalar for barrier function
	
		int numConstraints = B.rows();
		
		std::vector<double> d; d.resize(numConstraints);
		
		// Do some pre-processing
		std::vector<Eigen::VectorXd> bt(numConstraints);
		std::vector<Eigen::MatrixXd> btb(numConstraints);
		for(int j = 0; j < numConstraints; j++)
		{
			bt[j]  = B.row(j).transpose();                                              // Row vectors of B transposed
			btb[j] = bt[j]*bt[j].transpose();                                           // Outer product of row vectors
		}
		
		// Run the interior point method
		for(int i = 0; i < this->steps; i++)
		{
			// (Re)set values for new loop
			g.setZero();                                                                // Gradient vector
			I = H;                                                                      // Hessian for log-barrier function
			
			// Compute distance to each constraint
			for(int j = 0; j < numConstraints; j++)
			{
				d[j] = bt[j].dot(x) - z(j);                                         // Distance to jth constraint
				
				if(d[j] <= 0)
				{
					if(i == 0)
					{
						std::cerr << "[ERROR] [QPSOLVER] solve(): "
						          << "Start point x0 is outside the constraints!" << std::endl;
						          
						std::cout << "Constraint no. " << j+1 << " distance " << d[j] << std::endl;
						
						return x0;
					}
			
					d[j] = 1e-03;                                               // Set a small, non-zero value
					u *= 100;
				}
						
				g += -(u/d[j])*bt[j];                                               // Add up gradient vector
				I +=  (u/(d[j]*d[j]))*btb[j];                                       // Add up Hessian
				
			}
			
			g += H*x + f;                                                               // Finish summation of gradient vector

			dx = I.partialPivLu().solve(-g);
			
			// Shrink step size until within the constraint
			alpha = this->alpha0;
			for(int j = 0; j < numConstraints; j++)
			{
				while( d[j] + alpha*bt[j].dot(dx) < 0) alpha *= this->alphaMod;
			}

			if(alpha*dx.norm() < this->tol) break;
			
			// Update values for next loop
			x += alpha*dx;                                                              // Increment state
			u *= beta;                                                                  // Decrease barrier function
		}
		
		return x;
	}
}	

  ///////////////////////////////////////////////////////////////////////////////////////////////////
 //          Solve an unconstrained least squares problem: min 0.5(y-*Ax)'*W*(y-A*x)              //
///////////////////////////////////////////////////////////////////////////////////////////////////
Eigen::VectorXd QPSolver::least_squares(const Eigen::VectorXd &y,
                                        const Eigen::MatrixXd &A,
                                        const Eigen::MatrixXd &W,
                                        const Eigen::VectorXd &x0)
{
	// Check the inputs are sound
	int m = A.rows();
	int n = A.cols();
	if(y.size() != m or x0.size() != n)
	{
		std::cerr << "[ERROR] [QPSOLVER] least_squares(): "
		          << "Dimensions of inputs arguments do not match! "
		          << "The y vector was " << y.size() << "x1, "
		          << "the A matrix was " << m << "x" << n << ", "
		          << "the W matrix was " << W.rows() << "x" << W.cols() << ", and "
		          << "the x0 vector was " << x0.size() << "x1." << std::endl;
		
		return x0;
	}
	else if(W.rows() != m or W.cols() != m)
	{
		std::cerr << "[ERROR] [QPSOLVER] least_squares(): "
		          << "Weighting matrix W was " << W.rows() << "x" << W.cols() << ", "
		          << "but expected " << m << "x" << m << "." << std::endl;
		return x0;
	}
	else
	{
		Eigen::MatrixXd AtW = A.transpose()*W;
		return solve(AtW*A,-AtW*y,x0);                                                      // Convert to standard form and solve
	}
}
  
  ///////////////////////////////////////////////////////////////////////////////////////////////////
 //    Solve a constrained least squares problem 0.5*(y-A*x)'*W*(y-A*x) s.t. xMin <= x <= xMax    //
///////////////////////////////////////////////////////////////////////////////////////////////////                           
Eigen::VectorXd QPSolver::least_squares(const Eigen::VectorXd &y,
                                        const Eigen::MatrixXd &A,
                                        const Eigen::MatrixXd &W,
                                        const Eigen::VectorXd &xMin,
                                        const Eigen::VectorXd &xMax,
                                        const Eigen::VectorXd &x0)
{
	int m = A.rows();
	int n = A.cols();
	
	// Check that the inputs are sound
	if(y.size() != m or x0.size() != n or xMin.size() != n or xMax.size() != n or x0.size() != n)
	{
		std::cerr << "[ERROR] [QPSOLVER] least_squares(): "
		          << "Dimensions of inputs arguments do not match! "
		          << "The y vector was " << y.size() << "x1, "
		          << "the A matrix was " << m << "x" << n << ", "
		          << "the W matrix was " << W.rows() << "x" << W.cols() << ", "
		          << "the xMin vector was " << xMin.size() << "x1, "
		          << "the xMax vector was " << xMax.size() << "x1, and "
		          << "the x0 vector was " << x0.size() << "x1." << std::endl;
		
		return x0;
	}
	else if(W.rows() != m or W.cols() != m)
	{
		std::cerr << "[ERROR] [QPSOLVER] least_squares(): "
		          << "Weighting matrix W was " << W.rows() << "x" << W.cols() << ", "
		          << "but expected " << m << "x" << m << "." << std::endl;
		return x0;
	}
	else
	{
		// Set up constraint matrices in standard form Bx >= c
		Eigen::MatrixXd B(2*n,n);
		B.block(0,0,n,n) = -1*Eigen::MatrixXd::Identity(n,n);
		B.block(n,0,n,n) =    Eigen::MatrixXd::Identity(n,n);
		
		Eigen::VectorXd z(2*n);
		z.head(n) = -xMax;
		z.tail(n) =  xMin;
		
		Eigen::MatrixXd AtW = A.transpose()*W;                                              // Makes calcs a little simpler
		
		return solve(AtW*A,-AtW*y, B, z, x0);                                               // Convert to standard form and solve
	}
}

  ///////////////////////////////////////////////////////////////////////////////////////////////////
 //       Solve a least squares problem of the form min 0.5*(xd-x)'*W*(xd-x)  s.t. A*x = y        //
///////////////////////////////////////////////////////////////////////////////////////////////////                                  
Eigen::VectorXd QPSolver::least_squares(const Eigen::VectorXd &xd,
                                        const Eigen::MatrixXd &W,
                                        const Eigen::VectorXd &y,
                                        const Eigen::MatrixXd &A,
                                        const Eigen::VectorXd &x0)
{
	// Check that the inputs are sound
	int m = A.rows();
	int n = A.cols();
	
	if(xd.size() != n or y.size() != m or x0.size() != n)
	{
		std::cerr << "[ERROR] [QPSOLVER] least_squares(): "
		          << "Dimensions of input arguments do not match. "
		          << "The xd vector was " << xd.size() << "x1, "
		          << "the W matrix was " << W.rows() << "x" << W.cols() << ", "
		          << "the y vector was " << y.size() << "x1, "
		          << "the A matrix was " << m << "x" << n << ", and "
		          << "the x0 vector was " << x0.size() << "x1." << std::endl;
		
		return x0;
	}
	else if(W.rows() != n or W.cols() != n)
	{
		std::cerr << "[ERROR] [QPSOLVER] least_squares(): "
		          << "Weighting matrix W was " << W.rows() << "x" << W.cols() << ", "
		          << "but expected " << n << "x" << n << "." << std::endl;
		
		return x0;
	}
	else
	{
		// Lagrangian L = 0.5*x'*W*x - x'*W*xd + (A*x - y)'*lambda,
		// Solution exists for:
		//
		// [ dL/dlambda ]  =  [ 0   A ][ lambda ] - [   y  ] = [ 0 ]
		// [   dL/dx    ]     [ A'  W ][   x    ]   [ W*xd ]   [ 0 ]
		
		Eigen::MatrixXd H = Eigen::MatrixXd::Zero(m+n,m+n);
//		H.block(0,0,m,m) = Eigen::MatrixXd::Zero(m,n);
		H.block(0,m,m,n) = A;
		H.block(m,0,n,m) = A.transpose();
		H.block(m,m,n,n) = W;
		
		Eigen::VectorXd v; v.resize(m+n);
		v.head(m) = y;
		v.tail(n) = W*xd;
		
		Eigen::VectorXd solution = H.partialPivLu().solve(v);
		
		return solution.tail(n);
	}
}

  ///////////////////////////////////////////////////////////////////////////////////////////////////
 //     Solve a problem of the form min 0.5*(xd-x)'*W*(xd-x)  s.t. A*x = y, xMin <= x <= xMax     //
///////////////////////////////////////////////////////////////////////////////////////////////////  
Eigen::VectorXd QPSolver::least_squares(const Eigen::VectorXd &xd,
                                        const Eigen::MatrixXd &W,
                                        const Eigen::VectorXd &y,
                                        const Eigen::MatrixXd &A,
                                        const Eigen::VectorXd &xMin,
                                        const Eigen::VectorXd &xMax,
                                        const Eigen::VectorXd &x0)
{
	// Check that the inputs are sound
	int m = A.rows();
	int n = A.cols();
	if(xd.size() != n or y.size() != m or x0.size() != n)
	{
		std::cerr << "[ERROR] [QPSOLVER] least_squares(): "
		          << "Dimensions of input arguments do not match. "
		          << "The xd vector was " << xd.size() << "x1, "
		          << "the W matrix was " << W.rows() << "x" << W.cols() << ", "
		          << "the y vector was " << y.size() << "x1, "
		          << "the A matrix was " << m << "x" << n << ", "
		          << "the xMin vector was " << xMin.size() << "x1, "
		          << "the xMax vector was " << xMax.size() << "x1, and "
		          << "the x0 vector was " << x0.size() << "x1." << std::endl;
		
		return x0;
	}
	else if(W.rows() != n or W.cols() != n)
	{
		std::cerr << "[ERROR] [QPSOLVER] least_squares(): "
		          << "Weighting matrix W was " << W.rows() << "x" << W.cols() << ", "
		          << "but expected " << n << "x" << n << "." << std::endl;
		
		return x0;
	}
	else
	{
		// Convert to standard form 0.5*x'*H*x + x'*f subject to B*x >= z
		// where "x" is now [lambda' x' ]'
		
		// H = [ 0  A ]
		//     [ A' W ]
		Eigen::MatrixXd H(m+n,m+n);
		H.block(0,0,m,m) = Eigen::MatrixXd::Zero(m,m);
		H.block(0,m,m,n) = A;
		H.block(m,0,n,m) = A.transpose();
		H.block(m,m,n,n) = W;
		
		// B = [ 0 -I ]
		//     [ 0  I ]
		Eigen::MatrixXd B(2*n,m+n);
		B.block(0,0,2*n,m) = Eigen::MatrixXd::Zero(2*n,m);
		B.block(0,m,  n,n) =-Eigen::MatrixXd::Identity(n,n);
		B.block(n,m,  n,n) = Eigen::MatrixXd::Identity(n,n);

		// z = [ -xMax ]
		//     [  xMin ]
		Eigen::VectorXd z(2*n);
		z.head(n) = -xMax;
		z.tail(n) =  xMin;
		
		// Interior point method sometimes fails if desired point is outside constraints
		Eigen::VectorXd temp = xd;
		for(int i = 0; i < n; i++)
		{
			if     (xd(i) < xMin(i)) temp(i) = xMin(i) + 0.001;
			else if(xd(i) > xMax(i)) temp(i) = xMax(i) - 0.001;
		}

		// f = [   y  ]
		//     [ W*xd ]
		Eigen::VectorXd f(m+n);
		f.head(m) = -y;
		f.tail(n) = -W*temp;
		
		Eigen::VectorXd state(m+n);
		state.head(m) = Eigen::VectorXd::Zero(m);
		state.tail(n) = x0;
		
		state = solve(H,f,B,z,state);                                                       // Convert to standard form and solve
		
		return state.tail(n);
	}
}
#endif
