#include "image.h"
#include <logging/log.h>
#include <dfu/flash_img.h>
#include <dfu/mcuboot.h>
#include <errno.h>
#include <flash.h>
#include <flash_map.h>

LOG_MODULE_REGISTER(image, 4);

int cmp_version(struct image_version *v1, struct image_version *v2)
{
    int major     = v1->iv_major - v2->iv_major;
    int minor     = v1->iv_minor - v2->iv_minor;
    int revision  = v1->iv_revision - v2->iv_revision;
    int build_num = v1->iv_build_num - v2->iv_build_num;

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

int read_image_info(int area_id, uint32_t *size, struct image_version *version)
{
    const struct flash_area *fap;
    struct image_tlv_info info;
    struct image_header hdr;
    int rc = 0;

    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        rc = -1;
        goto done;
    }

    rc = flash_area_read(fap, 0, &hdr, sizeof(hdr));
    if (rc != 0) {
        rc = -1;
        goto done;
    }
    memcpy(version, &hdr.ih_ver, sizeof hdr.ih_ver);

    rc = flash_area_read(fap, hdr.ih_hdr_size + hdr.ih_img_size, &info, sizeof(info));
    if (rc != 0) {
        rc = -1;
        goto done;
    }

    if (info.it_magic != IMAGE_TLV_INFO_MAGIC) {
        rc = -1;
        goto done;
    }
    *size = hdr.ih_hdr_size + hdr.ih_img_size + info.it_tlv_tot;

done:
    flash_area_close(fap);
    return rc;
}

void print_version(struct image_version *ver)
{
    LOG_INF("Version: %d.%d.%d+%d", ver->iv_major, ver->iv_minor, ver->iv_revision,
            ver->iv_build_num);
}