/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     reservoir.cpp                                                        
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:

Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************************/

#include "herss.h"
#include <limits>


Reservoir::Reservoir(){
    reservoir_init_fr              = NOT_INIT;
    reservoir_init_masl            = NOT_INIT;
    reservoir_init_Mm3             = NOT_INIT;

    res_HRW                        = NOT_INIT;
    filling_at_hrw_Mm3             = NOT_INIT;
    filling_at_hatchlevel          = NOT_INIT;

    res_LRW                        = NOT_INIT;
    filling_at_lrw_Mm3             = NOT_INIT;

    res_Mm3                        = NOT_INIT;
    res_masl                       = NOT_INIT;
    res_fr                         = NOT_INIT;
    nr_points_res_curve            = 0;
    nr_points_ovefl_curve          = 0;
    outlet_hatch_in_use            = false;
    outlet_tunnel_in_use           = false;

    minQ_hatch                     = NOT_INIT;
    maxQ_hatch                     = NOT_INIT;
    hatch_masl                     = NOT_INIT;
    
    res_penalty                    = -1.0*NOT_INIT;
    floodlevel_cost                = -1.0*NOT_INIT;

}

Reservoir::~Reservoir(){}

int Reservoir::initArrayCurves(void) {
    
    // RESERVOIR CURVE    X=MASL  ,  Y = Mm3
    ac_res_masl_2_Mm3.nr_pts = this->nr_points_res_curve;
    for (int p = 0; p < ac_res_masl_2_Mm3.nr_pts; p++){
        ac_res_masl_2_Mm3.x_points[p] = res_curve_masl[p];
        ac_res_masl_2_Mm3.y_points[p] = res_curve_Mm3[p];
    }
    ac_res_masl_2_Mm3.initializeArrays();


    //ArrayCurve ac_res_Mm3_2_masl;
    ac_res_Mm3_2_masl.nr_pts = nr_points_res_curve;
    for (int p = 0; p < ac_res_Mm3_2_masl.nr_pts; p++){
        ac_res_Mm3_2_masl.x_points[p] = res_curve_Mm3[p];
        ac_res_Mm3_2_masl.y_points[p] = res_curve_masl[p];
    }
    ac_res_Mm3_2_masl.initializeArrays();
    //---------------------------------------------------------------------------

    //--------------------------------------------------------------------
    // Specify OVERFLOW_CURVE and number of points. If not used specify with "-9999"
    ac_ovefl_masl_2_m3s.nr_pts = this->nr_points_ovefl_curve;
    for (int p = 0; p < ac_ovefl_masl_2_m3s.nr_pts; p++){
        ac_ovefl_masl_2_m3s.x_points[p] = ovefl_curve_masl[p];
        ac_ovefl_masl_2_m3s.y_points[p] = ovefl_curve_m3s[p];
    }
    ac_ovefl_masl_2_m3s.initializeArrays();

    //ArrayCurve ac_ovefl_m3s_2_masl;
    ac_ovefl_m3s_2_masl.nr_pts = this->nr_points_ovefl_curve;
    for (int p = 0; p < ac_ovefl_m3s_2_masl.nr_pts; p++){
        ac_ovefl_m3s_2_masl.x_points[p] = ovefl_curve_m3s[p];
        ac_ovefl_m3s_2_masl.y_points[p] = ovefl_curve_masl[p];
    }
    ac_ovefl_m3s_2_masl.initializeArrays();

    return 0;
}
////////////////////////////////////////////////////////////////
double Reservoir::CalcOverflow() {

    double masl_start_overflow;
    double overflow_m3s;
    double overflow_Mm3;
    double current_filling;
    double max_overflow;

    overflow_m3s = 0.0;
    overflow_Mm3 = 0.0;

    masl_start_overflow = this->ovefl_curve_masl[0];

    // CHANGE BY OVE: Fast overflow calculation
    if (fast_overflow) {
        if (this->res_masl > masl_start_overflow) {
            overflow_Mm3 = this->res_Mm3 - filling_at_hrw_Mm3;
            if (overflow_Mm3 < 0.0) overflow_Mm3 = 0.0;
        }
        return overflow_Mm3;
    } else {
        // The bottom point in the overflow curve is usually the same as HRW, but not always.
        if(this->res_masl > masl_start_overflow) {
            overflow_m3s = ac_ovefl_masl_2_m3s.x2y(this->res_masl);
            overflow_Mm3 = MACRO_m3s_2_Mm3(overflow_m3s,S->dt);

            // We cannot allow the overflow to drain more than down to the top of the dam ( for now we assume HRW).
            // This has to do with numerical stability using large timesteps.
            // When the reservoir level is close to one of the points defining the reservoir- or overflow curve, we will get wrong answer
            // when the reservoirlevel drops from one point to below another one.
            // Maybe we need to use the old way of looping over all the points defining the reservoir and overflow curves.
            // WORK IN PROGRESS
            current_filling  = this->res_Mm3; 

            max_overflow = current_filling - filling_at_hrw_Mm3;

            if(overflow_Mm3 > max_overflow){
                overflow_Mm3 = max_overflow;
            }

            if(overflow_Mm3 < 0.0) {
                LOG_WARN("Negative overflow is not allowed \n");
                //double res_curve_masl[MAX_NR_POINTS_CURVE];
                //double res_curve_Mm3[MAX_NR_POINTS_CURVE];
                //size_t nr_points_res_curve;

                //double ovefl_curve_masl[MAX_NR_POINTS_CURVE];
                //double ovefl_curve_m3s[MAX_NR_POINTS_CURVE];
                //size_t nr_points_ovefl_curve;

                // printf("Reservoir curve: \n");
                // for(size_t t = 0; t < nr_points_res_curve; t++) {
                //     printf("%.4f %.4f\n", res_curve_masl[t], res_curve_Mm3[t]);
                // }

                LOG_WARN("current_filling  = " + std::to_string(this->res_Mm3)); 
                LOG_WARN("filling_at_hrw_Mm3 = " + std::to_string(filling_at_hrw_Mm3));
                LOG_WARN("masl_start_overflow= " + std::to_string(masl_start_overflow));
                LOG_WARN("overflow_Mm3 = " + std::to_string(overflow_Mm3));
                LOG_WARN("res_masl= " + std::to_string(this->res_masl));
                LOG_WARN("masl_start_overflow = " + std::to_string(masl_start_overflow));
                LOG_ERR("Node idnr = " + std::to_string(int(this->idnr)) + "   nodename = " + this->nodename );
                return -9;
            }
        }
        return overflow_Mm3;
    }
}
////////////////////////////////////////////////////////////////
void Reservoir::InitReservoir(void) {
    //printf("InitReservoir:  Node idnr = %d   nodename = %s\n ", int(this->idnr) , this->nodename.c_str() );
    for(size_t t = 0; t < S->stps; t++ ) {
        S->up_inflow[t]    = 0.0;
    }

    if(this->nr_points_res_curve < 2) {
        LOG_ERR("Reservoir curve not initialized");
    }
    
    if(this->reservoir_init_fr < -1.0) {
        LOG_ERR("ERROR Something wrong with reservoir_init_fr= " + std::to_string(this->reservoir_init_fr));
    }

    filling_at_lrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_LRW);
    filling_at_hrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_HRW);

    this->active_max_volume_Mm3 = filling_at_hrw_Mm3 - filling_at_lrw_Mm3;

    res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);

    this->reservoir_init_Mm3 = res_Mm3;
    this->reservoir_init_active_Mm3 = res_Mm3 - filling_at_lrw_Mm3;

    // Note that reservoir content is the water between HRW and LRW.
    // That volume cannot be used directly to calculate the filling in meters above sea level.
    res_masl = ac_res_Mm3_2_masl.x2y(res_Mm3);
    this->reservoir_init_masl = res_masl;

    // Calculate filling at hatch level here so do it only once, and not every timestep.
    if(outlet_hatch_in_use) {
        filling_at_hatchlevel = ac_res_masl_2_Mm3.x2y(this->hatch_masl);
    }
}
////////////////////////////////////////////////////////////////
// Check that initial settings are valid and within bounds. 
void Reservoir::ValidateReservoirSettings() {

    // The reservoir must be initialized between 0.0 and 1.5. 
    // Note that with a filling of 1.5, there is substantial flooding in the system . 
    if(this->reservoir_init_fr < -0.000001 || this->reservoir_init_fr > 1.5) {
        LOG_WARN("Warning reservoir_init_fr is out of bounds: " + std::to_string(this->reservoir_init_fr));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_HRW < 0.0 || this->res_HRW > MOUNT_EVEREST_MASL) {
        LOG_WARN("ERROR HRW is out of bounds: " + std::to_string(this->res_HRW));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_LRW < 0.0 || this->res_LRW > MOUNT_EVEREST_MASL) {
        LOG_WARN("ERROR LRW is out of bounds: " + std::to_string(this->res_LRW));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_LRW >= this->res_HRW) {
        LOG_ERR("ERROR: LRW must be lower than HRW: LRW=" + std::to_string(this->res_LRW) + ", HRW=" + std::to_string(this->res_HRW));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_penalty < 0.0 || this->res_penalty > VERY_LARGE_NUMBER) {
        LOG_WARN("ERROR: RES_PENALTY is out of bounds: " + std::to_string(this->res_penalty));
        LOG_ERR("Reservoir: " + this->nodename);
    }

    if(this->res_penalty < 0.0 || this->res_penalty > VERY_LARGE_NUMBER) {
        LOG_WARN("ERROR: RES_PENALTY must be non-negative, and not larger than VERY_LARGE_NUMBER : " + std::to_string(this->res_penalty));
        LOG_ERR("Reservoir: " + this->nodename);
        
        
    }

    if(this->nr_points_res_curve < 2 || this->nr_points_res_curve > MAX_NR_POINTS_CURVE) {
        LOG_WARN("ERROR: Number of points in reservoir curve is out of bounds: " + std::to_string(this->nr_points_res_curve));
        LOG_ERR("Reservoir: " + this->nodename);
    }
    
    if(this->nr_points_ovefl_curve < 0 || this->nr_points_ovefl_curve > MAX_NR_POINTS_CURVE) {
        LOG_WARN("ERROR: Number of points in overflow curve is out of bounds: " + std::to_string(this->nr_points_ovefl_curve));
        LOG_ERR("Reservoir: " + this->nodename);
    }


    // BVM, May 2026. 
    // WORK IN PROGRESS: Could probably do better on this. 
    // 
    // FLOODLEVEL_PENALTY 300
    // # Reservoir curve points, [masl, Mm3] Curve must go below hatch_masl.
    // RESERVOIR_CURVE 7
    // 747	0.0
    // 748	1.0
    // 749	2.37
    // 750	3.24
    // 757	10.0
    // 758	15.0
    // 760	500
    // # Overflow curve, points, downstream idnr   [masl, m3s]
    // OVERFLOW_CURVE 3 2
    // 757	0.0
    // 758	10.0
    // 760	200.0
    // FAST_OVERFLOW FALSE
    // # Outlet hatch downstream_nodeid, qmin_hatch, qmax_hatch, hatch_masl
    // OUTLET_HATCH -9999
    // OUTLET_TUNNEL 1
    // OUTLET_AUTO_QMIN -9999

}
////////////////////////////////////////////////////////////////


