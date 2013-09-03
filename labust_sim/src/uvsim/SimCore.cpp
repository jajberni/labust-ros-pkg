/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2010, LABUST, UNIZG-FER
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
 *
 *  Author: Dula Nad
 *  Created: 01.02.2013.
 *********************************************************************/
#include <labust/ros/SimCore.hpp>
#include <labust/tools/DynamicsLoader.hpp>

#include <sstream>

using namespace labust::simulation;

void labust::simulation::configureModel(const ros::NodeHandle& nh, RBModel& model)
{
	labust::tools::loadDynamicsParams(nh, model);

	nh.param("sampling_time",model.dT, model.dT);
	nh.param("coupled",model.isCoupled,model.isCoupled);
	Eigen::Vector3d bdg;
	labust::tools::getMatrixParam(nh,"bounding_ellipsoid",bdg);
	model.ae = bdg(0);
	model.be = bdg(1);
	model.ce = bdg(2);
	labust::tools::getMatrixParam(nh,"eta0",model.eta0);
	labust::tools::getMatrixParam(nh,"nu0",model.nu0);
	labust::tools::getMatrixParam(nh,"current",model.current);

	typedef Eigen::Matrix<double,6,1> NoiseVec;
	NoiseVec pn(NoiseVec::Zero()),mn(NoiseVec::Zero());
	labust::tools::getMatrixParam(nh,"process_noise",pn);
	labust::tools::getMatrixParam(nh,"measurement_noise",mn);
	model.noise.setNoiseParams(pn,mn);

	//Populate allocation
	int alloc_type(-1);
	Eigen::MatrixXd alloc;
	Eigen::MatrixXi dofs, groups;
	nh.param("allocation_type",alloc_type,-1);
  labust::tools::getMatrixParam(nh,"allocation_matrix",alloc);
	labust::tools::getMatrixParam(nh,"allocation_dofs",dofs);
	labust::tools::getMatrixParam(nh,"allocation_thruster_groups",groups);

	model.allocator.init(alloc_type, alloc, dofs, groups);
	model.init();
}

SimCore::SimCore():
		tau(vector::Zero()),
		rate(10),
		wrap(1)
{
	this->onInit();
}

void SimCore::onInit()
{
	ros::NodeHandle nh,ph("~");
	configureModel(nh, model);
	modelReport();

	tauIn = nh.subscribe<auv_msgs::BodyForceReq>("tauIn", 1, &SimCore::onTau<auv_msgs::BodyForceReq>, this);

	tauInWrench = nh.subscribe<geometry_msgs::WrenchStamped>("tauInWrench", 1, &SimCore::onTau<geometry_msgs::WrenchStamped>, this);

	currentsSub = nh.subscribe<geometry_msgs::TwistStamped>("currents", 1, &SimCore::onCurrents, this);

	//Publishers
	meas = nh.advertise<auv_msgs::NavSts>("meas_ideal",1);
	measn = nh.advertise<auv_msgs::NavSts>("meas_noisy",1);
	tauAch = nh.advertise<auv_msgs::BodyForceReq>("tauAch",1);

	odom = nh.advertise<nav_msgs::Odometry>("meas_odom",1);
	odomn = nh.advertise<nav_msgs::Odometry>("meas_odom_noisy",1);
    tauAchWrench = nh.advertise<geometry_msgs::WrenchStamped>("tauAchWrench",1);

	double fs(10);
	ph.param("Rate",fs,fs);
	ph.param("ModelWrap",wrap,wrap);
	model.dT = 1/(fs*wrap);
	rate = ros::Rate(fs);

	runner = boost::thread(boost::bind(&SimCore::start, this));
}

void SimCore::onCurrents(const geometry_msgs::TwistStamped::ConstPtr& currents)
{
	boost::mutex::scoped_lock l(model_mux);
	labust::tools::pointToVector(currents->twist.linear, model.current);
}

void SimCore::etaNuToNavSts(const vector& eta, const vector& nu, auv_msgs::NavSts& state)
{
	state.position.north = eta(RBModel::x);
	state.position.east = eta(RBModel::y);
	state.position.depth = eta(RBModel::z);
	state.orientation.roll = eta(RBModel::phi);
	state.orientation.pitch = eta(RBModel::theta);
	state.orientation.yaw = eta(RBModel::psi);

	state.body_velocity.x = nu(RBModel::u);
	state.body_velocity.y = nu(RBModel::v);
	state.body_velocity.z = nu(RBModel::w);
	state.orientation_rate.roll = nu(RBModel::p);
	state.orientation_rate.pitch = nu(RBModel::q);
	state.orientation_rate.yaw = nu(RBModel::r);
}

