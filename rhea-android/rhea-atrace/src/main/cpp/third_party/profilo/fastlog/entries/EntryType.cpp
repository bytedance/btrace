// @generated SignedSource<<78318a3b94021de54066d021eb55bf71>>

#include <stdexcept>

#include "entries/EntryType.h"

namespace facebook {
namespace profilo {
namespace entries {

const char* to_string(EntryType type) {
  switch(type) {
    case EntryType::UNKNOWN_TYPE: return "UNKNOWN_TYPE";
    case EntryType::UI_INPUT_START: return "UI_INPUT_START";
    case EntryType::UI_INPUT_END: return "UI_INPUT_END";
    case EntryType::UI_UPDATE_START: return "UI_UPDATE_START";
    case EntryType::UI_UPDATE_END: return "UI_UPDATE_END";
    case EntryType::NET_ADDED: return "NET_ADDED";
    case EntryType::NET_CANCEL: return "NET_CANCEL";
    case EntryType::NET_CHANGEPRI: return "NET_CHANGEPRI";
    case EntryType::NET_ERROR: return "NET_ERROR";
    case EntryType::NET_END: return "NET_END";
    case EntryType::NET_RESPONSE: return "NET_RESPONSE";
    case EntryType::NET_RETRY: return "NET_RETRY";
    case EntryType::NET_START: return "NET_START";
    case EntryType::NET_COUNTER: return "NET_COUNTER";
    case EntryType::CALL_START: return "CALL_START";
    case EntryType::CALL_END: return "CALL_END";
    case EntryType::ASYNC_CALL: return "ASYNC_CALL";
    case EntryType::SERV_CONN: return "SERV_CONN";
    case EntryType::SERV_DISCONN: return "SERV_DISCONN";
    case EntryType::SERV_END: return "SERV_END";
    case EntryType::ADAPTER_NOTIFY: return "ADAPTER_NOTIFY";
    case EntryType::MARK_FLAG: return "MARK_FLAG";
    case EntryType::MARK_PUSH: return "MARK_PUSH";
    case EntryType::MARK_POP: return "MARK_POP";
    case EntryType::LIFECYCLE_APPLICATION_START: return "LIFECYCLE_APPLICATION_START";
    case EntryType::LIFECYCLE_APPLICATION_END: return "LIFECYCLE_APPLICATION_END";
    case EntryType::LIFECYCLE_ACTIVITY_START: return "LIFECYCLE_ACTIVITY_START";
    case EntryType::LIFECYCLE_ACTIVITY_END: return "LIFECYCLE_ACTIVITY_END";
    case EntryType::LIFECYCLE_SERVICE_START: return "LIFECYCLE_SERVICE_START";
    case EntryType::LIFECYCLE_SERVICE_END: return "LIFECYCLE_SERVICE_END";
    case EntryType::LIFECYCLE_BROADCAST_RECEIVER_START: return "LIFECYCLE_BROADCAST_RECEIVER_START";
    case EntryType::LIFECYCLE_BROADCAST_RECEIVER_END: return "LIFECYCLE_BROADCAST_RECEIVER_END";
    case EntryType::LIFECYCLE_CONTENT_PROVIDER_START: return "LIFECYCLE_CONTENT_PROVIDER_START";
    case EntryType::LIFECYCLE_CONTENT_PROVIDER_END: return "LIFECYCLE_CONTENT_PROVIDER_END";
    case EntryType::LIFECYCLE_FRAGMENT_START: return "LIFECYCLE_FRAGMENT_START";
    case EntryType::LIFECYCLE_FRAGMENT_END: return "LIFECYCLE_FRAGMENT_END";
    case EntryType::LIFECYCLE_VIEW_START: return "LIFECYCLE_VIEW_START";
    case EntryType::LIFECYCLE_VIEW_END: return "LIFECYCLE_VIEW_END";
    case EntryType::TRACE_ABORT: return "TRACE_ABORT";
    case EntryType::TRACE_END: return "TRACE_END";
    case EntryType::TRACE_START: return "TRACE_START";
    case EntryType::TRACE_BACKWARDS: return "TRACE_BACKWARDS";
    case EntryType::TRACE_TIMEOUT: return "TRACE_TIMEOUT";
    case EntryType::BLACKBOX_TRACE_START: return "BLACKBOX_TRACE_START";
    case EntryType::COUNTER: return "COUNTER";
    case EntryType::STACK_FRAME: return "STACK_FRAME";
    case EntryType::QPL_START: return "QPL_START";
    case EntryType::QPL_END: return "QPL_END";
    case EntryType::QPL_CANCEL: return "QPL_CANCEL";
    case EntryType::QPL_NOTE: return "QPL_NOTE";
    case EntryType::QPL_POINT: return "QPL_POINT";
    case EntryType::QPL_EVENT: return "QPL_EVENT";
    case EntryType::TRACE_ANNOTATION: return "TRACE_ANNOTATION";
    case EntryType::WAIT_START: return "WAIT_START";
    case EntryType::WAIT_END: return "WAIT_END";
    case EntryType::WAIT_SIGNAL: return "WAIT_SIGNAL";
    case EntryType::STRING_KEY: return "STRING_KEY";
    case EntryType::STRING_VALUE: return "STRING_VALUE";
    case EntryType::QPL_TAG: return "QPL_TAG";
    case EntryType::QPL_ANNOTATION: return "QPL_ANNOTATION";
    case EntryType::TRACE_THREAD_NAME: return "TRACE_THREAD_NAME";
    case EntryType::TRACE_PRE_END: return "TRACE_PRE_END";
    case EntryType::TRACE_THREAD_PRI: return "TRACE_THREAD_PRI";
    case EntryType::MINOR_FAULT: return "MINOR_FAULT";
    case EntryType::MAJOR_FAULT: return "MAJOR_FAULT";
    case EntryType::PERFEVENTS_LOST: return "PERFEVENTS_LOST";
    case EntryType::CLASS_LOAD: return "CLASS_LOAD";
    case EntryType::JAVASCRIPT_STACK_FRAME: return "JAVASCRIPT_STACK_FRAME";
    case EntryType::MESSAGE_START: return "MESSAGE_START";
    case EntryType::MESSAGE_END: return "MESSAGE_END";
    case EntryType::CLASS_VALUE: return "CLASS_VALUE";
    case EntryType::HTTP2_REQUEST_INITIATED: return "HTTP2_REQUEST_INITIATED";
    case EntryType::HTTP2_FRAME_HEADER: return "HTTP2_FRAME_HEADER";
    case EntryType::HTTP2_WINDOW_UPDATE: return "HTTP2_WINDOW_UPDATE";
    case EntryType::HTTP2_PRIORITY: return "HTTP2_PRIORITY";
    case EntryType::HTTP2_EGRESS_FRAME_HEADER: return "HTTP2_EGRESS_FRAME_HEADER";
    case EntryType::PROCESS_LIST: return "PROCESS_LIST";
    case EntryType::IO_START: return "IO_START";
    case EntryType::IO_END: return "IO_END";
    case EntryType::CPU_COUNTER: return "CPU_COUNTER";
    case EntryType::CLASS_LOAD_START: return "CLASS_LOAD_START";
    case EntryType::CLASS_LOAD_END: return "CLASS_LOAD_END";
    case EntryType::CLASS_LOAD_FAILED: return "CLASS_LOAD_FAILED";
    case EntryType::STRING_NAME: return "STRING_NAME";
    case EntryType::JAVA_FRAME_NAME: return "JAVA_FRAME_NAME";
    case EntryType::BINDER_START: return "BINDER_START";
    case EntryType::BINDER_END: return "BINDER_END";
    case EntryType::MEMORY_ALLOCATION: return "MEMORY_ALLOCATION";
    case EntryType::STKERR_EMPTYSTACK: return "STKERR_EMPTYSTACK";
    case EntryType::STKERR_STACKOVERFLOW: return "STKERR_STACKOVERFLOW";
    case EntryType::STKERR_NOSTACKFORTHREAD: return "STKERR_NOSTACKFORTHREAD";
    case EntryType::STKERR_SIGNALINTERRUPT: return "STKERR_SIGNALINTERRUPT";
    case EntryType::STKERR_NESTEDUNWIND: return "STKERR_NESTEDUNWIND";
    case EntryType::MAPPING: return "MAPPING";
    case EntryType::LOGGER_PRIORITY: return "LOGGER_PRIORITY";
    case EntryType::CONDITIONAL_UPLOAD_RATE: return "CONDITIONAL_UPLOAD_RATE";
    case EntryType::NATIVE_ALLOC: return "NATIVE_ALLOC";
    case EntryType::NATIVE_FREE: return "NATIVE_FREE";
    case EntryType::NATIVE_ALLOC_FAILURE: return "NATIVE_ALLOC_FAILURE";
    default: throw std::invalid_argument("Unknown entry type");
  }
}

} // namespace entries
} // namespace profilo
} // namespace facebook
