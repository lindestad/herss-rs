/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     powerstation.cpp
Developer:    Bernt Viggo Matheussen, Ove Arstad
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
********************************************************************************/

#include "herss.h"
#include <vector>

Powerstation::Powerstation(){
    stps = 0;
    dt   = 0;
    static_gen_efficiency   = -1.0 * NOT_INIT;
    headlosscoef            = -1.0 * NOT_INIT;
    powstat_masl            = -1.0 * NOT_INIT;
    powstat_min_discharge   = -1.0 * NOT_INIT;
    powstat_startstop       = -1.0 * NOT_INIT;
    init_Power              = -1.0 * NOT_INIT;
    aggressive_actions_cost = -1.0 * NOT_INIT;

    // WIP 
    
}

Powerstation::~Powerstation(){}

// We check if the settings for the powerstation are valid. 
void Powerstation::ValidatePowerstationSettings() {

    if (nr_generators < 1 || nr_generators > MAX_NR_GENERATORS) {

        LOG_ERR("ERROR: Number of generators in powerstation " 
            + std::to_string(int(idnr)) 
            + " (" + nodename + ") is out of bounds: " 
            + std::to_string(nr_generators));



    }

    if (headlosscoef < 0.0 || headlosscoef > VERY_LARGE_NUMBER) {
        LOG_ERR("ERROR: Head loss coefficient in powerstation " 
            + std::to_string(int(idnr)) 
            + " (" + nodename + ") is out of bounds: " 
            + std::to_string(headlosscoef));
    }

    if (powstat_masl < -1000.0 || powstat_masl > MOUNT_EVEREST_MASL) {
        LOG_ERR("ERROR: Powerstation elevation (masl) in powerstation " 
            + std::to_string(int(idnr)) 
            + " (" + nodename + ") is out of bounds: " 
            + std::to_string(powstat_masl));
    }

    if(local_energy_equivalent < 0.0 || local_energy_equivalent > VERY_LARGE_NUMBER) {

        LOG_ERR("ERROR: Local energy equivalent in powerstation " 
            + std::to_string(int(idnr)) 
            + " (" + nodename + ") is out of bounds: " 
            + std::to_string(local_energy_equivalent));
                
    }

}
////////////////////////////////////////////////////////////////
int Powerstation::Simulate(size_t t) {

    // CHANGE BY OVE: Initialize all parameters as 0.0
    this->dt     = S->dt;
    this->stps   = S->stps;
    double Q = 0.0;
    double headloss = 0.0;
    double Hbrutto = 0.0;
    double Hnetto = 0.0;
    double turbine_efficiency = 0.0;
    double P = 0.0;
    double Power = 0.0;
    double total_Q = 0.0;
    double total_Power = 0.0;
    double income = 0.0;
    double startstopCost = 0.0;

    std::vector<double> Q_gen(generators.size(), 0.0); // Flow for each generator

    // CHANGES BY OVE: Calculate flow for each generator and powerstation total flow
    for (size_t g = 0; g < generators.size(); ++g) {
        Q = 0.0;
        if (generators[g].action.size() > t) {
            Q += generators[g].action[t] * generators[g].max_discharge;
        }
        Q_gen[g] = Q;
        total_Q += Q;
    }


    // BVM May 2026. 
    // At some point revisit this to check if we hsould use the minimum discharge as a hard constrain or soft constrain. 
    // For now we use it as a soft constrain. 
    if (total_Q > 0.001 && total_Q < powstat_min_discharge) {
        LOG_WARN("WARNING: Total powerstation flow " 
            + std::to_string(total_Q) + " m3/s) below minimum discharge (" + std::to_string(powstat_min_discharge) 
            + " m3/s) at timestep " + std::to_string(t) 
            + " for node " + std::to_string(int(idnr)) + " (" + nodename + ")\n");
    }


    Hbrutto  = ((this->start_of_stp_masl + this->end_of_stp_masl)/2.0 ) - this->powstat_masl;

    
    // CHANGES BY OVE: Handle both shared and separate penstock configurations
    // SHARED PENSTOCK: All generators share the same penstock, head loss based on combined flow
    if (shared_penstock) {
        headloss = this->headlosscoef * total_Q * total_Q;
        Hnetto = Hbrutto - headloss; 
        //printf("Shared penstock: total_Q = %.3f, headloss = %.3f\n", total_Q, headloss);

        
        for (size_t g = 0; g < generators.size(); ++g) {
            Q = Q_gen[g];
            
            turbine_efficiency = generators[g].eff_curve.x2y(Q) / 100.0;
            
            if(turbine_efficiency < 0.0) {
                LOG_WARN("ERROR:  Turbine efficiency is not working properly \n");
                LOG_WARN("ERROR:  Please revisit your topology file \n");
                LOG_WARN("Generator " + std::to_string(g) + ": Q = " + std::to_string(Q));
                LOG_WARN("Shared penstock: total_Q = " + std::to_string(total_Q) + ", headloss = " + std::to_string(headloss));
                LOG_WARN("Hbrutto  = " + std::to_string(Hbrutto));
                LOG_WARN("Shared Hnetto = " + std::to_string(Hnetto));
                LOG_WARN("Generator " + std::to_string(g) + ": turbine_efficiency = " + std::to_string(turbine_efficiency));
                LOG_ERR("In timestep  t= " + std::to_string(t));
            }
            
            P = turbine_efficiency * 1000 * GRAVITY * Hnetto * Q;  // Watt
            P = P /1000000.0; // MW

            P = P * static_gen_efficiency; 
            Power = P * this->dt / 3600.0; // MWh 
            
            //if(Q < powstat_min_discharge) {
            //    Power = 0.0;
            //}

            total_Power += Power;
            income += Power * S->price[t];
        }
    } else {
        // SEPARATE PENSTOCKS: Each generator has individual penstock, head loss is calculated for each generator
        for (size_t g = 0; g < generators.size(); ++g) {
            Q = Q_gen[g];
            headloss = this->headlosscoef * Q * Q; 

            Hnetto   = Hbrutto - headloss; 
            //printf("Separate penstock: Q = %.3f, headloss = %.3f\n", Q, headloss);

            turbine_efficiency = generators[g].eff_curve.x2y(Q) / 100.0;

            
            if(turbine_efficiency < 0.0) {
                LOG_WARN("ERROR:  Turbine efficiency is not working properly \n");
                LOG_WARN("ERROR:  Please revisit your topology file \n");
                LOG_WARN("file: " + std::string(__FILE__) + "  linenr: " + std::to_string(__LINE__) + "  function: " + std::string(__FUNCTION__));
                LOG_WARN("Generator " + std::to_string(g) + ": Q = " + std::to_string(Q));
                LOG_WARN("Generator " + std::to_string(g) + ": headloss = " + std::to_string(headloss));
                LOG_WARN("Generator " + std::to_string(g) + ": headlosscoef = " + std::to_string(headlosscoef));
                LOG_WARN("Hbrutto  = " + std::to_string(Hbrutto));
                LOG_WARN("Generator " + std::to_string(g) + ": Hnetto   = " + std::to_string(Hnetto));
                LOG_WARN("Generator " + std::to_string(g) + ": turbine_efficiency = " + std::to_string(turbine_efficiency));
                LOG_ERR("In timestep  t= " + std::to_string(t));
            }
            
            P = turbine_efficiency * 1000 * GRAVITY * Hnetto * Q; // Watt
            P = P /1000000.0; // MW

            P = P * static_gen_efficiency; 
            Power = P * this->dt / 3600.0; // MWh

            //if(Q < powstat_min_discharge) {
            //    Power = 0.0;
            //}
            total_Power += Power;
            income += Power * S->price[t];
        }
    }

    // printf("%s   Q=%.3f Hbrutto=%.3f  Hnetto=%.3f  turbine_efficiency=%.3f  P=%.3f  Power=%.3f\n", this->nodename.c_str() , Q, Hbrutto, Hnetto, turbine_efficiency, P, Power);    

    // Now we check for start and stop costs
    // We penalise when starting and stopping. 
    startstopCost = 0.0;
    
    for (size_t g = 0; g < generators.size(); ++g) {
        double previous_action = 0.0;
        if (t > 0) {
            previous_action = generators[g].action[t-1];
        }
        double current_action = 0.0;
        if (generators[g].action.size() > t) {
            current_action = generators[g].action[t];
        } else {
            current_action = 0.0;
        }
        if ((previous_action > 0.01) != (current_action > 0.01)) {
            startstopCost += this->powstat_startstop / 2.0;
        }
    }

    
    // We do allow for a powerstation to be the most downstream node in the riversystem. 
    if(this->ptr_downstream_node != NULL) {
        this->ptr_downstream_node->S->up_inflow[t] += total_Q;
    }

    // Estimating the local EEKV.
    double est_eekv = 0.0;
    //printf( "POWERSTATION idnr=%d   nodename=%s    Q=%.3f  Power=%.3f \n", int(idnr), nodename.c_str() , Q, Power );
    //printf("Power = %.3f  local_energy_equivalent = %.3f\n", Power, local_energy_equivalent);
    double est_GWh = total_Power / 1000;
    double q_Mm3 = MACRO_m3s_2_Mm3(total_Q, S->dt);  // Use variable timestep
    //printf("q_Mm3 = %.3f\n", q_Mm3);

    if(total_Power > 0.0) {
        est_eekv = est_GWh / q_Mm3;
    }

    // Save timeseries 
    S->income[t]           = income;
    S->cost[t]             = startstopCost + aggressive_actions_cost;
    

    // BVM , 15 oct 2024, 
    S->profit[t]           = income - startstopCost - aggressive_actions_cost;
    S->Hnetto[t]           = Hnetto;
    S->Hbrutto[t]          = Hbrutto;
    S->Power[t]            = total_Power;
    S->EstimatedEEKV[t]    = est_eekv;
    S->startStopCost[t]    = startstopCost;
    S->adjust_cost[t]      = aggressive_actions_cost;
    S->tot_outflow[t]      = total_Q;
    S->inflow[t]           = 0.0;  

    remaining_Mm3 = 0.0;  // The powerstation can never store water.
    remaining_active_Mm3 = 0.0;

    return 0;
}
////////////////////////////////////////////////////////////////
int Powerstation::ReadNodeData(string filename) {

    ifstream myfile;
    string line;
    string keyword;
    string value;
    Line line_obj;
    size_t tmp_idnr;
    string token;

    bool inside_node = false;

    // cout << "Powerstation::ReadNodeData   nodename: " << nodename << ", idnr: " << idnr << ", nodetype: " << EnumToString(nodetype) << "\n";
    
    for (size_t i = 0; i < gc->topoparser.getLineCount(); ++i) {

        line = gc->topoparser.getLine(i);
        string tmpline = line;  // Create a copy of the line for parsing

        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        if ( keyword.compare("ENDNODE") == 0 ) {
            inside_node = false;
        }

        if ( keyword.compare("NODE") == 0 && value.compare("PSTATION") == 0 ) {
            token = line_obj.extractNextElementFromLine(&line);
            tmp_idnr = atoi(token.c_str() );

            if(tmp_idnr == idnr) {
                
                // Now we are inside the correct node, we can extract the variables. 
                // We continue to loop over the lines until we find the next node, then we stop.
                inside_node = true;

                size_t k = i + 1;  // Start looking from the next line
                while(inside_node) {

                    if (k >= gc->topoparser.getLineCount()) {
                        LOG_ERR("ERROR: Reached end of topology file (" + filename + ") while looking for node data.");
                    }

                    line = gc->topoparser.getLine(k);
    
                    string tmpline2 = line;  // Create a copy of the line for parsing
                    string keyword2 = line_obj.extractNextElementFromLine(&line);
                    string value2   = line_obj.extractNextElementFromLine(&line);

                    if(keyword2.compare("ENDNODE") == 0) {
                        inside_node = false;
                        break;
                    }

                    // DOWNLINK_IDNR -9
                    if(keyword2.compare("DOWNLINK_IDNR") == 0 ) {
                        downstream_idnr = atoi(value2.c_str());
                        if(downstream_idnr  >= 0) {
                            downstream_node_in_use = true;
                        }   
                    }

                    // STATIC_GENERATOR_EFFICIENCY 0.96
                    if(keyword2.compare("STATIC_GENERATOR_EFFICIENCY") == 0 ) {
                        static_gen_efficiency = atof(value2.c_str());
                    }

                    // HEADLOSSCOEF 0.145
                    if( keyword2.compare("HEADLOSSCOEF") == 0 ) {
                        this->headlosscoef = atof(value2.c_str());
                    }

                    // SHARED_PENSTOCK	TRUE
                    if( keyword2.compare("SHARED_PENSTOCK") == 0 ) {
                        if (value2 == "TRUE") {
                            this->shared_penstock = 1;
                        } else if (value2 == "FALSE") {
                            this->shared_penstock = 0;
                        } else {
                            LOG_ERR("SHARED_PENSTOCK must be TRUE or FALSE in topologyfile " + filename + " ERROR\n");
                        }
                    }

                    // POWSTAT_MASL 218.0
                    if(keyword2.compare("POWSTAT_MASL") == 0 ) {
                        this->powstat_masl = atof(value2.c_str());
                    }

                    // POWSTAT_MIN_DISCHARGE 0.0
                    if(keyword2.compare("POWSTAT_MIN_DISCHARGE") == 0 ) {
                        this->powstat_min_discharge = atof(value2.c_str());
                    }

                    // POWSTAT_STARTSTOP	2.0
                    if(keyword2.compare("POWSTAT_STARTSTOP") == 0 ) {
                        this->powstat_startstop = atof(value2.c_str());
                    }

                    // LOCAL_ENERGY_EQUIVALENT 0.11
                    if(keyword2.compare("LOCAL_ENERGY_EQUIVALENT") == 0 ) {
                        this->local_energy_equivalent = atof(value2.c_str());
                    }

                    // AUTO_QMIN -9999
                    if(keyword2.compare("AUTO_QMIN") == 0 ) {
                        this->auto_qmin = atof(value2.c_str());
                    }

                    // MAX_ADJUST -9999
                    if(keyword2.compare("MAX_ADJUST") == 0 ) {
                        max_adjustment_pr_day = atoi(value2.c_str());
                        if(max_adjustment_pr_day > -1) {
                            LOG_ERR("MAX_ADJUST  WORK IN PROGRESS - This feature has not been quality controlled yet.  topologyfile " + filename);
                            //value   = line_obj.extractNextElementFromLine(&line);
                            //max_adjustment_cost = atof(value.c_str());
                        }
                    }

                    //---------------------------------------------------------------------------------
                    // --------   START GENERATORS      -----------------------------------------------
                    // Multi-generator feature was implemented by Ove Arstad, summer 2025.
                    // It has been modified here to be part of a more robust topology parser/reader.
                    // BVM May, 2026
                    
                    //# Turbine efficiency curve [M3s, %]
                    //NR_GENERATORS 1

                    // For each generator we extract this section
                    // GENERATOR 0
                    // TURBINE_CURVE 10
                    // 0.00	0
                    // 1.00	50
                    // 1.72	80
                    // 2.27	90
                    // 2.79	93
                    // 3.19	93
                    // 3.47	93
                    // 3.67	92
                    // 3.79	91
                    // 4.00	88  
                    // GENERATOR_MAX_DISCHARGE 4.0

                    if (keyword2.compare("NR_GENERATORS") == 0 ) {
                         this->nr_generators = size_t(atoi(value2.c_str()));

                        if(this->nr_generators > size_t(MAX_NR_GENERATORS)) {
                            string outstr = "ERROR: Maximum " + to_string(MAX_NR_GENERATORS) 
                                + " generators allowed per powerstation (input: " 
                                + to_string(this->nr_generators) + ")";
                            LOG_ERR(outstr);
                        }
                        generators.resize(this->nr_generators);

                        
                        cout << "Found NR_GENERATORS " << this->nr_generators << endl;

                        for(size_t g = 0; g < this->nr_generators; ++g) {

                            

                            line = gc->topoparser.getLine(k+g+1);
                            cout << line << endl; 
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);

                            int gen_id = int(MAX_NR_GENERATORS) + 1;
                            if(keyword == "GENERATOR") {
                                gen_id = atoi(value.c_str());
                                cout << "Found GENERATOR " << gen_id << endl;
                            }

                            if (keyword != "GENERATOR" || size_t(gen_id) != g) {
                                LOG_ERR("ERROR: Expected GENERATOR " + to_string(g));
                            }

                            k++;
                            line = gc->topoparser.getLine(k+g+1);
                            cout << line << endl; 
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);

                            if(keyword != "TURBINE_CURVE") {
                                LOG_ERR("ERROR: Expected TURBINE_CURVE for generator " + to_string(g));
                            }
                            
                            size_t n_points = size_t(atoi(value.c_str()));
                            generators[g].turb_virkn_Q.resize(n_points);
                            generators[g].turb_virkn_psnt.resize(n_points);

                            k++;
                            for (size_t p = 0; p < n_points; ++p) {
                                line = gc->topoparser.getLine(k+p+1);
                                keyword = line_obj.extractNextElementFromLine(&line);
                                value   = line_obj.extractNextElementFromLine(&line);
                                generators[g].turb_virkn_Q[p]    = atof(keyword.c_str());
                                generators[g].turb_virkn_psnt[p] = atof(value.c_str());
                            }

                            k += n_points;

                            generators[g].eff_curve.nr_pts = n_points;
                            for (size_t p = 0; p < n_points; ++p) {
                                generators[g].eff_curve.x_points[p] = generators[g].turb_virkn_Q[p];
                                generators[g].eff_curve.y_points[p] = generators[g].turb_virkn_psnt[p];
                            }
                            generators[g].eff_curve.initializeArrays();

                            line = gc->topoparser.getLine(k+1);
                            cout << line << endl;
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);

                            if(keyword != "GENERATOR_MAX_DISCHARGE") {
                                LOG_INFO("Powerstation::ReadNodeData   nodename: " + nodename + ", idnr: " + std::to_string(int(idnr)) + ", nodetype: " + EnumToString(nodetype));
                                LOG_WARN("Powerstation::ReadNodeData   nodename: " + nodename + ", idnr: " + std::to_string(int(idnr)) + ", nodetype: " + EnumToString(nodetype));
                                LOG_ERR("ERROR: Expected GENERATOR_MAX_DISCHARGE for generator " + to_string(g));
                            }
                            generators[g].max_discharge = atof(value.c_str());
                        }
                    }   
                    // --------   END GENERATORS      -----------------------------------------------
                    //---------------------------------------------------------------------------------

                    k++; // Move to the next line for the next iteration
                }
            }
        }
    }

    return 0;
}
///////////////////////////////////////////////////////////////////////////////
int Powerstation::ReadStateFile(string filename){

    bool found_node = false;
    ifstream myfile;
    string line;
    string keyword;
    string value;
    Line line_obj;
    size_t tmp_idnr;
    string token;

    myfile.open(filename.c_str() );

    if (!myfile.is_open()) 	{
        LOG_ERR("The statefile " + filename + " could not be found/opened.");
    }

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);
            if ( keyword.compare("NODE") == 0 && value.compare("PSTATION") == 0 ) {
                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                keyword = line_obj.extractNextElementFromLine(&line);
               
                if(tmp_idnr == idnr && keyword == nodename) {
                    value   = line_obj.extractNextElementFromLine(&line);
                    // NODE PSTATION
                    init_Power = atof( value.c_str() );
                    found_node = true;
                }
            }
        }
    }
    myfile.close();
    if(!found_node) {
        //cout << "This could have been caused by indexing of your nodes." << "\n";
        //cout << "Start with zero at the top, and work your way down, to the outlet." << "\n";
        LOG_WARN("Powerstation::ReadStateFile     idnr= " + std::to_string(int(idnr)) + "  nodename=" + nodename);
        LOG_ERR("There is something wrong in the statefile " + filename);
    }
    return 0;
}
///////////////////////////////////////////////////////////////////////

