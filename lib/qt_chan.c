/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2024 Members of the Gmerlin project
 * http://github.com/bplaum
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/



#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <avdec_private.h>

#include <qt.h>
// #include <utils.h>

int bgav_qt_chan_read(qt_atom_header_t * h, bgav_input_context_t * input,
                      qt_chan_t * ret)
  {
  int i;
  READ_VERSION_AND_FLAGS;
  memcpy(&ret->h, h, sizeof(*h));

  if(!bgav_input_read_32_be(input, &ret->mChannelLayoutTag) ||
     !bgav_input_read_32_be(input, &ret->mChannelBitmap) ||
     !bgav_input_read_32_be(input, &ret->mNumberChannelDescriptions))
    return 0;

  for(i = 0; i < ret->mNumberChannelDescriptions; i++)
    {
    if(!bgav_input_read_32_be(input, &ret->ChannelDescriptions[i].mChannelLabel) ||
       !bgav_input_read_32_be(input, &ret->ChannelDescriptions[i].mChannelFlags) ||
       !bgav_input_read_float_32_be(input, &ret->ChannelDescriptions[i].mCoordinates[0]) ||
       !bgav_input_read_float_32_be(input, &ret->ChannelDescriptions[i].mCoordinates[1]) ||
       !bgav_input_read_float_32_be(input, &ret->ChannelDescriptions[i].mCoordinates[2]))
      return 0;
    }
  return 1;
  }

/* We define all the ugly parts locally */

typedef enum
  {
    CHANNEL_LABEL_Unknown    = 0xFFFFFFFF,   // unknown role or uspecified
                                                // other use for channel
    CHANNEL_LABEL_Unused     = 0,            // channel is present, but
                                                // has no intended role or destination
    CHANNEL_LABEL_UseCoordinates         = 100,  // channel is described
                                                // solely by the mCoordinates fields
    
    CHANNEL_LABEL_Left                   = 1,
    CHANNEL_LABEL_Right                  = 2,
    CHANNEL_LABEL_Center                 = 3,
    CHANNEL_LABEL_LFEScreen              = 4,
    CHANNEL_LABEL_LeftSurround           = 5,    // WAVE (.wav files): "Back Left"
    CHANNEL_LABEL_RightSurround          = 6,    // WAVE: "Back Right"
    CHANNEL_LABEL_LeftCenter             = 7,    
    CHANNEL_LABEL_RightCenter            = 8,
    CHANNEL_LABEL_CenterSurround         = 9,    // WAVE: "Back Center or
                                                    // plain "Rear Surround"
    CHANNEL_LABEL_LeftSurroundDirect     = 10,   // WAVE: "Side Left"
    CHANNEL_LABEL_RightSurroundDirect    = 11,   // WAVE: "Side Right"
    CHANNEL_LABEL_TopCenterSurround      = 12,
    CHANNEL_LABEL_VerticalHeightLeft     = 13,   // WAVE: "Top Front Left
    CHANNEL_LABEL_VerticalHeightCenter   = 14,   // WAVE: "Top Front Center
    CHANNEL_LABEL_VerticalHeightRight    = 15,   // WAVE: "Top Front Right
    CHANNEL_LABEL_TopBackLeft            = 16,
    CHANNEL_LABEL_TopBackCenter          = 17,
    CHANNEL_LABEL_TopBackRight           = 18,
    CHANNEL_LABEL_RearSurroundLeft       = 33,
    CHANNEL_LABEL_RearSurroundRight      = 34,
    CHANNEL_LABEL_LeftWide               = 35,
    CHANNEL_LABEL_RightWide              = 36,
    CHANNEL_LABEL_LFE2                   = 37,
    CHANNEL_LABEL_LeftTotal              = 38,   // matrix encoded 4 channels
    CHANNEL_LABEL_RightTotal             = 39,   // matrix encoded 4 channels
    CHANNEL_LABEL_HearingImpaired        = 40,
    CHANNEL_LABEL_Narration              = 41,
    CHANNEL_LABEL_Mono                   = 42,
    CHANNEL_LABEL_DialogCentricMix       = 43,
    
    CHANNEL_LABEL_CenterSurroundDirect   = 44,   // back center, non diffuse

    // first order ambisonic channels
    
    CHANNEL_LABEL_Ambisonic_W            = 200,
    CHANNEL_LABEL_Ambisonic_X            = 201,
    CHANNEL_LABEL_Ambisonic_Y            = 202,
    CHANNEL_LABEL_Ambisonic_Z            = 203,
    
    // Mid/Side Recording

    CHANNEL_LABEL_MS_Mid                 = 204,
    CHANNEL_LABEL_MS_Side                = 205,
    
    // X-Y Recording

    CHANNEL_LABEL_XY_X                   = 206,
    CHANNEL_LABEL_XY_Y                   = 207,
    
    // other

    CHANNEL_LABEL_HeadphonesLeft         = 301,
    CHANNEL_LABEL_HeadphonesRight        = 302,
    CHANNEL_LABEL_ClickTrack             = 304,
    CHANNEL_LABEL_ForeignLanguage        = 305
  } channel_label_t;

static const struct
  {
  channel_label_t label;
  const char * name;
  }
