/* Portions Copyright 2019-2021 Xuesong Zhou and Peiheng Li, Cafer Avci

 * If you help write or modify the code, please also list your names here.
 * The reason of having Copyright info here is to ensure all the modified version, as a whole, under the GPL
 * and further prevent a violation of the GPL.
 *
 * More about "How to use GNU licenses for your own software"
 * http://www.gnu.org/licenses/gpl-howto.html
 */

 // Peiheng, 02/03/21, remove them later after adopting better casting
#pragma warning(disable : 4305 4267 4018)
// stop warning: "conversion from 'int' to 'float', possible loss of data"
#pragma warning(disable: 4244)

#ifdef _WIN32
#include "pch.h"
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <functional>
#include <stack>
#include <list>
#include <vector>
#include <map>
#include <omp.h>
#include "config.h"
#include "utils.h"


using std::max;
using std::min;
using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::map;
using std::ifstream;
using std::ofstream;
using std::istringstream;

#include "DTA.h"

void g_record_corridor_performance_summary(Assignment& assignment, int base_case_flag)
{
	for (int i = 0; i < g_link_vector.size(); ++i)
	{
		// virtual connectors
		if (g_link_vector[i].link_type == -1)
			continue;


		for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
		{

			if (assignment.g_DemandPeriodVector[tau].number_of_demand_files == 0)
				continue;

			float speed = g_link_vector[i].free_speed;  // default speed 
			if (g_link_vector[i].VDF_period[tau].avg_travel_time > 0.001f)
				speed = g_link_vector[i].free_speed / max(0.000001, g_link_vector[i].VDF_period[tau].avg_travel_time) * g_link_vector[i].VDF_period[tau].FFTT;

			float speed_ratio = speed / max(1.0, g_link_vector[i].free_speed);  // default speed 
			float vehicle_volume = g_link_vector[i].PCE_volume_per_period[tau] + g_link_vector[i].VDF_period[tau].preload;
			float person_volume = g_link_vector[i].person_volume_per_period[tau] + g_link_vector[i].VDF_period[tau].preload;
			//VMT,VHT,PMT,PHT,PDT
			float preload = g_link_vector[i].VDF_period[tau].preload;
			float VMT = vehicle_volume * g_link_vector[i].link_distance_VDF;

			float VHT = vehicle_volume * g_link_vector[i].VDF_period[tau].avg_travel_time / 60.0;
			float PMT = person_volume * g_link_vector[i].link_distance_VDF;
			float PHT = person_volume * g_link_vector[i].VDF_period[tau].avg_travel_time / 60.0;
			float PDT_vf = person_volume * (g_link_vector[i].VDF_period[tau].avg_travel_time - g_link_vector[i].VDF_period[tau].FFTT) / 60.0;
			float PDT_vc = max(0.0, person_volume * (g_link_vector[i].VDF_period[tau].avg_travel_time - g_link_vector[i].VDF_period[tau].FFTT * g_link_vector[i].free_speed / max(0.001f, g_link_vector[i].v_congestion_cutoff)) / 60.0);

			if (g_link_vector[i].tmc_corridor_name.size() > 0 || g_link_vector[i].VDF_period[tau].network_design_flag != 0)
			{  // with corridor name
				CPeriod_Corridor l_element;
				l_element.volume = vehicle_volume;
				l_element.speed = speed;
				l_element.DoC = g_link_vector[i].VDF_period[tau].DOC;
				l_element.P = g_link_vector[i].VDF_period[tau].P;

				string key = g_link_vector[i].tmc_corridor_name;
				if (g_link_vector[i].VDF_period[tau].network_design_flag != 0)
				{
					int idebug = 1;
				}
				if (base_case_flag == 0)
				{  // base case
					g_corridor_info_base0_map[key].record_link_2_corridor_data(l_element, tau);
					if (g_link_vector[i].VDF_period[tau].network_design_flag != 0)
					{
						key = g_link_vector[i].link_id;
						g_corridor_info_base0_map[key].record_link_2_corridor_data(l_element, tau);
					}
				}
				else  /// SA case
				{
					g_corridor_info_SA_map[key].record_link_2_corridor_data(l_element, tau);

					if (g_link_vector[i].VDF_period[tau].network_design_flag != 0)
					{
						key = g_link_vector[i].link_id;
						g_corridor_info_SA_map[key].record_link_2_corridor_data(l_element, tau);
					}
				}


			}


		}
	}
}

void g_OutputSummaryFiles(Assignment& assignment)
{
	// if 
	assignment.summary_corridor_file << "tmc_corridor_name,demand_period,links,vol0,speed0,DoC0,max_P0,vol,speed,Doc,P,diff_vol,diff_spd,diff_Doc,diff_P,d%_vol,d%_spd," << endl;

	std::map<string, CCorridorInfo>::iterator it;
	for (it = g_corridor_info_base0_map.begin(); it != g_corridor_info_base0_map.end(); ++it)
	{
		assignment.summary_corridor_file << it->first.c_str() << ",";
		for (int tau = 0; tau < assignment.g_DemandPeriodVector.size(); tau++)
		{
			if (assignment.g_DemandPeriodVector[tau].number_of_demand_files >= 1)
			{
				it->second.computer_avg_value(tau);

				assignment.summary_corridor_file <<
					assignment.g_DemandPeriodVector[tau].demand_period.c_str() << "," <<
					it->second.corridor_period[tau].count << "," <<
					it->second.corridor_period[tau].volume << "," <<
					it->second.corridor_period[tau].speed << "," <<
					it->second.corridor_period[tau].DoC << "," <<
					it->second.corridor_period[tau].P << ",";


				if (g_corridor_info_SA_map.find(it->first) != g_corridor_info_SA_map.end())
				{
					g_corridor_info_SA_map[it->first].computer_avg_value(tau);

					CCorridorInfo corridor1 = g_corridor_info_SA_map[it->first];

					assignment.summary_corridor_file <<
						corridor1.corridor_period[tau].volume << "," <<
						corridor1.corridor_period[tau].speed << "," <<
						corridor1.corridor_period[tau].DoC << "," <<
						corridor1.corridor_period[tau].P << "," <<
						corridor1.corridor_period[tau].volume - it->second.corridor_period[tau].volume << "," <<
						corridor1.corridor_period[tau].speed - it->second.corridor_period[tau].speed << "," <<
						corridor1.corridor_period[tau].DoC - it->second.corridor_period[tau].DoC << "," <<
						corridor1.corridor_period[tau].P - it->second.corridor_period[tau].P << "," <<
						(corridor1.corridor_period[tau].volume - it->second.corridor_period[tau].volume) / max(1.0, it->second.corridor_period[tau].volume) * 100.0 << "," <<
						(corridor1.corridor_period[tau].speed - it->second.corridor_period[tau].speed) / max(1.0, it->second.corridor_period[tau].speed) * 100.0 << ",";

				}

				assignment.summary_corridor_file << endl;
			}
		}
	}
}

// FILE* g_pFileOutputLog = nullptr;

void g_output_dynamic_queue_profile()  // generated from VDF, from numerical queue evolution calculation
{
	dtalog.output() << "writing link_queue_profile.csv.." << endl;

	int b_debug_detail_flag = 0;
	FILE* g_pFileLinkMOE = nullptr;

	string file_name = "link_queue_profile.csv";

	fopen_ss(&g_pFileLinkMOE, file_name.c_str(), "w");

	if (!g_pFileLinkMOE)
	{
		dtalog.output() << "File " << file_name.c_str() << " cannot be opened." << endl;
		g_program_stop();
	}
	else
	{

		// Option 2: BPR-X function
		fprintf(g_pFileLinkMOE, "link_id,tmc_corridor_name,tmc_road_sequence,tmc,link_type_name,from_node_id,to_node_id,geometry,");

		fprintf(g_pFileLinkMOE, "link_type_code,FT,AT,nlanes,link_distance_VDF,free_speed,capacity,k_critical,v_congestion_cutoff,");
		for (int tau = 0; tau < assignment.g_DemandPeriodVector.size(); tau++)
		{
			fprintf(g_pFileLinkMOE, "%s_Volume,%s_speed_BPR,%s_speed_QVDF, %s_t0,%s_t3,%s_P, %s_D,%s_DC_ratio,%s_mu,%s_gamma,",
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
				assignment.g_DemandPeriodVector[tau].demand_period.c_str()

			);
		}

		fprintf(g_pFileLinkMOE, "");

		// hourly data
		for (int t = 6 * 60; t < 20 * 60; t += 60)
		{
			int hour = t / 60;
			int minute = t - hour * 60;

			fprintf(g_pFileLinkMOE, "vh%02d,", hour);
		}

		for (int t = 6 * 60; t < 20 * 60; t += 60)
		{
			int hour = t / 60;
			int minute = t - hour * 60;

			fprintf(g_pFileLinkMOE, "vrh%02d,", hour);
		}

		for (int t = 6 * 60; t < 20 * 60; t += 15)
		{
			int hour = t / 60;
			int minute = t - hour * 60;
			fprintf(g_pFileLinkMOE, "v%02d:%02d,", hour, minute);
		}

		for (int t = 6 * 60; t < 20 * 60; t += 15)
		{
			int hour = t / 60;
			int minute = t - hour * 60;
			fprintf(g_pFileLinkMOE, "vr%02d:%02d,", hour, minute);
		}

		//for (int t = 6 * 60; t < 20 * 60; t += 5)
		//{
		//    int hour = t / 60;
		//    int minute = t - hour * 60;
		//    fprintf(g_pFileLinkMOE, "v%02d:%02d,", hour, minute);
		//}


		fprintf(g_pFileLinkMOE, "\n");

		//Initialization for all nodes
		for (int i = 0; i < g_link_vector.size(); ++i)
		{
			// virtual connectors
			if (g_link_vector[i].link_type == -1)
				continue;

			double vehicle_volume_0 = 0;
			for (int tau = 0; tau < min((size_t)3, assignment.g_DemandPeriodVector.size()); tau++)
			{

				vehicle_volume_0 += g_link_vector[i].PCE_volume_per_period[tau] + g_link_vector[i].VDF_period[tau].preload + g_link_vector[i].VDF_period[tau].sa_volume;
			}

			if (vehicle_volume_0 < 1)  // no volume links, skip
				continue;


			fprintf(g_pFileLinkMOE, "%s,%s,%d,%s,%s,%d,%d,\"%s\",",
				g_link_vector[i].link_id.c_str(),
				g_link_vector[i].tmc_corridor_name.c_str(),
				g_link_vector[i].tmc_road_sequence,
				g_link_vector[i].tmc_code.c_str(),

				g_link_vector[i].link_type_name.c_str(),

				g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
				g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
				g_link_vector[i].geometry.c_str());


			fprintf(g_pFileLinkMOE, "%s,%d,%d,%d,%f,%f,%f,%f,%f,",
				g_link_vector[i].link_type_code.c_str(),
				g_link_vector[i].FT,
				g_link_vector[i].AT,
				g_link_vector[i].number_of_lanes,
				g_link_vector[i].link_distance_VDF,
				g_link_vector[i].free_speed,
				g_link_vector[i].lane_capacity,
				g_link_vector[i].k_critical,
				g_link_vector[i].v_congestion_cutoff);

			//AM g_link_vector[i].VDF_period[0].

			int period_index = 0;

			double assignment_PMT;
			double assignment_PHT;
			double assignment_PSDT;
			double assignment_VCDT = 0;

			double assignment_VMT;
			double assignment_VHT;

			for (int tau = 0; tau < min((size_t)3, assignment.g_DemandPeriodVector.size()); tau++)
			{
				double vehicle_volume = g_link_vector[i].PCE_volume_per_period[period_index] + g_link_vector[i].VDF_period[period_index].preload + g_link_vector[i].VDF_period[period_index].sa_volume;
				double person_volume = g_link_vector[i].person_volume_per_period[period_index] + g_link_vector[i].VDF_period[period_index].preload + g_link_vector[i].VDF_period[period_index].sa_volume;

				assignment_VMT = g_link_vector[i].link_distance_VDF * vehicle_volume;
				assignment_VHT = g_link_vector[i].travel_time_per_period[period_index] * vehicle_volume / 60.0;  // 60.0 converts min to hour

				assignment_PMT = g_link_vector[i].link_distance_VDF * person_volume;
				assignment_PHT = g_link_vector[i].travel_time_per_period[period_index] * person_volume / 60.0;  // 60.0 converts min to hour

				assignment_PSDT = (g_link_vector[i].travel_time_per_period[period_index] - g_link_vector[i].free_flow_travel_time_in_min) * person_volume / 60.0;  // 60.0 converts min to hour

				double VCTT = g_link_vector[i].link_distance_VDF / max(1.0f, g_link_vector[i].v_congestion_cutoff) * 60;
				assignment_VCDT = max(0.0, g_link_vector[i].travel_time_per_period[period_index] - VCTT) * g_link_vector[i].PCE_volume_per_period[period_index] / 60.0;  // 60.0 converts min to hour


				fprintf(g_pFileLinkMOE, "%f,%f,%f, %f,%f,%f, %f,%f,%f,%f,",
					g_link_vector[i].PCE_volume_per_period[period_index] + g_link_vector[i].VDF_period[period_index].preload + g_link_vector[i].VDF_period[period_index].sa_volume,
					g_link_vector[i].VDF_period[period_index].avg_speed_BPR,
					g_link_vector[i].VDF_period[period_index].avg_queue_speed,

					g_link_vector[i].VDF_period[period_index].t0,
					g_link_vector[i].VDF_period[period_index].t3,
					g_link_vector[i].VDF_period[period_index].P,

					g_link_vector[i].VDF_period[period_index].lane_based_D,
					g_link_vector[i].VDF_period[period_index].DOC,
					g_link_vector[i].VDF_period[period_index].Q_mu,
					g_link_vector[i].VDF_period[period_index].Q_gamma
				);
			}

			for (int t = 6 * 60; t < 20 * 60; t += 60)
			{
				float speed = g_link_vector[i].get_model_hourly_speed(t);
				fprintf(g_pFileLinkMOE, "%.3f,", speed);
			}

			for (int t = 6 * 60; t < 20 * 60; t += 60)
			{
				float speed_ratio = g_link_vector[i].get_model_hourly_speed(t) / max(1.0f, g_link_vector[i].v_congestion_cutoff);
				if (speed_ratio > 1)
					speed_ratio = 1;

				fprintf(g_pFileLinkMOE, "%.3f,", speed_ratio);
			}


			for (int t = 6 * 60; t < 20 * 60; t += 15)
			{
				int time_interval = t / 5;
				float speed = g_link_vector[i].model_speed[time_interval];
				fprintf(g_pFileLinkMOE, "%.3f,", speed);
			}

			for (int t = 6 * 60; t < 20 * 60; t += 15)
			{
				int time_interval = t / 5;
				float speed_ratio = g_link_vector[i].model_speed[time_interval] / max(1.0f, g_link_vector[i].v_congestion_cutoff);
				if (speed_ratio > 1)
					speed_ratio = 1;

				fprintf(g_pFileLinkMOE, "%.3f,", speed_ratio);
			}

			fprintf(g_pFileLinkMOE, "\n");

		}  // for each link l
		fclose(g_pFileLinkMOE);
	}//assignment mode 2 as simulation

}

