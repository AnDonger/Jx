#include "KCore.h"
#include <math.h>
#include "GameDataDef.h"
#include "KMath.h"


#ifdef _USBKEY
    #include "USBKey/epsJO.h" 

    //---------------------------------------------------------------------------
    // 正弦表 (将浮点数 *1024 整型化)
    int		*g_nSin = NULL;

    // 余弦表 (将浮点数 *1024 整型化)
    int		*g_nCos = NULL;

    // 正弦余弦的查表函数代码缓冲区
    unsigned char *g_InternalDirSinCosCode = NULL;


    //---------------------------------------------------------------------------
    // 五行相生相克

    // 五行相生相克函数代码缓冲区
    unsigned char *g_InternalIsAccrueConquerCode = NULL;
    //---------------------------------------------------------------------------

    int g_nEPJInitFlag = false;

    // {982CD92C-027E-4638-8BD4-5855FFB6DCC1}
	const unsigned char g_cExeData[16] = { 
        0x98, 0x2c, 0xd9, 0x2c, 0x02, 0x7e, 0x46, 0x38, 
        0x8b, 0xd4, 0x58, 0x55, 0xff, 0xb6, 0xdc, 0xc1 
    };


    int _ReadDataFromUSBKey(int nSize, unsigned char abyBuffer[], int *pnRetSize, int nIndex)
    {
        int nResult  = false;
        int nRetCode = false;

		int   nReadSize = 0;
		unsigned char *pbyReadBuf = NULL;

        if (pnRetSize)
            *pnRetSize = 0;

        if (nSize <= 0)
            goto Exit0;

        if (!abyBuffer)
            goto Exit0;
        
        if (nIndex < 0)
            goto Exit0;

		nRetCode = epj_ReadKeyCode(
            (const char *)g_cExeData, sizeof(g_cExeData),
            (char **)(&pbyReadBuf), &nReadSize,
            nIndex
        );
        if (!nRetCode)
            goto Exit0;
        
        if (!pbyReadBuf)
            goto Exit0;

        if (nReadSize <= 0)
            goto Exit0;

        if (nReadSize > nSize)
            goto Exit0;

        memcpy(abyBuffer, pbyReadBuf, nReadSize);

        if (pnRetSize)
            *pnRetSize = nReadSize;

        free(pbyReadBuf);
        pbyReadBuf = NULL;
        nReadSize  = 0;

        nResult = true;
    Exit0:
        return nResult;
    }

    int g_InitMath()
    {
        int nResult = false;
        int nRetCode = false;

        if (g_nEPJInitFlag)
            goto Exit1;

        g_nSin = new int [1024 / sizeof(int)];
        if (!g_nSin)
            goto Exit0;

        g_nCos = new int [1024 / sizeof(int)];
        if (!g_nCos)
            goto Exit0;

        g_InternalDirSinCosCode = new unsigned char [1024];
        if (!g_InternalDirSinCosCode)
            goto Exit0;

        g_InternalIsAccrueConquerCode = new unsigned char [1024];
        if (!g_InternalIsAccrueConquerCode)
            goto Exit0;

        // 这个时候开始从USBKey中读入相应的数据
	    nRetCode = epj_Init("rockey");
        if (!nRetCode)
            goto Exit0;

        g_nEPJInitFlag = true;

        nRetCode = _ReadDataFromUSBKey(
            1024, (unsigned char *)g_nSin, NULL, 
            0
        );
        if (!nRetCode)
            goto Exit0;

        nRetCode = _ReadDataFromUSBKey(
            1024, (unsigned char *)g_nCos, NULL, 
            1
        );
        if (!nRetCode)
            goto Exit0;
        
        nRetCode = _ReadDataFromUSBKey(
            1024, (unsigned char *)g_InternalDirSinCosCode, NULL, 
            2
        );
        if (!nRetCode)
            goto Exit0;
        

        nRetCode = _ReadDataFromUSBKey(
            1024, (unsigned char *)g_InternalIsAccrueConquerCode, NULL, 
            3
        );
        if (!nRetCode)
            goto Exit0;

    Exit1:
        nResult = true;
    Exit0:
        if (!nResult)
        {
            if (g_nEPJInitFlag)
            {
                epj_UnInit();
                g_nEPJInitFlag = false;
            }

            if (g_InternalIsAccrueConquerCode)
            {
                delete []g_InternalIsAccrueConquerCode;
                g_InternalIsAccrueConquerCode = NULL;
            }

            if (g_InternalDirSinCosCode)
            {
                delete []g_InternalDirSinCosCode;
                g_InternalDirSinCosCode = NULL;
            }

            if (g_nCos)
            {
                delete []g_nCos;
                g_nCos = NULL;
            }

            if (g_nSin)
            {
                delete []g_nSin;
                g_nSin = NULL;
            }
        }
        return nResult;
    }

    int g_UnInitMath()
    {
        int nResult = false;

        if (g_nEPJInitFlag)
        {
            epj_UnInit();
            g_nEPJInitFlag = false;
        }


        if (g_InternalIsAccrueConquerCode)
        {
            delete []g_InternalIsAccrueConquerCode;
            g_InternalIsAccrueConquerCode = NULL;
        }

        if (g_InternalDirSinCosCode)
        {
            delete []g_InternalDirSinCosCode;
            g_InternalDirSinCosCode = NULL;
        }

        if (g_nCos)
        {
            delete []g_nCos;
            g_nCos = NULL;
        }

        if (g_nSin)
        {
            delete []g_nSin;
            g_nSin = NULL;
        }

        nResult = true;
    //Exit0:
        return nResult;
    }



