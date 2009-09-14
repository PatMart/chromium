// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/net/server_connection_manager.h"

#include <errno.h>

#include <ostream>
#include <string>
#include <vector>

#include "chrome/browser/sync/engine/net/http_return.h"
#include "chrome/browser/sync/engine/net/url_translator.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/event_sys-inl.h"

namespace browser_sync {

using std::ostream;
using std::string;
using std::vector;

static const char kSyncServerSyncPath[] = "/command/";

// At the /time/ path of the sync server, we expect to find a very simple
// time of day service that we can use to synchronize the local clock with
// server time.
static const char kSyncServerGetTimePath[] = "/time";

static const ServerConnectionEvent shutdown_event =
  { ServerConnectionEvent::SHUTDOWN, HttpResponse::CONNECTION_UNAVAILABLE,
    false };

typedef PThreadScopedLock<PThreadMutex> MutexLock;

struct ServerConnectionManager::PlatformMembers {
  explicit PlatformMembers(const string& user_agent) { }
  void Kill() { }
  void Reset() { }
  void Reset(MutexLock*) { }
};

bool ServerConnectionManager::Post::ReadBufferResponse(
    string* buffer_out, HttpResponse* response, bool require_response) {
  if (RC_REQUEST_OK != response->response_code) {
    response->server_status = HttpResponse::SYNC_SERVER_ERROR;
    return false;
  }

  if (require_response && (1 > response->content_length))
    return false;

  const int64 bytes_read = ReadResponse(buffer_out,
      static_cast<int>(response->content_length));
  if (bytes_read != response->content_length) {
    response->server_status = HttpResponse::IO_ERROR;
    return false;
  }
  return true;
}

bool ServerConnectionManager::Post::ReadDownloadResponse(
    HttpResponse* response, string* buffer_out) {
  const int64 bytes_read = ReadResponse(buffer_out,
      static_cast<int>(response->content_length));

  if (bytes_read != response->content_length) {
    LOG(ERROR) << "Mismatched content lengths, server claimed " <<
        response->content_length << ", but sent " << bytes_read;
    response->server_status = HttpResponse::IO_ERROR;
    return false;
  }
  return true;
}

namespace {
  string StripTrailingSlash(const string& s) {
    int stripped_end_pos = s.size();
    if (s.at(stripped_end_pos - 1) == '/') {
      stripped_end_pos = stripped_end_pos - 1;
    }

    return s.substr(0, stripped_end_pos);
  }
}  // namespace

// TODO(chron): Use a GURL instead of string concatenation.
  string ServerConnectionManager::Post::MakeConnectionURL(
    const string& sync_server, const string& path,
    bool use_ssl) const {
  string connection_url = (use_ssl ? "https://" : "http://");
  connection_url += sync_server;
  connection_url = StripTrailingSlash(connection_url);
  connection_url += path;

  return connection_url;
}

int ServerConnectionManager::Post::ReadResponse(string* out_buffer,
                                                int length) {
  int bytes_read = buffer_.length();
  CHECK(length <= bytes_read);
  out_buffer->assign(buffer_);
  return bytes_read;
}

// A helper class that automatically notifies when the status changes.
struct WatchServerStatus {
  WatchServerStatus(ServerConnectionManager* conn_mgr, HttpResponse* response)
    : conn_mgr_(conn_mgr), response_(response),
      reset_count_(conn_mgr->reset_count_),
      server_reachable_(conn_mgr->server_reachable_) {
    response->server_status = conn_mgr->server_status_;
  }
  ~WatchServerStatus() {
    // Don't update the status of the connection if it has been reset.
    // TODO(timsteele): Do we need this? Is this used by multiple threads?
    if (reset_count_ != conn_mgr_->reset_count_)
      return;
    if (conn_mgr_->server_status_ != response_->server_status) {
      conn_mgr_->server_status_ = response_->server_status;
      conn_mgr_->NotifyStatusChanged();
      return;
    }
    // Notify if we've gone on or offline.
    if (server_reachable_ != conn_mgr_->server_reachable_)
      conn_mgr_->NotifyStatusChanged();
  }
  ServerConnectionManager* const conn_mgr_;
  HttpResponse* const response_;
  // TODO(timsteele): Should this be Barrier:AtomicIncrement?
  base::subtle::AtomicWord reset_count_;
  bool server_reachable_;
};

ServerConnectionManager::ServerConnectionManager(
    const string& server, int port, bool use_ssl, const string& user_agent,
    const string& client_id)
    : sync_server_(server), sync_server_port_(port),
      channel_(new Channel(shutdown_event)),
      server_status_(HttpResponse::NONE), server_reachable_(false),
      client_id_(client_id), use_ssl_(use_ssl),
      user_agent_(user_agent),
      platform_(new PlatformMembers(user_agent)),
      reset_count_(0), error_count_(0),
      terminate_all_io_(false),
      proto_sync_path_(kSyncServerSyncPath),
      get_time_path_(kSyncServerGetTimePath) {
}

ServerConnectionManager::~ServerConnectionManager() {
  delete channel_;
  delete platform_;
  shutdown_event_mutex_.Lock();
  int result = pthread_cond_broadcast(&shutdown_event_condition_.condvar_);
  shutdown_event_mutex_.Unlock();
  if (result) {
    LOG(ERROR) << "Error signaling shutdown_event_condition_ last error = "
               << result;
  }
}

void ServerConnectionManager::NotifyStatusChanged() {
  ServerConnectionEvent event = { ServerConnectionEvent::STATUS_CHANGED,
                                  server_status_,
                                  server_reachable_ };
  channel_->NotifyListeners(event);
}

// Uses currently set auth token. Set by AuthWatcher.
bool ServerConnectionManager::PostBufferWithCachedAuth(
    const PostBufferParams* params) {
  string path =
      MakeSyncServerPath(proto_sync_path(), MakeSyncQueryString(client_id_));
  return PostBufferToPath(params, path, auth_token_);
}

bool ServerConnectionManager::PostBufferWithAuth(const PostBufferParams* params,
                                                 const string& auth_token) {
  string path = MakeSyncServerPath(proto_sync_path(),
                                   MakeSyncQueryString(client_id_));

  return PostBufferToPath(params, path, auth_token);
}

bool ServerConnectionManager::PostBufferToPath(const PostBufferParams* params,
                                               const string& path,
                                               const string& auth_token) {
  WatchServerStatus watcher(this, params->response);
  scoped_ptr<Post> post(MakePost());
  post->set_timing_info(params->timing_info);
  bool ok = post->Init(path.c_str(), auth_token, params->buffer_in,
                       params->response);

  if (!ok || RC_REQUEST_OK != params->response->response_code) {
    IncrementErrorCount();
    return false;
  }

  if (post->ReadBufferResponse(params->buffer_out, params->response, true)) {
    params->response->server_status = HttpResponse::SERVER_CONNECTION_OK;
    server_reachable_ = true;
    return true;
  }
  return false;
}

bool ServerConnectionManager::CheckTime(int32* out_time) {
  // Verify that the server really is reachable by checking the time. We need
  // to do this because of wifi interstitials that intercept messages from the
  // client and return HTTP OK instead of a redirect.
  HttpResponse response;
  WatchServerStatus watcher(this, &response);
  string post_body = "command=get_time";

  // We only retry the CheckTime call if we were reset during the CheckTime
  // attempt. We only try 3 times in case we're in a reset loop elsewhere.
  base::subtle::AtomicWord start_reset_count = reset_count_ - 1;
  for (int i = 0 ; i < 3 && start_reset_count != reset_count_ ; i++) {
    start_reset_count = reset_count_;
    scoped_ptr<Post> post(MakePost());

    // Note that the server's get_time path doesn't require authentication.
    string get_time_path =
        MakeSyncServerPath(kSyncServerGetTimePath, post_body);
    LOG(INFO) << "Requesting get_time from:" << get_time_path;

    string blank_post_body;
    bool ok = post->Init(get_time_path.c_str(), blank_post_body,
        blank_post_body, &response);
    if (!ok) {
      LOG(INFO) << "Unable to check the time";
      continue;
    }
    string time_response;
    time_response.resize(
        static_cast<string::size_type>(response.content_length));
    ok = post->ReadDownloadResponse(&response, &time_response);
    if (!ok || string::npos !=
        time_response.find_first_not_of("0123456789")) {
      LOG(ERROR) << "unable to read a non-numeric response from get_time:"
            << time_response;
      continue;
    }
    *out_time = atoi(time_response.c_str());
    LOG(INFO) << "Server was reachable.";
    return true;
  }
  IncrementErrorCount();
  return false;
}

bool ServerConnectionManager::IsServerReachable() {
  int32 time;
  return CheckTime(&time);
}

bool ServerConnectionManager::IsUserAuthenticated() {
  return IsGoodReplyFromServer(server_status_);
}

bool ServerConnectionManager::CheckServerReachable() {
  const bool server_is_reachable = IsServerReachable();
  if (server_reachable_ != server_is_reachable) {
    server_reachable_ = server_is_reachable;
    NotifyStatusChanged();
  }
  return server_is_reachable;
}

void ServerConnectionManager::kill() {
  {
    MutexLock lock(&terminate_all_io_mutex_);
    terminate_all_io_ = true;
  }
  platform_->Kill();
  shutdown_event_mutex_.Lock();
  int result = pthread_cond_broadcast(&shutdown_event_condition_.condvar_);
  shutdown_event_mutex_.Unlock();
  if (result) {
    LOG(ERROR) << "Error signaling shutdown_event_condition_ last error = "
               << result;
  }
}

void ServerConnectionManager::ResetAuthStatus() {
  ResetConnection();
  server_status_ = HttpResponse::NONE;
  NotifyStatusChanged();
}

void ServerConnectionManager::ResetConnection() {
  base::subtle::NoBarrier_AtomicIncrement(&reset_count_, 1);
  platform_->Reset();
}

bool ServerConnectionManager::IncrementErrorCount() {
#ifdef OS_WINDOWS
  error_count_mutex_.Lock();
  error_count_++;

  if (error_count_ > kMaxConnectionErrorsBeforeReset) {
    error_count_ = 0;

    // Be careful with this mutex because calling out to other methods can
    // result in being called back. Unlock it here to prevent any potential
    // double-acquisitions.
    error_count_mutex_.Unlock();

    if (!IsServerReachable()) {
      LOG(WARNING) << "Too many connection failures, server is not reachable. "
                   << "Resetting connections.";
      ResetConnection();
    } else {
      LOG(WARNING) << "Multiple connection failures while server is reachable.";
    }
    return false;
  }

  error_count_mutex_.Unlock();
  return true;
#endif
  return true;
}

void ServerConnectionManager::SetServerParameters(const string& server_url,
                                                  int port, bool use_ssl) {
  {
    ParametersLock lock(&server_parameters_mutex_);
    sync_server_ = server_url;
    sync_server_port_ = port;
    use_ssl_ = use_ssl;
  }
  platform_->Reset();
}

// Returns the current server parameters in server_url and port.
void ServerConnectionManager::GetServerParameters(string* server_url,
                                                  int* port, bool* use_ssl) {
  ParametersLock lock(&server_parameters_mutex_);
  if (server_url != NULL)
    *server_url = sync_server_;
  if (port != NULL)
    *port = sync_server_port_;
  if (use_ssl != NULL)
    *use_ssl = use_ssl_;
}

bool FillMessageWithShareDetails(sync_pb::ClientToServerMessage* csm,
                                 syncable::DirectoryManager* manager,
                                 const PathString &share) {
  syncable::ScopedDirLookup dir(manager, share);
  if (!dir.good()) {
    LOG(INFO) << "Dir lookup failed";
    return false;
  }
  string birthday = dir->store_birthday();
  if (!birthday.empty())
    csm->set_store_birthday(birthday);
  csm->set_share(ToUTF8(share).get_string());
  return true;
}

}  // namespace browser_sync

std::ostream& operator << (std::ostream& s,
                           const struct browser_sync::HttpResponse& hr) {
  s << " Response Code (bogus on error): " << hr.response_code;
  s << " Content-Length (bogus on error): " << hr.content_length;
  s << " Server Status: " << hr.server_status;
  return s;
}