void g_output_assignment_result(Assignment& assignment)
{
	g_record_corridor_performance_summary(assignment, 1);
	

	dtalog.output() << "writing link_performance.csv.." << endl;

	int b_debug_detail_flag = 0;
	FILE* g_pFileLinkMOE = nullptr;

	fopen_ss(&g_pFileLinkMOE, "link_performance.csv", "w");
	if (!g_pFileLinkMOE)
	{
		dtalog.output() << "File link_performance.csv cannot be opened." << endl;
		g_program_stop();
	}
	else
	{

		// Option 2: BPR-X function
		fprintf(g_pFileLinkMOE, "link_id,vdf_type,from_node_id,to_node_id,meso_link_id,meso_link_incoming_volume,tmc,tmc_corridor_name,tmc_corridor_id,tmc_road_order,tmc_road_sequence,subarea_id,link_type,link_type_code,vdf_code,time_period,volume,preload_volume,person_volume,travel_time,speed,speed_ratio,VOC,DOC,capacity,queue,total_simu_waiting_time_in_min,avg_simu_waiting_time_in_min,qdf,lanes,D,QVDF_cd,QVDF_n,P,severe_congestion_duration_in_h,vf,v_congestion_cutoff,QVDF_cp,QVDF_s,QVDF_v,vt2,VMT,VHT,PMT,PHT,PDT_vf,PDT_vc,geometry,");

		for (int at = 0; at < assignment.g_AgentTypeVector.size(); ++at)
			fprintf(g_pFileLinkMOE, "person_vol_%s,", assignment.g_AgentTypeVector[at].agent_type.c_str());


		fprintf(g_pFileLinkMOE, "obs_count,upper_bound_type,dev,");

		for (int t = 6 * 60; t < 20 * 60; t += 15)
		{
			int hour = t / 60;
			int minute = t - hour * 60;

			fprintf(g_pFileLinkMOE, "v%02d:%02d,", hour, minute);
		}

		for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_column_generation_iterations); iteration_number++)
			fprintf(g_pFileLinkMOE, "TT_%d,", iteration_number);

		for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_sensitivity_analysis_iterations); iteration_number++)
			fprintf(g_pFileLinkMOE, "SVol_%d,", iteration_number);


		fprintf(g_pFileLinkMOE, "notes\n");

		//Initialization for all nodes
		for (int i = 0; i < g_link_vector.size(); ++i)
		{
			// virtual connectors
			if (g_link_vector[i].link_type == -1)
				continue;

			string vdf_type_str;

			if (g_link_vector[i].vdf_type == bpr_vdf)
			{
				vdf_type_str = "bpr";
			}
			else
			{
				vdf_type_str = "qvdf";
			}

			for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
			{

				if (assignment.g_DemandPeriodVector[tau].number_of_demand_files == 0)
					continue;

				float speed = g_link_vector[i].free_speed;  // default speed 


				if (g_link_vector[i].VDF_period[tau].avg_travel_time > 0.001f)
					speed = g_link_vector[i].free_speed / max(0.000001, g_link_vector[i].VDF_period[tau].avg_travel_time) * g_link_vector[i].VDF_period[tau].FFTT;

				float speed_ratio = speed / max(1.0, g_link_vector[i].free_speed);  // default speed 
				float vehicle_volume = g_link_vector[i].PCE_volume_per_period[tau] + g_link_vector[i].VDF_period[tau].preload;
				float person_volume = g_link_vector[i].person_volume_per_period[tau] + g_link_vector[i].VDF_period[tau].preload;
				//VMT,VHT,PMT,PHT,PDT
				float preload = g_link_vector[i].VDF_period[tau].preload;
				float VMT = vehicle_volume * g_link_vector[i].link_distance_VDF;

				float VHT = vehicle_volume * g_link_vector[i].VDF_period[tau].avg_travel_time / 60.0;
				float PMT = person_volume * g_link_vector[i].link_distance_VDF;
				float PHT = person_volume * g_link_vector[i].VDF_period[tau].avg_travel_time / 60.0;
				float PDT_vf = person_volume * (g_link_vector[i].VDF_period[tau].avg_travel_time - g_link_vector[i].VDF_period[tau].FFTT) / 60.0;
				float PDT_vc = max(0.0, person_volume * (g_link_vector[i].VDF_period[tau].avg_travel_time - g_link_vector[i].VDF_period[tau].FFTT * g_link_vector[i].free_speed / max(0.001f, g_link_vector[i].v_congestion_cutoff)) / 60.0);

				fprintf(g_pFileLinkMOE, "%s,%s,%d,%d,%d,%d,%s,%s,%d,%d,%d,%d,%d,%s,%s,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,\"%s\",",
					g_link_vector[i].link_id.c_str(),
					vdf_type_str.c_str(),
					g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
					g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
					g_link_vector[i].meso_link_id,
					g_link_vector[i].total_simulated_meso_link_incoming_volume,
					g_link_vector[i].tmc_code.c_str(),
					g_link_vector[i].tmc_corridor_name.c_str(),
					g_link_vector[i].tmc_corridor_id,
					g_link_vector[i].tmc_road_order,
					g_link_vector[i].tmc_road_sequence,

					g_link_vector[i].subarea_id,
					g_link_vector[i].link_type,
					g_link_vector[i].link_type_code.c_str(),
					g_link_vector[i].vdf_code.c_str(),
					assignment.g_DemandPeriodVector[tau].time_period.c_str(),
					vehicle_volume,
					preload,
					person_volume,
					g_link_vector[i].VDF_period[tau].avg_travel_time,
					speed,  /* 60.0 is used to convert min to hour */
					speed_ratio,

					g_link_vector[i].VDF_period[tau].VOC,
					g_link_vector[i].VDF_period[tau].DOC,
					g_link_vector[i].VDF_period[tau].lane_based_ultimate_hourly_capacity,
					g_link_vector[i].VDF_period[tau].queue_length,
					max(0.0, g_link_vector[i].total_simulated_delay_in_min),
					max(0.0, g_link_vector[i].total_simulated_delay_in_min)/max(1.0f, vehicle_volume),
				
					g_link_vector[i].VDF_period[tau].queue_demand_factor,
					g_link_vector[i].VDF_period[tau].nlanes,
					g_link_vector[i].VDF_period[tau].lane_based_D,
					g_link_vector[i].VDF_period[tau].Q_cd,
					g_link_vector[i].VDF_period[tau].Q_n,
					g_link_vector[i].VDF_period[tau].P,
					g_link_vector[i].VDF_period[tau].Severe_Congestion_P,
					g_link_vector[i].VDF_period[tau].vf,
					g_link_vector[i].VDF_period[tau].v_congestion_cutoff,
					g_link_vector[i].VDF_period[tau].Q_cp,
					g_link_vector[i].VDF_period[tau].Q_s,
					g_link_vector[i].VDF_period[tau].avg_queue_speed,
					g_link_vector[i].VDF_period[tau].vt2,

					VMT, VHT, PMT, PHT, PDT_vf, PDT_vc,
					g_link_vector[i].geometry.c_str());

				for (int at = 0; at < assignment.g_AgentTypeVector.size(); ++at)
					fprintf(g_pFileLinkMOE, "%.1f,", g_link_vector[i].person_volume_per_period_per_at[tau][at]);

				if (g_link_vector[i].VDF_period[tau].obs_count >= 1) //ODME
					fprintf(g_pFileLinkMOE, "%.1f,%d,%.1f,", g_link_vector[i].VDF_period[tau].obs_count, g_link_vector[i].VDF_period[tau].upper_bound_flag, g_link_vector[i].VDF_period[tau].est_count_dev);
				else
					fprintf(g_pFileLinkMOE, ",,,");

				for (int t = 6 * 60; t < 20 * 60; t += 15)
				{
					float speed = g_link_vector[i].get_model_15_min_speed(t);
					fprintf(g_pFileLinkMOE, "%.3f,", speed);
				}

				for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_column_generation_iterations); iteration_number++)
				{
					float tt = -1;
					if (g_link_vector[i].VDF_period[tau].travel_time_per_iteration_map.find(iteration_number) != g_link_vector[i].VDF_period[tau].travel_time_per_iteration_map.end())
					{
						tt = g_link_vector[i].VDF_period[tau].travel_time_per_iteration_map[iteration_number];
					}
					else
					{
						tt = -1;
					}

					fprintf(g_pFileLinkMOE, "%.2f,", tt);
				}
				//SA
				float volume = 0;
				float pre_volume = 0;
				for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_sensitivity_analysis_iterations); iteration_number++)
				{

					if (g_link_vector[i].VDF_period[tau].link_volume_per_iteration_map.find(iteration_number) != g_link_vector[i].VDF_period[tau].link_volume_per_iteration_map.end())
					{
						volume = g_link_vector[i].VDF_period[tau].link_volume_per_iteration_map[iteration_number];
					}
					else
					{
						volume = -1;
					}

					if (iteration_number == 0)
					{
						fprintf(g_pFileLinkMOE, "%.2f,", volume);
					}
					else
					{
						fprintf(g_pFileLinkMOE, "%.2f,", volume - pre_volume);

					}

					pre_volume = volume;
				}

				fprintf(g_pFileLinkMOE, "period-based\n");
			}

		}
		fclose(g_pFileLinkMOE);
	}

	if (assignment.assignment_mode == 0 || assignment.path_output == 0)  //LUE
	{
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "route_assignment.csv", "w");
		fclose(g_pFilePathMOE);

	}
	else if (assignment.assignment_mode >= 1)  //UE mode, or ODME, DTA
	{
		dtalog.output() << "writing route_assignment.csv.." << endl;

		double path_time_vector[MAX_LINK_SIZE_IN_A_PATH];
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "route_assignment.csv", "w");

		if (!g_pFilePathMOE)
		{
			dtalog.output() << "File route_assignment.csv cannot be opened." << endl;
			g_program_stop();
		}

		fprintf(g_pFilePathMOE, "first_column,path_no,o_zone_id,d_zone_id,od_pair,within_OD_path_no,path_id,activity_zone_sequence,activity_agent_type_sequence,information_type,agent_type,demand_period,volume,simu_volume,subarea_flag,OD_impact_flag,");
		fprintf(g_pFilePathMOE, "path_network_design_flag,toll, #_of_nodes, travel_time, VDF_travel_time, VDF_travel_time_without_access_link,distance,distance_km,distance_mile,node_sequence,link_sequence, ");

		//// stage 1: column updating
		//for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_column_updating_iterations); iteration_number++)
		//{ 
		//    fprintf(g_pFilePathMOE, "TT_%d,", iteration_number);
		//}

		//for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_column_updating_iterations); iteration_number++)
		//{
		//    fprintf(g_pFilePathMOE, "Vol_%d,", iteration_number);
		//}

		//stage 2: ODME
		//for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_ODME_iterations); iteration_number++)
		//{
		//    fprintf(g_pFilePathMOE, "ODME_TT_%d,", iteration_number);
		//}

		//for (int iteration_number = max(0, assignment.g_number_of_ODME_iterations-10); iteration_number < assignment.g_number_of_ODME_iterations; iteration_number++)
		//{
		//    fprintf(g_pFilePathMOE, "ODME_Vol_%d,", iteration_number);
		//}

		//stage 3: sensitivity analysi
		//for (int iteration_number = 0; iteration_number < assignment.g_number_of_sensitivity_analysis_iterations; iteration_number++)
		//{
		//    fprintf(g_pFilePathMOE, "SA_TT_%d,", iteration_number);
		//}
		//for (int iteration_number = 0; iteration_number < assignment.g_number_of_sensitivity_analysis_iterations; iteration_number++)
		//{
		//    fprintf(g_pFilePathMOE, "SA_Vol_%d,", iteration_number);
		//}
		fprintf(g_pFilePathMOE, "geometry,");

		fprintf(g_pFilePathMOE, "link_type_name_sequence,link_code_sequence,link_link_distance_VDF_sequence,link_FFTT_sequence,");
		fprintf(g_pFilePathMOE, "\n");

		int count = 1;

		clock_t start_t, end_t;
		start_t = clock();
		clock_t iteration_t;


		int agent_type_size = assignment.g_AgentTypeVector.size();
		int zone_size = g_zone_vector.size();
		int demand_period_size = assignment.g_DemandPeriodVector.size();

		CColumnVector* p_column_pool;

		float path_toll = 0;
		double path_distance = 0;
		double path_distance_km = 0;
		double path_distance_ml = 0;
		float path_travel_time = 0;
		float path_travel_time_without_access_link = 0;
		float path_delay = 0;
		float path_FF_travel_time = 0;
		float time_stamp = 0;

		std::map<int, CColumnPath>::iterator it, it_begin, it_end;

		if (assignment.major_path_volume_threshold > 0.00001)  // performing screening of path flow pattern
		{
			//initialization 
			bool b_subarea_mode = false;

			int number_of_links = g_link_vector.size();
			for (int i = 0; i < number_of_links; ++i)
			{
				for (int tau = 0; tau < demand_period_size; ++tau)
				{
					// used in travel time calculation
					g_link_vector[i].background_PCE_volume_per_period[tau] = 0;
				}

				if (g_node_vector[g_link_vector[i].from_node_seq_no].subarea_id >= 1 && g_node_vector[g_link_vector[i].to_node_seq_no].node_id >= 1)
				{

					b_subarea_mode = true;
				}

			}


			/// <summary>  screening the path flow pattern
			for (int orig = 0; orig < zone_size; ++orig)
			{

				for (int at = 0; at < agent_type_size; ++at)
				{
					for (int dest = 0; dest < zone_size; ++dest)
					{
						for (int tau = 0; tau < demand_period_size; ++tau)
						{
							p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
							if (p_column_pool->od_volume > 0)
							{
								// scan through the map with different node sum for different continuous paths
								it_begin = p_column_pool->path_node_sequence_map.begin();
								it_end = p_column_pool->path_node_sequence_map.end();

								for (it = it_begin; it != it_end; ++it)
								{
									int subarea_output_flag = 0;
									if (b_subarea_mode == true)
									{
										int insubarea_flag = 0;

										for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
										{
											int link_seq_no = it->second.path_link_vector[nl];

											if (g_link_vector[link_seq_no].subarea_id >= 1)
											{
												insubarea_flag = 1;
												break;
											}

										}
										// 
										if (insubarea_flag && it->second.path_volume > assignment.major_path_volume_threshold)
										{
											subarea_output_flag = 1;
										}

									}
									else
									{
										if (it->second.path_volume > assignment.major_path_volume_threshold)
											subarea_output_flag = 1;

									}
									if (subarea_output_flag == 0)
									{
										it->second.subarea_output_flag = 0;  // disable the output of this column into route_assignment.csv

										for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
										{
											int link_seq_no = it->second.path_link_vector[nl];
											g_link_vector[link_seq_no].background_PCE_volume_per_period[tau] += it->second.path_volume;
										}
									}

								}
							}
						}
						/// </summary>
						/// <param name="assignment"></param>
					}

				}
			}

			/// output background_link_volume.csv
			dtalog.output() << "writing link_performance.csv.." << endl;

			int b_debug_detail_flag = 0;
			FILE* g_pFileLinkMOE = nullptr;
			int b_background_link_volume_file = 0;

			if (b_background_link_volume_file)
			{
				fopen_ss(&g_pFileLinkMOE, "link_background_volume.csv", "w");
				if (!g_pFileLinkMOE)
				{
					dtalog.output() << "File link_background_volume.csv cannot be opened." << endl;
					g_program_stop();
				}
				else
				{
					fprintf(g_pFileLinkMOE, "link_id,from_node_id,to_node_id,subarea_id,time_period,volume,background_volume,major_path_volume,ratio_of_major_path_flow,geometry,");

					fprintf(g_pFileLinkMOE, "notes\n");

					//Initialization for all nodes
					for (int i = 0; i < g_link_vector.size(); ++i)
					{
						//   virtual connectors
						if (g_link_vector[i].link_type == -1)
							continue;

						for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
						{
							double volume = g_link_vector[i].PCE_volume_per_period[tau] + g_link_vector[i].VDF_period[tau].preload;
							double major_path_link_volume = g_link_vector[i].PCE_volume_per_period[tau] + g_link_vector[i].VDF_period[tau].preload - g_link_vector[i].background_PCE_volume_per_period[tau];
							double ratio = major_path_link_volume / max(volume, 0.000001);

							if (volume < 0.0000001)
								ratio = -1;
							fprintf(g_pFileLinkMOE, "%s,%d,%d,%d,%s,%.3f,%.3f,%.3f,%.3f,\"%s\",",
								g_link_vector[i].link_id.c_str(),
								g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
								g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
								g_link_vector[i].subarea_id,
								assignment.g_DemandPeriodVector[tau].time_period.c_str(),
								g_link_vector[i].PCE_volume_per_period[tau] + g_link_vector[i].VDF_period[tau].preload,
								g_link_vector[i].background_PCE_volume_per_period[tau],
								major_path_link_volume,
								ratio,
								g_link_vector[i].geometry.c_str());
							fprintf(g_pFileLinkMOE, "\n");

						}

					}

					fclose(g_pFileLinkMOE);
				}

			}


		} // end of path flow pattern screening 
		dtalog.output() << "writing data for " << zone_size << "  zones " << endl;


		for (int orig = 0; orig < zone_size; ++orig)
		{
			if (g_zone_vector[orig].zone_id % 100 == 0)
				dtalog.output() << "o zone id =  " << g_zone_vector[orig].zone_id << endl;

			for (int at = 0; at < agent_type_size; ++at)
			{
				for (int dest = 0; dest < zone_size; ++dest)
				{
					for (int tau = 0; tau < demand_period_size; ++tau)
					{
						p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
						if (p_column_pool->od_volume > 0 ||
							assignment.zone_seq_no_2_activity_mapping.find(orig) != assignment.zone_seq_no_2_activity_mapping.end() ||
							assignment.zone_seq_no_2_info_mapping.find(dest) != assignment.zone_seq_no_2_info_mapping.end() ||
							(assignment.zone_seq_no_2_info_mapping.find(orig) != assignment.zone_seq_no_2_info_mapping.end()
								&& assignment.g_AgentTypeVector[at].real_time_information >= 1)
							)

						{

							if (g_zone_vector[orig].zone_id == 6 && g_zone_vector[dest].zone_id == 2)
							{
								int idebug = 1;
							}

							int information_type = p_column_pool->information_type;


							time_stamp = (assignment.g_DemandPeriodVector[tau].starting_time_slot_no + assignment.g_DemandPeriodVector[tau].ending_time_slot_no) / 2.0 * MIN_PER_TIMESLOT;

							// scan through the map with different node sum for different continuous paths
							it_begin = p_column_pool->path_node_sequence_map.begin();
							it_end = p_column_pool->path_node_sequence_map.end();

							for (it = it_begin; it != it_end; ++it)
							{

								if (count % 100000 == 0)
								{
									end_t = clock();
									iteration_t = end_t - start_t;
									dtalog.output() << "writing " << count / 1000 << "K agents with CPU time " << iteration_t / 1000.0 << " s" << endl;
								}

								path_toll = 0;
								path_distance = 0;
								path_distance_km = 0;
								path_distance_ml = 0;
								path_travel_time = 0;
								path_travel_time_without_access_link = 0;
								path_FF_travel_time = 0;

								path_time_vector[0] = time_stamp;
								path_time_vector[1] = time_stamp;

								string link_code_str;


								if (it->second.m_link_size >= MAX_LINK_SIZE_IN_A_PATH - 2)
								{
									dtalog.output() << "error: it->second.m_link_size < MAX_LINK_SIZE_IN_A_PATH" << endl;
									dtalog.output() << "o= " << g_zone_vector[orig].zone_id << endl;
									dtalog.output() << "d= " << g_zone_vector[dest].zone_id << endl;
									dtalog.output() << "agent type= " << assignment.g_AgentTypeVector[at].agent_type.c_str() << endl;

									for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
									{
										dtalog.output() << "node no." << nl << " =" << g_node_vector[it->second.path_node_vector[nl]].node_id << endl;
									}

									g_program_stop();
								}

								for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
								{

									int link_seq_no = it->second.path_link_vector[nl];
									if (g_link_vector[link_seq_no].link_type >= 0)
									{
										path_toll += g_link_vector[link_seq_no].VDF_period[tau].toll[at];
										path_distance += g_link_vector[link_seq_no].link_distance_VDF;
										path_distance_km += g_link_vector[link_seq_no].link_distance_km;
										path_distance_ml += g_link_vector[link_seq_no].link_distance_mile;
										float link_travel_time = g_link_vector[link_seq_no].travel_time_per_period[tau];
										path_travel_time += link_travel_time;

										if (g_link_vector[link_seq_no].link_type != 1000)   // skip access link
										{
											path_travel_time_without_access_link += link_travel_time;
										}

										path_FF_travel_time += g_link_vector[link_seq_no].VDF_period[tau].FFTT;
										time_stamp += link_travel_time;
										path_time_vector[nl + 1] = time_stamp;
										link_code_str += g_link_vector[link_seq_no].link_code_str;
									}
									else
									{
										int virtual_link = 0;
									}
								}

								double total_agent_path_travel_time = 0;

								for (int vi = 0; vi < it->second.agent_simu_id_vector.size(); ++vi)
								{
									int agent_simu_id = it->second.agent_simu_id_vector[vi];
									CAgent_Simu* pAgentSimu = g_agent_simu_vector[agent_simu_id];
									total_agent_path_travel_time += pAgentSimu->path_travel_time_in_min;
								}

								double final_path_travel_time = path_travel_time;  // by default

								if (it->second.agent_simu_id_vector.size() > 1)  // with simulated agents
								{
									final_path_travel_time = total_agent_path_travel_time / it->second.agent_simu_id_vector.size();
								}



								int virtual_first_link_delta = 1;
								int virtual_last_link_delta = 1;
								// fixed routes have physical nodes always, without virtual connectors
								if (p_column_pool->bfixed_route)
								{

									virtual_first_link_delta = 0;
									virtual_last_link_delta = 1;
								}
								if (information_type == 1 && it->second.path_volume < 0.1)
									continue;

								if (information_type == 1)  // information diversion start from physical nodes
								{
									virtual_first_link_delta = 0;
									virtual_last_link_delta = 1;

									it->second.path_volume = it->second.diverted_vehicle_map.size();
								}

								if (p_column_pool->activity_zone_no_vector.size() > 0)  // with activity zones 
								{
									virtual_first_link_delta = 0;
									virtual_last_link_delta = 0;
								}


								if (assignment.zone_seq_no_2_info_mapping.find(orig) != assignment.zone_seq_no_2_info_mapping.end()
									&& assignment.g_AgentTypeVector[at].real_time_information >= 1)
								{
									if (it->second.path_volume < 1)
										it->second.path_volume = 1;  // reset the volume as 1 to enable visualization 

								}

								// assignment_mode = 1, path flow mode
								if (assignment.assignment_mode == dta)
								{
									if (it->second.m_node_size - virtual_first_link_delta - virtual_last_link_delta <= 2)
										continue;

									fprintf(g_pFilePathMOE, ",%d,%d,%d,%d->%d,%d,%d,%s,%s,%d,%s,%s,%.2f,%d,%d,%d,%d,%.1f,%d,%.1f,%.4f,%.4f,%.4f,%.4f,%.4f,",
										count,
										g_zone_vector[orig].zone_id,
										g_zone_vector[dest].zone_id,
										g_zone_vector[orig].zone_id,
										g_zone_vector[dest].zone_id,
										it->second.path_seq_no,
										it->second.path_seq_no + 1,
										p_column_pool->activity_zone_sequence.c_str(),
										p_column_pool->activity_agent_type_sequence.c_str(),
										information_type,
										assignment.g_AgentTypeVector[at].agent_type.c_str(),
										assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
										it->second.path_volume,
										it->second.agent_simu_id_vector.size(),
										it->second.subarea_output_flag,
										p_column_pool->OD_network_design_flag,
										it->second.network_design_detour_mode,
										path_toll,
										it->second.m_node_size - virtual_first_link_delta - virtual_last_link_delta,
										final_path_travel_time,
										path_travel_time,
										path_travel_time_without_access_link,
										//path_FF_travel_time,
										//final_path_travel_time- path_FF_travel_time,
										path_distance,
										path_distance_km,
										path_distance_ml);

									/* Format and print various data */
									for (int ni = 0 + virtual_first_link_delta; ni < it->second.m_node_size - virtual_last_link_delta; ++ni)
										fprintf(g_pFilePathMOE, "%d;", g_node_vector[it->second.path_node_vector[ni]].node_id);

									fprintf(g_pFilePathMOE, ",");
									int link_seq_no;
									// link id sequence
									for (int nl = 0 + virtual_first_link_delta; nl < it->second.m_link_size - virtual_last_link_delta; ++nl)
									{
										link_seq_no = it->second.path_link_vector[nl];
										fprintf(g_pFilePathMOE, "%s;", g_link_vector[link_seq_no].link_id.c_str());
									}
									fprintf(g_pFilePathMOE, ",");


									//for (int nt = 0 + virtual_first_link_delta; nt < min(5000-1, it->second.m_node_size - virtual_last_link_delta); ++nt)
									//{
									//    if(path_time_vector[nt] < assignment.g_LoadingEndTimeInMin)
									//    {
									//    fprintf(g_pFilePathMOE, "%s;", g_time_coding(path_time_vector[nt]).c_str());
									//    }
									//}
									//fprintf(g_pFilePathMOE, ",");

									//for (int nt = 0 + virtual_first_link_delta; nt < it->second.m_link_size+1 - virtual_last_link_delta; ++nt)
									//    fprintf(g_pFilePathMOE, "%.2f;", path_time_vector[nt]);

									//fprintf(g_pFilePathMOE, ",");

									//for (int nt = 0 + virtual_first_link_delta; nt < it->second.m_link_size - virtual_last_link_delta; ++nt)
									//    fprintf(g_pFilePathMOE, "%.2f;", path_time_vector[nt+1]- path_time_vector[nt]);

									//fprintf(g_pFilePathMOE, ",");
									// output the TT and vol of column updating 

									// stage 1:
									//for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_column_updating_iterations); iteration_number++)
									//{
									//    double TT = -1;
									//    if (it->second.path_time_per_iteration_map.find(iteration_number) != it->second.path_time_per_iteration_map.end())
									//    {
									//        TT = it->second.path_time_per_iteration_map[iteration_number];
									//    }


									//    fprintf(g_pFilePathMOE, "%f,", TT);

									//}
									//for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_column_updating_iterations); iteration_number++)
									//{
									//    double Vol = 0;
									//    if (it->second.path_volume_per_iteration_map.find(iteration_number) != it->second.path_volume_per_iteration_map.end())
									//    {
									//        Vol = it->second.path_volume_per_iteration_map[iteration_number];
									//    }

									//    fprintf(g_pFilePathMOE, "%f,", Vol);

									//}
									// stage II: ODME
									//for (int iteration_number = 0; iteration_number < min(20, assignment.g_number_of_ODME_iterations); iteration_number++)
									//{
									//    double TT = -1;
									//    if (it->second.path_time_per_iteration_ODME_map.find(iteration_number) != it->second.path_time_per_iteration_ODME_map.end())
									//    {
									//        TT = it->second.path_time_per_iteration_ODME_map[iteration_number];
									//    }


									//    fprintf(g_pFilePathMOE, "%f,", TT);

									//}
									//for (int iteration_number = max(0, assignment.g_number_of_ODME_iterations-10); iteration_number < assignment.g_number_of_ODME_iterations; iteration_number++)
									//{
									//    double Vol = 0;
									//    if (it->second.path_volume_per_iteration_ODME_map.find(iteration_number) != it->second.path_volume_per_iteration_ODME_map.end())
									//    {
									//        Vol = it->second.path_volume_per_iteration_ODME_map[iteration_number];
									//    }

									//    fprintf(g_pFilePathMOE, "%f,", Vol);

									//}

									//stage III: 

									// output the TT and vol of sensitivity analysis
									//for (int iteration_number = 0; iteration_number < assignment.g_number_of_sensitivity_analysis_iterations; iteration_number++)
									//{
									//    double TT = -1;
									//    if (it->second.path_time_per_iteration_SA_map.find(iteration_number) != it->second.path_time_per_iteration_SA_map.end())
									//    {
									//        TT = it->second.path_time_per_iteration_SA_map[iteration_number];
									//    }

									//    fprintf(g_pFilePathMOE, "%f,", TT);

									//}
									//double prev_value = 0;
									//for (int iteration_number = 0; iteration_number < assignment.g_number_of_sensitivity_analysis_iterations; iteration_number++)
									//{
									//    double Vol = 0;
									//    if (it->second.path_volume_per_iteration_SA_map.find(iteration_number) != it->second.path_volume_per_iteration_SA_map.end())
									//    {
									//        Vol = it->second.path_volume_per_iteration_SA_map[iteration_number];
									//    }

									//    fprintf(g_pFilePathMOE, "%f,", Vol- prev_value);
									//    prev_value = Vol;

									//}


									if (it->second.m_node_size - virtual_first_link_delta - virtual_last_link_delta >= 2)
									{
										if (it->second.path_volume >= 5 || count < 3000)  // critcal path volume
										{
											fprintf(g_pFilePathMOE, "\"LINESTRING (");
											for (int ni = 0 + virtual_first_link_delta; ni < it->second.m_node_size - virtual_last_link_delta; ++ni)
											{
												fprintf(g_pFilePathMOE, "%f %f", g_node_vector[it->second.path_node_vector[ni]].x,
													g_node_vector[it->second.path_node_vector[ni]].y);

												if (ni != it->second.m_node_size - virtual_last_link_delta - 1)
													fprintf(g_pFilePathMOE, ", ");
											}

											fprintf(g_pFilePathMOE, ")\"");
										}
									}

									if (it->second.path_volume >= 5)  // critcal path volume
									{
										// link type name sequenece
										//for (int nl = 0 + virtual_first_link_delta; nl < it->second.m_link_size - virtual_last_link_delta; ++nl)
										//{
										//    link_seq_no = it->second.path_link_vector[nl];
										//    fprintf(g_pFilePathMOE, "%s;", g_link_vector[link_seq_no].link_type_name.c_str());
										//}
										fprintf(g_pFilePathMOE, ",");

										//// link code sequenece
										//for (int nl = 0 + virtual_first_link_delta; nl < it->second.m_link_size - virtual_last_link_delta; ++nl)
										//{
										//    link_seq_no = it->second.path_link_vector[nl];
										//    fprintf(g_pFilePathMOE, "%s;", g_link_vector[link_seq_no].link_code_str.c_str());
										//}
										fprintf(g_pFilePathMOE, ",");

										// link link_distance_VDF sequenece
										//for (int nl = 0 + virtual_first_link_delta; nl < it->second.m_link_size - virtual_last_link_delta; ++nl)
										//{
										//    link_seq_no = it->second.path_link_vector[nl];
										//    fprintf(g_pFilePathMOE, "%.3f;", g_link_vector[link_seq_no].link_distance_VDF);
										//}
										fprintf(g_pFilePathMOE, ",");

										// link FFTT sequenece
										//for (int nl = 0 + virtual_first_link_delta; nl < it->second.m_link_size - virtual_last_link_delta; ++nl)
										//{
										//    link_seq_no = it->second.path_link_vector[nl];
										//    fprintf(g_pFilePathMOE, "%.3f;", g_link_vector[link_seq_no].free_flow_travel_time_in_min);
										//}
										fprintf(g_pFilePathMOE, ",");
									}
									fprintf(g_pFilePathMOE, "\n");
									count++;
								}

							}
						}
					}
				}
			}
		}
		fclose(g_pFilePathMOE);

	}
	g_output_dynamic_queue_profile();
}

