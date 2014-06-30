/*
 * dataManager.hpp
 *
 *  Created on: Jun 20, 2014
 *      Author: Filip Mandic
 */

#ifndef DATAMANAGER_HPP_
#define DATAMANAGER_HPP_

/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2014, LABUST, UNIZG-FER
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

#ifndef EVENTEVALUATION_HPP_
#define EVENTEVALUATION_HPP_

#include <labust_mission/labustMission.hpp>

/*********************************************************************
 ***  DataManager class definition
 ********************************************************************/

namespace labust {
	namespace data {

		class DataManager{

		public:

			/*****************************************************************
			 ***  Class functions
			 ****************************************************************/

			DataManager();

			void updateData(const auv_msgs::NavSts::ConstPtr& data);

			/*********************************************************************
			 ***  Class variables
			 ********************************************************************/

			/* State Hat variables */
			enum {u=0, v, w, r, x, y, z, psi, x_var, y_var, z_var, psi_var, alt, stateHatNum};
			std::vector<double> stateHatVar;

			/* Events data variables */
			std::vector<double> eventsVar;

			/* Mission specific variables */ // Svaki specificni projekt ima klasu koja extenda DataManager klasu
			std::vector<double> missionVar;
		};

		DataManager::DataManager(){

			//ros::NodeHandle nh;
			stateHatVar.resize(stateHatNum);
		}

		void DataManager::updateData(const auv_msgs::NavSts::ConstPtr& data){

			stateHatVar[u] = data->body_velocity.x;
			stateHatVar[v] = data->body_velocity.y;
			stateHatVar[w] = data->body_velocity.z;
			stateHatVar[r] = data->orientation_rate.yaw;

			stateHatVar[x] = data->position.north;
			stateHatVar[y] = data->position.east;
			stateHatVar[z] = data->position.depth;
			stateHatVar[psi] = data->orientation.yaw;

			stateHatVar[x_var] = data->position_variance.north;
			stateHatVar[y_var] = data->position_variance.east;
			stateHatVar[z_var] = data->position_variance.depth;
			stateHatVar[psi_var] = data->orientation_variance.yaw;

			stateHatVar[alt] = data->altitude;

		}
	}
}

#endif /* DATAMANAGER_HPP_ */
