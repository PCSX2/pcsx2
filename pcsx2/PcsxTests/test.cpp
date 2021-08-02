#include "Utilities/PString.h"

TEST(StringConversionTest, wStringToPString)
{
	std::wstring testW = L"A Widened testString";
	PString testP = testW;
	EXPECT_EQ(testW, testP); 
}

TEST(StringConversionTest, wStringFromPString)
{
	PString testP(std::string("A NonWideTest"));
	std::wstring testWAssigned = L"A NonWideTest";
	std::wstring testW = testP;
	std::wcout << testW << std::endl << testWAssigned << std::endl;
	EXPECT_EQ(testW, testWAssigned);
}

TEST(StringConversionTest, wxStringToPstring)
{
	wxString testW = L"A Widened testString";
	PString testP = testW;
	EXPECT_EQ(testW, testP);
}

TEST(StringConversionTest, wxStringFromPstring)
{
	PString testP = std::string("A NonWideTest");
	wxString testW = testP;
	EXPECT_EQ(testW, testP);
}

#ifdef __cpp_lib_char8_t
TEST(StringConversionTest, u8StringFromPString)
{
	PString testP = std::string("UTF TEST");
	std::u8String testUTF = testp;
	EXPECT_EQ(testP, testUTF);
}

TEST(StringConversionTest, u8StringFromPString)
{
	// This probably is wrong?
	std::u8String testUTF = "TestUTF"
	PString testP = testUTF;
	EXPECT_EQ(testP, testUTF);
}
#endif

TEST(StringFunction, MutliByte)
{
	PString Test = std::string("MultiByte Test");
	std::string mbTest = Test.mb();
	EXPECT_EQ(Test, mbTest); // ? mb can loose data not sure if this is the correct test
}

TEST(StringFunction, u8)
{
	PString Test = std::string("U8 Test");
	std::string u8Test = Test.u8();
	EXPECT_EQ(Test, u8Test);
}

