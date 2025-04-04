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

void Assignment::GenerateDefaultMeasurementData()
{
	// step 1: read measurement.csv
	CCSVParser parser_measurement;
	if (parser_measurement.OpenCSVFile("measurement.csv", false))
	{
		parser_measurement.CloseCSVFile();
		return;
	}


	FILE* g_pFileModelLink = fopen("measurement.csv", "w");

	if (g_pFileModelLink != NULL)
	{
		fprintf(g_pFileModelLink, "measurement_id,measurement_type,o_zone_id,d_zone_id,from_node_id,to_node_id,count1,upper_bound_flag1,notes\n");
		//83	link			1	3	5000
		int measurement_id = 1;
		int sampling_rate = g_link_vector.size() / 100 + 1;
		for (int i = 0; i < g_link_vector.size(); i++)
		{
			if (i % sampling_rate == 0 && g_link_vector[i].lane_capacity < 2500 && g_link_vector[i].link_type >= 1)
			{

				fprintf(g_pFileModelLink, "%d,link,,,%d,%d,%f,0,generated from preprocssing based on 1/3 of link capacity\n",
					measurement_id++,
					g_node_vector[g_link_vector[i].from_node_seq_no].node_id,
					g_node_vector[g_link_vector[i].to_node_seq_no].node_id,
					g_link_vector[i].lane_capacity * g_link_vector[i].number_of_lanes * 0.3333);
			}
		}

		fclose(g_pFileModelLink);
	}

}
// updates for OD re-generations
void Assignment::Demand_ODME(int OD_updating_iterations, int sensitivity_analysis_iterations)
{
	if (OD_updating_iterations >= 1)
	{
		GenerateDefaultMeasurementData();
		// step 1: read measurement.csv
		CCSVParser parser_measurement;

		if (parser_measurement.OpenCSVFile("measurement.csv", true))
		{
			while (parser_measurement.ReadRecord())  // if this line contains [] mark, then we will also read field headers.
			{
				string measurement_type;
				parser_measurement.GetValueByFieldName("measurement_type", measurement_type);

				if (measurement_type == "link")
				{
					int from_node_id;
					if (!parser_measurement.GetValueByFieldName("from_node_id", from_node_id))
						continue;

					int to_node_id;
					if (!parser_measurement.GetValueByFieldName("to_node_id", to_node_id))
						continue;

					// add the to node id into the outbound (adjacent) node list
					if (g_node_id_to_seq_no_map.find(from_node_id) == assignment.g_node_id_to_seq_no_map.end())
					{
						dtalog.output() << "Error: from_node_id " << from_node_id << " in file measurement.csv is not defined in node.csv." << endl;
						//has not been defined
						continue;
					}
					if (g_node_id_to_seq_no_map.find(to_node_id) == assignment.g_node_id_to_seq_no_map.end())
					{
						dtalog.output() << "Error: to_node_id " << to_node_id << " in file measurement.csv is not defined in node.csv." << endl;
						//has not been defined
						continue;
					}

					float count = -1;

					for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
					{
						int upper_bound_flag = 0;
						{
							int demand_period_id = assignment.g_DemandPeriodVector[tau].demand_period_id;

							if (assignment.g_DemandPeriodVector[tau].number_of_demand_files == 0)
								continue;

							char VDF_field_name[50];
							sprintf(VDF_field_name, "count%d", demand_period_id);

							parser_measurement.GetValueByFieldName(VDF_field_name, count, true);


							sprintf(VDF_field_name, "upper_bound_flag%d", demand_period_id);
							parser_measurement.GetValueByFieldName(VDF_field_name, upper_bound_flag, true);

						}
						// map external node number to internal node seq no.
						int internal_from_node_seq_no = assignment.g_node_id_to_seq_no_map[from_node_id];
						int internal_to_node_seq_no = assignment.g_node_id_to_seq_no_map[to_node_id];

						if (g_node_vector[internal_from_node_seq_no].m_to_node_2_link_seq_no_map.find(internal_to_node_seq_no) != g_node_vector[internal_from_node_seq_no].m_to_node_2_link_seq_no_map.end())
						{
							int link_seq_no = g_node_vector[internal_from_node_seq_no].m_to_node_2_link_seq_no_map[internal_to_node_seq_no];
							if (g_link_vector[link_seq_no].VDF_period[tau].obs_count >= 1)  // data exist
							{
								if (upper_bound_flag == 0)
								{  // over write only if the new data are acutal counts, 

									g_link_vector[link_seq_no].VDF_period[tau].obs_count = count;
									g_link_vector[link_seq_no].VDF_period[tau].upper_bound_flag = upper_bound_flag;
								}
								else  // if the new data are upper bound, skip it and keep the actual counts 
								{
									// do nothing 
								}


							}
							else
							{
								g_link_vector[link_seq_no].VDF_period[tau].obs_count = count;
								g_link_vector[link_seq_no].VDF_period[tau].upper_bound_flag = upper_bound_flag;
							}
						}
						else
						{
							dtalog.output() << "Error: Link " << from_node_id << "->" << to_node_id << " in file timing.csv is not defined in link.csv." << endl;
							continue;
						}
					}

					if (measurement_type == "production")
					{
						int o_zone_id;
						if (!parser_measurement.GetValueByFieldName("o_zone_id", o_zone_id))
							continue;

						if (g_zoneid_to_zone_seq_no_mapping.find(o_zone_id) != g_zoneid_to_zone_seq_no_mapping.end())
						{
							float obs_production = -1;
							if (parser_measurement.GetValueByFieldName("count", obs_production))
							{
								g_zone_vector[g_zoneid_to_zone_seq_no_mapping[o_zone_id]].obs_production = obs_production;
							}
						}
					}

					if (measurement_type == "attraction")
					{
						int o_zone_id;
						if (!parser_measurement.GetValueByFieldName("d_zone_id", o_zone_id))
							continue;

						if (g_zoneid_to_zone_seq_no_mapping.find(o_zone_id) != g_zoneid_to_zone_seq_no_mapping.end())
						{
							float obs_attraction = -1;
							if (parser_measurement.GetValueByFieldName("count", obs_attraction))
							{
								g_zone_vector[g_zoneid_to_zone_seq_no_mapping[o_zone_id]].obs_attraction = obs_attraction;
							}
						}
					}
				}

			}

			parser_measurement.CloseCSVFile();
		}

		// step 1: input the measurements of
		// Pi
		// Dj
		// link l

		// step 2: loop for adjusting OD demand
		double prev_gap = 9999999;
		for (int s = 0; s < OD_updating_iterations; ++s)
		{
			float total_gap = 0;
			float total_relative_gap = 0;
			float total_system_travel_cost = 0;
			//step 2.1
			// we can have a recursive formulat to reupdate the current link volume by a factor of k/(k+1),
			// and use the newly generated path flow to add the additional 1/(k+1)
			double system_gap = 0;

			double gap = g_reset_and_update_link_volume_based_on_ODME_columns(g_link_vector.size(), s, system_gap);
			//step 2.2: based on newly calculated path volumn, update volume based travel time, and update volume based measurement error/deviation
					// and use the newly generated path flow to add the additional 1/(k+1)

			double gap_improvement = gap - prev_gap;

			//if (s >= 5 && gap_improvement > 0.01)  // convergency criterion  // comment out to maintain consistency 
			//    break;

			//if(s == OD_updating_iterations - sensitivity_analysis_iterations)
			//{
			//    //sensitivity analysis is activated SA iterations right before the end of entire ODME process to keep the route choice factors into account
			//    // e.g. ODME 200: SA = 10: then the lane change occurs exactly once at iteration 200-10 = 190, the route choice change still happens from 190, 191,192, till 200
			//    for (int i = 0; i < g_link_vector.size(); ++i)
			//    {
			//        for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
			//        {
			//            // used in travel time calculation
			//            if (g_link_vector[i].VDF_period[tau].sa_lanes_change != 0)  // we 
			//            {
			//                g_link_vector[i].VDF_period[tau].nlanes += g_link_vector[i].VDF_period[tau].sa_lanes_change; // apply the lane changes 
			//            }

			//        }
			//    }
			//}
			prev_gap = gap;

			int column_pool_counts = 0;
			int column_path_counts = 0;
			int column_pool_with_sensor_counts = 0;
			int column_path_with_sensor_counts = 0;

			//step 3: calculate shortest path at inner iteration of column flow updating
#pragma omp parallel for
			for (int orig = 0; orig < g_zone_vector.size(); ++orig)  // o
			{
				CColumnVector* p_column_pool;
				std::map<int, CColumnPath>::iterator it, it_begin, it_end;
				int column_vector_size;
				int path_seq_count = 0;

				float path_toll = 0;
				float path_gradient_cost = 0;
				float path_distance = 0;
				float path_travel_time = 0;

				int link_seq_no;

				float total_switched_out_path_volume = 0;

				float step_size = 0;
				float previous_path_volume = 0;

				for (int dest = 0; dest < g_zone_vector.size(); ++dest) //d
				{
					for (int at = 0; at < assignment.g_AgentTypeVector.size(); ++at)  //at
					{
						for (int tau = 0; tau < assignment.g_DemandPeriodVector.size(); ++tau)  //tau
						{
							p_column_pool = &(assignment.g_column_pool[orig][dest][at][tau]);
							if (p_column_pool->od_volume > 0)
							{
								column_pool_counts++;

								column_vector_size = p_column_pool->path_node_sequence_map.size();
								path_seq_count = 0;

								it_begin = p_column_pool->path_node_sequence_map.begin();
								it_end = p_column_pool->path_node_sequence_map.end();

								//stage 1: least cost 
								double least_cost = 999999;
								int least_cost_path_seq_no = -1;
								int least_cost_path_node_sum_index = -1;
								path_seq_count = 0;

								it_begin = p_column_pool->path_node_sequence_map.begin();
								it_end = p_column_pool->path_node_sequence_map.end();
								for (it = it_begin; it != it_end; ++it)
								{
									path_toll = 0;
									path_distance = 0;
									path_travel_time = 0;

									for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
									{
										link_seq_no = it->second.path_link_vector[nl];
										path_toll += g_link_vector[link_seq_no].VDF_period[tau].toll[at];
										path_distance += g_link_vector[link_seq_no].link_distance_VDF;
										double link_travel_time = g_link_vector[link_seq_no].travel_time_per_period[tau];
										path_travel_time += link_travel_time;
									}

									it->second.path_toll = path_toll;
									it->second.path_travel_time = path_travel_time;


									if (path_travel_time < least_cost)
									{
										least_cost = path_travel_time;
										least_cost_path_seq_no = it->second.path_seq_no;
										least_cost_path_node_sum_index = it->first;
									}
								}


								//stage 2: deviation based on observation
								int i = 0;
								for (it = it_begin; it != it_end; ++it, ++i) // for each k
								{

									column_path_counts++;

									//if (s >= 1 && it->second.measurement_flag == 0)  // after 1  iteration, if there are no data passing through this path column. we will skip it in the ODME process
									//    continue;

									it->second.UE_gap = (it->second.path_travel_time - least_cost);
									path_gradient_cost = 0;
									path_distance = 0;
									path_travel_time = 0;
									p_column_pool->m_passing_sensor_flag = 0;
									// step 3.1 origin production flow gradient

									// est_production_dev = est_production - obs_production;
									// requirement: when equality flag is 1,
									if (g_zone_vector[orig].obs_production > 0)
									{
										if (g_zone_vector[orig].obs_production_upper_bound_flag == 0)
											path_gradient_cost += g_zone_vector[orig].est_production_dev;

										if (g_zone_vector[orig].obs_production_upper_bound_flag == 1 && g_zone_vector[orig].est_production_dev > 0)
											/*only if est_production is greater than obs value , otherwise, do not apply*/
											path_gradient_cost += g_zone_vector[orig].est_production_dev;
										p_column_pool->m_passing_sensor_flag += 1;
										it->second.measurement_flag = 1;

									}

									// step 3.2 destination attraction flow gradient

									if (g_zone_vector[dest].obs_attraction > 0)
									{
										if (g_zone_vector[orig].obs_attraction_upper_bound_flag == 0)
											path_gradient_cost += g_zone_vector[dest].est_attraction_dev;

										if (g_zone_vector[orig].obs_attraction_upper_bound_flag == 1 && g_zone_vector[dest].est_attraction_dev > 0)
											path_gradient_cost += g_zone_vector[dest].est_attraction_dev;

										p_column_pool->m_passing_sensor_flag += 1;
										it->second.measurement_flag = 1;
									}

									float est_count_dev = 0;
									for (int nl = 0; nl < it->second.m_link_size; ++nl)  // arc a
									{
										// step 3.3 link flow gradient
										link_seq_no = it->second.path_link_vector[nl];
										if (g_link_vector[link_seq_no].VDF_period[tau].obs_count >= 1)
										{
											if (g_link_vector[link_seq_no].VDF_period[tau].upper_bound_flag == 0)
											{
												path_gradient_cost += g_link_vector[link_seq_no].VDF_period[tau].est_count_dev;
												est_count_dev += g_link_vector[link_seq_no].VDF_period[tau].est_count_dev;
											}

											if (g_link_vector[link_seq_no].VDF_period[tau].upper_bound_flag == 1 && g_link_vector[link_seq_no].VDF_period[tau].est_count_dev > 0)
											{// we only consider the over capaity value here to penalize the path flow 
												path_gradient_cost += g_link_vector[link_seq_no].VDF_period[tau].est_count_dev;
												est_count_dev += g_link_vector[link_seq_no].VDF_period[tau].est_count_dev;
											}
											p_column_pool->m_passing_sensor_flag += 1;
											it->second.measurement_flag = 1;
										}
									}

									// statistics collection 

									if (it->second.measurement_flag >= 1)
										column_path_with_sensor_counts++;

									it->second.path_gradient_cost = path_gradient_cost;

									step_size = 0.05;
									float prev_path_volume = it->second.path_volume;
									double weight_of_measurements = 1;  // ad hoc weight on the measurements with respect to the UE gap// because unit of UE gap  is around 1-5 mins, measurement error is around 100 vehicles per hour per lane

									double change = step_size * (weight_of_measurements * it->second.path_gradient_cost + (1 - weight_of_measurements) * it->second.UE_gap);

									//                                dtalog.output() <<" path =" << i << ", gradient cost of measurements =" << it->second.path_gradient_cost << ", UE gap=" << it->second.UE_gap << endl;

									float change_lower_bound = it->second.path_volume * 0.1 * (-1);
									float change_upper_bound = it->second.path_volume * 0.1;

									// reset
									if (change < change_lower_bound)
										change = change_lower_bound;

									// reset
									if (change > change_upper_bound)
										change = change_upper_bound;

									it->second.path_volume = max(1.0, it->second.path_volume - change);

									if (dtalog.log_odme() == 1)
									{
										dtalog.output() << "OD " << orig << "-> " << dest << " path id:" << i << ", prev_vol"
											<< prev_path_volume << ", gradient_cost = " << it->second.path_gradient_cost
											<< " UE gap," << it->second.UE_gap
											<< " link," << g_link_vector[link_seq_no].VDF_period[tau].est_count_dev
											<< " P," << g_zone_vector[orig].est_production_dev
											<< " A," << g_zone_vector[orig].est_attraction_dev
											<< "proposed change = " << step_size * it->second.path_gradient_cost
											<< "actual change = " << change
											<< "new vol = " << it->second.path_volume << endl;
									}
								}  // end of loop for all paths in the column pools


							 // record adjustment results
								for (it = it_begin; it != it_end; ++it) // for each k
								{
									it->second.path_time_per_iteration_ODME_map[s] = path_travel_time;
									it->second.path_volume_per_iteration_ODME_map[s] = it->second.path_volume;
								}

								//if (p_column_pool->m_passing_sensor_flag >= 1)
								//    column_pool_with_sensor_counts++;

							}
						}
					}
				}
			}
			if (s == 0)
			{
				float percentage_of_OD_columns_with_sensors = column_pool_with_sensor_counts * 1.0 / max(1, column_pool_counts) * 100;
				float percentage_of_paths_with_sensors = column_path_with_sensor_counts * 1.0 / max(1, column_path_counts) * 100;
				dtalog.output() << "count of all column pool vectors=" << column_pool_counts << ", "
					<< "count of all paths =" << column_path_counts << ", "
					<< "count of column_pools with sensors = " << column_pool_with_sensor_counts << "(" << percentage_of_OD_columns_with_sensors << "%), "
					<< "count of column_paths with sensors = " << column_path_with_sensor_counts << " (" << percentage_of_paths_with_sensors << "%)" << endl;

			}

		}


		// post-procese link volume based on OD volumns
		// very import: noted by Peiheng and Xuesong on 01/30/2022
		double system_gap = 0;
		g_reset_and_update_link_volume_based_on_ODME_columns(g_link_vector.size(), OD_updating_iterations, system_gap);
		// we now have a consistent link-to-path volumne in g_link_vector[link_seq_no].PCE_volume_per_period[tau] 
	}

	int base_case_flag = 0; //base_case
	g_record_corridor_performance_summary(assignment, 0);
	// stage II;

	g_classification_in_column_pool(assignment);

	for (int i = 0; i < g_link_vector.size(); ++i)
	{
		for (int tau = 0; tau < assignment.g_number_of_demand_periods; ++tau)
		{
			// used in travel time calculation
			if (g_link_vector[i].VDF_period[tau].network_design_flag != 0)  // we 
			{
				g_link_vector[i].VDF_period[tau].nlanes += g_link_vector[i].VDF_period[tau].sa_lanes_change; // apply the lane changes 
			}

		}
	}



	//    stage III 
	if (assignment.g_number_of_sensitivity_analysis_iterations > 0)
	{
		g_column_pool_optimization(assignment, assignment.g_number_of_sensitivity_analysis_iterations, true);
	}
	//if (assignment.g_pFileDebugLog != NULL)
	//	fprintf(assignment.g_pFileDebugLog, "CU: iteration %d: total_gap=, %f,total_relative_gap=, %f,\n", s, total_gap, total_gap / max(0.0001, total_system_travel_cost));
}