channel_label_names[] =
  {
    { CHANNEL_LABEL_Unknown,              "Unknown" },
    { CHANNEL_LABEL_Unused,               "Unused" },
                                                // has no intended role or destination
    { CHANNEL_LABEL_UseCoordinates,       "Use Coordinates" },
    
    { CHANNEL_LABEL_Left,                 "Left" },
    { CHANNEL_LABEL_Right,                "Right" },
    { CHANNEL_LABEL_Center,               "Center" },
    { CHANNEL_LABEL_LFEScreen,            "LFE" },
    { CHANNEL_LABEL_LeftSurround,         "Left Surround" },  // WAVE (.wav files): Back Left
    { CHANNEL_LABEL_RightSurround,        "Right Surround" }, // WAVE: "Back Right"
    { CHANNEL_LABEL_LeftCenter,           "Left Center" },
    { CHANNEL_LABEL_RightCenter,          "Right Center" },
    { CHANNEL_LABEL_CenterSurround,       "Center Surround" }, // WAVE: "Back Center or
                                                               // plain "Rear Surround"
    { CHANNEL_LABEL_LeftSurroundDirect,   "Side Left" },       // WAVE: "Side Left"
    { CHANNEL_LABEL_RightSurroundDirect,  "Side Right" },      // WAVE: "Side Right"
    { CHANNEL_LABEL_TopCenterSurround,    "Top Center Surround" },
    { CHANNEL_LABEL_VerticalHeightLeft,   "Top Front Left" },  // WAVE: Top Front Left
    { CHANNEL_LABEL_VerticalHeightCenter, "Top Front Center" },  // WAVE: Top Front Center
    { CHANNEL_LABEL_VerticalHeightRight,  "Top Front Right" },   // WAVE: "Top Front Right"
    { CHANNEL_LABEL_TopBackLeft,          "Top Back Left" }, 
    { CHANNEL_LABEL_TopBackCenter,        "Top Back Center" } ,
    { CHANNEL_LABEL_TopBackRight,         "Top Back Right" },
    { CHANNEL_LABEL_RearSurroundLeft,     "Rear Surround Left" },
    { CHANNEL_LABEL_RearSurroundRight,    "Rear Surround Right" },
    { CHANNEL_LABEL_LeftWide,             "Left Wide" },
    { CHANNEL_LABEL_RightWide,            "Right Wide" },
    { CHANNEL_LABEL_LFE2,                 "LFE 2" },
    { CHANNEL_LABEL_LeftTotal,            "Left Total" },    // matrix encoded 4 channels
    { CHANNEL_LABEL_RightTotal,           "Right Total" },   // matrix encoded 4 channels
    { CHANNEL_LABEL_HearingImpaired,      "Hearing Impaired" },
    { CHANNEL_LABEL_Narration,            "Narration" },
    { CHANNEL_LABEL_Mono,                 "Mono" },
    { CHANNEL_LABEL_DialogCentricMix,     "Dialog Centric Mix" },
    
    { CHANNEL_LABEL_CenterSurroundDirect, "Center Surround Direct" },  // back center, non diffuse

    // first order ambisonic channels
    
    { CHANNEL_LABEL_Ambisonic_W, "Ambisonic W" },
    { CHANNEL_LABEL_Ambisonic_X, "Ambisonic X" },
    { CHANNEL_LABEL_Ambisonic_Y, "Ambisonic Y" },
    { CHANNEL_LABEL_Ambisonic_Z, "Ambisonic Z" },
    
    // Mid/Side Recording

    { CHANNEL_LABEL_MS_Mid,  "MS Mid" },
    { CHANNEL_LABEL_MS_Side,  "MS Side" },
    
    // X-Y Recording

    { CHANNEL_LABEL_XY_X, "Label XY X" },
    { CHANNEL_LABEL_XY_Y, "Label XY Y" },
    
    // other

    { CHANNEL_LABEL_HeadphonesLeft, "Headphones Left" },
    { CHANNEL_LABEL_HeadphonesRight, "Headphones Right" },
    { CHANNEL_LABEL_ClickTrack,      "Click Track" },
    { CHANNEL_LABEL_ForeignLanguage,  "Foreign Language" },
    
  };

static const char * get_channel_name(channel_label_t label)
  {
  int i;
  for(i = 0; i < sizeof(channel_label_names)/sizeof(channel_label_names[0]); i++)
    {
    if(channel_label_names[i].label == label)
      return channel_label_names[i].name;
    }
  return NULL;
  }

typedef enum
  {
    CHANNEL_BIT_Left                 = (1<<0),
    CHANNEL_BIT_Right                = (1<<1),
    CHANNEL_BIT_Center               = (1<<2),
    CHANNEL_BIT_LFEScreen            = (1<<3),
    CHANNEL_BIT_LeftSurround         = (1<<4),   // WAVE: "Back Left"
    CHANNEL_BIT_RightSurround        = (1<<5),   // WAVE: "Back Right"
    CHANNEL_BIT_LeftCenter           = (1<<6),
    CHANNEL_BIT_RightCenter          = (1<<7),
    CHANNEL_BIT_CenterSurround       = (1<<8),   // WAVE: "Back Center"
    CHANNEL_BIT_LeftSurroundDirect   = (1<<9),   // WAVE: "Side Left"
    CHANNEL_BIT_RightSurroundDirect  = (1<<10), // WAVE: "Side Right"
    CHANNEL_BIT_TopCenterSurround    = (1<<11),
    CHANNEL_BIT_VerticalHeightLeft   = (1<<12), // WAVE: "Top Front Left"
    CHANNEL_BIT_VerticalHeightCenter = (1<<13), // WAVE: "Top Front Center"
    CHANNEL_BIT_VerticalHeightRight  = (1<<14), // WAVE: "Top Front Right"
    CHANNEL_BIT_TopBackLeft          = (1<<15),
    CHANNEL_BIT_TopBackCenter        = (1<<16),
    CHANNEL_BIT_TopBackRight         = (1<<17)
  } channel_bit_t;

static const struct
  {
  channel_bit_t bit;
  channel_label_t label;
  }
