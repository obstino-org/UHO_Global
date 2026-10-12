// UHO Global v3.0 for Windows
// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment (lib, "comctl32.lib")

#include <windows.h>
#include <windowsx.h>
#include <Richedit.h>
#include <CommCtrl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <codecvt>
#include <thread>
#include <map>
#include <regex>
#include <queue>
#include <mutex>
#include <filesystem>
#include <io.h>
#include <fcntl.h>

#include "nemo_realfeed.h"
#include "nemo_punct_capit.h"
//#include "nemo_punct_capit_OK_ON_WINDOWS.h"
#include "download.h"
#define IDI_ICON1	101

#define SX GetSystemMetrics(SM_CXSCREEN)
#define SY GetSystemMetrics(SM_CYSCREEN)

#define DLWND_WIDTH		350
#define DLWND_HEIGHT	250
#define DLWND_OK_ID		10
#define DLWND_CANCEL_ID	11
#define DLWND_MSG_PERCENTAGE (WM_APP+1)
#define DLWND_MSG_FINISHED (WM_APP+2)

#define OVERLAY_WIDTH	1000
#define OVERLAY_HEIGHT	80
#define LINE_HEIGHT	(OVERLAY_HEIGHT/2)

#define BASE_WIDTH 270
#define MAIN_WINDOW_WIDTH (BASE_WIDTH + 70)
#define MAIN_WINDOW_HEIGHT 330
#define TOP_OFFSET 20	// first button y offset

#define IDC_RADIO_QUALITY1	100
#define IDC_RADIO_QUALITY2	101
#define IDC_RADIO_QUALITY3	102
#define IDC_RADIO_QUALITY4	103
#define IDC_GROUPBOX1		104

#define IDC_RADIO_SOURCE_MIC	200
#define IDC_RADIO_SOURCE_AUDIO	201
#define IDC_GROUPBOX2			202

#define IDC_BUTTON_STARTSTOP	300

#define IDC_CHECKBOX_SHOWCAPTIONS	400
#define IDC_CHECKBOX_SPEAKERCHANGE	401

#define ID_EDIT_COPY 500

#define IDC_COMBO_LANGUAGE	600
#define IDC_BUTTON_DELETE 601

#define IDC_BUTTON_DONATE 700

HWND hWndOverlay = NULL;
HWND hWndPrintout = NULL;
int editFontSize = 40;

HBRUSH hbrBack = CreateSolidBrush(RGB(255, 255, 255));
HBRUSH hbrBlack = CreateSolidBrush(RGB(0, 0, 0));

HFONT font = CreateFont(
	LINE_HEIGHT, 0,
	0, 0, FW_NORMAL,
	false, false, false,
	DEFAULT_CHARSET,
	OUT_DEFAULT_PRECIS,
	CLIP_DEFAULT_PRECIS,
	CLEARTYPE_QUALITY,
	DEFAULT_PITCH | FF_SWISS,
	L"Arial");

HFONT hFontGui = CreateFont(
	19,
	0,
	0,
	0,
	FW_NORMAL,
	FALSE,
	FALSE,
	FALSE,
	ANSI_CHARSET,
	OUT_DEFAULT_PRECIS,
	CLIP_DEFAULT_PRECIS,
	DEFAULT_QUALITY,
	DEFAULT_PITCH,
	L"Segoe UI");

HFONT hFontGuiSm = CreateFont(
	14,
	0,
	0,
	0,
	FW_NORMAL,
	FALSE,
	FALSE,
	FALSE,
	ANSI_CHARSET,
	OUT_DEFAULT_PRECIS,
	CLIP_DEFAULT_PRECIS,
	DEFAULT_QUALITY,
	DEFAULT_PITCH,
	L"Segoe UI");

class LanguageInfo {
public:
	wstring comboText;	// dropdown combo-box text for this language
	//wstring modelPath;
	//wstring vocabPath;
	//wstring urlModel;
	//wstring urlVocab;
	map<wstring, UrlFileInfo> urlFileMap;

	bool unspacedText;	// for char based models, such as Japanese model
	bool isConformer;
	float timeBias;
	bool suppressTokens;
	TokenizerType pncTokType;
	vector<wstring> pncSymbols;
	bool pncUseCapitalLetters;

	LanguageInfo(
		wstring comboText,
		map<wstring, UrlFileInfo> urlFileMap,
		bool isConformer = false,
		float timeBias = 0.0f,
		bool unspacedText = false,
		bool suppressTokens = false,
		TokenizerType pncTokType = TokenizerType::none,
		vector<wstring> pncSymbols = {},
		bool pncUseCapitalLetters = true
		):
			comboText(comboText), urlFileMap(urlFileMap), isConformer(isConformer), timeBias(timeBias), unspacedText(unspacedText), suppressTokens(suppressTokens), pncTokType(pncTokType), pncSymbols(pncSymbols), pncUseCapitalLetters(pncUseCapitalLetters)
		{}
};
/*
vector<LanguageInfo> langInfo = {
	LanguageInfo(L"Arabic (ar-AR)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe128_ar-AR_3.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe128_ar-AR_3.0.txt")}}, true, 0.024f),
	LanguageInfo(L"Belarusian (be-BY)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_by_fastconformer_hybrid_large_pc.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_by_fastconformer_hybrid_large_pc.txt")}}, false, 0.0f),
	LanguageInfo(L"Brazilian Portuguese (pt-BR)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_bpe1024_pt-BR_1.2.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_bpe1024_pt-BR_1.2.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base_pt-BR_Riva-PnC-SET-1.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_pt.txt")}}, true, 0.024f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Catalan (ca)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_ca_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_ca_conformer_ctc_large.txt")}}, true, 0.070f),
	//LanguageInfo(L"Chinese (zh-CN)-English (en-US)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_punct_zh_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_zh.txt")}}, true, 0.0f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?", L"。", L"，", L"？"}, true),
	LanguageInfo(L"Chinese (zh-CN)-English (en-US)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.txt")}}, true, 0.0f, false, false),	// PnC not included as it doesn't predict punctuation well for Chinese
	LanguageInfo(L"Croatian (hr)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_hr_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_hr_conformer_ctc_large.txt")}}, true, 0.0f, false, true),
	LanguageInfo(L"Dutch (nl)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe256_nl-NL_1.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe256_nl-NL_1.0.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base-nl-NL_1.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_nl.txt")}}, true, 0.100f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),	// bias determined manually
	//LanguageInfo(L"English (en-US)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe128_en-US_6.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe128_en-US_6.0.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base_en-US_4.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_en.txt")}}, true, 0.0f, false, true, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	
	// TEST ONLY (TODO TEST URL)
	LanguageInfo(L"English (en-US)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe128_en-US_6.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe128_en-US_6.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe128_en-US_6.0.txt", L"ASRVOCAB_Conformer-CTC-L_spe128_en-US_6.0.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_en-US_4.0_quantized_weights.onnx", L"PNCMODEL_bert-base_en-US_4.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_en.txt", L"PNCVOCAB_en.txt")}}, true, 0.0f, false, true, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	
	LanguageInfo(L"Esperanto (eo)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_eo_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_eo_conformer_ctc_large.txt")}}, true, 0.035f),
	LanguageInfo(L"French (fr)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_fr_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_fr_conformer_ctc_large.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base_fr_fr_Riva-PnC-SET-2.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_fr.txt")}}, true, 0.1f, false, false, TokenizerType::BERT, {L"", L",", L"-", L".", L"?"}, true),
	LanguageInfo(L"German (de)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_punct_de_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_de.txt")}}, true, 0.00f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Hindi (hi)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base_hi-IN_Riva-PnC-SET-1.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_hi.txt")}}, true, 0.0f, false, false, TokenizerType::BERT, {L"", L"!", L",", L"?", L"।"}, false),
	LanguageInfo(L"Italian (it)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe1024_it-IT_2.1.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe1024_it-IT_2.1.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base_it-IT_Riva-PnC-SET-1.1_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_it.txt")}}, true, 0.100f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Japanese (ja-JP)-English (en-JP)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.txt")}}, true, -0.016f, true),
	LanguageInfo(L"Kinyarwanda (rw)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_rw_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_rw_conformer_ctc_large.txt")}}, true, 0.140f),
	LanguageInfo(L"Korean (ko-KR)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base_ko-KR_Riva-PnC-SET-1.1_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_ko.txt")}}, true, -0.024f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, false),
	LanguageInfo(L"Marathi (mr)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_mr_conformer_ctc_medium.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_mr_conformer_ctc_medium.txt")}}, true, -0.135f),
	LanguageInfo(L"Persian (fa)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_fa_fastconformer_hybrid_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_fa_fastconformer_hybrid_large.txt")}}, false, -0.056f),
	LanguageInfo(L"Polish (pl)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_pl_fastconformer_hybrid_large_pc.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_pl_fastconformer_hybrid_large_pc.txt")}}, false, 0.237f),
	LanguageInfo(L"Russian (ru)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_ru_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_ru_conformer_ctc_large.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base_ru-RU_Riva-PnC-SET-1.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_ru.txt")}}, true, -0.016f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Slovenian (sl-SI)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_sl.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_sl.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_punct_sl_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_sl.txt")}}, true, 0.0f, false, true, TokenizerType::RoBERTa, {L"", L"!", L",", L".", L"?"}, true), // 0.016f),
	LanguageInfo(L"Spanish (es-US)-English (en-US)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.txt")}, {L"pnc_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCMODEL_bert-base_es-US_Riva-PnC-SET-2.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\pnc\\final\\PNCVOCAB_es.txt")}}, true, -0.016f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?", L"¿"}, true),
	LanguageInfo(L"Ukrainian (ua)", {{L"asr_model", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRMODEL_stt_ua_fastconformer_hybrid_large_pc.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"E:\\uho_global_models_new\\asr\\export\\ASRVOCAB_stt_ua_fastconformer_hybrid_large_pc.txt")}}, false, 0.0f),
};*/

