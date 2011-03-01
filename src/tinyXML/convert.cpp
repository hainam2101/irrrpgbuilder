#include "convert.h"

//Tries to convert  accented characters from UT8
irr::core::stringw convert(irr::core::stringc text)
{
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
			if ((int)text[a+1]==-126 && (int)text[a+2]==-84) line+=L"�";
			a++;
			a++;
		}
		else if ((int)text[a]==-59)
		{
			// Special codes
			//printf("\nAccent found: 1:-59, 2:%d\n",(int)text[a+1]);
			if ((int)text[a+1]==-72)	line+=L"�";
			if ((int)text[a+1]==-109)	line+=L"�";
			if ((int)text[a+1]==-110)	line+=L"�";
			a++;
		}
		else if ((int)text[a]==-61)
		{
			// Extended character set
			//printf("\nAccent found: 1:-61, 2:%d\n",(int)text[a+1]);
			if ((int)text[a+1]==-65)	line+=L"�";
			if ((int)text[a+1]==-68)	line+=L"�";
			if ((int)text[a+1]==-69)	line+=L"�";
			if ((int)text[a+1]==-70)	line+=L"�";
			if ((int)text[a+1]==-71)	line+=L"�";
			if ((int)text[a+1]==-74)	line+=L"�";
			if ((int)text[a+1]==-75)	line+=L"�";
			if ((int)text[a+1]==-76)	line+=L"�";
			if ((int)text[a+1]==-77)	line+=L"�";
			if ((int)text[a+1]==-81)	line+=L"�";
			if ((int)text[a+1]==-82)	line+=L"�";
			if ((int)text[a+1]==-83)	line+=L"�";
			if ((int)text[a+1]==-84)	line+=L"�";
			if ((int)text[a+1]==-85)	line+=L"�";
			if ((int)text[a+1]==-86)	line+=L"�";
			if ((int)text[a+1]==-87)	line+=L"�";
			if ((int)text[a+1]==-88)	line+=L"�";
			if ((int)text[a+1]==-89)	line+=L"�";
			if ((int)text[a+1]==-92)	line+=L"�";
			if ((int)text[a+1]==-93)	line+=L"�";
			if ((int)text[a+1]==-94)	line+=L"�";
			if ((int)text[a+1]==-95)	line+=L"�";
			if ((int)text[a+1]==-96)	line+=L"�";
			if ((int)text[a+1]==-97)	line+=L"�";
			if ((int)text[a+1]==-100)	line+=L"�";
			if ((int)text[a+1]==-101)	line+=L"�";
			if ((int)text[a+1]==-102)	line+=L"�";
			if ((int)text[a+1]==-103)	line+=L"�";
			if ((int)text[a+1]==-106)	line+=L"�";
			if ((int)text[a+1]==-107)	line+=L"�";
			if ((int)text[a+1]==-108)	line+=L"�";
			if ((int)text[a+1]==-109)	line+=L"�";
			if ((int)text[a+1]==-113)	line+=L"�";
			if ((int)text[a+1]==-114)	line+=L"�";
			if ((int)text[a+1]==-115)	line+=L"�";
			if ((int)text[a+1]==-116)	line+=L"�";
			if ((int)text[a+1]==-117)	line+=L"�";
			if ((int)text[a+1]==-118)	line+=L"�";
			if ((int)text[a+1]==-119)	line+=L"�";
			if ((int)text[a+1]==-120)	line+=L"�";
			if ((int)text[a+1]==-121)	line+=L"�";
			if ((int)text[a+1]==-124)	line+=L"�";
			if ((int)text[a+1]==-125)	line+=L"�";
			if ((int)text[a+1]==-126)	line+=L"�";
			if ((int)text[a+1]==-127)	line+=L"�";
			if ((int)text[a+1]==-128)	line+=L"�";
			a++;
		}
		else
		{
			line.append(text[a]);
		}
		//printf ("%c",text[a]);
	}
	return line;
}