channel_bits[] =
  {
    { CHANNEL_BIT_Left,                 CHANNEL_LABEL_Left },
    { CHANNEL_BIT_Right,                CHANNEL_LABEL_Right },
    { CHANNEL_BIT_Center,               CHANNEL_LABEL_Center },
    { CHANNEL_BIT_LFEScreen,            CHANNEL_LABEL_LFEScreen },
    { CHANNEL_BIT_LeftSurround,         CHANNEL_LABEL_LeftSurround }, // WAVE: "Back Left"
    { CHANNEL_BIT_RightSurround,        CHANNEL_LABEL_RightSurround },// WAVE: "Back Right"
    { CHANNEL_BIT_LeftCenter,           CHANNEL_LABEL_LeftCenter    },
    { CHANNEL_BIT_RightCenter,          CHANNEL_LABEL_RightCenter },
    { CHANNEL_BIT_CenterSurround,       CHANNEL_LABEL_CenterSurround }, // WAVE: "Back Center"
    { CHANNEL_BIT_LeftSurroundDirect,   CHANNEL_LABEL_LeftSurroundDirect },   // WAVE: "Side Left"
    { CHANNEL_BIT_RightSurroundDirect,  CHANNEL_LABEL_RightSurroundDirect }, // WAVE: "Side Right"
    { CHANNEL_BIT_TopCenterSurround,    CHANNEL_LABEL_TopCenterSurround },
    { CHANNEL_BIT_VerticalHeightLeft,   CHANNEL_LABEL_VerticalHeightLeft }, // WAVE: "Top Front Left"
    { CHANNEL_BIT_VerticalHeightCenter, CHANNEL_LABEL_VerticalHeightCenter }, // WAVE: "Top Front Center"
    { CHANNEL_BIT_VerticalHeightRight,  CHANNEL_LABEL_VerticalHeightRight }, // WAVE: "Top Front Right"
    { CHANNEL_BIT_TopBackLeft,          CHANNEL_LABEL_TopBackLeft },
    { CHANNEL_BIT_TopBackCenter,        CHANNEL_LABEL_TopBackCenter },
    { CHANNEL_BIT_TopBackRight,         CHANNEL_LABEL_TopBackRight },
  };

static channel_label_t channel_bit_2_channel_label(uint32_t bit)
  {
  int i;
  for(i = 0; i < sizeof(channel_bits) / sizeof(channel_bits[0]); i++)
    {
    if(bit == channel_bits[i].bit)
      return channel_bits[i].label;
    }
  return CHANNEL_LABEL_Unknown;
  }

static const struct
  {
  gavl_channel_id_t lqt_channel;
  channel_label_t   channel_label;
  }
lqt_channels[] =
  {
    { GAVL_CHID_NONE,               CHANNEL_LABEL_Unknown },
    { GAVL_CHID_FRONT_LEFT,         CHANNEL_LABEL_Left },
    { GAVL_CHID_FRONT_RIGHT,        CHANNEL_LABEL_Right },
    { GAVL_CHID_FRONT_CENTER,       CHANNEL_LABEL_Center },
    { GAVL_CHID_FRONT_CENTER_LEFT,  CHANNEL_LABEL_LeftCenter },
    { GAVL_CHID_FRONT_CENTER_RIGHT, CHANNEL_LABEL_RightCenter },
    { GAVL_CHID_REAR_LEFT,          CHANNEL_LABEL_LeftSurround },
    { GAVL_CHID_REAR_CENTER,        CHANNEL_LABEL_CenterSurround },
    { GAVL_CHID_REAR_RIGHT,         CHANNEL_LABEL_RightSurround },
    { GAVL_CHID_SIDE_LEFT,          CHANNEL_LABEL_LeftSurroundDirect },
    { GAVL_CHID_SIDE_RIGHT,         CHANNEL_LABEL_RightSurroundDirect },
    { GAVL_CHID_LFE,                CHANNEL_LABEL_LFEScreen },
  };

static gavl_channel_id_t channel_label_2_channel(channel_label_t channel_label)
  {
  int i;
  for(i = 0; i < sizeof(lqt_channels)/sizeof(lqt_channels[0]); i++)
    {
    if(lqt_channels[i].channel_label == channel_label)
      return lqt_channels[i].lqt_channel;
    }
  return GAVL_CHID_NONE;
  }

/* Layout tags */

