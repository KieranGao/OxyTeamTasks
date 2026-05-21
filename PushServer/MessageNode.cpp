#include "MessageNode.h"

RecvNode::RecvNode(short max_len, short msg_id)
    : MessageNode(max_len), msg_id_(msg_id) {}

SendNode::SendNode(const char* msg, short max_len, short msg_id)
    : MessageNode(max_len), msg_id_(msg_id) {
    memcpy(data_, msg, max_len);
    cur_len_ = max_len;
}
