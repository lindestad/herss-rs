#include <gtest/gtest.h>
#include "herss.h"
#include <vector>

// Lightweight fixture to build a synthetic reservoir with simple curves
class ReservoirTest : public ::testing::Test {
protected:
    Reservoir res;
    Scenario* scen = nullptr;

    void SetUp() override {
        // Minimal scenario: 4 timesteps, 1-hour default
        scen = new Scenario(4, 3600, 0);
        res.S = scen;
    res.idnr = 0;
    res.nodename = "TEST_RES";

        // Simple reservoir curve: masl->[Mm3]
        // 100->0, 110->10, 120->25 (non-linear spacing)
        res.nr_points_res_curve = 3;
        res.res_curve_masl[0] = 100.0; res.res_curve_Mm3[0] = 0.0;
        res.res_curve_masl[1] = 110.0; res.res_curve_Mm3[1] = 10.0;
        res.res_curve_masl[2] = 120.0; res.res_curve_Mm3[2] = 25.0;

    // Overflow curve: start at LRW (100 masl) and extend beyond HRW to avoid edge issues
    res.nr_points_ovefl_curve = 3;
    res.ovefl_curve_masl[0] = 100.0; res.ovefl_curve_m3s[0] = 2.0;   // m3/s
    res.ovefl_curve_masl[1] = 110.0; res.ovefl_curve_m3s[1] = 8.0;
    res.ovefl_curve_masl[2] = 130.0; res.ovefl_curve_m3s[2] = 20.0;

        // Levels and penalties
        res.res_LRW = 100.0;
        res.res_HRW = 120.0;
        res.res_penalty = 10.0;
        res.fast_overflow = 0; // default slow overflow

        // Init curves
        ASSERT_EQ(res.initArrayCurves(), 0);

        // Initial fill at 50%
        res.reservoir_init_fr = 0.5; // halfway LRW-HRW -> ~110 masl

        // Downstream node to receive overflow (powerstation is fine; only using Scenario)
        auto* downstream = new Powerstation();
        downstream->S = new Scenario(4, 3600, 0);
        res.ptr_downstream_node_overflow = downstream;
        res.outlet_overflow_in_use = true;

        // Complete initialization
        res.InitReservoir();
    }

    void TearDown() override {
        // Clean up downstream allocations
        if (res.ptr_downstream_node_overflow) {
            delete res.ptr_downstream_node_overflow->S;
            delete res.ptr_downstream_node_overflow;
            res.ptr_downstream_node_overflow = nullptr;
        }
        delete scen;
    }
};

TEST_F(ReservoirTest, Constructor_InitializesCorrectly) {
    Reservoir r;
    EXPECT_EQ(r.nr_points_res_curve, 0u);
    EXPECT_EQ(r.nr_points_ovefl_curve, 0u);
    EXPECT_FALSE(r.outlet_hatch_in_use);
    EXPECT_FALSE(r.outlet_tunnel_in_use);
}

TEST_F(ReservoirTest, InitReservoir_SetsInitialState) {
    // After SetUp(): res at 50% between LRW (0 Mm3) and HRW (25 Mm3) => 12.5 Mm3
    // masl derived via inverse curve
    double expected_start_Mm3 = res.ac_res_masl_2_Mm3.x2y(res.res_LRW) +
                                0.5 * (res.ac_res_masl_2_Mm3.x2y(res.res_HRW) - res.ac_res_masl_2_Mm3.x2y(res.res_LRW));
    EXPECT_NEAR(res.res_Mm3, expected_start_Mm3, 1e-6);
    // We only require masl to be within LRW-HRW range (mapping specifics are handled elsewhere)
    EXPECT_GE(res.res_masl, res.res_LRW - 1e-9);
    EXPECT_LE(res.res_masl, res.res_HRW + 1e-9);
}

TEST_F(ReservoirTest, CalcOverflow_Slow_RespectsMaxOverflow) {
    // Put level above overflow start so overflow occurs
    res.S->dt = 3600; // 1 hour
    res.res_masl = 121.0;
    // Avoid out-of-range on reservoir curve: use HRW volume + small excess
    double vol_hrw = res.ac_res_masl_2_Mm3.x2y(res.res_HRW);
    res.res_Mm3 = vol_hrw + 1.0; // Mm3 above HRW
    res.filling_at_hrw_Mm3 = res.ac_res_masl_2_Mm3.x2y(res.res_HRW);
    res.fast_overflow = 0;

    // Compute overflow via curve; value determined by interpolation on overflow curve
    double overflow_Mm3 = res.CalcOverflow();
    // It should not exceed the amount above HRW
    double max_overflow = res.res_Mm3 - res.filling_at_hrw_Mm3;
    if (max_overflow < 0.0) max_overflow = 0.0;
    EXPECT_LE(overflow_Mm3, max_overflow + 1e-9);
}

TEST_F(ReservoirTest, CalcOverflow_Fast_EqualsExcessAboveHRW) {
    // Fast path should return exactly the excess volume above HRW (clipped at 0)
    res.res_masl = 121.0;
    // Avoid out-of-range: set volume slightly above HRW
    double vol_hrw2 = res.ac_res_masl_2_Mm3.x2y(res.res_HRW);
    res.res_Mm3 = vol_hrw2 + 1.0;
    res.filling_at_hrw_Mm3 = res.ac_res_masl_2_Mm3.x2y(res.res_HRW);
    res.fast_overflow = 1;

    double overflow_fast = res.CalcOverflow();
    double expected = res.res_Mm3 - res.filling_at_hrw_Mm3;
    if (expected < 0.0) expected = 0.0;
    EXPECT_NEAR(overflow_fast, expected, 1e-9);
}

TEST_F(ReservoirTest, Simulate_OverflowOnly_RoutesDownstreamAndUpdatesState) {
    // No tunnel/hatch/auto_qmin
    res.outlet_tunnel_in_use = false;
    res.outlet_hatch_in_use = false;
    res.outlet_auto_qmin_in_use = false;

    // Raise level above overflow start and give small local inflow to keep things changing
    res.res_masl = 121.0;
    // Avoid out-of-range: set volume slightly above HRW
    double vol_hrw3 = res.ac_res_masl_2_Mm3.x2y(res.res_HRW);
    res.res_Mm3 = vol_hrw3 + 1.0;
    res.fast_overflow = 1; // deterministic positive overflow equal to excess
    res.S->inflow[0] = 0.5; // m3/s
    res.S->up_inflow[0] = 0.0;
    res.S->dt = 3600;

    // Simulate t=0
    int rc = res.Simulate(0);
    EXPECT_EQ(rc, 0);
    // There should be positive overflow and corresponding downstream inflow
    EXPECT_GT(res.S->overflow_m3s[0], 0.0);
    ASSERT_NE(res.ptr_downstream_node_overflow, nullptr);
    EXPECT_NEAR(res.ptr_downstream_node_overflow->S->up_inflow[0], res.S->overflow_m3s[0], 1e-9);
    // Total outflow equals overflow in this setup
    EXPECT_NEAR(res.S->tot_outflow[0], res.S->overflow_m3s[0], 1e-9);
    // Reservoir bookkeeping should be populated and non-negative
    EXPECT_GE(res.S->res_Mm3[0], 0.0);
    EXPECT_GE(res.S->res_masl[0], res.res_LRW - 1e-9);
    EXPECT_LE(res.S->res_masl[0], res.res_HRW + 1e-9);
}