typedef enum
  {
    CHANNEL_LAYOUT_UseChannelDescriptions = (0<<16) | 0, // use the array of AudioChannelDescriptions to define the mapping.
    CHANNEL_LAYOUT_UseChannelBitmap       = (1<<16) | 0, // use the bitmap to define the mapping.

// 1 Channel Layout

    CHANNEL_LAYOUT_Mono                   = (100<<16) | 1, // a standard mono stream

// 2 Channel layouts

    CHANNEL_LAYOUT_Stereo                 = (101<<16) | 2, // a standard stereo stream (L R)
    CHANNEL_LAYOUT_StereoHeadphones       = (102<<16) | 2, // a standard stereo stream (L R) - implied headphone playback

    CHANNEL_LAYOUT_MatrixStereo           = (103<<16) | 2, // a matrix encoded stereo stream (Lt, Rt)
    CHANNEL_LAYOUT_MidSide                = (104<<16) | 2, // mid/side recording
    CHANNEL_LAYOUT_XY                     = (105<<16) | 2, // coincident mic pair (often 2 figure 8's)

    CHANNEL_LAYOUT_Binaural               = (106<<16) | 2, // binaural stereo (left, right)

// Symetric arrangements - same distance between speaker locations

    CHANNEL_LAYOUT_Ambisonic_B_Format     = (107<<16) | 4, // W, X, Y, Z
    CHANNEL_LAYOUT_Quadraphonic           = (108<<16) | 4, // front left, front right, back left, back right
    CHANNEL_LAYOUT_Pentagonal             = (109<<16) | 5, // left, right, rear left, rear right, center
    CHANNEL_LAYOUT_Hexagonal              = (110<<16) | 6, // left, right, rear left, rear right, center, rear
    CHANNEL_LAYOUT_Octagonal              = (111<<16) | 8, // front left, front right, rear left, rear right,
                                                                 // front center, rear center, side left, side right
    CHANNEL_LAYOUT_Cube                   = (112<<16) | 8, // left, right, rear left, rear right
                                                                 // top left, top right, top rear left, top rear right

//  MPEG defined layouts

    CHANNEL_LAYOUT_MPEG_1_0   = CHANNEL_LAYOUT_Mono,    //  C
    CHANNEL_LAYOUT_MPEG_2_0   = CHANNEL_LAYOUT_Stereo,  //  L R
    CHANNEL_LAYOUT_MPEG_3_0_A = (113<<16) | 3,         // L R C
    CHANNEL_LAYOUT_MPEG_3_0_B = (114<<16) | 3,         // C L R
    CHANNEL_LAYOUT_MPEG_4_0_A = (115<<16) | 4,         // L R C Cs
    CHANNEL_LAYOUT_MPEG_4_0_B = (116<<16) | 4,         // C L R Cs
    CHANNEL_LAYOUT_MPEG_5_0_A = (117<<16) | 5,         // L R C Ls Rs
    CHANNEL_LAYOUT_MPEG_5_0_B = (118<<16) | 5,         // L R Ls Rs C
    CHANNEL_LAYOUT_MPEG_5_0_C = (119<<16) | 5,         // L C R Ls Rs
    CHANNEL_LAYOUT_MPEG_5_0_D = (120<<16) | 5,         // C L R Ls Rs
    CHANNEL_LAYOUT_MPEG_5_1_A = (121<<16) | 6,         // L R C LFE Ls Rs
    CHANNEL_LAYOUT_MPEG_5_1_B = (122<<16) | 6,         // L R Ls Rs C LFE
    CHANNEL_LAYOUT_MPEG_5_1_C = (123<<16) | 6,         // L C R Ls Rs LFE
    CHANNEL_LAYOUT_MPEG_5_1_D = (124<<16) | 6,         // C L R Ls Rs LFE
    CHANNEL_LAYOUT_MPEG_6_1_A = (125<<16) | 7,         // L R C LFE Ls Rs Cs
    CHANNEL_LAYOUT_MPEG_7_1_A = (126<<16) | 8,         // L R C LFE Ls Rs Lc Rc
    CHANNEL_LAYOUT_MPEG_7_1_B = (127<<16) | 8,         // C Lc Rc L R Ls Rs LFE
    CHANNEL_LAYOUT_MPEG_7_1_C = (128<<16) | 8,         // L R C LFE Ls R Rls Rrs
    CHANNEL_LAYOUT_Emagic_Default_7_1 = (129<<16) | 8, //  L R Ls Rs C LFE Lc Rc
    CHANNEL_LAYOUT_SMPTE_DTV          = (130<<16) | 8, //  L R C LFE Ls Rs Lt Rt
                                                             //  (CHANNEL_LAYOUT_ITU_5_1 plus a matrix encoded stereo mix)

//  ITU defined layouts

    CHANNEL_LAYOUT_ITU_1_0    = CHANNEL_LAYOUT_Mono,    //  C
    CHANNEL_LAYOUT_ITU_2_0    = CHANNEL_LAYOUT_Stereo,  //  L R
    CHANNEL_LAYOUT_ITU_2_1    = (131<<16) | 3,         // L R Cs
    CHANNEL_LAYOUT_ITU_2_2    = (132<<16) | 4,         // L R Ls Rs
    CHANNEL_LAYOUT_ITU_3_0    = CHANNEL_LAYOUT_MPEG_3_0_A,  //  L R C
    CHANNEL_LAYOUT_ITU_3_1    = CHANNEL_LAYOUT_MPEG_4_0_A,  //  L R C Cs
    CHANNEL_LAYOUT_ITU_3_2    = CHANNEL_LAYOUT_MPEG_5_0_A,  //  L R C Ls Rs
    CHANNEL_LAYOUT_ITU_3_2_1  = CHANNEL_LAYOUT_MPEG_5_1_A,  //  L R C LFE Ls Rs
    CHANNEL_LAYOUT_ITU_3_4_1  = CHANNEL_LAYOUT_MPEG_7_1_C,  //  L R C LFE Ls Rs Rls Rrs

// DVD defined layouts

    CHANNEL_LAYOUT_DVD_0  = CHANNEL_LAYOUT_Mono,    // C (mono)
    CHANNEL_LAYOUT_DVD_1  = CHANNEL_LAYOUT_Stereo,  // L R
    CHANNEL_LAYOUT_DVD_2  = CHANNEL_LAYOUT_ITU_2_1, // L R Cs
    CHANNEL_LAYOUT_DVD_3  = CHANNEL_LAYOUT_ITU_2_2, // L R Ls Rs
    CHANNEL_LAYOUT_DVD_4  = (133<<16) | 3,                // L R LFE
    CHANNEL_LAYOUT_DVD_5  = (134<<16) | 4,                // L R LFE Cs
    CHANNEL_LAYOUT_DVD_6  = (135<<16) | 5,                // L R LFE Ls Rs
    CHANNEL_LAYOUT_DVD_7  = CHANNEL_LAYOUT_MPEG_3_0_A,// L R C
    CHANNEL_LAYOUT_DVD_8  = CHANNEL_LAYOUT_MPEG_4_0_A,// L R C Cs
    CHANNEL_LAYOUT_DVD_9  = CHANNEL_LAYOUT_MPEG_5_0_A,// L R C Ls Rs
    CHANNEL_LAYOUT_DVD_10 = (136<<16) | 4,                // L R C LFE
    CHANNEL_LAYOUT_DVD_11 = (137<<16) | 5,                // L R C LFE Cs
    CHANNEL_LAYOUT_DVD_12 = CHANNEL_LAYOUT_MPEG_5_1_A,// L R C LFE Ls Rs

    // 13 through 17 are duplicates of 8 through 12.

    CHANNEL_LAYOUT_DVD_13 = CHANNEL_LAYOUT_DVD_8,   // L R C Cs
    CHANNEL_LAYOUT_DVD_14 = CHANNEL_LAYOUT_DVD_9,   // L R C Ls Rs
    CHANNEL_LAYOUT_DVD_15 = CHANNEL_LAYOUT_DVD_10,  // L R C LFE
    CHANNEL_LAYOUT_DVD_16 = CHANNEL_LAYOUT_DVD_11,  // L R C LFE Cs
    CHANNEL_LAYOUT_DVD_17 = CHANNEL_LAYOUT_DVD_12,  // L R C LFE Ls Rs
    CHANNEL_LAYOUT_DVD_18 = (138<<16) | 5,                // L R Ls Rs LFE
    CHANNEL_LAYOUT_DVD_19 = CHANNEL_LAYOUT_MPEG_5_0_B,// L R Ls Rs C
    CHANNEL_LAYOUT_DVD_20 = CHANNEL_LAYOUT_MPEG_5_1_B,// L R Ls Rs C LFE

// These layouts are recommended for Mac OS X's AudioUnit use
// These are the symmetrical layouts

    CHANNEL_LAYOUT_AudioUnit_4= CHANNEL_LAYOUT_Quadraphonic,
    CHANNEL_LAYOUT_AudioUnit_5= CHANNEL_LAYOUT_Pentagonal,
    CHANNEL_LAYOUT_AudioUnit_6= CHANNEL_LAYOUT_Hexagonal,
    CHANNEL_LAYOUT_AudioUnit_8= CHANNEL_LAYOUT_Octagonal,

// These are the surround-based layouts

    CHANNEL_LAYOUT_AudioUnit_5_0  = CHANNEL_LAYOUT_MPEG_5_0_B, // L R Ls Rs C
    CHANNEL_LAYOUT_AudioUnit_6_0  = (139<<16) | 6,        // L R Ls Rs C Cs
    CHANNEL_LAYOUT_AudioUnit_7_0  = (140<<16) | 7,        // L R Ls Rs C Rls Rrs
    CHANNEL_LAYOUT_AudioUnit_5_1  = CHANNEL_LAYOUT_MPEG_5_1_A, // L R C LFE Ls Rs
    CHANNEL_LAYOUT_AudioUnit_6_1  = CHANNEL_LAYOUT_MPEG_6_1_A, // L R C LFE Ls Rs Cs
    CHANNEL_LAYOUT_AudioUnit_7_1  = CHANNEL_LAYOUT_MPEG_7_1_C, // L R C LFE Ls Rs Rls Rrs

// These layouts are used for AAC Encoding within the MPEG-4 Specification

    CHANNEL_LAYOUT_AAC_Quadraphonic   = CHANNEL_LAYOUT_Quadraphonic, // L R Ls Rs
    CHANNEL_LAYOUT_AAC_4_0= CHANNEL_LAYOUT_MPEG_4_0_B,  // C L R Cs
    CHANNEL_LAYOUT_AAC_5_0= CHANNEL_LAYOUT_MPEG_5_0_D,  // C L R Ls Rs
    CHANNEL_LAYOUT_AAC_5_1= CHANNEL_LAYOUT_MPEG_5_1_D,  // C L R Ls Rs Lfe
    CHANNEL_LAYOUT_AAC_6_0= (141<<16) | 6,            // C L R Ls Rs Cs
    CHANNEL_LAYOUT_AAC_6_1= (142<<16) | 7,            // C L R Ls Rs Cs Lfe
    CHANNEL_LAYOUT_AAC_7_0= (143<<16) | 7,            // C L R Ls Rs Rls Rrs
    CHANNEL_LAYOUT_AAC_7_1= CHANNEL_LAYOUT_MPEG_7_1_B, // C Lc Rc L R Ls Rs Lfe
    CHANNEL_LAYOUT_AAC_Octagonal  = (144<<16) | 8,    // C L R Ls Rs Rls Rrs Cs
    CHANNEL_LAYOUT_TMH_10_2_std   = (145<<16) | 16,   // L R C Vhc Lsd Rsd Ls Rs Vhl Vhr Lw Rw Csd Cs LFE1 LFE2
    CHANNEL_LAYOUT_TMH_10_2_full  = (146<<16) | 21,   // TMH_10_2_std plus: Lc Rc HI VI Haptic
    CHANNEL_LAYOUT_RESERVED_DO_NOT_USE= (147<<16)
} channel_layout_t;

