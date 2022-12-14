    ////////////////////////////////////////////////////////////////////////////////////////////////////
   //                                                                                                //
  //                     A class for interfacing with the joint motors on the iCub                  //
 //                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef JOINTINTERFACE_H_
#define JOINTINTERFACE_H_

#include <iostream>                                                                                 // std::cerr, std::cout
#include <math.h>                                                                                   // M_PI
#include <string>                                                                                   // std::string
#include <vector>                                                                                   // std::vector
#include <yarp/dev/ControlBoardInterfaces.h>                                                        // I don't know what this does exactly...
#include <yarp/dev/PolyDriver.h>                                                                    // ... or this...
#include <yarp/os/Property.h>                                                                       // ... or this.

class JointInterface
{
	public:
		JointInterface(const std::vector<std::string> &jointList,
		               const std::vector<std::string> &portList);
		
		bool activate_control();                                                            // Activates control if construction successful
		
		bool get_joint_state(std::vector<double> &_pos,
		                     std::vector<double> &_vel);
		                     
		bool get_joint_state(std::vector<double> &_pos,
		                     std::vector<double> &_vel,
		                     std::vector<double> &_acc,
		                     std::vector<double> &_torque);
		                     	
		bool read_encoders();                                                               // Update the joint states internally
		
		bool send_velocity_command(const double &command, const int &i);                    // Send a velocity command to a joint motor
		
		bool send_velocity_commands(const std::vector<double> &commands);                   // Send velocity commands to several motors
		
		std::vector<double> get_joint_positions()  const { return this->pos; }              // As it says on the label
		
		std::vector<double> get_joint_velocities() const { return this->vel; }              // As it says on the label
		
		void close();
        
        protected:
		int n;                                                                              // Number of joints being controlled
		
		std::vector<std::array<double,2>> pLim;
		std::vector<double>   vLim;                                                         // Maximum velocity for the joint motors
                                
	private:
		// Properties
		bool isValid = false;                                                               // Won't do anything if false	
		
		std::vector<double> pos;                                                            // Vector of current joint positions
		std::vector<double> vel;                                                            // Vector of current joint velocities
		std::vector<double> acc;
		std::vector<double> torque;
		
	   	// These interface with the hardware on the robot itself
		yarp::dev::IControlLimits*   limits;                                                // Joint limits?
		yarp::dev::IControlMode*     mode;                                                  // Sets the control mode of the motor
		yarp::dev::IEncoders*        encoders;                                              // Joint position values (in degrees)
		yarp::dev::IVelocityControl* controller;                                            // Low-level motor controller via YARP
//              yarp::dev::iVelocityControl  vController;
//              yarp::dev::iTorqueControl    tController;
		yarp::dev::PolyDriver        driver;                                                // Device driver
	
};                                                                                                  // Semicolon needed after class declaration

  ////////////////////////////////////////////////////////////////////////////////////////////////////
 //                                       Constructor                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////
