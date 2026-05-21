#!/home/bernt/py_env/herss_env/bin/python
# -*- coding: utf-8 -*-

"""
 * Project:       HERSS
 * Filename:      pyherss.py
 * Developers:    Bernt Viggo Matheussen
 * Original date: 14 October, 2024
 * Modified, May 2026
 *
 * Demonstrates how to use CPPYY to run the C++ herss model.  
 *
"""


import sys
import numpy as np
import time
import os
import cppyy


cppyy.load_library("../src/herss.so")
cppyy.include("../src/herss.h")

#---------------------------------------------------
# MAIN 
#---------------------------------------------------
if __name__ == "__main__":



    version      = cppyy.gbl.VERSION        # or a plain string like "1.0"
    version_date = cppyy.gbl.VERSION_DATE   # or a plain string like "2026-05-19"

    logfilename = f"herss_{version}_{version_date}.log"

    if not cppyy.gbl.Logger.instance().init(logfilename):
        print(f"Could not open log file: {logfilename}")



    # We need to set the inputdir and outputdir inside python code
    herss_inputdir="../data/mini_utahps_daily/"
    gc            = cppyy.gbl.GlobalConfig()

    gc.globalfile = herss_inputdir + "global.txt";

    print("Global file = ", gc.globalfile)

    gc.readGlobalFile()

    gc.inputdir = herss_inputdir
    gc.outputdir = herss_inputdir + "output/"
    gc.SetDirectoriesAndFilenames()
    gc.Diagnose()
    gc.checkNrSteps()

    data = cppyy.gbl.Dataset(gc)
    data.readAllData()

    herss = cppyy.gbl.Herss(gc)
    herss.prepaireSimulation(data)
    

    # Check that basic variables are initialized and sett within aceptable limits. 
    herss.rs.DiagnoseRiversystemConfiguration()

    herss.Simulate()

    print("Water Value (price) at end of planning horizon = ", data.restprice)

    vf = herss.rs.CalcVF(data.restprice)
    print("ValueFunction = ", vf)


    n_reservoirs = herss.gc.nr_reservoirs
    print("n_reservoirs = ", n_reservoirs)
    print("Nr of nodes in the riversystem = ", herss.nr_nodes)
    print("Timesteps in planning horizon = ", herss.stps)

    # Get the price, inflow, initial reservoir levels, ending reservoir levels, actions, from HERSS or the DATA class. 
    print("Price in timestep 3 (from herss) = ", herss.GetPrice(2) )
    print("Price in timestep 3 (from data)  = ", data.price[2] )


    # Node zero [0] is a reservoir,
    t = 3
    node_idnr = 0
    print("Inflow to node 0 (reservoir) in timestep 4 = ", herss.GetInflowInNode(t, node_idnr) )
    print("Initial reservoir level in node 0  = ", herss.GetReservoir_Init_fr(0)  )
    print("Simulated reservoir level at end of timestep 4 = ", herss.GetReservoirLevel_fr(node_idnr, t) )


    # We now have multi-generator options, so getting the actions is a bit more tricky. 
    #  We need to specify the node_idnr and the generator_idnr. 
    # Reservoirs dont have generators, so in reservoir nodes we do not use the gen_idnr.

    # C++ double Herss::GetAction(size_t node_idnr, size_t gen_idnr, size_t t) {
    # You have to keep track of the node idnr i
    node_idnr = 1 # Powerstation node
    gen_idnr = 0 # Generator idnr, for now we only have one generator per powerstation, so gen_idnr is always 0. 
    t = 4
    print("Action in Powerstation (node_idnr 1), at timestep t   = ", herss.GetAction(node_idnr, gen_idnr, t) )


    # Set new price and restprice 
    new_price = 99.9
    restprice = data.restprice + 5.0
    t = 1
    herss.SetPrice(t , new_price, restprice)


    # New version of SetAction, where we specify the node_idnr, gen_idnr, timestep and value.
    # Powerstation: node 4, generator 0, timestep 5, action 0.8
    # herss.SetAction(4, 0, 5, 0.8)
    # Reservoir with hatch: node 1, gen_idnr ignored but must be passed, timestep 5
    # herss.SetAction(1, 0, 5, 0.6)

    # Setting Action in Powerstation (node_idnr = 1)  for mini_utahps_daily, gen_idnr = 0, timestep t = 3, action = 0.75
    herss.SetAction(1, 0,  t, 0.75)


    # Set new inflow in reservoir at t  [m3/s]
    herss.SetInflowInNode(t, 0, 3.76)
    herss.SetReservoir_Init_fr(0, 0.33)
    

    # Now we can simulate again with the altered Price, Inflow, etc. 
    # Note that we dont need to calll herss.prepaireSimulation(data)
    # This is taken care of within herss.Simulate()
    herss.Simulate()
    vf = herss.rs.CalcVF(restprice)
    print("New ValueFunction = ", vf)





    print("THE-END")
#---------------------------------------------------
