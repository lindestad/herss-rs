// Integration test for full uTAHPS simulation value function
#include <gtest/gtest.h>
#include "herss.h"
#include <iostream>
#include <iomanip>

// This test mirrors running: ./herss.exe ./global.txt (using the uTAHPS test dataset)
// and verifies that the computed value function matches a known reference value.
// Reference value provided: 7759325.85235 (Euro)
// We allow a small tolerance to account for floating point rounding.
TEST(ValueFunctionIntegrationTest, UtahpsSystem_ComputesExpectedValueFunction) {
	// Arrange
	auto *gc = new GlobalConfig();
	// Use the copies of the input files placed under src_tests/utahps_test
	gc->globalfile      = std::string("../src_tests/utahps_test/global.txt");

	// Read and diagnose configuration
	gc->readGlobalFile();
	gc->SetDirectoriesAndFilenames();      // Resolves relative INPUTDIR/OUTPUTDIR paths
	gc->DiagnoseTopologyFile();            // Populate nodetypes / counts
	gc->checkNrSteps();                    // Determine number of timesteps from price file

	// (Optional) avoid writing per-node output files during the test for speed
	gc->write_nodefiles = false;           // We still allow aggregated output if invoked

	// Load dataset (price, inflow, actions, time vectors)
	Dataset *data = new Dataset(gc);
	data->readAllData();                   // Also initializes multi temporal resolution

	// Prepare simulation objects
	Herss *herss = new Herss(gc);
	herss->prepaireSimulation(data);       // Sets scenarios, pointers, state & generator actions

	// Act: run full simulation sequence (same order as main.cpp)
	herss->Simulate();
	herss->CheckWaterBalance();
	herss->GlobalWaterBalance();
	herss->CalcAdjustmenCosts();
	double vf = herss->rs->CalcVF(data->restprice);

	// Print the simulated value function
	std::cout << std::fixed << std::setprecision(5)
			  << "[ValueFunctionIntegrationTest] uTAHPS ValueFunction = " << vf << "\n";

	// Assert: compare against reference value function
	const double kExpectedVF = 7759325.85235;
	// Tolerance: 1e-2 Euro (cent-level) should be sufficient; relax slightly if platform differences arise
	EXPECT_NEAR(vf, kExpectedVF, 1e-2) << "ValueFunction mismatch for uTAHPS integration test";
	// Internal consistency: stored valuefunction_Euro should equal returned value
	EXPECT_NEAR(herss->rs->valuefunction_Euro, vf, 10);

	// Cleanup
	delete herss;
	delete data;
	delete gc;
}