void g_output_accessibility_result(Assignment& assignment)
{

	if (assignment.assignment_mode == 0 || assignment.path_output == 0)  //LUE
	{
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "od_accessibility.csv", "w");
		fclose(g_pFilePathMOE);

	}
	else if (assignment.assignment_mode >= 1)  //UE mode, or ODME, DTA
	{
		dtalog.output() << "writing od_accessibility.csv.." << endl;

		double path_time_vector[MAX_LINK_SIZE_IN_A_PATH];
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "od_accessibility.csv", "w");

		if (!g_pFilePathMOE)
		{
			dtalog.output() << "File od_accessibility.csv cannot be opened." << endl;
			g_program_stop();
		}

		fprintf(g_pFilePathMOE, "od_no,o_zone_id,d_zone_id,agent_type,demand_period,volume,connectivity_flag,path_FF_travel_time_min,distance_km,distance_mile,");

		for (int d = 0; d < assignment.g_number_of_sensitivity_analysis_iterations; d++)
		{
			fprintf(g_pFilePathMOE, "OD%d,", d);
		}


		fprintf(g_pFilePathMOE, "\n");
		int count = 1;

		clock_t start_t, end_t;
		start_t = clock();
		clock_t iteration_t;


		int agent_type_size = assignment.g_AgentTypeVector.size();
		int zone_size = g_zone_vector.size();
		int demand_period_size = assignment.g_DemandPeriodVector.size();

		CColumnVector* p_column_pool;

		float path_toll = 0;
		float path_distance = 0;
		float path_travel_time = 0;
		float path_travel_time_without_access_link = 0;
		float path_delay = 0;
		float path_FF_travel_time = 0;
		float time_stamp = 0;

		std::map<int, CColumnPath>::iterator it, it_begin, it_end;

		if (assignment.major_path_volume_threshold > 0.00001)  // performing screening of path flow pattern
		{

			//initialization 
			bool b_subarea_mode = false;

			int number_of_links = g_link_vector.size();
			for (int i = 0; i < number_of_links; ++i)
			{
				for (int tau = 0; tau < demand_period_size; ++tau)
				{
					// used in travel time calculation
					g_link_vector[i].background_PCE_volume_per_period[tau] = 0;
				}

				if (g_node_vector[g_link_vector[i].from_node_seq_no].subarea_id >= 1 && g_node_vector[g_link_vector[i].to_node_seq_no].node_id >= 1)
				{

					b_subarea_mode = true;
				}

			}


			/// <summary>  screening the path flow pattern
			for (int orig = 0; orig < zone_size; ++orig)
			{

				for (int at = 0; at < agent_type_size; ++at)
				{
					for (int dest = 0; dest < zone_size; ++dest)
					{
						for (int tau = 0; tau < demand_period_size; ++tau)
						{
							p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
							if (p_column_pool->od_volume > 0)
							{
								// scan through the map with different node sum for different continuous paths
								it_begin = p_column_pool->path_node_sequence_map.begin();
								it_end = p_column_pool->path_node_sequence_map.end();

								for (it = it_begin; it != it_end; ++it)
								{
									int subarea_output_flag = 0;
									if (b_subarea_mode == true)
									{
										int insubarea_flag = 0;

										for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
										{
											int link_seq_no = it->second.path_link_vector[nl];

											if (g_link_vector[link_seq_no].subarea_id >= 1)
											{
												insubarea_flag = 1;
												break;
											}

										}
										// 
										if (insubarea_flag && it->second.path_volume > assignment.major_path_volume_threshold)
										{
											subarea_output_flag = 1;
										}

									}
									else
									{
										if (it->second.path_volume > assignment.major_path_volume_threshold)
											subarea_output_flag = 1;

									}
									if (subarea_output_flag == 0)
									{
										it->second.subarea_output_flag = 0;  // disable the output of this column into route_assignment.csv

										for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
										{
											int link_seq_no = it->second.path_link_vector[nl];
											g_link_vector[link_seq_no].background_PCE_volume_per_period[tau] += it->second.path_volume;
										}
									}

								}
							}
						}
						/// </summary>
						/// <param name="assignment"></param>
					}

				}
			}


		} // end of path flow pattern screening 
		dtalog.output() << "writing data for " << zone_size << "  zones " << endl;

		int number_of_connected_OD_pairs = 0;

		std::map<string, bool> l_origin_destination_map;
		std::map<string, bool> l_origin_destination_disconnected_map;

		for (int orig = 0; orig < zone_size; ++orig)
		{
			if (g_zone_vector[orig].zone_id % 100 == 0)
				dtalog.output() << "o zone id =  " << g_zone_vector[orig].zone_id << endl;
			for (int dest = 0; dest < zone_size; ++dest)
			{

				for (int at = 0; at < agent_type_size; ++at)
				{
					for (int tau = 0; tau < demand_period_size; ++tau)
					{
						p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
						if (p_column_pool->od_volume > 0 || assignment.zone_seq_no_2_info_mapping.find(orig) != assignment.zone_seq_no_2_info_mapping.end())
						{

							int information_type = 0;

							if (assignment.zone_seq_no_2_info_mapping.find(orig) != assignment.zone_seq_no_2_info_mapping.end())
							{
								information_type = 1;
							}

							time_stamp = (assignment.g_DemandPeriodVector[tau].starting_time_slot_no + assignment.g_DemandPeriodVector[tau].ending_time_slot_no) / 2.0 * MIN_PER_TIMESLOT;

							string od_string;

							od_string = std::to_string(g_zone_vector[orig].zone_id) + "->" + std::to_string(g_zone_vector[dest].zone_id);

							if (p_column_pool->path_node_sequence_map.size() >= 1)
							{
								l_origin_destination_map[od_string] = true;
							}
							else
							{
								if (orig != dest)
								{
									od_string = od_string + ":at = " + assignment.g_AgentTypeVector[at].agent_type.c_str();
									od_string = od_string + ":dp = " + assignment.g_DemandPeriodVector[tau].demand_period.c_str();
									l_origin_destination_disconnected_map[od_string] = false;
								}
							}


							if (p_column_pool->path_node_sequence_map.size() == 0)
							{

								fprintf(g_pFilePathMOE, "%d,%d,%d,",
									count,
									g_zone_vector[orig].zone_id,
									g_zone_vector[dest].zone_id);
								fprintf(g_pFilePathMOE, "%s,%s,%.2f,0\n",
									assignment.g_AgentTypeVector[at].agent_type.c_str(),
									assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
									p_column_pool->od_volume);

								continue;
							}
							// scan through the map with different node sum for different continuous paths
							it_begin = p_column_pool->path_node_sequence_map.begin();
							it_end = p_column_pool->path_node_sequence_map.end();


							for (it = it_begin; it != it_end; ++it)
							{
								if (it->second.subarea_output_flag == 0)
									continue;

								if (count % 100000 == 0)
								{
									end_t = clock();
									iteration_t = end_t - start_t;
									dtalog.output() << "writing " << count / 1000 << "K agents with CPU time " << iteration_t / 1000.0 << " s" << endl;
								}

								path_toll = 0;
								path_distance = 0;
								path_travel_time = 0;
								path_travel_time_without_access_link = 0;
								path_FF_travel_time = 0;

								path_time_vector[0] = time_stamp;
								string link_code_str;

								for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
								{

									int link_seq_no = it->second.path_link_vector[nl];
									if (g_link_vector[link_seq_no].link_type >= 0)
									{
										path_toll += g_link_vector[link_seq_no].VDF_period[tau].toll[at];
										path_distance += g_link_vector[link_seq_no].link_distance_km;
										float link_travel_time = g_link_vector[link_seq_no].travel_time_per_period[tau];
										path_travel_time += link_travel_time;

										if (g_link_vector[link_seq_no].link_type != 1000)   // skip access link
										{
											path_travel_time_without_access_link += link_travel_time;
										}
										path_FF_travel_time += g_link_vector[link_seq_no].VDF_period[tau].FFTT;
										time_stamp += link_travel_time;
										path_time_vector[nl + 1] = time_stamp;

										link_code_str += g_link_vector[link_seq_no].link_code_str;
									}
									else
									{
										int virtual_link = 0;
									}
								}

								double total_agent_path_travel_time = 0;

								for (int vi = 0; vi < it->second.agent_simu_id_vector.size(); ++vi)
								{
									int agent_simu_id = it->second.agent_simu_id_vector[vi];
									CAgent_Simu* pAgentSimu = g_agent_simu_vector[agent_simu_id];
									total_agent_path_travel_time += pAgentSimu->path_travel_time_in_min;
								}

								double final_path_travel_time = path_travel_time;  // by default

								if (it->second.agent_simu_id_vector.size() > 1)  // with simulated agents
								{
									final_path_travel_time = total_agent_path_travel_time / it->second.agent_simu_id_vector.size();
								}



								int virtual_first_link_delta = 1;
								int virtual_last_link_delta = 1;
								// fixed routes have physical nodes always, without virtual connectors
								if (p_column_pool->bfixed_route)
								{

									virtual_first_link_delta = 0;
									virtual_last_link_delta = 1;
								}
								if (information_type == 1 && it->second.path_volume < 0.1)
									continue;

								if (information_type == 1)  // information diversion start from physical nodes
								{
									virtual_first_link_delta = 0;
									virtual_last_link_delta = 1;

									it->second.path_volume = it->second.diverted_vehicle_map.size();
								}


								// assignment_mode = 1, path flow mode
								if (assignment.assignment_mode >= 1)
								{

									int o_zone_agent_type_cover_flag = 0;
									int d_zone_agent_type_cover_flag = 0;

									if (assignment.g_AgentTypeVector[at].zone_id_cover_map.find(g_zone_vector[orig].zone_id) != assignment.g_AgentTypeVector[at].zone_id_cover_map.end())
									{
										o_zone_agent_type_cover_flag = 1;
									}

									if (assignment.g_AgentTypeVector[at].zone_id_cover_map.find(g_zone_vector[dest].zone_id) != assignment.g_AgentTypeVector[at].zone_id_cover_map.end())
									{
										d_zone_agent_type_cover_flag = 1;
									}

									int number_of_nodes = it->second.m_link_size + 1;

									if (number_of_nodes <= 1)
									{
										int i_debug = 1;
									}
									int route_no = it->second.global_path_no;
									if (route_no == -1)
										route_no = count;

									fprintf(g_pFilePathMOE, "%d,%d,%d,",
										count,
										g_zone_vector[orig].zone_id,
										g_zone_vector[dest].zone_id);

									fprintf(g_pFilePathMOE, "%s,%s,%.2f,1,%.4f,%.4f,%.4f,",
										assignment.g_AgentTypeVector[at].agent_type.c_str(),
										assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
										it->second.path_volume,
										path_FF_travel_time,
										path_distance,
										path_distance / 1.609
									);
									//it->second.path_seq_no,
									//information_type,
									//it->second.agent_simu_id_vector.size(),
									//it->second.subarea_output_flag,
									//it->second.measurement_flag,
									//path_toll,
									//final_path_travel_time,
									//path_travel_time,
									//path_travel_time_without_access_link,
									////path_FF_travel_time,
									////final_path_travel_time- path_FF_travel_time,
									//number_of_nodes,
									//path_distance);

									double prev_value = 0;

									for (int d = 0; d < assignment.g_number_of_sensitivity_analysis_iterations; d++)
									{
										double value = 0;

										if (p_column_pool->od_volume_per_iteration_map.find(d) != p_column_pool->od_volume_per_iteration_map.end())
										{
											value = p_column_pool->od_volume_per_iteration_map[d];
										}

										fprintf(g_pFilePathMOE, "%f,", value - prev_value);
										prev_value = value;
									}
									fprintf(g_pFilePathMOE, "\n");


									count++;
								}

							}
						}
					}
				}
			}
		}
		fclose(g_pFilePathMOE);
		assignment.summary_file << "Step 3: check OD connectivity and accessibility in od_accessibility.csv" << endl;

		assignment.summary_file << ", # of connected OD pairs =, " << l_origin_destination_map.size() << endl;
		assignment.summary_file << ", # of OD/agent_type/demand_type columns without paths =, " << l_origin_destination_disconnected_map.size() << endl;
		dtalog.output() << ", # of connected OD pairs = " << l_origin_destination_map.size() << endl;

		if (l_origin_destination_map.size() == 0)
		{
			g_OutputModelFiles(10); // label cost tree
			dtalog.output() << "Please check the connectivity of OD pairs in network." << endl;
			cout << "Please check the model_shortest_path_tree.csv file." << endl;
			g_program_stop();
		}

	}
}