// CHANGE BY OVE:  CheckWaterBalance method that uses variable timesteps
int Powerstation::CheckWaterBalance(class Herss *herss_obj) {
    double sum_inflow  = 0.0;
    double sum_outflow = 0.0;
    for(size_t t = 0; t < this->stps; t++) {
        int variable_dt = herss_obj->getDeltaT(t);
        sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , variable_dt);
        sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], variable_dt);
    }

    double waterbalance = sum_inflow - sum_outflow;

    if(WATERBALANCE_WARNINGS) {
        printf( "WATERBALANCE POWERSTATION (Variable Timesteps) idnr=%d   nodename=%s\n", int(idnr), nodename.c_str()  );
        printf("sum_inflow_Mm3    = %.6f\n", sum_inflow);
        printf("sum_outflow_Mm3   = %.6f\n", sum_outflow);
        printf("waterbalance      = %.6f\n", waterbalance);
        printf("-------------------------------------------\n");
    }

    if(abs(waterbalance) > 0.0001) {
        printf( "WATERBALANCE POWERSTATION (Variable Timesteps) idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
        sum_inflow  = 0.0;
        sum_outflow = 0.0;
        for(size_t t = 0; t < this->stps; t++) {
            int variable_dt = herss_obj->getDeltaT(t);
            sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , variable_dt);
            sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], variable_dt);

            printf("%d %d %d %d %d %.5f %.5f %.5f  actions", int(t), 
                S->year[t], S->month[t], S->day[t], S->hour[t], 
                MACRO_m3s_2_Mm3(this->S->inflow[t], variable_dt), 
                MACRO_m3s_2_Mm3(this->S->up_inflow[t], variable_dt ),
                MACRO_m3s_2_Mm3(this->S->tot_outflow[t], variable_dt));

            for (size_t g = 0; g < generators.size(); ++g) {
                printf(" g%zu=%.5f", g, generators[g].action[t]);
            }

            printf("  sum_in= %.6f  sum_out= %.6f diff= %.6f \n",
                sum_inflow, sum_outflow , sum_inflow - sum_outflow );
        }

        LOG_WARN("WATERBALANCE POWERSTATION  idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename);
        LOG_WARN("Action larger than available water?");
        LOG_WARN("sum_inflow    = " + std::to_string(sum_inflow));
        LOG_WARN("sum_outflow   = " + std::to_string(sum_outflow));
        LOG_WARN("waterbalance  = " + std::to_string(waterbalance));
        LOG_ERR("idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename);
    }

    return 0; 
}
///////////////////////////////////////////////////////////////////////
double Powerstation::GetStartWater_Mm3(void) {  // Powerstation have no storage. 
    return 0.0;
}
//------------------------------------------------------------------------
double Powerstation::GetEndWater_Mm3(void) {
    return 0.0;
}
//------------------------------------------------------------------------
int Powerstation::WriteNodeOutput(GlobalConfig *gc) {

    // CHANGES BY OVE: Added print for generator actions
    FILE *fp;
    char outfilename [100];
    char outstr [100];

    sprintf (outfilename, "%snode%lu_%s.txt", gc->outputdir.c_str() , (idnr) , nodename.c_str()  );

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        LOG_ERR("Cannot open file " + std::string(outfilename));
    }

    sprintf (outstr, "POWERSTATION node %d %s\n", int(idnr) , nodename.c_str()  );
    fprintf(fp, "%s", outstr);
    fprintf(fp, "init_Power = %.5f\n", this->init_Power);
    fprintf(fp, "penstock_config = %s\n", shared_penstock ? "SHARED" : "SEPARATE");
    fprintf(fp, "headloss_coef = %.6f\n", this->headlosscoef);

    fprintf(fp, "yyyy mm dd hh [m3/s]    [Euro/MWh] ");
    for (size_t g = 0; g < generators.size(); ++g) {
        fprintf(fp, " [fr_g%zu]", g);
    }
    fprintf(fp, " [m3/s] [m3/s] [Euro] [Euro] [m] [m] [MWh] [Euro] [Euro] [GWh/Mm3]\n");

    fprintf(fp, "yyyy mm dd hh Up_Inflow Price");
    for (size_t g = 0; g < generators.size(); ++g) {
        fprintf(fp, " Action_g%zu", g);
    }
    fprintf(fp, " tot_outflow auto_qmin income startstopCost Hnetto Hbrutto Power adjust_cost profit est_eekv\n");

    for(size_t t = 0; t < this->stps; t++) {
        fprintf(fp, "%d %d %d %d ", S->year[t], S->month[t], S->day[t], S->hour[t]);
        fprintf(fp, "%.4f %.4f", S->up_inflow[t], S->price[t]);
        for (size_t g = 0; g < generators.size(); ++g) {
            double act = (generators[g].action.size() > t) ? generators[g].action[t] : -9999.0;
            fprintf(fp, " %.4f", act);
        }
        fprintf(fp, " %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
            S->tot_outflow[t], S->auto_qmin_m3s[t], S->income[t], S->cost[t] - S->adjust_cost[t],
            S->Hnetto[t], S->Hbrutto[t], S->Power[t], S->adjust_cost[t], S->profit[t], S->EstimatedEEKV[t]);
    }

    fclose(fp);
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////
double Powerstation::GetTunnelFLow(size_t t) {
    double flow = 0.0;
    aggressive_actions_cost = 0.0;
    S->auto_qmin_m3s[t] = 0.0;

    // Sum actions for all generators
    double total_action = 0.0;
    for (size_t g = 0; g < generators.size(); ++g) {
        if (generators[g].action.size() > t && generators[g].action[t] < -0.000001) {
            LOG_WARN("ERROR: action is negative for generator " + std::to_string(g));
            LOG_ERR("NODE PSTATION " + std::to_string(int(idnr)) + " " + nodename + " action= " + std::to_string(generators[g].action[t]));
        }
        if (generators[g].action.size() > t) {
            total_action += generators[g].action[t];
        }
    }

    if (total_action < 0.0000001) {
        flow = 0.0;
    } else {
        // Calculate flow based on individual generator max discharge values
        flow = 0.0;
        for (size_t g = 0; g < generators.size(); ++g) {
            if (generators[g].action.size() > t) {
                flow += generators[g].action[t] * generators[g].max_discharge;
            }
        }
    }

    // Here we check the auto qmin water release in downstream connected Powerstation
    if (auto_qmin > 0.0 && flow < auto_qmin) {
        flow = auto_qmin;
        S->auto_qmin_m3s[t] = flow;
    }

    double Q_Mm3 = MACRO_m3s_2_Mm3(flow, S->dt);



    // We shut down production and auto_qmin if the reservoir is dry or water level below tunnel 
    // I think we should give a minor penalty when we run action to aggresively. Just so we dont get the same value in VF. 
    if (Q_Mm3 > up_res_Mm3) {
        //printf("DEBUG: Aggressive action triggered at timestep %zu for node %d (%s)\n", t, int(idnr), nodename.c_str());
        //printf("DEBUG: Q_Mm3 = %.6f, up_res_Mm3 = %.6f, flow = %.6f, S->dt = %ld\n", Q_Mm3, up_res_Mm3, flow, S->dt);
        //printf("DEBUG: Action values: ");
        //for (size_t g = 0; g < generators.size(); ++g) {
        //    if (generators[g].action.size() > t) {
        //        printf("g%zu=%.3f ", g, generators[g].action[t]);
        //    }
        //}
        //printf("\n");
        aggressive_actions_cost = (Q_Mm3 - up_res_Mm3) * 900000000.0;
        flow = 0.0;

    }

    return flow;
}
//////////////////////////////////////////////////////////////////////////////////
int Powerstation::WriteStateFile(FILE *fp) {
    fprintf (fp, "NODE PSTATION %d %s %.5f\n", int(idnr), nodename.c_str(), this->S->Power[S->stps-1]);
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////
double Powerstation::CalcAdjustmenCosts(void) {

    double prev_power = init_Power;
    double diff;
    int nr_changes_pr_day = 0;
    double sum_cost = 0.0;
    for(size_t t = 0; t < S->stps; t++) {
        diff = abs(prev_power - S->Power[t]);

        // We define a change of 0.1 MW as significant
        if(diff > 0.1) {
            nr_changes_pr_day++;
        }

        if(t > 2) {
            if((t+1) % 24 == 0) {
                if(nr_changes_pr_day > this->max_adjustment_pr_day) {
                    sum_cost += this->max_adjustment_cost;
                    S->adjust_cost[t]  = this->max_adjustment_cost;
                    S->cost[t]        += this->max_adjustment_cost;
                    S->profit[t]      -= this->max_adjustment_cost;
                } 
                nr_changes_pr_day = 0;
            }
        }
        prev_power = S->Power[t];
    }
    return sum_cost;
}
//////////////////////////////////////////////////////////////////////////////////