#else
// 不需要加载USBKey

    // 正弦表 (将浮点数 *1024 整型化)
    int		g_nSinBuffer[64] = 
    {
	    1024,	1019,	1004,	979,	946,	903,	851,	791,	
        724,	649,	568,	482,	391,	297,	199,	100,	
        0,	   -100,	-199,	-297,	-391,	-482,	-568,	-649,	
        -724,	-791,	-851,	-903,	-946,	-979,	-1004,	-1019,	
        -1024,	-1019,	-1004,	-979,	-946,	-903,	-851,	-791,	
        -724,	-649,	-568,	-482,	-391,	-297,	-199,	-100,	
        0,	     100,	199,	297,	391,	482,	568,	649,	
        724,	791,	851,	903,	946,	979,	1004,	1019
    };

    //---------------------------------------------------------------------------
    // 余弦表 (将浮点数 *1024 整型化)
    int		g_nCosBuffer[64] = 
    {
	    0,	    -100,	-199,	-297,	-391,	-482,	-568,	-649,	
        -724,	-791,	-851,	-903,	-946,	-979,	-1004,	-1019,	
        -1024,	-1019,	-1004,	-979,	-946,	-903,	-851,	-791,	
        -724,	-649,	-568,	-482,	-391,	-297,	-199,	-100,	
        0,	     100,	199,	297,	391,	482,	568,	649,	
        724,	791,	851,	903,	946,	979,	1004,	1019,	
        1024,	1019,	1004,	979,	946,	903,	851,	791,	
        724,	649,	568,	482,	391,	297,	199,	100,
    };

    // 正弦余弦的查表函数代码缓冲区
    unsigned char g_InternalDirSinCosCodeBuffer[] =
    {
        0x8B, 0x44, 0x24, 0x08, 0x85, 0xC0, 0x7C, 0x16, 
        0x8B, 0x4C, 0x24, 0x0C, 0x3B, 0xC1, 0x7D, 0x0E, 
        0xC1, 0xE0, 0x06, 0x99, 0xF7, 0xF9, 0x8B, 0x4C, 
        0x24, 0x04, 0x8B, 0x04, 0x81, 0xC3, 0x83, 0xC8, 
        0xFF, 0xC3
    };

    // 正弦余弦的查表函数代码
    //int	__cdecl g_InternalDirSinCos(int pSinCosTable[], int nDir, int nMaxDir)
    //{
    //	if (nDir < 0 || nDir >= nMaxDir)
    //		return -1;
    //
    //	int	nIndex = (nDir << 6) / nMaxDir;
    //
    //	return pSinCosTable[nIndex];
    //}


    // 五行相生相克函数代码缓冲区
    unsigned char g_InternalIsAccrueConquerCodeBuffer[] =
    {
        0x8B, 0x44, 0x24, 0x08, 0x85, 0xC0, 0x7C, 0x1E, 
        0x83, 0xF8, 0x05, 0x7D, 0x19, 0x8B, 0x54, 0x24, 
        0x0C, 0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x33, 
        0xC9, 0x8B, 0x3C, 0x86, 0x3B, 0xD7, 0x5F, 0x0F, 
        0x94, 0xC1, 0x8B, 0xC1, 0x5E, 0xC3, 0x33, 0xC0, 
        0xC3
    };

    // 五行相生相克函数代码
    //int __cdecl g_InternalIsAccrueConquer(int pAccrueConquerTable[], int nSrcSeries, int nDesSeries)
    //{
    //	if (nSrcSeries < 0 || nSrcSeries >= series_num)
    //		return false;
    //
    //	return (nDesSeries == pAccrueConquerTable[nSrcSeries]);
    //}

    //---------------------------------------------------------------------------
    // 正弦表 (将浮点数 *1024 整型化)
    int		*g_nSin = g_nSinBuffer;

    // 余弦表 (将浮点数 *1024 整型化)
    int		*g_nCos = g_nCosBuffer;

    // 正弦余弦的查表函数代码缓冲区
    unsigned char *g_InternalDirSinCosCode = g_InternalDirSinCosCodeBuffer;


    //---------------------------------------------------------------------------
    // 五行相生相克

    // 五行相生相克函数代码缓冲区
    unsigned char *g_InternalIsAccrueConquerCode = g_InternalIsAccrueConquerCodeBuffer;
    //---------------------------------------------------------------------------

    int g_InitMath()
    {
        return true;

    }

    int g_UnInitMath()
    {
        return true;
    }