JointInterface::JointInterface(const std::vector<std::string> &jointList,
                               const std::vector<std::string> &portList) :
                               n(jointList.size())
{
	// Resize std::vector objects
	this->pos.resize(this->n);                                                                  // Resize position vector
	this->vel.resize(this->n);                                                                  // Resize velocity vector
	this->acc.resize(this->n);                                                                  // Resize acceleration vector
	this->torque.resize(this->n);                                                               // Resize the torque vector
	this->pLim.resize(this->n);                                                                 // Resize position limits vector
	this->vLim.resize(this->n);                                                                 // Resize max velocity vector
	
	/************************** I copied this code from elsewhere *****************************/
	
	// Open up device drivers
	yarp::os::Property options;									
	options.put("device", "remotecontrolboardremapper");
	options.addGroup("axesNames");
	
	yarp::os::Bottle & bottle = options.findGroup("axesNames").addList();
	for(int i = 0; i < jointList.size(); i++) bottle.addString(jointList[i].c_str());           // Add the list of all the joint names
		
	yarp::os::Bottle remoteControlBoards;
	yarp::os::Bottle & remoteControlBoardsList = remoteControlBoards.addList();
	for(int i = 0; i < portList.size(); i++) remoteControlBoardsList.addString(portList[i]);    // Add the remote control board port names
	
	options.put("remoteControlBoards", remoteControlBoards.get(0));
	options.put("localPortPrefix", "/local");
	
	yarp::os::Property &remoteControlBoardsOpts = options.addGroup("REMOTE_CONTROLBOARD_OPTIONS");
			    remoteControlBoardsOpts.put("writeStrict", "on");
			    
	/******************************************************************************************/
	
	if(not this->driver.open(options))
	{
		std::cerr << "[ERROR] [JOINT INTERFACE] Constructor: "
			  << "Could not open device driver." << std::endl;
	}
	else
	{	
		if(not this->driver.view(this->controller))
		{
			std::cerr << "[ERROR] [JOINT INTERFACE] Constructor: "
			          << "Unable to configure the controller for the joint motors." << std::endl;
		}
		else if(not this->driver.view(this->mode))
		{
			std::cerr << "[ERROR] [JOINT INTERFACE] Constructor: "
				  << "Unable to configure the control mode." << std::endl;
		}
		else if(not this->driver.view(this->limits))
		{
			std::cerr << "[ERROR] [JOINT INTERFACE] Constructor: "
			          << "Unable to obtain the joint limits." << std::endl;
		}
		else
		{
			// Opened the motor controllers, so get the joint limits
			for(int i = 0; i < this->n; i++)
			{
				double notUsed;
				this->limits->getLimits   (i, &this->pLim[i][0], &this->pLim[i][1]);// Get the position limits
				this->limits->getVelLimits(i, &notUsed,          &this->vLim[i]);   // Get velocity limits (assume vMin = -vMax);

				// Convert from degrees to radians
				this->pLim[i][0] *= M_PI/180;
				this->pLim[i][1] *= M_PI/180;
				this->vLim[i]    *= M_PI/180;
			}
			
			// Finally, configure the encoders
			if(not this->driver.view(this->encoders))
			{
				std::cerr << "[ERROR] [JOINT INTERFACE] Constructor: "
					  << "Unable to configure the encoders." << std::endl;
			}
			else
			{
				double temp[this->n];                                               // Temporary array to hold encoder values
				
				// Make 5 attempts to read the encoders
				for(int i = 0; i < 5; i++)
				{
					if(not this->encoders->getEncoders(temp) and i == 4)
					{
						std::cerr << "[ERROR] [JOINT INTERFACE] Constructor: "
						          << "Could not obtain encoder values in 5 attempts." << std::endl;
					}
					else
					{
						this->isValid = true;                               // Success! We made it to the end.
						
						for(int j = 0; j < this->n; j++) this->pos[i] = temp[i]*M_PI/180; // Assign initial joint values
						
						break;
					}
				}
			}
		}					
	}
	
	if(this->isValid)
	{
		std::cout << "[INFO] [JOINT INTERFACE] Constructor: "
			  << "Successfully configured the drivers." << std::endl;
	}					  
	else this->driver.close();
}

  ////////////////////////////////////////////////////////////////////////////////////////////////////
 //                       Set control mode and allow commands to be sent                           //
////////////////////////////////////////////////////////////////////////////////////////////////////
bool JointInterface::activate_control()
{
	if(not this->isValid)
	{
		std::cerr << "[ERROR] [JOINT INTERFACE] activate_control(): "
		          << "Something went wrong during the construction of this object. "
		          << "Could not activate joint control." << std::endl;
		          
		return false;
	}
	else
	{
		for(int i = 0; i < this->n; i++)
		{
			this->mode->setControlMode(i, VOCAB_CM_VELOCITY);                           // Activate velocity control mode on the joint motors
			this->controller->setRefAcceleration(i, std::numeric_limits<double>::max()); // CHANGE THIS?
			this->controller->velocityMove(i, 0.0);					// Ensure initial velocity is zero
		}	
		return true;
	}  
}

  ////////////////////////////////////////////////////////////////////////////////////////////////////
 //                            Get both the joint positions and velocities                         //
////////////////////////////////////////////////////////////////////////////////////////////////////
bool JointInterface::get_joint_state(std::vector<double> &_pos,
                                     std::vector<double> &_vel)
{
	if(not this->isValid)
	{
		std::cerr << "[ERROR] [JOINT INTERFACE] get_joint_state(): "
		          << "Something went wrong during the construction of this object. "
		          << "Could not obtain the joint state." << std::endl;
		          
		return false;
	}
	else
	{
		_pos = this->pos;
		_vel = this->vel;
		
		return true;
	}
}

  ///////////////////////////////////////////////////////////////////////////////////////////////////
 //                                 Get the full joint state                                      //
