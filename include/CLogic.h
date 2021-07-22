#ifndef CLOGIC_ONCE
#define CLOGIC_ONCE
#include "CTimeTag.h"


namespace TimeTag
{
    class CLogic
    {
        int* logic;
		bool ecSet;
		bool owSet;
        CHelper *helper;
		CUsb *usb;
        CTimeTag *ttInterface;
		time_t oldTime;
		long long tmCounter;
		static const int maxDelay = (1 << 24) - 1;
		static const int inputs = 16;
        static const int lSize = 1 << 16;



	private:
		int bitCount(int p);
        void InitOutputs();
		

	public:
		 CLogic(CTimeTag *tti);


        void SwitchLogicMode();
        
        void SetWindowWidth(int window);
		void SetWindowWidthEx(int index, int window);
        void SetDelay(int input, int delay);
        long long ReadLogic();
        int CalcCountPos(int pattern);

        int CalcCount(int pos, int neg);
        long long GetTimeCounter();

        void SetOutputWidth(int width);

        void SetOutputPattern(int output, int pos, int neg);
        void SetOutputEventCount(int events);





    };
}
#endif