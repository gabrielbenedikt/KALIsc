#ifndef CTIMETAG_ONCE
#define CTIMETAG_ONCE

#define _CRT_SECURE_NO_DEPRECATE  //supress microsoft warnings
#include <string>  
#include <stdexcept>

typedef long long TimeType;
typedef unsigned char ChannelType;

namespace TimeTag
{

class CUsb;
class CHelper;
class CTimetagReader;
class CLogic;
class CTest;

			

class Exception:public std::runtime_error 
	{
	private:

	public:
		Exception(const char * m):
			std::runtime_error(m){}
	Exception(std::string m):
		runtime_error(m.c_str())
		{
		}

		std::string GetMessageText()
		{
			return what();
		}

	};
			

class CTimeTag
{
private:
   		CUsb *usb;
		CHelper *helper;
		CTimetagReader *reader;
		CLogic *logic;
		CTest *test;
		double caldata[16];
		double resolution;
		
		 
public:

		CTimeTag();
		~CTimeTag();

		CTimetagReader * GetReader();
		CLogic * GetLogic();

		void Open(int nr= 1);
		bool IsOpen();
		void Close();
		void Calibrate();
        void SetInputThreshold(int input, double voltage);
        void SetFilterMinCount(int MinCount);

        void SetFilterMaxTime(int MaxTime);
		 
        double GetResolution()  ;
        int GetFpgaVersion()  ;
        void SetLedBrightness(int percent) ;
        std::string GetErrorText(int flags); 
        void EnableGating(bool enable);
        void GatingLevelMode(bool enable);
        void SetGateWidth(int duration);
		void SwitchSoftwareGate(bool onOff);



        void SetInversionMask(int mask);
        void SetDelay(int Input, int Delay);
        int ReadErrorFlags();
        int GetNoInputs();
        void UseTimetagGate(bool use);
        void UseLevelGate(bool p);
        bool LevelGateActive();
        void Use10MHz(bool use);  
        void SetFilterException(int exception);
		CTest * GetTest();
		CHelper * GetHelper();

		void StartTimetags();
		void StopTimetags();
        int ReadTags(ChannelType*& channel_ret, TimeType *&time_ret);

private:
	ChannelType* channel_data;
	TimeType *time_data;
	int size;
public:
		void PythonInternReadData()
		{
			size = ReadTags(channel_data, time_data);
		}
		void PythonInternGetChannels(unsigned char** channel_ret, int *channel_size)
		{
			*channel_size = size;
			*channel_ret = channel_data;
		}

		void PythonInternGetTimes(long long**time_ret, int*time_size)
		{
			*time_size = size;
			*time_ret = time_data;
			
		}
		//bool ReadNextTag(char & channel, long long &time);
		bool ReadNextTag(ChannelType & channel, TimeType &time);
		void SaveDcCalibration(char * filename);
		void LoadDcCalibration(char * filename);
		void SetFG(int periode, int high);
		void SetFGCount(int periode, int high, int count);

		int GetSingleCount(int i);
		long long FreezeSingleCounter();

		/*
		void RetDummy(long long ** pointer, int *size)
		{
			static long long dummy[100];
			for (int i = 0; i < 100; i++)
				dummy[i] = i;
			*size = 100;
			*pointer = dummy;
		}

		long long DummySum(long long * seq, int n)
		{
			long long ret= 0;
			for (int i = 0; i < n; i++)
				ret += seq[i];
			return ret;
		}
		

		double DummySumDouble(double * seq, int n)
		{
			double ret = 0;
			for (int i = 0; i < n; i++)
				ret += seq[i];
			return ret;
		}

		//int double_it(int x) { return x * 2; }
		
		void Dummy2(long long ** pointer, int *size, long long ** pointer2, int *size2)
		{
			static long long dummy[100];
			for (int i = 0; i < 100; i++)
				dummy[i] = i;
			*size = 100;
			*pointer = dummy;
			*size2 = 4;
			*pointer2 = dummy;
		}
		*/
private:

		void Init();
		double GetDcCalibration(int chan);


};
}

#endif
