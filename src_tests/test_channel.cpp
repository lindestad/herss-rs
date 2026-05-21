#include <gtest/gtest.h>
#include "herss.h"
#include <vector>
#include <memory>

// Minimal helper to allocate a GlobalConfig with given stps and one channel node (optionally plus downstream reservoir)
static GlobalConfig* makeGC(size_t stps, size_t nr_nodes) {
    auto* gc = new GlobalConfig();
    gc->stps = stps;
    gc->dt = 3600; // base hour; variable dt not used directly here
    gc->nr_nodes = nr_nodes;
    gc->nr_reservoirs = 0;
    gc->nr_pstations = 0;
    gc->nr_channels = 0;
    for (size_t i=0;i<nr_nodes;++i) {
        gc->nodetypes[i] = CHANNEL;
        gc->nr_channels++;
    }
    return gc;
}

// Helper to attach blank Scenario
static void attachScenario(Node* n, size_t stps, size_t dt) {
    n->S = new Scenario(stps, dt, n->idnr);
    for (size_t t=0;t<stps;++t) {
        n->S->up_inflow[t] = 0.0;
        n->S->tot_outflow[t] = 0.0;
        n->S->channel_storage_Mm3[t] = 0.0;
        n->S->dt = dt;
        n->S->stps = stps;
        n->S->year[t]=2022; n->S->month[t]=9; n->S->day[t]=1; n->S->hour[t]=t;
    }
}

class ChannelTest : public ::testing::Test {
protected:
    GlobalConfig* gc=nullptr;
    Riversystem* rs=nullptr;
    void TearDown() override {
        if(rs) {
            for(size_t n=0;n<gc->nr_nodes;++n) delete rs->nodes[n]->S;
            delete rs;
        }
        delete gc;
    }
};

TEST_F(ChannelTest, TraveltimeZero_RoutesUpInflowDirectly_NoStorage) {
    gc = makeGC(3,1);
    rs = new Riversystem(gc);
    auto* ch = static_cast<Channel*>(rs->nodes[0]);
    attachScenario(ch, gc->stps, gc->dt);
    ch->traveltime = 0; ch->decay = 1.0; ch->nodetype = CHANNEL;

    // Provide upstream inflow sequence (treated as arriving each step)
    ch->S->up_inflow[0]=2.0; ch->S->up_inflow[1]=3.0; ch->S->up_inflow[2]=0.5; // m3/s

    for(size_t t=0;t<gc->stps;++t) {
        ch->Simulate(t);
    }
    // Expect direct passthrough
    EXPECT_DOUBLE_EQ(ch->S->tot_outflow[0], 2.0);
    EXPECT_DOUBLE_EQ(ch->S->tot_outflow[1], 3.0);
    EXPECT_DOUBLE_EQ(ch->S->tot_outflow[2], 0.5);
    for(size_t t=0;t<gc->stps;++t) EXPECT_DOUBLE_EQ(ch->S->channel_storage_Mm3[t], 0.0);
}

TEST_F(ChannelTest, TraveltimeTwo_WithDecay_UpdatesStorageAndOutflow) {
    gc = makeGC(4,1);
    rs = new Riversystem(gc);
    auto* ch = static_cast<Channel*>(rs->nodes[0]);
    attachScenario(ch, gc->stps, gc->dt);
    ch->traveltime = 2; ch->decay = 0.9; ch->nodetype=CHANNEL;
    ch->waterflow_m3[0]=0.0; ch->waterflow_m3[1]=0.0;

    // Constant upstream inflow 10 m3/s
    for(size_t t=0;t<gc->stps;++t) ch->S->up_inflow[t]=10.0;

    for(size_t t=0;t<gc->stps;++t) ch->Simulate(t);

    // Outflow lags by traveltime steps. With traveltime=2 we expect:
    EXPECT_NEAR(ch->S->tot_outflow[0], 0.0, 1e-12); // no water yet
    EXPECT_NEAR(ch->S->tot_outflow[1], 0.0, 1e-12); // still filling second slot
    EXPECT_GT(ch->S->tot_outflow[2], 0.0);          // water from slot 1 appears
    EXPECT_GT(ch->S->channel_storage_Mm3[0], 0.0);
    EXPECT_GT(ch->S->channel_storage_Mm3[1], 0.0);
}