void g_output_dynamic_link_performance(Assignment& assignment, int output_mode = 1)
{
	//dtalog.output() << "writing dynamic_waiting_performance_profile.csv.." << endl;

	//int b_debug_detail_flag = 0;
	//FILE* g_pFileLinkMOE = nullptr;

	//string file_name = "dynamic_link_waiting_time_profile.csv";

	// fopen_ss(&g_pFileLinkMOE, file_name.c_str(), "w");

	//if (!g_pFileLinkMOE)
	//{
	//    dtalog.output() << "File " << file_name.c_str() << " cannot be opened." << endl;
	//    g_program_stop();
	//}

	//    // Option 2: BPR-X function
	//    fprintf(g_pFileLinkMOE, "link_id,scenario_flag,from_node_id,to_node_id,geometry,");
	//    for (int t = 1; t < assignment.g_number_of_intervals_in_min; ++t)
	//        fprintf(g_pFileLinkMOE, "WT%d,",t);

	//    for (int t = 1; t < assignment.g_number_of_intervals_in_min; ++t)
	//        fprintf(g_pFileLinkMOE, "QL%d,", t);

	//    fprintf(g_pFileLinkMOE, "notes\n");


	//    //Initialization for all nodes
	//    for (int i = 0; i < g_link_vector.size(); ++i)
	//    {
	//        // virtual connectors
	//        if (g_link_vector[i].link_type == -1)
	//            continue;

	//                fprintf(g_pFileLinkMOE, "%s,%d,%d,%d,\"%s\",",
	//                    g_link_vector[i].link_id.c_str(),
	//                    g_link_vector[i].capacity_reduction_map.size(),
	//                    g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
	//                    g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
	//                    g_link_vector[i].geometry.c_str());

	//                // first loop for time t
	//                for (int t = 1; t < assignment.g_number_of_intervals_in_min; ++t)
	//                {
	//                    float waiting_time_in_min = 0;

	//                       int timestamp_in_min = assignment.g_LoadingStartTimeInMin + t;

	//                        //if (g_link_vector[i].RT_travel_time_map.find(timestamp_in_min) != g_link_vector[i].RT_travel_time_map.end())
	//                        //{
	//                        //    waiting_time_in_min = g_link_vector[i].RT_travel_time_map[timestamp_in_min];
	//                        //}

	//                        if(timestamp_in_min>0.05)
	//                            fprintf(g_pFileLinkMOE, "%.2f,", waiting_time_in_min);
	//                        else
	//                            fprintf(g_pFileLinkMOE, ",");

	//                }

	//                // first loop for time t
	//                for (int t = 1; t < assignment.g_number_of_intervals_in_min; ++t)
	//                {
	//                    float QL = (assignment.m_LinkCumulativeArrivalVector[i][t] - assignment.m_LinkCumulativeDepartureVector[i][t]);

	//                        fprintf(g_pFileLinkMOE, "%.1f,", QL);
	//                }
	//                fprintf(g_pFileLinkMOE, "\n");
	//    }  // for each link l
	//    fclose(g_pFileLinkMOE);
}

