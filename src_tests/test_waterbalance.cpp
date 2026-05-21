#include <gtest/gtest.h>
#include "herss.h"
#include <string>

// Integration-style test verifying ending reservoir volumes (Mm3) for specific nodes
// using the uTAHPS test dataset. We assert that reservoir node 5 (TOPPSY) ends at
// 382.3622 Mm3 and reservoir node 9 (KROKNESVATN) ends at 216.4803 Mm3.
// These values are taken from the generated node output files in
// src_tests/utahps_test/output after a reference simulation.
// Tolerance: 1e-3 Mm3 (1e3 m3) which is stricter than typical volumetric rounding.

TEST(WaterBalanceEndVolumesTest, Utahps_EndReservoirVolumesMatchReference) {
    // Arrange
    auto *gc = new GlobalConfig();
    gc->globalfile = std::string("../src_tests/utahps_test/global.txt");

    gc->readGlobalFile();
    gc->SetDirectoriesAndFilenames();
    gc->DiagnoseTopologyFile();
    gc->checkNrSteps();
    gc->write_nodefiles = false; // speed: don't emit per-node outputs during test

    Dataset *data = new Dataset(gc);
    data->readAllData();

    Herss *herss = new Herss(gc);
    herss->prepaireSimulation(data);

    // Act
    herss->Simulate();
    herss->CheckWaterBalance();
    herss->GlobalWaterBalance();

    // We need the internal Reservoir objects. The node idnr is mapped to reservoir index via reservoir_idnr.
    // Node 5 TOPPSY, Node 9 KROKNESVATN.
    size_t node_toppsy = 5;
    size_t node_kroknesvatn = 9;

    ASSERT_LT(node_toppsy, gc->nr_nodes);
    ASSERT_LT(node_kroknesvatn, gc->nr_nodes);

    Node *n5 = herss->rs->nodes[node_toppsy];
    Node *n9 = herss->rs->nodes[node_kroknesvatn];
    ASSERT_EQ(n5->nodetype, NodeType::RESERVOIR);
    ASSERT_EQ(n9->nodetype, NodeType::RESERVOIR);

    size_t res_idx5 = n5->reservoir_idnr;
    size_t res_idx9 = n9->reservoir_idnr;
    ASSERT_LT(res_idx5, herss->rs->nr_reservoirs);
    ASSERT_LT(res_idx9, herss->rs->nr_reservoirs);

    double end5 = herss->rs->reservoirs[res_idx5].GetEndWater_Mm3();
    double end9 = herss->rs->reservoirs[res_idx9].GetEndWater_Mm3();

    // Assert (reference values)
    EXPECT_NEAR(end5, 382.3622, 1e-3);
    EXPECT_NEAR(end9, 216.4803, 1e-3);

    // Cleanup
    delete herss;
    delete data;
    delete gc;
}