static const struct
  {
  channel_layout_t layout;
  const channel_label_t * channels;
  }
channel_locations[] =
  {
    { CHANNEL_LAYOUT_Mono, (const channel_label_t[]){ CHANNEL_LABEL_Center } }, // a standard mono stream

// 2 Channel layouts

    { CHANNEL_LAYOUT_Stereo,  (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                   CHANNEL_LABEL_Right } }, // a standard stereo stream (L R)

    { CHANNEL_LAYOUT_StereoHeadphones, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                            CHANNEL_LABEL_Right } }, // a standard stereo stream (L R) - implied headphone playback
    { CHANNEL_LAYOUT_MatrixStereo, (const channel_label_t[]){ CHANNEL_LABEL_LeftTotal,
                                                        CHANNEL_LABEL_RightTotal } }, // a matrix encoded stereo stream (Lt, Rt)

    { CHANNEL_LAYOUT_MidSide, (const channel_label_t[]){ CHANNEL_LABEL_MS_Mid,
                                                   CHANNEL_LABEL_MS_Side } }, // mid/side recording

    { CHANNEL_LAYOUT_XY, (const channel_label_t[]){ CHANNEL_LABEL_XY_X, CHANNEL_LABEL_XY_Y } }, // coincident mic pair (often 2 figure 8's)

    { CHANNEL_LAYOUT_Binaural, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                    CHANNEL_LABEL_Right  } }, // binaural stereo (left, right)