void SimCore::publishNavSts()
{
	auv_msgs::NavStsPtr state(new auv_msgs::NavSts()),
			nstate(new auv_msgs::NavSts());

	etaNuToNavSts(model.Eta(),model.Nu(),*state);
	etaNuToNavSts(model.EtaNoisy(),model.NuNoisy(),*nstate);

	//Handle LAT-LON additions and frame publishing

	//Publish messages
	meas.publish(state);
	measn.publish(nstate);
}

void SimCore::publishOdom()
{
	nav_msgs::OdometryPtr state(new nav_msgs::Odometry()),
			nstate(new nav_msgs::Odometry());

	etaNuToOdom(model.Eta(),model.Nu(),*state);
	etaNuToOdom(model.EtaNoisy(),model.NuNoisy(),*nstate);

	//Handle LAT-LON additions and frame publishing
	//Handle covariance additions

	//Publish messages
	odom.publish(state);
	odomn.publish(nstate);
}

void SimCore::etaNuToOdom(const vector& eta, const vector& nu, nav_msgs::Odometry& state)
{
	labust::tools::vectorToPoint(eta,state.pose.pose.position);

	Eigen::Quaternion<double> quat;
	labust::tools::quaternionFromEulerZYX(eta(RBModel::phi),
			eta(RBModel::theta),
			eta(RBModel::psi), quat);
	state.pose.pose.orientation.x = quat.x();
	state.pose.pose.orientation.y = quat.y();
	state.pose.pose.orientation.z = quat.z();
	state.pose.pose.orientation.w = quat.w();

	labust::tools::vectorToPoint(nu,state.twist.twist.linear);
	labust::tools::vectorToPoint(nu,state.twist.twist.angular,3);
}