vector<LanguageInfo> langInfo = {
	LanguageInfo(L"Arabic (ar-AR)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe128_ar-AR_3.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe128_ar-AR_3.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe128_ar-AR_3.0.txt", L"ASRVOCAB_Conformer-CTC-L_spe128_ar-AR_3.0.txt")}}, true, 0.024f),
	LanguageInfo(L"Belarusian (be-BY)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_by_fastconformer_hybrid_large_pc.int8.onnx", L"ASRMODEL_stt_by_fastconformer_hybrid_large_pc.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_by_fastconformer_hybrid_large_pc.txt", L"ASRVOCAB_stt_by_fastconformer_hybrid_large_pc.txt")}}, false, 0.0f),
	LanguageInfo(L"Brazilian Portuguese (pt-BR)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_bpe1024_pt-BR_1.2.int8.onnx", L"ASRMODEL_Conformer-CTC-L_bpe1024_pt-BR_1.2.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_bpe1024_pt-BR_1.2.txt", L"ASRVOCAB_Conformer-CTC-L_bpe1024_pt-BR_1.2.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_pt-BR_Riva-PnC-SET-1.0_quantized_weights.onnx", L"PNCMODEL_bert-base_pt-BR_Riva-PnC-SET-1.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_pt.txt", L"PNCVOCAB_pt.txt")}}, true, 0.024f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Catalan (ca)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_ca_conformer_ctc_large.int8.onnx", L"ASRMODEL_stt_ca_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_ca_conformer_ctc_large.txt", L"ASRVOCAB_stt_ca_conformer_ctc_large.txt")}}, true, /*-0.056f*/0.070f),
	//LanguageInfo(L"Chinese (zh-CN)-English (en-US)", {{L"asr_model", UrlFileInfo(L"", L"ASRMODEL_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"", L"ASRVOCAB_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.txt")}, {L"pnc_model", UrlFileInfo(L"", L"PNCMODEL_punct_zh_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"", L"PNCVOCAB_zh.txt")}}, true, 0.0f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?", L"。", L"，", L"？"}, true),
	LanguageInfo(L"Chinese (zh-CN)-English (en-US)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.txt", L"ASRVOCAB_Conformer-CTC-L_spe7000_ml_cs_zh-en-CN_2.0.txt")}}, true, 0.0f, false, false),	// PnC not included as it doesn't predict punctuation well for Chinese
	LanguageInfo(L"Croatian (hr)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_hr_conformer_ctc_large.int8.onnx", L"ASRMODEL_stt_hr_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_hr_conformer_ctc_large.txt", L"ASRVOCAB_stt_hr_conformer_ctc_large.txt")}, /*{L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/nlp_tc_pc.int8.onnx", L"nlp_tc_pc.int8.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/sloberta_vocab.txt", L"sloberta_vocab.txt")}*/}, true, 0.0f, false, true/*, TokenizerType::RoBERTa*/),
	LanguageInfo(L"Dutch (nl)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe256_nl-NL_1.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe256_nl-NL_1.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe256_nl-NL_1.0.txt", L"ASRVOCAB_Conformer-CTC-L_spe256_nl-NL_1.0.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base-nl-NL_1.0_quantized_weights.onnx", L"PNCMODEL_bert-base-nl-NL_1.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_nl.txt", L"PNCVOCAB_nl.txt")}}, true, /*0.075f*/0.100f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),	// bias determined manually
	LanguageInfo(L"English (en-US)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe128_en-US_6.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe128_en-US_6.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe128_en-US_6.0.txt", L"ASRVOCAB_Conformer-CTC-L_spe128_en-US_6.0.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_en-US_4.0_quantized_weights.onnx", L"PNCMODEL_bert-base_en-US_4.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_en.txt", L"PNCVOCAB_en.txt")}}, true, /*-0.016f*/0.0f, false, true, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Esperanto (eo)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_eo_conformer_ctc_large.int8.onnx", L"ASRMODEL_stt_eo_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_eo_conformer_ctc_large.txt", L"ASRVOCAB_stt_eo_conformer_ctc_large.txt")}}, true, /*-0.016f*/0.035f),
	LanguageInfo(L"French (fr)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_fr_conformer_ctc_large.int8.onnx", L"ASRMODEL_stt_fr_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_fr_conformer_ctc_large.txt", L"ASRVOCAB_stt_fr_conformer_ctc_large.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_fr_fr_Riva-PnC-SET-2.0_quantized_weights.onnx", L"PNCMODEL_bert-base_fr_fr_Riva-PnC-SET-2.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_fr.txt", L"PNCVOCAB_fr.txt")}}, true, 0.1f, false, false, TokenizerType::BERT, {L"", L",", L"-", L".", L"?"}, true),
	LanguageInfo(L"German (de)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.txt", L"ASRVOCAB_Conformer-CTC-L_spe-1024_de-DE_Riva-ASR-SET-2.0.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_punct_de_quantized_weights.onnx", L"PNCMODEL_punct_de_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_de.txt", L"PNCVOCAB_de.txt")}}, true, 0.00f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Hindi (hi)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.txt", L"ASRVOCAB_Conformer-CTC-L_spe-128_hi-IN_Riva-ASR-SET-3.0.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_hi-IN_Riva-PnC-SET-1.0_quantized_weights.onnx", L"PNCMODEL_bert-base_hi-IN_Riva-PnC-SET-1.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_hi.txt", L"PNCVOCAB_hi.txt")}}, true, 0.0f, false, false, TokenizerType::BERT, {L"", L"!", L",", L"?", L"।"}, false),
	LanguageInfo(L"Italian (it)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe1024_it-IT_2.1.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe1024_it-IT_2.1.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe1024_it-IT_2.1.txt", L"ASRVOCAB_Conformer-CTC-L_spe1024_it-IT_2.1.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_it-IT_Riva-PnC-SET-1.1_quantized_weights.onnx", L"PNCMODEL_bert-base_it-IT_Riva-PnC-SET-1.1_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_it.txt", L"PNCVOCAB_it.txt")}}, true, 0.100f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Japanese (ja-JP)-English (en-JP)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.txt", L"ASRVOCAB_Conformer-CTC-L-unified_char_ml_cs_ja-en-JP_7.0.txt")}}, true, -0.016f, true),
	LanguageInfo(L"Kinyarwanda (rw)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_rw_conformer_ctc_large.int8.onnx", L"ASRMODEL_stt_rw_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_rw_conformer_ctc_large.txt", L"ASRVOCAB_stt_rw_conformer_ctc_large.txt")}}, true, 0.140f/*-0.140f*/),
	LanguageInfo(L"Korean (ko-KR)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.txt", L"ASRVOCAB_Conformer-CTC-L_spe-2048_ko-KR_Riva-ASR-SET-1.0.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_ko-KR_Riva-PnC-SET-1.1_quantized_weights.onnx", L"PNCMODEL_bert-base_ko-KR_Riva-PnC-SET-1.1_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_ko.txt", L"PNCVOCAB_ko.txt")}}, true, -0.024f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, false),
	LanguageInfo(L"Marathi (mr)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_mr_conformer_ctc_medium.int8.onnx", L"ASRMODEL_stt_mr_conformer_ctc_medium.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_mr_conformer_ctc_medium.txt", L"ASRVOCAB_stt_mr_conformer_ctc_medium.txt")}}, true, -0.135f),
	LanguageInfo(L"Persian (fa)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_fa_fastconformer_hybrid_large.int8.onnx", L"ASRMODEL_stt_fa_fastconformer_hybrid_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_fa_fastconformer_hybrid_large.txt", L"ASRVOCAB_stt_fa_fastconformer_hybrid_large.txt")}}, false, -0.056f),
	LanguageInfo(L"Polish (pl)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_pl_fastconformer_hybrid_large_pc.int8.onnx", L"ASRMODEL_stt_pl_fastconformer_hybrid_large_pc.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_pl_fastconformer_hybrid_large_pc.txt", L"ASRVOCAB_stt_pl_fastconformer_hybrid_large_pc.txt")}}, false, 0.237f),
	LanguageInfo(L"Russian (ru)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_ru_conformer_ctc_large.int8.onnx", L"ASRMODEL_stt_ru_conformer_ctc_large.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_ru_conformer_ctc_large.txt", L"ASRVOCAB_stt_ru_conformer_ctc_large.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_ru-RU_Riva-PnC-SET-1.0_quantized_weights.onnx", L"PNCMODEL_bert-base_ru-RU_Riva-PnC-SET-1.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_ru.txt", L"PNCVOCAB_ru.txt")}}, true, -0.016f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?"}, true),
	LanguageInfo(L"Slovenian (sl-SI)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_sl.int8.onnx", L"ASRMODEL_sl.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_sl.txt", L"ASRVOCAB_sl.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_punct_sl_quantized_weights.onnx", L"PNCMODEL_punct_sl_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_sl.txt", L"PNCVOCAB_sl.txt")}}, true, 0.0f, false, true, TokenizerType::RoBERTa, {L"", L"!", L",", L".", L"?"}, true), // 0.016f),
	LanguageInfo(L"Spanish (es-US)-English (en-US)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.int8.onnx", L"ASRMODEL_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.txt", L"ASRVOCAB_Conformer-CTC-L_spe1024_ml_cs_es-en-US_1.1.txt")}, {L"pnc_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCMODEL_bert-base_es-US_Riva-PnC-SET-2.0_quantized_weights.onnx", L"PNCMODEL_bert-base_es-US_Riva-PnC-SET-2.0_quantized_weights.onnx")}, {L"pnc_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/PNCVOCAB_es.txt", L"PNCVOCAB_es.txt")}}, true, -0.016f, false, false, TokenizerType::BERT, {L"", L",", L".", L"?", L"¿"}, true),
	LanguageInfo(L"Ukrainian (ua)", {{L"asr_model", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRMODEL_stt_ua_fastconformer_hybrid_large_pc.int8.onnx", L"ASRMODEL_stt_ua_fastconformer_hybrid_large_pc.int8.onnx")}, {L"asr_vocab", UrlFileInfo(L"https://github.com/obstino-org/UHO_Global/releases/download/v3.0.0-assets2/ASRVOCAB_stt_ua_fastconformer_hybrid_large_pc.txt", L"ASRVOCAB_stt_ua_fastconformer_hybrid_large_pc.txt")}}, false, 0.0f),
};

shared_ptr<NeMoRealFeed> g_NeMoRealFeed;
shared_ptr<NeMoPC> g_NeMoPunctCapit;

thread mainWndLooperThread;
bool g_showCaptions = true;
bool g_speakerChange = true;

mutex textLock;
wstring fullText = L"Transcript will be displayed here.";	// L"";
wstring truncFullText = fullText; //L"";
int fullTextCutIndex = 0;

auto lastUpdateTime = std::chrono::system_clock::now().time_since_epoch();
wstring incFullText = L"";

void fullTextUpdater(std::stop_token stop_token) {
	int newLen = 0;
	int prevLen = 0;
	float prevLettersPerSec = 5.0f;
	float endIndex = 0.0f;

	int minLettersPerSecond = 5;	// config
	int maxLettersPerSecond = 100;	// config
	float normalSecondsPerUpdate = g_NeMoRealFeed->getTimeASR(); //0.6f;	// config
	float minSecondsPerLetter = 1.0f / (float)maxLettersPerSecond;
	float maxSecondsPerLetter = 1.0f / (float)minLettersPerSecond;

	while (!stop_token.stop_requested()) {
		textLock.lock();
		newLen = fullText.length();
		textLock.unlock();

		if (newLen < prevLen) {
			prevLen = newLen;
			endIndex = std::clamp(endIndex, 0.0f, (float)newLen);
		}

		float lettersPerSec;
		if (newLen == prevLen) {
			lettersPerSec = prevLettersPerSec;
		}
		else {
			normalSecondsPerUpdate = g_NeMoRealFeed->getTimeASR();
			float secondsPerLetter = normalSecondsPerUpdate / (float)(newLen - prevLen);
			secondsPerLetter = std::clamp(secondsPerLetter, minSecondsPerLetter, maxSecondsPerLetter);	// in [sec]
			lettersPerSec = 1.0f / secondsPerLetter;
			//wprintf(L"lettersPerSec = %.3f\n", lettersPerSec);
			prevLettersPerSec = lettersPerSec;
		}

		auto t0 = chrono::high_resolution_clock::now();
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
		auto t1 = chrono::high_resolution_clock::now();

		float timeElapsedSeconds = 0.001f * (float)chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
		float letterIncrement = timeElapsedSeconds * lettersPerSec;

		endIndex += letterIncrement;
		
		if (prevLen == 0) { // todo check if needed
			prevLen = newLen;
			endIndex = newLen;
		}
		
		if (endIndex >= (float)prevLen) {
			prevLen = newLen;
		}

		endIndex = std::clamp(endIndex, 0.0f, (float)newLen);

		textLock.lock();
		int tmp = (int)endIndex;
		incFullText = fullText.substr(0, tmp);
		textLock.unlock();
	}
}

wstring replaceSubstring(wstring strIn, wstring subStr, wstring replacement) {
	int idx;
	while ((idx = strIn.find(subStr)) != wstring::npos)
		strIn = strIn.replace(idx, subStr.length(), replacement);
	return strIn;
}

void updateFullText(wstring newFullText) {
	//wprintf(L"[updateFullText] newFullText=\"%s\"\n", newFullText.c_str());

	textLock.lock();
	wstring prevFullText = fullText;
	textLock.unlock();

	if (g_NeMoPunctCapit != nullptr) {
		//wprintf(L"prevFullText = \"%s\", newFullText = \"%s\"\n", prevFullText.c_str(), newFullText.c_str());

		auto t0 = chrono::high_resolution_clock::now();
		newFullText = g_NeMoPunctCapit->punctuateAndCapitalize(prevFullText, newFullText);
		auto t1 = chrono::high_resolution_clock::now();
		
		//wprintf(L"PNC time = %i ms\n", (int)chrono::duration_cast<chrono::milliseconds>(t1 - t0).count());
		//wprintf(L"now newFullText = %s\n", newFullText.c_str());
	}

	textLock.lock();
	fullText = newFullText;
	lastUpdateTime = std::chrono::system_clock::now().time_since_epoch();
	textLock.unlock();
}

void newTextCallback(wstring newFullText)
{
	if (newFullText.length() == 0) {
		textLock.lock();
		fullText = truncFullText = L"";
		fullTextCutIndex = 0;
		incFullText = L"";
		textLock.unlock();
		return;
	}

	updateFullText(newFullText);
}

bool g_bStarted = false;

enum MainWndMsgs {
	SwitchSoundSource,
	StartStop,
	StopSelf,
};

mutex mainQueueLock;
queue<MainWndMsgs> mainQueue;

void stopNeMo(HWND hWnd) {
	g_NeMoRealFeed->NeMoStopFeed();
	
	// wait for it to stop (TODO do some kindof timeout)
	while (g_NeMoRealFeed->NeMoIsRunning())
		Sleep(100);

	g_NeMoPunctCapit = nullptr;

	SetWindowText(GetDlgItem(hWnd, IDC_BUTTON_STARTSTOP), L"START");
	g_bStarted = false;
}

void startNeMo(HWND hWnd) {
	if (g_NeMoRealFeed->NeMoIsRunning())
		return;

	bool micOrSpeakers;
	if (IsDlgButtonChecked(hWnd, IDC_RADIO_SOURCE_AUDIO) == BST_CHECKED)
		micOrSpeakers = false;
	else if (IsDlgButtonChecked(hWnd, IDC_RADIO_SOURCE_MIC) == BST_CHECKED)
		micOrSpeakers = true;
	else
		micOrSpeakers = false;

	wcout << (micOrSpeakers ? L"mic" : L"stream") << endl;

	int iSel = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_LANGUAGE));
	if (!(iSel >= 0 && iSel < langInfo.size()))
		return;

	wstring modelPath = langInfo[iSel].urlFileMap[L"asr_model"].path;
	wstring vocabPath = langInfo[iSel].urlFileMap[L"asr_vocab"].path;
	bool unspacedText = langInfo[iSel].unspacedText;	// true for Japanese char-based model
	float timeBias = langInfo[iSel].timeBias;
	bool suppressTokens = langInfo[iSel].suppressTokens;
	bool isConformer = langInfo[iSel].isConformer;
	
	if (langInfo[iSel].urlFileMap.contains(L"pnc_model") && langInfo[iSel].urlFileMap.contains(L"pnc_vocab")) {
		wstring pncModelPath = langInfo[iSel].urlFileMap[L"pnc_model"].path;
		wstring pncVocabPath = langInfo[iSel].urlFileMap[L"pnc_vocab"].path;
		TokenizerType pncTokType = langInfo[iSel].pncTokType;
		vector<wstring> punctSymbols = langInfo[iSel].pncSymbols;
		bool useCapitalLetters = langInfo[iSel].pncUseCapitalLetters;
		g_NeMoPunctCapit = make_shared<NeMoPC>(pncModelPath, pncVocabPath, pncTokType, punctSymbols, useCapitalLetters);
	}

	bool experimental = g_speakerChange;

	g_NeMoRealFeed->NeMoBeginFeed(modelPath, vocabPath, newTextCallback, micOrSpeakers, timeBias, unspacedText, false, isConformer, suppressTokens, experimental);
	
	g_bStarted = true;
	// wait for it to start (TODO do some kindof timeout)
	while (!g_NeMoRealFeed->NeMoIsRunning())
		Sleep(100);

	SetWindowText(GetDlgItem(hWnd, IDC_BUTTON_STARTSTOP), L"STOP");
}