// Symetric arrangements - same distance between speaker locations

    { CHANNEL_LAYOUT_Ambisonic_B_Format, (const channel_label_t[]){ CHANNEL_LABEL_Ambisonic_W,
                                                              CHANNEL_LABEL_Ambisonic_X,
                                                              CHANNEL_LABEL_Ambisonic_Y,
                                                              CHANNEL_LABEL_Ambisonic_Z } }, // W, X, Y, Z

    { CHANNEL_LAYOUT_Quadraphonic, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                        CHANNEL_LABEL_Right,
                                                        CHANNEL_LABEL_LeftSurround,
                                                        CHANNEL_LABEL_RightSurround } }, // front left, front right, back left, back right

    { CHANNEL_LAYOUT_Pentagonal, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_Center } }, // left, right, rear left, rear right, center

    { CHANNEL_LAYOUT_Hexagonal, (const channel_label_t[]){  CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_CenterSurround } }, // left, right, rear left, rear right, center, rear

    { CHANNEL_LAYOUT_Octagonal, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                     CHANNEL_LABEL_Right,
                                                     CHANNEL_LABEL_LeftSurround,
                                                     CHANNEL_LABEL_RightSurround,
                                                     CHANNEL_LABEL_Center,
                                                     CHANNEL_LABEL_CenterSurround,
                                                     CHANNEL_LABEL_LeftSurroundDirect,
                                                     CHANNEL_LABEL_RightSurroundDirect } }, // front left, front right, rear left, rear right,
                                                                                            // front center, rear center, side left, side right

    { CHANNEL_LAYOUT_Cube, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                CHANNEL_LABEL_Right,
                                                CHANNEL_LABEL_LeftSurround,
                                                CHANNEL_LABEL_RightSurround,
                                                CHANNEL_LABEL_VerticalHeightLeft,
                                                CHANNEL_LABEL_VerticalHeightRight,
                                                CHANNEL_LABEL_TopBackLeft,
                                                CHANNEL_LABEL_TopBackRight } }, // left, right, rear left, rear right
                                                                                // top left, top right, top rear left, top rear right

//  MPEG defined layouts

    { CHANNEL_LAYOUT_MPEG_3_0_A, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_Center } },         // L R C

    { CHANNEL_LAYOUT_MPEG_4_0_A, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_CenterSurround } },         // L R C Cs

    { CHANNEL_LAYOUT_MPEG_5_0_A, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround } },         // L R C Ls Rs

    { CHANNEL_LAYOUT_MPEG_5_1_A, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_LFEScreen,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround } },         // L R C LFE Ls Rs

    { CHANNEL_LAYOUT_MPEG_6_1_A, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_LFEScreen,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_CenterSurround } },         // L R C LFE Ls Rs Cs

    { CHANNEL_LAYOUT_MPEG_7_1_A, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_LFEScreen,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_LeftCenter,
                                                      CHANNEL_LABEL_RightCenter } }, // L R C LFE Ls Rs Lc Rc
    
    { CHANNEL_LAYOUT_MPEG_5_0_B, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_Center } },         // L R Ls Rs C

    { CHANNEL_LAYOUT_MPEG_5_1_B, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_LFEScreen } },         // L R Ls Rs C LFE

    { CHANNEL_LAYOUT_MPEG_3_0_B,  (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                       CHANNEL_LABEL_Left,
                                                       CHANNEL_LABEL_Right } },        // C L R

    { CHANNEL_LAYOUT_MPEG_4_0_B, (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_CenterSurround } },         // C L R Cs

    { CHANNEL_LAYOUT_MPEG_5_0_D, (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround } },         // C L R Ls Rs

    { CHANNEL_LAYOUT_MPEG_5_1_D, (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_LFEScreen } },         // C L R Ls Rs LFE
    
    { CHANNEL_LAYOUT_MPEG_5_0_C, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround } },         // L C R Ls Rs

    { CHANNEL_LAYOUT_MPEG_5_1_C, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_LFEScreen } },         // L C R Ls Rs LFE

    { CHANNEL_LAYOUT_MPEG_7_1_B, (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_LeftCenter,
                                                      CHANNEL_LABEL_RightCenter,
                                                      CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_LFEScreen  } },         // C Lc Rc L R Ls Rs LFE

    { CHANNEL_LAYOUT_MPEG_7_1_C, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_LFEScreen,
                                                      CHANNEL_LABEL_LeftSurroundDirect,
                                                      CHANNEL_LABEL_RightSurroundDirect,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround  } },         // L R C LFE Ls Rs Rls Rrs

    { CHANNEL_LAYOUT_Emagic_Default_7_1,  (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                               CHANNEL_LABEL_Right,
                                                               CHANNEL_LABEL_LeftSurround,
                                                               CHANNEL_LABEL_RightSurround,
                                                               CHANNEL_LABEL_Center,
                                                               CHANNEL_LABEL_LFEScreen,
                                                               CHANNEL_LABEL_LeftCenter,
                                                               CHANNEL_LABEL_RightCenter } },//  L R Ls Rs C LFE Lc Rc

    { CHANNEL_LAYOUT_SMPTE_DTV,  (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                      CHANNEL_LABEL_Right,
                                                      CHANNEL_LABEL_Center,
                                                      CHANNEL_LABEL_LFEScreen,
                                                      CHANNEL_LABEL_LeftSurround,
                                                      CHANNEL_LABEL_RightSurround,
                                                      CHANNEL_LABEL_LeftTotal,
                                                      CHANNEL_LABEL_RightTotal } },         //  L R C LFE Ls Rs Lt Rt
                                                      //  (CHANNEL_LAYOUT_ITU_5_1 plus a matrix encoded stereo mix)

