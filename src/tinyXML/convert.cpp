#include "convert.h"

//Tries to convert  accented characters from UT8
irr::core::stringw convert(irr::core::stringc text)
{
// This method is working 100% on Windows with MSVC. But cause a problem in with the GCC compiler. 
// Currently only MSVC compiler seem to support unicode encoding
#ifdef WIN32
	irr::core::stringw line = "";
	for (int a=0; a<(int)text.size(); a++)
	{
		// Found out that -61 is the marker for a accent
		// The next character define what type of accent
		//if ((int)text[a]<0 && (int)text[a]!=-61)
			//printf("Found another control code: %d\n",(int)text[a]);
		if ((int)text[a]==-30)
		{
			// Euro Symbol (Might not be available in the font)
			if ((int)text[a+1]==-126 && (int)text[a+2]==-84)line+=irr::core::stringw(L"�");
			a++;
			a++;
		}
		else if ((int)text[a]==-59)
		{
			// Special codes
			//printf("\nAccent found: 1:-59, 2:%d\n",(int)text[a+1]);
			if ((int)text[a+1]==-72)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-109)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-110)	line+=irr::core::stringw(L"�");
			a++;
		}
		else if ((int)text[a]==-61)
		{
			// Extended character set
			//printf("\nAccent found: 1:-61, 2:%d\n",(int)text[a+1]);
			//printf ("\nAccent found: 1:-61, 2:%x",(int)text[a+1]);
			if ((int)text[a+1]==-65)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-68)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-69)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-70)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-71)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-74)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-75)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-76)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-77)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-81)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-82)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-83)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-84)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-85)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-86)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-87)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-88)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-89)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-92)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-93)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-94)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-95)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-96)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-97)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-100)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-101)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-102)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-103)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-106)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-107)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-108)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-109)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-113)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-114)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-115)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-116)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-117)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-118)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-119)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-120)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-121)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-124)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-125)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-126)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-127)	line+=irr::core::stringw(L"�");
			if ((int)text[a+1]==-128)	line+=irr::core::stringw(L"�");
			a++;
		}
		else
		{
			line.append(text[a]);
		}
		//printf ("%c",text[a]);
	}
	return line;
#else
	// GCC compiler doesnt seem to support UNICODE, so there is no convertion on Linux. (Until we find a way)
	// Hope this work on Linux, as it did not on windows without the "L"
	irr::core::stringw line = "";
	for (int a=0; a<(int)text.size(); a++)
	{
		// Found out that -61 is the marker for a accent
		// The next character define what type of accent
		//if ((int)text[a]<0 && (int)text[a]!=-61)
			//printf("Found another control code: %d\n",(int)text[a]);
		if ((int)text[a]==-30)
		{
			// Euro Symbol (Might not be available in the font)
			if ((int)text[a+1]==-126 && (int)text[a+2]==-84)line+=irr::core::stringw(L"�");
			a++;
			a++;
		}
		else if ((int)text[a]==-59)
		{
			// Special codes
			//printf("\nAccent found: 1:-59, 2:%d\n",(int)text[a+1]);
			if ((int)text[a+1]==-72)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-109)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-110)	line+=irr::core::stringw("�");
			a++;
		}
		else if ((int)text[a]==-61)
		{
			// Extended character set
			//printf("\nAccent found: 1:-61, 2:%d\n",(int)text[a+1]);
			//printf ("\nAccent found: 1:-61, 2:%x",(int)text[a+1]);
			if ((int)text[a+1]==-65)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-68)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-69)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-70)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-71)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-74)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-75)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-76)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-77)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-81)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-82)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-83)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-84)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-85)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-86)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-87)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-88)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-89)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-92)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-93)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-94)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-95)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-96)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-97)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-100)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-101)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-102)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-103)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-106)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-107)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-108)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-109)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-113)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-114)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-115)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-116)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-117)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-118)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-119)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-120)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-121)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-124)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-125)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-126)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-127)	line+=irr::core::stringw("�");
			if ((int)text[a+1]==-128)	line+=irr::core::stringw("�");
			a++;
		}
		else
		{
			line.append(text[a]);
		}
	}
	return line;
#endif
}
