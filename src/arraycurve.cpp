/********************************************************************************
Project:      The Hydraulic Economic River System Simulator (HERSS)
Filename:     arraycurve.cpp                                                        
Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
Organization: Å Energi, www.ae.no

This software is released under the MIT license:
Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>

********************************************************************************/

#include "arraycurve.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <limits>
#include "herss.h"

// Static variable to track current timestep for error reporting
static size_t current_timestep = 0;
// Static variables to track current node for error reporting
static size_t current_node_idnr = 0;
static std::string current_node_name = "";

ArrayCurve::ArrayCurve(){}
ArrayCurve::~ArrayCurve(){}

void ArrayCurve::setCurrentTimestep(size_t timestep) {
    current_timestep = timestep;
}

// Function to set current node information for error reporting
void ArrayCurve::setCurrentNode(size_t idnr, const std::string& nodename) {
    current_node_idnr = idnr;
    current_node_name = nodename;
}


//////////////////////////////////////////////////////////////////
double ArrayCurve::initializeArrays() {
    xmin = ymin =  99999999.9;
    xmax = ymax = -999999999.9;

    for(int i = 0; i < nr_pts; i++) {

        if( x_points[i] > xmax) {
            xmax = x_points[i];
        }

        if( x_points[i] < xmin) {
            xmin = x_points[i];
        }

        if( y_points[i] > ymax) {
            ymax = y_points[i];
        }

        if( y_points[i] < ymin) {
            ymin = y_points[i];
        }
    }

    // Now we normalize both axis [0,1]
    for(int i = 0; i < nr_pts; i++) {
        x_points[i]  = (x_points[i] - xmin) / (xmax - xmin);
        y_points[i]  = (y_points[i] - ymin) / (ymax - ymin);
    }

    xlower[0] = x_points[0];
    ylower[0] = y_points[0];
    xupper[0] = x_points[1];
    yupper[0] = y_points[1];

    int idx_points = 0;
    double dx = (x_points[nr_pts-1] - x_points[0])/double(POINTS_IN_ARRAY);
    double x;
    for(int t = 1; t < POINTS_IN_ARRAY; t++) {
        x = x_points[0] + double(t)*dx;
        if(x >= x_points[idx_points+1]){
            idx_points++;
        }
        xlower[t] = x_points[idx_points];
        ylower[t] = y_points[idx_points];
        xupper[t] = x_points[idx_points+1];
        yupper[t] = y_points[idx_points+1];
    }
    return 0.0;
}
///////////////////////////////////////////////////////////////////////////
//
// BUG: Fix later.
// When the flow is at maximum. We are at the upper end of the efficiency curves.
// The current method have a numerical problem with it.
// The quick solution is to make the curves go to a tiny fraction above max flow.
// We have to look at this later
double ArrayCurve::x2y(double x) {

    double xt = x + 0.0;

    xt = (x-xmin)/(xmax-xmin);

    if(xt > 1.0 || xt < 0.0) {

        LOG_WARN("ERROR with normalization   [0,1]");

        printf("ERROR with normalization   [0,1]\n");
        printf("xt=%.8f\n", xt );
        printf("xmin = %.4f\n", xmin );
        printf("xmax = %.4f\n", xmax );
        printf("timestep = %zu\n", current_timestep );
        printf("node idnr = %zu, nodename = %s\n", current_node_idnr, current_node_name.c_str() );
        for(int i = 0; i < nr_pts; i++) {
            printf("%d x_points[i]=%.5f  y_points[i]=%.5f\n", i, x_points[i], y_points[i]);
        }
        
        return -1.0 * VERY_LARGE_NUMBER;

    }

    int idx;
    idx = int(  0.5 +  ( xt - x_points[0]) / (x_points[nr_pts-1] - x_points[0]) * double(POINTS_IN_ARRAY)) ;

    if(xt < x_points[0] || xt > x_points[nr_pts-1]) {
        
        LOG_WARN("ERROR: x value is out of bounds for interpolation");

        printf("HOUSTON - we have a problem!\n");
        printf("x=%.3f\n", xt);
        printf("idx = %d\n", idx);
        printf("file: %s  linenr: %d  function: %s \n", __FILE__ , __LINE__, __FUNCTION__);
        return -1.0 * VERY_LARGE_NUMBER;
    }

    double y;
    double slope;
    slope = (yupper[idx] - ylower[idx]) / ( xupper[idx] - xlower[idx] );
    y = slope * (xt- xlower[idx]) +  ylower[idx];

    // De-Normalize
    y = y * (ymax-ymin) + ymin;
    return y;
}
///////////////////////////////////////////////////////////////////////////