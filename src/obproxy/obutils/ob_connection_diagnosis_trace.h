/**
 * Copyright (c) 2021 OceanBase
 * OceanBase Database Proxy(ODP) is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef OB_CONNECTION_DIAGNOSIS_TRACE
#define OB_CONNECTION_DIAGNOSIS_TRACE

#include "iocore/net/ob_inet.h"
#include "lib/string/ob_string.h"
#include "lib/oblog/ob_log_module.h"
#include "iocore/eventsystem/ob_buf_allocator.h"
#include "rpc/obmysql/ob_mysql_packet.h"
#include "lib/string/ob_string.h"
#include "lib/ptr/ob_ptr.h"

#define COLLECT_CONNECTION_DIAGNOSIS(diag, type, trace_type, args...)          \
  if (OB_NOT_NULL(diag) && diag->need_record_diagnosis_log(trace_type)) {      \
    diag->set_trace_type(trace_type);                                          \
    diag->record_##type##_disconnection(args);                                 \
  }                                                                            \


namespace oceanbase
{
namespace obproxy
{
namespace obutils
{

using namespace common;

enum ObVCDisconnectInitiator
{
  OB_UNKNOWN = 0,
  OB_CLIENT,
  OB_SERVER,
  OB_PROXY,
  OB_MAX_VC_INITIATOR,
};

enum ObConnectionDiagnosisTraceType
{
  OB_UNKNOWN_TRACE,
  OB_LOGIN_DISCONNECT_TRACE,
  OB_PROXY_INTERNAL_TRACE,
  OB_CLIENT_VC_TRACE,
  OB_SERVER_VC_TRACE,
  OB_VC_DISCONNECT_TRACE,
  OB_INTERNAL_REQUEST_VC_TRACE,
  OB_TIMEOUT_DISCONNECT_TRACE,
  OB_MAX_TRACE_TYPE,
};

enum ObInactivityTimeoutEvent
{
  OB_TIMEOUT_UNKNOWN_EVENT = 0,
  OB_CLIENT_DELETE_CLUSTER_RESOURCE,
  OB_CLIENT_INTERNAL_CMD_TIMEOUT,
  OB_CLIENT_CONNECT_TIMEOUT,
  OB_CLIENT_NET_READ_TIMEOUT,
  OB_CLIENT_EXECEUTE_PLAN_TIMEOUT,
  OB_CLIENT_WAIT_TIMEOUT,
  OB_CLIENT_NET_WRITE_TIMEOUT,
  OB_SERVER_QUERY_TIMEOUT,
  OB_SERVER_TRX_TIMEOUT,
  OB_SERVER_WAIT_TIMEOUT,
  OB_MAX_TIMEOUT_EVENT
};

#define MAX_MSG_BUF_LEN  128

class ObConnectionDiagnosisInfo;
class ObProxyInternalDiagnosisInfo;
class ObConnectionDiagnosisTrace : public ObSharedRefCount
{
public:
  ObConnectionDiagnosisTrace() : trace_type_(OB_UNKNOWN_TRACE), diagnosis_info_(NULL), is_user_client_(true), is_first_packet_received_(false), is_com_quit_(false) {}
  ~ObConnectionDiagnosisTrace() { reset(); }
  void reset();
  ObConnectionDiagnosisInfo *get_diagnosis_info() { return diagnosis_info_; }
  void set_diagnosis_info(ObConnectionDiagnosisInfo *info) { diagnosis_info_ = info; }
  template<typename T>
  int create_diagnosis_info(T *&diagnosis_info) {
    int ret = OB_SUCCESS;
    if (OB_ISNULL(diagnosis_info)) {
      if (OB_ISNULL(diagnosis_info = op_alloc(T))) {
        ret = OB_ALLOCATE_MEMORY_FAILED;
        PROXY_LOG(WARN, "fail to allocate mem for connection diagnosis info", K(ret));
      } else {
        diagnosis_info->reset();
      }
    }
    return ret;
  }
  int record_vc_disconnection(int vc_event, ObVCDisconnectInitiator initiator, int error_code, ObString error_msg = "");
  int record_inactivity_timeout_disconnection(ObInactivityTimeoutEvent event, ObHRTime timeout, int error_code, ObString error_msg = "");
  int record_obproxy_internal_disconnection(const ObString &sql, obmysql::ObMySQLCmd sql_cmd, obmysql::ObMySQLCmd request_cmd, int error_code, ObString error_msg = "");
  int record_login_disconnection (const ObString &internal_sql, int error_code, ObString error_msg = "");
  void log_dianosis_info() const;
  void set_trace_type(ObConnectionDiagnosisTraceType trace_type) { trace_type_ = trace_type; }
  void set_user_client(const bool is_user_client) { is_user_client_ = is_user_client; }
  void set_first_packet_received(const bool is_first_packet_received) { is_first_packet_received_ = is_first_packet_received; }
  bool need_record_diagnosis_log(ObConnectionDiagnosisTraceType trace_type) const;
  void set_com_quit(bool is_com_quit) { is_com_quit_ = is_com_quit; }
  virtual void free();
public:
  ObConnectionDiagnosisTraceType trace_type_;
  ObConnectionDiagnosisInfo *diagnosis_info_;
  bool is_user_client_;
  bool is_first_packet_received_;
  bool is_com_quit_;
};

class ObConnectionDiagnosisInfo
{
public: 
  ObConnectionDiagnosisInfo() { reset(); }
  virtual ~ObConnectionDiagnosisInfo() {}
  virtual void destroy() { reset(); op_free(this); }
  virtual void reset() {
    cs_id_ = 0;
    ss_id_ = 0;
    proxy_session_id_ = 0;
    server_session_id_ = 0;
    MEMSET(client_addr_, '\0', net::INET6_ADDRPORTSTRLEN);
    MEMSET(server_addr_, '\0', net::INET6_ADDRPORTSTRLEN);
    cluster_name_.reset();
    tenant_name_.reset();
    user_name_.reset();
    error_code_ = 0;
  }
  virtual int64_t to_string(char *buf, const int64_t buf_len) const;
public:
  int64_t cs_id_;
  int64_t ss_id_;
  int64_t proxy_session_id_;
  int64_t server_session_id_;
  char client_addr_[net::INET6_ADDRPORTSTRLEN];
  char server_addr_[net::INET6_ADDRPORTSTRLEN];
  common::ObString cluster_name_;
  common::ObString tenant_name_;
  common::ObString user_name_;
  int64_t error_code_;
  char error_msg_[MAX_MSG_BUF_LEN + 1];
};

class ObLoginDiagnosisInfo : public ObConnectionDiagnosisInfo
{
public:
  ObLoginDiagnosisInfo() { reset(); };
  ~ObLoginDiagnosisInfo() {};
  void destroy() { reset(); op_free(this); }
  int64_t to_string(char *buf, const int64_t buf_len) const;
  void reset();
public:
  char internal_sql_[OB_SHORT_SQL_LENGTH + 1];
};

class ObVCDiagnosisInfo : public ObConnectionDiagnosisInfo
{
public:
  ObVCDiagnosisInfo() { reset(); } 
  ~ObVCDiagnosisInfo() {};
  void destroy() { reset(); op_free(this); }
  int64_t to_string(char *buf, const int64_t buf_len) const;
  void reset();

public:
  int vc_event_;
  ObVCDisconnectInitiator disconnect_initiator_;
};

class ObInactivityTimeoutDiagnosisInfo : public ObConnectionDiagnosisInfo
{
public: 
  ObInactivityTimeoutDiagnosisInfo() : inactivity_timeout_event_(OB_TIMEOUT_UNKNOWN_EVENT), timeout_(0) {};
  ~ObInactivityTimeoutDiagnosisInfo() {};
  void destroy() { reset(); op_free(this); }
  int64_t to_string(char *buf, const int64_t buf_len) const;
  void reset();
public:
  ObInactivityTimeoutEvent inactivity_timeout_event_;
  ObHRTime timeout_;
};

class ObProxyInternalDiagnosisInfo : public ObConnectionDiagnosisInfo
{
public:
  ObProxyInternalDiagnosisInfo() { reset(); }
  ~ObProxyInternalDiagnosisInfo() {};
  void destroy() { reset(); op_free(this); }
  int64_t to_string(char *buf, const int64_t buf_len) const;
  void reset();
public:
  ObString sql_;
  obmysql::ObMySQLCmd sql_cmd_;
  obmysql::ObMySQLCmd request_cmd_;
};

}
}
}

#endif