TEST_F(ChannelTest, RemainingVolumes_SetFromStorageAndNotNegative) {
    gc = makeGC(2,1);
    rs = new Riversystem(gc);
    auto* ch = static_cast<Channel*>(rs->nodes[0]);
    attachScenario(ch, gc->stps, gc->dt);
    ch->traveltime=1; ch->decay=1.0; ch->waterflow_m3[0]=0.0; ch->nodetype=CHANNEL;

    ch->S->up_inflow[0]=5.0; ch->Simulate(0);
    EXPECT_GE(ch->remaining_Mm3, 0.0);
    EXPECT_EQ(ch->remaining_active_Mm3, 0.0); // per code: always 0 for channels
}

TEST_F(ChannelTest, QminPenalty_AppliedWhenOutflowBelowRequirement) {
    gc = makeGC(1,1);
    rs = new Riversystem(gc);
    auto* ch = static_cast<Channel*>(rs->nodes[0]);
    attachScenario(ch, gc->stps, gc->dt);
    ch->traveltime=0; ch->decay=1.0; ch->nodetype=CHANNEL;

    // Configure a single Qmin period requiring 3 m3/s with penalty 100 Euro/h
    ch->qmin.nr_periods = 1; ch->qmin_in_use = true;
    ch->qmin.timeperiods[0].start_day=1; ch->qmin.timeperiods[0].start_month=1;
    ch->qmin.timeperiods[0].end_day=31; ch->qmin.timeperiods[0].end_month=12;
    ch->qmin.timeperiods[0].min_discharge=3.0; ch->qmin.timeperiods[0].penalty_cost=100.0;

    // Provide small inflow (1 m3/s) so outflow=1 < 3
    ch->S->up_inflow[0]=1.0;
    ch->Simulate(0);
    // cost_qmin = penalty_cost * dt/3600 = 100 * 3600/3600 = 100
    EXPECT_DOUBLE_EQ(ch->S->cost_qmin[0], 100.0);
    EXPECT_DOUBLE_EQ(ch->S->cost[0], 100.0);
}

TEST_F(ChannelTest, WaterBalance_ConservationWithinTolerance) {
    gc = makeGC(3,1);
    rs = new Riversystem(gc);
    auto* ch = static_cast<Channel*>(rs->nodes[0]);
    attachScenario(ch, gc->stps, gc->dt);
    ch->traveltime=2; ch->decay=1.0; ch->nodetype=CHANNEL;
    ch->waterflow_m3[0]=0.0; ch->waterflow_m3[1]=0.0;

    ch->S->up_inflow[0]=4.0; ch->S->up_inflow[1]=0.0; ch->S->up_inflow[2]=0.0;
    for(size_t t=0;t<gc->stps;++t) ch->Simulate(t);

    // Manual water balance: starting + inflow - ending - outflow = 0 within tolerance.
    double start_storage = 0.0; // initial waterflow_m3 sum
    double end_storage= ch->waterflow_m3[0]+ch->waterflow_m3[1];
    // Inflow volume: only step 0 contributes (4 m3/s * 3600 s)/1e6 Mm3
    double inflow_Mm3 = (4.0 * 3600.0)/1e6;
    // Outflow volume: sum tot_outflow * dt
    double outflow_Mm3=0.0;
    for(size_t t=0;t<gc->stps;++t) outflow_Mm3 += ch->S->tot_outflow[t]*3600.0/1e6;
    double balance = (start_storage/1e6) + inflow_Mm3 - (end_storage/1e6) - outflow_Mm3;
    EXPECT_NEAR(balance, 0.0, 1e-6);
}