#endif

//---------------------------------------------------------------------------
// 五行相生相克
int		g_nAccrueSeries[series_num];
int		g_nConquerSeries[series_num];

void g_InitSeries()
{
	g_nAccrueSeries[series_metal] = series_water;
	g_nConquerSeries[series_metal] = series_wood;
	g_nAccrueSeries[series_wood] = series_fire;
	g_nConquerSeries[series_wood] = series_earth;
	g_nAccrueSeries[series_water] = series_wood;
	g_nConquerSeries[series_water] = series_fire;
	g_nAccrueSeries[series_fire] = series_earth;
	g_nConquerSeries[series_fire] = series_metal;
	g_nAccrueSeries[series_earth] = series_metal;
	g_nConquerSeries[series_earth] = series_water;
}



// Release/Client 当前版 0x7948C0 的玩家寻路方向计算。
// 查表数据与参考客户端 KMath.h 一致；不要改变战斗距离和怪物的旧数学接口。
static const unsigned int g_PlayerMoveSqrtTable[256] =
{
	531980127, 532026288, 532072271, 532118079, 532163712, 532209174, 532254465, 532299589,
	532344546, 532389339, 532433970, 532478440, 532522750, 532566903, 532610900, 532654744,
	532698434, 532741974, 532785365, 532828607, 532871704, 532914655, 532957463, 533000129,
	533042654, 533085041, 533127289, 533169401, 533211378, 533253220, 533294931, 533336509,
	533377958, 533419278, 533460470, 533501535, 533542475, 533583291, 533623984, 533664554,
	533705004, 533745334, 533785545, 533825638, 533865615, 533905476, 533945222, 533984855,
	534024374, 534063782, 534103079, 534142267, 534181345, 534220315, 534259178, 534297934,
	534336585, 534375132, 534413574, 534451914, 534490152, 534528288, 534566324, 534604260,
	534642098, 534679837, 534717478, 534755023, 534792473, 534829827, 534867086, 534904252,
	534941325, 534978305, 535015194, 535051992, 535088699, 535125317, 535161846, 535198287,
	535234640, 535270905, 535307085, 535343178, 535379187, 535415110, 535450950, 535486706,
	535522379, 535557970, 535593480, 535628908, 535664255, 535699523, 535734711, 535769820,
	535804850, 535839803, 535874678, 535909476, 535944198, 535978844, 536013414, 536047910,
	536082331, 536116678, 536150952, 536185153, 536219281, 536253337, 536287322, 536321235,
	536355078, 536388850, 536422553, 536456186, 536489750, 536523246, 536556673, 536590033,
	536623325, 536656551, 536689709, 536722802, 536755829, 536788791, 536821688, 536854520,
	536887280, 536919921, 536952436, 536984827, 537017094, 537049241, 537081267, 537113174,
	537144963, 537176637, 537208195, 537239640, 537270972, 537302193, 537333304, 537364306,
	537395200, 537425987, 537456669, 537487246, 537517720, 537548091, 537578361, 537608530,
	537638600, 537668572, 537698446, 537728224, 537757906, 537787493, 537816986, 537846387,
	537875696, 537904913, 537934040, 537963078, 537992027, 538020888, 538049662, 538078350,
	538106952, 538135470, 538163903, 538192254, 538220521, 538248707, 538276812, 538304837,
	538332781, 538360647, 538388434, 538416144, 538443776, 538471332, 538498812, 538526217,
	538553548, 538580804, 538607987, 538635097, 538662136, 538689102, 538715997, 538742822,
	538769577, 538796263, 538822880, 538849428, 538875909, 538902322, 538928668, 538954949,
	538981163, 539007312, 539033396, 539059416, 539085373, 539111265, 539137095, 539162863,
	539188568, 539214212, 539239794, 539265316, 539290778, 539316180, 539341522, 539366806,
	539392031, 539417197, 539442306, 539467358, 539492352, 539517290, 539542171, 539566997,
	539591768, 539616483, 539641143, 539665749, 539690301, 539714800, 539739245, 539763637,
	539787976, 539812264, 539836499, 539860682, 539884815, 539908896, 539932927, 539956907,
	539980838, 540004718, 540028549, 540052332, 540076065, 540099750, 540123387, 540146976,
	540170517, 540194011, 540217458, 540240858, 540264211, 540287519, 540310780, 540333996,
};