//  ITU defined layouts

    { CHANNEL_LAYOUT_ITU_2_1, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                   CHANNEL_LABEL_Right,
                                                   CHANNEL_LABEL_CenterSurround } },            // L R Cs

    { CHANNEL_LAYOUT_ITU_2_2, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                   CHANNEL_LABEL_Right,
                                                   CHANNEL_LABEL_LeftSurround,
                                                   CHANNEL_LABEL_RightSurround } },            // L R Ls Rs

// DVD defined layouts

    { CHANNEL_LAYOUT_DVD_4, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                 CHANNEL_LABEL_Right,
                                                 CHANNEL_LABEL_LFEScreen } },              // L R LFE
    { CHANNEL_LAYOUT_DVD_5, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                 CHANNEL_LABEL_Right,
                                                 CHANNEL_LABEL_LFEScreen,
                                                 CHANNEL_LABEL_CenterSurround } },              // L R LFE Cs
    
    { CHANNEL_LAYOUT_DVD_6,  (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                  CHANNEL_LABEL_Right,
                                                  CHANNEL_LABEL_LFEScreen,
                                                  CHANNEL_LABEL_LeftSurround,
                                                  CHANNEL_LABEL_RightSurround } },             // L R LFE Ls Rs
    
    { CHANNEL_LAYOUT_DVD_10, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                  CHANNEL_LABEL_Right,
                                                  CHANNEL_LABEL_Center,
                                                  CHANNEL_LABEL_LFEScreen  } },             // L R C LFE
    
    { CHANNEL_LAYOUT_DVD_11,  (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                   CHANNEL_LABEL_Right,
                                                   CHANNEL_LABEL_Center,
                                                   CHANNEL_LABEL_LFEScreen,
                                                   CHANNEL_LABEL_CenterSurround } },            // L R C LFE Cs
    
    // 13 through 17 are duplicates of 8 through 12.

    { CHANNEL_LAYOUT_DVD_18, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                  CHANNEL_LABEL_Right,
                                                  CHANNEL_LABEL_LeftSurround,
                                                  CHANNEL_LABEL_RightSurround,
                                                  CHANNEL_LABEL_LFEScreen } },             // L R Ls Rs LFE
    
    // These are the surround-based layouts
    
    { CHANNEL_LAYOUT_AudioUnit_6_0, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                         CHANNEL_LABEL_Right,
                                                         CHANNEL_LABEL_LeftSurround,
                                                         CHANNEL_LABEL_RightSurround,
                                                         CHANNEL_LABEL_Center,
                                                         CHANNEL_LABEL_CenterSurround   } },      // L R Ls Rs C Cs

    { CHANNEL_LAYOUT_AudioUnit_7_0, (const channel_label_t[]){ CHANNEL_LABEL_Left,
                                                         CHANNEL_LABEL_Right,
                                                         CHANNEL_LABEL_LeftSurroundDirect,
                                                         CHANNEL_LABEL_RightSurroundDirect,
                                                         CHANNEL_LABEL_Center,
                                                         CHANNEL_LABEL_LeftSurround,
                                                         CHANNEL_LABEL_RightSurround } },      // L R Ls Rs C Rls Rrs

// These layouts are used for AAC Encoding within the MPEG-4 Specification

    { CHANNEL_LAYOUT_AAC_6_0, (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                   CHANNEL_LABEL_Left,
                                                   CHANNEL_LABEL_Right,
                                                   CHANNEL_LABEL_LeftSurround,
                                                   CHANNEL_LABEL_RightSurround,
                                                   CHANNEL_LABEL_CenterSurround  } },            // C L R Ls Rs Cs

    { CHANNEL_LAYOUT_AAC_6_1, (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                   CHANNEL_LABEL_Left,
                                                   CHANNEL_LABEL_Right,
                                                   CHANNEL_LABEL_LeftSurround,
                                                   CHANNEL_LABEL_RightSurround,
                                                   CHANNEL_LABEL_CenterSurround,
                                                   CHANNEL_LABEL_LFEScreen  } },            // C L R Ls Rs Cs Lfe

    { CHANNEL_LAYOUT_AAC_7_0, (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                   CHANNEL_LABEL_Left,
                                                   CHANNEL_LABEL_Right,
                                                   CHANNEL_LABEL_LeftSurroundDirect,
                                                   CHANNEL_LABEL_RightSurroundDirect,
                                                   CHANNEL_LABEL_LeftSurround,
                                                   CHANNEL_LABEL_RightSurround  } },            // C L R Ls Rs Rls Rrs
    
    { CHANNEL_LAYOUT_AAC_Octagonal, (const channel_label_t[]){ CHANNEL_LABEL_Center,
                                                         CHANNEL_LABEL_Left,
                                                         CHANNEL_LABEL_Right,
                                                         CHANNEL_LABEL_LeftSurroundDirect,
                                                         CHANNEL_LABEL_RightSurroundDirect,
                                                         CHANNEL_LABEL_LeftSurround,
                                                         CHANNEL_LABEL_RightSurround,
                                                         CHANNEL_LABEL_CenterSurround  } },      // C L R Ls Rs Rls Rrs Cs
    
    /* No, sorry the following 2 a to weird. The one who has such files, can program it */
    //    { CHANNEL_LAYOUT_TMH_10_2_std, (const channel_label_t[]){  } },       // L R C Vhc Lsd Rsd Ls Rs Vhl Vhr Lw Rw Csd Cs LFE1 LFE2
    //    { CHANNEL_LAYOUT_TMH_10_2_full, (const channel_label_t[]){  } },      // TMH_10_2_std plus: Lc Rc HI VI Haptic
      
  };