void g_output_TD_link_performance(Assignment& assignment, int output_mode = 1)
{
	dtalog.output() << "writing TD_link_performance.csv.." << endl;
	cout << "writing TD_link_performance.csv.." << endl;

	int b_debug_detail_flag = 0;
	FILE* g_pFileLinkMOE = nullptr;

	string file_name = "TD_link_performance.csv";

	fopen_ss(&g_pFileLinkMOE, file_name.c_str(), "w");

	if (!g_pFileLinkMOE)
	{
		dtalog.output() << "File " << file_name.c_str() << " cannot be opened." << endl;
		g_program_stop();
	}
	else
	{

		// Option 2: BPR-X function
		fprintf(g_pFileLinkMOE, "link_id,tmc_corridor_name,link_type_name,from_node_id,to_node_id,meso_link_id,from_cell_code,lanes,length,free_speed,FFTT,time_period,inflow_volume,outflow_volume,CA,CD,density,queue_length_in_process,queue_ratio,discharge_cap,TD_free_flow_travel_time,waiting_time_in_sec,speed,geometry,");
		fprintf(g_pFileLinkMOE, "notes\n");

		int sampling_time_interval = 1; // min by min

		if (g_link_vector.size() > 5000)
			sampling_time_interval = 15;

		if (g_link_vector.size() > 10000)
			sampling_time_interval = 30;

		if (g_link_vector.size() > 50000)
			sampling_time_interval = 60;

		if (g_link_vector.size() > 500000)
			sampling_time_interval = 120;

		//Initialization for all nodes
		for (int i = 0; i < g_link_vector.size(); ++i)
		{
			// virtual connectors
			if (g_link_vector[i].link_type == -1)
			    continue;

			// first loop for time t
			for (int t = 1; t < assignment.g_number_of_intervals_in_min; ++t)
			{


				if (t % (sampling_time_interval) == 0)
				{
					int time_in_min = t;  //relative time

					float inflow_volume = 0;
					float outflow_volume = 0;
					float queue = 0;
					float waiting_time_in_sec = 0;
					int arrival_rate = 0;
					float avg_waiting_time_in_sec = 0;

					float travel_time = (float)(g_link_vector[i].free_flow_travel_time_in_min + avg_waiting_time_in_sec / 60.0);
					float speed = g_link_vector[i].link_distance_VDF / (g_link_vector[i].free_flow_travel_time_in_min / 60.0);
					float virtual_arrival = 0;

					float discharge_rate = g_link_vector[i].lane_capacity * g_link_vector[i].number_of_lanes * sampling_time_interval / 60.0;

					if (time_in_min >= 1)
					{
						inflow_volume = assignment.m_LinkCumulativeArrivalVector[i][t] - assignment.m_LinkCumulativeArrivalVector[i][t - sampling_time_interval];
						outflow_volume = assignment.m_LinkCumulativeDepartureVector[i][t] - assignment.m_LinkCumulativeDepartureVector[i][t - sampling_time_interval];

						queue = assignment.m_LinkCumulativeArrivalVector[i][t] - assignment.m_LinkCumulativeDepartureVector[i][t];
						//							waiting_time_in_min = queue / (max(1, volume));

						float waiting_time_count = 0;

						waiting_time_in_sec = assignment.m_link_TD_waiting_time[i][t] * number_of_seconds_per_interval;

						arrival_rate = assignment.m_LinkCumulativeArrivalVector[i][t] - assignment.m_LinkCumulativeArrivalVector[i][t - sampling_time_interval];
						avg_waiting_time_in_sec = waiting_time_in_sec / max(1, arrival_rate);

						travel_time = (float)(g_link_vector[i].free_flow_travel_time_in_min + avg_waiting_time_in_sec / 60.0);
						speed = g_link_vector[i].link_distance_VDF / (max(0.00001, travel_time / 60.0));
					}

					if (speed >= 1000)
					{
						int i_debug = 1;
					}
					float density = (assignment.m_LinkCumulativeArrivalVector[i][t] - assignment.m_LinkCumulativeDepartureVector[i][t]) / (g_link_vector[i].link_distance_VDF * g_link_vector[i].number_of_lanes);
					float queue_ratio = (assignment.m_LinkCumulativeArrivalVector[i][t] - assignment.m_LinkCumulativeDepartureVector[i][t]) / (g_link_vector[i].link_distance_VDF * g_link_vector[i].number_of_lanes * g_link_vector[i].kjam);
					if (queue_ratio > 1)
						queue_ratio = 1;
					if (output_mode == 0)
					{
						if (assignment.m_LinkCumulativeArrivalVector[i][t] < 1000)
							continue; //skip
					}

					fprintf(g_pFileLinkMOE, "%s,%s,%s,%d,%d,%d,%s,%d,%.3f,%.3f,%.3f,%s_%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,\"%s\",",
						g_link_vector[i].link_id.c_str(),
						g_link_vector[i].tmc_corridor_name.c_str(),
						g_link_vector[i].link_type_name.c_str(),

						g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
						g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
						g_link_vector[i].meso_link_id,
						g_node_vector[g_link_vector[i].from_node_seq_no].cell_str.c_str(),
						g_link_vector[i].number_of_lanes,
						g_link_vector[i].link_distance_VDF,
						g_link_vector[i].free_speed,
						g_link_vector[i].free_flow_travel_time_in_min,

						g_time_coding(assignment.g_LoadingStartTimeInMin + time_in_min - sampling_time_interval).c_str(),
						g_time_coding(assignment.g_LoadingStartTimeInMin + time_in_min).c_str(),
						inflow_volume,
						outflow_volume,
						assignment.m_LinkCumulativeArrivalVector[i][t],
						assignment.m_LinkCumulativeDepartureVector[i][t],
						density,
						queue,
						queue_ratio,
						discharge_rate,
						travel_time,
						avg_waiting_time_in_sec,
						speed,
						g_link_vector[i].geometry.c_str());

					fprintf(g_pFileLinkMOE, "simulation-based\n");
				}
			}  // for each time t
		}  // for each link l
		fclose(g_pFileLinkMOE);
	}//assignment mode 2 as simulation

}

void g_output_dynamic_link_state(Assignment& assignment, int output_mode = 1)
{
	dtalog.output() << "writing dynamic_link_state.csv.." << endl;

	int b_debug_detail_flag = 0;
	FILE* g_pFileLinkMOE = nullptr;

	string file_name = "dynamic_link_state.csv";

	fopen_ss(&g_pFileLinkMOE, file_name.c_str(), "w");

	if (!g_pFileLinkMOE)
	{
		dtalog.output() << "File " << file_name.c_str() << " cannot be opened." << endl;
		g_program_stop();
	}
	else
	{

		// Option 2: BPR-X function
		fprintf(g_pFileLinkMOE, "link_id,from_node_id,to_node_id,time_period,duration_in_sec,state,state_code\n");


		//Initialization for all nodes
		for (unsigned li = 0; li < g_link_vector.size(); ++li)
		{

			if (g_link_vector[li].timing_arc_flag || g_link_vector[li].m_link_pedefined_capacity_map.size() > 0)  // only output the capaicty for signalized data 
			{
				// reset for signalized links (not freeway links as type code != 'f' for the case of freeway workzones)
				// only for the loading period

				int t = 0;
				int last_t = t;
				if (assignment.m_LinkOutFlowState == NULL)
					break;

				int current_state = assignment.m_LinkOutFlowState[li][t];
				if (current_state == 0)  //close in the beginning
					current_state = -1; // change the initial state to be able to be record the change below

				while (t < assignment.g_number_of_loading_intervals_in_sec - 1)
				{
					int next_state = assignment.m_LinkOutFlowState[li][t + 1];

					bool print_out_flag = false;

					if (current_state == 0 && t % 60 == 0 && g_link_vector[li].m_link_pedefined_capacity_map.find(t) != g_link_vector[li].m_link_pedefined_capacity_map.end())
					{
						print_out_flag = true;
					}

					if (print_out_flag == false && next_state == current_state && t < assignment.g_number_of_loading_intervals_in_sec - 2)
					{
						// do nothing 
					}
					else
					{  // change of state

						string state_str;
						if (g_link_vector[li].timing_arc_flag)
						{


							if (current_state == 1)
								state_str = "g";

							if (current_state == 0)
								state_str = "r";
						}
						else if (g_link_vector[li].m_link_pedefined_capacity_map.find(t) != g_link_vector[li].m_link_pedefined_capacity_map.end())
						{
							state_str = "w";
						}

						if (state_str.size() > 0)  // with data to output
						{
							fprintf(g_pFileLinkMOE, "%s,%d,%d,%s_%s,%d,%d,%s\n",
								g_link_vector[li].link_id.c_str(),
								g_node_vector[g_link_vector[li].from_node_seq_no].node_id,
								g_node_vector[g_link_vector[li].to_node_seq_no].node_id,
								g_time_coding(assignment.g_LoadingStartTimeInMin + last_t / 60.0).c_str(),
								g_time_coding(assignment.g_LoadingStartTimeInMin + (t + 1) / 60.0).c_str(),
								t + 1 - last_t,
								current_state,
								state_str.c_str());
						}

						last_t = t + 1;
						current_state = assignment.m_LinkOutFlowState[li][t + 1];

						if (t + 1 == assignment.g_number_of_loading_intervals_in_sec - 2)
						{
							//boundary condition anyway
							current_state = -1;
						}

					}
					t++;
				}


			}



		}
		fclose(g_pFileLinkMOE);

	}
}

//void g_output_simulation_agents(Assignment& assignment)
//{
//    if (assignment.assignment_mode == 0 || assignment.trajectory_output_count == 0)  //LUE
//    {
//        FILE* g_pFilePathMOE = nullptr;
//        fopen_ss(&g_pFilePathMOE, "agent.csv", "w");
//        fclose(g_pFilePathMOE);
//    }
//    else if (assignment.assignment_mode >= 1)  //UE mode, or ODME, DTA
//    {
//        dtalog.output() << "writing agent.csv.." << endl;
//
//        double path_time_vector[MAX_LINK_SIZE_IN_A_PATH];
//        FILE* g_pFileAgent = nullptr;
//        fopen_ss(&g_pFileAgent, "agent.csv", "w");
//
//        if (!g_pFileAgent)
//        {
//            dtalog.output() << "File agent.csv cannot be opened." << endl;
//            g_program_stop();
//        }
//
//        fprintf(g_pFileAgent, "agent_id,o_zone_id,d_zone_id,OD_index,path_id,#_of_links,diversion_flag,agent_type,demand_period,volume,toll,travel_time,distance_km,speed,departure_time_in_min,arrival_time_in_min,departure_time_slot_no,\n");
//
//        int count = 1;
//
//        clock_t start_t, end_t;
//        start_t = clock();
//        clock_t iteration_t;
//
//        int agent_type_size = assignment.g_AgentTypeVector.size();
//        int zone_size = g_zone_vector.size();
//        int demand_period_size = assignment.g_DemandPeriodVector.size();
//
//        CColumnVector* p_column_pool;
//
//        float path_toll = 0;
//        float path_distance = 0;
//        float path_travel_time = 0;
//        float path_travel_time_without_access_link = 0;
//        float time_stamp = 0;
//
//        if (assignment.trajectory_sampling_rate < 0.01)
//            assignment.trajectory_sampling_rate = 0.01;
//        int sampling_step = 100 / int(100 * assignment.trajectory_sampling_rate + 0.5);
//
//        std::map<int, CColumnPath>::iterator it, it_begin, it_end;
//
//        dtalog.output() << "writing data for " << zone_size << "  zones " << endl;
//
//        for (int orig = 0; orig < zone_size; ++orig)
//        {
//            if (g_zone_vector[orig].zone_id % 100 == 0)
//                dtalog.output() << "o zone id =  " << g_zone_vector[orig].zone_id << endl;
//
//            for (int at = 0; at < agent_type_size; ++at)
//            {
//                for (int dest = 0; dest < zone_size; ++dest)
//                {
//                    for (int tau = 0; tau < demand_period_size; ++tau)
//                    {
//                        p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
//                        if (p_column_pool->od_volume > 0)
//                        {
//
//                            // scan through the map with different node sum for different continuous paths
//                            it_begin = p_column_pool->path_node_sequence_map.begin();
//                            it_end = p_column_pool->path_node_sequence_map.end();
//
//                            for (it = it_begin; it != it_end; ++it)
//                            {
//                                if (count % 100000 == 0)
//                                {
//                                    end_t = clock();
//                                    iteration_t = end_t - start_t;
//                                    dtalog.output() << "writing " << count / 1000 << "K agents with CPU time " << iteration_t / 1000.0 << " s" << endl;
//                                }
//
//                                if (count % sampling_step != 0)
//                                    continue;
//
//
//                                path_toll = 0;
//                                path_distance = 0;
//                                path_travel_time = 0;
//                                path_travel_time_without_access_link = 0;
//                                path_time_vector[0] = time_stamp;
//
//
//
//                                // assignment_mode = 1, path flow mode
//                                {
//                                    // assignment_mode = 2, simulated agent flow mode //DTA simulation 
//
//                                    for (int vi = 0; vi < it->second.agent_simu_id_vector.size(); ++vi)
//                                    {
//                                        int agent_simu_id = it->second.agent_simu_id_vector[vi];
//                                        CAgent_Simu* pAgentSimu = g_agent_simu_vector[agent_simu_id];
//                                        time_stamp = assignment.g_LoadingStartTimeInMin + pAgentSimu->departure_time_in_min;
//
//                                        float departure_time_in_min = time_stamp;
//                                        float arrival_time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->arrival_time_in_min;
//                                        int departure_time_in_slot_no = time_stamp / MIN_PER_TIMESLOT;
//                                        float speed = pAgentSimu->path_distance / max(0.001, pAgentSimu->path_travel_time_in_min) * 60;
//
//                                        if (it->second.m_link_size >= MAX_LINK_SIZE_IN_A_PATH -1)
//                                        {
//                                            dtalog.output() << "error: it->second.m_link_size >= MAX_LINK_SIZE_IN_A_PATH-1" << endl;
//                                            g_program_stop();
//                                        }
//
//                                        fprintf(g_pFileAgent, "%d,%d,%d,%d->%d,%d,%d,%d,%s,%s,1,%.1f,%.4f,%.4f,%.4f,%.4f,%.4f,%d",
//                                            pAgentSimu->agent_id,
//                                            g_zone_vector[orig].zone_id,
//                                            g_zone_vector[dest].zone_id,
//                                            g_zone_vector[orig].zone_id,
//                                            g_zone_vector[dest].zone_id,
//                                            it->second.path_seq_no,
//                                            it->second.m_link_size,
//                                            pAgentSimu->diversion_flag,
//                                            assignment.g_AgentTypeVector[at].agent_type.c_str(),
//                                            assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
//                                            path_toll,
//                                            pAgentSimu->path_travel_time_in_min,
//                                            pAgentSimu->path_distance, speed,
//                                            departure_time_in_min,
//                                            arrival_time_in_min,
//                                            departure_time_in_slot_no);
//
//                                        count++;
//
//                                        fprintf(g_pFileAgent, "\n");
//                                    }
//                                }
//                            }
//                        }
//                    }
//                }
//            }
//        }
//        fclose(g_pFileAgent);
//    }
//}

