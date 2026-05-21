#include <gtest/gtest.h>
#include "arraycurve.h"
#include "herss.h"
#include <cmath>

// Test fixture for ArrayCurve tests
class ArrayCurveTest : public ::testing::Test
{
protected:
    ArrayCurve *curve;
    void SetUp() override { 
        curve = new ArrayCurve(); 
    }
    void TearDown() override { 
        delete curve; 
    }
};

// Test: Constructor creates object successfully
TEST_F(ArrayCurveTest, ConstructorCreatesObject)
{
    // Verifies that the ArrayCurve object is created successfully
    ASSERT_NE(curve, nullptr);
}

// Test: initializeArrays calculates min/max values correctly
TEST_F(ArrayCurveTest, InitializeArraysCalculatesMinMax)
{
    // Sets up simple test data with known min/max values
    curve->nr_pts = 3;
    curve->x_points[0] = 1.0;
    curve->x_points[1] = 5.0;
    curve->x_points[2] = 10.0;
    curve->y_points[0] = 2.0;
    curve->y_points[1] = 8.0;
    curve->y_points[2] = 4.0;

    curve->initializeArrays();

    // Checks that min/max values are calculated correctly
    EXPECT_DOUBLE_EQ(curve->xmin, 1.0);
    EXPECT_DOUBLE_EQ(curve->xmax, 10.0);
    EXPECT_DOUBLE_EQ(curve->ymin, 2.0);
    EXPECT_DOUBLE_EQ(curve->ymax, 8.0);
}

// Test: initializeArrays normalizes points to [0,1] range
TEST_F(ArrayCurveTest, InitializeArraysNormalizesPoints)
{
    // Sets up test data
    curve->nr_pts = 3;
    curve->x_points[0] = 10.0;
    curve->x_points[1] = 20.0;
    curve->x_points[2] = 30.0;
    curve->y_points[0] = 100.0;
    curve->y_points[1] = 200.0;
    curve->y_points[2] = 300.0;

    curve->initializeArrays();

    // Checks that points are normalized to [0,1] range
    EXPECT_DOUBLE_EQ(curve->x_points[0], 0.0);  // (10-10)/(30-10) = 0
    EXPECT_DOUBLE_EQ(curve->x_points[1], 0.5);  // (20-10)/(30-10) = 0.5
    EXPECT_DOUBLE_EQ(curve->x_points[2], 1.0);  // (30-10)/(30-10) = 1.0
    
    EXPECT_DOUBLE_EQ(curve->y_points[0], 0.0);  // (100-100)/(300-100) = 0
    EXPECT_DOUBLE_EQ(curve->y_points[1], 0.5);  // (200-100)/(300-100) = 0.5
    EXPECT_DOUBLE_EQ(curve->y_points[2], 1.0);  // (300-100)/(300-100) = 1.0
}

// Test: x2y returns correct interpolated values
TEST_F(ArrayCurveTest, X2YReturnsCorrectInterpolatedValues)
{
    // Sets up a linear curve for easy verification
    curve->nr_pts = 3;
    curve->x_points[0] = 0.0;
    curve->x_points[1] = 5.0;
    curve->x_points[2] = 10.0;
    curve->y_points[0] = 0.0;
    curve->y_points[1] = 50.0;
    curve->y_points[2] = 100.0;

    curve->initializeArrays();

    // Tests interpolation at known points
    double result1 = curve->x2y(0.0);   // Should return 0.0
    double result2 = curve->x2y(5.0);   // Should return 50.0
    double result3 = curve->x2y(2.5);   // Should return 25.0 (interpolated)

    EXPECT_NEAR(result1, 0.0, 1.0);
    EXPECT_NEAR(result2, 50.0, 5.0);
    EXPECT_NEAR(result3, 25.0, 10.0);  // Allow more tolerance for interpolation
}

// Test: x2y handles boundary conditions correctly
TEST_F(ArrayCurveTest, X2YHandlesBoundaryConditions)
{
    // Sets up test data
    curve->nr_pts = 2;
    curve->x_points[0] = 1.0;
    curve->x_points[1] = 9.0;
    curve->y_points[0] = 10.0;
    curve->y_points[1] = 100.0;

    curve->initializeArrays();

    // Tests at exact boundary points
    double result_min = curve->x2y(1.0);  // Minimum x value
    double result_max = curve->x2y(8.9);  // Maximum x value

    EXPECT_NEAR(result_min, 10.0, 5.0);
    EXPECT_NEAR(result_max, 90.0, 15.0);
}

// Test: x2y returns error value on out-of-bounds input
TEST_F(ArrayCurveTest, X2YReturnsErrorOnOutOfBoundsInput)
{
    // Sets up test data
    curve->nr_pts = 2;
    curve->x_points[0] = 5.0;
    curve->x_points[1] = 15.0;
    curve->y_points[0] = 50.0;
    curve->y_points[1] = 150.0;

    curve->initializeArrays();

    // Tests that out-of-bounds input returns error value (-99.9)
    double result1 = curve->x2y(-1.0);  // Below minimum
    double result2 = curve->x2y(20.0);  // Above maximum

    EXPECT_DOUBLE_EQ(result1, -99.9);
    EXPECT_DOUBLE_EQ(result2, -99.9);
}