///////////////////////////////////////////////////////////////////////////////////////////////////
bool JointInterface::get_joint_state(std::vector<double> &_pos,
                                     std::vector<double> &_vel,
                                     std::vector<double> &_acc,
                                     std::vector<double> &_torque)
{
	if(not this->isValid)
	{
		std::cerr << "[ERROR] [JOINT INTERFACE] get_joint_state(): "
		          << "Something went wrong during the construction of this object. "
		          << "Could not obtain the joint state." << std::endl;
		
		return false;
	}
	else
	{
		_pos    = this->pos;
		_vel    = this->vel;
		_acc    = this->acc;
		_torque = this->torque;
	
		return true;
	}
}

  ////////////////////////////////////////////////////////////////////////////////////////////////////
 //                         Get new joint state information from the encoders                      //
////////////////////////////////////////////////////////////////////////////////////////////////////
bool JointInterface::read_encoders()
{
	if(this->isValid)
	{
		bool success = true;
		for(int i = 0; i < this->n; i++)
		{
			success &= this->encoders->getEncoder(i, &this->pos[i]);                            // Read the position
			success &= this->encoders->getEncoderSpeed(i, &this->vel[i]);                       // Read the velocity
			success &= this->encoders->getEncoderAcceleration(i, &this->acc[i]);                // Read the acceleration
			
			this->pos[i] *= M_PI/180;                                                           // Convert to rad
			this->vel[i] *= M_PI/180;                                                           // Convert to rad/s
			this->acc[i] *= M_PI/180;                                                           // Convert to rad/s/s
			
			// Ensure values are always within limits to avoid problems
			// with joint limit avoidance functions
			
			     if(this->pos[i] < this->pLim[i][0]) this->pos[i] = this->pLim[i][0];
			else if(this->pos[i] > this->pLim[i][1]) this->pos[i] = this->pLim[i][1];
		}
		
		if(not success) std::cerr << "[ERROR] [JOINT INTERFACE] read_encoders(): "
					  << "Could not obtain new encoder values." << std::endl;
		return success;
	}
	else
	{
		std::cerr << "[ERROR] [JOINT INTERFACE] read_encoders(): "
		          << "There was a problem during the construction of this object." << std::endl;
		
		return false;
	}
}


  ////////////////////////////////////////////////////////////////////////////////////////////////////
 //                              Send a velocity command to a joint motor                          //
////////////////////////////////////////////////////////////////////////////////////////////////////
bool JointInterface::send_velocity_command(const double &command, const int &i)
{
	if(i >= this->n)
	{
		std::cerr << "[ERROR] [JOINT INTERFACE] send_velocity_command(): "
		          << "There are only " << this->n << " joints, but your argument was for "
		          << " joint " << i+1 << "." << std::endl;
		
		return false;
	}
	else
	{
		this->controller->velocityMove(i,command*180/M_PI);                                 // Be sure to convert to deg/s!
		return true;
	}
}

  ////////////////////////////////////////////////////////////////////////////////////////////////////
 //                                    Send commands to the joints                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////
bool JointInterface::send_velocity_commands(const std::vector<double> &commands)
{
	if(not this->isValid)
	{
		std::cerr << "[ERROR] [JOINT INTERFACE] send_joint_commands(): "
		          << "There was a problem during the construction of this object. "
		          << "Joint commands cannot be sent." << std::endl;
		          
		return false;
	}
	else if(commands.size() != this->n)
	{
		std::cerr << "[ERROR] [JOINT INTERFACE] send_joint_commands(): "
		          << "The input vector had " << commands.size() << " elements, "
		          << "but this robot has " << this->n << " joints!" << std::endl;
		          
		return false;
	}
	else
	{
		for(int i = 0; i < this->n; i++) this->controller->velocityMove(i,commands[i]*180/M_PI);
	
		return true;
	}
}

  ////////////////////////////////////////////////////////////////////////////////////////////////////
 //                             Close the device interfaces on the robot                           //
////////////////////////////////////////////////////////////////////////////////////////////////////
void JointInterface::close()
{
	if(this->isValid)
	{
		// Put it in to position mode to lock the joints
		for(int i = 0; i < this->n; i++) this->mode->setControlMode(i, VOCAB_CM_POSITION);
	}
	this->driver.close();
}

#endif