void SimCore::start()
{
	SimSensorInterface::Hook hook(model,broadcast,listener);

	while (ros::ok())
	{
		boost::mutex::scoped_lock model_lock(model_mux);
		{
			boost::mutex::scoped_lock l(tau_mux);
			for (size_t i=0; i<wrap;++i) model.step(tau);
		}

		{
			boost::mutex::scoped_lock l(sensor_mux);
			for (std::vector<SimSensorInterface::Ptr>::iterator it=sensors.begin();
					it != sensors.end(); ++it)
			{
				(*it)->step(hook);
			}
		}

		//Publish states
		publishNavSts();
		publishOdom();

		model_lock.unlock();
		rate.sleep();
	}
		/*while (ros::ok())
			{

				using namespace labust::simulation;
				Eigen::Vector3f tauXYN,tauXYNsc;
				tauXYN<<tau(RBModel::X),tau(RBModel::Y),tau(RBModel::N);
				double scale = allocator.scale(tauXYN,&tauXYNsc);

		//		tau(RBModel::X) = labust::math::coerce(tau(RBModel::X), minThrust, maxThrust);
		//		tau(RBModel::N) = labust::math::coerce(tau(RBModel::N), minThrust, maxThrust);
		//
		//		//Differential allocation
		//		double t1 = (tau(RBModel::X) + tau(RBModel::N))/2;
		//		double t2 = (tau(RBModel::X) - tau(RBModel::N))/2;
		//
		//		t1 = labust::math::coerce(t1, minThrust, maxThrust);
		//		t2 = labust::math::coerce(t2, minThrust, maxThrust);

				auv_msgs::BodyForceReq t;
				//tau(RBModel::X) = t.wrench.force.x = t1+t2;
				tau(RBModel::X) = t.wrench.force.x = tauXYNsc(0);
				tau(RBModel::Y) = t.wrench.force.y = tauXYNsc(1);
				t.wrench.force.z = tau(RBModel::Z);
				t.wrench.torque.x = tau(RBModel::K);
				t.wrench.torque.y = tau(RBModel::M);
				tau(RBModel::N) = t.wrench.torque.z = tauXYNsc(2);
				//tau(RBModel::N) = t.wrench.torque.z = t1-t2;
				t.header.stamp = ros::Time::now();

				//t.disable_axis.x = tau(RBModel::X) != tauXYN(0);
				//t.disable_axis.yaw = tau(RBModel::N) != tauXYN(2);

				//scale = 1;

				//Publish the scaled values if scaling occured
				if (scale>1)
				{
					//Signal windup occured
					t.disable_axis.x = t.disable_axis.y = t.disable_axis.yaw = 1;
				}

				tauAch.publish(t);

				model.setCurrent(current);
				//Perform simulation with smaller sampling type if wrap>1
				for (size_t i=0; i<wrap;++i) model.step(tau);

				//uwsim.publish(*mapToUWSimOdometry(model.Eta(),model.Nu(),&odom, lisWorld));
				state.publish(*mapToNavSts(model.Eta(),model.Nu(),&nav));
				stateNoisy.publish(*mapToNavSts(model.EtaNoisy(),model.NuNoisy(),&navNoisy));

				const vector& Nu = (useNoisy?model.NuNoisy():model.Nu());
				const vector& Eta = (useNoisy?model.EtaNoisy():model.Eta());
				if ((ros::Time::now()-lastGps).sec >= gpsTime)
				{
					mapToNavSatFix(Eta,Nu,&fix,utmzone,lisWorld,localFrame);
					if (fix.altitude >= 0)
					{
						gpsFix.publish(fix);
					}
					lastGps = ros::Time::now();
				}

				imuMeas.publish(*mapToImu(Eta,Nu,model.NuAcc(),&imu,localFrame));
				dvlMeas.publish(*mapToDvl(Eta,Nu,&dvl,localFrame));
				depth.fluid_pressure = model.getPressure(Eta(RBModel::z));
				depth.header.frame_id = "local";
				pressureMeas.publish(depth);

				tf::Transform transform;
				transform.setOrigin(tf::Vector3(originLon, originLat, 0));
				transform.setRotation(tf::createQuaternionFromRPY(0,0,0));
				Eigen::Quaternion<float> q;
				labust::tools::quaternionFromEulerZYX(M_PI,0,M_PI/2,q);
				if (publishWorld)
				{
					localFrame.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "/worldLatLon", "/world"));
					transform.setOrigin(tf::Vector3(0, 0, 0));
					transform.setRotation(tf::Quaternion(q.x(),q.y(),q.z(),q.w()));
					localFrame.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "/world", "local"));
				}

				tf::Transform transform3;
				transform3.setOrigin(tf::Vector3(0, 0, 0));
				q = q.conjugate();
				transform3.setRotation(tf::Quaternion(q.x(),q.y(),q.z(),q.w()));
				localFrame.sendTransform(tf::StampedTransform(transform3, ros::Time::now(), "local", "uwsim_frame"));

				const vector& eta = model.Eta();
				//tf::Transform transform;
				transform.setOrigin(tf::Vector3(eta(RBModel::x),
						eta(RBModel::y),
						eta(RBModel::z)));
				labust::tools::quaternionFromEulerZYX(eta(RBModel::phi),
						eta(RBModel::theta),
						eta(RBModel::psi), q);
				transform.setRotation(tf::Quaternion(q.x(),q.y(),q.z(),q.w()));
				localFrame.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "local", "base_link_sim"));

				tf::Transform transform2;
				transform2.setOrigin(tf::Vector3(0, 0, -0.25));
				transform2.setRotation(tf::createQuaternionFromRPY(0,0,0));
				localFrame.sendTransform(tf::StampedTransform(transform2, ros::Time::now(), "base_link", "gps_frame"));

				ros::spinOnce();
	}
	*/
}

void SimCore::modelReport()
{
	ROS_INFO("Loaded the model:");
	ROS_INFO(" sampling-time: %f, mass: %f, gravity: %f, density: %f",
			model.dT, model.m, model.g_acc, model.rho);
	std::ostringstream str;
	str<<"["<<model.eta0.transpose()<<"], ["<<model.nu0.transpose()<<"]"<<"\n";
	ROS_INFO(" (Eta0,Nu0): %s",str.str().c_str());
	str.str("");
	str<<"["<<model.current.transpose()<<"]";
	ROS_INFO(" Current: %s",str.str().c_str());
	ROS_INFO(" Bounding ellipsoid: (%f,%f,%f)",model.ae, model.be, model.ce);
	ROS_INFO(" Is coupled model: %d",model.isCoupled);
	str.str("");
	str<<model.Io<<"\n"<<model.Ma<<"\n"<<model.Dlin<<"\n"<<model.Dquad;
	ROS_INFO("(Io,Ma,Dlin,Dquad):\n%s",str.str().c_str());
}