bool isLangInstalled(LanguageInfo& lang) {
	bool asrOK;
	bool pncOK;

	if (lang.urlFileMap.contains(L"asr_model") &&
		lang.urlFileMap.contains(L"asr_vocab") &&
		std::filesystem::exists(lang.urlFileMap[L"asr_model"].path) &&
		std::filesystem::exists(lang.urlFileMap[L"asr_vocab"].path))
	{
		asrOK = true;
	}
	else {
		asrOK = false;
	}

	if (!lang.urlFileMap.contains(L"pnc_model") ||
		!lang.urlFileMap.contains(L"pnc_vocab"))
	{
		pncOK = true;
	}
	else if (std::filesystem::exists(lang.urlFileMap[L"pnc_model"].path) &&
			std::filesystem::exists(lang.urlFileMap[L"pnc_vocab"].path)) {
		pncOK = true;
	} else {
		pncOK = false;
	}

	return (asrOK && pncOK);
}

struct DownloadWindowState {
	bool exiting;
	HWND btnOK, btnCancel, wndProgress;
	LanguageInfo* langInfo;

	shared_ptr<FileDownloader> downloader;
	int numPrevPercent;
};

LRESULT CALLBACK hWndDownloadProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	DownloadWindowState* dlWndState;

	switch (msg) {
	case WM_CREATE: {
		dlWndState = new DownloadWindowState();
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)dlWndState);

		dlWndState->exiting = false;

		CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
		LPVOID lpParam = pCreate->lpCreateParams;
		dlWndState->langInfo = (LanguageInfo*)lpParam;

		wprintf(L"[WM_CREATE] langInfo->modelPath = %s\n", dlWndState->langInfo->urlFileMap[L"asr_model"].path.c_str());

		dlWndState->numPrevPercent = 0;

		HWND wndStaticInfo = CreateWindowEx(
			0,
			L"STATIC", L"",
			WS_CHILD | WS_VISIBLE,
			10, 10,
			DLWND_WIDTH - 40, 100,
			hWnd,
			NULL, NULL, NULL
		);

		wstring staticText =
			L"Language not installed.\n"
			L"Click 'Download' to obtain language files for \"" + dlWndState->langInfo->comboText + L"\".";

		SetWindowText(wndStaticInfo, staticText.c_str());

		dlWndState->btnOK = CreateWindowEx(0,
			L"BUTTON",
			L"Download",
			WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
			20, 150,
			120, 25,
			hWnd,
			(HMENU)DLWND_OK_ID,
			NULL, NULL);

		dlWndState->btnCancel = CreateWindowEx(0,
			L"BUTTON",
			L"Cancel",
			WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
			190, 150,
			120, 25,
			hWnd,
			(HMENU)DLWND_CANCEL_ID,
			NULL, NULL);

		dlWndState->wndProgress = CreateWindowEx(0,
			PROGRESS_CLASS,
			L"",
			WS_VISIBLE | WS_CHILD,
			20, 100,
			DLWND_WIDTH - 60, 30,
			hWnd,
			NULL, NULL, NULL);

		SendMessage(wndStaticInfo, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		SendMessage(dlWndState->btnOK, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		SendMessage(dlWndState->btnCancel, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
	} break;

	case WM_DESTROY:
		dlWndState = (DownloadWindowState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		wprintf(L"inside WM_DESTROY. deleting dlWndState\n");
		delete dlWndState;
		wprintf(L"deleted dlWndState\n");
		SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
		PostQuitMessage(0);
		break;

	case WM_COMMAND:
		switch (wParam) {
		case DLWND_OK_ID: {
			dlWndState = (DownloadWindowState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			EnableWindow(dlWndState->btnOK, FALSE);

			if (isLangInstalled(*(dlWndState->langInfo))) {
				MessageBox(0, L"Language already installed.", L"Information", MB_ICONINFORMATION);
				PostQuitMessage(0);
				break;
			}

			SetWindowText(dlWndState->btnOK, L"Downloading...");

			dlWndState = (DownloadWindowState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			vector<UrlFileInfo> urlFileInfoVect;
			if (dlWndState->langInfo->urlFileMap.contains(L"asr_model"))
				urlFileInfoVect.push_back(dlWndState->langInfo->urlFileMap[L"asr_model"]);
			if (dlWndState->langInfo->urlFileMap.contains(L"asr_vocab"))
				urlFileInfoVect.push_back(dlWndState->langInfo->urlFileMap[L"asr_vocab"]);
			if (dlWndState->langInfo->urlFileMap.contains(L"pnc_model"))
				urlFileInfoVect.push_back(dlWndState->langInfo->urlFileMap[L"pnc_model"]);
			if (dlWndState->langInfo->urlFileMap.contains(L"pnc_vocab"))
				urlFileInfoVect.push_back(dlWndState->langInfo->urlFileMap[L"pnc_vocab"]);

			dlWndState->downloader = make_shared<FileDownloader>(urlFileInfoVect);

			dlWndState->downloader->download(
				[hWnd, dlWndState](float p) {
					int numPercent = (int)p;
					if (numPercent != dlWndState->numPrevPercent) {
						dlWndState->numPrevPercent = numPercent;
						PostMessage(hWnd, DLWND_MSG_PERCENTAGE, (WPARAM)numPercent, 0);
					}
				},
				[hWnd](bool success) {
					PostMessage(hWnd, DLWND_MSG_FINISHED, (WPARAM)success, 0);
				}
				);
		} break;
		case DLWND_CANCEL_ID:
			dlWndState = (DownloadWindowState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			dlWndState->exiting = true;

			wprintf(L"Cancel\n");
			DestroyWindow(hWnd);
			break;
		}
		break;

	case DLWND_MSG_PERCENTAGE:
		dlWndState = (DownloadWindowState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (dlWndState->exiting)
			break;

		//wprintf(L"Received percentage message: %i%%\n", (int)wParam);
		SendMessage(dlWndState->wndProgress, PBM_SETPOS, (WPARAM)wParam, NULL);
		break;

	case DLWND_MSG_FINISHED: {
		dlWndState = (DownloadWindowState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (dlWndState->exiting) {
			wprintf(L"Not displaying messagebox\n");
			DestroyWindow(hWnd);
			break;
		}

		bool success = (bool)wParam;
		//wprintf(L"Received finished message: %i\n", success);

		if (success) {
			MessageBox(
				0,
				L"Download finished! You may now use the language.",
				L"Finished",
				MB_ICONINFORMATION);
		}
		else {
			MessageBox(
				0,
				L"Download error!",
				L"Error",
				MB_ICONERROR);
		}

		dlWndState->exiting = true;
		DestroyWindow(hWnd);
	} break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool processLanguageSelection(HWND hWnd)
{
	int iSel = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_LANGUAGE));
	if (!(iSel >= 0 && iSel < langInfo.size()))
		return false;

	LanguageInfo& lang = langInfo[iSel];
	wprintf(L"islanginstalled = %i\n", isLangInstalled(lang));

	if (isLangInstalled(lang))
		return true;

	HWND hWndDownload;
	LPCWSTR szDownloadClass = L"UHOGlobalDownload";

	int x = (SX - DLWND_WIDTH) / 2;
	int y = (SY - DLWND_HEIGHT) / 2;

	WNDCLASSEX wc = { 0 };
	wc.style = 0;
	wc.lpfnWndProc = hWndDownloadProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = szDownloadClass;
	wc.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1));
	wc.cbSize = sizeof(WNDCLASSEX);
	RegisterClassEx(&wc);

	hWndDownload = CreateWindowEx(
		WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE,
		szDownloadClass,
		L"Download language files",
		WS_VISIBLE | WS_POPUP | WS_SYSMENU | WS_DLGFRAME | WS_CAPTION,
		x, y,
		DLWND_WIDTH, DLWND_HEIGHT,
		NULL, NULL,
		GetModuleHandle(0),
		(LPVOID)&lang);

	ShowWindow(hWnd, SW_HIDE);

	MSG msg;
	while (GetMessage(&msg, 0, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ShowWindow(hWnd, SW_SHOWNORMAL);
	SetForegroundWindow(hWnd);

	return isLangInstalled(lang);
}

void mainWndLooper(HWND hWnd) {

	while (true) {
		// get element from queue if there's any
		mainQueueLock.lock();
		if (!mainQueue.size()) {
			mainQueueLock.unlock();
			continue;
		}
		MainWndMsgs msg = mainQueue.front();
		mainQueue.pop();
		mainQueueLock.unlock();

		switch (msg) {
		case StartStop:
			if (g_bStarted == true) {
				stopNeMo(hWnd);
				EnableWindow(GetDlgItem(hWnd, IDC_COMBO_LANGUAGE), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_DELETE), TRUE);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECKBOX_SPEAKERCHANGE), TRUE);
			}
			else {
				EnableWindow(GetDlgItem(hWnd, IDC_COMBO_LANGUAGE), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_DELETE), FALSE);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECKBOX_SPEAKERCHANGE), FALSE);

				// launch a procedure that determines language
				if(processLanguageSelection(hWnd) == true)
					startNeMo(hWnd);
				else {
					EnableWindow(GetDlgItem(hWnd, IDC_COMBO_LANGUAGE), TRUE);
					EnableWindow(GetDlgItem(hWnd, IDC_BUTTON_DELETE), TRUE);
				}
			}

			break;
		case SwitchSoundSource:
			if (g_bStarted == false) {
				wcout << L"g_bStarted = false (SwitchSoundSource)" << endl;
				break;
			}
			
			stopNeMo(hWnd);
			startNeMo(hWnd);
			break;
		case StopSelf:
			wcout << L"inside stopself\n";
			// cleanup NeMo
			if (g_bStarted)
				stopNeMo(hWnd);
			return;
		}
	}
}