// We use this to check if the reservoir level is valid.
void Reservoir::ValidateReservoirLevelMm3(size_t t, double level_Mm3) {

    if(level_Mm3 > res_curve_Mm3[nr_points_res_curve-1]) {
        LOG_WARN("ERROR: Numerical instability, there is too much water in your system \n");
        LOG_WARN("Reservoir::Simulate nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype));
        LOG_ERR("To fix this add more volume at the top of you reservoir curve");
        //printf("The current reservoir level (res_Mm3=%.5f)", this->res_Mm3); 
        //printf(" is above the highest allowed level in the reservoir curve (%.5f) \n", res_curve_Mm3[nr_points_res_curve-1]);
        //printf("I suggest you set the triple of max in current simulation (%.1f)\n", 3.0* level_Mm3);
        //printf("Reservoir curve points, [masl, Mm3] \n");
        //for(size_t p = 0; p < nr_points_res_curve; p++) {
        //    printf("Point %lu: masl = %.5f  Mm3 = %.5f\n", p, res_curve_masl[p], res_curve_Mm3[p]);
        //}
    }
}
///////////////////////////////////////////////////////////////
int Reservoir::Simulate(size_t t) {


    // BVM 27 june 2025:
    // We minimize the updating of the MASL in the reservoir. 
    // the function x2y in the ArrayCurve is used alot and is time consuming, 
    // so we try to avoid unnecessary usage 

    
    // Upstream inflow has already been set to zero or adjusted earlier.
    this->dt     = S->dt;
    this->stps   = S->stps;
    double hatchflow_Mm3 = 0.0; // To void warning
    double tunnelflow_Mm3;
    double overflow_Mm3;
    double outlet_auto_qmin_flow_Mm3;
    double total_inflow_Mm3;
    double max_hatchflow;
    double current_filling;
    current_filling = -999.0; // To void warning


    #if HERSS_DEBUG_ALL
        printf("Node idnr = %d   nodename = %s\n ", int(this->idnr) , this->nodename.c_str() );
        if( S->inflow[t] < 0.0 || S->inflow[t] > 5000.0) {
            printf("Reservoir::Simulate() There is something wrong with inflow =%.3f\n", S->inflow[t]);
            printf("Node idnr = %d   nodename = %s", int(this->idnr) , this->nodename.c_str() );
            printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
            exit(EXIT_FAILURE);
        }

        if( S->price[t] < 0.0 || S->price[t] > 5000.0) {
            printf("Reservoir::Simulate() There is something wrong with price =%.3f\n", S->price[t]);
            printf("Node idnr = %d   nodename = %s", int(this->idnr) , this->nodename.c_str() );
            printf("file: %s  linenr: %d\n", __FILE__ , __LINE__);
            exit(EXIT_FAILURE);
        }
    #endif


    total_inflow_Mm3 = S->inflow[t]+S->up_inflow[t];
    total_inflow_Mm3 = MACRO_m3s_2_Mm3(total_inflow_Mm3,S->dt);

    // Add local inflow
    this->res_Mm3 += MACRO_m3s_2_Mm3(S->inflow[t],S->dt);    // Mm3

    S->sum_local_inflow_Mm3 += MACRO_m3s_2_Mm3(S->inflow[t],S->dt);    // Mm3

    // Add upstream inflow
    this->res_Mm3 += MACRO_m3s_2_Mm3(S->up_inflow[t],S->dt);  // Mm3   All initialized to zero 

    ValidateReservoirLevelMm3(t, this->res_Mm3);


    // Update filling height - Testing without updating this 27June20205, BVM
    // this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    //---------------------------------------------------------------------
    // We have maximum four outlets. Tunnel, Hatch, auto_qmin_hatch, Overflow
    // We start with TUNNEL
    // CASE A: Normal production.
    // CASE B: Auto_Qmin. 
    // CASE C: Completely empty reservoir, we shut down both A and B. 

    tunnelflow_Mm3 = 0.0;

    if(outlet_tunnel_in_use) {

        if(ptr_downstream_node_tunnel    == NULL) {
            LOG_WARN("ERROR IN RESERVOIR:  Something is wrong with the pointer:  ptr_downstream_node_tunnel \n");
            LOG_WARN("idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename + "   timestep=" + std::to_string(t));
            LOG_ERR("res_masl     = " + std::to_string(this->res_masl));
        }

        // I think we should not allow for higher pressure than up to HRW, it should never pay off to flood.
        ptr_downstream_node_tunnel->start_of_stp_masl = this->res_masl;
        if(  this->res_masl > this->res_HRW) {
            ptr_downstream_node_tunnel->start_of_stp_masl = this->res_HRW;
        }

        ptr_downstream_node_tunnel->up_res_Mm3 = this->res_Mm3;
        ptr_downstream_node_tunnel->S->dt = S->dt;
        double tunnelf_m3s = ptr_downstream_node_tunnel->GetTunnelFLow(t);
        ptr_downstream_node_tunnel->S->up_inflow[t] = tunnelf_m3s;
        tunnelflow_Mm3 = MACRO_m3s_2_Mm3(tunnelf_m3s ,S->dt);  // Mm3 
    }


    this->res_Mm3 -= tunnelflow_Mm3;
    ValidateReservoirLevelMm3(t, this->res_Mm3);
    this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);


    //-------------------------------------------------------------------
    // OUTLET HATCH
    if(outlet_hatch_in_use) {

        hatchflow_Mm3 = 0.0;
        if(this->res_masl > this->hatch_masl ) {
            // Some places we need to release water regardless of the actions set 
            // This can be done by setting minQ_hatch to a low level.
            hatchflow_Mm3 = this->minQ_hatch + S->action[t][this->idnr]*(this->maxQ_hatch - this->minQ_hatch);

            hatchflow_Mm3 = MACRO_m3s_2_Mm3(hatchflow_Mm3, S->dt);  // Mm3

            current_filling = ac_res_masl_2_Mm3.x2y(this->res_masl);

            max_hatchflow = current_filling - filling_at_hatchlevel;

            if (hatchflow_Mm3 > max_hatchflow) {
                hatchflow_Mm3 = max_hatchflow;
            }
        }
        ptr_downstream_node_hatch->S->up_inflow[t] += MACRO_Mm3_2_m3s(hatchflow_Mm3, S->dt);  // m3/s
        this->res_Mm3 -= hatchflow_Mm3;
        ValidateReservoirLevelMm3(t, this->res_Mm3);
        // Update the reservoir masl     BVM 27 June 2025
        this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);
    }



    // AUTO HATCH 
    outlet_auto_qmin_flow_Mm3 = 0.0;    
    // Here we simulate the effect of an automatic water release set by the operators.
    if(outlet_auto_qmin_in_use){
        double void_cost;
        outlet_auto_qmin_flow_Mm3 = this->qmin.calcQminRequirement(S->year[t], S->month[t], S->day[t],  &void_cost  );  // m3/s
        this->ptr_downstream_node_auto_qmin->S->up_inflow[t] += outlet_auto_qmin_flow_Mm3;
        outlet_auto_qmin_flow_Mm3 = MACRO_m3s_2_Mm3(outlet_auto_qmin_flow_Mm3, S->dt);  // Mm3
        this->res_Mm3 -= outlet_auto_qmin_flow_Mm3;
        ValidateReservoirLevelMm3(t, this->res_Mm3);
    }


    // Update the reservoir masl
    this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    if(this->res_masl < 1.0 + (-1.0 * VERY_LARGE_NUMBER)   ) {

        LOG_WARN("ERROR: Calling reservoir masl calculation (x2y) for node " 
            + nodename + " at timestep " + std::to_string(t));
        LOG_WARN("There is something wrong with the reservoir masl calculation (x2y) for node " 
            + nodename + " at timestep " + std::to_string(t));
    }


    #if HERSS_DEBUG_ALL
        printf("Node idnr = %d   nodename = %s\n ", int(this->idnr) , this->nodename.c_str() );
        printf("masl= %.4f  Mm3 = %.4f\n", this->res_masl,  this->res_Mm3);
        printf("-----------------------\n");
    #endif


    // Overflow is always used
    overflow_Mm3 = this->CalcOverflow();
    ptr_downstream_node_overflow->S->up_inflow[t] += MACRO_Mm3_2_m3s(overflow_Mm3, this->dt);  // m3/s

    this->res_Mm3 -= overflow_Mm3;
    this->res_masl = ac_res_Mm3_2_masl.x2y(this->res_Mm3);

    // There is a treshold here. We make the penalty dependent on how much below LRW we are
    // This is so we can see an improvement when we do gradient optimization.
    cost_lrw = 0.0;
    if( this->res_masl  < this->res_LRW) {
        // cost_lrw = this->res_penalty*dt/3600;    Old code
        cost_lrw = (this->res_penalty*this->dt/3600) * (this->res_LRW - this->res_masl);
    }

    if(outlet_tunnel_in_use) {
        ptr_downstream_node_tunnel->end_of_stp_masl = this->res_masl;
        // I think we should not allow for higher pressure than up to HRW, it should never pay off to flood.
        if(  this->res_masl > this->res_HRW) {
            ptr_downstream_node_tunnel->end_of_stp_masl = this->res_HRW;
        }
    }

    // Fractional_filling
    double fract_filling = (res_Mm3  - filling_at_lrw_Mm3) / (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);

    remaining_Mm3 = res_Mm3;

    // We only value water up to HRW
    remaining_active_Mm3 = fract_filling  * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);
    
    if(fract_filling > 1.0) {
        remaining_active_Mm3 = (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);
    }

    if(fract_filling < 0.0) {
        remaining_active_Mm3 = 0.0;
    }

    if(fract_filling < -1.0) {
        LOG_WARN("There is obviously something wrong with the fract_filling calculations => NON PHYSICAL SITUATIONS ");
        LOG_WARN("idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename + "   timestep=" + std::to_string(t));
        LOG_ERR("current_filling     = " + std::to_string(res_Mm3));
    }

    this->res_fr = fract_filling;

    // Transfer timeseries 
    S->tot_inflow[t]      = MACRO_Mm3_2_m3s(total_inflow_Mm3,this->dt);
    S->res_Mm3[t]         = res_Mm3;
    S->res_active_Mm3[t]  = remaining_active_Mm3;
    S->res_masl[t]        = res_masl;
    S->res_fr[t]          = fract_filling;
    S->overflow_Mm3[t]    = overflow_Mm3;
    S->cost[t]            = cost_lrw;
    S->Power[t]           = 0.0;  // No power production in reservoirs


    double tot_out        = hatchflow_Mm3 + tunnelflow_Mm3 + overflow_Mm3 + outlet_auto_qmin_flow_Mm3;
    S->tot_outflow[t]     = MACRO_Mm3_2_m3s(tot_out, this->dt);
    S->tunnelflow_m3s[t]  = MACRO_Mm3_2_m3s(tunnelflow_Mm3, this->dt);
    S->hatchflow_m3s[t]   = MACRO_Mm3_2_m3s(hatchflow_Mm3, this->dt);
    S->overflow_m3s[t]    = MACRO_Mm3_2_m3s(overflow_Mm3, this->dt);
    S->auto_qmin_m3s[t]   = MACRO_Mm3_2_m3s(outlet_auto_qmin_flow_Mm3, this->dt);
    S->income[t]  = 0.0;  // No income in reservoirs 
    S->profit[t]  = S->income[t] - S->cost[t];

    return 0;
}
////////////////////////////////////////////////////////////////
double Reservoir::GetReservoirFraction(size_t t){
    return S->res_fr[t];
}
////////////////////////////////////////////////////////////////
int Reservoir::ReadNodeData(string filename){

	ifstream myfile;
	string line;
    string keyword;
    string value;
    Line line_obj;
    size_t tmp_idnr;
    string token;

    // cout << "Reservoir::ReadNodeData      nodename: " << nodename << ", idnr: " << idnr << ", nodetype: " << EnumToString(nodetype) << "\n";
    
    // We have the whole topology file saved in the Topoparser in GlobalConfig. 
    // We first extract single value variables. 
    // Then we extract the reservoir curve and overflow curve.

    bool inside_node = false;


    // Single value variables 
    for (size_t i = 0; i < gc->topoparser.getLineCount(); ++i) {
        line = gc->topoparser.getLine(i);

        string tmpline = line;  // Create a copy of the line for parsing

        keyword = line_obj.extractNextElementFromLine(&line);
        value   = line_obj.extractNextElementFromLine(&line);

        if ( keyword.compare("ENDNODE") == 0 ) {
            inside_node = false;
        }

        if ( keyword.compare("NODE") == 0 && value.compare("RESERVOIR") == 0 ) {
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

                    if (keyword2.compare("ENDNODE") == 0) {
                        inside_node = false;
                        break;
                    }

                    // Here we can extract the variables for the reservoir, we are still inside the correct node. 
                    // We can use keyword2 and value2 to extract the variables.
                    if (keyword2.compare("HRW") == 0) {
                         this->res_HRW = atof(value2.c_str() );
                    }
                    
                    if (keyword2.compare("LRW") == 0) {
                         this->res_LRW = atof(value2.c_str() );
                    }

                    if (keyword2.compare("RES_PENALTY") == 0) {
                         this->res_penalty = atof(value2.c_str() );
                    }
                    
                    if (keyword2.compare("FLOODLEVEL_PENALTY") == 0) {
                         this->floodlevel_penalty = atof(value2.c_str() );
                    }

                    // FAST_OVERFLOW FALSE
                    if (keyword2.compare("FAST_OVERFLOW") == 0) {
                        if (value2 == "TRUE") {
                            this->fast_overflow = 1;
                        } else if (value2 == "FALSE") {
                            this->fast_overflow = 0;
                        }  else {
                            LOG_ERR("FAST_OVERFLOW must be TRUE or FALSE in topologyfile " + filename + " ERROR");
                        }                  
                    }

                    // OUTLET_HATCH -9999
                    if ( keyword2.compare("OUTLET_HATCH") == 0) {
                        // BVM, May 2026.
                        if(atoi(value2.c_str() ) >= 0) { 
                            downstream_idnr_hatch = atoi(value2.c_str());
                            outlet_hatch_in_use            = true;
                            downstream_node_in_use         = true;

                            // There should be five columns in this line
                            // OUTLET_HATCH downstream_nodeid, qmin_hatch, qmax_hatch, hatch_masl
                            int n_cols = line_obj.calcNrCols(&tmpline2);
                            if(n_cols != 5) {
                                LOG_INFO("ERROR: OUTLET_HATCH line should have 5 columns, but it has " + std::to_string(n_cols) + " columns. Check topology file " + filename);
                                LOG_INFO("Reservoir::ReadNodeData  nodename: " + nodename + ", idnr: " + std::to_string(idnr) + ", nodetype: " + EnumToString(nodetype) + "\n");
                                LOG_ERR("ERROR: OUTLET_HATCH line should have 5 columns, but it has " + std::to_string(n_cols) + " columns. Check topology file " + filename);
                            }

                            value2      = line_obj.extractNextElementFromLine(&line);
                            minQ_hatch = atof(value2.c_str());
                            value2      = line_obj.extractNextElementFromLine(&line);
                            maxQ_hatch = atof(value2.c_str());
                            value2      = line_obj.extractNextElementFromLine(&line);
                            hatch_masl = atof(value2.c_str());

                            if(hatch_masl < 0.0 || hatch_masl > MOUNT_EVEREST_MASL) {
                                LOG_WARN("ERROR: Hatch masl is out of bounds: " + std::to_string(hatch_masl));
                                LOG_ERR("Reservoir: " + this->nodename);
                            }

                            if(minQ_hatch < 0.0 || minQ_hatch > VERY_LARGE_NUMBER) {
                                LOG_WARN("ERROR: minQ_hatch is out of bounds: " + std::to_string(minQ_hatch));
                                LOG_ERR("Reservoir: " + this->nodename);
                            }

                            if(maxQ_hatch < 0.0 || maxQ_hatch > VERY_LARGE_NUMBER) {
                                LOG_WARN("ERROR: maxQ_hatch is out of bounds: " + std::to_string(maxQ_hatch));
                                LOG_ERR("Reservoir: " + this->nodename);
                            }
                        }
                    }

                    // OUTLET_TUNNEL 1
                    if ( keyword2.compare("OUTLET_TUNNEL") == 0) {
                        downstream_idnr_tunnel = atoi(value2.c_str()  );
                        if(downstream_idnr_tunnel >=0) {
                            outlet_tunnel_in_use           = true;
                            downstream_node_in_use         = true;
                        }
                    }
                    

                    // OUTLET_AUTO_QMIN -9999
                    if (keyword2.compare("OUTLET_AUTO_QMIN") == 0) {
                        // Number of timeperiods and downstream node idnr
                        // OUTLET_AUTO_QMIN 2 4
                        // 01.10 30.04	5.0
                        // 01.05 30.09	10.5
                        outlet_auto_qmin_in_use = false;

                        if(atoi(value2.c_str() ) >= 0) { 
                            cout << "Found OUTLET_AUTO_QMIN " << endl;
                            cout << "WORK IN PROGRESS - make a topofile with this setting active, then QR" << endl;
                            LOG_ERR("OUTLET_AUTO_QMIN is WORK IN PROGRESS ");

                            outlet_auto_qmin_in_use = true;
                            this->qmin.nr_periods = atoi(value2.c_str());
                            
                            // Downstream node
                            value   = line_obj.extractNextElementFromLine(&line);
                            this->downstream_idnr_auto_qmin = atoi(value.c_str());

                            // Now we read in the qmin periods (MAXIMUM 5)
                            for(int q = 0; q < this->qmin.nr_periods; q++) {

                                // getline(myfile, line);
                                line = gc->topoparser.getLine(k+q+1);
                                cout << "Reading qmin period line: " << line << endl;

                                // value   = line_obj.extractNextElementFromLine(&line);
                                // qmin.timeperiods[q].start_day = atoi(value.substr(0,2).c_str() );
                                // qmin.timeperiods[q].start_month  = atoi(value.substr(3,2).c_str() );
                        
                                // value   = line_obj.extractNextElementFromLine(&line);
                                // qmin.timeperiods[q].end_day = atoi(value.substr(0,2).c_str() );
                                // qmin.timeperiods[q].end_month  = atoi(value.substr(3,2).c_str() );

                                // value   = line_obj.extractNextElementFromLine(&line);
                                // qmin.timeperiods[q].min_discharge = atof(value.c_str() );
                                // qmin.timeperiods[q].penalty_cost = 0.0;  // This is automatic water release. We check actual qmin in channels. 

                            }
                        }
                    }
                    
                    if ( keyword2.compare("RESERVOIR_CURVE") == 0 ) {
                        this->nr_points_res_curve = atoi(value2.c_str() );
                        if( nr_points_res_curve > MAX_NR_POINTS_CURVE) {
                            LOG_ERR("ERROR: nr_points_res_curve > MAX_NR_POINTS_CURVE ");
                        }
                        for(size_t p = 0; p < nr_points_res_curve; p++) {
                            line = gc->topoparser.getLine(k+p+1);
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);
                            res_curve_masl[p] = atof(keyword.c_str());
                            res_curve_Mm3[p]  = atof(value.c_str());
                        }
                    }

                    // # Overflow curve, points, downstream idnr   [masl, m3s]
                    if (keyword2.compare("OVERFLOW_CURVE") == 0 ) {
                        token   = line_obj.extractNextElementFromLine(&line);

                        nr_points_ovefl_curve = atoi(value2.c_str());
                        if( nr_points_ovefl_curve > MAX_NR_POINTS_CURVE) {
                            LOG_ERR("nr_points_ovefl_curve > MAX_NR_POINTS_CURVE ");
                        }

                        downstream_idnr_overflow = atoi(token.c_str());
                        this->outlet_overflow_in_use = true;

                        for(size_t p = 0; p < nr_points_ovefl_curve; p++) {
                            line = gc->topoparser.getLine(k+p+1);
                            keyword = line_obj.extractNextElementFromLine(&line);
                            value   = line_obj.extractNextElementFromLine(&line);
                            ovefl_curve_masl[p] = atof(keyword.c_str());
                            ovefl_curve_m3s[p]  = atof(value.c_str());
                        }
                    }
                    
                    k++; // Move to the next line for the next iteration
                }
            }
        }
    }

    // We need to set the downstream_idnr
    // We always choose Powerstation node if it exists.
    // Every Reservoir has overflow so if we dont have tunnel we set to overflow node. 
    if(outlet_overflow_in_use) { 
        downstream_idnr = downstream_idnr_overflow;
        downstream_node_in_use = true;
    }
    if(outlet_tunnel_in_use) {
        downstream_idnr = downstream_idnr_tunnel;
        downstream_node_in_use = true;
    }
    return 0;
}
//------------------------------------------------------------------------
int Reservoir::ReadStateFile(string filename){

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
		LOG_ERR("The statefile " + filename + " could not be found/opened");
    }

    while(!myfile.eof()){
        getline(myfile, line);
        if( line.length()  > 0 && ( line[0] != '#') ) {
            // Line is not empty and doesn't start with # (hash/pound sign)
            string str_tmpline = line;  // Create a copy of the line for parsing
            keyword = line_obj.extractNextElementFromLine(&line);
            value   = line_obj.extractNextElementFromLine(&line);
            if ( keyword.compare("NODE") == 0 && value.compare("RESERVOIR") == 0 ) {
                size_t n_cols = line_obj.calcNrCols(&str_tmpline);

                // BVM May 2026. 
                if(n_cols != 5) {
                    LOG_WARN("Invalid line in statefile " + filename + ": " + str_tmpline);
                    LOG_ERR("Expected 5 columns for NODE RESERVOIR, but got " + std::to_string(n_cols));
                }

                token = line_obj.extractNextElementFromLine(&line);
                tmp_idnr = atoi(token.c_str() );
                keyword = line_obj.extractNextElementFromLine(&line);
                if(tmp_idnr == idnr && keyword == nodename) {
                    // NODE RESERVOIR
                    value   = line_obj.extractNextElementFromLine(&line);
                    this->reservoir_init_fr = atof( value.c_str() );
                    found_node = true;
                }
            }
        }
    }

    if(!found_node) {
        LOG_WARN("ReadStateFile     idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename);
		LOG_ERR("There is something wrong with nodes in the statefile " + filename);
    }

    myfile.close();
    return 0;
}
//------------------------------------------------------------------------
double Reservoir::GetStartWater_Mm3(void) {
    // Starting water volume.
    filling_at_lrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_LRW);
    filling_at_hrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_HRW);
    double start_res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);
    return start_res_Mm3;
}
//------------------------------------------------------------------------
double Reservoir::GetEndWater_Mm3(void) {
    // Ending water volume
    return this->res_Mm3;  // This is the water in the reservoir at the end of the last timestep
}
//------------------------------------------------------------------------
int Reservoir::WriteNodeOutput(GlobalConfig *gc){

    FILE *fp;
    char outfilename [100];
    sprintf (outfilename, "%snode%lu_%s.txt", gc->outputdir.c_str() , (idnr) , nodename.c_str()  );

    char outstr [100];

    if((fp = fopen(  outfilename ,"w"))==NULL) {
        LOG_ERR("Cannot open file " + std::string(outfilename));
    }

    sprintf (outstr, "RESERVOIR node %d %s\n", int(idnr), nodename.c_str()  );
    fprintf(fp, "%s", outstr);
    fprintf(fp, "reservoir_init_fr= %.5f\n", this->reservoir_init_fr);

    fprintf(fp, "yyyy mm dd hh [m3/s] [Euro/MWh] [fr] [m3/s] [Mm3] [masl] [fr] [Euro]         [m3/s]     [m3/s]    [m3/s]   [m3/s]    [m3/s] \n");
    fprintf(fp, "yyyy mm dd hh Inflow Price Action Up_Inflow Res_Mm3 Res_masl Res_fr lrw_cost tunnelflow hatchflow overflow auto_qmin tot_outflow\n");

    for(size_t t = 0; t < this->stps; t++) {
        fprintf(fp, "%d %d %d %d ", S->year[t], S->month[t], S->day[t], S->hour[t]);
        fprintf(fp, "%.4f %.4f %.4f ", S->inflow[t] , S->price[t], S->action[t][this->idnr] ); // CHANGE BY OVE: Added outoput for action pr generator
        fprintf(fp, "%.4f ", S->up_inflow[t]);
        fprintf(fp, "%.4f %.4f %.4f ", S->res_Mm3[t] , S->res_masl[t], S->res_fr[t] );
        fprintf(fp, "%.4f ", S->cost[t]);
        fprintf(fp, "%.4f %.4f %.4f %.4f ",  S->tunnelflow_m3s[t], S->hatchflow_m3s[t], S->overflow_m3s[t] , S->auto_qmin_m3s[t]);
        fprintf(fp, "%.4f ", S->tot_outflow[t]);
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}
/////////////////////////////////////////////////////////////////////////
double Reservoir::GetTunnelFLow(size_t t) {
    LOG_WARN("ERROR reservoir cannot use this function");
    LOG_ERR("NODE RESERVOIR " + std::to_string(int(idnr)) + " " + nodename);
    return -99.0;
}
/////////////////////////////////////////////////////////////////////////
int Reservoir::WriteStateFile(FILE *fp) {
    // # NODE RESERVOIR IDNR NAME INIT_RES_FR
    fprintf (fp, "NODE RESERVOIR %d %s %.5f\n", int(idnr), nodename.c_str() , this->S->res_fr[S->stps-1] );
    return 0; 
}
/////////////////////////////////////////////////////////////////////////
// CHANGE BY OVE: CheckWaterBalance method that uses variable timesteps
int Reservoir::CheckWaterBalance(class Herss *herss_obj) { 

    // Starting water volume.
    filling_at_lrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_LRW);
    filling_at_hrw_Mm3 = ac_res_masl_2_Mm3.x2y(this->res_HRW);
    double start_res_Mm3 = filling_at_lrw_Mm3 + reservoir_init_fr * (filling_at_hrw_Mm3 - filling_at_lrw_Mm3);
    double sum_inflow  = 0.0;
    double sum_outflow = 0.0;

    for(size_t t = 0; t < this->stps; t++) {
        // Use variable timestep for each timestep
        int variable_dt = herss_obj->getDeltaT(t);
        sum_inflow += MACRO_m3s_2_Mm3( (this->S->inflow[t] + this->S->up_inflow[t]) , variable_dt);
        sum_outflow += MACRO_m3s_2_Mm3(this->S->tot_outflow[t], variable_dt);
    }

    // Ending water volume
    double end_res_Mm3 = this->res_Mm3;
    double waterbalance = start_res_Mm3 + sum_inflow - end_res_Mm3 - sum_outflow;


    if(WATERBALANCE_WARNINGS) {
        printf( "WATERBALANCE RESERVOIR (Variable Timesteps) idnr=%d  nodename=%s\n", int(idnr), nodename.c_str()  );
        printf("start_res_Mm3     = %.6f\n", start_res_Mm3);
        printf("sum_inflow_Mm3    = %.6f\n", sum_inflow);
        printf("sum_outflow_Mm3   = %.6f\n", sum_outflow);
        printf("end_res_Mm3       = %.6f\n", end_res_Mm3);
        printf("waterbalance      = %.6f\n", waterbalance);
        printf( "---------------------------\n" );
    }


    if(abs(waterbalance) > 0.0001) {
        LOG_WARN("------ERROR ERROR ERROR (Variable Timesteps) --------------");
        LOG_WARN("WATERBALANCE RESERVOIR idnr=" + std::to_string(int(idnr)) + "  nodename=" + nodename);
        LOG_WARN("start_res_Mm3 = " + std::to_string(start_res_Mm3));
        LOG_WARN("sum_inflow    = " + std::to_string(sum_inflow));
        LOG_WARN("sum_outflow   = " + std::to_string(sum_outflow));
        LOG_WARN("end_res_Mm3   = " + std::to_string(end_res_Mm3));
        LOG_WARN("waterbalance  = " + std::to_string(waterbalance));
        LOG_WARN("idnr=" + std::to_string(int(idnr)) + "   nodename=" + nodename);
        LOG_ERR("---------------------------");
    }
    return 0; 
}
/////////////////////////////////////////////////////////////////////////