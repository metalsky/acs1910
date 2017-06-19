
#include <drv_imgs.h>


DRV_ImgsIpipeConfig gDRV_imgsIpipeConfig_Vnfdemo = {

  .ipipeifParams = {

    .vpiIsifInDpcEnable	    = FALSE,
    .vpiIsifInDpcThreshold  = 0,
    .ddrInDpcEnable         = FALSE,
    .ddrInDpcThreshold      = 0,
    .gain                   = 0x200,
    .outClip                = 0xFFF,
    .avgFilterEnable        = FALSE,

  },

  .ipipeParams = {

    .colPat = {

      .colPat = {

        { CSL_IPIPE_SOURCE_COLOUR_GR, CSL_IPIPE_SOURCE_COLOUR_R   },
        { CSL_IPIPE_SOURCE_COLOUR_B , CSL_IPIPE_SOURCE_COLOUR_GB  },

      },
    },

    .lsc = {
      0
    },

    .dpc = {
      .lutEnable = FALSE,
      .otfEnable = TRUE,
      .otfType = CSL_IPIPE_DPC_OTF_DEFECT_DETECTION_METHOD_MAX2_MIN1, //MINMAX2
      .otfAlg = CSL_IPIPE_DPC_OTF_ALG_DPC2,  //DPC2.0
      .otf2DetThres = {1024, 1024, 1024, 1024},
      .otf2CorThres = {512, 512, 512, 512},
    },

    .nf1 = {
      .enable = FALSE,
      .spreadVal = 4,
      .spreadValSrc = CSL_IPIPE_NF_SPR_SRC_SPR_REG,
      .lutAddrShift = 0,
      .greenSampleMethod = CSL_IPIPE_NF_GREEN_SAMPLING_METHOD_BOX,
      .lscGainEnable = 0,

      .edgeDetectThresMin = 0,
      .edgeDetectThresMax = 2047,

      .lutThresTable = {
        10, 19, 28, 37,
        37, 42, 47, 53,
      },
      .lutIntensityTable = {
        31, 29, 27, 26,
        26, 17,  8,  0,
      },
      .lutSpreadTable = {
         0,  0,  0,  0,
         0,  0,  0,  0,
      },
    },

    .nf2 = {
      .enable = TRUE,
      .spreadVal = 3,
      .spreadValSrc = CSL_IPIPE_NF_SPR_SRC_SPR_REG,
      .lutAddrShift = 0,
      .greenSampleMethod = CSL_IPIPE_NF_GREEN_SAMPLING_METHOD_BOX,
      .lscGainEnable = 0,
      .edgeDetectThresMin = 0,
      .edgeDetectThresMax = 2047,
      .lutThresTable = {
        23, 33, 44, 54,
        63, 72, 81, 90,
      },
      .lutIntensityTable = {
        16, 15, 15, 14,
        13, 12, 11, 10,
      },
      .lutSpreadTable = {
         0,  0,  0,  0,
         0,  0,  0,  0,
      },
    },

    .gic = {
      .enable = FALSE,
    },

    .wb = {
      .offset = {
        0, 0, 0, 0
      },
      .gain = {
        0x200, 0x200, 0x200, 0x200
      },
    },

    .cfa = {
      .mode = CSL_IPIPE_CFA_MODE_2DAC,
      .twoDirHpValLowThres = 600,
      .twoDirHpValSlope    = 57,
      .twoDirHpMixThres    = 10,
      .twoDirHpMixSlope    = 10,
      .twoDirDirThres      = 4,
      .twoDirDirSlope      = 10,
      .twoDirNonDirWeight  = 16,
      .daaHueFrac          = 24,
      .daaEdgeThres        = 25,
      .daaThresMin         = 27,
      .daaThresSlope       = 20,
      .daaSlopeMin         = 50,
      .daaSlopeSlope       = 40,
      .daaLpWeight         = 16,

    },

    .rgb2rgb1 = {
#if 0 //original
      .matrix = {
        { 0x0100, 0x0000, 0x0000 },
        { 0x0000, 0x0100, 0x0000 },
        { 0x0000, 0x0000, 0x0100 },
      },
#else //Gang: tuning for CCM
      .matrix = {
        { 507, 4096-286, 35},
        { 4096-131, 401, 4096-14},
        { 4096-68, 4096-321, 644},
      },
#endif
      .offset = {
        0, 0, 0
      },

    },

	.gamma = {
		.tableSize = CSL_IPIPE_GAMMA_CORRECTION_TABLE_SIZE_512,
		.tableSrc = CSL_IPIPE_GAMMA_CORRECTION_TABLE_SELECT_RAM,
      .bypassR = FALSE,
      .bypassG = FALSE,
      .bypassB = FALSE,
    },

    .gammaTableR = {
			#include "gamma_MT9P031_5MP.txt"
    },
    .gammaTableG = {
			#include "gamma_MT9P031_5MP.txt"
    },
    .gammaTableB = {
			#include "gamma_MT9P031_5MP.txt"
    },

    .rgb2rgb2 = {

#if 1 //original
      .matrix = {
        { 0x0100, 0x0000, 0x0000 },
        { 0x0000, 0x0100, 0x0000 },
        { 0x0000, 0x0000, 0x0100 },
      },
#else //Gang: tuning for CCM
      .matrix = {
        { 258, 2048-10, 8},
        { 4, 247, 5},
        { 2048-3, 7, 253},
      },
#endif
      .offset = {
        0, 0, 0
      },
    },

    .lut3d = {
      .enable = FALSE,
    },

    .rgb2yuv = {

      .matrix = {
        { 0x004d, 0x0096, 0x001d },
        { 0x0fd5, 0x0fab, 0x0080 },
        { 0x0080, 0x0f95, 0x0feb },
      },
      .offset = {
        0x00, 0x80, 0x80
      },

      .cLpfEnable = FALSE,
      .cPos = CSL_IPIPE_YUV_CHROMA_POS_LUM,
    },

    .cntBrt = {
      .brightness = 0x00,
      .contrast   = 0x10,
    },

    .gbce = {
      .enable = FALSE,
    },

    .yee = {
      .enable = FALSE,
    },

    .car = {
      .enable = FALSE,
    },

    .cgs = {
      .enable = FALSE,
    },
  },
};

