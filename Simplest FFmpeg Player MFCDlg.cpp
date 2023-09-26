
// Simplest FFmpeg Player MFCDlg.cpp : 实现文件
//

/**
* 最简单的图形界面的视频播放器
* Simplest FFmpeg Player GUI
*
* 原程序：
* 雷霄骅 Lei Xiaohua
* leixiaohua1020@126.com
* 中国传媒大学/数字电视技术
* Communication University of China / Digital TV Technology
* http://blog.csdn.net/leixiaohua1020
*
* 修改：
* 刘文晨 Liu Wenchen
* 812288728@qq.com
* 电子科技大学/电子信息
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*/

#include "stdafx.h"
#include "Simplest FFmpeg Player MFC.h"
#include "Simplest FFmpeg Player MFCDlg.h"
#include "afxdialogex.h"

#pragma warning(disable:4996)

#define __STDC_CONSTANT_MACROS

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL2/SDL.h"
};

// 报错：
// LNK2019 无法解析的外部符号 __imp__fprintf，该符号在函数 _ShowError 中被引用
// LNK2019 无法解析的外部符号 __imp____iob_func，该符号在函数 _ShowError 中被引用

// 解决办法：
// 包含库的编译器版本低于当前编译版本，需要将包含库源码用vs2017重新编译，由于没有包含库的源码，此路不通。
// 然后查到说是stdin, stderr, stdout 这几个函数vs2015和以前的定义得不一样，所以报错。
// 解决方法呢，就是使用{ *stdin,*stdout,*stderr }数组自己定义__iob_func()
#pragma comment(lib,"legacy_stdio_definitions.lib")
extern "C"
{
	FILE __iob_func[3] = { *stdin,*stdout,*stderr };
}

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

														// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CSimplestFFmpegPlayerMFCDlg 对话框
CSimplestFFmpegPlayerMFCDlg::CSimplestFFmpegPlayerMFCDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_SIMPLESTFFMPEGPLAYERMFC_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSimplestFFmpegPlayerMFCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_URL, m_url);
}

BEGIN_MESSAGE_MAP(CSimplestFFmpegPlayerMFCDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_ABOUT, &CSimplestFFmpegPlayerMFCDlg::OnBnClickedAbout)
	ON_BN_CLICKED(IDC_FILEDIALOG, &CSimplestFFmpegPlayerMFCDlg::OnBnClickedFiledialog)
	ON_BN_CLICKED(IDC_PLAY, &CSimplestFFmpegPlayerMFCDlg::OnBnClickedPlay)
	ON_BN_CLICKED(IDC_PAUSE, &CSimplestFFmpegPlayerMFCDlg::OnBnClickedPause)
	ON_BN_CLICKED(IDC_STOP, &CSimplestFFmpegPlayerMFCDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_CLOSE, &CSimplestFFmpegPlayerMFCDlg::OnBnClickedClose)
END_MESSAGE_MAP()

// 自定义消息类型
#define REFRESH_EVENT  (SDL_USEREVENT + 1) // Refresh Event
#define BREAK_EVENT  (SDL_USEREVENT + 2) // Break

// 线程标志位
int thread_exit = 0;// 退出标志，等于1则退出
int thread_pause = 0;// 暂停标志，等于1则暂停

// 视频播放相关参数
int delay_time = 40;
// bool video_gray = false;

// 画面刷新线程
int refresh_video(void *opaque)
{
	thread_exit = 0;
	thread_pause = 0;
	while (thread_exit == 0)
	{
		if (thread_pause == 0)
		{
			SDL_Event event;
			event.type = REFRESH_EVENT;
			// 向主线程发送刷新事件
			SDL_PushEvent(&event);
		}
		// 工具函数，用于延时
		SDL_Delay(delay_time);
	}
	// 需要结束播放
	SDL_Event event;
	event.type = BREAK_EVENT;
	// 向主线程发送退出循环事件
	SDL_PushEvent(&event);
	thread_exit = 0;
	thread_pause = 0;
	return 0;
}

