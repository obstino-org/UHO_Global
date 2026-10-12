// Copyright (c) obstino-org. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <iostream>
#include <mutex>
#include <functional>
#include <thread>
#include <curl/curl.h>
using namespace std;

// (libcurl 8.18.0)
// cmake .. -G "Visual Studio 16 2019" -A x64 -DCURL_USE_LIBPSL=OFF -DCURL_ENABLE_SSL=ON && cmake --build . --config Release

class UrlFileInfo {
public:
	wstring url;
	wstring path;
	UrlFileInfo(wstring url, wstring path) : url(url), path(path) { }
	UrlFileInfo() {}
};

class FileDownloader {
public:
	class DownloadInfo {
	public:
		FileDownloader& md;
		int dlnow;
		int dltotal;
		bool finished;

		int prevnow;
		unsigned long long lastTransferTime;
		DownloadInfo(FileDownloader& md) : prevnow(0), lastTransferTime(getTime()), md(md), dlnow(0), dltotal(0), finished(false) {}
		
		void reset() {
			prevnow = 0;
			lastTransferTime = getTime();
			dlnow = 0;
			dltotal = 0;
			finished = false;
		}

		static long long getTime() {
			return chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
		}
	};

	const int dlTimeoutMs = 10000;	// after this time of no new data, download is canceled
	static std::once_flag initFlag;

	wstring url;
	wstring path;
	
	atomic<bool> cancelTransfer = false;

	mutex callbackLock;
	std::function<void(float percentage)> onDataReceived;
	std::function<void(bool success)> onFinished;
	//void(*onDataReceived)(float percentage);
	//void(*onFinished)(bool success);

	vector<UrlFileInfo> urlFileInfoVect;
	vector<DownloadInfo> dlInfoVect;
	vector<thread> threads;
	//DownloadInfo dlInfo1, dlInfo2;
	//thread t1, t2;

	static void initOnce() {
		curl_global_init(CURL_GLOBAL_ALL);
		std::atexit(curl_global_cleanup);
	}

	FileDownloader(vector<UrlFileInfo> urlFileInfoVect): urlFileInfoVect(urlFileInfoVect) //:
		//urlModel(urlModel), pathModel(pathModel), urlVocab(urlVocab), pathVocab(pathVocab), dlInfo1(DownloadInfo(*this)), dlInfo2(DownloadInfo(*this))
	{
		std::call_once(initFlag, initOnce);
	}

	~FileDownloader() {
		wprintf(L"FileDownloader deconstructor\n");
		cancelDownload();

		for(auto&t: threads)
		if (t.joinable())
			t.join();
		
		wprintf(L"FileDownloader join()-ed all threads\n");
	}

	void cancelDownload() {
		cancelTransfer = true;
	}

	void download(
		std::function<void(float percentage)> onDataReceived,
		std::function<void(bool success)> onFinished
	) {
		this->onDataReceived = onDataReceived;
		this->onFinished = onFinished;

		cancelTransfer = false;
		
		callbackLock.lock();
		for(int i = 0; i < urlFileInfoVect.size(); i++)
			dlInfoVect.push_back(DownloadInfo(*this));
		for(auto& dlInfo: dlInfoVect)
			dlInfo.reset();
		callbackLock.unlock();

		for (int i = 0; i < urlFileInfoVect.size(); i++) {
			UrlFileInfo& urlFileInfo = urlFileInfoVect[i];
			DownloadInfo& dlInfo = dlInfoVect[i];
			threads.push_back(thread(&FileDownloader::curlDownload, this, std::ref(urlFileInfo), std::ref(dlInfo)));
		}

		//t2 = thread(&FileDownloader::curlDownload, this, urlVocab, pathVocab, &dlInfo2);
	}