void g_output_agent_csv(Assignment& assignment)
{

	if (assignment.assignment_mode == 0)  //LUE
	{
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "agent.csv", "w");
		fclose(g_pFilePathMOE);
	}
	else if (assignment.assignment_mode >= 1)  //UE mode, or ODME, DTA
	{
		dtalog.output() << "writing agent.csv.." << endl;
		assignment.summary_file << ",summary by multi-modal agents,agent_type,#_agents,avg_distance,avg_travel_time_in_min,avg_free_speed," << endl;

		double path_time_vector[MAX_LINK_SIZE_IN_A_PATH];
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "agent.csv", "w");

		if (!g_pFilePathMOE)
		{
			dtalog.output() << "File agent.csv cannot be opened." << endl;
			g_program_stop();
		}

		fprintf(g_pFilePathMOE, "first_column,agent_id,o_zone_id,d_zone_id,OD_key,path_id,activity_zone_sequence,activity_agent_type_sequence,display_code,impacted_flag,info_receiving_flag,diverted_flag,agent_type_no,agent_type,PCE_unit,demand_period,volume,toll,departure_time,dt_hhmm,departure_time_in_1min,departure_time_in_5_min,travel_time,distance_mile,speed_mph,waiting_time,max_link_waiting_time,max_wait_link,\n");

		int count = 1;

		clock_t start_t, end_t;
		start_t = clock();
		clock_t iteration_t;

		int agent_type_size = assignment.g_AgentTypeVector.size();
		int zone_size = g_zone_vector.size();
		int demand_period_size = assignment.g_DemandPeriodVector.size();

		CColumnVector* p_column_pool;

		float path_toll = 0;
		float path_distance = 0;
		float path_travel_time = 0;
		float speed_mph = 0;
		float time_stamp = 0;

		if (assignment.trajectory_sampling_rate < 0.01)
			assignment.trajectory_sampling_rate = 0.01;
		int sampling_step = 100 / int(100 * assignment.trajectory_sampling_rate + 0.5);

		std::map<int, CColumnPath>::iterator it, it_begin, it_end;

		dtalog.output() << "writing data for " << zone_size << "  zones " << endl;

		for (int at = 0; at < agent_type_size; ++at)
		{
			int agent_count = 0;
			double total_agent_distance = 0;
			double total_agent_travel_time = 0;

			for (int orig = 0; orig < zone_size; ++orig)
			{
				if (g_zone_vector[orig].zone_id % 100 == 0)
					dtalog.output() << "o zone id =  " << g_zone_vector[orig].zone_id << endl;

				for (int dest = 0; dest < zone_size; ++dest)
				{
					for (int tau = 0; tau < demand_period_size; ++tau)
					{
						p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
						if (p_column_pool->od_volume > 0)
						{

							// scan through the map with different node sum for different continuous paths
							it_begin = p_column_pool->path_node_sequence_map.begin();
							it_end = p_column_pool->path_node_sequence_map.end();

							for (it = it_begin; it != it_end; ++it)
							{
								if (count % 100000 == 0)
								{
									end_t = clock();
									iteration_t = end_t - start_t;
									dtalog.output() << "writing " << count / 1000 << "K agents with CPU time " << iteration_t / 1000.0 << " s" << endl;
								}

								if (count % sampling_step != 0)
									continue;

								if (count > assignment.trajectory_output_count && assignment.trajectory_output_count >= 1)  // if trajectory_output_count ==-1, this constraint does not apply
									continue;

								path_toll = 0;
								path_distance = 0;
								path_travel_time = 0;
								path_time_vector[0] = time_stamp;

								for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
								{
									int link_seq_no = it->second.path_link_vector[nl];
									path_toll += g_link_vector[link_seq_no].VDF_period[tau].toll[at];
									path_distance += g_link_vector[link_seq_no].link_distance_VDF;
									float link_travel_time = g_link_vector[link_seq_no].travel_time_per_period[tau];

									time_stamp += link_travel_time;
									path_time_vector[nl + 1] = time_stamp;
								}

								int virtual_link_delta = 1;
								int virtual_begin_link_delta = 1;
								int virtual_end_link_delta = 1;
								// fixed routes have physical nodes always, without virtual connectors
								if (p_column_pool->bfixed_route)
								{
									virtual_begin_link_delta = 0;
									virtual_end_link_delta = 1;

								}

								// assignment_mode = 1, path flow mode
								{
									// assignment_mode = 2, simulated agent flow mode //DTA simulation 

									agent_count += it->second.agent_simu_id_vector.size();
									for (int vi = 0; vi < it->second.agent_simu_id_vector.size(); ++vi)
									{


										int agent_simu_id = it->second.agent_simu_id_vector[vi];
										CAgent_Simu* pAgentSimu = g_agent_simu_vector[agent_simu_id];
										if (pAgentSimu->agent_id == 81)
										{
											int idebug = 1;
										}
										if (assignment.trajectory_diversion_only == 1 && pAgentSimu->diversion_flag == 0)  // diversion flag only, then we skip the non-diversion path
											continue;

										float vehicle_departure_time = assignment.g_LoadingStartTimeInMin + pAgentSimu->departure_time_in_min;

										float waiting_time_in_min = pAgentSimu->waiting_time_in_min;
										float max_link_waiting_time_in_min = pAgentSimu->max_link_waiting_time_in_min;

										int from_node_id_max_wait = -1;
										int to_node_id_max_wait = -1;

										if (pAgentSimu->max_waiting_time_link_no >= 0)
										{
											from_node_id_max_wait = g_node_vector[g_link_vector[pAgentSimu->max_waiting_time_link_no].from_node_seq_no].node_id;
											to_node_id_max_wait = g_node_vector[g_link_vector[pAgentSimu->max_waiting_time_link_no].to_node_seq_no].node_id;
										}

										time_stamp = assignment.g_LoadingStartTimeInMin + pAgentSimu->departure_time_in_min;
										for (int nt = 0 + virtual_link_delta; nt < pAgentSimu->path_link_seq_no_vector.size() + 1 - virtual_link_delta; ++nt)
										{
											float time_in_min = 0;

											if (nt < pAgentSimu->path_link_seq_no_vector.size() - virtual_link_delta)
												time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->m_veh_link_arrival_time_in_simu_interval[nt] * number_of_seconds_per_interval / 60.0;
											else
												time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->m_veh_link_departure_time_in_simu_interval[nt - 1] * number_of_seconds_per_interval / 60.0;  // last link in the path

											path_time_vector[nt] = time_in_min;
										}

										float vehicle_travel_time = pAgentSimu->path_travel_time_in_min;
										speed_mph = path_distance / max(0.01, vehicle_travel_time / 60.0);
										//if (count >= 2000)  // only output up to 2000
										//    break;

										int hour = vehicle_departure_time / 60;
										int minute = (int)((vehicle_departure_time / 60.0 - hour) * 60);

										// some bugs for output link performances before
										fprintf(g_pFilePathMOE, ",%d,%d,%d,%d->%d,%d,%s,%s,%d,%d,%d,%d,%d,%s,",
											pAgentSimu->agent_id,
											g_zone_vector[orig].zone_id,
											g_zone_vector[dest].zone_id,
											g_zone_vector[orig].zone_id,
											g_zone_vector[dest].zone_id,
											it->second.path_seq_no,
											p_column_pool->activity_zone_sequence.c_str(),
											p_column_pool->activity_agent_type_sequence.c_str(),
											0,
											pAgentSimu->impacted_flag,
											pAgentSimu->info_receiving_flag,
											pAgentSimu->diverted_flag,
											pAgentSimu->agent_type_no,
											assignment.g_AgentTypeVector[at].agent_type.c_str());

										fprintf(g_pFilePathMOE, "%d,%s,1,0,%.4f,T%02d%02d,%d,%d,%.4f,%.4f,",
											pAgentSimu->PCE_unit_size,
											assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
											vehicle_departure_time,
											hour, minute,
											int(vehicle_departure_time),
											int(vehicle_departure_time / 5) * 5,
											vehicle_travel_time,
											path_distance, speed_mph);

										total_agent_distance += path_distance;

										total_agent_travel_time += vehicle_travel_time;

										fprintf(g_pFilePathMOE, "%.1f,%.1f,", waiting_time_in_min, max_link_waiting_time_in_min);

										if (max_link_waiting_time_in_min >= 0.1)
											fprintf(g_pFilePathMOE, "%d->%d,", from_node_id_max_wait, to_node_id_max_wait);
										else
											fprintf(g_pFilePathMOE, ",");


										//fprintf(g_pFilePathMOE, ")\"");
										fprintf(g_pFilePathMOE, "\n");

										count++;
									}
								}
							}
						}
					}
				}
			}

			assignment.summary_file << ",," << assignment.g_AgentTypeVector[at].agent_type.c_str() << "," << agent_count << "," << total_agent_distance / max(1, agent_count)
				<< "," << total_agent_travel_time / max(1, agent_count) << "," << total_agent_distance / max(0.0001, total_agent_travel_time / 60.0) << endl;


		}
		fclose(g_pFilePathMOE);
	}


}

