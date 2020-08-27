/***************************************
@Copyright(C) All rights reserved.
@Author DENG Ganglin
@Date   2020.08
@File   main.cpp
***************************************/

#include "dji_vehicle.hpp"
#include "dji_linux_helpers.hpp"

#include <glib.h>
#include <stdlib.h>
#include <pthread.h>
#include <gst/gst.h>
#include <gst/gstminiobject.h>

#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <queue>
#include <pthread.h>
#include <unistd.h>

#include "loopqueue.hpp"

using namespace DJI::OSDK;
using namespace std;

//creat pipeline element
GstElement *pipeline, *appsrc, *parse, *decoder, *scale, *filter1, *conv, *encoder, *filter2, *rtppay, *udpsink;
GstCaps *scale_caps;
GstCaps *omx_caps;
static GMainLoop *loop;
GstBuffer *buf;


// 定义结构体,创建链表结点,模拟公共区
typedef struct Msg
{
	int len;
	uint8_t* data;
} msg;

// 创建队列
LoopQueue<msg*> loopqueue(1024);

pthread_mutex_t myMutex = PTHREAD_MUTEX_INITIALIZER; /*初始化互斥锁*/
pthread_cond_t  cond = PTHREAD_COND_INITIALIZER; //init cond

int count_t = 0;

static void cb_need_data(GstElement *appsrc, guint size, gpointer user_data)
{
	static GstClockTime timestamp = 0;
	GstFlowReturn ret;
	GstMapInfo map;

	count_t++;

	// 给互斥量加锁
	pthread_mutex_lock(&myMutex);


	// 判断条件变量是否满足 访问公共区条件
	while (0 == loopqueue.getSize())
	{
		pthread_cond_wait(&cond, &myMutex);
	}
	// 访问公共区, 取数据
	// 往appsrc输入数据
	buf = gst_buffer_new_allocate(NULL, loopqueue.top()->len, NULL);
	gst_buffer_map(buf, &map, GST_MAP_WRITE);
	memcpy((guchar *)map.data, (guchar *)(loopqueue.top()->data), loopqueue.top()->len);

	printf("count_t = %d, bufLen = %d\n", count_t, loopqueue.top()->len);
	// 给互斥量解锁
	pthread_mutex_unlock(&myMutex);

	GST_BUFFER_PTS(buf) = timestamp;
	GST_BUFFER_DURATION(buf) = gst_util_uint64_scale_int(1, GST_SECOND, 1);
	timestamp += GST_BUFFER_DURATION(buf);
	g_signal_emit_by_name(appsrc, "push-buffer", buf, &ret);

	gst_buffer_unref(buf);
	if (ret != 0)
	{
		g_main_loop_quit(loop);
	}

	// 释放摘下的结点
	delete loopqueue.top()->data;
	delete loopqueue.top();
	loopqueue.pop();
}

void liveViewSampleCb(uint8_t* buf264, int bufLen, void* userData)
{
	// 生产数据
	msg *mp = new msg;
	mp->len = bufLen;
	mp->data = new uint8_t[bufLen];
	memcpy(mp->data, buf264, bufLen);
	// 加锁
	pthread_mutex_lock(&myMutex);
	// 访问公共区
	loopqueue.push(mp);
	// 解锁
	pthread_mutex_unlock(&myMutex);
	// 唤醒阻塞在条件变量上的消费者
	pthread_cond_signal(&cond);
	return;
}

// 创建管道，触发need-data信号
void *consumer(void *arg)
{
	// init GStreamer
	gst_init(NULL, NULL);
	loop = g_main_loop_new(NULL, FALSE);

	// setup pipeline
	pipeline = gst_pipeline_new("pipeline");
	appsrc = gst_element_factory_make("appsrc", "source");
	parse = gst_element_factory_make("h264parse", "parse");
	decoder = gst_element_factory_make("avdec_h264", "decoder");
	scale = gst_element_factory_make("videoscale", "scale");
	filter1 = gst_element_factory_make("capsfilter", "filter1");
	conv = gst_element_factory_make("videoconvert", "conv");
	encoder = gst_element_factory_make("omxh264enc", "encoder");
	filter2 = gst_element_factory_make("capsfilter", "filter2");
	rtppay = gst_element_factory_make("rtph264pay", "rtppay");
	udpsink = gst_element_factory_make("udpsink", "sink");

	scale_caps = gst_caps_new_simple("video/x-raw",
	                                 "width", G_TYPE_INT, 1280,
	                                 "height", G_TYPE_INT, 720,
	                                 NULL);

	omx_caps = gst_caps_new_simple("video/x-h264",
	                               "stream-format", G_TYPE_STRING, "byte-stream",
	                               NULL);

	g_object_set(G_OBJECT(filter1), "caps", scale_caps, NULL);
	g_object_set(G_OBJECT(filter2), "caps", omx_caps, NULL);
	gst_caps_unref(scale_caps);
	gst_caps_unref(omx_caps);

	g_object_set(udpsink, "host", "127.0.0.1", NULL);
	g_object_set(udpsink, "port", 5555, NULL);
	g_object_set(udpsink, "sync", false, NULL);
	g_object_set(udpsink, "async", false, NULL);

	// setup
	//g_object_set(G_OBJECT(appsrc), "caps",
	//	gst_caps_new_simple("video/x-raw",
	//		"stream-format", G_TYPE_STRING, "byte-stream",
	//		NULL), NULL);

	gst_bin_add_many(GST_BIN(pipeline), appsrc, parse, decoder, scale, filter1, conv, encoder, filter2, rtppay, udpsink, NULL);
	gst_element_link_many(appsrc, parse, decoder, scale, filter1, conv, encoder, filter2, rtppay, udpsink, NULL);

	// setup appsrc
	//g_object_set(G_OBJECT(appsrc),
	//	"stream-type", 0,
	//	"format", GST_FORMAT_TIME, NULL);

	g_signal_connect(appsrc, "need-data", G_CALLBACK(cb_need_data), NULL);

	/* play */
	std::cout << "start sender pipeline" << std::endl;
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	g_main_loop_run(loop);
	/* clean up */
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(pipeline));
	g_main_loop_unref(loop);
	gst_buffer_unref(buf);

}

// 初始化飞机开始推送264码流数据包
void *producer(void *arg)
{
	while (true)
	{
		// Setup OSDK.
		bool enableAdvancedSensing = true;
		LinuxSetup linuxEnvironment(1, NULL, enableAdvancedSensing);
		Vehicle *vehicle = linuxEnvironment.getVehicle();
		if (vehicle == nullptr)
		{
			std::cout << "Vehicle not initialized, exiting.\n";
			return NULL;
		}
		vehicle->advancedSensing->startH264Stream(LiveView::OSDK_CAMERA_POSITION_NO_1,
		        liveViewSampleCb,
		        NULL);
		sleep(1000);
	}
}

int main(int argc, char** argv)
{
	// 创建一个生产者线程和一个消费者线程
	int rc1, rc2;
	pthread_t pid, cid;

	if (rc1 = pthread_create(&pid, NULL, producer, NULL))
		cout << "producer thread creation failed: " << rc1 << endl;
	//g_usleep(50 * 1000); // 让队列先有数据
	if (rc2 = pthread_create(&cid, NULL, consumer, NULL))
		cout << "consumer thread creation failed: " << rc2 << endl;

	pthread_join(pid, NULL);
	pthread_join(cid, NULL);

	pthread_mutex_destroy(&myMutex);
	pthread_cond_destroy(&cond);
	return 0;
}