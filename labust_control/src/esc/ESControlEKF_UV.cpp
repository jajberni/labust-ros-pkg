/*********************************************************************
 * ESControlEKF_UV.cpp
 *
 *  Created on: Jan 8, 2015
 *      Author: Filip Mandic
 *
 ********************************************************************/

/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, LABUST, UNIZG-FER
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the LABUST nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include <labust/control/esc/EscEKF.hpp>
#include <labust/control/HLControl.hpp>
#include <labust/control/EnablePolicy.hpp>
#include <labust/control/WindupPolicy.hpp>
#include <labust/math/NumberManipulation.hpp>
#include <labust/tools/MatrixLoader.hpp>
#include <labust/tools/conversions.hpp>

#include <Eigen/Dense>
#include <auv_msgs/BodyForceReq.h>
#include <std_msgs/Float32.h>
#include <ros/ros.h>

namespace labust{
	namespace control{

		/*************************************************************
		 *** Extremum seeking controller with speed control inputs.
		 ************************************************************/

		struct ESControlEKF_UV : DisableAxis{

			enum {x = 0, y};

			ESControlEKF_UV():Ts(0.1), esc_controller(2,Ts),count(0){};

			void init(){

				ros::NodeHandle nh;
				initialize_controller();
			}

			void windup(const auv_msgs::BodyForceReq& tauAch){

			};

			void idle(const std_msgs::Float32& ref, const auv_msgs::NavSts& state,
					const auv_msgs::BodyVelocityReq& track){

			};

			void reset(const std_msgs::Float32& ref, const auv_msgs::NavSts& state){

			};

			auv_msgs::BodyVelocityReqPtr step(const std_msgs::Float32& ref, const auv_msgs::NavSts& state){

				if((count++)%20 == 0){

					Eigen::Vector2d out, in;
					Eigen::Matrix2d R;

					Eigen::VectorXd input(2);
					input << state.position.north, state.position.east;

					in = esc_controller.step(ref.data, input);

					auv_msgs::BodyVelocityReqPtr nu(new auv_msgs::BodyVelocityReq());
					nu->header.stamp = ros::Time::now();
					nu->goal.requester = "esc_ekf_controller";
					labust::tools::vectorToDisableAxis(disable_axis, nu->disable_axis);

					double yaw = state.orientation.yaw;
					R<<cos(yaw),-sin(yaw),sin(yaw),cos(yaw);
					out = R.transpose()*in;

					nu->twist.linear.x = out[x];
					nu->twist.linear.y = out[y];

					nu_past = nu;
					return nu;
				}else{

					ROS_ERROR("PAST");
					return nu_past;
				}
			}

			void initialize_controller(){

				ROS_INFO("Initializing extremum seeking controller...");

				ros::NodeHandle nh;

				double sin_amp = 0.25;
				double	sin_freq = 0.09;
				double	corr_gain =  -0.075;
				double	high_pass_pole = 3;
				double	low_pass_pole = 0;
				double	comp_zero =  0;
				double	comp_pole = 0;
				double sampling_time = 0.1;

				std::vector<double> Q, R;
				Q.assign(3,1);
				R.assign(3,1);

				nh.param("esc_ekf/sin_amp", sin_amp, sin_amp);
				nh.param("esc_ekf/sin_freq", sin_freq, sin_freq);
				nh.param("esc_ekf/corr_gain", corr_gain, corr_gain);
				nh.param("esc_ekf/high_pass_pole", high_pass_pole, high_pass_pole);
				nh.param("esc_ekf/low_pass_pole", low_pass_pole, low_pass_pole);
				nh.param("esc_ekf/comp_zero", comp_zero, comp_zero);
				nh.param("esc_ekf/comp_pole", comp_pole, comp_pole);
				nh.param("esc_ekf/sampling_time", sampling_time, sampling_time);
				nh.param("esc_ekf/Q", Q, Q);
				nh.param("esc_ekf/R", R, R);

				esc::EscEkfGrad::vector Q0(3);
				Q0 << Q[0],Q[1],Q[2];
				esc::EscEkfGrad::vector R0(3);
				R0 << R[0],R[1],R[2];

				esc_controller.initController(sin_amp, sin_freq, corr_gain, high_pass_pole, low_pass_pole, comp_zero, comp_pole, sampling_time, Q0, R0);

				disable_axis[x] = 0;
				disable_axis[y] = 0;

				ROS_INFO("Extremum seeking controller initialized.");
			}

		private:

			double Ts;
			esc::EscEkfGrad esc_controller;
			auv_msgs::BodyVelocityReqPtr nu_past;
			int count;

		};
	}
}

int main(int argc, char* argv[])
{
	ros::init(argc,argv,"esc_classic_control");

	/***
	template <
			class Controller,
			class Enable = NoEnable,
			class Windup = NoWindup,
			class OutputType = auv_msgs::BodyVelocityReq,
			class InputType = auv_msgs::NavSts,
			class ReferenceType = auv_msgs::NavSts
			>
	***/

	labust::control::HLControl<labust::control::ESControlEKF_UV,
	labust::control::EnableServicePolicy,
	labust::control::NoWindup,
	auv_msgs::BodyVelocityReq,
	auv_msgs::NavSts,
	std_msgs::Float32> controller;
	ros::spin();

	return 0;
}