static const channel_label_t * get_channel_locations(uint32_t layout, int * num_channels)
  {
  int i;
  *num_channels = layout & 0xffff;
  for(i = 0; i < sizeof(channel_locations)/sizeof(channel_locations[0]); i++)
    {
    if(channel_locations[i].layout == layout)
      {
      return channel_locations[i].channels;
      }
    }
  return NULL;
  }

void bgav_qt_chan_dump(int indent, qt_chan_t * chan)
  {
  const channel_label_t * channel_labels;
  int num_channels;
  int i, j;
  uint32_t mask;
  gavl_dprintf( "channel description (chan)\n");
  gavl_dprintf( "  version                     %d\n", chan->version);
  gavl_dprintf( "  flags                       %d\n", chan->flags);
  gavl_dprintf( "  mChannelLayoutTag:          0x%08x", chan->mChannelLayoutTag);

  if(chan->mChannelLayoutTag == CHANNEL_LAYOUT_UseChannelDescriptions)
    {
    gavl_dprintf( " [Use channel descriptions]\n");
    }
  else if(chan->mChannelLayoutTag == CHANNEL_LAYOUT_UseChannelBitmap)
    {
    gavl_dprintf( " [Use channel bitmap]\n");
    }
  else
    {
    channel_labels = get_channel_locations(chan->mChannelLayoutTag, &num_channels);
    
    gavl_dprintf( " [");

    if(channel_labels)
      {
      for(i = 0; i < num_channels; i++)
        {
        gavl_dprintf( "%s", get_channel_name(channel_labels[i]));
        if(i < num_channels-1)
        gavl_dprintf( ", ");
        }
      }
    else
      gavl_dprintf( "Not available");
    
    gavl_dprintf( "]\n");
    }
  
  gavl_dprintf( "  mChannelBitmap:             0x%08x", chan->mChannelBitmap);

  if(chan->mChannelLayoutTag == CHANNEL_LAYOUT_UseChannelBitmap)
    {
    gavl_dprintf( " [");
    j = 0;
    mask = 1;
    for(i = 0; i < 32; i++)
      {
      if(chan->mChannelBitmap & mask)
        {
        if(j)
          gavl_dprintf( ", ");
        gavl_dprintf( "%s", get_channel_name(channel_bit_2_channel_label(mask)));
        j++;
        }
      mask <<= 1;
      }
    gavl_dprintf( "]\n");
    }
  else
    gavl_dprintf( "\n");
   
  
  gavl_dprintf( "        mNumberChannelDescriptions: %d\n", chan->mNumberChannelDescriptions);
  for(i = 0; i < chan->mNumberChannelDescriptions; i++)
    {
    gavl_dprintf( "         mChannelLabel[%d]: 0x%08x [%s]\n", i,
           chan->ChannelDescriptions[i].mChannelLabel, get_channel_name(chan->ChannelDescriptions[i].mChannelLabel));
    gavl_dprintf( "         mChannelFlags[%d]: 0x%08x\n", i,
           chan->ChannelDescriptions[i].mChannelFlags);
    gavl_dprintf( "         mCoordinates[%d]: [%f %f %f]\n", i,
           chan->ChannelDescriptions[i].mCoordinates[0],
           chan->ChannelDescriptions[i].mCoordinates[1],
           chan->ChannelDescriptions[i].mCoordinates[2]);
    }

  }

void bgav_qt_chan_free(qt_chan_t * chan)
  {
  if(chan->ChannelDescriptions)
    free(chan->ChannelDescriptions);
  }


void bgav_qt_chan_get(qt_chan_t * chan, gavl_audio_format_t * format)
  {
  int i, num_channels;
  uint32_t mask;
  const channel_label_t * channel_labels;

  if(chan->mChannelLayoutTag == CHANNEL_LAYOUT_UseChannelDescriptions)
    {
    format->num_channels = chan->mNumberChannelDescriptions;
    for(i = 0; i < chan->mNumberChannelDescriptions; i++)
      format->channel_locations[i] = channel_label_2_channel(chan->ChannelDescriptions[i].mChannelLabel);
    }
  else if(chan->mChannelLayoutTag == CHANNEL_LAYOUT_UseChannelBitmap)
    {
    /* Count the channels */
    mask = 1;
    num_channels = 0;
    for(i = 0; i < 32; i++)
      {
      if(chan->mChannelBitmap & mask)
        num_channels++;
      mask <<= 1;
      }

    /* Set the channels */
    format->num_channels = num_channels;
    
    mask = 1;
    num_channels = 0;
    for(i = 0; i < 32; i++)
      {
      if(chan->mChannelBitmap & mask)
        {
        format->channel_locations[num_channels] =
          channel_label_2_channel(channel_bit_2_channel_label(mask));
        num_channels++;
        }
      mask <<= 1;
      }
    }
  else /* Predefined channel layout */
    {
    channel_labels = get_channel_locations(chan->mChannelLayoutTag, &num_channels);

    format->num_channels = num_channels;

    if(channel_labels)
      {
      for(i = 0; i < num_channels; i++)
        format->channel_locations[i] = channel_label_2_channel(channel_labels[i]);
      }
    
    }
  
  }