LanguageInfo *getSelectedLangInfo(HWND hWnd) {
	int iSel = ComboBox_GetCurSel(GetDlgItem(hWnd, IDC_COMBO_LANGUAGE));
	if (!(iSel >= 0 && iSel < langInfo.size()))
		return NULL;

	return &langInfo[iSel];
}

void deleteLangFiles(LanguageInfo& lang) {
	if (lang.urlFileMap.contains(L"asr_model"))
		_wremove(lang.urlFileMap[L"asr_model"].path.c_str());

	if (lang.urlFileMap.contains(L"asr_vocab"))
		_wremove(lang.urlFileMap[L"asr_vocab"].path.c_str());

	if (lang.urlFileMap.contains(L"pnc_model"))
		_wremove(lang.urlFileMap[L"pnc_model"].path.c_str());

	if (lang.urlFileMap.contains(L"pnc_vocab"))
		_wremove(lang.urlFileMap[L"pnc_vocab"].path.c_str());
}

LRESULT CALLBACK mainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND wndDonate;
	static HANDLE iconDonate = NULL;
	static HWND wndShowCaptions;
	static HWND wndSpeakerChange;
	static HWND wndComboLanguage;
	static HWND wndDeleteLanguage;
	static HANDLE iconDelete = NULL;

	switch (uMsg) {
	case WM_CREATE: {
		// Create donation button
		wndDonate = CreateWindowEx(
			0,
			L"BUTTON",
			L"",
			WS_VISIBLE | WS_CHILD | BS_ICON | BS_CENTER | BS_VCENTER | BS_DEFPUSHBUTTON,
			10, TOP_OFFSET + 220,
			30, 30,
			hWnd,
			(HMENU)IDC_BUTTON_DONATE, NULL, NULL
		);
		iconDonate = LoadImage(GetModuleHandle(NULL), L"donate_2_new.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
		SendMessage(wndDonate, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)iconDonate);

		// Create sound source buttons
		HWND wndSoundSource = CreateWindowEx(0,
			L"BUTTON",
			L"Sound source",
			WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
			5, TOP_OFFSET, BASE_WIDTH + 30, 95,
			hWnd,
			(HMENU)IDC_GROUPBOX2,
			NULL,
			NULL);

		HWND wndSpeakers = CreateWindowEx(
			0,
			L"BUTTON",
			L"Speakers/Headphones",
			BS_AUTORADIOBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP,
			10, TOP_OFFSET + 25, BASE_WIDTH, 25,
			hWnd,
			(HMENU)IDC_RADIO_SOURCE_AUDIO,
			NULL,
			NULL);


		HWND wndMicrophone = CreateWindowEx(
			0,
			L"BUTTON",
			L"Microphone",
			BS_AUTORADIOBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
			10, TOP_OFFSET + 55, BASE_WIDTH, 25,
			hWnd,
			(HMENU)IDC_RADIO_SOURCE_MIC,
			NULL,
			NULL);

		SendMessage(GetDlgItem(hWnd, IDC_RADIO_SOURCE_AUDIO), BM_SETCHECK, BST_CHECKED, 0);

		// Create language selection controls
		HWND wndStaticLanguage = CreateWindowEx(
			0,
			L"STATIC", L"Select language:",
			WS_CHILD | WS_VISIBLE,
			10, TOP_OFFSET + 100,
			BASE_WIDTH, 25,
			hWnd,
			NULL, NULL, NULL
		);

		wndComboLanguage = CreateWindowEx(
			0,
			WC_COMBOBOX,
			L"",
			WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_OVERLAPPED,
			10, TOP_OFFSET + 130,
			BASE_WIDTH - 20, 700,
			hWnd,
			(HMENU)IDC_COMBO_LANGUAGE, NULL, NULL
		);

		for (auto& lang: langInfo)
			ComboBox_AddString(wndComboLanguage, lang.comboText.c_str());
		ComboBox_SetCurSel(wndComboLanguage, 0);

		// Create recycle bin button to delete language files
		wndDeleteLanguage = CreateWindowEx(
			0,
			L"BUTTON",
			L"",
			WS_VISIBLE | WS_CHILD | BS_ICON | BS_CENTER | BS_VCENTER,
			BASE_WIDTH, TOP_OFFSET + 125,
			30, 30,
			hWnd,
			(HMENU)IDC_BUTTON_DELETE, NULL, NULL
		);

		iconDelete = LoadImage(GetModuleHandle(NULL), L"recycle-bin-icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
		SendMessage(wndDeleteLanguage, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)iconDelete);

		// Create checkbox (to show captions or not)
		wndShowCaptions = CreateWindowEx(
			0,
			L"BUTTON",
			L"Show captions in bottom row",
			BS_CHECKBOX | WS_CHILD | WS_VISIBLE | BST_CHECKED | BS_MULTILINE,
			10, TOP_OFFSET + 160, BASE_WIDTH + 30, 25,
			hWnd,
			(HMENU)IDC_CHECKBOX_SHOWCAPTIONS,
			NULL,
			NULL);

		SendMessage(wndShowCaptions, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);

		// Create checkbox (to show captions or not)
		wndSpeakerChange = CreateWindowEx(
			0,
			L"BUTTON",
			L"Speaker change (experimental)",
			BS_CHECKBOX | WS_CHILD | WS_VISIBLE | BST_CHECKED | BS_MULTILINE,
			10, TOP_OFFSET + 185, BASE_WIDTH + 30, 25,
			hWnd,
			(HMENU)IDC_CHECKBOX_SPEAKERCHANGE,
			NULL,
			NULL);

		SendMessage(wndSpeakerChange, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
		
		// Create Start/Stop button

		HWND wndStart = CreateWindowEx(0,
			L"BUTTON",
			L"START",
			WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
			105, TOP_OFFSET + 225,
			100, 25,
			hWnd,
			(HMENU)IDC_BUTTON_STARTSTOP,
			NULL,
			NULL);

		SendMessage(wndSoundSource, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		SendMessage(wndSpeakers, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		SendMessage(wndMicrophone, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		SendMessage(wndStaticLanguage, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		SendMessage(wndComboLanguage, WM_SETFONT, (WPARAM)hFontGuiSm, (LPARAM)TRUE);
		SendMessage(wndStart, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		SendMessage(wndShowCaptions, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		SendMessage(wndSpeakerChange, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);

		SetTimer(hWnd, 0, 100, 0);	// timer to show/hide delete language button

		mainWndLooperThread = thread(mainWndLooper, hWnd);
	} break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_COMBO_LANGUAGE: {
			WORD code = HIWORD(wParam);
			if (code == CBN_SELENDOK) {
				thread t(processLanguageSelection, hWnd);
				t.detach();
				//wprintf(L"Combo selection: %i (%s)\n", iSel, langInfo[iSel].comboText.c_str());
			}
		} break;

		case IDC_BUTTON_DELETE: {
			LanguageInfo *lang = getSelectedLangInfo(hWnd);
			if (lang == NULL)	// just in case
				break;

			if (!isLangInstalled(*lang)) {
				ShowWindow(wndDeleteLanguage, SW_HIDE);
				break;
			}

			int mbResult = MessageBox(
				0,
				L"Are you sure you want to delete files for the selected language?",
				L"Warning",
				MB_ICONQUESTION | MB_YESNO
			);

			if (mbResult == IDYES) {
				deleteLangFiles(*lang);
				ShowWindow(wndDeleteLanguage, SW_HIDE);
			}
		} break;

		case IDC_BUTTON_STARTSTOP:
			mainQueueLock.lock();
			mainQueue.push(StartStop);
			mainQueueLock.unlock();
			break;

		case IDC_BUTTON_DONATE:
			ShellExecute(0, L"open", L"https://obstino.org/uho_transcribe/donate.php", L"", L"", SW_SHOWNORMAL);
			break;

		case IDC_RADIO_SOURCE_AUDIO:
		case IDC_RADIO_SOURCE_MIC:
			mainQueueLock.lock();
			mainQueue.push(SwitchSoundSource);
			mainQueueLock.unlock();
			break;

		case IDC_CHECKBOX_SHOWCAPTIONS:
			g_showCaptions = (IsDlgButtonChecked(hWnd, IDC_CHECKBOX_SHOWCAPTIONS) == BST_CHECKED);
			break;
		}

		case IDC_CHECKBOX_SPEAKERCHANGE:
		g_speakerChange = (IsDlgButtonChecked(hWnd, IDC_CHECKBOX_SPEAKERCHANGE) == BST_CHECKED);
		break;

		break;

	case WM_TIMER: {
		LanguageInfo* lang = getSelectedLangInfo(hWnd);
		if (lang == NULL)
			break;
		ShowWindow(wndDeleteLanguage, isLangInstalled(*lang) ? SW_SHOWNORMAL : SW_HIDE);
	} break;

	case WM_DESTROY:
		wcout << L"sending StopSelf\n";

		mainQueueLock.lock();
		mainQueue.push(StopSelf);
		mainQueueLock.unlock();
		
		DestroyIcon((HICON)iconDelete);
		iconDelete = NULL;

		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

enum ZBID
{
	ZBID_DEFAULT = 0,
	ZBID_DESKTOP = 1,
	ZBID_UIACCESS = 2,
	ZBID_IMMERSIVE_IHM = 3,
	ZBID_IMMERSIVE_NOTIFICATION = 4,
	ZBID_IMMERSIVE_APPCHROME = 5,
	ZBID_IMMERSIVE_MOGO = 6,
	ZBID_IMMERSIVE_EDGY = 7,
	ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
	ZBID_IMMERSIVE_INACTIVEDOCK = 9,
	ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
	ZBID_IMMERSIVE_ACTIVEDOCK = 11,
	ZBID_IMMERSIVE_BACKGROUND = 12,
	ZBID_IMMERSIVE_SEARCH = 13,
	ZBID_GENUINE_WINDOWS = 14,
	ZBID_IMMERSIVE_RESTRICTED = 15,
	ZBID_SYSTEM_TOOLS = 16,

	//Windows 10+
	ZBID_LOCK = 17,
	ZBID_ABOVELOCK_UX = 18
};

void DebugWString(const std::wstring& ws) {
	for (wchar_t c : ws) {
		std::wcout << L"Character: " << c << L" Code: 0x"
			<< std::hex << std::setw(4) << std::setfill(L'0')
			<< static_cast<int>(c) << std::endl;
	}
}

bool isChinese(uint32_t c) {
	uint32_t code = (uint32_t)c;
	return ((code >= 0x4E00 && code <= 0x62FF) ||
		(code >= 0x6300 && code <= 0x77FF) ||
		(code >= 0x7800 && code <= 0x8CFF) ||
		(code >= 0x8D00 && code <= 0x9FCC) ||
		(code >= 0x3400 && code <= 0x4DB5) ||
		(code >= 0x20000 && code <= 0x215FF) ||
		(code >= 0x21600 && code <= 0x230FF) ||
		(code >= 0x23100 && code <= 0x245FF) ||
		(code >= 0x24600 && code <= 0x260FF) ||
		(code >= 0x26100 && code <= 0x275FF) ||
		(code >= 0x27600 && code <= 0x290FF) ||
		(code >= 0x29100 && code <= 0x2A6DF) ||
		(code >= 0x2A700 && code <= 0x2B734) ||
		(code >= 0x2B740 && code <= 0x2B81D) ||
		(code >= 0x2B820 && code <= 0x2CEAF) ||
		(code >= 0x2CEB0 && code <= 0x2EBEF) ||
		(code >= 0x2F800 && code <= 0x2FA1F));
}

bool isJapanese(uint32_t c) {
	uint32_t code = (uint32_t)c;
	return ((code >= 0x3040 && code <= 0x309F) ||
		(code >= 0x30A0 && code <= 0x30FF) ||
		(code >= 0x4E00 && code <= 0x9FFF) ||
		(code >= 0x3000 && code <= 0x303F) ||
		(code >= 0xFF00 && code <= 0xFFEF) ||
		(code >= 0x31F0 && code <= 0x31FF));
}

bool isThai(uint32_t c) {
	uint32_t code = (uint32_t)c;
	return (code >= 0x0E00 && code <= 0x0E7F);
}

bool isKoreanWithHan(uint32_t c) {
	uint32_t code = (uint32_t)c;
	return ((code >= 0xAC00 && code <= 0xD7A3) ||
		(code >= 0x1100 && code <= 0x11FF) ||
		(code >= 0x3130 && code <= 0x318F) ||
		(code >= 0xA960 && code <= 0xA97F) ||
		(code >= 0xD7B0 && code <= 0xD7FF) ||
		(code >= 0x4E00 && code <= 0x9FFF) ||
		(code >= 0xF900 && code <= 0xFAFF));
}

bool isUnspacedCharLanguage(wint_t c) {
	return (isChinese(c) || isJapanese(c) || isThai(c) || isKoreanWithHan(c));
}

LRESULT CALLBACK wndOverlayProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HBRUSH rectBrush;
	static COLORREF captionsBackground = RGB(127, 127, 127);

	switch (uMsg) {
	case WM_DESTROY:
		DeleteObject(rectBrush);
		PostQuitMessage(0);
		break;

	case WM_CREATE:
		rectBrush = CreateSolidBrush(captionsBackground);
		SetTimer(hWnd, 0, 10, 0);	// set a 10ms timer (doesn't even have to be too accurate)
		break;

	case WM_TIMER: {
		textLock.lock();
		
		auto time2 = std::chrono::system_clock::now().time_since_epoch();
		bool dontShowCaptions = std::chrono::duration_cast<chrono::seconds>(time2 - lastUpdateTime).count() >= 10 || !g_showCaptions;
		if (dontShowCaptions) {
			fullTextCutIndex = (int)incFullText.length();
			truncFullText = L"";
		}

		textLock.unlock();

		HDC hdc, hdcMem;
		HBITMAP hbm;
		int idc;
		POINT p;
		RECT r;

		hdc = GetDC(hWnd);
		hdcMem = CreateCompatibleDC(hdc);
		hbm = CreateCompatibleBitmap(hdc, OVERLAY_WIDTH, OVERLAY_HEIGHT);

		idc = SaveDC(hdcMem);
		SelectObject(hdcMem, hbm);

		// <draw>
		GetClientRect(hWnd, &r);
		FillRect(hdcMem, &r, hbrBlack);

		GetCursorPos(&p);
		
		int wndX = (SX - OVERLAY_WIDTH) / 2;
		int wndY = (SY - 2 * OVERLAY_HEIGHT);
		SetWindowPos(
			hWnd,
			HWND_TOPMOST,
			wndX,
			wndY,
			OVERLAY_WIDTH,
			OVERLAY_HEIGHT,
			SWP_NOACTIVATE | SWP_SHOWWINDOW
		);

		idc = SaveDC(hdc);
		SetBkColor(hdcMem, RGB(127, 127, 127));
		SetTextColor(hdcMem, RGB(255, 255, 255));
		
		RECT rect;
		rect.top = 0;
		rect.left = 0;
		rect.right = OVERLAY_WIDTH;
		rect.bottom = OVERLAY_HEIGHT;

		if (!dontShowCaptions)
			FillRect(hdcMem, &rect, rectBrush);

		SelectObject(hdcMem, font);
		
		textLock.lock();
		wstring tmpFullText = truncFullText;

		while (true) {
			RECT probeRect;

			memset(&probeRect, 0, sizeof(RECT));
			probeRect.right = OVERLAY_WIDTH;
			auto t0 = std::chrono::high_resolution_clock::now();
			DrawText(hdcMem, tmpFullText.c_str(), -1, &probeRect, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
			auto t1 = std::chrono::high_resolution_clock::now();
			//wprintf(L"DrawText time = %lld [ms]\n", chrono::duration_cast<chrono::milliseconds>(t1 - t0).count());
			//wcout << "ProbeRect(bottom-top)=" << probeRect.bottom - probeRect.top << "; OVERLAY_HEIGHT=" << OVERLAY_HEIGHT << endl;
			if (probeRect.bottom - probeRect.top <= OVERLAY_HEIGHT)
				break;

			int lastNonAlphaCharPos = 0;

			for (int i = 0; i < tmpFullText.length(); i++) {
				memset(&probeRect, 0, sizeof(RECT));
				probeRect.right = OVERLAY_WIDTH;

				wstring subStr = wstring(tmpFullText.begin(), tmpFullText.begin() + i);
				DrawText(hdcMem, subStr.c_str(), -1, &probeRect, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
				if (probeRect.bottom - probeRect.top > LINE_HEIGHT)
					break;

				/*
				// @18.2.2026: code below defaulted to true (otherwise infinite loop with uninterrupted string of characters such as the case with Chinese text)
				if ((!iswalpha(tmpFullText[i]) && tmpFullText[i] != L'*') || 0 == 0) {
					lastNonAlphaCharPos = i;
					if (tmpFullText.length() > lastNonAlphaCharPos)
						lastNonAlphaCharPos++;
				}
				*/

				wint_t c = tmpFullText[i];
				if ((!iswalpha(c) && c != L'*') || isUnspacedCharLanguage(c))
					lastNonAlphaCharPos = i;
			}

			fullTextCutIndex += lastNonAlphaCharPos;
			tmpFullText = wstring(tmpFullText.begin() + lastNonAlphaCharPos, tmpFullText.end());
		}
		
		if (fullTextCutIndex <= incFullText.size()) {
			truncFullText = wstring(incFullText.begin() + fullTextCutIndex, incFullText.end());
		}

		tmpFullText = truncFullText;
		std::erase(tmpFullText, L'*');

		textLock.unlock();

		DrawText(hdcMem, tmpFullText.c_str(), -1, &r, DT_LEFT | DT_WORDBREAK);

		RestoreDC(hdc, idc);
		// </draw>

		BitBlt(hdc, 0, 0, SX, SY, hdcMem, 0, 0, SRCCOPY);

		RestoreDC(hdcMem, idc);
		DeleteObject(hbm);
		DeleteDC(hdcMem);
		ReleaseDC(hWnd, hdc);
	} break;

	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

void overlayThread()
{
	//HWND wndOverlay;
	WNDCLASSW wc = { 0 };
	MSG msg;
	WCHAR lpClass[] = L"UHO_Overlay";
	WCHAR lpWndName[] = L"UHO Overlay";
	
	wc.style = 0;
	wc.lpfnWndProc = wndOverlayProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = hbrBack;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = lpClass;

	RegisterClassW(&wc);

	HWND(WINAPI * CreateWindowInBand)(
		DWORD dwExStyle,
		LPCWSTR lpClassName,
		LPCWSTR lpWindowName,
		DWORD dwStyle,
		int x,
		int y,
		int nWidth,
		int nHeight,
		HWND hWndParent,
		HMENU hMenu,
		HINSTANCE hInstance,
		LPVOID lpParam,
		DWORD dwBand);

	*(FARPROC*)&CreateWindowInBand = GetProcAddress(
		GetModuleHandle(L"user32.dll"), "CreateWindowInBand");

	if (CreateWindowInBand && (hWndOverlay = CreateWindowInBand(
		/*WS_EX_CLIENTEDGE | */WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		lpClass,
		lpWndName,
		WS_VISIBLE | WS_POPUP,
		0, 0,
		OVERLAY_WIDTH, OVERLAY_HEIGHT,
		NULL,
		NULL,
		GetModuleHandle(NULL),
		NULL,
		ZBID_UIACCESS
	))) {
		//MessageBox(0, L"success", L"", 0);
	}
	else {
		hWndOverlay = CreateWindowExW(
			/*WS_EX_CLIENTEDGE | */WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
			lpClass,
			lpWndName,
			WS_VISIBLE | WS_POPUP,
			0, 0,
			OVERLAY_WIDTH, OVERLAY_HEIGHT,
			NULL,
			NULL,
			GetModuleHandle(NULL),
			NULL
		);
	}

	BYTE opacity = 230; // 0 opacity means completely transparent, 255 means not transparent at all
	SetLayeredWindowAttributes(hWndOverlay, RGB(0, 0, 0), opacity, LWA_COLORKEY | LWA_ALPHA);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	wprintf(L"[overlay] got to this point\n");

	hWndOverlay = NULL;
	return;
}

void resetEdit(HWND hWnd, HWND hEdit) {
	int padding = 30;
	
	RECT r;
	GetClientRect(hWnd, &r);

	int newX, newY, newWidth, newHeight;
	newX = newY = padding;
	newWidth = r.right - 2 * padding;
	newHeight = r.bottom - 2 * padding;

	MoveWindow(hEdit, newX, newY, newWidth, newHeight, TRUE);
}

void resetStatic(HWND hWnd, HWND hStatic) {
	int padding = 30;

	RECT r;
	GetClientRect(hWnd, &r);

	int newX, newY, newWidth, newHeight;
	newX = padding;
	newY = 0;
	newWidth = r.right - 2 * padding;
	newHeight = 30;

	MoveWindow(hStatic, newX, newY, newWidth, newHeight, TRUE);
}

void setEditFontSize(HWND hEdit, int size) {
	static HFONT editFont = NULL;
	HFONT newFont;

	newFont = CreateFont(
		size, 0,
		0, 0, FW_NORMAL,
		false, false, false,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_SWISS,
		L"Arial");
	SendMessage(hEdit, WM_SETFONT, (WPARAM)newFont, (LPARAM)TRUE);

	if (editFont != NULL)
		DeleteObject(editFont);
	editFont = newFont;
}

LRESULT CALLBACK hWndPrintoutProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int timerPeriodMs = 50;
	static HWND hEdit;
	static HWND hStatic;

	switch (uMsg) {
	case WM_DESTROY:
		wprintf(L"[hWndPrintoutProc] got WM_DESTROY\n");
		PostQuitMessage(0);
		break;

	case WM_CREATE: {
		// Disable [X] button
		HMENU hMenu = GetSystemMenu(hWnd, FALSE);
		if (hMenu)
			EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

		/*
		hEdit = CreateWindow(
			L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY,
			0, 0, 0, 0,
			hWnd,
			NULL, NULL, NULL
		);
		*/
		hEdit = CreateWindow(
			MSFTEDIT_CLASS, L"Here the transcribed text will be written.",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY | WS_BORDER | WS_TABSTOP,
			0, 0, 0, 0,
			hWnd,
			NULL, NULL, NULL
		);

		setEditFontSize(hEdit, editFontSize);
		resetEdit(hWnd, hEdit);

		hStatic = CreateWindow(
			L"STATIC", L"Text selected - transcription temporarily stopped",
			WS_CHILD,
			0, 0, 0, 0,
			hWnd,
			NULL, NULL, NULL
		);
		SendMessage(hStatic, WM_SETFONT, (WPARAM)hFontGui, (LPARAM)TRUE);
		resetStatic(hWnd, hStatic);

		SetTimer(hWnd, 0, timerPeriodMs, 0);
	} break;

	case WM_COMMAND:
		if (LOWORD(wParam) == ID_EDIT_COPY)
			SendMessage(hEdit, WM_COPY, 0, 0);
		break;

	case WM_CTLCOLORSTATIC: {
		HDC hdc = (HDC)wParam;
		SetTextColor(hdc, RGB(255, 0, 0));
		SetBkMode(hdc, TRANSPARENT);
		return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
	} break;

	case WM_SIZE:
		resetEdit(hWnd, hEdit);
		resetStatic(hWnd, hStatic);
		break;

	case WM_CONTEXTMENU: {
		HMENU hMenu = CreatePopupMenu();
		AppendMenu(hMenu, MF_STRING, ID_EDIT_COPY, TEXT("Kopiraj"));
		TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, hWnd, NULL);
	} break;

	case WM_TIMER: {
		static bool updateOnDeselect = false;
		static bool smoothScroll = false;
		static wstring prevFullTextMod = L"";
		static POINT maxScrollPos = { 0 };

		// Smooth scroll to the bottom if necessary
		if (smoothScroll == true) {
			double linesPerSec = 4.0;
			double pixPerSec = editFontSize * linesPerSec;
			int pixPerIt = (int)(pixPerSec * (double)timerPeriodMs/1000.0);

			POINT tmpScrollPos = { 0 };
			SendMessage(hEdit, EM_GETSCROLLPOS, 0, (LPARAM)&tmpScrollPos);
			tmpScrollPos.y += pixPerIt;
			if(tmpScrollPos.y < maxScrollPos.y)
				SendMessage(hEdit, EM_SETSCROLLPOS, 0, (LPARAM)&tmpScrollPos);
			else {
				SendMessage(hEdit, EM_SETSCROLLPOS, 0, (LPARAM)&maxScrollPos);
				smoothScroll = false;
			}
		}

		// Make a copy of full text
		wstring fullTextMod;
		textLock.lock();
		fullTextMod = incFullText;
		textLock.unlock();

		// Change * symbol to newline
		//fullTextMod = std::regex_replace(fullTextMod, std::wregex(L"\\*"), L"\r\n\r\n");
		fullTextMod = replaceSubstring(fullTextMod, L"*!", L"!\r\n\r\n");
		fullTextMod = replaceSubstring(fullTextMod, L"*,", L",\r\n\r\n");
		fullTextMod = replaceSubstring(fullTextMod, L"*.", L".\r\n\r\n");
		fullTextMod = replaceSubstring(fullTextMod, L"*?", L"?\r\n\r\n");
		fullTextMod = replaceSubstring(fullTextMod, L"*¿", L"¿\r\n\r\n");
		fullTextMod = replaceSubstring(fullTextMod, L"*।", L"।\r\n\r\n");
		fullTextMod = replaceSubstring(fullTextMod, L"*", L"\r\n\r\n");

		// Get selection info
		DWORD dwSelFrom, dwSelTo;
		SendMessage(hEdit, EM_GETSEL, (WPARAM)&dwSelFrom, (LPARAM)&dwSelTo);
		bool isTextSelected = dwSelFrom != dwSelTo;

		// don't update if previous text is same (unless just now text was deselected)
		if (fullTextMod == prevFullTextMod) {
			if(isTextSelected)
				ShowWindow(hStatic, SW_SHOW);
			else
				ShowWindow(hStatic, SW_HIDE);
			
			if (!updateOnDeselect)
				break;
			else
				updateOnDeselect = false;
		}

		prevFullTextMod = fullTextMod;

		// if currently text is being selected, don't update
		if (isTextSelected) {
			updateOnDeselect = true;
			ShowWindow(hStatic, SW_SHOW);
			break;
		}
		ShowWindow(hStatic, SW_HIDE);

		// Procede
		POINT scrollPos = { 0 };
		SendMessage(hEdit, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);

		// Disable redraw and get current text selection
		SendMessage(hEdit, WM_SETREDRAW, FALSE, 0);
		//SendMessage(hEdit, EM_GETSEL, (WPARAM)&dwSelFrom, (LPARAM)&dwSelTo);

		// Update EDIT
		SetWindowText(hEdit, fullTextMod.c_str());
		
		// Obtain maximum scroll
		SendMessage(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
		//POINT maxScrollPos = { 0 };
		SendMessage(hEdit, EM_GETSCROLLPOS, 0, (LPARAM)&maxScrollPos);
		//wprintf(L"Current position: %i of %i\n", scrollPos.y, maxScrollPos.y);
		int numLinesBehind = (maxScrollPos.y - scrollPos.y) / editFontSize;
		if (maxScrollPos.y > 0 && numLinesBehind <= 3)
			smoothScroll = true;

		// Restore selection
		SendMessage(hEdit, EM_SETSEL, (WPARAM)dwSelFrom, (LPARAM)dwSelTo);

		// Scroll to saved position
		SendMessage(hEdit, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);

		// Redraw EDIT and restore selection (TODO: check if from/to values are in valid range)
		SendMessage(hEdit, WM_SETREDRAW, TRUE, 0);

		InvalidateRect(hEdit, NULL, TRUE);
		UpdateWindow(hEdit);
	} break;

	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

void printoutThread()
{
	//HWND hWndPrintout;
	WNDCLASSEXW wc = { 0 };
	MSG msg;
	WCHAR lpClass[] = L"UHO_Global_Printout";
	WCHAR lpWndName[] = L"UHO Global";

	int sx, sy;
	int initialWidth;
	int initialHeight;
	int initialX, initialY;

	//DEVMODE devMode;
	//EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode);
	sx = GetSystemMetrics(SM_CXSCREEN);//devMode.dmPelsWidth;
	sy = GetSystemMetrics(SM_CYSCREEN);//devMode.dmPelsHeight;

	initialWidth = (sx / 2);
	initialHeight = (sy / 2);
	initialX = (sx - initialWidth) / 2;
	initialY = (sy - initialHeight) / 2;

	wc.style = 0;
	wc.lpfnWndProc = hWndPrintoutProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = lpClass;
	wc.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1));
	wc.cbSize = sizeof(WNDCLASSEX);

	RegisterClassExW(&wc);

	hWndPrintout = CreateWindowExW(
		WS_EX_OVERLAPPEDWINDOW,
		lpClass,
		lpWndName,
		WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		initialX, initialY,
		initialWidth, initialHeight,
		NULL,
		NULL,
		GetModuleHandle(NULL),
		NULL
	);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	wprintf(L"[printout] got to this point\n");

	hWndPrintout = NULL;

	return;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR pStr, int nCmd)
{
	_setmode(_fileno(stdout), _O_U16TEXT);	// needed for std::towlower etc.
	// Set console output to UTF-8
	SetConsoleOutputCP(CP_UTF8);
	// For wcout, also set the input code page
	SetConsoleCP(CP_UTF8);

	LoadLibrary(TEXT("Msftedit.dll"));
	
	// make sure we can use progress control (download indicator)
	INITCOMMONCONTROLSEX icc = { 0 };
	icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icc.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icc);

	g_NeMoRealFeed = make_shared<NeMoRealFeed>();

	thread thrOverlay = thread(overlayThread);
	thread thrPrintout = thread(printoutThread);
	jthread thrTextUpdater = jthread(fullTextUpdater);

	Sleep(1000);	// wait so that main window will be displayed on top (i.e. not behind printout window)

	WNDCLASSEX wc = { 0 };

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = mainWndProc;
	wc.lpszClassName = L"UHO_Global_Windows";
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(hInst, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);//(HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
	wc.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));

	RegisterClassEx(&wc);

	HWND hWnd;

	int defXPos, defYPos;
	defXPos = (SX - MAIN_WINDOW_WIDTH) / 2;
	defYPos = (SY - MAIN_WINDOW_HEIGHT) / 2;

	hWnd = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW,
		wc.lpszClassName,
		L"UHO Global v3",
		WS_SYSMENU | WS_VISIBLE | WS_MINIMIZEBOX,
		defXPos, defYPos,
		MAIN_WINDOW_WIDTH,
		MAIN_WINDOW_HEIGHT,
		NULL, NULL, hInst, NULL);

	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	wcout << L"Outside GetMessage in WinMain" << endl;
	if(mainWndLooperThread.joinable())
		mainWndLooperThread.join();

	if(hWndOverlay)
		SendMessage(hWndOverlay, WM_DESTROY, 0, 0);
	if(hWndPrintout)
		SendMessage(hWndPrintout, WM_DESTROY, 0, 0);

	thrTextUpdater.request_stop();
	if(thrOverlay.joinable())
		thrOverlay.join();
	if(thrPrintout.joinable())
		thrPrintout.join();

	return EXIT_SUCCESS;
}

void test();
void test2();

/*
int main() {
	//test(); return 0;
	//test2(); return 0;
	return _WinMain(GetModuleHandle(NULL), 0, GetCommandLineA(), SW_SHOW);
}*/

void test2() {
	_setmode(_fileno(stdout), _O_U16TEXT);	// needed for std::towlower etc.
	// Set console output to UTF-8
	SetConsoleOutputCP(CP_UTF8);
	// For wcout, also set the input code page
	SetConsoleCP(CP_UTF8);

	int langIndex = 7;

	NeMoPC pnc = NeMoPC(
		langInfo[langIndex].urlFileMap[L"pnc_model"].path,
		langInfo[langIndex].urlFileMap[L"pnc_vocab"].path,
		TokenizerType::BERT,
		langInfo[langIndex].pncSymbols,
		langInfo[langIndex].pncUseCapitalLetters
	);

	wstring text1 = L"Hello, this is a test";
	wstring text2 = L"hello this is a test of english speech recognition";
	wcout << pnc.punctuateAndCapitalize(text1, text2);
}

void test() {
	_setmode(_fileno(stdout), _O_U16TEXT);
	// Set console output to UTF-8
	SetConsoleOutputCP(CP_UTF8);
	// For wcout, also set the input code page
	SetConsoleCP(CP_UTF8);

	//NeMoInference nemo = NeMoInference();
	NeMoInference nemo = NeMoInference(
		L"stt_hr_quantized_weights.onnx",
		L"vocab_hr.txt"
	);

	NeMoVAD tmpVad = NeMoVAD(L"marblenet2.onnx", 5.0f);

	vector<float> audio = readWave("hr_audio.wav");
	//vector<float> audio = readWave("d:\\music\\asr\\multi_lang\\de3_30s.wav");
	audio.insert(audio.begin(), 8000, 0.0f);
	NeMoInference::NeMoOut out = nemo.forward(audio);
	for (int i = 0; i < out.uniqueTokTimestamps.size(); i++) {
		wprintf(L"%.3f (%s)\n", out.uniqueTokTimestamps[i], nemo.toks[out.uniqueOutTokens[i]].c_str());
	}
	return;

	vector<float> tmpAudio = vector<float>(audio.begin(), audio.begin() + std::min(5*16000, (int)audio.size()));
	auto t0 = chrono::high_resolution_clock::now();
	vector<bool> tmpVadOut = tmpVad.forward(tmpAudio, false);
	auto t1 = chrono::high_resolution_clock::now();
	wprintf(L"VAD took %i ms\n", chrono::duration_cast<chrono::milliseconds>(t1 - t0).count());

	float firstSilenceTime = 0.0f;
	for (int i = 0; i < tmpVadOut.size(); i++) {
		if (tmpVadOut[i] == false)
			firstSilenceTime += 0.020f;
		else
			break;
	}

	wprintf(L"FirstSilenceTime = %.2f (%.2f)", out.firstSilenceTime, firstSilenceTime);
	for (int i = 0; i < out.rawOutTokens.size(); i++) {
		int tok = out.rawOutTokens[i];
		wstring tokStr = nemo.toks[tok];
		wprintf(L"raw_tok_%i = %i\t(%s)\n", i, tok, tokStr.c_str());
	}

	//for (int i = 0; i < out.uniqueOutTokens.size(); i++) {
	//	int tok = out.uniqueOutTokens[i];
	//	wprintf(L"(%.3f) %s\n", out.uniqueTokTimestamps[i], nemo.toks[tok].c_str());
	//}

	NeMoInference::WordLevelOutput wlOut = nemo.convertTokensToWords(out);
	wcout << L"wlOut text:\n" << wlOut.toString();
	wcout << L"wlOut timestamps:\n" << wlOut.toStringTimestamps();

	return;
}
