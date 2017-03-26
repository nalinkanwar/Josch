#ifndef TLV_H
#define TLV_H

enum tlv_types {
    TLV_NONE = 0,
    TLV_REGISTER_JOB,
    TLV_UNREGISTER_JOB,
    TLV_LIST_JOBS,
    TLV_SUCCESS,
    TLV_FAILURE
};

struct tlv {
    int32_t type;
    int32_t length;
    int32_t last;
    char value[0];
};


#endif // TLV_H
