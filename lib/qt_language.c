/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
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

#include <avdec_private.h>

#include <qt.h>

/* Definitions for charsets. These are mappings from Macintosh charset
   symbols to iconv charsets. The ones, which are set to NULL
   are not available in iconv (replacements??) */

#define smRoman            "MACINTOSH"
#define smHebrew           NULL
#define smJapanese         NULL
#define smArabic           NULL
#define smExtArabic        NULL
#define smGreek            NULL
#define smCentralEuroRoman NULL
#define smIcelandic        "MAC-IS"
#define smTradChinese      NULL
#define smDevanagari       NULL
#define smThai             NULL
#define smKorean           NULL
#define smSami             "MAC-SAMI"
#define smCyrillic         "MAC-CYRILLIC"
#define smSimpChinese      NULL
#define smCeltic           NULL
#define smRomanian         NULL
#define smUkrainian        "MAC-UK"
#define smArmenian         NULL
#define smGeorgian         NULL
#define smMongolian        NULL
#define smTibetan          NULL
#define smBengali          NULL
#define smGuriati          NULL
#define smGurmukhi         NULL
#define smOriya            NULL
#define smMalayalam        NULL
#define smKannada          NULL
#define smTamil            NULL
#define smTelugu           NULL
#define smSinhalese        NULL
#define smBurmese          NULL
#define smKhmer            NULL
#define smLaotian          NULL
#define smVietnamese       NULL
#define smEthiopic         NULL

/* Language / character set codecs */

static const struct
  {
  int  mac_code;  // Integer mac code
  const char language[4];   // 3 character language code
  const char * charset; // Character set (understood by iconv_open)
  }
