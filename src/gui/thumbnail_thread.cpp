//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/thumbnail_thread.hpp>
#include <util/platform.hpp>
#include <util/error.hpp>
#include <wx/thread.h>

typedef pair<ThumbnailRequestP,Image> pair_ThumbnailRequestP_Image;
DECLARE_TYPEOF_COLLECTION(pair_ThumbnailRequestP_Image);

// ----------------------------------------------------------------------------- : Image Cache

String user_settings_dir();
String image_cache_dir() {
	String dir = user_settings_dir() + _("/cache");
	if (!wxDirExists(dir)) wxMkdir(dir);
	return dir + _("/");
}

/// A name that is safe to use as a filename, for the cache
String safe_filename(const String& str) {
	String ret; ret.reserve(str.size());
	FOR_EACH_CONST(c, str) {
		if (isAlnum(c)) {
			ret += c;
		} else if (c==_(' ') || c==_('-')) {
			ret += _('-');
		} else {
			ret += _('_');
		}
	}
	return ret;
}

// ----------------------------------------------------------------------------- : ThumbnailThreadWorker

class ThumbnailThreadWorker : public wxThread {
  public:
	ThumbnailThreadWorker(ThumbnailThread* parent);
	
	virtual ExitCode Entry();
	
	ThumbnailRequestP current; ///< Request we are working on
	ThumbnailThread*  parent;
	bool              stop; ///< Suspend computation
};

ThumbnailThreadWorker::ThumbnailThreadWorker(ThumbnailThread* parent)
	: parent(parent)
	, stop(false)
{}

wxThread::ExitCode ThumbnailThreadWorker::Entry() {
	while (true) {
		do {
			Sleep(1);
		} while (stop);
		// get a request
		{
			wxMutexLocker lock(parent->mutex);
			if (parent->open_requests.empty()) {
				parent->worker = nullptr;
				return 0; // No more requests
			}
			current = parent->open_requests.front();
			parent->open_requests.pop_front();
		}
		// perform request
		Image img;
		try {
			img = current->generate();
		} catch (const Error& e) {
			handle_error(e);
		} catch (...) {
		}
		// store in cache
		if (img.Ok()) {
			String filename = image_cache_dir() + safe_filename(current->cache_name) + _(".png");
			img.SaveFile(filename, wxBITMAP_TYPE_PNG);
			// set modification time
			wxFileName fn(filename);
			fn.SetTimes(0, &current->modified, 0);
		}
		// store result in closed request list
		{
			wxMutexLocker lock(parent->mutex);
			parent->closed_requests.push_back(make_pair(current,img));
			current = ThumbnailRequestP();
			parent->completed.Signal();
		}
	}
}

bool operator < (const ThumbnailRequestP& a, const ThumbnailRequestP& b) {
	if (a->owner < b->owner) return true;
	if (a->owner > b->owner) return false;
	return a->cache_name < b->cache_name;
}

// ----------------------------------------------------------------------------- : ThumbnailThread

ThumbnailThread thumbnail_thread;

ThumbnailThread::ThumbnailThread()
	: completed(mutex)
	, worker(nullptr)
{}

void ThumbnailThread::request(const ThumbnailRequestP& request) {
	assert(wxThread::IsMain());
	// Is the request in progress?
	if (request_names.find(request) != request_names.end()) {
		return;
	}
	// Is the image in the cache?
	String filename = image_cache_dir() + safe_filename(request->cache_name) + _(".png");
	wxFileName fn(filename);
	if (fn.FileExists()) {
		wxDateTime modified;
		if (fn.GetTimes(0, &modified, 0) && modified >= request->modified) {
			// yes it is
			Image img(filename);
			request->store(img);
			return;
		}
	}
	if (request->threadSafe()) {
		request_names.insert(request);
		// request generation
		{
			wxMutexLocker lock(mutex);
			open_requests.push_back(request);
		}
		// is there a worker?
		if (!worker) {
			worker = new ThumbnailThreadWorker(this);
			worker->Create();
			worker->Run();
		}
	}
	else {
		Image img;
		try {
			img = request->generate();
		} catch (const Error& e) {
			handle_error(e);
		} catch (...) {
		}
		// store in cache
		if (img.Ok()) {
			String filename = image_cache_dir() + safe_filename(request->cache_name) + _(".png");
			img.SaveFile(filename, wxBITMAP_TYPE_PNG);
			// set modification time
			wxFileName fn(filename);
			fn.SetTimes(0, &request->modified, 0);
		}
		{
			wxMutexLocker lock(mutex);
			closed_requests.push_back(make_pair(request,img));
			completed.Signal();
		}
	}
}

bool ThumbnailThread::done(void* owner) {
	assert(wxThread::IsMain());
	// find finished requests
	vector<pair<ThumbnailRequestP,Image> > finished;
	{
		wxMutexLocker lock(mutex);
		for (size_t i = 0 ; i < closed_requests.size() ; ) {
			if (closed_requests[i].first->owner == owner) {
				// move to finished list
				finished.push_back(closed_requests[i]);
				closed_requests.erase(closed_requests.begin() + i, closed_requests.begin() + i + 1);
			} else {
				++i;
			}
		}
	}
	// store them
	FOR_EACH(r, finished) {
		// store image
		r.first->store(r.second);
		// remove from name list
		request_names.erase(r.first);
	}
	return !finished.empty();
}

void ThumbnailThread::abort(void* owner) {
	assert(wxThread::IsMain());
	mutex.Lock();
	if (worker && worker->current && worker->current->owner == owner) {
		// a request for this owner is in progress, wait until it is done
		worker->stop = true;
		completed.Wait();
		mutex.Lock();
		worker->stop = false;
	}
	// remove open requests for this owner
	for (size_t i = 0 ; i < open_requests.size() ; ) {
		if (open_requests[i]->owner == owner) {
			// remove
			request_names.erase(open_requests[i]);
			open_requests.erase(open_requests.begin() + i, open_requests.begin() + i + 1);
		} else {
			++i;
		}
	}
	// remove closed requests for this owner
	for (size_t i = 0 ; i < closed_requests.size() ; ) {
		if (closed_requests[i].first->owner == owner) {
			// remove
			request_names.erase(closed_requests[i].first);
			closed_requests.erase(closed_requests.begin() + i, closed_requests.begin() + i + 1);
		} else {
			++i;
		}
	}
	mutex.Unlock();
}

void ThumbnailThread::abortAll() {
	assert(wxThread::IsMain());
	mutex.Lock();
	open_requests.clear();
	closed_requests.clear();
	request_names.clear();
	// end worker
	if (worker && worker->current) {
		completed.Wait();
	} else {
		mutex.Unlock();
	}
	// There may still be a worker, but if there is, it has no current object, so it is
	// in, before or after the stop loop. It can do nothing but end.
	// An unfortunate side effect is that we might leak some memory (of the worker object),
	// when the thread gets Kill()ed by wx.
}