int g_GetPlayerMoveDirIndex(int nX1, int nY1, int nX2, int nY2)
{
	if (nX1 == nX2 && nY1 == nY2)
		return -1;
	long long nDeltaX = (long long)nX2 - nX1;
	long long nDeltaY = (long long)nY2 - nY1;
	long long nSquare = nDeltaX * nDeltaX + nDeltaY * nDeltaY;
	int nDistance;
	if (nSquare > 0x7fffffff)
	{
		nDistance = (int)(sqrt((double)nSquare) + 0.5);
	}
	else
	{
		union
		{
			unsigned int Bits;
			float Value;
		} Number;
		Number.Value = (float)nSquare;
		Number.Bits = ((Number.Bits >> 1) & 0x3f800000) +
			g_PlayerMoveSqrtTable[(Number.Bits >> 16) & 255];
		Number.Value += 12582912.0f;
		nDistance = (int)Number.Bits - 1262485504;
	}
	if (nDistance <= 0)
		return -1;
	int nSin = (int)(nDeltaY * 1024 / nDistance);
	int nDir = 0;
	for (int i = 0; i < 32; i++)
	{
		if (nSin > g_nSin[i])
			break;
		nDir = i;
	}
	return nDeltaX > 0 ? 63 - nDir : nDir;
}