void g_output_trajectory_csv(Assignment& assignment)
{

	if (assignment.assignment_mode == 0 || assignment.trajectory_output_count == 0)  //LUE
	{
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "trajectory.csv", "w");
		fclose(g_pFilePathMOE);
	}
	else if (assignment.assignment_mode >= 1)  //UE mode, or ODME, DTA
	{
		int total_number_of_nodes = 0;
		dtalog.output() << "writing trajectory.csv.." << endl;

		double path_time_vector[MAX_LINK_SIZE_IN_A_PATH];
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "trajectory.csv", "w");

		if (!g_pFilePathMOE)
		{
			dtalog.output() << "File trajectory.csv cannot be opened." << endl;
			g_program_stop();
		}

		fprintf(g_pFilePathMOE, "first_column,agent_id,o_zone_id,d_zone_id,path_id,display_code,impacted_flag,info_receiving_flag,diverted_flag,agent_type_no,agent_type,PCE_unit,demand_period,volume,toll,departure_time,travel_time,distance_km,distance_mile,speed_kmph,speed_mph,waiting_time,max_link_waiting_time,max_wait_link,node_sequence,time_sequence\n");

		int count = 1;

		clock_t start_t, end_t;
		start_t = clock();
		clock_t iteration_t;

		int agent_type_size = assignment.g_AgentTypeVector.size();
		int zone_size = g_zone_vector.size();
		int demand_period_size = assignment.g_DemandPeriodVector.size();

		CColumnVector* p_column_pool;

		float path_toll = 0;
		double path_distance_km = 0;
		double path_distance_mile = 0;
		float path_travel_time = 0;
		float speed_mph = 0;
		float speed_kmph = 0;
		float time_stamp = 0;

		if (assignment.trajectory_sampling_rate < 0.01)
			assignment.trajectory_sampling_rate = 0.01;
		int sampling_step = 100 / int(100 * assignment.trajectory_sampling_rate + 0.5);

		std::map<int, CColumnPath>::iterator it, it_begin, it_end;

		dtalog.output() << "writing data for " << zone_size << "  zones " << endl;

		for (int orig = 0; orig < zone_size; ++orig)
		{
			if (g_zone_vector[orig].zone_id % 100 == 0)
				dtalog.output() << "o zone id =  " << g_zone_vector[orig].zone_id << endl;

			for (int at = 0; at < agent_type_size; ++at)
			{
				for (int dest = 0; dest < zone_size; ++dest)
				{
					for (int tau = 0; tau < demand_period_size; ++tau)
					{
						p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
						if (p_column_pool->od_volume > 0)
						{

							// scan through the map with different node sum for different continuous paths
							it_begin = p_column_pool->path_node_sequence_map.begin();
							it_end = p_column_pool->path_node_sequence_map.end();

							for (it = it_begin; it != it_end; ++it)
							{
								if (count % 100000 == 0)
								{
									end_t = clock();
									iteration_t = end_t - start_t;
									dtalog.output() << "writing " << count / 1000 << "K agents with CPU time " << iteration_t / 1000.0 << " s" << endl;
								}

								if (count % sampling_step != 0)
									continue;

								if (count > assignment.trajectory_output_count && assignment.trajectory_output_count >= 1)  // if trajectory_output_count ==-1, this constraint does not apply
									continue;

								path_toll = 0;
								path_distance_km = 0;
								path_distance_mile = 0;
								path_travel_time = 0;
								path_time_vector[0] = time_stamp;

								for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
								{
									int link_seq_no = it->second.path_link_vector[nl];
									path_toll += g_link_vector[link_seq_no].VDF_period[tau].toll[at];
									path_distance_km += g_link_vector[link_seq_no].link_distance_km;
									path_distance_mile += g_link_vector[link_seq_no].link_distance_mile;
									float link_travel_time = g_link_vector[link_seq_no].travel_time_per_period[tau];

									time_stamp += link_travel_time;
									path_time_vector[nl + 1] = time_stamp;
								}

								int virtual_link_delta = 1;
								int virtual_begin_link_delta = 1;
								int virtual_end_link_delta = 1;
								// fixed routes have physical nodes always, without virtual connectors
								if (p_column_pool->bfixed_route)
								{
									virtual_begin_link_delta = 0;
									virtual_end_link_delta = 1;

								}

								// assignment_mode = 1, path flow mode
								{
									// assignment_mode = 2, simulated agent flow mode //DTA simulation 

									for (int vi = 0; vi < it->second.agent_simu_id_vector.size(); ++vi)
									{


										int agent_simu_id = it->second.agent_simu_id_vector[vi];
										CAgent_Simu* pAgentSimu = g_agent_simu_vector[agent_simu_id];
										if (pAgentSimu->agent_id == 81)
										{
											int idebug = 1;
										}
										if (assignment.trajectory_diversion_only == 1 && pAgentSimu->diversion_flag == 0)  // diversion flag only, then we skip the non-diversion path
											continue;

										float vehicle_departure_time = assignment.g_LoadingStartTimeInMin + pAgentSimu->departure_time_in_min;

										float waiting_time_in_min = pAgentSimu->waiting_time_in_min;
										float max_link_waiting_time_in_min = pAgentSimu->max_link_waiting_time_in_min;

										int from_node_id_max_wait = -1;
										int to_node_id_max_wait = -1;

										if (pAgentSimu->max_waiting_time_link_no >= 0)
										{
											from_node_id_max_wait = g_node_vector[g_link_vector[pAgentSimu->max_waiting_time_link_no].from_node_seq_no].node_id;
											to_node_id_max_wait = g_node_vector[g_link_vector[pAgentSimu->max_waiting_time_link_no].to_node_seq_no].node_id;
										}

										time_stamp = assignment.g_LoadingStartTimeInMin + pAgentSimu->departure_time_in_min;
										for (int nt = 0 + virtual_link_delta; nt < pAgentSimu->path_link_seq_no_vector.size() + 1 - virtual_link_delta; ++nt)
										{
											float time_in_min = 0;

											if (nt < pAgentSimu->path_link_seq_no_vector.size() - virtual_link_delta)
												time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->m_veh_link_arrival_time_in_simu_interval[nt] * number_of_seconds_per_interval / 60.0;
											else
												time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->m_veh_link_departure_time_in_simu_interval[nt - 1] * number_of_seconds_per_interval / 60.0;  // last link in the path

											path_time_vector[nt] = time_in_min;
										}

										float vehicle_travel_time = pAgentSimu->path_travel_time_in_min;
										speed_mph = path_distance_mile / max(0.01, vehicle_travel_time / 60.0);
										speed_kmph = path_distance_km / max(0.01, vehicle_travel_time / 60.0);
										//if (count >= 2000)  // only output up to 2000
										//    break;

										// some bugs for output link performances before
										fprintf(g_pFilePathMOE, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,",
											pAgentSimu->agent_id,
											g_zone_vector[orig].zone_id,
											g_zone_vector[dest].zone_id,
											it->second.path_seq_no,
											0,
											pAgentSimu->impacted_flag,
											pAgentSimu->info_receiving_flag,
											pAgentSimu->diverted_flag,
											pAgentSimu->agent_type_no,
											assignment.g_AgentTypeVector[at].agent_type.c_str());

										fprintf(g_pFilePathMOE, "%d,%s,1,0,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,",
											pAgentSimu->PCE_unit_size,
											assignment.g_DemandPeriodVector[tau].demand_period.c_str(),
											vehicle_departure_time,
											vehicle_travel_time,
											path_distance_km, path_distance_mile, speed_kmph, speed_mph);

										fprintf(g_pFilePathMOE, "%.1f,%.1f,", waiting_time_in_min, max_link_waiting_time_in_min);

										if (max_link_waiting_time_in_min >= 0.1)
											fprintf(g_pFilePathMOE, "%d->%d,", from_node_id_max_wait, to_node_id_max_wait);
										else
											fprintf(g_pFilePathMOE, ",");


										total_number_of_nodes += (pAgentSimu->path_link_seq_no_vector.size() + 1);
										/* Format and print various data */
										for (int ni = 0 + virtual_link_delta; ni < pAgentSimu->path_link_seq_no_vector.size(); ++ni)
										{
											int node_id = g_node_vector[g_link_vector[pAgentSimu->path_link_seq_no_vector[ni]].from_node_seq_no].node_id;
											fprintf(g_pFilePathMOE, "%d;", node_id);
										}

										fprintf(g_pFilePathMOE, ",");

										for (int nt = 0 + virtual_link_delta; nt < pAgentSimu->path_link_seq_no_vector.size() + 1 - virtual_link_delta; ++nt)
											fprintf(g_pFilePathMOE, "%f;", path_time_vector[nt]);

										// time coded
										//for (int nt = 0 + virtual_link_delta; nt < pAgentSimu->path_link_seq_no_vector.size() + 1 - virtual_link_delta; ++nt)
										//    fprintf(g_pFilePathMOE, "%s;", g_time_coding(path_time_vector[nt]).c_str());

										//fprintf(g_pFilePathMOE, "\"LINESTRING (");

										//for (int ni = 0 + virtual_link_delta; ni < pAgentSimu->path_link_seq_no_vector.size(); ++ni)
										//{

										//    int node_no = g_link_vector[pAgentSimu->path_link_seq_no_vector[ni]].from_node_seq_no;
										//    fprintf(g_pFilePathMOE, "%f %f", g_node_vector[node_no].x,
										//        g_node_vector[node_no].y);

										//    if (ni != pAgentSimu->path_link_seq_no_vector.size() - 1)
										//        fprintf(g_pFilePathMOE, ", ");
										//}


										//fprintf(g_pFilePathMOE, ")\"");
										fprintf(g_pFilePathMOE, "\n");

										count++;
									}
								}
							}
						}
					}
				}
			}
		}
		fclose(g_pFilePathMOE);
		assignment.summary_file << ", # of simulated agents in trajectory.csv=," << count << ",avg # of nodes per agent=" << total_number_of_nodes * 1.0 / max(1, count) << endl;
	}

	int b_trace_file = false;

	// output trace file
	if (assignment.assignment_mode == 0 || assignment.trace_output == 0)  //LUE
	{
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "trace.csv", "w");
		fclose(g_pFilePathMOE);
	}
	else if (assignment.assignment_mode >= 1)  //UE mode, or ODME, DTA
	{
		dtalog.output() << "writing trace.csv.." << endl;

		double path_time_vector[MAX_LINK_SIZE_IN_A_PATH];
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "trace.csv", "w");

		if (!g_pFilePathMOE)
		{
			dtalog.output() << "File trace.csv cannot be opened." << endl;
			g_program_stop();
		}

		fprintf(g_pFilePathMOE, "agent_id,seq_no,node_id,timestamp,timestamp_in_min,trip_time_in_min,travel_time_in_sec,waiting_time_in_simu_interval,x_coord,y_coord\n");

		int count = 1;

		clock_t start_t, end_t;
		start_t = clock();
		clock_t iteration_t;

		int agent_type_size = assignment.g_AgentTypeVector.size();
		int zone_size = g_zone_vector.size();
		int demand_period_size = assignment.g_DemandPeriodVector.size();

		CColumnVector* p_column_pool;

		float path_toll = 0;
		float path_distance = 0;
		float path_travel_time = 0;
		float time_stamp = 0;

		if (assignment.trajectory_sampling_rate < 0.01)
			assignment.trajectory_sampling_rate = 0.01;
		int sampling_step = 1;

		std::map<int, CColumnPath>::iterator it, it_begin, it_end;

		dtalog.output() << "writing data for " << zone_size << "  zones " << endl;

		for (int orig = 0; orig < zone_size; ++orig)
		{
			for (int at = 0; at < agent_type_size; ++at)
			{
				for (int dest = 0; dest < zone_size; ++dest)
				{
					for (int tau = 0; tau < demand_period_size; ++tau)
					{
						p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
						if (p_column_pool->od_volume > 0)
						{

							// scan through the map with different node sum for different continuous paths
							it_begin = p_column_pool->path_node_sequence_map.begin();
							it_end = p_column_pool->path_node_sequence_map.end();

							for (it = it_begin; it != it_end; ++it)
							{
								if (count % 100000 == 0)
								{
									end_t = clock();
									iteration_t = end_t - start_t;
									dtalog.output() << "writing " << count / 1000 << "K agents with CPU time " << iteration_t / 1000.0 << " s" << endl;
								}

								if (count % sampling_step != 0)
									continue;

								if (count >= 2000)  // only output up to 2000
									break;

								path_toll = 0;
								path_distance = 0;
								path_travel_time = 0;
								path_time_vector[0] = time_stamp;

								for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
								{
									int link_seq_no = it->second.path_link_vector[nl];
									path_toll += g_link_vector[link_seq_no].VDF_period[tau].toll[at];
									path_distance += g_link_vector[link_seq_no].link_distance_VDF;
									float link_travel_time = g_link_vector[link_seq_no].travel_time_per_period[tau];
									path_travel_time += link_travel_time;
									time_stamp += link_travel_time;
									path_time_vector[nl + 1] = time_stamp;
								}

								int virtual_link_delta = 1;

								// Xuesong: 11/20/2021, need to check again 
								// fixed routes have physical nodes always, without virtual connectors
								//if (p_column_pool->bfixed_route)
								//    virtual_link_delta = 0;

								// assignment_mode = 1, path flow mode
								{
									// assignment_mode = 2, simulated agent flow mode //DTA simulation 

									for (int vi = 0; vi < it->second.agent_simu_id_vector.size(); ++vi)
									{

										int agent_simu_id = it->second.agent_simu_id_vector[vi];
										CAgent_Simu* pAgentSimu = g_agent_simu_vector[agent_simu_id];


										if (pAgentSimu->agent_id == 81)
										{
											int idebug = 1;
										}
										//if (assignment.trajectory_diversion_only == 1 && pAgentSimu->diversion_flag == 0)  // diversion flag only, then we skip the non-diversion path
										//     continue;

										for (int nt = 0 + virtual_link_delta; nt < pAgentSimu->path_link_seq_no_vector.size() + 1 - virtual_link_delta; ++nt)
										{
											float time_in_min = 0;

											if (nt < pAgentSimu->path_link_seq_no_vector.size() - virtual_link_delta)
												time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->m_veh_link_arrival_time_in_simu_interval[nt] * number_of_seconds_per_interval / 60.0;
											else
												time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->m_veh_link_departure_time_in_simu_interval[nt - 1] * number_of_seconds_per_interval / 60.0;  // last link in the path

											path_time_vector[nt] = time_in_min;
										}


										for (int nt = 0 + virtual_link_delta; nt < pAgentSimu->path_link_seq_no_vector.size() - virtual_link_delta; ++nt)
										{
											float trip_time_in_min = path_time_vector[nt] - path_time_vector[0 + virtual_link_delta];

											int node_id = g_node_vector[g_link_vector[pAgentSimu->path_link_seq_no_vector[nt]].from_node_seq_no].node_id;

											if (nt >= pAgentSimu->path_link_seq_no_vector.size() - virtual_link_delta)
											{
												node_id = g_node_vector[g_link_vector[pAgentSimu->path_link_seq_no_vector[nt]].to_node_seq_no].node_id;

											}
											int link_seq_no = pAgentSimu->path_link_seq_no_vector[nt];
											if (link_seq_no < 0)
											{
												int i_error = 1;
												continue;
											}
											float travel_time_in_sec = (path_time_vector[nt + 1] - path_time_vector[nt]) * 60;
											int waiting_time_in_simu_interaval = (path_time_vector[nt + 1] - path_time_vector[nt] - g_link_vector[link_seq_no].free_flow_travel_time_in_min) * number_of_simu_intervals_in_min;

											int node_no = g_link_vector[pAgentSimu->path_link_seq_no_vector[nt]].from_node_seq_no;

											fprintf(g_pFilePathMOE, "%d,%d,%d,T%s,%.5f,%f,%.4f,%d,%f,%f\n",
												pAgentSimu->agent_id, nt, node_id,
												g_time_coding(path_time_vector[nt]).c_str(),
												path_time_vector[nt],
												trip_time_in_min,
												travel_time_in_sec,
												waiting_time_in_simu_interaval,
												g_node_vector[node_no].x, g_node_vector[node_no].y);

										}
										count++;
									}
								}
							}
						}
					}
				}
			}
		}
		fclose(g_pFilePathMOE);
	}


}

void g_output_trajectory_bin(Assignment& assignment)
{

	if (assignment.assignment_mode == 0 || assignment.trajectory_output_count == 0)  //LUE
	{
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "trajectory.bin", "wb");
		fclose(g_pFilePathMOE);
	}
	else if (assignment.assignment_mode >= 1)  //UE mode, or ODME, DTA
	{
		dtalog.output() << "writing trajectory.bin.." << endl;

		int path_node_vector[MAX_LINK_SIZE_IN_A_PATH];
		double path_time_vector[MAX_LINK_SIZE_IN_A_PATH];
		FILE* g_pFilePathMOE = nullptr;
		fopen_ss(&g_pFilePathMOE, "trajectory.bin", "wb");

		if (!g_pFilePathMOE)
		{
			dtalog.output() << "File trajectory.bin cannot be opened." << endl;
			g_program_stop();
		}


		struct STrajectoryHeader
		{
			int agent_id, o_zone_id, d_zone_id, path_id;
			int display_code;
			int impacted_flag, info_receiving_flag, diverted_flag;
			int agent_type_no, PCE_unit, demand_period;
			int node_size, link_size;
			float volume, toll, travel_time, distance_km;
			int reserved1;
			int reserved2;
			float distance_mile;
			float reserved4;

		};

		STrajectoryHeader header;

		int count = 1;

		clock_t start_t, end_t;
		start_t = clock();
		clock_t iteration_t;

		int agent_type_size = assignment.g_AgentTypeVector.size();
		int zone_size = g_zone_vector.size();
		int demand_period_size = assignment.g_DemandPeriodVector.size();

		CColumnVector* p_column_pool;

		float path_toll = 0;
		float path_distance_km = 0;
		float path_distance_mile = 0;
		float path_travel_time = 0;
		float time_stamp = 0;

		if (assignment.trajectory_sampling_rate < 0.01)
			assignment.trajectory_sampling_rate = 0.01;
		int sampling_step = 100 / int(100 * assignment.trajectory_sampling_rate + 0.5);

		std::map<int, CColumnPath>::iterator it, it_begin, it_end;

		dtalog.output() << "writing data for " << zone_size << "  zones " << endl;

		for (int orig = 0; orig < zone_size; ++orig)
		{
			if (g_zone_vector[orig].zone_id % 100 == 0)
				dtalog.output() << "o zone id =  " << g_zone_vector[orig].zone_id << endl;

			for (int at = 0; at < agent_type_size; ++at)
			{
				for (int dest = 0; dest < zone_size; ++dest)
				{
					for (int tau = 0; tau < demand_period_size; ++tau)
					{
						p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
						if (p_column_pool->od_volume > 0)
						{

							// scan through the map with different node sum for different continuous paths
							it_begin = p_column_pool->path_node_sequence_map.begin();
							it_end = p_column_pool->path_node_sequence_map.end();

							for (it = it_begin; it != it_end; ++it)
							{
								if (count % 100000 == 0)
								{
									end_t = clock();
									iteration_t = end_t - start_t;
									dtalog.output() << "writing " << count / 1000 << "K binary agents with CPU time " << iteration_t / 1000.0 << " s" << endl;
								}

								if (count % sampling_step != 0)
									continue;


								path_toll = 0;
								path_distance_km = 0;
								path_distance_mile = 0;
								path_travel_time = 0;
								path_time_vector[0] = time_stamp;

								for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
								{
									int link_seq_no = it->second.path_link_vector[nl];
									path_toll += g_link_vector[link_seq_no].VDF_period[tau].toll[at];
									path_distance_km += g_link_vector[link_seq_no].link_distance_km;
									path_distance_mile += g_link_vector[link_seq_no].link_distance_mile;
									float link_travel_time = g_link_vector[link_seq_no].travel_time_per_period[tau];

									time_stamp += link_travel_time;
									path_time_vector[nl + 1] = time_stamp;
								}

								int virtual_link_delta = 1;
								int virtual_begin_link_delta = 1;
								int virtual_end_link_delta = 1;
								// fixed routes have physical nodes always, without virtual connectors
								if (p_column_pool->bfixed_route)
								{
									virtual_begin_link_delta = 0;
									virtual_end_link_delta = 1;

								}

								// assignment_mode = 1, path flow mode
								{
									// assignment_mode = 2, simulated agent flow mode //DTA simulation 

									for (int vi = 0; vi < it->second.agent_simu_id_vector.size(); ++vi)
									{


										int agent_simu_id = it->second.agent_simu_id_vector[vi];
										CAgent_Simu* pAgentSimu = g_agent_simu_vector[agent_simu_id];
										if (pAgentSimu->agent_id == 81)
										{
											int idebug = 1;
										}
										if (assignment.trajectory_diversion_only == 1 && pAgentSimu->diversion_flag == 0)  // diversion flag only, then we skip the non-diversion path
											continue;

										time_stamp = assignment.g_LoadingStartTimeInMin + pAgentSimu->departure_time_in_min;
										for (int nt = 0 + virtual_link_delta; nt < pAgentSimu->path_link_seq_no_vector.size() + 1 - virtual_link_delta; ++nt)
										{
											double time_in_min = 0;

											if (nt < pAgentSimu->path_link_seq_no_vector.size() - virtual_link_delta)
												time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->m_veh_link_arrival_time_in_simu_interval[nt] * number_of_seconds_per_interval / 60.0;
											else
												time_in_min = assignment.g_LoadingStartTimeInMin + pAgentSimu->m_veh_link_departure_time_in_simu_interval[nt - 1] * number_of_seconds_per_interval / 60.0;  // last link in the path

											path_time_vector[nt - virtual_link_delta] = time_in_min;
										}

										float vehicle_travel_time = pAgentSimu->path_travel_time_in_min;


										header.agent_id = pAgentSimu->agent_id;
										header.o_zone_id = g_zone_vector[orig].zone_id;
										header.d_zone_id = g_zone_vector[dest].zone_id;
										header.path_id = it->second.path_seq_no + 1;

										header.impacted_flag = pAgentSimu->impacted_flag; // assignment.g_AgentTypeVector[at].display_code_no;
										header.info_receiving_flag = pAgentSimu->info_receiving_flag; // assignment.g_AgentTypeVector[at].display_code_no;
										header.diverted_flag = pAgentSimu->diverted_flag; // assignment.g_AgentTypeVector[at].display_code_no;

										header.agent_type_no = pAgentSimu->agent_type_no; // assignment.g_AgentTypeVector[at].agent_type_no;
										header.PCE_unit = pAgentSimu->PCE_unit_size; // pAgentSimu->PCE_unit_size;
										header.demand_period = tau;
										header.toll = path_toll;
										header.travel_time = vehicle_travel_time;
										header.distance_km = path_distance_km;
										header.distance_mile = path_distance_mile;
										header.node_size = pAgentSimu->path_link_seq_no_vector.size() - virtual_link_delta;
										header.link_size = pAgentSimu->path_link_seq_no_vector.size() + 1 - virtual_link_delta;

										fwrite(&header, sizeof(struct STrajectoryHeader), 1, g_pFilePathMOE);

										/* Format and print various data */

										for (int ni = 0 + virtual_link_delta; ni < pAgentSimu->path_link_seq_no_vector.size(); ++ni)
										{
											int link_seq_no = pAgentSimu->path_link_seq_no_vector[ni];
											if (link_seq_no < 0)
											{
												int i_error = 1;
												continue;
											}

											path_node_vector[ni - virtual_link_delta] = g_node_vector[g_link_vector[pAgentSimu->path_link_seq_no_vector[ni]].from_node_seq_no].node_id;
										}

										fwrite(&path_node_vector, sizeof(int), header.node_size, g_pFilePathMOE);
										fwrite(&path_time_vector, sizeof(double), header.link_size, g_pFilePathMOE);


										count++;
									}
								}
							}
						}
					}
				}
			}
		}

		end_t = clock();
		iteration_t = end_t - start_t;
		dtalog.output() << "Comlete writing " << count / 1000 << "K binary agents with CPU time " << iteration_t / 1000.0 << " s." << endl;
		fclose(g_pFilePathMOE);
	}

}