	static int xferinfo_callback(void* clientp,
		curl_off_t dltotal,
		curl_off_t dlnow,
		curl_off_t ultotal,
		curl_off_t ulnow)
	{
		DownloadInfo* dlInfo = (DownloadInfo*)clientp;
		vector<DownloadInfo>& refDlInfoVect = dlInfo->md.dlInfoVect;
		
		dlInfo->md.callbackLock.lock();

		if (dlnow != dlInfo->prevnow)
			dlInfo->lastTransferTime = DownloadInfo::getTime();
		
		dlInfo->dltotal = dltotal;
		dlInfo->dlnow = dlnow;
		dlInfo->prevnow = dlnow;
		
		// if too much time has elapsed since last new data, cancel transfer
		if (DownloadInfo::getTime() - dlInfo->lastTransferTime > dlInfo->md.dlTimeoutMs)
			dlInfo->md.cancelTransfer = true;

		dlInfo->md.callbackLock.unlock();
		
		if (dlInfo->md.cancelTransfer == true)
			return 1;

		if (dlnow == 0)
			return 0;

		dlInfo->md.callbackLock.lock();
		float percentage;
		
		/*if (dlInfo->md.dlInfo1.dltotal == 0 || dlInfo->md.dlInfo2.dltotal == 0)
			percentage = 0.0f;
		else
			percentage = 100.0f * (float)(dlInfo->md.dlInfo1.dlnow + dlInfo->md.dlInfo2.dlnow) / (float)(dlInfo->md.dlInfo1.dltotal + dlInfo->md.dlInfo2.dltotal);*/
		int sumDlNow = 0;
		int sumDlTotal = 0;
		bool isAnyZeroTotal = false;
		for (auto& tmpDlInfo : refDlInfoVect) {
			//printf("dlnow: %i\n", tmpDlInfo.dlnow);
			sumDlNow += tmpDlInfo.dlnow;
			sumDlTotal += tmpDlInfo.dltotal;

			if (tmpDlInfo.dltotal == 0) {
				isAnyZeroTotal = true;
				//break;
			}
		}
		//printf("\n");

		if (isAnyZeroTotal)
			percentage = 0.0f;
		else
			percentage = 100.0f * (float)sumDlNow / (float)sumDlTotal;

		//printf("sumDlTotal = %i\n", sumDlTotal);

		if(!std::isnan(percentage) && !std::isinf(percentage))
			dlInfo->md.onDataReceived(percentage);

		dlInfo->md.callbackLock.unlock();
		
		return 0;
	}

	void onDownloadFinished() {
		bool areAllFinished = true;
		bool areAllSuccessful = true;
		for (auto& dlInfo : dlInfoVect) {
			if (!dlInfo.finished)
				areAllFinished = false;
			if (dlInfo.dlnow == 0)
				areAllSuccessful = false;
		}

		if (!areAllFinished) {
			for (auto& dlInfo : dlInfoVect) {
				if (dlInfo.finished && dlInfo.dlnow == 0)
					cancelTransfer = true;
			}
		}
		else {
			if (areAllSuccessful) {
				onFinished(true);
			}
			else {
				for(auto& dlInfo: dlInfoVect)
					_wremove(dlInfo.md.path.c_str());
				//_wremove(dlInfo1.md.pathModel.c_str());
				//_wremove(dlInfo2.md.pathVocab.c_str());
				onFinished(false);
			}
		}
	}
	/*void onDownloadFinished() {
		if (dlInfo1.finished && !dlInfo2.finished) {
			if (dlInfo1.dlnow == 0)
				cancelTransfer = true;
		}
		else if (dlInfo2.finished && !dlInfo1.finished) {
			if (dlInfo2.dlnow == 0)
				cancelTransfer = true;
		}
		else if (dlInfo1.finished && dlInfo2.finished) {
			if (dlInfo1.dlnow != 0 && dlInfo2.dlnow != 0)
				onFinished(true);
			else {
				_wremove(dlInfo1.md.pathModel.c_str());
				_wremove(dlInfo1.md.pathVocab.c_str());
				onFinished(false);
			}
		}
	}*/

	bool curlDownload(UrlFileInfo& urlFileInfo, DownloadInfo& dlInfo)
	{
		bool success = false;
		CURL* curl;
		FILE* f;
		CURLcode result;
		string surl(urlFileInfo.url.begin(), urlFileInfo.url.end());
		//printf("URL is %s\n", surl.c_str());

		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_URL, surl.c_str());
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, (void*)&dlInfo);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo_callback);

		wstring destPath = urlFileInfo.path;
		_wfopen_s(&f, destPath.c_str(), L"wb");
		if (f) {
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
			result = curl_easy_perform(curl);
			fclose(f);

			if (result == CURLE_OK) {
				success = true;
				//wprintf(L"result = OK\n");
			}
			else {
				//wprintf(L"result = Not OK: %i\n", result);
				_wremove(destPath.c_str());
			}
			curl_easy_cleanup(curl);
		}

		callbackLock.lock();

		if(!success)
			dlInfo.dlnow = 0;
		dlInfo.finished = true;
		onDownloadFinished();

		callbackLock.unlock();

		return success;
	}
};

std::once_flag FileDownloader::initFlag;
