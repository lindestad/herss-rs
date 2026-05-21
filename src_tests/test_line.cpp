#include <gtest/gtest.h>
#include "herss.h"
#include <string>

// Test fixture for Line class tests
class LineTest : public ::testing::Test
{
protected:
    Line line_obj;
    void SetUp() override{}
    void TearDown() override {}
};


TEST_F(LineTest, ExtractNextElementFromLine_SingleElement)
{
    std::string test_line = "onlyword";
    std::string result = line_obj.extractNextElementFromLine(&test_line);
    
    EXPECT_EQ(result, "onlyword");
    EXPECT_EQ(test_line, "");  // Should be empty after extracting only element
}

TEST_F(LineTest, ExtractNextElementFromLine_EmptyString)
{
    std::string test_line = "";
    std::string result = line_obj.extractNextElementFromLine(&test_line);
    EXPECT_EQ(result, "");
    EXPECT_EQ(test_line, "");
}

TEST_F(LineTest, ExtractNextElementFromLine_OnlyWhitespace)
{
    std::string test_line = "   \t\n  ";
    std::string result = line_obj.extractNextElementFromLine(&test_line);
    EXPECT_EQ(result, "");
    EXPECT_EQ(test_line, "");
}

TEST_F(LineTest, CalcNrCols_WithExtraWhitespace)
{
    std::string test_line = "  first   second\tthird   \n fourth  ";
    int result = line_obj.calcNrCols(&test_line);
    
    EXPECT_EQ(result, 4);
}

TEST_F(LineTest, CalcNrCols_SingleWord)
{
    std::string test_line = "onlyword";
    int result = line_obj.calcNrCols(&test_line);
    
    EXPECT_EQ(result, 1);
}

TEST_F(LineTest, CalcNrCols_EmptyString)
{
    std::string test_line = "";
    int result = line_obj.calcNrCols(&test_line);
    
    EXPECT_EQ(result, 0);
}

TEST_F(LineTest, CalcNrCols_OnlyWhitespace)
{
    std::string test_line = "   \t\n  ";
    int result = line_obj.calcNrCols(&test_line);
    
    EXPECT_EQ(result, 0);
}

// Tests for extractLastElementFromLine method
TEST_F(LineTest, ExtractLastElementFromLine_BasicString)
{
    std::string test_line = "first second third";
    std::string result = line_obj.extractLastElementFromLine(&test_line);
    
    EXPECT_EQ(result, "third");
    EXPECT_EQ(test_line, "first second");  // Last element should be removed
}

TEST_F(LineTest, ExtractLastElementFromLine_WithTrailingWhitespace)
{
    std::string test_line = "first second third   \t\n  ";
    std::string result = line_obj.extractLastElementFromLine(&test_line);
    
    EXPECT_EQ(result, "third");
    EXPECT_EQ(test_line, "first second");
}

TEST_F(LineTest, ExtractLastElementFromLine_SingleElement)
{
    std::string test_line = "onlyword";
    std::string result = line_obj.extractLastElementFromLine(&test_line);
    
    EXPECT_EQ(result, "onlyword");
    EXPECT_EQ(test_line, "");  // Should be empty after extracting only element
}

TEST_F(LineTest, ExtractLastElementFromLine_WithWhitespace)
{
    std::string test_line = "  onlyword  ";
    std::string result = line_obj.extractLastElementFromLine(&test_line);
    
    EXPECT_EQ(result, "onlyword");
    EXPECT_EQ(test_line, "");
}

TEST_F(LineTest, ExtractLastElementFromLine_EmptyString)
{
    std::string test_line = "";
    std::string result = line_obj.extractLastElementFromLine(&test_line);
    
    EXPECT_EQ(result, "");
    EXPECT_EQ(test_line, "");
}

// Tests for removeWhites method
TEST_F(LineTest, RemoveWhites_BasicString)
{
    std::string test_line = "  first second third  ";
    int result = line_obj.removeWhites(&test_line);
    
    EXPECT_EQ(result, 0);  // Method returns 0 on success
    EXPECT_EQ(test_line, "first second third");
}

TEST_F(LineTest, RemoveWhites_WithTabsAndNewlines)
{
    std::string test_line = "\t\n  first second third\t\n  ";
    int result = line_obj.removeWhites(&test_line);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(test_line, "first second third");
}

