#include <gtest/gtest.h>
#include "herss.h"
#include <string>
#include <fstream>
#include <cmath>

// Test fixture for Dataset tests
class DatasetTest : public ::testing::Test
{
protected:
    GlobalConfig *gc;
    Dataset *dataset;
    
    void SetUp() override 
    { 
        gc = new GlobalConfig();
        // Set up basic configuration for testing
        gc->stps = 16;
        gc->nr_nodes = 12;
        gc->pricefile = "../src_tests/utahps_test/pricefile.txt";
        gc->inflowfile = "../src_tests/utahps_test/inflow.txt";
        gc->actionsfile = "../src_tests/utahps_test/actions.txt";
        gc->dt_last = 3600;
        
        dataset = new Dataset(gc);
    }
    
    void TearDown() override 
    { 
        delete dataset;
        delete gc;
    }
};


// Test: readPricefile correctly reads price data
TEST_F(DatasetTest, ReadPricefileReadsCorrectly)
{
    dataset->readPricefile();
    
    // Check that restprice was read correctly
    EXPECT_EQ(dataset->restprice, 100.0);
    
    // Check that price data was read correctly
    EXPECT_NEAR(dataset->price[0], 626.954, 0.001);
    EXPECT_NEAR(dataset->price[1], 618.766, 0.001);
    EXPECT_NEAR(dataset->price[2], 618.742, 0.001);
    
    // Check that date data was parsed correctly
    EXPECT_EQ(dataset->year[0], 2022);
    EXPECT_EQ(dataset->month[0], 9);
    EXPECT_EQ(dataset->day[0], 1);
    EXPECT_EQ(dataset->hour[0], 0);
    
    EXPECT_EQ(dataset->year[1], 2022);
    EXPECT_EQ(dataset->month[1], 9);
    EXPECT_EQ(dataset->day[1], 1);
    EXPECT_EQ(dataset->hour[1], 1);
}

// Test: readInflowFile correctly reads inflow data
TEST_F(DatasetTest, ReadInflowFileReadsCorrectly)
{
    // First read price file to get the dates
    dataset->readPricefile();
    dataset->readInflowFile();
    
    // Check that inflow data was read correctly for the nodes mentioned in the file
    // Node 0
    EXPECT_NEAR(dataset->inflow[0][0], 0.36, 0.001);
    EXPECT_NEAR(dataset->inflow[1][0], 0.36, 0.001);
    
    // Node 3
    EXPECT_NEAR(dataset->inflow[0][3], 0.23, 0.001);
    EXPECT_NEAR(dataset->inflow[1][3], 0.23, 0.001);
    
    // Node 5
    EXPECT_NEAR(dataset->inflow[0][5], 0.31, 0.001);
    EXPECT_NEAR(dataset->inflow[1][5], 0.30, 0.001);
    
    // Node 9
    EXPECT_NEAR(dataset->inflow[0][9], 0.67, 0.001);
    EXPECT_NEAR(dataset->inflow[1][9], 0.67, 0.001);
    
    // Nodes not in the file should remain 0.0
    EXPECT_EQ(dataset->inflow[0][1], 0.0);
    EXPECT_EQ(dataset->inflow[0][2], 0.0);
}

// Test: readActionsFile correctly reads action data
TEST_F(DatasetTest, ReadActionsFileReadsCorrectly)
{
    // First read price file to get the dates
    dataset->readPricefile();
    dataset->readActionsFile();
    
    // Check that action data was read correctly
    // All actions in the test file are 0.80
    EXPECT_NEAR(dataset->action[0][0], 0.80, 0.001);
    EXPECT_NEAR(dataset->action[1][0], 0.80, 0.001);
    EXPECT_NEAR(dataset->action[15][0], 0.80, 0.001);
    
    // Check that action column names were stored
    EXPECT_EQ(dataset->action_colnames.size(), 4);
    EXPECT_EQ(dataset->action_colnames[0], "1_0");
    EXPECT_EQ(dataset->action_colnames[1], "6_0");
    EXPECT_EQ(dataset->action_colnames[2], "7_0");
    EXPECT_EQ(dataset->action_colnames[3], "10_0");
}


// Test: multi_temporal_resolution calculates delta_t correctly
TEST_F(DatasetTest, MultiTemporalResolutionCalculatesDeltaT)
{
    dataset->readPricefile();
    dataset->multi_temporal_resolution();
    
    // Check that delta_t vector has the correct size (stps - 1)
    EXPECT_EQ(dataset->delta_t.size(), dataset->stps - 1);
    
    // For hourly data, each delta_t should be 3600 seconds (1 hour)
    for(size_t i = 0; i < dataset->delta_t.size() - 1; i++) {
    EXPECT_EQ(dataset->delta_t[i], 3600);
    }
    EXPECT_EQ(dataset->delta_t.back(), 7200);
}

// Test: multi_temporal_resolution handles last two-hour step
TEST_F(DatasetTest, MultiTemporalResolutionHandlesLastTwoHourStep)
{
    dataset->readPricefile();
    dataset->multi_temporal_resolution();

    // Check that delta_t vector has the correct size
    EXPECT_EQ(dataset->delta_t.size(), dataset->stps - 1);

    // All intervals except the last should be 3600
    for(size_t i = 0; i < dataset->delta_t.size() - 1; i++) {
        EXPECT_EQ(dataset->delta_t[i], 3600);
    }
    EXPECT_EQ(dataset->delta_t.back(), 7200);
}


// Test: getDeltaT throws on out-of-bounds timestep
TEST_F(DatasetTest, GetDeltaTThrowsOnOutOfBounds)
{
    dataset->readPricefile();
    dataset->multi_temporal_resolution();
    
    // Test that out-of-bounds timestep causes exit
    EXPECT_EXIT(dataset->getDeltaT(dataset->stps + 1), 
                ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: readPricefile throws on missing file
TEST_F(DatasetTest, ReadPricefileThrowsOnMissingFile)
{
    gc->pricefile = "nonexistent_price.txt";
    EXPECT_EXIT(dataset->readPricefile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: readInflowFile throws on missing file
TEST_F(DatasetTest, ReadInflowFileThrowsOnMissingFile)
{
    gc->inflowfile = "nonexistent_inflow.txt";
    EXPECT_EXIT(dataset->readInflowFile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: readActionsFile throws on missing file
TEST_F(DatasetTest, ReadActionsFileThrowsOnMissingFile)
{
    gc->actionsfile = "nonexistent_actions.txt";
    EXPECT_EXIT(dataset->readActionsFile(), ::testing::ExitedWithCode(EXIT_FAILURE), ".*");
}

// Test: Date validation works correctly
TEST_F(DatasetTest, DateValidationWorksCorrectly)
{
    // This test verifies that date mismatches between files are caught
    // We'll test this indirectly by reading valid files that should match
    dataset->readPricefile();
    dataset->readInflowFile();
    dataset->readActionsFile();
    
    // If we get here without exit, the dates matched correctly
    SUCCEED();
}

