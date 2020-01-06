#include "image.h"

int cmp_version(struct image_version v1, struct image_version v2)
{
    int major     = v1.iv_major - v2.iv_major;
    int minor     = v1.iv_minor - v2.iv_minor;
    int revision  = v1.iv_revision - v2.iv_revision;
    int build_num = v1.iv_build_num - v2.iv_build_num;

    if (major) {
        return major;
    }
    if (minor) {
        return minor;
    }
    if (revision) {
        return revision;
    }
    return build_num;
}