TEST_F(LineTest, RemoveWhites_NoWhitespace)
{
    std::string test_line = "first";
    int result = line_obj.removeWhites(&test_line);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(test_line, "first");  // Should remain unchanged
}

TEST_F(LineTest, RemoveWhites_OnlyWhitespace)
{
    std::string test_line = "   \t\n  ";
    int result = line_obj.removeWhites(&test_line);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(test_line, "");  // Should become empty
}

TEST_F(LineTest, RemoveWhites_EmptyString)
{
    std::string test_line = "";
    int result = line_obj.removeWhites(&test_line);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(test_line, "");
}

// Tests for checkDigit method
TEST_F(LineTest, CheckDigit_ContainsDigits)
{
    std::string test_line = "abc123def";
    int result = line_obj.checkDigit(test_line);
    
    EXPECT_EQ(result, 1);  // Should return 1 if digits found
}

TEST_F(LineTest, CheckDigit_OnlyDigits)
{
    std::string test_line = "123456";
    int result = line_obj.checkDigit(test_line);
    
    EXPECT_EQ(result, 1);
}

TEST_F(LineTest, CheckDigit_NoDigits)
{
    std::string test_line = "abcdef";
    int result = line_obj.checkDigit(test_line);
    
    EXPECT_EQ(result, 0);  // Should return 0 if no digits found
}

TEST_F(LineTest, CheckDigit_OnlySpecialChars)
{
    std::string test_line = "!@#$%^&*()";
    int result = line_obj.checkDigit(test_line);
    
    EXPECT_EQ(result, 0);
}

TEST_F(LineTest, CheckDigit_EmptyString)
{
    std::string test_line = "";
    int result = line_obj.checkDigit(test_line);
    
    EXPECT_EQ(result, 0);
}

TEST_F(LineTest, CheckDigit_SingleDigit)
{
    std::string test_line = "5";
    int result = line_obj.checkDigit(test_line);
    
    EXPECT_EQ(result, 1);
}

TEST_F(LineTest, CheckDigit_MixedWithWhitespace)
{
    std::string test_line = "abc 123 def";
    int result = line_obj.checkDigit(test_line);
    
    EXPECT_EQ(result, 1);
}

// Integration tests - combining multiple methods
TEST_F(LineTest, Integration_ExtractAndCount)
{
    std::string test_line = "  first   second\tthird   fourth  ";
    
    // First count columns
    int cols = line_obj.calcNrCols(&test_line);
    EXPECT_EQ(cols, 4);
    
    // Extract elements one by one
    std::string first = line_obj.extractNextElementFromLine(&test_line);
    EXPECT_EQ(first, "first");
    
    std::string second = line_obj.extractNextElementFromLine(&test_line);
    EXPECT_EQ(second, "second");
    
    std::string last = line_obj.extractLastElementFromLine(&test_line);
    EXPECT_EQ(last, "fourth");
    
    // Should have only "third" left
    line_obj.removeWhites(&test_line);
    EXPECT_EQ(test_line, "third");
}

TEST_F(LineTest, Integration_ProcessDataLine)
{
    // Simulate processing a typical data file line
    std::string data_line = "2024 01 15 12  123.45  NODE_NAME  456.78  ";
    
    // Count columns
    int cols = line_obj.calcNrCols(&data_line);
    EXPECT_EQ(cols, 7);
    
    // Extract year
    std::string year = line_obj.extractNextElementFromLine(&data_line);
    EXPECT_EQ(year, "2024");
    EXPECT_EQ(line_obj.checkDigit(year), 1);
    
    // Extract month  
    std::string month = line_obj.extractNextElementFromLine(&data_line);
    EXPECT_EQ(month, "01");
    EXPECT_EQ(line_obj.checkDigit(month), 1);
    
    // Extract node name (should be non-numeric)
    line_obj.extractNextElementFromLine(&data_line); // day
    line_obj.extractNextElementFromLine(&data_line); // hour
    line_obj.extractNextElementFromLine(&data_line); // value
    std::string node_name = line_obj.extractNextElementFromLine(&data_line);
    EXPECT_EQ(node_name, "NODE_NAME");
    EXPECT_EQ(line_obj.checkDigit(node_name), 0);
}