mac_languages[] =
  {
    {   0, "eng", smRoman }, // English
    {   1, "fra", smRoman }, // French
    {   2, "ger", smRoman }, // German
    {   3, "ita", smRoman }, // Italian
    {   4, "dut", smRoman }, // Dutch
    {   5, "swe", smRoman }, // Swedish
    {   6, "spa", smRoman }, // Spanish
    {   7, "dan", smRoman }, // Danish
    {   8, "por", smRoman }, // Portuguese
    {   9, "nor", smRoman }, // Norwegian
    {  10, "heb", smHebrew }, // Hebrew
    {  11, "jpn", smJapanese }, // Japanese
    {  12, "ara", smArabic }, // Arabic
    {  13, "fin", smRoman }, // Finnish
    {  14, "gre", smGreek }, // Greek
    {  15, "ice", smIcelandic }, // Icelandic
    {  16, "mlt", smRoman }, // Maltese
    {  17, "tur", smRoman }, // Turkish
    {  18, "scr", smRoman }, // Croatian
    {  19, "chi", smTradChinese }, // Traditional Chinese
    {  20, "urd", smArabic }, // Urdu
    {  21, "hin", smDevanagari }, // Hindi
    {  22, "tha", smThai }, // Thai
    {  23, "kor", smKorean}, // Korean
    {  24, "lit", smCentralEuroRoman }, // Lithuanian
    {  25, "pol", smCentralEuroRoman }, // Polish
    {  26, "hun", smCentralEuroRoman }, // Hungarian
    {  27, "est", smCentralEuroRoman }, // Estonian
    {  28, "lav", smCentralEuroRoman }, // Latvian
    {  29, "smi", smSami }, // Saamisk
    {  30, "fao", smIcelandic}, // Faeroese
    {  31, "far", smArabic }, // Farsi
    {  32, "rus", smCyrillic }, // Russian
    {  33, "chi", smSimpChinese }, // Simplified Chinese
    {  34, "dut", smRoman }, // Flemish
    {  35, "gle", smCeltic }, // Irish
    {  36, "alb", smRoman }, // Albanian
    {  37, "rum", smRomanian }, // Romanian
    {  38, "cze", smCentralEuroRoman }, // Czech
    {  39, "slo", smCentralEuroRoman }, // Slovak
    {  40, "slv", smCentralEuroRoman }, // Slovenian
    {  41, "yid", smHebrew }, // Yiddish
    {  42, "scc", smCyrillic }, // Serbian
    {  43, "mac", smCyrillic }, // Macedonian
    {  44, "bul", smCyrillic }, // Bulgarian
    {  45, "ukr", smUkrainian }, // Ukrainian
    {  46, "bel", smCyrillic }, // Byelorussian
    {  47, "uzb", smCyrillic }, // Uzbek
    {  48, "kaz", smCyrillic }, // Kazakh
    {  49, "aze", smCyrillic }, // Azerbaijani (cyrillic)
    {  50, "aze", smArabic }, // Azerbaijani (arabic)
    {  51, "arm", smArmenian }, // Armenian
    {  52, "geo", smGeorgian }, // Georgian
    {  53, "mol", smCyrillic }, // Moldavian
    {  54, "kir", smCyrillic }, // Kirghiz
    {  55, "tgk", smCyrillic }, // Tajiki
    {  56, "tuk", smCyrillic }, // Turkmen
    {  57, "mon", smMongolian }, // Mongolian
    {  58, "mon", smCyrillic }, // Mongolian (cyrillic)
    {  59, "pus", smArabic }, // Pashto
    {  60, "kur", smArabic }, // Kurdish
    {  61, "kas", smArabic }, // Kashmiri
    {  62, "snd", smExtArabic }, // Sindhi
    {  63, "tib", smTibetan }, // Tibetan
    {  64, "nep", smDevanagari }, // Nepali
    {  65, "san", smDevanagari }, // Sanskrit
    //    {  66, "", smDevanagari }, // Marathi ??
    {  67, "ben", smBengali }, // Bengali
    {  68, "asm", smBengali }, // Assamese
    {  69, "guj", smGuriati }, // Gujarati
    {  70, "pan", smGurmukhi }, // Punjabi
    {  71, "ori", smOriya }, // Oriya
    {  72, "mal", smMalayalam }, // Malayalam
    {  73, "kan", smKannada }, // Kannada
    {  74, "tam", smTamil }, // Tamil
    {  75, "tel", smTelugu }, // Telugu
    {  76, "sin", smSinhalese }, // Sinhalese
    {  77, "bur", smBurmese }, // Burmese
    {  78, "khm", smKhmer }, // Khmer
    {  79, "lao", smLaotian }, // Lao
    {  80, "vie", smVietnamese }, // Vietnamese
    {  81, "ind", smRoman }, // Indonesian
    {  82, "tgl", smRoman }, // Tagalog
    {  83, "may", smRoman }, // Malay (roman)
    {  84, "may", smArabic }, // Malay (arabic)
    {  85, "amh", smEthiopic }, // Amharic
    {  86, "tir", smEthiopic }, // Tigrinya
    {  87, "orm", smEthiopic }, // Oromo
    {  88, "som", smRoman }, // Somali
    {  89, "swa", smRoman }, // Swahili
    {  90, "kin", smRoman }, // Kinyarwanda
    {  91, "run", smRoman }, // Rundi
    {  92, "nya", smRoman }, // Chewa
    {  93, "mlg", smRoman }, // Malagasy
    {  94, "epo", smRoman }, // Esperanto
    { 128, "wel", smRoman }, // Welsh
    { 129, "baq", smRoman }, // Basque
    { 130, "cat", smRoman }, // Catalan
    { 131, "lat", smRoman }, // Latin
    { 132, "que", smRoman }, // Quechua
    { 133, "grn", smRoman }, // Guarani
    { 134, "aym", smRoman }, // Aymara
    { 135, "tat", smCyrillic }, // Tatar
    { 136, "uig", smArabic }, // Uighur
    { 137, "dzo", smTibetan }, // Dzongkha
    { 138, "jav", smRoman }, // Javanese (roman)
  };

#define NUM_CODES (sizeof(mac_languages)/sizeof(mac_languages[0]))

int bgav_qt_get_language(int mac_code, char * ret)
  {
  int i;
  for(i = 0; i < NUM_CODES; i++)
    {
    if(mac_code == mac_languages[i].mac_code)
      {
      strcpy(ret, mac_languages[i].language);
      return 1;
      }
    }
  return 0;
  }

const char * bgav_qt_get_charset(int mac_code)
  {
  int i;
  for(i = 0; i < NUM_CODES; i++)
    {
    if(mac_code == mac_languages[i].mac_code)
      {
      return mac_languages[i].charset;
      }
    }
  return NULL;
  }
