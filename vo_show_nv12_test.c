
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h> 
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <linux/videodev2.h> 
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
#include <sys/select.h>  
#include <pthread.h>

#include "mpp_buffer.h"

static char optstr[] = "?i:d:w:h:";

int main(int argc, char **argv)
{
	int c;
	int fd;
	int w = 0, h = 0;
	int dev = 1;
	int ret = 0;
	char input_path[256] = {0};
	FILE *fp;
	
	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
			case 'i':
				strcpy(input_path, optarg); 
				printf("input path: %s\n", input_path);
				break;
			case 'w':
				w = atoi(optarg);
				break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'd':
				dev = atoi(optarg);  
				break;
			case '?':
			default:
				printf("usage example: ./vo_test -d 1 -i test.yuv -w 1920 -h 1080\n"); 
				exit(0);
		}
	}
// 1. init drm
	fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		printf("open /dev/dri/card0 device failed \n ");
		return -1;
	}
	drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	drmModeRes *res = drmModeGetResources(fd);
	drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[dev]);
	if (!conn) {
		printf("cannot retrieve DRM connector\n");
		return -1;
	}

	if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes <= 0) {
		printf("Drm connector is disconnected vo dev %d is invalid \n", dev);
		return -1;
	}

	drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
	uint32_t crtc = enc->crtc_id;
	printf("crtc = %d\n", crtc);

	drmModePlaneResPtr pres = drmModeGetPlaneResources(fd);
	if (!pres) {
		printf("drmModeGetPlaneResources failed\n");
		return -1;
	}

	//NV12 overlay plane
	int plane_id = pres->planes[2];
	printf("plane_id = %d\n", plane_id);
// 2. alloc drm buffer
	int size = w*h*3/2;
	
	MppBufferGroup group;
	ret = mpp_buffer_group_get_internal(&group, MPP_BUFFER_TYPE_DRM);
	if (ret) {
		printf("mpp_buffer_group_get_internal failed ret %d\n", ret);
		return -1;
	}

	MppBuffer nv12_buf;
	MPP_RET r = mpp_buffer_get(group, &nv12_buf, size);
	if (r != MPP_OK) {
		printf("mpp_buffer_get failed\n");
		return -1;
	}

// 3. load nv12 image file

	fp = fopen(input_path, "rb");
	if(NULL == fp) {
		printf("open input path %s failed \n", input_path);
		return -1;
	}

	int len = fread(mpp_buffer_get_ptr(nv12_buf), 1, w*h*3/2, fp);
	if (len != size) {
		printf("fread failed \n");
		return -1;
	}

	fclose(fp);
// 4. show nv12 image
	int frame_fd = mpp_buffer_get_fd(nv12_buf);

	uint32_t handle;
	if (drmPrimeFDToHandle(fd, frame_fd, &handle)) {
		printf("drmPrimeFDToHandle errinfo: %s\n", strerror(errno));
		return -1;
	}

	uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
	offsets[0] = 0;
	handles[0] = handle;
	pitches[0] = w;
	pitches[1] = pitches[0];
	offsets[1] = pitches[0] * h;
	handles[1] = handle;

	uint32_t fb;
	if (drmModeAddFB2(fd, w, h, DRM_FORMAT_NV12, handles, pitches, offsets, &fb, 0)) {
		printf("failed to add fb: %s\n", strerror(errno));
		return -1;
	}

	ret = drmModeSetPlane(fd, plane_id, crtc, fb, 0,
			-1650, 0, 1920, 1080,
			0, 0, w << 16, h << 16);
	if (ret) {
		printf("drmModeSetPlane failed ret = %d \n", ret);
	}
	
	while(1) {
		usleep(100000);
	}
	
	return 0;
}