UINT ffmpegPlayer(LPVOID lpParam)
{
	AVFormatContext	*pFormatCtx;
	int				i, videoindex;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pFrame, *pFrameYUV;
	uint8_t *out_buffer;
	AVPacket *packet;
	int y_size;
	int ret, got_picture;
	//------------ SDL ----------------
	int screen_w, screen_h;
	SDL_Window *screen;// 窗口
	SDL_Renderer* sdlRenderer;// 渲染器
	SDL_Texture* sdlTexture;// 纹理
	SDL_Rect sdlRect;// 渲染显示面积
	SDL_Thread *refresh_thread;// 画面刷新线程
	SDL_Event event;// 主线程使用的事件

	struct SwsContext *img_convert_ctx;
	//------------MFC----------------
	CSimplestFFmpegPlayerMFCDlg *sfp_dlg = (CSimplestFFmpegPlayerMFCDlg *)lpParam;
	char filepath[250] = { 0 };// 输入文件路径
	GetWindowText(sfp_dlg->m_url, (LPSTR)filepath, 250);
	//----------MFC End--------------
	int frame_cnt;

	av_register_all();
	avformat_network_init();
	// 申请avFormatContext空间，记得要释放
	pFormatCtx = avformat_alloc_context();
	// 打开媒体文件
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0)
	{
		printf("Couldn't open input stream.\n");
		return -1;
	}
	// 读取媒体文件信息，给pFormatCtx赋值
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
	{
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoindex = i;
			break;
		}
	}
	if (videoindex == -1)
	{
		printf("Didn't find a video stream.\n");
		return -1;
	}
	pCodecCtx = pFormatCtx->streams[videoindex]->codec;
	// 根据视频流信息的codec_id找到对应的解码器
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}
	// 使用给定的pCodec初始化pCodecCtx
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("Could not open codec.\n");
		return -1;
	}

	/*
	* 在此处添加输出视频信息的代码
	* 取自于pFormatCtx，使用fprintf()
	*/
	FILE *fp_txt = fopen("info.txt", "wb+");

	fprintf(fp_txt, "封装格式：%s\n", pFormatCtx->iformat->long_name);
	fprintf(fp_txt, "比特率：%d\n", pFormatCtx->bit_rate);
	fprintf(fp_txt, "视频时长：%d\n", pFormatCtx->duration);
	fprintf(fp_txt, "视频编码方式：%s\n", pFormatCtx->streams[videoindex]->codec->codec->long_name);
	fprintf(fp_txt, "视频分辨率：%d * %d\n", pFormatCtx->streams[videoindex]->codec->width, pFormatCtx->streams[videoindex]->codec->height);

	// 在avcodec_receive_frame()函数作为参数，获取到frame
	// 获取到的frame有些可能是错误的要过滤掉，否则相应帧可能出现绿屏
	pFrame = av_frame_alloc();
	//作为yuv输出的frame承载者，会进行缩放和过滤出错的帧，YUV相应的数据也是从该对象中读取
	pFrameYUV = av_frame_alloc();
	// 用于渲染的数据，且格式为YUV420P
	out_buffer = (uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
	avpicture_fill((AVPicture *)pFrameYUV, out_buffer, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);

	// Output Info
	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");
	// 由于解码出来的帧格式不一定是YUV420P的,在渲染之前需要进行格式转换
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	// 帧计数器
	frame_cnt = 0;

	// ---------------------- SDL ----------------------
	// 初始化SDL系统
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER))
	{
		printf("Couldn't initialize SDL - %s\n", SDL_GetError());
		return -1;
	}
	// SDL 2.0 Support for multiple windows
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	// 视频像素的宽和高
	const int pixel_w = screen_w;
	const int pixel_h = screen_h;
	// 创建窗口SDL_Window（显示在弹出窗口）
	// screen = SDL_CreateWindow("Simplest FFmpeg Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	//显示在MFC控件上
	screen = SDL_CreateWindowFrom(sfp_dlg->GetDlgItem(IDC_SCREEN)->GetSafeHwnd());
	if (!screen)
	{
		printf("SDL: Couldn't create window - exiting:%s\n", SDL_GetError());
		return -1;
	}
	// 创建渲染器SDL_Renderer
	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);

	Uint32 pixformat = SDL_PIXELFORMAT_IYUV;
	// IYUV: Y + U + V  (3 planes)
	// YV12: Y + V + U  (3 planes)
	// 创建纹理SDL_Texture
	sdlTexture = SDL_CreateTexture(sdlRenderer, pixformat, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	// 创建画面刷新线程
	refresh_thread = SDL_CreateThread(refresh_video, NULL, NULL);
	// ---------------------- SDL End ----------------------
	// 开始一帧一帧读取
	while (1)
	{
		// Wait
		SDL_WaitEvent(&event);// 从事件队列中取事件
		if (event.type == REFRESH_EVENT)
		{
			while (1)
			{
				if (av_read_frame(pFormatCtx, packet) < 0)
				{
					thread_exit = 1;
				}
				if (packet->stream_index == videoindex)
				{
					break;
				}
			}

			// 输出每一个解码前视频帧参数：帧大小
			fprintf(fp_txt, "帧%d大小：%d\n", frame_cnt, packet->size);

			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0)
			{
				printf("Decode Error.\n");
				return -1;
			}
			if (got_picture)
			{
				// 格式转换，解码后的数据经过sws_scale()函数处理，去除无效像素
				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
					pFrameYUV->data, pFrameYUV->linesize);
				printf("Decoded frame index: %d\n", frame_cnt);

				// 输出每一个解码后视频帧参数：帧类型
				char pict_type_str[10];
				switch (pFrame->pict_type)
				{
				case AV_PICTURE_TYPE_NONE:
					sprintf(pict_type_str, "NONE");
					break;
				case AV_PICTURE_TYPE_I:
					sprintf(pict_type_str, "I");
					break;
				case AV_PICTURE_TYPE_P:
					sprintf(pict_type_str, "P");
					break;
				case AV_PICTURE_TYPE_B:
					sprintf(pict_type_str, "B");
					break;
				case AV_PICTURE_TYPE_SI:
					sprintf(pict_type_str, "SI");
					break;
				case AV_PICTURE_TYPE_S:
					sprintf(pict_type_str, "S");
					break;
				case AV_PICTURE_TYPE_SP:
					sprintf(pict_type_str, "SP");
					break;
				case AV_PICTURE_TYPE_BI:
					sprintf(pict_type_str, "BI");
					break;
				default:
					break;
				}
				fprintf(fp_txt, "帧%d类型：%s\n", frame_cnt, pict_type_str);

				// 若选择显示黑白图像，则将buffer的U、V数据设置为128
				//if (video_gray == true)
				//{
				//	// U、V是图像中的经过偏置处理的色度分量
				//	// 在偏置处理前，它的取值范围是-128-127，这时，把U和V数据修改为0代表无色
				//	// 在偏置处理后，它的取值范围变成了0-255，所以这时候需要取中间值，即128
				//	memset(pFrameYUV->data[0] + pixel_w * pixel_h, 128, pixel_w * pixel_h / 2);
				//}
				// 设置纹理的数据
				SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
				// FIX: If window is resize
				sdlRect.x = 0;
				sdlRect.y = 0;
				sdlRect.w = screen_w;
				sdlRect.h = screen_h;

				// 使用图形颜色清除当前的渲染目标
				SDL_RenderClear(sdlRenderer);
				// 将纹理的数据拷贝给渲染器
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
				// 显示纹理的数据
				SDL_RenderPresent(sdlRenderer);

				frame_cnt++;
			}
			av_free_packet(packet);
		}
		else if (event.type == SDL_KEYDOWN)
		{
			// 根据按下键盘键位决定事件
			switch (event.key.keysym.sym)
			{
			case SDLK_ESCAPE:
				thread_exit = 1;// 按下ESC键，直接退出播放器
				break;
			case SDLK_SPACE:
				thread_pause = !thread_pause;// 按下Space键，控制视频播放暂停
				break;
			//case SDLK_F1:
			//	delay_time += 10;// 按下F1，视频减速
			//	break;
			//case SDLK_F2:
			//	if (delay_time > 10)
			//	{
			//		delay_time -= 10;// 按下F2，视频加速
			//	}
			//	break;
			//case SDLK_LSHIFT:
			//	video_gray = !video_gray;// 按下左Shift键，切换显示彩色/黑白图像
			//	break;
			default:
				break;
			}
		}
		else if (event.type == SDL_QUIT)
		{
			thread_exit = 1;
		}
		else if (event.type == BREAK_EVENT)
		{
			break;
		}
	}

	// 关闭文件
	fclose(fp_txt);

	// 释放FFmpeg相关资源
	sws_freeContext(img_convert_ctx);
	// 释放SDL资源
	SDL_DestroyTexture(sdlTexture);
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(screen);
	// 退出SDL系统
	SDL_Quit();

	//FIX Small Bug
	//SDL Hide Window When it finished
	sfp_dlg->GetDlgItem(IDC_SCREEN)->ShowWindow(SW_SHOWNORMAL);

	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}