DRV_ImgsIpipeConfig gDRV_imgsIpipeConfig_Common = {

  .ipipeifParams = {

    .vpiIsifInDpcEnable	    = FALSE,
    .vpiIsifInDpcThreshold  = 0,
    .ddrInDpcEnable         = FALSE,
    .ddrInDpcThreshold      = 0,
    .gain                   = 0x200,
    .outClip                = 0xFFF,
    .avgFilterEnable        = FALSE,

  },

  .ipipeParams = {

    .colPat = {

      .colPat = {

        { CSL_IPIPE_SOURCE_COLOUR_GR, CSL_IPIPE_SOURCE_COLOUR_R   },
        { CSL_IPIPE_SOURCE_COLOUR_B , CSL_IPIPE_SOURCE_COLOUR_GB  },

      },
    },

    .lsc = {
      0
    },

    .dpc = {
      .lutEnable = FALSE,
      .otfEnable = TRUE,
      .otfType = CSL_IPIPE_DPC_OTF_DEFECT_DETECTION_METHOD_MAX2_MIN1, //MINMAX2
      .otfAlg = CSL_IPIPE_DPC_OTF_ALG_DPC2,  //DPC2.0
      .otf2DetThres = {1024, 1024, 1024, 1024},
      .otf2CorThres = {512, 512, 512, 512},
    },

    .nf1 = {
      .enable = FALSE,
      .spreadVal = 4,
      .spreadValSrc = CSL_IPIPE_NF_SPR_SRC_SPR_REG,
      .lutAddrShift = 0,
      .greenSampleMethod = CSL_IPIPE_NF_GREEN_SAMPLING_METHOD_BOX,
      .lscGainEnable = 0,

      .edgeDetectThresMin = 0,
      .edgeDetectThresMax = 2047,

      .lutThresTable = {
        10, 19, 28, 37,
        37, 42, 47, 53,
      },
      .lutIntensityTable = {
        31, 29, 27, 26,
        26, 17,  8,  0,
      },
      .lutSpreadTable = {
         0,  0,  0,  0,
         0,  0,  0,  0,
      },
    },

    .nf2 = {
      .enable = TRUE,
      .spreadVal = 3,
      .spreadValSrc = CSL_IPIPE_NF_SPR_SRC_SPR_REG,
      .lutAddrShift = 0,
      .greenSampleMethod = CSL_IPIPE_NF_GREEN_SAMPLING_METHOD_BOX,
      .lscGainEnable = 0,
      .edgeDetectThresMin = 0,
      .edgeDetectThresMax = 2047,
      .lutThresTable = {
        23, 33, 44, 54,
        63, 72, 81, 90,
      },
      .lutIntensityTable = {
        16, 15, 15, 14,
        13, 12, 11, 10,
      },
      .lutSpreadTable = {
         0,  0,  0,  0,
         0,  0,  0,  0,
      },
    },

    .gic = {
      .enable = FALSE,
    },

    .wb = {
      .offset = {
        0, 0, 0, 0
      },
      .gain = {
        0x200, 0x200, 0x200, 0x200
      },
    },

    .cfa = {
      .mode = CSL_IPIPE_CFA_MODE_2DAC,
      .twoDirHpValLowThres = 600,
      .twoDirHpValSlope    = 57,
      .twoDirHpMixThres    = 10,
      .twoDirHpMixSlope    = 10,
      .twoDirDirThres      = 4,
      .twoDirDirSlope      = 10,
      .twoDirNonDirWeight  = 16,
      .daaHueFrac          = 24,
      .daaEdgeThres        = 25,
      .daaThresMin         = 27,
      .daaThresSlope       = 20,
      .daaSlopeMin         = 50,
      .daaSlopeSlope       = 40,
      .daaLpWeight         = 16,

    },

    .rgb2rgb1 = {

      .matrix = {
        { 0x0100, 0x0000, 0x0000 },
        { 0x0000, 0x0100, 0x0000 },
        { 0x0000, 0x0000, 0x0100 },
      },
      .offset = {
        0, 0, 0
      },

    },

	.gamma = {
		.tableSize = CSL_IPIPE_GAMMA_CORRECTION_TABLE_SIZE_512,
		.tableSrc = CSL_IPIPE_GAMMA_CORRECTION_TABLE_SELECT_RAM,
      .bypassR = FALSE,
      .bypassG = FALSE,
      .bypassB = FALSE,
    },

    .gammaTableR = {
			#include "gamma_MT9P031_5MP.txt"
    },
    .gammaTableG = {
			#include "gamma_MT9P031_5MP.txt"
    },
    .gammaTableB = {
			#include "gamma_MT9P031_5MP.txt"
    },

    .rgb2rgb2 = {

      .matrix = {
        { 0x0100, 0x0000, 0x0000 },
        { 0x0000, 0x0100, 0x0000 },
        { 0x0000, 0x0000, 0x0100 },
      },
      .offset = {
        0, 0, 0
      },
    },

    .lut3d = {
      .enable = FALSE,
    },

    .rgb2yuv = {

      .matrix = {
        { 0x004d, 0x0096, 0x001d },
        { 0x0fd5, 0x0fab, 0x0080 },
        { 0x0080, 0x0f95, 0x0feb },
      },
      .offset = {
        0x00, 0x80, 0x80
      },

      .cLpfEnable = FALSE,
      .cPos = CSL_IPIPE_YUV_CHROMA_POS_LUM,
    },

    .cntBrt = {
      .brightness = 0x00,
      .contrast   = 0x10,
    },

    .gbce = {
      .enable = FALSE,
    },

    .yee = {
      .enable = FALSE,
    },

    .car = {
      .enable = FALSE,
    },

    .cgs = {
      .enable = FALSE,
    },
  },
};