void g_output_simulation_result(Assignment& assignment)
{
	//    g_output_dynamic_link_performance(assignment, 2);
	g_output_TD_link_performance(assignment, 1);
	g_output_dynamic_link_state(assignment, 1);
	g_output_trajectory_bin(assignment);
	//g_output_simulation_agents(assignment);

	g_output_trajectory_csv(assignment);
	g_OutputSummaryFiles(assignment);
}

void g_OutputModelFiles(int mode)
{
	if (mode == 1)
	{
		FILE* g_pFileModelNode = fopen("model_node.csv", "w");

		if (g_pFileModelNode != NULL)
		{
			fprintf(g_pFileModelNode, "node_id,node_no,node_type,is_boundary,#_of_outgoing_nodes,activity_node_flag,agent_type,zone_id,cell_code,info_zone_flag,x_coord,y_coord\n");
			for (int i = 0; i < g_node_vector.size(); i++)
			{


				if (g_node_vector[i].node_id >= 0)  //for all physical links
				{

					fprintf(g_pFileModelNode, "%d,%d,%s,%d,%d,%d,%s, %ld,%s,%d,%f,%f\n",
						g_node_vector[i].node_id,
						g_node_vector[i].node_seq_no,
						g_node_vector[i].node_type.c_str(),
						g_node_vector[i].is_boundary,
						g_node_vector[i].m_outgoing_link_seq_no_vector.size(),
						g_node_vector[i].is_activity_node,
						g_node_vector[i].agent_type_str.c_str(),

						g_node_vector[i].zone_org_id,
						g_node_vector[i].cell_str.c_str(),
						0,
						g_node_vector[i].x,
						g_node_vector[i].y
					);

				}

			}

			fclose(g_pFileModelNode);
		}
		else
		{
			dtalog.output() << "Error: File model_node.csv cannot be opened.\n It might be currently used and locked by EXCEL." << endl;
			g_program_stop();


		}

	}

	if (mode == 2)
	{
		FILE* g_pFileModelLink = fopen("model_link.csv", "w");

		if (g_pFileModelLink != NULL)
		{
			fprintf(g_pFileModelLink, "link_id,link_no,from_node_id,to_node_id,link_type,link_type_name,lanes,link_distance_VDF,free_speed,cutoff_speed,fftt,capacity,allow_uses,");
			fprintf(g_pFileModelLink, "BPR_plf,BPR_alpha,BPR_beta,QVDF_qdf,QVDF_alpha,QVDF_beta,QVDF_cd,QVDF_n,");
			fprintf(g_pFileModelLink, "geometry\n");

			//VDF_fftt1,VDF_cap1,VDF_alpha1,VDF_beta1
			for (int i = 0; i < g_link_vector.size(); i++)
			{

				fprintf(g_pFileModelLink, "%s,%d,%d,%d,%d,%s,%d,%f,%f,%f,%f,%f,%s,%f,%f,%f,%f,%f,%f,%f,%f,",
					g_link_vector[i].link_id.c_str(),
					g_link_vector[i].link_seq_no,
					g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
					g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
					g_link_vector[i].link_type,
					g_link_vector[i].link_type_name.c_str(),
					g_link_vector[i].number_of_lanes,
					g_link_vector[i].link_distance_VDF,
					g_link_vector[i].free_speed,
					g_link_vector[i].v_congestion_cutoff,
					g_link_vector[i].free_flow_travel_time_in_min,
					g_link_vector[i].lane_capacity,
					g_link_vector[i].VDF_period[0].allowed_uses.c_str(),
					g_link_vector[i].VDF_period[0].peak_load_factor,
					g_link_vector[i].VDF_period[0].alpha,
					g_link_vector[i].VDF_period[0].beta,

					g_link_vector[i].VDF_period[0].queue_demand_factor,
					g_link_vector[i].VDF_period[0].Q_alpha,
					g_link_vector[i].VDF_period[0].Q_beta,
					g_link_vector[i].VDF_period[0].Q_cd,
					g_link_vector[i].VDF_period[0].Q_n
				);

				if (g_link_vector[i].geometry.size() > 0)
				{
					fprintf(g_pFileModelLink, "\"%s\",", g_link_vector[i].geometry.c_str());
				}
				else
				{
					fprintf(g_pFileModelLink, "\"LINESTRING (");

					fprintf(g_pFileModelLink, "%f %f,", g_node_vector[g_link_vector[i].from_node_seq_no].x, g_node_vector[g_link_vector[i].from_node_seq_no].y);
					fprintf(g_pFileModelLink, "%f %f", g_node_vector[g_link_vector[i].to_node_seq_no].x, g_node_vector[g_link_vector[i].to_node_seq_no].y);

					fprintf(g_pFileModelLink, ")\"");
				}

				fprintf(g_pFileModelLink, "\n");

			}


			fclose(g_pFileModelLink);
		}
		else
		{
			dtalog.output() << "Error: File model_link.csv cannot be opened.\n It might be currently used and locked by EXCEL." << endl;
			g_program_stop();

		}

	}
	if (mode == 3)
	{
		int connector_count = 0;
		for (int i = 0; i < g_link_vector.size(); i++)
		{
			if (g_link_vector[i].link_type == 1000)  // connector
			{
				connector_count += 1;
			}
		}

		if (connector_count >= 1)
		{
			FILE* g_pFileModelLink = fopen("access_link.csv", "w");

			if (g_pFileModelLink != NULL)
			{
				fprintf(g_pFileModelLink, "link_id,link_no,from_node_id,to_node_id,link_type,link_type_name,lanes,link_distance_VDF,free_speed,fftt,capacity,allow_uses,geometry\n");

				//VDF_fftt1,VDF_cap1,VDF_alpha1,VDF_beta1
				for (int i = 0; i < g_link_vector.size(); i++)
				{
					if (g_link_vector[i].link_type == 1000)  // connector
					{

						fprintf(g_pFileModelLink, "%s,%d,%d,%d,%d,%s,%d,%f,%f,%f,%f,%s,",
							g_link_vector[i].link_id.c_str(),
							g_link_vector[i].link_seq_no,
							g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
							g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
							g_link_vector[i].link_type,
							g_link_vector[i].link_type_name.c_str(),
							g_link_vector[i].number_of_lanes,
							g_link_vector[i].link_distance_VDF,
							g_link_vector[i].free_speed,
							g_link_vector[i].free_flow_travel_time_in_min,
							g_link_vector[i].lane_capacity,
							g_link_vector[i].VDF_period[0].allowed_uses.c_str()
							//g_link_vector[i].VDF_period[0].FFTT,
						   //g_link_vector[i].VDF_period[0].period_capacity,
						   //g_link_vector[i].VDF_period[0].alpha,
						   //g_link_vector[i].VDF_period[0].beta,
						);

						if (g_link_vector[i].geometry.size() > 0)
						{
							fprintf(g_pFileModelLink, "\"%s\",", g_link_vector[i].geometry.c_str());
						}
						else
						{
							fprintf(g_pFileModelLink, "\"LINESTRING (");

							fprintf(g_pFileModelLink, "%f %f,", g_node_vector[g_link_vector[i].from_node_seq_no].x, g_node_vector[g_link_vector[i].from_node_seq_no].y);
							fprintf(g_pFileModelLink, "%f %f", g_node_vector[g_link_vector[i].to_node_seq_no].x, g_node_vector[g_link_vector[i].to_node_seq_no].y);

							fprintf(g_pFileModelLink, ")\"");
						}

						fprintf(g_pFileModelLink, "\n");

					}


				}

				fclose(g_pFileModelLink);
			}
			else
			{
				dtalog.output() << "Error: File access_link.csv cannot be opened.\n It might be currently used and locked by EXCEL." << endl;
				g_program_stop();

			}

		}

	}

	if (mode == 3)  // cell
	{
		//FILE* g_pFileZone = nullptr;
		//g_pFileZone = fopen("model_cell.csv", "w");

		//if (g_pFileZone == NULL)
		//{
		//    cout << "File model_cell.csv cannot be opened." << endl;
		//    g_program_stop();
		//}
		//else
		//{


		//    fprintf(g_pFileZone, "cell_code,geometry\n");

		//    std::map<string, CInfoCell>::iterator it;

		//    for (it = g_info_cell_map.begin(); it != g_info_cell_map.end(); ++it)
		//    {

		//        fprintf(g_pFileZone, "%s,", it->first.c_str());
		//        fprintf(g_pFileZone, "\"LINESTRING (");

		//        for (int s = 0; s < it->second.m_ShapePoints.size(); s++)
		//        {
		//            fprintf(g_pFileZone, "%f %f,", it->second.m_ShapePoints[s].x, it->second.m_ShapePoints[s].y);
		//        }

		//        fprintf(g_pFileZone, ")\"");
		//        fprintf(g_pFileZone, "\n");
		//    }
		//    fclose(g_pFileZone);
		//}
	}

	    if (mode == 10)
	    {
	        FILE* g_pFileModel_LC = fopen("model_shortest_path_tree.csv", "w");
	
	        if (g_pFileModel_LC != NULL)
	        {
	            fprintf(g_pFileModel_LC, "iteration,agent_type,zone_id,node_id,d_zone_id,connected_flag,pred,label_cost,pred_link_cost,x_coord,y_coord,geometry\n");
	            for (int i = 0; i < g_node_vector.size(); i++)
	            {

	
	//                if (g_node_vector[i].node_id >= 0)  //for all physical links
	                {
	                    std::map<string, float> ::iterator it;
	
	                    for (it = g_node_vector[i].label_cost_per_iteration_map.begin(); it != g_node_vector[i].label_cost_per_iteration_map.end(); ++it)
	                    {
	                        int node_pred_id = -1;
	                        int pred_no = g_node_vector[i].pred_per_iteration_map[it->first];
							float pred_link_cost = 0;
	                        if (pred_no >= 0)
							{
								std::map<string, float> ::iterator it2;

								float pred_node_label_cost = 0;
								for (it2 = g_node_vector[pred_no].label_cost_per_iteration_map.begin(); it2 != g_node_vector[pred_no].label_cost_per_iteration_map.end(); ++it2)
								{
									pred_node_label_cost = it2->second;
								}

								pred_link_cost = it->second - pred_node_label_cost;
								node_pred_id = g_node_vector[pred_no].node_id;

							}
	                        int d_zone_id = g_node_vector[i].zone_id;
	                        int connected_flag = 0;
	
	                        if (it->second < 100000)
	                            connected_flag = 1;
	
							if(connected_flag==1)
	                        {
	                        fprintf(g_pFileModel_LC, "%s,%d,%d,%d,%f,%f,%f,%f,", it->first.c_str(), d_zone_id, connected_flag, node_pred_id, it->second, 
								pred_link_cost,
								g_node_vector[i].x, g_node_vector[i].y);

							int pred_no_2 = i;
							if (pred_no >= 0)
								pred_no_2 = pred_no;

							fprintf(g_pFileModel_LC, "\"LINESTRING (");
								fprintf(g_pFileModel_LC, "%f %f,", g_node_vector[i].x, g_node_vector[i].y);

								fprintf(g_pFileModel_LC, "%f %f", g_node_vector[pred_no_2].x, g_node_vector[pred_no_2].y);

							fprintf(g_pFileModel_LC, ")\"");

							fprintf(g_pFileModel_LC, "\n");
							}
	                    }
	
	                }
	
	            }
	
	            fclose(g_pFileModel_LC);
	        }
	        else
	        {
	            dtalog.output() << "Error: File model_label_cost_tree.csv cannot be opened.\n It might be currently used and locked by EXCEL." << endl;
	            g_program_stop();
	
	
	        }
	
	    }

	FILE* g_pFileModel_LC = fopen("model_RT_shortest_tree.csv", "w");

	if (g_pFileModel_LC != NULL)
	{
		fprintf(g_pFileModel_LC, "demand_period,agent_type,d_zone_id,node_id,o_zone_id,connected_flag,pred,label_cost,geometry\n");
		for (int i = 0; i < g_node_vector.size(); i++)
		{
			//                if (g_node_vector[i].node_id >= 0)  //for all physical links
			{
				std::map<string, float> ::iterator it;

				for (it = g_node_vector[i].label_cost_RT_map.begin(); it != g_node_vector[i].label_cost_RT_map.end(); ++it)
				{
					int node_pred_id = -1;
					int pred_no = g_node_vector[i].pred_RT_map[it->first];
					if (pred_no >= 0)
						node_pred_id = g_node_vector[pred_no].node_id;
					int o_zone_id = g_node_vector[i].zone_id;
					int connected_flag = 0;

					if (it->second < 1000)  // feasible link only
					{
						connected_flag = 1;
						fprintf(g_pFileModel_LC, "%s,%d,%d,%d,%f,", it->first.c_str(), o_zone_id, connected_flag, node_pred_id, it->second);

						if (node_pred_id >= 0)
						{
							fprintf(g_pFileModel_LC, "\"LINESTRING (");
							fprintf(g_pFileModel_LC, "%f %f,", g_node_vector[i].x, g_node_vector[i].y);
							fprintf(g_pFileModel_LC, "%f %f,", g_node_vector[pred_no].x, g_node_vector[pred_no].y);
							fprintf(g_pFileModel_LC, ")\"");
							fprintf(g_pFileModel_LC, ",");
						}

						fprintf(g_pFileModel_LC, "\n");
					}
				}

			}

		}

		fclose(g_pFileModel_LC);
	}
	else
	{
		dtalog.output() << "Error: File model_RT_shortest_tree.csv cannot be opened.\n It might be currently used and locked by EXCEL." << endl;
		g_program_stop();
	}
}