// CSimplestFFmpegPlayerMFCDlg 消息处理程序

BOOL CSimplestFFmpegPlayerMFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

									// TODO: 在此添加额外的初始化代码

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CSimplestFFmpegPlayerMFCDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CSimplestFFmpegPlayerMFCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CSimplestFFmpegPlayerMFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CSimplestFFmpegPlayerMFCDlg::OnBnClickedAbout()
{
	// TODO: 在此添加控件通知处理程序代码
	CAboutDlg about_dlg;
	about_dlg.DoModal();
}


void CSimplestFFmpegPlayerMFCDlg::OnBnClickedFiledialog()
{
	// TODO: 在此添加控件通知处理程序代码
	CString file_path_name;
	CFileDialog file_dlg(TRUE, NULL, NULL, NULL, NULL);//TRUE为OPEN对话框，FALSE为SAVE AS对话框 
	if (file_dlg.DoModal() == IDOK)
	{
		file_path_name = file_dlg.GetPathName();
		m_url.SetWindowText(file_path_name);
	}
}

UINT Thread_Play(LPVOID lpParam)
{
	CSimplestFFmpegPlayerMFCDlg *sfp_dlg = (CSimplestFFmpegPlayerMFCDlg *)lpParam;
	ffmpegPlayer(lpParam);
	return 0;
}

void CSimplestFFmpegPlayerMFCDlg::OnBnClickedPlay()
{
	// TODO: 在此添加控件通知处理程序代码
	CString str;
	m_url.GetWindowText(str);
	if (str.IsEmpty())
	{
		AfxMessageBox("请输入文件路径！");
		return;
	}
	AfxBeginThread(Thread_Play, this);//开启线程
}


void CSimplestFFmpegPlayerMFCDlg::OnBnClickedPause()
{
	// TODO: 在此添加控件通知处理程序代码
	thread_pause = !thread_pause;
}


void CSimplestFFmpegPlayerMFCDlg::OnBnClickedStop()
{
	// TODO: 在此添加控件通知处理程序代码
	thread_exit = 1;
}


void CSimplestFFmpegPlayerMFCDlg::OnBnClickedClose()
{
	// TODO: 在此添加控件通知处理程序代码
	CDialogEx::OnCancel();
}
